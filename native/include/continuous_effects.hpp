#pragma once
#include "encounter_capture.hpp"
#include <map>

namespace ffr {
struct ObservedContinuousEffect {
    std::int32_t source_unit=-1,target_unit=-1;
    std::map<std::string,std::int32_t> raw;
    std::vector<std::int32_t> parameters;
};
struct ContinuousEffectObservation {
    std::vector<ObservedContinuousEffect> effects;
    std::vector<std::vector<std::int32_t>> unit_effects;
    std::size_t references_used{};
};

// Base components only. Shared effect identity and ordered per-unit lists are
// preserved through local pool indices. No UObject pointer or shallow engine
// container is exported. Unknown source/target parameters make this whole
// optional graph unavailable rather than silently dropping an effect.
inline std::optional<ContinuousEffectObservation> capture_continuous_effects(const Reader& read,
    const std::vector<std::uint64_t>& actors,const std::vector<std::uint64_t>& parameters,std::size_t budget) {
    if(actors.empty() || actors.size()!=parameters.size() || actors.size()>64)return {};
    auto value=[&]<class T>(std::uint64_t address,T& out){return address && read(address,&out,sizeof(out));};
    auto object=[&](std::uint64_t address) {
        std::array<std::uint64_t,4> header{};
        return address && read(address,header.data(),sizeof(header)) && header[0] && header[2] && !(header[1]&0x40000000u);
    };
    std::map<std::uint64_t,std::int32_t> parameter_indices,effect_indices;
    for(std::size_t i=0;i<parameters.size();++i) {
        std::uint64_t current{},owner{};
        if(!parameters[i] || !parameter_indices.emplace(parameters[i],static_cast<std::int32_t>(i)).second
            || !value(actors[i]+0x838,current) || current!=parameters[i] || !value(parameters[i]+0x360,owner) || owner!=actors[i])return {};
    }
    const auto initial_budget=budget;std::size_t parameter_budget=4096;
    auto link=[&](std::uint64_t pointer,std::int32_t& out) {
        if(!budget)return false;--budget;if(!pointer){out=-1;return true;}
        const auto found=parameter_indices.find(pointer);if(found==parameter_indices.end())return false;out=found->second;return true;
    };
    ContinuousEffectObservation out;
    for(auto parameter:parameters) {
        RawArray list{};std::vector<std::uint64_t> pointers;std::vector<std::int32_t> indices;
        if(!value(parameter+0xb8,list) || !valid_array(list,1024) || static_cast<std::size_t>(list.count)>budget
            || !copy_array(read,list,1024,pointers))return {};
        budget-=pointers.size();
        for(auto pointer:pointers) {
            if(!pointer)return {}; // Null list entries cannot become invented effects.
            auto found=effect_indices.find(pointer);
            if(found!=effect_indices.end()){indices.push_back(found->second);continue;}
            if(out.effects.size()>=256 || !object(pointer))return {};
            std::uint64_t source{},target{};ObservedContinuousEffect effect;
            if(!value(pointer+0x28,source) || !value(pointer+0x30,target) || !link(source,effect.source_unit) || !link(target,effect.target_unit))return {};
            // CPP_ContEffect::GetEffectData copies SKILL_EFFECT_DATA from +0x48.
            // GetLifeTurn/GetStatusCondition corroborate +0x98/+0x78 directly.
            const std::pair<const char*,std::uint32_t> integers[]{
                {"effectId",0x50},{"endActionCount",0x58},{"addProbability",0x60},{"activationProbability",0x64},
                {"conditionParam1",0x6c},{"conditionParam2",0x70},{"conditionParam3",0x74},{"lifeTurn",0x98}};
            for(const auto& [name,offset]:integers)if(!value(pointer+offset,effect.raw[name]))return {};
            const std::pair<const char*,std::uint32_t> bytes[]{
                {"effectType",0x54},{"applyTiming",0x55},{"proceedTiming",0x56},{"unremovable",0x5c},
                {"conditionType",0x68},{"statusCondition",0x78},{"executingReaction",0x90}};
            for(const auto& [name,offset]:bytes) {
                std::uint8_t copied{};if(!value(pointer+offset,copied))return {};effect.raw[name]=copied;
            }
            if(effect.raw.at("unremovable")>1 || effect.raw.at("executingReaction")>1)return {};
            RawArray effect_parameters{};
            if(!value(pointer+0x80,effect_parameters) || !valid_array(effect_parameters,128)
                || static_cast<std::size_t>(effect_parameters.count)>parameter_budget
                || !copy_array(read,effect_parameters,128,effect.parameters))return {};
            parameter_budget-=effect.parameters.size();
            const auto index=static_cast<std::int32_t>(out.effects.size());effect_indices.emplace(pointer,index);
            out.effects.push_back(std::move(effect));indices.push_back(index);
        }
        out.unit_effects.push_back(std::move(indices));
    }
    out.references_used=initial_budget-budget;return out;
}
inline std::string continuous_effects_json(const std::optional<ContinuousEffectObservation>& observation) {
    if(!observation)return "null";
    std::string out="{\"coverage\":\"base-continuous-effect-components-only\",\"unitEffects\":[";
    for(std::size_t i=0;i<observation->unit_effects.size();++i){if(i)out+=',';out+=integers(observation->unit_effects[i]);}
    out+="],\"effects\":[";
    for(std::size_t i=0;i<observation->effects.size();++i) {
        if(i)out+=',';const auto& effect=observation->effects[i];
        out+="{\"index\":"+std::to_string(i)+",\"sourceUnit\":"+std::to_string(effect.source_unit)+",\"targetUnit\":"+std::to_string(effect.target_unit)+",\"raw\":{";
        bool first=true;for(const auto& [name,value]:effect.raw){if(!first)out+=',';first=false;out+=quote(name)+':'+std::to_string(value);}
        out+="},\"parameters\":"+integers(effect.parameters)+'}';
    }
    return out+"]}";
}
} // namespace ffr
