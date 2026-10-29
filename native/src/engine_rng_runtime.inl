// No function pointers to the generator or initializer are constructed. Both
// routines mutate RNG state and must never be called merely to inspect it.
std::atomic<bool> engine_rng_profile_ready{false};
bool engine_rng_code_matches(std::uint64_t base) {
    for(const auto& function:{std::tuple{ffr::engine_rng_profile::draw_rva,ffr::engine_rng_profile::draw_size,ffr::engine_rng_profile::draw_sha256},
                             std::tuple{ffr::engine_rng_profile::initialize_rva,ffr::engine_rng_profile::initialize_size,ffr::engine_rng_profile::initialize_sha256}}) {
        std::vector<unsigned char> code(std::get<1>(function));
        if(!read_memory(base+std::get<0>(function),code.data(),code.size()))return false;
        Sha256 hash;hash.add(code.data(),code.size());if(hash.finish()!=std::get<2>(function))return false;
    }
    return true;
}
void initialize_engine_rng_profile() {
    engine_rng_profile_ready=engine_rng_code_matches(reinterpret_cast<std::uint64_t>(GetModuleHandleW(nullptr)));
}
std::optional<std::uint32_t> capture_live_engine_rng() {
    if(!engine_rng_profile_ready)return {};
    const auto error=GetLastError();struct Restore{DWORD value;~Restore(){SetLastError(value);}} restore{error};
    const auto base=reinterpret_cast<std::uint64_t>(GetModuleHandleW(nullptr));
    if(!engine_rng_code_matches(base))return {};
    auto seed=ffr::capture_engine_global_seed(read_memory,base,ffr::engine_rng_profile::seed_rva);
    if(!seed || !engine_rng_code_matches(base))return {};
    return seed;
}
