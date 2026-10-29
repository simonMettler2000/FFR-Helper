#include "encounter_reentry.hpp"
#include <cstdlib>
#include <iostream>
#include <map>

#define CHECK(x) do {if(!(x))throw std::runtime_error("Reentry check failed at "+std::to_string(__LINE__));}while(false)
ffr::ReentrySource sample() {
    ffr::ReentrySource source;
    auto& e=source.encounter;
    e.friend_ids={1001,1002};e.friend_locations={0,1};e.enemy_ids={11};e.enemy_locations={2};
    e.passives={{77,88},{99},{}};e.bgm="Fixture";e.event=-1;e.can_escape=true;e.escape_probability=50;
    auto& s=source.snapshot;s.root_identity=1;s.progression={'G','V','A','S',3,0,0,0,42};
    s.field={7,{1,2,3},4};
    for(auto id:e.friend_ids) {
        ffr::MapUnitComponent unit;
        for(auto [name,offset]:ffr::basic_unit_fields)unit.raw[name]=0;
        unit.raw["unitId"]=id;unit.raw["hpBase"]=123;unit.raw["mp"]=17;
        unit.accessories={2};unit.passives={3};s.cache.units.push_back(unit);
    }
    return source;
}
struct Fixture {
    ffr::ReentrySource source=sample();ffr::ReentrySnapshot current=source.snapshot;
    std::map<void*,std::size_t> allocations;
    unsigned snapshots{},invocations{},allocations_made{},releases{},profile_checks{},boundary_checks{};
    unsigned fail_allocation=100,change_profile_at=100,change_boundary_at=100;
    bool throw_in_entry=false;
    std::function<void()> entering;
    std::function<void(unsigned)> sampling;
    std::string supplied;
    void release(void* p) {
        CHECK(allocations.erase(p)==1);++releases;std::free(p);
    }
    void consume(const ffr::RawEntry& raw) {
        const auto nested=reinterpret_cast<const ffr::RawArray*>(raw.passives.data);
        for(int i=0;i<raw.passives.count;++i)if(nested[i].data)release(reinterpret_cast<void*>(nested[i].data));
        for(const auto& a:{raw.bgm,raw.passives,raw.enemy_locations,raw.enemy_ids,raw.friend_locations,raw.friend_ids})
            if(a.data)release(reinterpret_cast<void*>(a.data));
    }
    ffr::ReentryAPI api() {
        return {
            [&]{return ++boundary_checks<change_boundary_at;},
            [&]{return ++profile_checks<change_profile_at;},
            [&]()->std::optional<ffr::ReentrySnapshot> {if(sampling)sampling(++snapshots);else ++snapshots;return current;},
            [&](std::size_t bytes)->void* {
                if(++allocations_made==fail_allocation)return nullptr;
                auto* p=std::malloc(bytes);if(p)allocations[p]=bytes;return p;
            },
            [&](void* p){release(p);},
            [&](ffr::RawEntry* raw,bool skip) {
                ++invocations;
                ffr::Reader reader=[](auto address,auto destination,auto bytes) {std::memcpy(destination,reinterpret_cast<void*>(address),bytes);return true;};
                auto copied=ffr::capture_entry(reader,reinterpret_cast<std::uint64_t>(raw),skip);
                CHECK(copied);supplied=ffr::to_json(*copied);
                if(entering)entering();
                consume(*raw);
                if(throw_in_entry)throw std::runtime_error("Synthetic uncertain return");
            }
        };
    }
};
int main() {
    try {
        {
            Fixture f;ffr::EncounterReentry trial(f.source);auto api=f.api();
            // The retained source must not follow later mutations of its input.
            f.source.encounter.enemy_ids[0]=999;
            f.entering=[&]{CHECK(trial.invocation_attempted());CHECK(!trial.attempt(api));};
            CHECK(trial.attempt(api));CHECK(trial.phase()==ffr::ReentryPhase::returned);
            CHECK(f.invocations==1 && f.snapshots==2 && f.allocations.empty());
            CHECK(f.supplied==ffr::to_json(sample().encounter));
            CHECK(!trial.attempt(api) && f.invocations==1);
            CHECK(trial.retained_source().snapshot.progression==sample().snapshot.progression);
        }
        for(unsigned mode=0;mode<11;++mode) {
            Fixture f;
            if(mode==0)++f.current.progression.back();
            if(mode==1)++f.current.cache.units[0].raw["mp"];
            if(mode==2)++f.current.cache.units[0].raw["hpBase"];
            if(mode==3)f.current.cache.units[0].accessories.push_back(8);
            if(mode==4)f.current.cache.units[0].passives.push_back(8);
            if(mode==5)f.current.cache.units[0].continuous_effect_count=1;
            if(mode==6)++f.current.root_identity;
            if(mode==7)++f.current.field.position[0];
            if(mode==8)f.current.cache.units[0].raw.erase("maxHp");
            if(mode==9)f.current.cache.units[1].raw["unitId"]=1001;
            if(mode==10)++f.current.field.direction;
            ffr::EncounterReentry trial(f.source);
            CHECK(!trial.attempt(f.api()) && !trial.invocation_attempted() && !f.invocations && f.allocations.empty());
            CHECK(!f.allocations_made && !trial.attempt(f.api()));
        }
        for(unsigned mode=0;mode<5;++mode) {
            Fixture f;
            if(mode==0)f.change_profile_at=2;
            if(mode==1)f.change_boundary_at=2;
            if(mode==2)f.sampling=[&](auto count){if(count==2)++f.current.root_identity;};
            if(mode==3)f.sampling=[&](auto count){if(count==2)throw std::runtime_error("Snapshot unavailable");};
            if(mode==4)f.fail_allocation=3;
            ffr::EncounterReentry trial(f.source);
            CHECK(!trial.attempt(f.api()) && !trial.invocation_attempted() && !f.invocations && f.allocations.empty());
            CHECK(f.releases>0);
        }
        {
            Fixture f;f.throw_in_entry=true;ffr::EncounterReentry trial(f.source);
            CHECK(!trial.attempt(f.api()) && trial.invocation_attempted() && trial.phase()==ffr::ReentryPhase::uncertain);
            CHECK(f.invocations==1 && f.allocations.empty() && !trial.attempt(f.api()));
        }
        {
            Fixture f;auto api=f.api();api.consuming_map_entry=[&](auto*,bool){++f.invocations;throw std::runtime_error("Uncertain before consumption");};
            {
                ffr::EncounterReentry trial(f.source);CHECK(!trial.attempt(api));
                CHECK(trial.invocation_attempted() && !f.allocations.empty() && !f.releases);
            }
            // An uncertain native entry may already own/free these blocks. The
            // helper must retain rather than double-free. Only the fake engine
            // below knows these particular test allocations remain outstanding.
            CHECK(!f.releases);
            while(!f.allocations.empty())f.release(f.allocations.begin()->first);
        }
        for(unsigned mode=0;mode<4;++mode) {
            Fixture f;
            if(mode==0)f.source.encounter.preemption=1;
            if(mode==1)f.source.encounter.event=4;
            if(mode==2)f.source.encounter.skip_intro=true;
            if(mode==3)f.source.snapshot.cache.units[0].continuous_effect_count=1;
            ffr::EncounterReentry trial(f.source);CHECK(!trial.attempt(f.api()));
            CHECK(!f.snapshots && !f.allocations_made && !f.invocations);
        }
        std::cout<<"Unbound source-preserving reentry, drift refusal and consuming-input lifetime passed.\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
