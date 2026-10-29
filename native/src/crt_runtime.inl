// Reads existing FLS values only. Never invokes rand/srand, the CRT's allocating
// PTD getter, a slot allocator/setter or any RNG setter.
std::atomic<bool> crt_profile_ready{false};
std::uint64_t crt_base{};
void initialize_crt_profile() {
    auto module=GetModuleHandleW(ffr::crt_profile::module_name[0]?ffr::crt_profile::module_name:nullptr);
    if(!module || file_hash(module_path(module))!=ffr::crt_profile::module_sha256)return;
    const auto base=reinterpret_cast<std::uint64_t>(module),game=reinterpret_cast<std::uint64_t>(GetModuleHandleW(nullptr));
    std::uint64_t target{};if(!read_memory(game+ffr::crt_profile::import_rva,&target,sizeof(target)) || target!=base+ffr::crt_profile::rand_rva)return;
    for(const auto& function:{std::tuple{ffr::crt_profile::rand_rva,ffr::crt_profile::rand_size,ffr::crt_profile::rand_sha256},
                             std::tuple{ffr::crt_profile::helper_rva,ffr::crt_profile::helper_size,ffr::crt_profile::helper_sha256}}) {
        std::vector<unsigned char> code(std::get<1>(function));
        if(!read_memory(base+std::get<0>(function),code.data(),code.size()))return;
        Sha256 hash;hash.add(code.data(),code.size());if(hash.finish()!=std::get<2>(function))return;
    }
    crt_base=base;crt_profile_ready=true;
}
std::optional<ffr::CrtRandSample> capture_live_crt_rand(DWORD thread) {
    if(!crt_profile_ready)return {};
    const auto error=GetLastError();struct Restore{DWORD value;~Restore(){SetLastError(value);}} restore{error};
    const auto game=reinterpret_cast<std::uint64_t>(GetModuleHandleW(nullptr));
    std::uint64_t target{};
    if(!read_memory(game+ffr::crt_profile::import_rva,&target,sizeof(target)) || target!=crt_base+ffr::crt_profile::rand_rva)return {};
    std::uint64_t getter{};if(!read_memory(crt_base+ffr::crt_profile::getter_rva,&getter,sizeof(getter)))return {};
    const auto kernel=GetModuleHandleW(L"kernel32.dll"),ntdll=GetModuleHandleW(L"ntdll.dll");
    const auto fls=reinterpret_cast<std::uint64_t>(GetProcAddress(kernel,"FlsGetValue"));
    const auto fast=reinterpret_cast<std::uint64_t>(GetProcAddress(ntdll,"RtlFlsGetValue2"));
    if(!getter || (getter!=fls && getter!=fast))return {}; // Unknown/TLS selection is unsupported.
    ffr::CrtRandLayout layout{crt_base,getter,ffr::crt_profile::ready_rva,ffr::crt_profile::fast_rva,
        ffr::crt_profile::data_slot_rva,ffr::crt_profile::state_slot_rva,ffr::crt_profile::getter_rva};
    ffr::CrtRandAPI api;
    api.thread_id=[]()->std::uint64_t{return GetCurrentThreadId();};
    api.fast_index=[]()->std::uint64_t{return __readgsqword(0x1820);};
    api.is_fiber=[]{return IsThreadAFiber()!=FALSE;};
    api.fls_value=[](std::uint32_t slot)->std::optional<std::uint64_t> {
        SetLastError(ERROR_SUCCESS);auto* value=FlsGetValue(slot);
        if(!value && GetLastError()!=ERROR_SUCCESS)return {};return reinterpret_cast<std::uint64_t>(value);
    };
    return ffr::capture_crt_rand(read_memory,layout,api,thread);
}
