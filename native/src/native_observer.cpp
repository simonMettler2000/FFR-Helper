// Experimental, exact-build native entry observer. No gameplay-state setters,
// save/load calls or restart commands. The original encounter call runs once.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <MinHook.h>
#ifdef FFR_CAPTURE_PROFILE_HEADER
#include FFR_CAPTURE_PROFILE_HEADER
#else
#include "build_profile.hpp"
#endif
#include "encounter_capture.hpp"
#include <atomic>
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <thread>

namespace {
using Clock = std::chrono::system_clock;
using EntryFunction = void(__fastcall*)(void*, const void*, bool);
using HostMethod = void(__fastcall*)(void*);
HMODULE own_module{};
std::atomic<EntryFunction> original_entry{nullptr};
void* hook_address{};
void* host_object{};
HostMethod destroy_host{};
bool host_constructed{};
std::atomic<bool> ever_started{false};
std::atomic<unsigned> in_flight{0};

std::int64_t seconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(Clock::now().time_since_epoch()).count();
}

struct Handle {
    HANDLE value{INVALID_HANDLE_VALUE};
    explicit Handle(HANDLE input) : value(input) {}
    ~Handle() { if (value != INVALID_HANDLE_VALUE && value != nullptr) CloseHandle(value); }
    bool valid() const { return value != INVALID_HANDLE_VALUE && value != nullptr; }
};

struct Sha256 {
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    std::vector<unsigned char> storage;
    Sha256() {
        DWORD length{}, received{};
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0
            || BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&length), sizeof(length), &received, 0) < 0)
            throw std::runtime_error("SHA-256 unavailable");
        storage.resize(length);
        if (BCryptCreateHash(algorithm, &hash, storage.data(), length, nullptr, 0, 0) < 0)
            throw std::runtime_error("SHA-256 initialization failed");
    }
    ~Sha256() { if (hash) BCryptDestroyHash(hash); if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0); }
    void add(const void* data, std::size_t bytes) {
        if (bytes > MAXDWORD || BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<void*>(data)), static_cast<ULONG>(bytes), 0) < 0)
            throw std::runtime_error("SHA-256 update failed");
    }
    std::string finish() {
        unsigned char result[32]{};
        if (BCryptFinishHash(hash, result, sizeof(result), 0) < 0) throw std::runtime_error("SHA-256 finalization failed");
        constexpr char digits[] = "0123456789abcdef";
        std::string text;
        for (auto byte : result) { text += digits[byte>>4]; text += digits[byte&15]; }
        return text;
    }
};

std::filesystem::path module_path(HMODULE module) {
    std::wstring buffer(32768, L'\0');
    auto count = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (!count || count >= buffer.size()) throw std::runtime_error("Module path unavailable");
    buffer.resize(count);
    return buffer;
}

std::string file_hash(const std::filesystem::path& path) {
    Handle handle(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
    if (!handle.valid()) return {};
    Sha256 hash;
    std::array<unsigned char, 65536> buffer{};
    for (;;) {
        DWORD count{};
        if (!ReadFile(handle.value, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr)) return {};
        if (!count) break;
        hash.add(buffer.data(), count);
    }
    return hash.finish();
}

bool read_memory(std::uint64_t address, void* destination, std::size_t length) {
    SIZE_T copied{};
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(address), destination, length, &copied)
        && copied == length;
}

bool read_small(const std::filesystem::path& path, std::string& out, DWORD maximum) {
    Handle file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file.valid()) return false;
    std::vector<char> buffer(maximum+1);
    DWORD count{};
    if (!ReadFile(file.value, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr) || count > maximum) return false;
    out.assign(buffer.data(), count);
    return true;
}

bool write_atomic(const std::filesystem::path& path, const std::string& data) {
    if (data.size() > 512*1024) return false;
    auto temporary = path;
    temporary += L".native-tmp";
    {
        Handle file(CreateFileW(temporary.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
        DWORD written{};
        if (!file.valid() || !WriteFile(file.value, data.data(), static_cast<DWORD>(data.size()), &written, nullptr)
            || written != data.size() || !FlushFileBuffers(file.value)) return false;
    }
    for (unsigned attempt=0; attempt<6; ++attempt) {
        if (MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
        auto error = GetLastError();
        if (error != ERROR_SHARING_VIOLATION && error != ERROR_ACCESS_DENIED && error != ERROR_LOCK_VIOLATION) return false;
        Sleep(10u << attempt);
    }
    return false;
}

bool hex_id(const std::string& value) {
    return value.size() == 32 && value.find_first_not_of("0123456789abcdef") == std::string::npos;
}

struct Lease { std::string session, request; std::int64_t expiry{}; };
std::optional<Lease> read_lease(const std::filesystem::path& path) {
    std::string raw;
    if (!read_small(path, raw, 256)) return {};
    std::istringstream input(raw);
    std::string version, session, expiry, request, extra;
    if (!std::getline(input, version) || !std::getline(input, session) || !std::getline(input, expiry)
        || !std::getline(input, request) || std::getline(input, extra)
        || version != "FFR_HELPER_OBSERVER_V1" || !hex_id(session) || !hex_id(request)
        || expiry.empty() || expiry.size() > 12 || expiry.find_first_not_of("0123456789") != std::string::npos) return {};
    auto until = std::stoll(expiry);
    if (until < seconds() || until > seconds()+8) return {};
    return Lease{session, request, until};
}

struct Observer {
    std::filesystem::path connection;
    std::mutex mutex;
    std::deque<std::string> events;
    std::size_t event_bytes{};
    std::uint64_t sequence{};
    std::string session, request, message{"Waiting for native capture initialization"};
    std::atomic<std::int64_t> expires{0};
    std::atomic<bool> quit{false}, ready{false};
    std::atomic<unsigned> failures{0};
    std::thread worker;

    std::string envelope(const Lease& lease) {
        return "\"protocol\":1,\"adapter\":\"ffr-helper-native-entry-observer\",\"session\":"
            + ffr::quote(lease.session) + ",\"time\":" + std::to_string(seconds()) + ",\"readOnly\":true";
    }
    void run() noexcept {
        while (!quit.load()) {
            try {
                auto lease = read_lease(connection/L"observe.txt");
                if (!lease) { expires.store(0); Sleep(250); continue; }
                std::deque<std::string> copied;
                std::string note;
                bool requested = false;
                {
                    std::lock_guard lock(mutex);
                    if (session != lease->session) {
                        events.clear(); event_bytes = 0; sequence = 0; request.clear(); session = lease->session;
                    }
                    expires.store(lease->expiry);
                    note = message;
                    if (lease->request != std::string(32, '0') && lease->request != request) {
                        copied = events; requested = true;
                    }
                }
                const auto base = envelope(*lease);
                auto status = "{" + base + ",\"hooks\":{\"native-encounter-entry\":"
                    + ffr::quote(ready.load() ? "observing" : "unavailable") + "},\"message\":" + ffr::quote(note) + '}';
                write_atomic(connection/L"observer-status.json", status);
                if (requested) {
                    std::string report = "{" + base + ",\"request\":" + ffr::quote(lease->request)
                        + ",\"format\":\"ffr-helper-runtime-report\",\"version\":1,\"kind\":\"diagnostic-observation\",\"restorable\":false"
                        + ",\"objects\":{\"nativeEntry\":{\"ready\":" + (ready.load() ? "true" : "false")
                        + ",\"captureFailures\":" + std::to_string(failures.load()) + "}},\"events\":[";
                    for (std::size_t i=0; i<copied.size(); ++i) { if (i) report += ','; report += copied[i]; }
                    report += "],\"missingState\":[\"Complete pre-battle progression and resources\",\"Full combat and RNG state\",\"Verified restore path\"]}";
                    if (write_atomic(connection/L"runtime-report.json", report)) {
                        std::lock_guard lock(mutex); request = lease->request;
                    }
                }
            } catch (...) { /* Invalid input/I/O cannot affect the game's encounter call. */ }
            Sleep(250);
        }
        expires.store(0);
    }
};
// The DLL is pinned once the hook is enabled. Keep state/trampoline allocations
// until process exit so an in-flight callback cannot return into unloaded code.
std::unique_ptr<Observer> observer;
std::atomic<Observer*> published_observer{nullptr};

void __fastcall encounter_hook(void* context, const void* entry, bool skip_intro) {
    struct Guard {
        Guard() { in_flight.fetch_add(1); }
        ~Guard() { in_flight.fetch_sub(1); }
    } guard;
    const auto last_error = GetLastError();
    auto* current = published_observer.load(std::memory_order_acquire);
    ffr::observe_and_forward([&] {
        if (!current || current->quit.load() || current->expires.load() < seconds()) return;
        auto copied = ffr::capture_entry(read_memory, reinterpret_cast<std::uint64_t>(entry), skip_intro);
        if (!copied) { current->failures.fetch_add(1); return; }
        auto json = ffr::to_json(*copied);
        if (json.size() > 128*1024) { current->failures.fetch_add(1); return; }
        std::lock_guard lock(current->mutex);
        auto event = "{\"kind\":\"encounter-opening-native\",\"sequence\":" + std::to_string(++current->sequence)
            + ",\"time\":" + std::to_string(seconds()) + ",\"capturePoint\":\"before-native-entry\",\"entry\":" + json + '}';
        while (!current->events.empty() && (current->events.size() >= 32 || current->event_bytes+event.size() > 384*1024)) {
            current->event_bytes -= current->events.front().size(); current->events.pop_front();
        }
        current->event_bytes += event.size();
        current->events.push_back(std::move(event));
    }, [&] {
        SetLastError(last_error);
        original_entry.load(std::memory_order_acquire)(context, entry, skip_intro);
    });
}

void set_message(std::string message) {
    std::lock_guard lock(observer->mutex);
    observer->message = std::move(message);
}

bool install_entry_hook() {
    if (file_hash(module_path(nullptr)) != ffr::profile::game_sha256) {
        set_message("Unsupported game build; native hook was not installed"); return false;
    }
    auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    hook_address = reinterpret_cast<void*>(base+ffr::profile::entry_rva);
    std::array<unsigned char, ffr::profile::prologue_size> bytes{};
    if (!read_memory(reinterpret_cast<std::uint64_t>(hook_address), bytes.data(), bytes.size())) {
        set_message("Native entry is unreadable; hook was not installed"); return false;
    }
    Sha256 hash; hash.add(bytes.data(), bytes.size());
    if (hash.finish() != ffr::profile::prologue_sha256) {
        set_message("Native entry does not match the verified code profile; hook was not installed"); return false;
    }
    void* trampoline{};
    if (MH_Initialize() != MH_OK || MH_CreateHook(hook_address, reinterpret_cast<void*>(&encounter_hook), &trampoline) != MH_OK) {
        set_message("Native detour could not be prepared; game execution is unchanged"); return false;
    }
    original_entry.store(reinterpret_cast<EntryFunction>(trampoline), std::memory_order_release);
    published_observer.store(observer.get(), std::memory_order_release);
    HMODULE pinned{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                            reinterpret_cast<LPCWSTR>(&encounter_hook), &pinned)) {
        MH_RemoveHook(hook_address);
        set_message("Unable to retain native observer code safely; hook was not installed"); return false;
    }
    if (MH_EnableHook(hook_address) != MH_OK) {
        MH_RemoveHook(hook_address);
        set_message("Native detour could not be enabled; hook was not installed"); return false;
    }
    observer->ready.store(true);
    set_message("Experimental native encounter entry observer connected; restoration is unavailable");
    return true;
}

} // namespace

extern "C" __declspec(dllexport) void* start_mod() noexcept {
    if (ever_started.exchange(true)) return nullptr;
    try {
        const auto root = module_path(own_module).parent_path().parent_path();
        if (root.filename() != L"FFRHelperRuntimeNative" || !std::filesystem::is_directory(root/L"connection")) return nullptr;
        auto loader = GetModuleHandleW(L"UE4SS.dll");
        if (!loader || file_hash(module_path(loader)) != ffr::profile::loader_sha256) return nullptr;
        auto construct = reinterpret_cast<HostMethod>(GetProcAddress(loader, ffr::profile::constructor_name));
        destroy_host = reinterpret_cast<HostMethod>(GetProcAddress(loader, ffr::profile::destructor_name));
        if (reinterpret_cast<std::uintptr_t>(construct) != reinterpret_cast<std::uintptr_t>(loader)+ffr::profile::constructor_rva
            || reinterpret_cast<std::uintptr_t>(destroy_host) != reinterpret_cast<std::uintptr_t>(loader)+ffr::profile::destructor_rva) return nullptr;
        // The exact DLL constructs/destructs its own CppUserModBase in aligned
        // storage. Its verified public definition uses 192 bytes. We reserve a
        // full page, never inspect its fields, copy STL types or replace vtables.
        host_object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ffr::profile::host_storage_size);
        if (!host_object) return nullptr;
        construct(host_object);
        host_constructed = true;
        observer = std::make_unique<Observer>();
        observer->connection = root/L"connection";
        auto* state = observer.get();
        observer->worker = std::thread([state] { state->run(); });
        install_entry_hook();
        return host_object;
    } catch (...) {
        if (observer) {
            observer->expires.store(0); observer->quit.store(true);
            if (observer->worker.joinable()) observer->worker.join();
            if (observer->ready.exchange(false)) MH_DisableHook(hook_address);
        }
        if (host_constructed && destroy_host && host_object) {
            destroy_host(host_object); HeapFree(GetProcessHeap(), 0, host_object); host_object = nullptr; host_constructed = false;
        }
        // If initialization is interrupted, preserve the host allocation rather
        // than invoking a destructor on an incompletely constructed object.
        return nullptr;
    }
}

extern "C" __declspec(dllexport) void uninstall_mod(void* object) noexcept {
    if (object != host_object) return;
    try {
        if (observer) {
            observer->expires.store(0); observer->quit.store(true);
            if (observer->worker.joinable()) observer->worker.join();
            if (observer->ready.exchange(false)) MH_DisableHook(hook_address);
        }
        // Retain the disabled MinHook trampoline and pinned module until process
        // exit, including a callback paused before its in-flight counter update.
        if (destroy_host && host_object) destroy_host(host_object);
        if (host_object) HeapFree(GetProcessHeap(), 0, host_object);
        host_object = nullptr; host_constructed = false;
    } catch (...) { /* Unload errors never trigger game-state recovery calls. */ }
}

extern "C" __declspec(dllexport) const char* ffr_observer_game_profile() noexcept { return ffr::profile::game_sha256; }
extern "C" __declspec(dllexport) const char* ffr_observer_loader_profile() noexcept { return ffr::profile::loader_sha256; }

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { own_module = instance; DisableThreadLibraryCalls(instance); }
    return TRUE;
}
