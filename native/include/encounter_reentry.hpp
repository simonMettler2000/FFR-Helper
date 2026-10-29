#pragma once
#include "encounter_rebuild.hpp"
#include "field_anchor.hpp"
#include "map_unit_cache.hpp"
#include "progression_capture.hpp"
#include <functional>
#include <memory>

namespace ffr {
struct ReentrySnapshot {
    std::uint64_t root_identity{};
    std::vector<unsigned char> progression;
    MapUnitCache cache;
    FieldAnchor field;
};
struct ReentrySource {
    Encounter encounter;
    ReentrySnapshot snapshot;
};
struct ReentryAPI {
    // The native owner must establish the captured thread and map, fresh inactive
    // field/UI, released battle managers and no queued work or other mutation.
    std::function<bool()> field_boundary;
    // Check the complete profiled map-entry/copy/destructor/allocator chain.
    // A fingerprinted downstream consumer alone is NOT a consuming map entry.
    std::function<bool()> consuming_entry_profile;
    std::function<std::optional<ReentrySnapshot>()> snapshot;
    std::function<void*(std::size_t)> allocate;
    // Paired with allocate; must not throw, including during failed preparation.
    std::function<void(void*)> release;
    std::function<void(RawEntry*,bool)> consuming_map_entry;
};
enum class ReentryPhase { idle, checking, refused, invoked, returned, uncertain };

inline bool same_reentry_cache(const MapUnitCache& a,const MapUnitCache& b) {
    if(a.units.size()!=b.units.size() || a.units.empty() || a.units.size()>64)return false;
    std::set<std::int32_t> ids;
    for(std::size_t i=0;i<a.units.size();++i) {
        const auto& left=a.units[i];const auto& right=b.units[i];
        // These captures do not include full effect objects. A nonzero count
        // cannot be treated as an equal or restorable effect graph.
        if(left.continuous_effect_count || right.continuous_effect_count
            || left.raw!=right.raw || left.accessories!=right.accessories || left.passives!=right.passives
            || left.accessories.size()>16 || left.passives.size()>128)return false;
        for(const auto& [name,offset]:basic_unit_fields)if(!left.raw.count(name))return false;
        if(left.raw.at("unitId")<0 || !ids.insert(left.raw.at("unitId")).second)return false;
    }
    return true;
}
inline bool same_reentry_snapshot(const ReentrySnapshot& source,const ReentrySnapshot& current) {
    constexpr unsigned char magic[]{'G','V','A','S',3,0,0,0};
    if(!source.root_identity || source.root_identity!=current.root_identity || source.progression.size()<8
        || source.progression.size()>static_cast<std::size_t>(max_progression_bytes)
        || std::memcmp(source.progression.data(),magic,sizeof(magic)) || source.progression!=current.progression
        || source.field.map_id<0 || source.field.map_id!=current.field.map_id
        || source.field.direction>8 || source.field.direction!=current.field.direction
        || source.field.position!=current.field.position)return false;
    for(auto coordinate:source.field.position)if(!std::isfinite(coordinate) || std::abs(coordinate)>1e12)return false;
    return same_reentry_cache(source.cache,current.cache);
}

// Unbound, experimental recreation core. It performs NO rollback or RNG reset.
// Byte-identical saved progression and matching PARTIAL field/cache evidence
// are prerequisites for a future supervised source-preserving reentry test;
// they do not prove complete cache/world coherence or a functioning restart.
// The native owner must bind an explicit, expiring, session/nonce/source request
// after verified abort cleanup. This core is not connected to either game DLL.
class EncounterReentry {
    const ReentrySource source;
    ReentryPhase state=ReentryPhase::idle;
    std::string note="No encounter recreation requested";
    bool invoked=false;
    bool refuse(const char* message) {state=ReentryPhase::refused;note=message;return false;}
public:
    explicit EncounterReentry(ReentrySource retained):source(std::move(retained)) {}
    ReentryPhase phase() const {return state;}
    bool invocation_attempted() const {return invoked;}
    const std::string& message() const {return note;}
    const ReentrySource& retained_source() const {return source;}
    bool attempt(const ReentryAPI& api) {
        if(state!=ReentryPhase::idle)return false;
        // Reentrancy and all failed preparations are terminal for this instance.
        state=ReentryPhase::checking;
        try {
            const auto& e=source.encounter;
            if(!api.field_boundary || !api.consuming_entry_profile || !api.snapshot || !api.allocate || !api.release
                || !api.consuming_map_entry || e.event!=-1 || !e.can_escape || e.skip_win_result || e.skip_lose_result
                || e.bounty || e.battle_test || e.preemption || e.skip_intro || e.friend_ids.empty() || e.enemy_ids.empty()
                || !same_reentry_snapshot(source.snapshot,source.snapshot))return refuse("Unsupported source or incomplete reentry API");
            std::set<std::int32_t> friends;
            for(auto id:e.friend_ids)if(id<0 || !friends.insert(id).second)return refuse("Invalid source party");
            for(const auto& unit:source.snapshot.cache.units)friends.erase(unit.raw.at("unitId"));
            if(!friends.empty())return refuse("Source party is absent from the retained cache");
            if(!api.field_boundary() || !api.consuming_entry_profile())return refuse("Inactive field or consuming-entry profile is unavailable");
            const auto before=api.snapshot();
            if(!before || !same_reentry_snapshot(source.snapshot,*before))return refuse("Progression, field, root or captured cache changed");
            EngineEncounter rebuilt(source.encounter,api.allocate,api.release);
            // Allocating the graph cannot waive a new context or code mismatch.
            const auto after=api.snapshot();
            if(!after || !same_reentry_snapshot(source.snapshot,*after) || !api.field_boundary()
                || !api.consuming_entry_profile())return refuse("Reentry prerequisites changed during preparation");
            // Transfer immediately before native entry. Never free input blocks
            // ourselves after an uncertain native call: the map path consumes
            // them. The descriptor itself remains alive until the call returns.
            auto descriptor=rebuilt.transfer();
            invoked=true;state=ReentryPhase::invoked;
            api.consuming_map_entry(&descriptor,source.encounter.skip_intro);
            state=ReentryPhase::returned;
            note="Consuming map entry returned once; new encounter and full state still require independent verification";
            return true;
        }catch(...) {
            state=invoked?ReentryPhase::uncertain:ReentryPhase::refused;
            note=invoked?"Native entry outcome is uncertain; no retry or helper input-buffer free":"Reentry preparation failed; no native entry was invoked";
            return false;
        }
    }
};
} // namespace ffr
