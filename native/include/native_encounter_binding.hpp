#pragma once
// Internal Windows adapter for the verified consuming map entry. No IPC or
// progression/cache/clock restoration is exposed here. The practice DLL alone
// supplies a separate field/recovery/source transaction; the observer excludes it.
#include <windows.h>
#include <psapi.h>
#include "encounter_rebuild.hpp"
#include <atomic>
#include <algorithm>
#include <functional>
#include <span>

namespace ffr {
struct EncounterCodeRegion {std::uintptr_t rva{};std::size_t size{};std::string sha256;};
struct NativeEncounterProfile {
    std::string game_sha256;
    EncounterCodeRegion map_entry,allocate,release;
    std::vector<EncounterCodeRegion> dependencies;
    bool consuming_input_ownership_reviewed=false;
};
struct NativeEncounterResult {
    bool attempted{},input_transferred{},native_returned{},cleanup_verified=true;
    unsigned allocations{},released_before_transfer{},retained_on_cleanup_failure{};
    // Returning from map entry is not proof that the encounter reached a menu.
    bool new_encounter_verified=false,restores_invoked=false;
    std::string message="Encounter entry has not been requested";
};

class NativeEncounterBinding {
public:
    using Digest=std::function<std::string(std::span<const unsigned char>)>;
private:
    using Allocate=void*(__fastcall*)(std::size_t,std::uint32_t);
    using Release=void(__fastcall*)(void*);
    using Entry=void(__fastcall*)(void*,RawEntry*,bool);
    std::atomic<bool> initialized{false},prepared{false},bound{false},attempted{false};
    std::uintptr_t base{},image_size{},handler{},trampoline{};
    NativeEncounterProfile profile;
    Digest digest;
    std::vector<unsigned char> entry_before,entry_after,trampoline_after;
    static bool read(std::uintptr_t address,void* out,std::size_t size) {
        SIZE_T copied{};
        return address && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),out,size,&copied)
            && copied==size;
    }
    static bool executable(std::uintptr_t address,void* allocation=nullptr) {
        MEMORY_BASIC_INFORMATION info{};
        return VirtualQuery(reinterpret_cast<void*>(address),&info,sizeof(info)) && info.State==MEM_COMMIT
            && !(info.Protect&(PAGE_GUARD|PAGE_NOACCESS))
            && (info.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))
            && (!allocation || (info.Type==MEM_IMAGE && info.AllocationBase==allocation));
    }
    bool bytes(const EncounterCodeRegion& part,std::vector<unsigned char>& out) const {
        if(!part.size || part.size>16384 || part.rva>=image_size || part.size>image_size-part.rva
            || !executable(base+part.rva,reinterpret_cast<void*>(base)))return false;
        out.resize(part.size);return read(base+part.rva,out.data(),out.size());
    }
    bool matches(const EncounterCodeRegion& part) const {
        std::vector<unsigned char> out;
        return part.sha256.size()==64 && bytes(part,out) && digest(out)==part.sha256;
    }
    bool dependencies_match() const {
        if(!matches(profile.allocate)||!matches(profile.release))return false;
        for(const auto& dependency:profile.dependencies)if(!matches(dependency))return false;
        return true;
    }
    static bool entry_reaches(std::uintptr_t from,std::uintptr_t expected) {
        // The observer uses MinHook's x64 E9 -> FF25 relay. Do not follow
        // arbitrary code or accept an unrelated hook based on a saved patch.
        for(unsigned hops=0;hops<2;++hops) {
            if(from==expected)return true;
            if(!executable(from))return false;
            std::array<unsigned char,16> code{};if(!read(from,code.data(),code.size()))return false;
            std::int32_t relative{};
            if(code[0]==0xe9) {std::memcpy(&relative,code.data()+1,4);from=from+5+relative;}
            else if(code[0]==0xff && code[1]==0x25) {
                std::memcpy(&relative,code.data()+2,4);
                if(!read(from+6+relative,&from,sizeof(from)))return false;
            }else return false;
        }
        return from==expected;
    }
public:
    // Run before the observer changes map_entry. The file/code identity and
    // consuming copy/destructor chain must be reviewed together, never just a
    // fingerprinted downstream consumer that does not destroy its input.
    bool initialize(NativeEncounterProfile supplied,Digest hash,const std::function<std::string()>& file_hash) {
        if(initialized.exchange(true))return false;
        try {
            profile=std::move(supplied);digest=std::move(hash);
            MODULEINFO info{};auto module=GetModuleHandleW(nullptr);
            if(!digest || !file_hash || !profile.consuming_input_ownership_reviewed
                || profile.game_sha256.size()!=64 || file_hash()!=profile.game_sha256
                || !GetModuleInformation(GetCurrentProcess(),module,&info,sizeof(info)))return false;
            base=reinterpret_cast<std::uintptr_t>(info.lpBaseOfDll);image_size=info.SizeOfImage;
            if(profile.map_entry.size<64 || profile.dependencies.empty() || !matches(profile.map_entry)
                || !dependencies_match() || !bytes(profile.map_entry,entry_before))return false;
            prepared.store(true);return true;
        }catch(...) {return false;}
    }
    // The observer still owns/pins its hook and trampoline. Bind their exact
    // identities after installation, checking the relay target and unchanged
    // tail rather than treating any changed entry bytes as our own hook.
    bool bind_observer_hook(void* own_handler,void* own_trampoline) {
        if(!prepared.load() || bound.load() || attempted.load() || entry_before.empty())return false;
        try {
            handler=reinterpret_cast<std::uintptr_t>(own_handler);
            trampoline=reinterpret_cast<std::uintptr_t>(own_trampoline);
            if(!handler || !trampoline || !executable(handler) || !executable(trampoline)
                || !entry_reaches(base+profile.map_entry.rva,handler) || !dependencies_match()
                || !bytes(profile.map_entry,entry_after) || entry_after[0]!=0xe9
                || !std::equal(entry_after.begin()+5,entry_after.end(),entry_before.begin()+5))return false;
            trampoline_after.resize(64);
            if(!read(trampoline,trampoline_after.data(),trampoline_after.size()))return false;
            HMODULE pinned{};
            if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                    reinterpret_cast<LPCWSTR>(handler),&pinned))return false;
            bound.store(true);return true;
        }catch(...) {return false;}
    }
    bool ready() const {
        if(!bound.load())return false;
        try {
            std::vector<unsigned char> entry,original(64);
            return bytes(profile.map_entry,entry) && entry==entry_after && executable(trampoline)
                && read(trampoline,original.data(),original.size()) && original==trampoline_after
                && entry_reaches(base+profile.map_entry.rva,handler) && dependencies_match();
        }catch(...) {return false;}
    }
    NativeEncounterResult invoke(void* map_context,const Encounter& source,DWORD expected_thread,
                                 const std::function<bool()>& field_and_recovery_ready) {
        NativeEncounterResult result;
        if(attempted.exchange(true)) {result.message="Encounter entry was already attempted; no retry";return result;}
        try {
            // Own the source before any callback/allocation. Native capture may
            // replace the observer's mutable latest-opening pointer on reentry.
            const auto retained=source;
            if(!map_context || expected_thread!=GetCurrentThreadId() || !field_and_recovery_ready
                || !ready() || !field_and_recovery_ready() || retained.event!=-1 || !retained.can_escape
                || retained.skip_intro || retained.skip_win_result || retained.skip_lose_result
                || retained.preemption || retained.bounty || retained.battle_test
                || retained.friend_ids.empty() || retained.enemy_ids.empty()) {
                result.message="Encounter profile, thread, ordinary setup or field/recovery context differs";return result;
            }
            auto run=[&] {
                auto allocate=[&](std::size_t size)->void* {
                    if(!matches(profile.allocate))throw std::runtime_error("Native allocator changed");
                    auto* pointer=reinterpret_cast<Allocate>(base+profile.allocate.rva)(size,8);
                    if(pointer)++result.allocations;return pointer;
                };
                auto release=[&](void* pointer) noexcept {
                    try {
                        if(!matches(profile.release))throw std::runtime_error("Native releaser changed");
                        reinterpret_cast<Release>(base+profile.release.rva)(pointer);++result.released_before_transfer;
                    }catch(...) {result.cleanup_verified=false;++result.retained_on_cleanup_failure;}
                };
                EngineEncounter rebuilt(retained,allocate,release);
                if(expected_thread!=GetCurrentThreadId() || !ready() || !field_and_recovery_ready()) {
                    result.message="Context or code changed during reconstruction; no native entry";return;
                }
                auto descriptor=rebuilt.transfer();
                // Transfer and mark before entering game code. On an uncertain
                // native outcome only the engine may free the input graph.
                result.input_transferred=true;result.attempted=true;
                reinterpret_cast<Entry>(base+profile.map_entry.rva)(map_context,&descriptor,retained.skip_intro);
                result.native_returned=true;
                result.message="Consuming map entry returned once; new encounter/resources still need independent verification";
            };
            run();
        }catch(...) {
            // A thrown consuming call leaves engine ownership/cleanup unknown,
            // even if the helper itself has no remaining allocation to release.
            if(result.attempted)result.cleanup_verified=false;
            result.message=result.attempted?"Native encounter outcome uncertain; no retry or helper input free":
                "Encounter preparation failed; no native entry was invoked";
        }
        return result;
    }
};
} // namespace ffr
