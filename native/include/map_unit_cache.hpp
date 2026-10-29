#pragma once
#include "encounter_capture.hpp"
#include "unit_components.hpp"
#include <map>
#include <set>
namespace ffr {
struct MapUnitComponent {
    std::map<std::string,std::int32_t> raw;
    std::vector<std::int32_t> accessories,passives;
    std::int32_t continuous_effect_count{};
};
struct MapUnitCache {std::vector<MapUnitComponent> units;};
inline std::optional<MapUnitCache> capture_map_unit_cache(const Reader& read,std::uint64_t map,std::uint64_t expected_storage_vtable) {
    auto value=[&]<class T>(std::uint64_t address,T& out){return address && read(address,&out,sizeof(out));};
    auto object=[&](std::uint64_t address) {
        std::array<std::uint64_t,4> header{};
        return address && read(address,header.data(),sizeof(header)) && header[0] && header[2] && !(header[1]&0x40000000u);
    };
    std::uint64_t storage{},vtable{};RawArray list{};
    if(!expected_storage_vtable || !object(map) || !value(map+0x208,storage) || !object(storage)
        || !value(storage,vtable) || vtable!=expected_storage_vtable || !value(storage+0x28,list))return {};
    std::vector<std::uint64_t> pointers;
    if(!copy_array(read,list,64,pointers))return {};
    MapUnitCache out;std::set<std::uint64_t> seen;std::size_t budget=2048;
    for(auto pointer:pointers) {
        if(!object(pointer) || !seen.insert(pointer).second)return {};
        MapUnitComponent unit;
        for(const auto& [name,offset]:basic_unit_fields)if(!value(pointer+offset,unit.raw[name]))return {};
        if(unit.raw.at("unitId")<0)return {};
        RawArray accessories{},passives{},effects{};
        if(!value(pointer+0x1f0,accessories) || !copy_array(read,accessories,16,unit.accessories)
            || !value(pointer+0x208,passives) || !copy_array(read,passives,128,unit.passives)
            || !value(pointer+0xb8,effects) || !valid_array(effects,1024)
            || unit.accessories.size()+unit.passives.size()>budget)return {};
        budget-=unit.accessories.size()+unit.passives.size();unit.continuous_effect_count=effects.count;
        out.units.push_back(std::move(unit));
    }
    std::uint64_t after{};RawArray latest{};
    if(!value(map+0x208,after) || after!=storage || !value(storage+0x28,latest)
        || latest.data!=list.data || latest.count!=list.count || latest.capacity!=list.capacity)return {};
    std::vector<std::uint64_t> last;
    if(!copy_array(read,latest,64,last) || last!=pointers || !value(map+0x208,after) || after!=storage
        || !value(storage,vtable) || vtable!=expected_storage_vtable)return {};
    return out;
}
inline std::string to_json(const MapUnitCache& cache) {
    std::string out="{\"kind\":\"partial-map-unit-cache\",\"source\":\"map-manager-unit-parameter-storage\","
        "\"coverage\":\"raw-parameter-components-only\",\"readOnly\":true,\"restorable\":false,\"coherentWithProgression\":false,\"units\":[";
    for(std::size_t i=0;i<cache.units.size();++i) {
        if(i)out+=',';const auto& unit=cache.units[i];out+="{\"index\":"+std::to_string(i)+",\"raw\":{";bool first=true;
        for(const auto& [name,value]:unit.raw){if(!first)out+=',';first=false;out+=quote(name)+':'+std::to_string(value);}
        out+="},\"accessories\":"+integers(unit.accessories)+",\"passives\":"+integers(unit.passives)
            +",\"continuousEffectCount\":"+std::to_string(unit.continuous_effect_count)+'}';
    }
    return out+"]}";
}
}
