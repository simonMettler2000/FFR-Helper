// Actual Windows adapter and MinHook relay, exercised only in this dummy EXE.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "native_encounter_binding.hpp"
#include "build_profile.hpp"
#include "encounter_entry_profile.hpp" // Compile, but never invoke, the FFR profile.
#include <MinHook.h>
#include <bcrypt.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <thread>
#define CHECK(x) do {if(!(x))throw std::runtime_error("Encounter binding check failed at "+std::to_string(__LINE__));}while(false)
namespace {
using Entry=void(__fastcall*)(void*,ffr::RawEntry*,bool);
Entry original{};
int map_marker{};
unsigned allocations{},frees{},entries{},captures{},gates{};
std::map<void*,std::size_t> blocks;
std::string mode,captured;
ffr::Encounter* mutable_source{};
__declspec(noinline) void* __fastcall engine_allocate(std::size_t size,std::uint32_t alignment) {
    CHECK(alignment==8 && size>0);++allocations;
    if(mode=="allocation-failed" && allocations==3)return nullptr;
    if(mutable_source)mutable_source->friend_ids[0]=999;
    auto* pointer=HeapAlloc(GetProcessHeap(),0,size);if(pointer)blocks[pointer]=size;return pointer;
}
__declspec(noinline) void __fastcall engine_release(void* pointer) {
    CHECK(blocks.erase(pointer)==1);++frees;CHECK(HeapFree(GetProcessHeap(),0,pointer));
}
__declspec(noinline) void dependency_marker() {SetLastError(2468);}
void consume(ffr::RawEntry* raw,bool skip) {
    ffr::Reader reader=[](auto address,auto output,auto size){std::memcpy(output,reinterpret_cast<void*>(address),size);return true;};
    const auto entry=ffr::capture_entry(reader,reinterpret_cast<std::uint64_t>(raw),skip);
    CHECK(entry);captured=ffr::to_json(*entry);
    const auto nested=reinterpret_cast<ffr::RawArray*>(raw->passives.data);
    for(int i=0;i<raw->passives.count;++i)if(nested[i].data)engine_release(reinterpret_cast<void*>(nested[i].data));
    for(auto array:{raw->bgm,raw->passives,raw->enemy_ids,raw->enemy_locations,raw->friend_ids,raw->friend_locations})
        if(array.data)engine_release(reinterpret_cast<void*>(array.data));
}
__declspec(noinline) void __fastcall native_entry(void* context,ffr::RawEntry* raw,bool skip) {
    CHECK(context==&map_marker && !skip && raw->friend_ids.count==2 && raw->enemy_ids.count==1);
    ++entries;SetLastError(1234);
    if(mode=="native-partial")throw std::runtime_error("Uncertain engine ownership");
    consume(raw,skip);dependency_marker();
    if(mode=="native-throw")throw std::runtime_error("Uncertain native return");
    SetLastError(5678);
}
__declspec(noinline) void __fastcall observer_hook(void* context,ffr::RawEntry* raw,bool skip) {
    ++captures;original(context,raw,skip);
}
__declspec(noinline) void other_hook() {SetLastError(9999);}
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
ffr::EncounterCodeRegion function(void* pointer,std::size_t size=0) {
    const auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)),address=reinterpret_cast<std::uintptr_t>(pointer);
    if(!size) {DWORD64 module{};auto* row=RtlLookupFunctionEntry(address,&module,nullptr);CHECK(row && module==base && row->BeginAddress+base==address);size=row->EndAddress-row->BeginAddress;}
    return {address-base,size,sha256({reinterpret_cast<const unsigned char*>(pointer),size})};
}
void change_byte(void* at) {
    DWORD protection{},ignored{};CHECK(VirtualProtect(at,1,PAGE_EXECUTE_READWRITE,&protection));
    *static_cast<unsigned char*>(at)^=1;CHECK(VirtualProtect(at,1,protection,&ignored));FlushInstructionCache(GetCurrentProcess(),at,1);
}
ffr::Encounter sample() {
    ffr::Encounter source;source.friend_ids={1001,1002};source.friend_locations={0,1};source.enemy_ids={11};source.enemy_locations={2};
    source.passives={{77,88},{99},{}};source.bgm="Fixture";source.event=-1;source.can_escape=true;source.escape_probability=50;return source;
}
}
int main(int argc,char** argv) {
    try {
        CHECK(argc==2);mode=argv[1];auto source=sample();const auto expected=ffr::to_json(source);
        ffr::NativeEncounterProfile profile{file_digest(),function(&native_entry),function(&engine_allocate),function(&engine_release),{function(&dependency_marker,16)},true};
        CHECK(profile.map_entry.size>=64);
        if(mode=="build-mismatch")profile.game_sha256=std::string(64,'0');
        if(mode=="ownership-unreviewed")profile.consuming_input_ownership_reviewed=false;
        if(mode=="profile-mismatch")profile.map_entry.sha256=std::string(64,'0');
        ffr::NativeEncounterBinding binding;
        const bool initialized=binding.initialize(profile,sha256,file_digest);
        if(mode=="build-mismatch" || mode=="ownership-unreviewed" || mode=="profile-mismatch") {
            CHECK(!initialized && !binding.ready() && !binding.bind_observer_hook(&observer_hook,&native_entry));
            CHECK(!binding.invoke(&map_marker,source,GetCurrentThreadId(),[]{return true;}).attempted && allocations==0 && entries==0);return 0;
        }
        CHECK(initialized && !binding.ready() && !binding.bind_observer_hook(&observer_hook,&native_entry));
        CHECK(MH_Initialize()==MH_OK);void* trampoline{};
        CHECK(MH_CreateHook(&native_entry,&observer_hook,&trampoline)==MH_OK);original=reinterpret_cast<Entry>(trampoline);
        CHECK(MH_EnableHook(&native_entry)==MH_OK);
        if(mode=="wrong-hook") {
            CHECK(!binding.bind_observer_hook(&other_hook,trampoline) && !binding.ready());
        }else CHECK(binding.bind_observer_hook(&observer_hook,trampoline) && binding.ready());
        void* changed=nullptr;
        if(mode=="entry-drift")changed=reinterpret_cast<unsigned char*>(&native_entry)+24;
        if(mode=="trampoline-drift")changed=reinterpret_cast<unsigned char*>(trampoline)+60;
        if(mode=="dependency-drift")changed=reinterpret_cast<void*>(&dependency_marker);
        if(changed)change_byte(changed);
        auto gate=[&] {++gates;return mode!="wrong-context" && !(mode=="context-drift" && gates==2);};
        if(mode=="source-mutated")mutable_source=&source;
        auto thread=GetCurrentThreadId();if(mode=="wrong-thread")++thread;
        const auto result=binding.invoke(&map_marker,source,thread,gate);
        const bool attempted=mode=="good" || mode=="source-mutated" || mode=="native-throw" || mode=="native-partial" || mode.starts_with("rearm-");
        CHECK(result.attempted==attempted && result.input_transferred==attempted && entries==(attempted?1u:0u) && captures==entries);
        CHECK(!result.new_encounter_verified && !result.restores_invoked);
        if(mode=="native-partial") {CHECK(!result.native_returned && frees==0 && !blocks.empty());}
        else {CHECK(blocks.empty());CHECK(result.cleanup_verified==(mode!="native-throw"));}
        if(mode=="native-partial")CHECK(!result.cleanup_verified);
        if(attempted && mode!="native-partial")CHECK(captured==expected && result.released_before_transfer==0 && allocations==frees);
        if(mode=="good" || mode=="source-mutated")CHECK(result.native_returned);
        if(mode=="native-throw")CHECK(!result.native_returned);
        if(mode=="allocation-failed" || mode=="context-drift")CHECK(!result.attempted && frees>0 && result.released_before_transfer==frees);
        CHECK(!binding.invoke(&map_marker,source,thread,gate).attempted && entries==(attempted?1u:0u));
        CHECK(binding.generation()==1);
        if(mode.starts_with("rearm-")) {
            unsigned checks=0;bool nested_blocked=false;
            const auto rearm_thread=mode=="rearm-wrong-thread"?GetCurrentThreadId()+1:GetCurrentThreadId();
            const auto generation=mode=="rearm-stale"?0u:1u;
            auto verify=[&] {
                ++checks;
                if(mode=="rearm-throw")throw std::runtime_error("Fixture child check failed");
                if(mode=="rearm-reentrant") {
                    nested_blocked=!binding.invoke(&map_marker,source,thread,gate).attempted
                        && !binding.rearm_verified_child(1,thread,[]{return true;});
                }
                if(mode=="rearm-drift" && checks==2)change_byte(reinterpret_cast<void*>(&dependency_marker));
                return mode!="rearm-refused";
            };
            const bool good=mode=="rearm-good" || mode=="rearm-reentrant";
            CHECK(binding.rearm_verified_child(generation,rearm_thread,verify)==good);
            CHECK(binding.generation()==(good?2u:1u));
            if(mode=="rearm-reentrant")CHECK(nested_blocked);
            if(mode=="rearm-drift")change_byte(reinterpret_cast<void*>(&dependency_marker));
            if(good) {
                CHECK(checks==2 && binding.invoke(&map_marker,source,thread,gate).native_returned && entries==2 && blocks.empty());
                CHECK(!binding.rearm_verified_child(1,thread,[]{return true;}) && binding.generation()==2);
                CHECK(binding.rearm_verified_child(2,thread,[]{return true;}) && binding.generation()==3);
                CHECK(binding.invoke(&map_marker,source,thread,gate).native_returned && entries==3 && captures==3 && blocks.empty());
                CHECK(!binding.invoke(&map_marker,source,thread,gate).attempted && entries==3);
            }else CHECK(!binding.invoke(&map_marker,source,thread,gate).attempted && entries==1);
        }else if(!result.native_returned || !result.cleanup_verified) {
            CHECK(!binding.rearm_verified_child(1,GetCurrentThreadId(),[]{return true;}));
            CHECK(binding.generation()==1);
        }
        if(changed)change_byte(changed);
        CHECK(MH_DisableHook(&native_entry)==MH_OK);CHECK(!binding.ready());CHECK(MH_RemoveHook(&native_entry)==MH_OK);CHECK(MH_Uninitialize()==MH_OK);
        // Simulated engine owner alone disposes of uncertain consumed input.
        for(auto [pointer,size]:blocks)CHECK(HeapFree(GetProcessHeap(),0,pointer));blocks.clear();
        std::cout<<"Verified consuming-entry adapter/observer relay: "<<mode<<". No game or restoration used.\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
