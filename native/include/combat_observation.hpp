#pragma once
// Detached partial combat evidence. No setters, game calls or restoration.
#include "encounter_capture.hpp"
#include <map>
#include <set>

namespace ffr {
struct RawSkillStack {
    std::int32_t skill{},padding{};
    RawArray targets{};
};
static_assert(sizeof(RawSkillStack)==24 && offsetof(RawSkillStack,targets)==8);
struct ObservedSkillStack {std::int32_t skill{};std::vector<std::int32_t> targets;};
struct ObservedAI {
    std::int32_t skill{},scheduled_skill{},status_changed_skill{};
    std::vector<std::int32_t> targets;
    std::vector<ObservedSkillStack> stack;
};
struct ObservedCombatUnit {
    // These are raw stored components. MP/stagger getters clamp their display,
    // and the level getter applies a multiplier. Do not normalize them on capture.
    std::map<std::string,std::int32_t> raw;
    std::vector<std::int32_t> accessories,passives;
    std::optional<ObservedAI> ai;
    std::int32_t continuous_effect_count{};
};
struct CombatObservation {
    std::vector<ObservedCombatUnit> units;
    std::vector<std::int32_t> target_units,additional_timeline;
    std::int32_t selected_unit=-1,battle_turn{};
};

inline std::optional<CombatObservation> capture_combat_observation(const Reader& read,std::uint64_t manager) {
    auto object=[&](std::uint64_t pointer) {
        std::array<std::uint64_t,4> header{};
        return pointer && read(pointer,header.data(),sizeof(header)) && header[0] && header[2] && !(header[1]&0x40000000u);
    };
    auto value=[&]<class T>(std::uint64_t address,T& out){return address && read(address,&out,sizeof(out));};
    if(!object(manager))return {};
    std::uint8_t active{};
    std::uint64_t units{},jobs{};
    if(!value(manager+0x1a9,active)||active!=1||!value(manager+0x48,units)||!object(units)
        ||!value(manager+0x40,jobs)||!object(jobs))return {};
    RawArray waiting{},working{},actors{};
    if(!value(jobs+0x28,waiting)||!value(jobs+0x38,working)||!valid_array(waiting,1024)||!valid_array(working,1024)
        ||waiting.count!=0||working.count!=0||!value(units+0x28,actors)||!valid_array(actors,64)||actors.count==0)return {};
    std::vector<std::uint64_t> actor_list;
    if(!copy_array(read,actors,64,actor_list))return {};
    std::map<std::uint64_t,std::int32_t> indices;
    for(std::size_t i=0;i<actor_list.size();++i) {
        if(!object(actor_list[i])||!indices.emplace(actor_list[i],static_cast<std::int32_t>(i)).second)return {};
    }
    CombatObservation out;
    std::size_t reference_budget=4096,stack_budget=256;
    auto targets=[&](const RawArray& raw,std::int32_t maximum,std::vector<std::int32_t>& detached) {
        if(!valid_array(raw,maximum)||static_cast<std::size_t>(raw.count)>reference_budget)return false;
        reference_budget-=raw.count;
        std::vector<std::uint64_t> pointers;if(!copy_array(read,raw,maximum,pointers))return false;
        for(auto pointer:pointers) {
            if(pointer==0) {detached.push_back(-1);continue;}
            const auto found=indices.find(pointer);if(found==indices.end())return false;
            detached.push_back(found->second);
        }return true;
    };
    std::uint64_t selected{},scheduler{},scheduler_owner{};
    RawArray target_list{},additional{};
    if(!value(units+0x38,selected)||!indices.count(selected)||!value(units+0x48,target_list)||!targets(target_list,64,out.target_units)
        ||!value(units+0x40,scheduler)||!object(scheduler)||!value(scheduler+0x28,scheduler_owner)||scheduler_owner!=units
        ||!value(scheduler+0x30,additional)||!targets(additional,256,out.additional_timeline)
        ||!value(manager+0x200,out.battle_turn)||out.battle_turn<0)return {};
    out.selected_unit=indices.at(selected);
    std::set<std::uint64_t> parameters;
    for(auto actor:actor_list) {
        std::uint64_t parameter{},owner{},ai{};
        if(!value(actor+0x838,parameter)||!object(parameter)||!parameters.insert(parameter).second
            ||!value(parameter+0x360,owner)||owner!=actor||!value(actor+0x870,ai))return {};
        ObservedCombatUnit unit;
        // UNIT_PARAMETER begins at +0xc8. These scalar offsets are corroborated
        // by its reflected struct and native getter thunks. No FText, maps or
        // UObject references are shallow-copied from that structure.
        const std::pair<const char*,std::uint32_t> fields[]{
            {"unitId",0xd0},{"baseLevel",0x100},{"hpBase",0x10c},{"mp",0x110},{"stagger",0x114},{"limitBurst",0x118},
            {"maxHp",0x120},{"maxMp",0x124},{"maxStagger",0x128},{"attack",0x12c},{"defence",0x130},
            {"intelligence",0x134},{"mind",0x138},{"divine",0x13c},{"agility",0x140},
            {"weaponId",0x1e8},{"armorId",0x1ec},{"visionId",0x200},{"temporaryHp",0x268},
            {"actionCount",0x4dc},{"thinkCount",0x4fc},{"receivedDamageCount",0x500},{"breakCount",0x504},{"duplicationId",0x550}};
        for(const auto& [name,offset]:fields)if(!value(parameter+offset,unit.raw[name]))return {};
        if(unit.raw.at("unitId")<0)return {};
        RawArray accessories{},passives{},effects{};
        if(!value(parameter+0x1f0,accessories)||!copy_array(read,accessories,16,unit.accessories)
            ||!value(parameter+0x208,passives)||!copy_array(read,passives,128,unit.passives)
            ||!value(parameter+0xb8,effects)||!valid_array(effects,1024))return {};
        unit.continuous_effect_count=effects.count;
        if(ai) {
            std::uint64_t ai_owner{};RawArray ai_targets{},stack{};
            if(!object(ai)||!value(ai+0x28,ai_owner)||ai_owner!=actor)return {};
            ObservedAI copied;
            if(!value(ai+0x40,copied.skill)||!value(ai+0x44,copied.scheduled_skill)||!value(ai+0x48,copied.status_changed_skill)
                ||!value(ai+0x30,ai_targets)||!targets(ai_targets,64,copied.targets)||!value(ai+0x50,stack)
                ||!valid_array(stack,256)||static_cast<std::size_t>(stack.count)>stack_budget)return {};
            stack_budget-=stack.count;
            std::vector<RawSkillStack> pending;if(!copy_array(read,stack,256,pending))return {};
            for(const auto& item:pending) {
                ObservedSkillStack saved;saved.skill=item.skill;
                if(!targets(item.targets,64,saved.targets))return {};
                copied.stack.push_back(std::move(saved));
            }
            unit.ai=std::move(copied);
        }
        out.units.push_back(std::move(unit));
    }
    return out;
}

inline std::string to_json(const CombatObservation& observation) {
    std::string out="{\"kind\":\"partial-combat-observation\",\"restorable\":false,\"readOnly\":true,"
        "\"referenceEncoding\":\"capture-local-unit-index; -1 is null\",\"battleTurn\":"+std::to_string(observation.battle_turn)
        +",\"selectedUnit\":"+std::to_string(observation.selected_unit)+",\"targetUnits\":"+integers(observation.target_units)
        +",\"additionalTimeline\":"+integers(observation.additional_timeline)+",\"units\":[";
    for(std::size_t index=0;index<observation.units.size();++index) {
        if(index)out+=',';const auto& unit=observation.units[index];
        out+="{\"index\":"+std::to_string(index)+",\"raw\":{";bool first=true;
        for(const auto& [name,value]:unit.raw) {if(!first)out+=',';first=false;out+=quote(name)+':'+std::to_string(value);}
        out+="},\"accessories\":"+integers(unit.accessories)+",\"passives\":"+integers(unit.passives)
            +",\"continuousEffectCount\":"+std::to_string(unit.continuous_effect_count)+",\"ai\":";
        if(!unit.ai)out+="null";
        else {
            const auto& ai=*unit.ai;
            out+="{\"skillId\":"+std::to_string(ai.skill)+",\"scheduledSkillId\":"+std::to_string(ai.scheduled_skill)
                +",\"statusChangedSkillId\":"+std::to_string(ai.status_changed_skill)+",\"targets\":"+integers(ai.targets)+",\"stack\":[";
            for(std::size_t i=0;i<ai.stack.size();++i) {
                if(i)out+=',';out+="{\"skillId\":"+std::to_string(ai.stack[i].skill)+",\"targets\":"+integers(ai.stack[i].targets)+'}';
            }out+="]}";
        }out+='}';
    }
    return out+"],\"missingState\":[\"Complete scheduler state and native callbacks\",\"Continuous effects and status durations\","
        "\"Resistance maps and calculated modifiers\",\"Shared AI state, phases and other native counters\","
        "\"Native RNG state and draw order\",\"Actor lifecycle and world restoration\",\"Verified restoration path\"]}";
}
} // namespace ffr
