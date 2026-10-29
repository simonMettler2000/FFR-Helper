#include "combat_observation.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
#define CHECK(x) do {if(!(x))throw std::runtime_error("Combat observation check failed at "+std::to_string(__LINE__));}while(false)

struct Memory {
    std::vector<unsigned char> bytes=std::vector<unsigned char>(128*1024);
    std::size_t used=16;
    std::uint64_t allocate(std::size_t count) {used=(used+7)&~std::size_t(7);CHECK(used+count<=bytes.size());auto at=used;used+=count;return at+0x10000;}
    template<class T> void put(std::uint64_t address,const T& value) {CHECK(address>=0x10000 && address-0x10000+sizeof(T)<=bytes.size());std::memcpy(bytes.data()+address-0x10000,&value,sizeof(T));}
    template<class T> ffr::RawArray array(const std::vector<T>& items) {
        auto address=allocate(items.size()*sizeof(T));if(!items.empty())std::memcpy(bytes.data()+address-0x10000,items.data(),items.size()*sizeof(T));
        return {address,static_cast<std::int32_t>(items.size()),static_cast<std::int32_t>(items.size())};
    }
    std::uint64_t object(std::size_t size) {auto address=allocate(size);put(address,std::uint64_t{1});put(address+16,std::uint64_t{1});return address;}
    bool read(std::uint64_t address,void* out,std::size_t count) const {
        if(address<0x10000||address-0x10000>bytes.size()||count>bytes.size()-(address-0x10000))return false;
        std::memcpy(out,bytes.data()+address-0x10000,count);return true;
    }
};
struct Fixture {
    Memory m;
    std::uint64_t manager=m.object(0x240),units=m.object(0x58),jobs=m.object(0x48),scheduler=m.object(0x40);
    std::array<std::uint64_t,2> actors{m.object(0x880),m.object(0x880)},parameters{m.object(0x560),m.object(0x560)};
    std::uint64_t ai=m.object(0x60);
    ffr::RawArray actor_list{},stack{};
    Fixture() {
        m.put(manager+0x1a9,std::uint8_t{1});m.put(manager+0x48,units);m.put(manager+0x40,jobs);m.put(manager+0x200,std::int32_t{3});
        actor_list=m.array<std::uint64_t>({actors[0],actors[1]});m.put(units+0x28,actor_list);m.put(units+0x38,actors[0]);
        m.put(units+0x40,scheduler);m.put(scheduler+0x28,units);
        m.put(scheduler+0x30,m.array<std::uint64_t>({actors[1],actors[0],actors[1]}));
        m.put(units+0x48,m.array<std::uint64_t>({actors[1]}));
        for(unsigned i=0;i<2;++i) {
            m.put(actors[i]+0x838,parameters[i]);m.put(parameters[i]+0x360,actors[i]);
            m.put(parameters[i]+0xd0,std::int32_t{1001});m.put(parameters[i]+0x550,static_cast<std::int32_t>(i));
            m.put(parameters[i]+0x100,std::int32_t{12});m.put(parameters[i]+0x10c,std::int32_t{150});
            m.put(parameters[i]+0x268,std::int32_t{25});m.put(parameters[i]+0x110,std::int32_t{-20});
            m.put(parameters[i]+0x114,std::int32_t{500});m.put(parameters[i]+0x118,std::int32_t{44});
            m.put(parameters[i]+0x120,std::int32_t{200});m.put(parameters[i]+0x124,std::int32_t{99});m.put(parameters[i]+0x128,std::int32_t{100});
            m.put(parameters[i]+0x4dc,std::int32_t{2});m.put(parameters[i]+0x4fc,std::int32_t{3});
            m.put(parameters[i]+0x500,std::int32_t{4});m.put(parameters[i]+0x504,std::int32_t{5});
            m.put(parameters[i]+0x1f0,m.array<std::int32_t>({101,202}));m.put(parameters[i]+0x208,m.array<std::int32_t>({303,404}));
            m.put(parameters[i]+0xb8,m.array<std::uint64_t>({1,2})); // Count only; effects are explicitly incomplete.
        }
        m.put(actors[1]+0x870,ai);m.put(ai+0x28,actors[1]);m.put(ai+0x40,std::int32_t{3001});
        m.put(ai+0x44,std::int32_t{3002});m.put(ai+0x48,std::int32_t{-1});m.put(ai+0x30,m.array<std::uint64_t>({actors[0]}));
        const ffr::RawSkillStack first{3003,0,m.array<std::uint64_t>({actors[1],0})},second{3004,0,m.array<std::uint64_t>({actors[0]})};
        stack=m.array<ffr::RawSkillStack>({first,second});m.put(ai+0x50,stack);
        m.put(parameters[0]+0x368,actors[1]);m.put(parameters[1]+0x370,actors[0]);m.put(parameters[0]+0x380,actors[1]);
        m.put(parameters[0]+0x388,m.array<std::uint64_t>({actors[1],actors[1],0}));
        m.put(parameters[0]+0x270,m.array<std::uint64_t>({parameters[1],0,parameters[0]}));
    }
    std::optional<ffr::CombatObservation> capture() {return ffr::capture_combat_observation([&](auto a,auto b,auto c){return m.read(a,b,c);},manager);}
};
int main() {
    try {
        {
            Fixture f;auto result=f.capture();CHECK(result && result->units.size()==2 && result->selected_unit==0 && result->battle_turn==3);
            CHECK(result->target_units==std::vector<std::int32_t>{1});CHECK(result->additional_timeline==std::vector<std::int32_t>({1,0,1}));
            const auto& first=result->units[0];const auto& second=result->units[1];
            CHECK(first.raw.at("unitId")==second.raw.at("unitId") && second.raw.at("duplicationId")==1);
            CHECK(first.raw.at("hpBase")==150 && first.raw.at("temporaryHp")==25);
            CHECK(first.raw.at("mp")==-20 && first.raw.at("stagger")==500 && first.raw.at("maxStagger")==100);
            CHECK(first.continuous_effect_count==2 && first.accessories==std::vector<std::int32_t>({101,202}));
            CHECK(!first.ai && second.ai && second.ai->targets==std::vector<std::int32_t>{0});
            CHECK(second.ai->stack[0].targets==std::vector<std::int32_t>({1,-1}) && second.ai->stack[1].skill==3004);
            CHECK(result->unit_relations && result->unit_relations->size()==2);
            const auto& links=result->unit_relations->at(0);CHECK(links.vision==1 && links.vision_owner==-1 && links.last_damage_dealer==1);
            CHECK(links.damage_sources==std::vector<std::int32_t>({1,1,-1}) && links.active_skill_targets==std::vector<std::int32_t>({1,-1,0}));
            CHECK(result->unit_relations->at(1).vision_owner==0);
            const auto json=ffr::to_json(*result);CHECK(json.find("\"restorable\":false")!=std::string::npos);
            CHECK(json.find("\"mp\":-20")!=std::string::npos && json.find("Native RNG state and draw order")!=std::string::npos);
            f.m.put(f.parameters[0]+0x10c,std::int32_t{1});f.m.put(f.stack.data,std::int32_t{999});
            f.m.put(f.parameters[0]+0x368,f.actors[0]);CHECK(ffr::to_json(*result)==json);
        }
        for(unsigned failure=0;failure<14;++failure) {
            Fixture f;
            switch(failure) {
            case 0:f.m.put(f.manager+0x1a9,std::uint8_t{0});break;
            case 1:f.m.put(f.jobs+0x28,f.m.array<std::uint64_t>({1}));break;
            case 2:f.m.put(f.units+0x28,ffr::RawArray{f.actor_list.data,65,65});break;
            case 3:f.m.put(f.units+0x28,f.m.array<std::uint64_t>({f.actors[0],f.actors[0]}));break;
            case 4:f.m.put(f.parameters[0]+0x360,f.actors[1]);break;
            case 5:f.m.put(f.units+0x38,std::uint64_t{0});break;
            case 6:f.m.put(f.ai+0x30,f.m.array<std::uint64_t>({0x99999999}));break;
            case 7:f.m.put(f.ai+0x28,f.actors[0]);break;
            case 8:f.m.put(f.actors[0]+8,std::uint32_t{0x40000000});break;
            case 9:f.m.put(f.manager+0x200,std::int32_t{-1});break;
            case 10:f.m.put(f.parameters[0]+0x208,ffr::RawArray{1,129,129});break;
            case 11:f.m.put(f.scheduler+0x28,std::uint64_t{0});break;
            case 12:f.m.put(f.ai+0x50,ffr::RawArray{f.stack.data,257,257});break;
            case 13:f.m.put(f.stack.data+8,f.m.array<std::uint64_t>({0x99999999}));break;
            }
            CHECK(!f.capture());
        }
        {
            Fixture f;auto targets=f.m.array<std::uint64_t>(std::vector<std::uint64_t>(64,f.actors[0]));
            f.m.put(f.ai+0x50,f.m.array<ffr::RawSkillStack>(std::vector<ffr::RawSkillStack>(64,ffr::RawSkillStack{1,0,targets})));
            CHECK(!f.capture()); // Aggregate reference budget, not merely per-array limits.
        }
        for(unsigned failure=0;failure<4;++failure) {
            Fixture f;
            if(failure==0)f.m.put(f.parameters[0]+0x368,std::uint64_t{0x99999999});
            if(failure==1)f.m.put(f.parameters[0]+0x270,f.m.array<std::uint64_t>({f.actors[1]})); // Wrong domain: actor, not parameter.
            if(failure==2)f.m.put(f.parameters[0]+0x388,f.m.array<std::uint64_t>({f.parameters[1]}));
            if(failure==3)f.m.put(f.parameters[0]+0x388,ffr::RawArray{1,65,65});
            auto result=f.capture();CHECK(result && !result->unit_relations && result->units[0].raw.at("hpBase")==150);
            CHECK(ffr::to_json(*result).find("\"unitRelations\":null")!=std::string::npos);
        }
        {
            Fixture f;const std::vector<std::uint64_t> actors(f.actors.begin(),f.actors.end()),parameters(f.parameters.begin(),f.parameters.end());
            CHECK(!ffr::capture_combat_relations([&](auto a,auto b,auto c){return f.m.read(a,b,c);},actors,parameters,11));
            CHECK(ffr::capture_combat_relations([&](auto a,auto b,auto c){return f.m.read(a,b,c);},actors,parameters,12));
        }
        CHECK(!ffr::capture_combat_observation([](auto,auto,auto){return false;},0x10000));
        std::cout<<"Detached combat components, duplicate-unit identities and deep AI-reference checks passed.\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
