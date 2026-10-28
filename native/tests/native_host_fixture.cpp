#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "encounter_capture.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <stdexcept>
#include <cstring>

#define CHECK(condition) do { if (!(condition)) throw std::runtime_error("Fixture check failed at line " + std::to_string(__LINE__)); } while (false)
namespace {
volatile unsigned calls{};
void* last_context{};
const void* last_data{};
bool last_skip{};
std::int32_t last_friend{};
DWORD last_input_error{};
unsigned serialized{}, released{}, last_serialized{};
unsigned fixture_errors{};
std::array<unsigned char,32> fake_root{};
}
extern "C" __declspec(dllexport) __declspec(noinline)
void* __fastcall ffr_test_root_getter() {
    return fake_root.data();
}
extern "C" __declspec(dllexport) __declspec(noinline)
bool __fastcall ffr_test_save_memory(void* object, ffr::RawArray* output) {
    if (object!=fake_root.data()) { ++fixture_errors; return false; } ++serialized;
    const unsigned char bytes[]{'G','V','A','S',3,0,0,0,static_cast<unsigned char>(41+serialized),7,9,0};
    auto* data=HeapAlloc(GetProcessHeap(),0,sizeof(bytes)); if (!data) return false;
    std::memcpy(data,bytes,sizeof(bytes));
    *output={reinterpret_cast<std::uint64_t>(data),sizeof(bytes),sizeof(bytes)};
    return true;
}
extern "C" __declspec(dllexport) __declspec(noinline)
void __fastcall ffr_test_memory_free(void* data) {
    if (data) { ++released; if (!HeapFree(GetProcessHeap(),0,data)) ++fixture_errors; }
}
extern "C" __declspec(dllexport) __declspec(noinline)
void __fastcall ffr_test_encounter(void* context, const void* data, bool skip) {
    last_input_error = GetLastError();
    last_serialized=serialized;
    calls = calls+1;
    last_context = context; last_data = data; last_skip = skip;
    const auto* entry = static_cast<const ffr::RawEntry*>(data);
    if (entry->friend_ids.count) last_friend = *reinterpret_cast<const std::int32_t*>(entry->friend_ids.data);
    MemoryBarrier();
}

namespace {
std::string read(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), {});
}
void write(const std::filesystem::path& path, const std::string& value) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << value; out.close(); CHECK(out.good());
}
std::int64_t now() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
template<class Condition> void wait_for(Condition condition) {
    auto deadline = std::chrono::steady_clock::now()+std::chrono::seconds(8);
    while (!condition()) {
        CHECK(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
template<class T> ffr::RawArray array(std::vector<T>& value) {
    return {reinterpret_cast<std::uint64_t>(value.data()), static_cast<std::int32_t>(value.size()), static_cast<std::int32_t>(value.size())};
}
}

int wmain(int argc, wchar_t** argv) {
    try {
        CHECK(argc == 5);
        const std::filesystem::path module_path = argv[1];
        const std::filesystem::path loader_path = argv[2];
        const std::filesystem::path connection = argv[3];
        const std::wstring mode = argv[4];
        const std::uint64_t klass=1; std::memcpy(fake_root.data()+16,&klass,sizeof(klass));
        auto loader = LoadLibraryW(loader_path.c_str()); CHECK(loader);
        auto host_count = reinterpret_cast<int(*)()>(GetProcAddress(loader, "ffr_test_hosts_alive")); CHECK(host_count);
        auto host_errors = reinterpret_cast<int(*)()>(GetProcAddress(loader, "ffr_test_host_errors")); CHECK(host_errors);
        auto native = LoadLibraryW(module_path.c_str()); CHECK(native);
        auto start = reinterpret_cast<void*(*)()>(GetProcAddress(native, "start_mod")); CHECK(start);
        auto stop = reinterpret_cast<void(*)(void*)>(GetProcAddress(native, "uninstall_mod")); CHECK(stop);
        auto entry_function = reinterpret_cast<void(__fastcall*)(void*, const void*, bool)>(
            GetProcAddress(GetModuleHandleW(nullptr), "ffr_test_encounter")); CHECK(entry_function);
        unsigned char initial_code[64]{};
        std::memcpy(initial_code, reinterpret_cast<const void*>(entry_function), sizeof(initial_code));
        auto object = start();
        if (mode == L"bad-loader") {
            CHECK(object == nullptr && host_count() == 0);
            CHECK(std::memcmp(initial_code, reinterpret_cast<const void*>(entry_function), sizeof(initial_code)) == 0);
            std::cout << "Unsupported loader refused before host construction.\n";
            return 0;
        }
        CHECK(object != nullptr && host_count() == 1);
        CHECK(start() == nullptr); // Restart is required; no duplicate detour/worker.
        std::vector<std::int32_t> friends{1001, 1002}, enemies{11, 22}, positions{0, 1}, skills{77, 88};
        std::vector<ffr::RawArray> passives{array(skills)};
        std::vector<char16_t> bgm{u'F', u'i', u'x', u't', u'u', u'r', u'e', 0};
        ffr::RawEntry entry{};
        entry.friend_ids = array(friends); entry.friend_locations = array(positions);
        entry.enemy_ids = array(enemies); entry.enemy_locations = array(positions);
        entry.passives = array(passives); entry.bgm = array(bgm); entry.group = 123; entry.stage = 456;
        entry.can_escape = 1; entry.escape_probability = 0.75f;
        void* context = reinterpret_cast<void*>(0x12345678);
        entry_function(context, &entry, true);
        CHECK(calls == 1 && last_context == context && last_data == &entry && last_skip && last_friend == 1001);
        auto lease = [&](const std::string& nonce, std::int64_t expiry) {
            write(connection/L"observe.txt", "FFR_HELPER_OBSERVER_V1\n"+std::string(32,'a')+"\n"+std::to_string(expiry)+"\n"+nonce+"\n");
        };
        lease(std::string(32, '0'), now()+8);
        wait_for([&] { return !read(connection/L"observer-status.json").empty(); });
        if (mode != L"good") {
            CHECK(read(connection/L"observer-status.json").find("unavailable") != std::string::npos);
            CHECK(std::memcmp(initial_code, reinterpret_cast<const void*>(entry_function), sizeof(initial_code)) == 0);
        } else {
            CHECK(read(connection/L"observer-status.json").find("observing") != std::string::npos);
            CHECK(std::memcmp(initial_code, reinterpret_cast<const void*>(entry_function), sizeof(initial_code)) != 0);
        }
        SetLastError(1234);
        entry_function(context, &entry, false);
        CHECK(calls == 2 && !last_skip && last_input_error == 1234);
        CHECK(serialized==released && last_serialized==(mode==L"good" ? 1u : 0u));
        friends[0] = 9999; enemies[0] = 999; skills[0] = 999; bgm[0] = u'X';
        lease(std::string(32,'b'), now()+8);
        wait_for([&] { return read(connection/L"runtime-report.json").find(std::string(32,'b')) != std::string::npos; });
        const auto snapshot = read(connection/L"runtime-report.json");
        write(connection/L"first-report.json", snapshot);
        if (mode == L"good") {
            CHECK(snapshot.find("encounter-opening-native") != std::string::npos);
            CHECK(snapshot.find("\"friendliesUnitIdList\":[1001,1002]") != std::string::npos);
            CHECK(snapshot.find("\"bgmCueName\":\"Fixture\"") != std::string::npos);
            CHECK(snapshot.find("\"byteLength\":12") != std::string::npos);
            CHECK(snapshot.find("\"progressionCaptureFailures\":0") != std::string::npos);
            write(connection/L"first-progression.gvas",read(connection/L"opening-progression.gvas"));
            auto bad_entry = entry; bad_entry.enemy_ids.count = -1;
            entry_function(context, &bad_entry, true);
            CHECK(calls == 3 && last_data == &bad_entry && last_friend == 9999);
            CHECK(serialized==1 && released==1);
            lease(std::string(32,'c'), now()+8);
            wait_for([&] { return read(connection/L"runtime-report.json").find(std::string(32,'c')) != std::string::npos; });
            CHECK(read(connection/L"runtime-report.json").find("\"captureFailures\":1") != std::string::npos);
            CHECK(read(connection/L"runtime-report.json").find("\"opening\":null") != std::string::npos);
            entry_function(context,&entry,false); CHECK(calls==4 && serialized==2 && released==2);
            lease(std::string(32,'d'),now()+8);
            wait_for([&] { return read(connection/L"runtime-report.json").find(std::string(32,'d')) != std::string::npos; });
            write(connection/L"replacement-report.json",read(connection/L"runtime-report.json"));
            write(connection/L"replacement-progression.gvas",read(connection/L"opening-progression.gvas"));
            lease(std::string(32,'d'), 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
            entry_function(context, &entry, true); CHECK(calls == 5);
            CHECK(serialized==2 && released==2);
            lease(std::string(32,'e'), now()+8);
            wait_for([&] { return read(connection/L"runtime-report.json").find(std::string(32,'e')) != std::string::npos; });
            write(connection/L"after-expiry-report.json", read(connection/L"runtime-report.json"));
        } else CHECK(snapshot.find("encounter-opening-native") == std::string::npos);
        stop(object);
        CHECK(host_count() == 0 && host_errors() == 0);
        CHECK(fixture_errors==0);
        CHECK(std::memcmp(initial_code, reinterpret_cast<const void*>(entry_function), sizeof(initial_code)) == 0);
        const auto before = calls;
        entry_function(context, &entry, false); CHECK(calls == before+1);
        CHECK(start() == nullptr);
        FreeLibrary(native); FreeLibrary(loader);
        std::cout << "Native observer fixture passed without a game process.\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << " Win32=" << GetLastError() << '\n';
        return 1;
    }
}
