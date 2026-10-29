// Production binding in an isolated executable; no game or personal data used.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "native_load_binding.hpp"
#include "load_profile.hpp" // Compile the supported profile without invoking it.
#include "progression_rebind.hpp"
#include "clone_payload.hpp"
#include <bcrypt.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <thread>
#define CHECK(x) do {if(!(x))throw std::runtime_error("Load binding check failed at "+std::to_string(__LINE__));}while(false)
namespace {
std::uintptr_t manager_vtable[]{1},save_vtable[]{2};
struct SaveRoot {void* vtable=save_vtable;std::uint32_t flags{},index{};std::uintptr_t klass=1,padding{};};
struct Manager {
    void* vtable=manager_vtable;std::uint32_t flags{},index{};std::uintptr_t klass=1;
    unsigned char padding[0x60-24]{};std::atomic<void*> root{nullptr};
};
static_assert(offsetof(Manager,root)==0x60);
Manager manager,other_manager;SaveRoot disk_root,captured_root;
std::atomic<unsigned> disk_checks{},disk_reads{},updates{},ends{};
std::atomic<std::uintptr_t> validity_return{},reader_return{};
DWORD validity_error{},reader_error{};
std::function<void()> on_private_start;
__declspec(noinline) int dependency_marker(){return 42;}
__declspec(noinline) bool __fastcall native_valid(void* source,std::int32_t slot) {
    validity_error=GetLastError();validity_return=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    ++disk_checks;return source && slot==7;
}
__declspec(noinline) std::uint8_t __fastcall native_reader(void* source,std::int32_t slot,void** out) {
    reader_error=GetLastError();reader_return=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    ++disk_reads;if(!out)return 6;
    *out=source && slot==7?&disk_root:nullptr;return *out?0:7;
}
__declspec(noinline) bool __fastcall native_load(void* input,std::int32_t slot) {
    auto* source=static_cast<Manager*>(input);
    if(!native_valid(source,slot))return false;
    if(slot==ffr::memory_load_private_slot && on_private_start)on_private_start();
    void* target=nullptr;(void)native_reader(source,slot,&target);
    if(!target)return false;
    source->root.store(target);++updates;++ends;return true;
}
std::string sha256(std::span<const unsigned char> bytes) {
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};
    struct Cleanup {BCRYPT_ALG_HANDLE& a;BCRYPT_HASH_HANDLE& h;~Cleanup(){if(h)BCryptDestroyHash(h);if(a)BCryptCloseAlgorithmProvider(a,0);}} cleanup{algorithm,hash};
    CHECK(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0);
    DWORD size{},received{};CHECK(BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof(size),&received,0)>=0);
    std::vector<unsigned char> storage(size);CHECK(BCryptCreateHash(algorithm,&hash,storage.data(),size,nullptr,0,0)>=0);
    CHECK(bytes.size()<=MAXDWORD && BCryptHashData(hash,const_cast<PUCHAR>(bytes.data()),static_cast<ULONG>(bytes.size()),0)>=0);
    std::array<unsigned char,32> result{};CHECK(BCryptFinishHash(hash,result.data(),static_cast<ULONG>(result.size()),0)>=0);
    std::string text;constexpr char hex[]="0123456789abcdef";for(auto b:result){text+=hex[b>>4];text+=hex[b&15];}return text;
}
std::string file_digest() {
    wchar_t path[32768]{};CHECK(GetModuleFileNameW(nullptr,path,32768));
    std::ifstream stream(std::filesystem::path(path),std::ios::binary);
    const std::vector<unsigned char> bytes(std::istreambuf_iterator<char>(stream),{});CHECK(!bytes.empty());return sha256(bytes);
}
ffr::NativeLoadProfile fixture_profile(std::uintptr_t valid_caller,std::uintptr_t read_caller) {
    const auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    auto function=[&](auto* pointer,std::size_t length) {
        const auto address=reinterpret_cast<std::uintptr_t>(pointer);
        return ffr::LoadFunctionProfile{address-base,length,sha256({reinterpret_cast<const unsigned char*>(address),length})};
    };
    DWORD64 image{};auto* info=RtlLookupFunctionEntry(reinterpret_cast<DWORD64>(&native_load),&image,nullptr);
    CHECK(info && image==base && base+info->BeginAddress==reinterpret_cast<std::uintptr_t>(&native_load));
    return {file_digest(),function(&native_load,info->EndAddress-info->BeginAddress),function(&native_valid,64),function(&native_reader,64),
        valid_caller-base,read_caller-base,reinterpret_cast<std::uintptr_t>(manager_vtable)-base,reinterpret_cast<std::uintptr_t>(save_vtable)-base,
        {function(&dependency_marker,16)}};
}
}
int main(int argc,char** argv) {
    try {
        const std::string mode=argc==2?argv[1]:"good";
        CHECK(mode=="good" || mode=="build-mismatch" || mode=="profile-mismatch" || mode=="callsite-mismatch"
            || mode=="dependency-mismatch" || mode=="dependency-changed"
            || mode=="wrong-thread" || mode=="wrong-context" || mode=="wrong-class" || mode=="post-context");
        CHECK(native_load(&manager,7));auto profile=fixture_profile(validity_return,reader_return);
        std::array<unsigned char,64> initial_valid{},initial_reader{};
        std::memcpy(initial_valid.data(),reinterpret_cast<void*>(&native_valid),64);std::memcpy(initial_reader.data(),reinterpret_cast<void*>(&native_reader),64);
        CHECK(MH_Initialize()==MH_OK);auto* binding=new ffr::NativeLoadBinding;
        if(mode=="build-mismatch" || mode=="profile-mismatch" || mode=="callsite-mismatch" || mode=="dependency-mismatch") {
            if(mode=="build-mismatch")profile.game_sha256=std::string(64,'0');
            else if(mode=="profile-mismatch")profile.reader.sha256[0]=profile.reader.sha256[0]=='0'?'1':'0';
            else if(mode=="dependency-mismatch")profile.unhooked_dependencies[0].sha256=std::string(64,'0');
            else ++profile.reader_return_rva;
            CHECK(!binding->initialize(profile,file_digest,sha256));
            CHECK(std::memcmp(initial_valid.data(),reinterpret_cast<void*>(&native_valid),64)==0);
            CHECK(std::memcmp(initial_reader.data(),reinterpret_cast<void*>(&native_reader),64)==0);
            CHECK(native_load(&manager,7) && !native_load(&manager,ffr::memory_load_private_slot));
            std::cout<<"Profile/callsite mismatch refused without installing a detour.\n";return 0;
        }
        bool dependency_changed=false;
        auto digest=[&](std::span<const unsigned char> bytes){return dependency_changed && bytes.size()==16?std::string(64,'0'):sha256(bytes);};
        CHECK(binding->initialize(profile,file_digest,digest));CHECK(!binding->initialize(profile,file_digest,digest));
        dependency_changed=mode=="dependency-changed"; // Inject a failed fresh dependency check without altering executable bytes.
        const DWORD game_thread=GetCurrentThreadId();
        if(mode!="good" && mode!="post-context") {
            const auto count=updates.load();ffr::NativeLoadBindingResult result;
            if(mode=="wrong-class")captured_root.vtable=manager_vtable;
            auto attempt=[&]{result=binding->apply(&manager,&disk_root,&captured_root,game_thread,[&]{return mode!="wrong-context";});};
            if(mode=="wrong-thread"){std::thread worker(attempt);worker.join();}else attempt();
            CHECK(!result.native_called && !result.passed && updates==count && manager.root==&disk_root);
            CHECK(!binding->apply(&manager,&disk_root,&captured_root,game_thread,[]{return true;}).native_called);
            binding->disable();std::cout<<"Thread/context/class refusal preceded native load; no retry.\n";return 0;
        }
        SetLastError(4321);CHECK(native_load(&manager,7));CHECK(validity_error==4321 && reader_error==4321);
        unsigned checks_after{},reads_after{},updates_after{},ends_after{};
        bool field_ready=true;
        on_private_start=[&] {
            CHECK(!native_valid(&manager,ffr::memory_load_private_slot));
            void* out=nullptr;CHECK(native_reader(&manager,ffr::memory_load_private_slot,&out)==7 && out==nullptr);
            CHECK(!native_load(&other_manager,ffr::memory_load_private_slot));CHECK(native_load(&other_manager,7));
            std::atomic<bool> worker_ok{false};std::thread worker([&]{worker_ok=native_load(&manager,7) && !native_load(&manager,ffr::memory_load_private_slot);});
            worker.join();CHECK(worker_ok);
            checks_after=disk_checks;reads_after=disk_reads;updates_after=updates;ends_after=ends;
            if(mode=="post-context")field_ready=false;
        };
        std::set<void*> pins;auto recovery=clone_fixture_payload(),target=recovery;target[8]^=1;
        ffr::NativeLoadBindingResult native_result;ffr::ProgressionRebindAPI api;
        api.objects.ready=[&] {return field_ready;};api.objects.current_root=[]{return manager.root.load();};
        api.objects.same_save_class=[](void* a,void* b){return (a==&disk_root || a==&captured_root) && b==&disk_root;};
        api.objects.is_rooted=[&](void* p){return pins.count(p)!=0;};api.objects.add_root=[&](void* p){CHECK(pins.insert(p).second);};
        api.objects.remove_root=[&](void* p){CHECK(pins.erase(p)==1);};
        api.objects.load_memory=[&](const ffr::RawArray* input)->void* {
            CHECK(pins.count(&disk_root) && input->count==static_cast<int>(target.size()));
            CHECK(std::memcmp(reinterpret_cast<void*>(input->data),target.data(),target.size())==0);return &captured_root;
        };
        api.objects.save_memory=[&](void* p)->std::optional<std::vector<unsigned char>> {CHECK(pins.count(p));return p==&disk_root?recovery:target;};
        api.native_load=[&](void* p) {
            CHECK(pins.count(&disk_root) && pins.count(&captured_root));
            native_result=binding->apply(&manager,&disk_root,p,game_thread,[&]{return field_ready;});return native_result.passed;
        };
        ffr::ProgressionRebindController controller;const auto result=controller.run(target,recovery,true,api);
        if(mode=="post-context") {
            CHECK(!result.passed && result.native_attempted && !result.verified && result.retained_owned_pins==2 && pins.size()==2);
            CHECK(native_result.native_called && native_result.native_returned && native_result.route_complete && native_result.root_matches
                && !native_result.context_matches && !native_result.passed);
        }else {
            CHECK(result.passed && result.native_attempted && result.bytes_match && !result.pins_released
                && result.retention_expected && result.retention_verified && result.retained_owned_pins==2 && pins.size()==2);
            CHECK(native_result.passed && native_result.native_called && native_result.route_complete && native_result.root_matches);
        }
        CHECK(manager.root==&captured_root && disk_checks==checks_after && disk_reads==reads_after);
        CHECK(updates==updates_after+1 && ends==ends_after+1 && !native_load(&manager,ffr::memory_load_private_slot));
        CHECK(!controller.run(target,recovery,true,api).native_attempted);
        CHECK(!binding->apply(&manager,&captured_root,&disk_root,game_thread,[]{return true;}).native_called);
        binding->disable();
        CHECK(std::memcmp(initial_valid.data(),reinterpret_cast<void*>(&native_valid),64)==0);
        CHECK(std::memcmp(initial_reader.data(),reinterpret_cast<void*>(&native_reader),64)==0);
        std::cout<<"Production adapter verified real detours, stack-only output, caller/thread isolation, native assignment and pinned rebind.\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
