#pragma once
#include "encounter_capture.hpp"
#include <cstring>

namespace ffr {
struct PlaytimeBasis {
    float saved_seconds{};
    std::int64_t start_seconds{},suspended_start_seconds{},total_suspended_seconds{},sample_seconds{},elapsed_seconds{};
};
inline bool valid_playtime_basis(const PlaytimeBasis& value) {
    constexpr std::int64_t limit=1000000000000LL;
    return std::isfinite(value.saved_seconds) && value.saved_seconds>=0 && value.saved_seconds<=limit
        && value.sample_seconds>0 && value.sample_seconds<=limit
        && value.start_seconds>0 && value.start_seconds<=value.sample_seconds
        && value.total_suspended_seconds>=0 && value.total_suspended_seconds<=value.sample_seconds-value.start_seconds
        && (value.suspended_start_seconds==0 || (value.suspended_start_seconds>=value.start_seconds
            && value.suspended_start_seconds<=value.sample_seconds))
        && value.elapsed_seconds==value.sample_seconds-value.start_seconds-value.total_suspended_seconds;
}

// Read-only evidence, not a timer setter or complete restore state. The native
// elapsed getter subtracts COMPLETED suspension only. Keep the ongoing suspended
// start separately instead of calling this value active-play duration.
template<class GetRoot,class GetClock,class GetSeconds>
std::optional<PlaytimeBasis> capture_playtime_basis(const Reader& read,void* expected_root,
        GetRoot&& get_root,GetClock&& get_clock,GetSeconds&& get_seconds) {
    auto valid_object=[&](void* pointer) {
        std::array<std::uint64_t,4> header{};
        return pointer && read(reinterpret_cast<std::uint64_t>(pointer),header.data(),sizeof(header))
            && header[0] && header[2] && !(header[1]&0x40000000u);
    };
    if(!expected_root || get_root()!=expected_root || !valid_object(expected_root))return {};
    auto* clock=get_clock();if(!valid_object(clock))return {};
    const auto address=reinterpret_cast<std::uint64_t>(clock);
    const auto root=reinterpret_cast<std::uint64_t>(expected_root);
    PlaytimeBasis result;
    std::array<std::int64_t,3> before{},after{};
    float saved_after{};
    if(!read(address+0x30,before.data(),sizeof(before)) || !read(root+0x38,&result.saved_seconds,sizeof(float)))return {};
    result.sample_seconds=get_seconds();
    if(!read(address+0x30,after.data(),sizeof(after)) || before!=after
        || !read(root+0x38,&saved_after,sizeof(float))
        || std::memcmp(&saved_after,&result.saved_seconds,sizeof(float))
        || get_clock()!=clock || get_root()!=expected_root)return {};
    result.start_seconds=before[0];result.suspended_start_seconds=before[1];result.total_suspended_seconds=before[2];
    // Bound operands before subtraction; corrupt int64 data must not overflow.
    if(result.start_seconds<=0 || result.start_seconds>1000000000000LL
        || result.sample_seconds<result.start_seconds || result.sample_seconds>1000000000000LL
        || result.total_suspended_seconds<0 || result.total_suspended_seconds>result.sample_seconds-result.start_seconds)return {};
    result.elapsed_seconds=result.sample_seconds-result.start_seconds-result.total_suspended_seconds;
    if(!valid_playtime_basis(result))return {};
    return result;
}
inline std::string to_json(const PlaytimeBasis& value) {
    std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17);
    out<<"{\"kind\":\"partial-playtime-basis\",\"restorable\":false,\"source\":\"live-playtime-subsystem\","
        "\"coverage\":\"saved-base-and-clock-only\",\"sameRootChecked\":true,"
        "\"elapsedDefinition\":\"sample-minus-start-minus-completed-suspension\",\"savedSeconds\":"<<value.saved_seconds
        <<",\"startSeconds\":"<<value.start_seconds<<",\"suspendedStartSeconds\":"<<value.suspended_start_seconds
        <<",\"totalSuspendedSeconds\":"<<value.total_suspended_seconds<<",\"sampleSeconds\":"<<value.sample_seconds
        <<",\"elapsedSeconds\":"<<value.elapsed_seconds<<'}';return out.str();
}
} // namespace ffr
