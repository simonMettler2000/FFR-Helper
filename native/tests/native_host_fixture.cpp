#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "encounter_capture.hpp"
#include "clone_payload.hpp"
#include "recovery_payload.hpp"
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
std::array<unsigned char,0x40> fake_root{};
std::array<unsigned char,0x48> fake_playtime{};
std::array<unsigned char,0x40> fake_clone{};
bool clone_fixture{},clone_mismatch{},clone_mutates{};
bool normalization_fixture{},normalization_template{};
unsigned clone_loads{};
bool recovery_fixture{},recovery_drift{};
std::int32_t recovery_money=101,fixture_map_id=1000;
std::array<unsigned char,0x280> fake_manager{};
std::array<unsigned char,0x80> fake_jobs{},fake_units{};
std::array<unsigned char,0x40> fake_command{},fake_command_request{};
std::array<unsigned char,0x40> fake_other_command{};
void* wait_argument{};
std::array<unsigned char,0x90> fake_ui{};
std::array<unsigned char,32> fake_world{};
std::array<unsigned char,0x210> fake_map{};
std::array<unsigned char,0x38> fake_map_storage{};
std::array<std::array<unsigned char,0x2b0>,2> fake_map_parameters{};
std::array<unsigned char,0x800> fake_field_player{};
std::array<unsigned char,0x230> fake_field_component{};
std::array<unsigned char,0xd0> fake_field_operation{};
std::array<std::array<unsigned char,0x880>,2> fake_actors{};
std::array<std::array<unsigned char,0x560>,2> fake_parameters{};
std::array<unsigned char,0x40> fake_scheduler{};
unsigned abort_calls{},tick_calls{};
DWORD abort_thread{};
unsigned wait_gate_calls{};
unsigned state_wait_calls{};
bool release_managers_on_abort{};
unsigned decision_calls{};void* decision_context{};void* decision_data{};DWORD decision_input_error{};
unsigned dispatch_calls{},skill_producer_calls{},unrelated_producer_calls{};
void* dispatch_context{};void* dispatch_data{};std::uint64_t dispatch_name{};DWORD dispatch_input_error{};
std::array<unsigned char,0x790> fake_rng_data{};
unsigned rand_calls{},rng_helper_calls{};
unsigned engine_rng_draw_calls{},engine_rng_initialize_calls{};
bool engine_rng_drift{};
float last_delta{};
}
extern "C" {__declspec(dllexport) std::uint64_t ffr_test_btl_vtable[1]={0};}
extern "C" {__declspec(dllexport) std::uint64_t ffr_test_save_vtable[1]={0};}
extern "C" {__declspec(dllexport) std::uint64_t ffr_test_map_storage_vtable[1]={0};}
extern "C" {
__declspec(dllexport) std::uint8_t ffr_test_rng_ready=1,ffr_test_rng_fast=0;
__declspec(dllexport) std::uint32_t ffr_test_rng_data_slot=0xffffffffu,ffr_test_rng_state_slot=0xffffffffu;
__declspec(dllexport) std::uint64_t ffr_test_rng_getter=0,ffr_test_rand_import=0;
__declspec(dllexport) std::uint32_t ffr_test_engine_rng_seed=0xffffffffu;
}
extern "C" __declspec(dllexport) __declspec(noinline) unsigned ffr_test_crt_rand(){++rand_calls;return 1;}
extern "C" __declspec(dllexport) __declspec(noinline) void* ffr_test_crt_helper(){++rng_helper_calls;return nullptr;}
extern "C" __declspec(dllexport) __declspec(noinline) unsigned ffr_test_engine_rng_draw(){++engine_rng_draw_calls;return ffr_test_engine_rng_seed;}
extern "C" __declspec(dllexport) __declspec(noinline) void ffr_test_engine_rng_initialize(){++engine_rng_initialize_calls;}
extern "C" __declspec(dllexport) __declspec(noinline)
void __fastcall ffr_test_dispatch(void* context,std::uint64_t name,void* data) {
    ++dispatch_calls;dispatch_context=context;dispatch_name=name;dispatch_data=data;dispatch_input_error=GetLastError();
    SetLastError(8642);
}
extern "C" __declspec(dllexport) __declspec(noinline)
void __fastcall ffr_test_skill_begin(void* context,std::uint64_t name,void* data) {
    ffr_test_dispatch(context,name,data);++skill_producer_calls;
}
extern "C" __declspec(dllexport) __declspec(noinline)
void __fastcall ffr_test_unrelated_event(void* context,std::uint64_t name,void* data) {
    ffr_test_dispatch(context,name,data);++unrelated_producer_calls;
}
extern "C" __declspec(dllexport) __declspec(noinline)
void __fastcall ffr_test_decision(void* context,void* data) {
    ++decision_calls;decision_context=context;decision_data=data;decision_input_error=GetLastError();
    SetLastError(2468);
}
extern "C" __declspec(dllexport) __declspec(noinline)
void* __fastcall ffr_test_field_get_world(void* map) {
    if(map!=fake_map.data()){++fixture_errors;return nullptr;}return fake_world.data();
}
extern "C" __declspec(dllexport) __declspec(noinline)
void* __fastcall ffr_test_field_get_player() {return fake_field_player.data();}
extern "C" __declspec(dllexport) __declspec(noinline)
std::int32_t __fastcall ffr_test_field_get_map_id() {return fixture_map_id;}
extern "C" __declspec(dllexport) __declspec(noinline)
void* __fastcall ffr_test_abort_get_world(void* manager) {
    if(manager!=fake_manager.data()){++fixture_errors;return nullptr;}return fake_world.data();
}
extern "C" __declspec(dllexport) __declspec(noinline)
void* __fastcall ffr_test_abort_get_map(void* world) {
    if(world!=fake_world.data()){++fixture_errors;return nullptr;}return fake_map.data();
}
extern "C" __declspec(dllexport) __declspec(noinline)
void* __fastcall ffr_test_abort_get_ui(void* world) {
    if(world!=fake_world.data()){++fixture_errors;return nullptr;}return fake_ui.data();
}
extern "C" __declspec(dllexport) __declspec(noinline)
void __fastcall ffr_test_abort_end_battle() {
    abort_thread=GetCurrentThreadId();++abort_calls;fake_manager[0x222]=1;
}
extern "C" __declspec(dllexport) __declspec(noinline)
bool __fastcall ffr_test_command_wait(void* command) {
    ++wait_gate_calls;ffr::RawArray requests{};
    std::memcpy(&requests,static_cast<unsigned char*>(command)+0x28,sizeof(requests));
    bool waiting=false;
    for(int i=0;i<requests.count;++i)
        if(!*(reinterpret_cast<unsigned char*>(requests.data)+64*i+0x38))waiting=true;
    SetLastError(6543);return waiting;
}
extern "C" __declspec(dllexport) __declspec(noinline)
bool __fastcall ffr_test_state_wait(void* command) {
    const auto waiting=ffr_test_command_wait(command);++state_wait_calls;return waiting;
}
extern "C" __declspec(dllexport) __declspec(noinline)
void __fastcall ffr_test_abort_tick(void* context,float delta) {
    ++tick_calls;last_delta=delta;
    if(context!=fake_manager.data()+0x30)++fixture_errors;
    // Like FFR: the nested helper can return before the outer wait/ForceEnd check.
    if(ffr_test_state_wait(wait_argument?wait_argument:fake_command.data())) {SetLastError(9876);return;}
    const auto waiting=ffr_test_command_wait(wait_argument?wait_argument:fake_command.data());
    if(fake_manager[0x222] && !waiting) {
        fake_manager[0x222]=0;fake_manager[0x1a9]=0;fake_ui[0x80]=0;
        const std::int32_t zero=0;std::memcpy(fake_units.data()+0x30,&zero,4);
        if(release_managers_on_abort) {
            const std::uint64_t none=0;std::memcpy(fake_manager.data()+0x40,&none,8);std::memcpy(fake_manager.data()+0x48,&none,8);
        }
    }
    SetLastError(9876);
}
extern "C" __declspec(dllexport) __declspec(noinline)
void* __fastcall ffr_test_root_getter() {
    return fake_root.data();
}
extern "C" __declspec(dllexport) __declspec(noinline)
void* __fastcall ffr_test_playtime_get_clock() {return fake_playtime.data();}
extern "C" __declspec(dllexport) __declspec(noinline)
std::int64_t __fastcall ffr_test_playtime_get_seconds() {return 10000;}
extern "C" __declspec(dllexport) __declspec(noinline)
bool __fastcall ffr_test_save_memory(void* object, ffr::RawArray* output) {
    if(engine_rng_drift)++ffr_test_engine_rng_seed; // Simulates an unrelated concurrent draw in this fixture only.
    if (object!=fake_root.data() && !((clone_fixture || normalization_fixture) && object==fake_clone.data())) { ++fixture_errors; return false; } ++serialized;
    auto bytes=clone_fixture?clone_fixture_payload():std::vector<unsigned char>{'G','V','A','S',3,0,0,0,static_cast<unsigned char>(41+serialized),7,9,0};
    if(recovery_fixture) {float saved{};std::memcpy(&saved,fake_root.data()+0x38,4);bytes=recovery_fixture_data::payload(saved,recovery_money+(recovery_drift?static_cast<std::int32_t>(serialized):0));}
    if(normalization_fixture) {float saved{};std::memcpy(&saved,static_cast<unsigned char*>(object)+0x38,4);bytes=recovery_fixture_data::payload(saved,object==fake_root.data()?recovery_money:101);}
    if(clone_fixture && ((clone_mismatch && object==fake_clone.data()) || (clone_mutates && clone_loads && object==fake_root.data())))bytes[8]^=1;
    auto* data=HeapAlloc(GetProcessHeap(),0,bytes.size()); if (!data) return false;
    std::memcpy(data,bytes.data(),bytes.size());
    *output={reinterpret_cast<std::uint64_t>(data),static_cast<std::int32_t>(bytes.size()),static_cast<std::int32_t>(bytes.size())};
    return true;
}
extern "C" __declspec(dllexport) __declspec(noinline)
void* __fastcall ffr_test_load_memory(const ffr::RawArray* input) {
    ++clone_loads;const auto expected=normalization_fixture?recovery_fixture_data::payload(1000.25f,101):clone_fixture_payload();
    if((!clone_fixture && !normalization_fixture) || input->count!=static_cast<int>(expected.size())
        || input->capacity!=input->count || std::memcmp(reinterpret_cast<void*>(input->data),expected.data(),expected.size())) {
        ++fixture_errors;return nullptr;
    }
    if(normalization_fixture) {
        const float saved=1000.25f;std::memcpy(fake_clone.data()+0x38,&saved,4);
        if(normalization_template){const std::uint32_t flags=0x10;std::memcpy(fake_clone.data()+8,&flags,4);}
    }
    return fake_clone.data();
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
    // Match the production reader's delete sharing: status publication replaces
    // files atomically and must not race a fixture's CRT sharing mode.
    const auto error=GetLastError();
    struct Restore {DWORD value;~Restore(){SetLastError(value);}} restore{error};
    const auto file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return {};
    struct Close {HANDLE value;~Close(){CloseHandle(value);}} close{file};
    LARGE_INTEGER size{};
    if(!GetFileSizeEx(file,&size)||size.QuadPart<0||size.QuadPart>32*1024*1024)return {};
    std::string raw(static_cast<std::size_t>(size.QuadPart),'\0');DWORD count{};
    if(!ReadFile(file,raw.data(),static_cast<DWORD>(raw.size()),&count,nullptr)||count!=raw.size())return {};
    return raw;
}
void write(const std::filesystem::path& path, const std::string& value) {
    auto temporary=path;temporary+=L".fixture-tmp";
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    out << value; out.close(); CHECK(out.good());
    CHECK(MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH));
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
void run_abort_fixture(const std::filesystem::path& connection,const std::wstring& mode);
void run_clone_fixture(const std::filesystem::path& connection,const std::wstring& mode,HMODULE loader);
void run_combat_fixture(const std::filesystem::path& connection,const std::wstring& mode);
void run_recovery_fixture(const std::filesystem::path& connection,const std::wstring& mode);
void run_normalization_fixture(const std::filesystem::path& connection,const std::wstring& mode,HMODULE loader);
void run_menu_fixture(const std::filesystem::path& connection,bool available);
void run_skill_fixture(const std::filesystem::path& connection,bool available);
}

#include "abort_fixture.inl"
#include "clone_fixture.inl"
#include "combat_fixture.inl"
#include "recovery_fixture.inl"
#include "normalization_fixture.inl"
#include "menu_fixture.inl"
#include "skill_fixture.inl"
int wmain(int argc, wchar_t** argv) {
    try {
        CHECK(argc == 5);
        const std::filesystem::path module_path = argv[1];
        const std::filesystem::path loader_path = argv[2];
        const std::filesystem::path connection = argv[3];
        const std::wstring mode = argv[4];
        recovery_fixture=mode.starts_with(L"recovery");recovery_drift=mode==L"recovery-drift";
        normalization_fixture=mode.starts_with(L"normalize");normalization_template=mode==L"normalize-template";
        const std::uint64_t klass=1; std::memcpy(fake_root.data()+16,&klass,sizeof(klass));
        object(fake_world);object(fake_map);object(fake_field_player);object(fake_field_component);object(fake_field_operation);
        object(fake_root);object(fake_playtime);
        object(fake_map_storage,reinterpret_cast<std::uint64_t>(ffr_test_map_storage_vtable));
        put(fake_map,0x208,reinterpret_cast<std::uint64_t>(fake_map_storage.data()));
        std::vector<std::uint64_t> map_parameters;
        for(unsigned i=0;i<2;++i) {
            object(fake_map_parameters[i]);put(fake_map_parameters[i],0xd0,std::int32_t{1001}); // Distinct objects may share an ID.
            put(fake_map_parameters[i],0x100,std::int32_t{12});put(fake_map_parameters[i],0x10c,std::int32_t{150});
            put(fake_map_parameters[i],0x110,std::int32_t{-20});map_parameters.push_back(reinterpret_cast<std::uint64_t>(fake_map_parameters[i].data()));
        }
        put(fake_map_storage,0x28,array(map_parameters));
        ffr_test_rng_data_slot=FlsAlloc(nullptr);ffr_test_rng_state_slot=FlsAlloc(nullptr);
        CHECK(ffr_test_rng_data_slot!=FLS_OUT_OF_INDEXES && ffr_test_rng_state_slot!=FLS_OUT_OF_INDEXES);
        CHECK(FlsSetValue(ffr_test_rng_data_slot,fake_rng_data.data()) && FlsSetValue(ffr_test_rng_state_slot,nullptr));
        ffr_test_rng_getter=reinterpret_cast<std::uint64_t>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"FlsGetValue"));
        ffr_test_rand_import=reinterpret_cast<std::uint64_t>(&ffr_test_crt_rand);put(fake_rng_data,0x28,std::uint32_t{1234567});
        put(fake_root,0x38,1000.25f);
        put(fake_playtime,0x30,std::array<std::int64_t,3>{9000,0,30});
        put(fake_field_player,0x1b8,reinterpret_cast<std::uint64_t>(fake_field_component.data()));
        put(fake_field_player,0x778,reinterpret_cast<std::uint64_t>(fake_field_operation.data()));
        put(fake_field_component,0x200,std::array<double,3>{123.125,-456.25,7.5});fake_field_operation[0xc9]=2;
        auto loader = LoadLibraryW(loader_path.c_str()); CHECK(loader);
        auto host_count = reinterpret_cast<int(*)()>(GetProcAddress(loader, "ffr_test_hosts_alive")); CHECK(host_count);
        auto host_errors = reinterpret_cast<int(*)()>(GetProcAddress(loader, "ffr_test_host_errors")); CHECK(host_errors);
        auto* decision_code=reinterpret_cast<unsigned char*>(&ffr_test_decision);
        std::array<unsigned char,64> initial_decision{};std::memcpy(initial_decision.data(),decision_code,64);
        if(mode==L"decision-unavailable") {
            DWORD protection{},unused{};CHECK(VirtualProtect(decision_code,1,PAGE_EXECUTE_READWRITE,&protection));
            *decision_code^=1;CHECK(VirtualProtect(decision_code,1,protection,&unused));
            FlushInstructionCache(GetCurrentProcess(),decision_code,1);
        }
        auto native = LoadLibraryW(module_path.c_str()); CHECK(native);
        auto start = reinterpret_cast<void*(*)()>(GetProcAddress(native, "start_mod")); CHECK(start);
        auto stop = reinterpret_cast<void(*)(void*)>(GetProcAddress(native, "uninstall_mod")); CHECK(stop);
        auto entry_function = reinterpret_cast<void(__fastcall*)(void*, const void*, bool)>(
            GetProcAddress(GetModuleHandleW(nullptr), "ffr_test_encounter")); CHECK(entry_function);
        unsigned char initial_code[64]{};
        std::memcpy(initial_code, reinterpret_cast<const void*>(entry_function), sizeof(initial_code));
        auto* fixture_tick=reinterpret_cast<const void*>(GetProcAddress(GetModuleHandleW(nullptr),"ffr_test_abort_tick"));CHECK(fixture_tick);
        std::array<unsigned char,64> initial_tick{};std::memcpy(initial_tick.data(),fixture_tick,initial_tick.size());
        auto* producer_code=reinterpret_cast<unsigned char*>(&ffr_test_skill_begin);
        const auto initial_producer=*producer_code;
        auto* dispatch_code=reinterpret_cast<unsigned char*>(&ffr_test_dispatch);
        std::array<unsigned char,64> initial_dispatch{};std::memcpy(initial_dispatch.data(),dispatch_code,64);
        if(mode==L"skill-unavailable") {
            DWORD protection{},unused{};CHECK(VirtualProtect(producer_code,1,PAGE_EXECUTE_READWRITE,&protection));
            *producer_code^=1;CHECK(VirtualProtect(producer_code,1,protection,&unused));FlushInstructionCache(GetCurrentProcess(),producer_code,1);
        }
        if(mode==L"menu-unavailable") {
            auto* code=const_cast<unsigned char*>(static_cast<const unsigned char*>(fixture_tick));
            DWORD protection{},unused{};CHECK(VirtualProtect(code,1,PAGE_EXECUTE_READWRITE,&protection));
            *code^=1;CHECK(VirtualProtect(code,1,protection,&unused));FlushInstructionCache(GetCurrentProcess(),code,1);
        }
        auto object = start();
        if (mode == L"bad-loader") {
            CHECK(object == nullptr && host_count() == 0);
            CHECK(std::memcmp(initial_code, reinterpret_cast<const void*>(entry_function), sizeof(initial_code)) == 0);
            std::cout << "Unsupported loader refused before host construction.\n";
            return 0;
        }
        CHECK(object != nullptr && host_count() == 1);
        CHECK(start() == nullptr); // Restart is required; no duplicate detour/worker.
        if(mode.starts_with(L"skill-")) {
            run_skill_fixture(connection,mode==L"skill-history");
            if(mode==L"skill-unavailable") {
                DWORD protection{},unused{};CHECK(VirtualProtect(producer_code,1,PAGE_EXECUTE_READWRITE,&protection));
                *producer_code=initial_producer;CHECK(VirtualProtect(producer_code,1,protection,&unused));FlushInstructionCache(GetCurrentProcess(),producer_code,1);
            }
            stop(object);CHECK(host_count()==0 && host_errors()==0 && fixture_errors==0);
            CHECK(std::memcmp(initial_dispatch.data(),dispatch_code,64)==0 && abort_calls==0 && clone_loads==0);
            std::cout<<"Skill.Begin return-site history fixture passed without game actions.\n";return 0;
        }
        if(mode.starts_with(L"menu-")) {
            run_menu_fixture(connection,mode==L"menu-frame");
            if(mode==L"menu-unavailable") {
                auto* code=const_cast<unsigned char*>(static_cast<const unsigned char*>(fixture_tick));
                DWORD protection{},unused{};CHECK(VirtualProtect(code,1,PAGE_EXECUTE_READWRITE,&protection));
                *code=initial_tick[0];CHECK(VirtualProtect(code,1,protection,&unused));FlushInstructionCache(GetCurrentProcess(),code,1);
            }
            stop(object);CHECK(host_count()==0 && host_errors()==0 && fixture_errors==0);
            CHECK(std::memcmp(initial_tick.data(),fixture_tick,initial_tick.size())==0);
            CHECK(abort_calls==0 && clone_loads==0);
            std::cout<<"Read-only menu-frame fixture passed without practice dispatch.\n";return 0;
        }
        if(mode.starts_with(L"decision-")) {
            const auto active=mode==L"decision-history";
            auto lease=[&](char session,std::int64_t expiry) {
                write(connection/L"observe.txt","FFR_HELPER_OBSERVER_V1\n"+std::string(32,session)+"\n"+std::to_string(expiry)+"\n"+std::string(32,'0')+"\n");
            };
            lease('a',now()+8);
            wait_for([&]{return read(connection/L"observer-status.json").find(std::string(32,'a'))!=std::string::npos;});
            CHECK(read(connection/L"observer-status.json").find("\"decisionHistory\":null")!=std::string::npos);
            std::vector<std::int32_t> friends{1001};ffr::RawEntry entry{};entry.friend_ids=array(friends);
            entry_function(fake_map.data(),&entry,false);CHECK(serialized==1 && calls==1);
            if(active) {
                wait_for([&]{return read(connection/L"observer-status.json").find("\"handlerEntries\":0")!=std::string::npos;});
                const auto source=read(connection/L"opening-progression.gvas");
                CHECK(std::memcmp(initial_decision.data(),decision_code,64)!=0);
                SetLastError(1357);ffr_test_decision(fake_manager.data(),fake_ui.data());
                CHECK(GetLastError()==2468 && decision_calls==1 && decision_input_error==1357);
                CHECK(decision_context==fake_manager.data() && decision_data==fake_ui.data());
                wait_for([&]{return read(connection/L"observer-status.json").find("\"handlerEntries\":1")!=std::string::npos;});
                write(connection/L"decision-first.json",read(connection/L"observer-status.json"));
                CHECK(read(connection/L"opening-progression.gvas")==source && serialized==1);
                std::thread other([]{ffr_test_decision(nullptr,nullptr);});other.join();
                wait_for([&]{return read(connection/L"observer-status.json").find("\"foreignThreadObserved\":true")!=std::string::npos;});
                write(connection/L"decision-foreign.json",read(connection/L"observer-status.json"));
                lease('a',0);std::this_thread::sleep_for(std::chrono::milliseconds(800));
                lease('a',now()+8);
                wait_for([&]{return read(connection/L"observer-status.json").find("\"decisionHistory\":null")!=std::string::npos;});
                // Reconnecting to the same encounter cannot invent missed history.
                ffr_test_decision(nullptr,nullptr);std::this_thread::sleep_for(std::chrono::milliseconds(300));
                CHECK(read(connection/L"observer-status.json").find("\"decisionHistory\":null")!=std::string::npos);
                entry_function(fake_map.data(),&entry,false);
                wait_for([&]{return read(connection/L"observer-status.json").find("\"handlerEntries\":0")!=std::string::npos;});
                lease('b',now()+8);
                wait_for([&]{const auto s=read(connection/L"observer-status.json");return s.find(std::string(32,'b'))!=std::string::npos && s.find("\"decisionHistory\":null")!=std::string::npos;});
            }else {
                wait_for([&]{return !read(connection/L"opening-progression.gvas").empty();});
                CHECK(read(connection/L"observer-status.json").find("\"decisionHistory\":null")!=std::string::npos);
                DWORD protection{},unused{};CHECK(VirtualProtect(decision_code,1,PAGE_EXECUTE_READWRITE,&protection));
                *decision_code=initial_decision[0];CHECK(VirtualProtect(decision_code,1,protection,&unused));
                FlushInstructionCache(GetCurrentProcess(),decision_code,1);
            }
            stop(object);CHECK(host_count()==0 && host_errors()==0 && fixture_errors==0);
            CHECK(abort_calls==0 && clone_loads==0);
            CHECK(std::memcmp(initial_decision.data(),decision_code,64)==0);
            std::cout<<"Read-only decision history fixture passed; no practice commands enabled.\n";return 0;
        }
        if(mode==L"abort-wait-unavailable") {
            write(connection/L"observe.txt","FFR_HELPER_OBSERVER_V1\n"+std::string(32,'a')+"\n"+std::to_string(now()+8)+"\n"+std::string(32,'b')+"\n");
            wait_for([&]{return read(connection/L"abort-status.json").find("Scoped command wait profile unavailable")!=std::string::npos;});
            const auto status=read(connection/L"abort-status.json");
            CHECK(status.find("\"available\":false")!=std::string::npos);
            CHECK(status.find("\"commandWaitReady\":false")!=std::string::npos);
            CHECK(abort_calls==0 && clone_loads==0 && serialized==0);
            stop(object);CHECK(host_count()==0 && host_errors()==0 && fixture_errors==0);
            std::cout<<"Command wait mismatch disables abort before any game action.\n";return 0;
        }
        if(mode==L"boundary-unreviewed" || mode==L"abort-full-unavailable") {
            write(connection/L"observe.txt","FFR_HELPER_OBSERVER_V1\n"+std::string(32,'a')+"\n"+std::to_string(now()+8)+"\n"+std::string(32,'b')+"\n");
            const auto expected=mode==L"boundary-unreviewed"?"Opening command boundary is unreviewed":"Full EndBattle envelope mismatch";
            wait_for([&]{return read(connection/L"abort-status.json").find(expected)!=std::string::npos;});
            const auto status=read(connection/L"abort-status.json");
            CHECK(status.find("\"available\":false")!=std::string::npos);
            CHECK(std::memcmp(initial_tick.data(),fixture_tick,initial_tick.size())==0);
            CHECK(abort_calls==0 && clone_loads==0 && serialized==0);
            stop(object);CHECK(host_count()==0 && host_errors()==0 && fixture_errors==0);
            CHECK(std::memcmp(initial_code,reinterpret_cast<const void*>(entry_function),sizeof(initial_code))==0);
            std::cout<<"Unreviewed boundary refused before practice tick hook or import initialization.\n";return 0;
        }
        if(mode.starts_with(L"normalize")) {
            run_normalization_fixture(connection,mode,loader);stop(object);
            CHECK(host_count()==0 && host_errors()==0 && fixture_errors==0);
            CHECK(std::memcmp(initial_code,reinterpret_cast<const void*>(entry_function),sizeof(initial_code))==0);
            std::cout<<"Detached normalization DLL fixture passed without FFR.\n";return 0;
        }
        if(mode.starts_with(L"recovery")) {
            run_recovery_fixture(connection,mode);stop(object);
            CHECK(host_count()==0 && host_errors()==0 && fixture_errors==0);
            CHECK(std::memcmp(initial_code,reinterpret_cast<const void*>(entry_function),sizeof(initial_code))==0);
            std::cout<<"Read-only coherent recovery DLL fixture passed without FFR.\n";return 0;
        }
        if(mode.starts_with(L"combat")) {
            run_combat_fixture(connection,mode);stop(object);
            CHECK(host_count()==0 && host_errors()==0 && fixture_errors==0);
            CHECK(std::memcmp(initial_code,reinterpret_cast<const void*>(entry_function),sizeof(initial_code))==0);
            std::cout<<"Read-only command-boundary combat DLL fixture passed without FFR.\n";return 0;
        }
        if(mode.starts_with(L"clone")) {
            run_clone_fixture(connection,mode,loader);stop(object);
            CHECK(host_count()==0 && host_errors()==0 && fixture_errors==0);
            CHECK(std::memcmp(initial_code,reinterpret_cast<const void*>(entry_function),sizeof(initial_code))==0);
            std::cout<<"Detached import DLL fixture passed without FFR.\n";return 0;
        }
        if(mode.starts_with(L"abort")) {
            run_abort_fixture(connection,mode);stop(object);
            CHECK(host_count()==0 && host_errors()==0 && fixture_errors==0);
            CHECK(std::memcmp(initial_code,reinterpret_cast<const void*>(entry_function),sizeof(initial_code))==0);
            std::cout<<"Experimental abort DLL fixture passed without FFR.\n";return 0;
        }
        std::vector<std::int32_t> friends{1001, 1002}, enemies{11, 22}, positions{0, 1}, skills{77, 88};
        std::vector<ffr::RawArray> passives{array(skills)};
        std::vector<char16_t> bgm{u'F', u'i', u'x', u't', u'u', u'r', u'e', 0};
        ffr::RawEntry entry{};
        entry.friend_ids = array(friends); entry.friend_locations = array(positions);
        entry.enemy_ids = array(enemies); entry.enemy_locations = array(positions);
        entry.passives = array(passives); entry.bgm = array(bgm); entry.group = 123; entry.stage = 456;
        entry.can_escape = 1; entry.escape_probability = 0.75f;
        void* context = fake_map.data();
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
        // Later live changes must not alter the already detached opening basis.
        put(fake_root,0x38,2000.5f);
        put(fake_playtime,0x30,std::array<std::int64_t,3>{9500,0,40});
        put(fake_map_parameters[0],0x10c,std::int32_t{7});
        put(fake_rng_data,0x28,std::uint32_t{99});
        ffr_test_engine_rng_seed=17;
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
            // Unavailable/incoherent clock evidence must not suppress a valid
            // progression/encounter capture or invent a restorable timer.
            put(fake_playtime,0x30,std::int64_t{20000});
            put(fake_map_storage,0,std::uint64_t{1}); // Unsupported optional cache must not suppress progression.
            ffr_test_rand_import=1; // A changed game import invalidates optional seed coverage.
            engine_rng_drift=true;
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
            CHECK(abort_calls==0 && clone_loads==0 && serialized==2 && released==2);
            put(fake_map_storage,0,reinterpret_cast<std::uint64_t>(ffr_test_map_storage_vtable));
            ffr_test_rand_import=reinterpret_cast<std::uint64_t>(&ffr_test_crt_rand);
            engine_rng_drift=false;
            std::vector<std::int32_t> many_skills(128,2147483647);std::vector<ffr::RawArray> many_passives(64,array(many_skills));
            auto large_entry=entry;large_entry.passives=array(many_passives);
            for(unsigned i=0;i<3;++i)entry_function(context,&large_entry,false);
            lease(std::string(32,'f'),now()+8);
            wait_for([&]{return read(connection/L"runtime-report.json").find(std::string(32,'f'))!=std::string::npos;});
            const auto large_report=read(connection/L"runtime-report.json");
            CHECK(large_report.size()<=512*1024 && large_report.find("\"eventsOmittedForSize\":0")==std::string::npos);
            write(connection/L"large-history-report.json",large_report);
            // Corrupt only our never-called dummy draw code. A late profile
            // mismatch must suppress optional RNG metadata, not progression.
            auto* draw_code=reinterpret_cast<unsigned char*>(&ffr_test_engine_rng_draw);
            const auto saved_byte=*draw_code;DWORD protection{},unused{};
            CHECK(VirtualProtect(draw_code,1,PAGE_EXECUTE_READWRITE,&protection));*draw_code^=1;
            CHECK(VirtualProtect(draw_code,1,protection,&unused));FlushInstructionCache(GetCurrentProcess(),draw_code,1);
            entry_function(context,&entry,false);lease(std::string(32,'1'),now()+8);
            wait_for([&]{return read(connection/L"runtime-report.json").find(std::string(32,'1'))!=std::string::npos;});
            write(connection/L"bad-rng-code-report.json",read(connection/L"runtime-report.json"));
            CHECK(VirtualProtect(draw_code,1,PAGE_EXECUTE_READWRITE,&protection));*draw_code=saved_byte;
            CHECK(VirtualProtect(draw_code,1,protection,&unused));FlushInstructionCache(GetCurrentProcess(),draw_code,1);
        } else CHECK(snapshot.find("encounter-opening-native") == std::string::npos);
        stop(object);
        CHECK(host_count() == 0 && host_errors() == 0);
        CHECK(fixture_errors==0 && rand_calls==0 && rng_helper_calls==0 && engine_rng_draw_calls==0 && engine_rng_initialize_calls==0);
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
