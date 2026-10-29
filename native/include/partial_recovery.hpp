#pragma once
#include "progression_clone.hpp"
#include "field_anchor.hpp"
#include "playtime_capture.hpp"
#include <bit>

namespace ffr {
struct PartialRecovery {
    std::vector<unsigned char> progression;
    FieldAnchor field;
    PlaytimeBasis clock;
};
struct PartialRecoveryAPI {
    // The future native owner must prove the exact game thread, matching map,
    // cleared battle UI/manager/units/targets/jobs and no pending mutation.
    std::function<bool()> field_ready;
    std::function<void*()> current_root;
    std::function<std::optional<std::vector<unsigned char>>()> serialize_current;
    std::function<std::optional<FieldAnchor>()> field;
    std::function<std::optional<PlaytimeBasis>()> clock;
};
inline bool recovery_field_valid(const FieldAnchor& value) {
    if(value.map_id<0 || value.direction>8)return false;
    for(auto coordinate:value.position)if(!std::isfinite(coordinate) || std::abs(coordinate)>1e12)return false;
    return true;
}
inline bool same_recovery_field(const FieldAnchor& a,const FieldAnchor& b) {
    return a.map_id==b.map_id && a.direction==b.direction && a.position==b.position;
}
inline bool same_recovery_clock_basis(const PlaytimeBasis& a,const PlaytimeBasis& b) {
    return valid_playtime_basis(a) && valid_playtime_basis(b)
        && std::bit_cast<std::uint32_t>(a.saved_seconds)==std::bit_cast<std::uint32_t>(b.saved_seconds)
        && a.start_seconds==b.start_seconds && a.suspended_start_seconds==b.suspended_start_seconds
        && a.total_suspended_seconds==b.total_suspended_seconds && a.rounding_mode==b.rounding_mode
        && b.sample_seconds>=a.sample_seconds && b.sample_seconds-a.sample_seconds<=2;
}

// Read-only copying prerequisite, not connected to a game command. Two byte-
// identical serialized objects plus three stable field/clock samples detect
// drift across the synchronous operation. This is NOT an atomic world snapshot.
inline std::optional<PartialRecovery> capture_partial_recovery(const PartialRecoveryAPI& api,std::int32_t expected_map_id) {
    if(expected_map_id<0 || !api.field_ready())return {};
    auto* root=api.current_root();if(!root)return {};
    auto first_field=api.field();auto first_clock=api.clock();
    if(!first_field || !first_clock || !recovery_field_valid(*first_field) || first_field->map_id!=expected_map_id
        || !valid_playtime_basis(*first_clock))return {};
    auto first=api.serialize_current();
    if(!first || !captured_progression_header(*first) || api.current_root()!=root || !api.field_ready())return {};
    auto middle_clock=api.clock();auto middle_field=api.field();
    if(!middle_clock || !middle_field || !same_recovery_clock_basis(*first_clock,*middle_clock)
        || !recovery_field_valid(*middle_field) || !same_recovery_field(*first_field,*middle_field))return {};
    auto second=api.serialize_current();
    if(!second || *second!=*first || api.current_root()!=root || !api.field_ready())return {};
    auto final_clock=api.clock();auto final_field=api.field();
    if(!final_clock || !final_field || !same_recovery_clock_basis(*first_clock,*final_clock)
        || !same_recovery_clock_basis(*middle_clock,*final_clock) || !recovery_field_valid(*final_field)
        || !same_recovery_field(*first_field,*final_field) || api.current_root()!=root || !api.field_ready())return {};
    return PartialRecovery{std::move(*first),*final_field,*final_clock};
}
} // namespace ffr
