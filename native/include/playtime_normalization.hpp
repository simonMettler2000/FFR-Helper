#pragma once
#include "playtime_capture.hpp"
#include <bit>
#include <limits>

namespace ffr {
// Explicit IEEE binary32 nearest-even conversion. Positive normal values only;
// no ambient FPU rounding mode is used to decide the final binary32 bits.
inline std::optional<float> playtime_nearest_float32(double value) {
    static_assert(sizeof(float)==4 && sizeof(double)==8 && std::numeric_limits<float>::is_iec559 && std::numeric_limits<double>::is_iec559);
    if(value==0)return 0.0f;
    if(!std::isfinite(value) || value<static_cast<double>(std::numeric_limits<float>::min()) || value>1e12)return {};
    const auto raw=std::bit_cast<std::uint64_t>(value);
    auto exponent=static_cast<int>((raw>>52)&0x7ff)-1023;
    const auto significand=(raw&0xfffffffffffffULL)|(1ULL<<52);
    auto rounded=significand>>29;
    const auto remainder=significand&((1ULL<<29)-1),half=1ULL<<28;
    if(remainder>half || (remainder==half && (rounded&1)))++rounded;
    if(rounded==(1ULL<<24)){rounded>>=1;++exponent;}
    if(exponent<-126 || exponent>127)return {};
    const auto result=std::bit_cast<float>(static_cast<std::uint32_t>((exponent+127)<<23)|static_cast<std::uint32_t>(rounded&0x7fffff));
    if(result>1e12)return {};
    return result;
}
struct PlaytimeNormalization {float rounded_elapsed{},saved_after_normal_load{};};
// Planning only: no UObject access or clock/reset/root writes. A future adapter
// must also check its CURRENT rounding mode and actual native load/reset path.
inline std::optional<PlaytimeNormalization> plan_playtime_normalization(const PlaytimeBasis& basis) {
    if(!valid_playtime_basis(basis) || basis.rounding_mode!=0 || basis.suspended_start_seconds!=0)return {};
    const auto saved_bits=std::bit_cast<std::uint32_t>(basis.saved_seconds);
    if((saved_bits&0x7f800000u)==0 && (saved_bits&0x7fffffu)!=0)return {}; // FTZ/DAZ coverage is not established.
    auto elapsed=playtime_nearest_float32(static_cast<double>(basis.elapsed_seconds));
    if(!elapsed)return {};
    // Both operands are binary32. Binary64 addition followed by explicit
    // nearest-even binary32 rounding also covers the native addss tie cases.
    auto total=playtime_nearest_float32(static_cast<double>(basis.saved_seconds)+static_cast<double>(*elapsed));
    if(!total)return {};
    return PlaytimeNormalization{*elapsed,*total};
}
} // namespace ffr
