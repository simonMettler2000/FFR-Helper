#pragma once
// Windows binding prerequisite. Compiled by the isolated load-route test only;
// no observer/practice command includes or invokes this adapter yet.
#include <windows.h>
#include <psapi.h>
#include <intrin.h>
#include <MinHook.h>
#include "load_substitution.hpp"
#include <array>
#include <atomic>
#include <cstring>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace ffr {
struct LoadFunctionProfile {std::uintptr_t rva{};std::size_t size{};std::string sha256;};
struct NativeLoadProfile {
    std::string game_sha256;
    LoadFunctionProfile load,validity,reader;
    std::uintptr_t validity_return_rva{},reader_return_rva{},manager_vtable_rva{},save_vtable_rva{};
    std::vector<LoadFunctionProfile> unhooked_dependencies;
};
struct NativeLoadBindingResult {
    bool native_called{},native_returned{},route_complete{},root_matches{},context_matches{},passed{};
    std::string message="Native load was not requested";
};

// Own this object and its code until process exit, with its module pinned. The
// caller owns MinHook initialization and all game-thread/field/recovery/GC gates.
// No command, path, payload, slot, address or profile is accepted from IPC.
class NativeLoadBinding {
public:
    using Digest=std::function<std::string(std::span<const unsigned char>)>;
    using FileDigest=std::function<std::string()>;
private:
    using Load=bool(__fastcall*)(void*,std::int32_t);
    using Valid=bool(__fastcall*)(void*,std::int32_t);
    using Reader=std::uint8_t(__fastcall*)(void*,std::int32_t,void**);
    inline static std::atomic<NativeLoadBinding*> owner{nullptr};
    ScopedLoadSubstitution routing;
    std::atomic<bool> ready{false},attempted{false};
    std::atomic<bool> initialized{false};
    bool valid_created=false,reader_created=false;
    std::uintptr_t base{},image_size{};
    NativeLoadProfile profile;
    Digest memory_digest;
    Load load{};
    Valid original_valid{};
    Reader original_reader{};
    void* valid_address{};
    void* reader_address{};

    static bool copy(std::uintptr_t from,void* to,std::size_t size) {
        SIZE_T read{};return from && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(from),to,size,&read) && read==size;
    }
    bool in_image(std::uintptr_t rva,std::size_t size) const {
        return rva<image_size && size && size<=image_size-rva;
    }
    bool function_matches(const LoadFunctionProfile& function,const Digest& digest) const {
        if(function.size<1 || function.size>4096 || !in_image(function.rva,function.size))return false;
        MEMORY_BASIC_INFORMATION info{};
        if(!VirtualQuery(reinterpret_cast<void*>(base+function.rva),&info,sizeof(info)) || info.Type!=MEM_IMAGE
            || reinterpret_cast<std::uintptr_t>(info.AllocationBase)!=base
            || !(info.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return false;
        std::vector<unsigned char> bytes(function.size);
        return copy(base+function.rva,bytes.data(),bytes.size()) && digest(bytes)==function.sha256;
    }
    bool direct_call_matches(std::uintptr_t returned,std::uintptr_t target) const {
        if(returned<profile.load.rva+5 || returned-profile.load.rva>profile.load.size)return false;
        std::array<unsigned char,5> instruction{};
        if(!copy(base+returned-5,instruction.data(),instruction.size()) || instruction[0]!=0xe8)return false;
        std::int32_t relative{};std::memcpy(&relative,instruction.data()+1,4);
        return static_cast<std::int64_t>(returned)+relative==static_cast<std::int64_t>(target);
    }
    bool object_matches(void* object,std::uintptr_t vtable) const {
        std::array<std::uint64_t,4> header{};
        return object && copy(reinterpret_cast<std::uintptr_t>(object),header.data(),sizeof(header))
            && header[0]==base+vtable && header[2] && !(header[1]&0x40000000u);
    }
    static bool stack_destination(void** out,void*& current) {
        ULONG_PTR low{},high{};GetCurrentThreadStackLimits(&low,&high);
        const auto address=reinterpret_cast<std::uintptr_t>(out);
        if(address<low || address>high || sizeof(void*)>high-address || address%alignof(void*))return false;
        MEMORY_BASIC_INFORMATION info{};
        if(!VirtualQuery(out,&info,sizeof(info)) || info.State!=MEM_COMMIT || (info.Protect&(PAGE_GUARD|PAGE_NOACCESS))
            || !(info.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return false;
        return copy(address,&current,sizeof(current));
    }
    static bool __fastcall validity_hook(void* manager,std::int32_t slot) {
        auto* self=owner.load(std::memory_order_acquire);if(!self)return false;
        const auto error=GetLastError();
        const LoadCall call{reinterpret_cast<std::uintptr_t>(manager),reinterpret_cast<std::uintptr_t>(_ReturnAddress()),GetCurrentThreadId(),slot};
        const auto result=self->routing.validity(call);SetLastError(error);
        return result?*result:self->original_valid(manager,slot);
    }
    static std::uint8_t __fastcall reader_hook(void* manager,std::int32_t slot,void** out) {
        auto* self=owner.load(std::memory_order_acquire);if(!self)return 6;
        const auto error=GetLastError();void* current{};
        const bool writable_stack=stack_destination(out,current);
        const LoadCall call{reinterpret_cast<std::uintptr_t>(manager),reinterpret_cast<std::uintptr_t>(_ReturnAddress()),GetCurrentThreadId(),slot};
        const auto result=self->routing.reader(call,writable_stack && current==nullptr);
        SetLastError(error);
        if(!result)return self->original_reader(manager,slot,out);
        // Only this exact native caller's current-thread stack out parameter can
        // be written. Invalid destinations never become arbitrary memory writes.
        if(writable_stack) {auto* replacement=reinterpret_cast<void*>(result->root);std::memcpy(out,&replacement,sizeof(replacement));}
        SetLastError(error);return result->status;
    }
public:
    NativeLoadBinding()=default;
    NativeLoadBinding(const NativeLoadBinding&)=delete;
    NativeLoadBinding& operator=(const NativeLoadBinding&)=delete;
    ~NativeLoadBinding()=delete; // Bound callbacks/trampolines retain this owner until exit.

    bool initialize(const NativeLoadProfile& requested,const FileDigest& file_digest,const Digest& digest) {
        if(initialized.exchange(true))return false;
        const auto saved_error=GetLastError();
        struct Restore {DWORD error;~Restore(){SetLastError(error);}} restore{saved_error};
        try {
            if(file_digest()!=requested.game_sha256 || requested.game_sha256.size()!=64)return false;
            auto module=GetModuleHandleW(nullptr);MODULEINFO module_info{};
            if(!K32GetModuleInformation(GetCurrentProcess(),module,&module_info,sizeof(module_info)))return false;
            base=reinterpret_cast<std::uintptr_t>(module);image_size=module_info.SizeOfImage;profile=requested;memory_digest=digest;
            if(profile.load.size<5 || profile.validity.size<5 || profile.reader.size<5 || profile.unhooked_dependencies.size()>16
                || !function_matches(profile.load,digest) || !function_matches(profile.validity,digest) || !function_matches(profile.reader,digest)
                || !direct_call_matches(profile.validity_return_rva,profile.validity.rva)
                || !direct_call_matches(profile.reader_return_rva,profile.reader.rva)
                || profile.validity_return_rva==profile.reader_return_rva
                || !in_image(profile.manager_vtable_rva,8) || !in_image(profile.save_vtable_rva,8))return false;
            for(const auto& dependency:profile.unhooked_dependencies)if(!function_matches(dependency,digest))return false;
            HMODULE pinned{};
            if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                    reinterpret_cast<LPCWSTR>(&validity_hook),&pinned))return false;
            load=reinterpret_cast<Load>(base+profile.load.rva);
            valid_address=reinterpret_cast<void*>(base+profile.validity.rva);reader_address=reinterpret_cast<void*>(base+profile.reader.rva);
            void* valid_trampoline{},*reader_trampoline{};
            if(MH_CreateHook(valid_address,reinterpret_cast<void*>(&validity_hook),&valid_trampoline)!=MH_OK)return false;
            valid_created=true;
            if(MH_CreateHook(reader_address,reinterpret_cast<void*>(&reader_hook),&reader_trampoline)!=MH_OK) {
                MH_RemoveHook(valid_address);valid_created=false;return false; // Never enabled, no in-flight trampoline.
            }
            reader_created=true;
            original_valid=reinterpret_cast<Valid>(valid_trampoline);original_reader=reinterpret_cast<Reader>(reader_trampoline);
            NativeLoadBinding* expected=nullptr;
            if(!owner.compare_exchange_strong(expected,this,std::memory_order_release,std::memory_order_relaxed)) {
                MH_RemoveHook(valid_address);MH_RemoveHook(reader_address);valid_created=false;reader_created=false;return false;
            }
            // Publish both originals before either detour can execute.
            if(MH_EnableHook(valid_address)!=MH_OK || MH_EnableHook(reader_address)!=MH_OK) {disable();return false;}
            ready.store(true,std::memory_order_release);return true;
        }catch(...) {disable();return false;}
    }
    NativeLoadBindingResult apply(void* manager,void* expected_original,void* target,DWORD expected_game_thread,
                                  const std::function<bool()>& context_ready) {
        NativeLoadBindingResult result;
        if(attempted.exchange(true)) {result.message="Native load binding already attempted; no retry";return result;}
        const auto saved_error=GetLastError();
        struct Restore {DWORD error;~Restore(){SetLastError(error);}} restore{saved_error};
        struct Close {ScopedLoadSubstitution& routing;~Close(){routing.close();}} close{routing};
        try {
            void* current{};
            if(!ready.load(std::memory_order_acquire) || !expected_game_thread || GetCurrentThreadId()!=expected_game_thread
                || !context_ready() || expected_original==target
                || !object_matches(manager,profile.manager_vtable_rva) || !object_matches(expected_original,profile.save_vtable_rva)
                || !object_matches(target,profile.save_vtable_rva)
                || !copy(reinterpret_cast<std::uintptr_t>(manager)+0x60,&current,sizeof(current)) || current!=expected_original) {
                result.message="Native load profile, thread, context, objects or original root did not match";return result;
            }
            // The normal load passes its private slot to a shipping no-op. Its
            // loaded body must still match before application; otherwise an
            // unexpected slot consumer could interpret that sentinel as an index.
            for(const auto& dependency:profile.unhooked_dependencies)if(!function_matches(dependency,memory_digest)) {
                result.message="A required unhooked native-load dependency changed";return result;
            }
            const LoadSubstitutionConfig config{reinterpret_cast<std::uintptr_t>(manager),reinterpret_cast<std::uintptr_t>(target),
                base+profile.validity_return_rva,base+profile.reader_return_rva,expected_game_thread};
            if(!routing.arm(config)) {result.message="Native load routing scope could not be armed";return result;}
            result.native_called=true;result.native_returned=load(manager,memory_load_private_slot);
            result.route_complete=routing.close();
            result.context_matches=context_ready();
            result.root_matches=copy(reinterpret_cast<std::uintptr_t>(manager)+0x60,&current,sizeof(current)) && current==target;
            result.passed=result.native_returned && result.route_complete && result.context_matches && result.root_matches;
            result.message=result.passed?"Native load route completed; caches, clock and world restoration are not established":
                "Native load was attempted with an unverified outcome; no retry or rollback";
        }catch(...) {result.message=result.native_called?"Native load raised an error; application may be partial. No retry":"Native load preflight failed before invocation";}
        return result;
    }
    void disable() noexcept {
        ready.store(false,std::memory_order_release);
        if(valid_created)MH_DisableHook(valid_address);
        if(reader_created)MH_DisableHook(reader_address);
        // Keep the pinned module, owner and disabled trampolines until exit.
    }
};
} // namespace ffr
