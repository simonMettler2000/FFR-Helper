#pragma once
#include "encounter_capture.hpp"
#include <limits>
namespace ffr {
inline std::optional<std::uint32_t> capture_engine_global_seed(const Reader& read,std::uint64_t base,std::uint64_t offset) {
    if(!base || !offset || offset>std::numeric_limits<std::uint64_t>::max()-base)return {};
    const auto address=base+offset;
    if(address%alignof(std::uint32_t) || address>std::numeric_limits<std::uint64_t>::max()-sizeof(std::uint32_t))return {};
    std::uint32_t first{},second{};
    if(!read(address,&first,sizeof(first)) || !read(address,&second,sizeof(second)) || first!=second)return {};
    return first;
}
inline std::string engine_global_rng_json(std::uint32_t seed) {
    return "{\"kind\":\"partial-engine-global-rng-state\",\"readOnly\":true,\"restorable\":false,"
        "\"coverage\":\"single-profiled-global-seed-only\",\"otherSourcesCovered\":false,\"atomicSnapshot\":false,"
        "\"matchingSamplesAroundProgressionCopy\":true,\"seed\":"+std::to_string(seed)+'}';
}
}
