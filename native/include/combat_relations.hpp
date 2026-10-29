#pragma once
#include "encounter_capture.hpp"
#include <map>

namespace ffr {
struct ObservedUnitRelations {
    std::int32_t vision=-1,vision_owner=-1,last_damage_dealer=-1;
    std::vector<std::int32_t> damage_sources,active_skill_targets;
};

// Optional capture-local graph. Actor links and parameter links use DIFFERENT
// pointer domains but produce the same local unit indices. An unrepresented
// reserve/vision/source makes this entire optional graph unavailable, never null
// by invention. The caller may still retain its other partial diagnostics.
inline std::optional<std::vector<ObservedUnitRelations>> capture_combat_relations(
    const Reader& read,const std::vector<std::uint64_t>& actors,const std::vector<std::uint64_t>& parameters,std::size_t budget) {
    if(actors.empty() || actors.size()!=parameters.size() || actors.size()>64)return {};
    std::map<std::uint64_t,std::int32_t> actor_indices,parameter_indices;
    for(std::size_t i=0;i<actors.size();++i) {
        if(!actors[i] || !parameters[i] || !actor_indices.emplace(actors[i],static_cast<std::int32_t>(i)).second
            || !parameter_indices.emplace(parameters[i],static_cast<std::int32_t>(i)).second)return {};
    }
    auto value=[&]<class T>(std::uint64_t address,T& out){return address && read(address,&out,sizeof(out));};
    auto link=[&](std::uint64_t pointer,const auto& indices,std::int32_t& result) {
        if(!budget)return false;--budget;
        if(!pointer){result=-1;return true;}
        const auto found=indices.find(pointer);if(found==indices.end())return false;result=found->second;return true;
    };
    auto list=[&](std::uint64_t address,const auto& indices,std::vector<std::int32_t>& result) {
        RawArray raw{};std::vector<std::uint64_t> copied;
        if(!value(address,raw) || !valid_array(raw,64) || static_cast<std::size_t>(raw.count)>budget
            || !copy_array(read,raw,64,copied))return false;
        for(auto pointer:copied){std::int32_t index{};if(!link(pointer,indices,index))return false;result.push_back(index);}return true;
    };
    std::vector<ObservedUnitRelations> result;
    for(std::size_t i=0;i<actors.size();++i) {
        const auto parameter=parameters[i];std::uint64_t current{},owner{},vision{},vision_owner{},dealer{};
        // Class reflection: CPP_BtlUnitParameter +0x368/+0x370/+0x380/+0x388;
        // inherited CPP_UnitParameter::m_ActiveSkillTargets +0x270.
        if(!value(actors[i]+0x838,current) || current!=parameter || !value(parameter+0x360,owner) || owner!=actors[i]
            || !value(parameter+0x368,vision) || !value(parameter+0x370,vision_owner) || !value(parameter+0x380,dealer))return {};
        ObservedUnitRelations copied;
        if(!link(vision,actor_indices,copied.vision) || !link(vision_owner,actor_indices,copied.vision_owner)
            || !link(dealer,actor_indices,copied.last_damage_dealer)
            || !list(parameter+0x388,actor_indices,copied.damage_sources)
            || !list(parameter+0x270,parameter_indices,copied.active_skill_targets))return {};
        result.push_back(std::move(copied));
    }
    return result;
}
inline std::string relations_json(const std::optional<std::vector<ObservedUnitRelations>>& relations) {
    if(!relations)return "null";
    std::string out="{\"coverage\":\"capture-local-links-only\",\"units\":[";
    for(std::size_t i=0;i<relations->size();++i) {
        if(i)out+=',';const auto& unit=relations->at(i);
        out+="{\"index\":"+std::to_string(i)+",\"vision\":"+std::to_string(unit.vision)
            +",\"visionOwner\":"+std::to_string(unit.vision_owner)+",\"lastDamageDealer\":"+std::to_string(unit.last_damage_dealer)
            +",\"damageSources\":"+integers(unit.damage_sources)+",\"activeSkillTargets\":"+integers(unit.active_skill_targets)+'}';
    }
    return out+"]}";
}
} // namespace ffr
