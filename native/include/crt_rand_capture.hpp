#pragma once
#include "encounter_capture.hpp"
#include <limits>
namespace ffr {
struct CrtRandLayout {
    std::uint64_t base{},getter{},ready_rva{},fast_rva{},data_slot_rva{},state_slot_rva{},getter_rva{};
};
struct CrtRandAPI {
    std::function<std::uint64_t()> thread_id,fast_index;
    std::function<bool()> is_fiber;
    std::function<std::optional<std::uint64_t>(std::uint32_t)> fls_value;
};
struct CrtRandSample {
    std::uint64_t object{}; // Internal comparison identity only; never serialized.
    std::uint32_t seed{},state_index{};
    bool operator==(const CrtRandSample&)const=default;
};
inline std::optional<CrtRandSample> capture_crt_rand(const Reader& read,const CrtRandLayout& layout,const CrtRandAPI& api,std::uint64_t thread) {
    if(!thread || !layout.base || !layout.getter || api.thread_id()!=thread)return {};
    auto value=[&]<class T>(std::uint64_t address,T& out){return address && read(address,&out,sizeof(out));};
    struct Basis {std::uint8_t ready{},fast{};std::uint32_t data_slot{},state_slot{};std::uint64_t getter{};bool fiber{};};
    auto basis=[&]()->std::optional<Basis> {
        Basis out;
        if(!value(layout.base+layout.ready_rva,out.ready) || out.ready!=1 || !value(layout.base+layout.fast_rva,out.fast) || out.fast>1
            || !value(layout.base+layout.data_slot_rva,out.data_slot) || out.data_slot==0xffffffffu
            || !value(layout.base+layout.state_slot_rva,out.state_slot) || out.state_slot==0xffffffffu || out.state_slot==out.data_slot
            || !value(layout.base+layout.getter_rva,out.getter) || out.getter!=layout.getter)return {};
        out.fiber=api.is_fiber();return out;
    };
    auto sample=[&](const Basis& b)->std::optional<CrtRandSample> {
        auto array=api.fls_value(b.data_slot);if(!array || !*array)return {};
        auto index=b.fast && !b.fiber?std::optional<std::uint64_t>{api.fast_index()}:api.fls_value(b.state_slot);
        if(!index || *index>1)return {};
        const auto object=*array+*index*0x3c8;
        if(object<*array || object>std::numeric_limits<std::uint64_t>::max()-0x2c)return {};
        CrtRandSample out{object,0,static_cast<std::uint32_t>(*index)};
        if(!value(object+0x28,out.seed))return {};return out;
    };
    auto before=basis();if(!before)return {};auto first=sample(*before);if(!first)return {};
    auto after=basis();if(!after || before->ready!=after->ready || before->fast!=after->fast || before->data_slot!=after->data_slot
        || before->state_slot!=after->state_slot || before->getter!=after->getter || before->fiber!=after->fiber || api.thread_id()!=thread)return {};
    auto second=sample(*after);
    if(!second || *first!=*second || api.thread_id()!=thread)return {};
    return first;
}
inline std::string crt_rand_json(const CrtRandSample& value,const std::string& module_hash) {
    return "{\"kind\":\"partial-crt-rand-state\",\"readOnly\":true,\"restorable\":false,"
        "\"coverage\":\"current-game-thread-crt-rand-only\",\"otherSourcesCovered\":false,\"stableAcrossProgressionCopy\":true,"
        "\"moduleSha256\":"+quote(module_hash)+",\"stateIndex\":"+std::to_string(value.state_index)+",\"seed\":"+std::to_string(value.seed)+'}';
}
}
