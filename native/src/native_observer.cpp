// Default build: exact-build observation only. The separately named practice
// build adds explicit opening-abort and detached-import experiments. Neither
// replaces the live progression root, restores a battle or writes a game slot.
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
#include "progression_capture.hpp"
#include "field_anchor.hpp"
#include "playtime_capture.hpp"
#ifdef FFR_PLAYTIME_PROFILE_HEADER
#include FFR_PLAYTIME_PROFILE_HEADER
#else
#include "playtime_profile.hpp"
#endif
#ifdef FFR_FIELD_PROFILE_HEADER
#include FFR_FIELD_PROFILE_HEADER
#else
#include "field_profile.hpp"
#endif
#ifdef FFR_PRACTICE_BUILD
#include "abort_trial.hpp"
#include "progression_clone.hpp"
#include "combat_transport.hpp"
#ifdef FFR_IMPORT_PROFILE_HEADER
#include FFR_IMPORT_PROFILE_HEADER
#else
#include "import_profile.hpp"
#endif
#ifdef FFR_ABORT_PROFILE_HEADER
#include FFR_ABORT_PROFILE_HEADER
#else
#include "abort_profile.hpp"
#endif
#include <set>
#endif
#include <atomic>
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <thread>
#include <string_view>

namespace {
using Clock = std::chrono::system_clock;
using EntryFunction = void(__fastcall*)(void*, const void*, bool);
using HostMethod = void(__fastcall*)(void*);
using RootGetter = void*(__fastcall*)();
using SaveMemory = bool(__fastcall*)(void*, ffr::RawArray*);
using MemoryFree = void(__fastcall*)(void*);
RootGetter root_getter{};
SaveMemory save_memory{};
MemoryFree memory_free{};
using FieldObjectGetter=void*(__fastcall*)(void*);
using FieldPlayerGetter=void*(__fastcall*)();
using FieldMapIdGetter=std::int32_t(__fastcall*)();
FieldObjectGetter field_world{},field_map{};
FieldPlayerGetter field_player{};
FieldMapIdGetter field_map_id{};
std::atomic<bool> field_profile_ready{false};
using PlaytimeClockGetter=void*(__fastcall*)();
using PlaytimeSecondsGetter=std::int64_t(__fastcall*)();
PlaytimeClockGetter playtime_clock{};
PlaytimeSecondsGetter playtime_seconds{};
std::atomic<bool> playtime_profile_ready{false};
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

bool write_atomic(const std::filesystem::path& path, std::string_view data, std::size_t maximum=512*1024) {
    if (data.size() > maximum) return false;
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
    struct Opening {
        std::uint64_t sequence{};
        std::int64_t time{};
        std::string session, entry;
        std::string field_anchor="null";
        std::string playtime_basis="null";
        std::vector<unsigned char> progression;
#ifdef FFR_PRACTICE_BUILD
        ffr::Encounter encounter;
        std::uint64_t context_identity{};
        DWORD thread{};
#endif
    };
    std::filesystem::path connection;
    std::mutex mutex;
    std::deque<std::string> events;
    std::size_t event_bytes{};
    std::uint64_t sequence{};
    std::string session, request, message{"Waiting for native capture initialization"};
    std::atomic<std::int64_t> expires{0};
    std::atomic<bool> quit{false}, ready{false};
    std::atomic<unsigned> failures{0};
    std::atomic<unsigned> progression_failures{0};
    std::atomic<unsigned> field_failures{0};
    std::atomic<unsigned> playtime_failures{0};
    std::shared_ptr<const Opening> opening;
#ifdef FFR_PRACTICE_BUILD
    ffr::AbortTrial trial;
    ffr::AbortGuard last_guard;
    std::optional<ffr::AbortRequest> abort_request;
    bool abort_available=false;
    std::string abort_message="Abort trial has not initialized", opening_hash, recovery_hash;
    std::int64_t probe_time{};
    std::vector<unsigned char> after_progression;
    std::string after_hash;
    bool import_available=false, import_attempted=false, import_loader_ready=false;
    std::string import_nonce, import_session, import_hash, import_phase="idle";
    std::uint64_t import_sequence{};
    ffr::CloneProbeResult import_result;
    std::optional<ffr::CombatReadRequest> combat_request;
    ffr::CombatReadController combat_controller;
    void publish_abort(const Lease& lease);
    void publish_combat(const Lease& lease);
#endif
    std::thread worker;

    std::string envelope(const Lease& lease) {
        return "\"protocol\":1,\"adapter\":\"ffr-helper-native-entry-observer\",\"session\":"
            + ffr::quote(lease.session) + ",\"time\":" + std::to_string(seconds()) + ",\"readOnly\":true";
    }
    void run() noexcept {
        std::shared_ptr<const Opening> published;
        std::string opening_metadata="null";
        while (!quit.load()) {
            try {
                auto lease = read_lease(connection/L"observe.txt");
                if (!lease) { expires.store(0); Sleep(250); continue; }
                std::deque<std::string> copied;
                std::string note;
                std::shared_ptr<const Opening> current_opening;
                bool requested = false;
                {
                    std::lock_guard lock(mutex);
                    if (session != lease->session) {
                        events.clear(); event_bytes = 0; sequence = 0; request.clear(); session = lease->session;
                        opening.reset(); published.reset(); opening_metadata="null";
#ifdef FFR_PRACTICE_BUILD
                        combat_request.reset();combat_controller.clear_latest();
#endif
                    }
                    expires.store(lease->expiry);
                    note = message;
                    current_opening = opening;
                    if (lease->request != std::string(32, '0') && lease->request != request) {
                        copied = events; requested = true;
                    }
                }
                if (current_opening != published) {
                    opening_metadata="null";
                    if (current_opening && !current_opening->progression.empty()) {
                        const auto& payload=current_opening->progression;
                        Sha256 hash; hash.add(payload.data(),payload.size());
                        const auto digest=hash.finish();
                        if (write_atomic(connection/L"opening-progression.gvas",
                                std::string_view(reinterpret_cast<const char*>(payload.data()),payload.size()),ffr::max_progression_bytes)) {
                            opening_metadata="{\"sequence\":"+std::to_string(current_opening->sequence)
                                +",\"time\":"+std::to_string(current_opening->time)
                                +",\"capturePoint\":\"before-native-entry\",\"gameSha256\":"+ffr::quote(ffr::profile::game_sha256)
                                +",\"file\":\"opening-progression.gvas\",\"byteLength\":"+std::to_string(payload.size())
                                +",\"sha256\":"+ffr::quote(digest)+",\"entry\":"+current_opening->entry
                                +",\"fieldAnchor\":"+current_opening->field_anchor
                                +",\"playtimeBasis\":"+current_opening->playtime_basis+'}';
                            published=current_opening;
#ifdef FFR_PRACTICE_BUILD
                            {std::lock_guard lock(mutex); if(opening==current_opening) opening_hash=digest;}
#endif
                        }
                    } else published=current_opening;
                }
                const auto base = envelope(*lease);
                auto status = "{" + base + ",\"hooks\":{\"native-encounter-entry\":"
                    + ffr::quote(ready.load() ? "observing" : "unavailable") + "},\"message\":" + ffr::quote(note) + '}';
                write_atomic(connection/L"observer-status.json", status);
                if (requested) {
                    std::string report = "{" + base + ",\"request\":" + ffr::quote(lease->request)
                        + ",\"format\":\"ffr-helper-runtime-report\",\"version\":1,\"kind\":\"diagnostic-observation\",\"restorable\":false"
                        + ",\"objects\":{\"nativeEntry\":{\"ready\":" + (ready.load() ? "true" : "false")
                        + ",\"captureFailures\":" + std::to_string(failures.load())
                        + ",\"progressionCaptureFailures\":"+std::to_string(progression_failures.load())
                        + ",\"fieldAnchorReady\":"+(field_profile_ready.load()?"true":"false")
                        + ",\"fieldAnchorFailures\":"+std::to_string(field_failures.load())
                        + ",\"playtimeBasisReady\":"+(playtime_profile_ready.load()?"true":"false")
                        + ",\"playtimeBasisFailures\":"+std::to_string(playtime_failures.load())
                        + ",\"opening\":"+opening_metadata+"}},\"events\":[";
                    for (std::size_t i=0; i<copied.size(); ++i) { if (i) report += ','; report += copied[i]; }
                    report += "],\"missingState\":[\"Live verification of serialized progression/resource coverage\",\"Full combat and RNG state\",\"Verified restore path\"]}";
                    if (write_atomic(connection/L"runtime-report.json", report)) {
                        std::lock_guard lock(mutex); request = lease->request;
                    }
                }
#ifdef FFR_PRACTICE_BUILD
                publish_abort(*lease);
                publish_combat(*lease);
#endif
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
#ifdef FFR_PRACTICE_BUILD
#include "abort_runtime.inl"
#endif

std::optional<ffr::FieldAnchor> capture_live_field(void* context) {
    if(!field_profile_ready || !context)return {};
    std::array<std::uint64_t,4> header{};
    const auto address=reinterpret_cast<std::uint64_t>(context);
    if(!read_memory(address,header.data(),sizeof(header)) || !header[0] || !header[2] || (header[1]&0x40000000u))return {};
    auto* world=field_world(context);
    if(!world || !read_memory(reinterpret_cast<std::uint64_t>(world),header.data(),sizeof(header))
        || !header[0] || !header[2] || (header[1]&0x40000000u) || field_map(world)!=context)return {};
    return ffr::capture_field_anchor(read_memory,field_map_id(),reinterpret_cast<std::uint64_t>(field_player()));
}

void initialize_field_profile() {
    const auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    for(const auto& function:{std::pair{ffr::field_profile::get_world_rva,ffr::field_profile::get_world_sha256},
            std::pair{ffr::field_profile::get_map_rva,ffr::field_profile::get_map_sha256},
            std::pair{ffr::field_profile::get_player_rva,ffr::field_profile::get_player_sha256},
            std::pair{ffr::field_profile::get_map_id_rva,ffr::field_profile::get_map_id_sha256}}) {
        std::array<unsigned char,64> code{};
        if(!read_memory(base+function.first,code.data(),code.size()))return;
        Sha256 hash;hash.add(code.data(),code.size());if(hash.finish()!=function.second)return;
    }
    field_world=reinterpret_cast<FieldObjectGetter>(base+ffr::field_profile::get_world_rva);
    field_map=reinterpret_cast<FieldObjectGetter>(base+ffr::field_profile::get_map_rva);
    field_player=reinterpret_cast<FieldPlayerGetter>(base+ffr::field_profile::get_player_rva);
    field_map_id=reinterpret_cast<FieldMapIdGetter>(base+ffr::field_profile::get_map_id_rva);
    field_profile_ready=true;
}

void initialize_playtime_profile() {
    const auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    for(const auto& function:{std::pair{ffr::playtime_profile::get_clock_rva,ffr::playtime_profile::get_clock_sha256},
            std::pair{ffr::playtime_profile::get_seconds_rva,ffr::playtime_profile::get_seconds_sha256}}) {
        std::array<unsigned char,64> code{};
        if(!read_memory(base+function.first,code.data(),code.size()))return;
        Sha256 hash;hash.add(code.data(),code.size());if(hash.finish()!=function.second)return;
    }
    playtime_clock=reinterpret_cast<PlaytimeClockGetter>(base+ffr::playtime_profile::get_clock_rva);
    playtime_seconds=reinterpret_cast<PlaytimeSecondsGetter>(base+ffr::playtime_profile::get_seconds_rva);
    playtime_profile_ready=true;
}

void __fastcall encounter_hook(void* context, const void* entry, bool skip_intro) {
    struct Guard {
        Guard() { in_flight.fetch_add(1); }
        ~Guard() { in_flight.fetch_sub(1); }
    } guard;
    const auto last_error = GetLastError();
    auto* current = published_observer.load(std::memory_order_acquire);
    ffr::observe_and_forward([&] {
        if (!current || current->quit.load()) return;
        if (current->expires.load() < seconds()) {
            // An encounter entered while observation is off cannot inherit a
            // previous encounter's opening when the same session reconnects.
            std::lock_guard lock(current->mutex); current->opening.reset(); return;
        }
        std::string capture_session;
        { std::lock_guard lock(current->mutex); capture_session=current->session; current->opening.reset(); }
        auto copied = ffr::capture_entry(read_memory, reinterpret_cast<std::uint64_t>(entry), skip_intro);
        if (!copied) { current->failures.fetch_add(1); return; }
        auto json = ffr::to_json(*copied);
        if (json.size() > 128*1024) { current->failures.fetch_add(1); return; }
        auto opening=std::make_shared<Observer::Opening>();
        opening->entry=json; opening->time=seconds(); opening->session=capture_session;
        try {
            auto anchor=capture_live_field(context);
            if(anchor)opening->field_anchor=ffr::to_json(*anchor);
            else current->field_failures.fetch_add(1);
        }catch(...) {current->field_failures.fetch_add(1);}
#ifdef FFR_PRACTICE_BUILD
        opening->encounter=std::move(*copied); opening->context_identity=reinterpret_cast<std::uint64_t>(context);
        opening->thread=GetCurrentThreadId();
#endif
        try {
            void* captured_root=nullptr;
            auto progression=ffr::capture_progression(read_memory,[&] {captured_root=root_getter();return captured_root;},save_memory,memory_free);
            if (progression) {
                opening->progression=std::move(*progression);
                try {
                    auto basis=playtime_profile_ready?ffr::capture_playtime_basis(read_memory,captured_root,root_getter,playtime_clock,playtime_seconds):std::nullopt;
                    if(basis)opening->playtime_basis=ffr::to_json(*basis);
                    else current->playtime_failures.fetch_add(1);
                }catch(...) {current->playtime_failures.fetch_add(1);}
            }
            else current->progression_failures.fetch_add(1);
        } catch (...) { current->progression_failures.fetch_add(1); }
        std::lock_guard lock(current->mutex);
        if (current->session!=capture_session || current->expires.load()<seconds() || current->quit.load()) return;
        opening->sequence=current->sequence+1;
        current->opening=std::move(opening);
#ifdef FFR_PRACTICE_BUILD
        current->opening_hash.clear();
#endif
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
    initialize_field_profile();
    initialize_playtime_profile();
    for (const auto& profile : {std::pair{ffr::profile::root_getter_rva,ffr::profile::root_getter_sha256},
                               std::pair{ffr::profile::save_memory_rva,ffr::profile::save_memory_sha256},
                               std::pair{ffr::profile::memory_free_rva,ffr::profile::memory_free_sha256}}) {
        std::array<unsigned char,64> code{};
        if (!read_memory(base+profile.first,code.data(),code.size())) {
            set_message("Progression serializer code is unreadable; hook was not installed"); return false;
        }
        Sha256 hash; hash.add(code.data(),code.size());
        if (hash.finish()!=profile.second) {
            set_message("Progression serializer does not match its verified code profile; hook was not installed"); return false;
        }
    }
    root_getter=reinterpret_cast<RootGetter>(base+ffr::profile::root_getter_rva);
    save_memory=reinterpret_cast<SaveMemory>(base+ffr::profile::save_memory_rva);
    memory_free=reinterpret_cast<MemoryFree>(base+ffr::profile::memory_free_rva);
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
#ifdef FFR_PRACTICE_BUILD
        const auto expected_folder=L"FFRHelperBattlePractice";
#else
        const auto expected_folder=L"FFRHelperRuntimeNative";
#endif
        if (root.filename() != expected_folder || !std::filesystem::is_directory(root/L"connection")) return nullptr;
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
        const auto installed=install_entry_hook();
#ifdef FFR_PRACTICE_BUILD
        if(installed)install_abort_tick_hook();
#else
        (void)installed;
#endif
        return host_object;
    } catch (...) {
        if (observer) {
            observer->expires.store(0); observer->quit.store(true);
#ifdef FFR_PRACTICE_BUILD
            shutdown_abort_tick_hook();
#endif
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
#ifdef FFR_PRACTICE_BUILD
            shutdown_abort_tick_hook();
#endif
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
extern "C" __declspec(dllexport) const char* ffr_observer_runtime_mode() noexcept {
#ifdef FFR_PRACTICE_BUILD
    return "experimental-abort-test";
#else
    return "read-only-observer";
#endif
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { own_module = instance; DisableThreadLibraryCalls(instance); }
    return TRUE;
}
