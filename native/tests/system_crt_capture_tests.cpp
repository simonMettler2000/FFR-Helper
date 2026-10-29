// Exercises only this isolated test process. Never loads or controls FFR.
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <intrin.h>
#include "crt_profile.hpp"
#include "crt_rand_capture.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <tuple>

std::string digest(const unsigned char* data,std::size_t size) {
    if(size>std::numeric_limits<ULONG>::max())return {};
    unsigned char hash[32]{};
    if(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,const_cast<PUCHAR>(data),static_cast<ULONG>(size),hash,sizeof(hash))<0)return {};
    static constexpr char hex[]="0123456789abcdef";std::string out;
    for(auto byte:hash){out+=hex[byte>>4];out+=hex[byte&15];}return out;
}
int main() {
    auto module=GetModuleHandleW(ffr::crt_profile::module_name);if(!module)return 77;
    wchar_t path[32768]{};const auto length=GetModuleFileNameW(module,path,32768);
    if(!length || length>=32768)return 77;
    std::ifstream stream(std::filesystem::path(path),std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(stream)),{});
    if(digest(bytes.data(),bytes.size())!=ffr::crt_profile::module_sha256)return 77;
    const auto base=reinterpret_cast<std::uint64_t>(module);
    for(const auto& function:{std::tuple{ffr::crt_profile::rand_rva,ffr::crt_profile::rand_size,ffr::crt_profile::rand_sha256},
                             std::tuple{ffr::crt_profile::helper_rva,ffr::crt_profile::helper_size,ffr::crt_profile::helper_sha256}})
        if(digest(reinterpret_cast<unsigned char*>(base+std::get<0>(function)),std::get<1>(function))!=std::get<2>(function))return 77;
    auto read=[](std::uint64_t address,void* output,std::size_t count) {
        SIZE_T copied{};return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),output,count,&copied) && copied==count;
    };
    std::uint64_t getter{};if(!read(base+ffr::crt_profile::getter_rva,&getter,sizeof(getter)))return 77;
    if(getter!=reinterpret_cast<std::uint64_t>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"FlsGetValue"))
        && getter!=reinterpret_cast<std::uint64_t>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"RtlFlsGetValue2")))return 77;
    ffr::CrtRandLayout layout{base,getter,ffr::crt_profile::ready_rva,ffr::crt_profile::fast_rva,
        ffr::crt_profile::data_slot_rva,ffr::crt_profile::state_slot_rva,ffr::crt_profile::getter_rva};
    ffr::CrtRandAPI api;
    api.thread_id=[]()->std::uint64_t{return GetCurrentThreadId();};
    api.fast_index=[]()->std::uint64_t{return __readgsqword(0x1820);};
    api.is_fiber=[]{return IsThreadAFiber()!=FALSE;};
    api.fls_value=[](std::uint32_t slot)->std::optional<std::uint64_t> {
        const auto saved=GetLastError();SetLastError(ERROR_SUCCESS);auto* value=FlsGetValue(slot);
        const auto error=GetLastError();SetLastError(saved);
        if(!value && error!=ERROR_SUCCESS)return {};return reinterpret_cast<std::uint64_t>(value);
    };
    // Deliberately seeds/draws only our executable's CRT stream. The production
    // observer contains neither srand nor rand calls and never invokes its PTD getter.
    for(const auto initial:{0u,1u,12345u,0xffffffffu}) {
        std::srand(initial);auto expected=initial;
        for(int step=0;step<64;++step) {
            const auto first=ffr::capture_crt_rand(read,layout,api,GetCurrentThreadId());
            const auto second=ffr::capture_crt_rand(read,layout,api,GetCurrentThreadId());
            if(!first || !second || *first!=*second || first->seed!=expected)return 1;
            expected=expected*214013u+2531011u;
            if(static_cast<unsigned>(std::rand())!=((expected>>16)&0x7fffu))return 2;
            const auto after=ffr::capture_crt_rand(read,layout,api,GetCurrentThreadId());
            if(!after || after->seed!=expected)return 3;
        }
    }
    std::cout<<"256 CRT transitions verified in an isolated helper process; no game invoked.\n";
}
