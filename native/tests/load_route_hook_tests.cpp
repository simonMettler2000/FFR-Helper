// Isolated native detour test. No game process, loader, save or game data used.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <intrin.h>
#include <MinHook.h>
#include "load_substitution.hpp"
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#define CHECK(x) do {if(!(x))throw std::runtime_error("Load detour check failed at "+std::to_string(__LINE__));}while(false)

namespace {
struct Manager {std::atomic<void*> root{nullptr};};
Manager manager,other_manager;
int disk_root{},captured_root{};
std::atomic<unsigned> disk_checks{},disk_reads{},starts{},updates{},ends{};
std::atomic<std::uintptr_t> validity_return{},reader_return{};
ffr::ScopedLoadSubstitution routing;
using Valid=bool(__fastcall*)(Manager*,std::int32_t);
using Reader=std::uint8_t(__fastcall*)(Manager*,std::int32_t,void**);
Valid original_valid{};
Reader original_reader{};

__declspec(noinline) bool __fastcall native_valid(Manager* source,std::int32_t slot) {
    validity_return.store(reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
    ++disk_checks;return source && slot==7;
}
__declspec(noinline) std::uint8_t __fastcall native_reader(Manager* source,std::int32_t slot,void** out) {
    reader_return.store(reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
    ++disk_reads;if(!out)return 6;
    *out=source && slot==7?&disk_root:nullptr;return *out?0:7;
}
__declspec(noinline) bool __fastcall native_load(Manager* source,std::int32_t slot) {
    if(!native_valid(source,slot))return false;
    ++starts;void* target=nullptr;(void)native_reader(source,slot,&target);
    if(!target)return false;
    // This assignment belongs to the simulated native load, never the hooks.
    source->root.store(target);++updates;++ends;return true;
}
__declspec(noinline) bool __fastcall validity_hook(Manager* source,std::int32_t slot) {
    const ffr::LoadCall call{reinterpret_cast<std::uintptr_t>(source),reinterpret_cast<std::uintptr_t>(_ReturnAddress()),GetCurrentThreadId(),slot};
    const auto result=routing.validity(call);return result?*result:original_valid(source,slot);
}
__declspec(noinline) std::uint8_t __fastcall reader_hook(Manager* source,std::int32_t slot,void** out) {
    const ffr::LoadCall call{reinterpret_cast<std::uintptr_t>(source),reinterpret_cast<std::uintptr_t>(_ReturnAddress()),GetCurrentThreadId(),slot};
    const auto result=routing.reader(call,out && *out==nullptr);
    if(!result)return original_reader(source,slot,out);
    if(out)*out=reinterpret_cast<void*>(result->root);return result->status;
}
}
int main() {
    try {
        CHECK(native_load(&manager,7));
        const auto valid_caller=validity_return.load(),read_caller=reader_return.load();
        CHECK(valid_caller && read_caller && valid_caller!=read_caller && manager.root==&disk_root);
        CHECK(MH_Initialize()==MH_OK);
        void* valid_trampoline{},*read_trampoline{};
        CHECK(MH_CreateHook(reinterpret_cast<void*>(&native_valid),reinterpret_cast<void*>(&validity_hook),&valid_trampoline)==MH_OK);
        CHECK(MH_CreateHook(reinterpret_cast<void*>(&native_reader),reinterpret_cast<void*>(&reader_hook),&read_trampoline)==MH_OK);
        original_valid=reinterpret_cast<Valid>(valid_trampoline);original_reader=reinterpret_cast<Reader>(read_trampoline);
        CHECK(MH_EnableHook(MH_ALL_HOOKS)==MH_OK);
        CHECK(native_load(&manager,7));CHECK(!native_load(&manager,8));
        const ffr::LoadSubstitutionConfig config{reinterpret_cast<std::uintptr_t>(&manager),reinterpret_cast<std::uintptr_t>(&captured_root),valid_caller,read_caller,GetCurrentThreadId()};
        CHECK(routing.arm(config));
        // Same manager/slot from a different caller retains the original path.
        CHECK(!native_valid(&manager,ffr::memory_load_private_slot));
        void* unrelated=nullptr;CHECK(native_reader(&manager,ffr::memory_load_private_slot,&unrelated)==7 && unrelated==nullptr);
        // Same callsites from a different manager or thread also pass through.
        CHECK(native_load(&other_manager,7));
        std::atomic<bool> worker_ok{false};std::thread worker([&]{worker_ok=native_load(&manager,7);});worker.join();CHECK(worker_ok);
        CHECK(routing.view().validations==0 && routing.view().readers==0);
        const auto checks=disk_checks.load(),reads=disk_reads.load(),before_starts=starts.load(),before_updates=updates.load(),before_ends=ends.load();
        CHECK(native_load(&manager,ffr::memory_load_private_slot));
        CHECK(manager.root==&captured_root && disk_checks==checks && disk_reads==reads);
        CHECK(starts==before_starts+1 && updates==before_updates+1 && ends==before_ends+1);
        CHECK(routing.view().validations==1 && routing.view().readers==1 && routing.close());
        CHECK(!native_load(&manager,ffr::memory_load_private_slot));
        CHECK(manager.root==&captured_root && updates==before_updates+1 && !routing.arm(config));
        CHECK(MH_DisableHook(MH_ALL_HOOKS)==MH_OK);CHECK(MH_Uninitialize()==MH_OK);
        std::cout<<"Real detours preserved native assignment/notifications, caller/thread isolation and one-shot in-memory substitution.\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
