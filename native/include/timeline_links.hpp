#pragma once
// Partial, capture-local UI links only. No engine calls, setters or rendering.
#include "encounter_capture.hpp"
#include <map>
#include <set>

namespace ffr {
struct TimelineUnitLink {
    std::int32_t unique_id{},source_unit=-1,cached_unit=-1;
    bool source_matches_cached{};
    std::string cached_state;
    std::optional<std::int32_t> cached_id,fallback_relation;
};
struct TimelineLinks {
    std::vector<TimelineUnitLink> links;
    std::vector<std::int32_t> missing_units;
};

inline std::optional<TimelineLinks> capture_timeline_links(const Reader& read,std::uint64_t ui,
                                                          const std::vector<std::uint64_t>& parameters) {
    if(parameters.empty() || parameters.size()>64)return {};
    struct Region {std::uint64_t address;std::vector<unsigned char> bytes;};
    std::vector<Region> regions;std::size_t budget=256*1024;
    auto copy=[&](std::uint64_t address,void* output,std::size_t size) {
        if(address<0x10000 || address>=0x7fffffffffff || !size || size>budget || size>=0x7fffffffffff-address)return false;
        std::vector<unsigned char> bytes(size);if(!read(address,bytes.data(),size))return false;
        budget-=size;
        for(const auto& prior:regions)if(prior.address==address && prior.bytes.size()==size && prior.bytes!=bytes)return false;
        std::memcpy(output,bytes.data(),size);regions.push_back({address,std::move(bytes)});return true;
    };
    auto value=[&]<class T>(std::uint64_t address,T& output){return copy(address,&output,sizeof(output));};
    auto valid=[](const auto& h){return h[0] && h[2] && !(h[1]&0x40000000u);};
    auto object=[&](std::uint64_t address){std::array<std::uint64_t,4> h{};return value(address,h) && valid(h);};
    try {
        std::map<std::uint64_t,std::int32_t> indices;
        for(std::size_t i=0;i<parameters.size();++i) {
            if(!parameters[i] || !indices.emplace(parameters[i],static_cast<std::int32_t>(i)).second)return {};
        }
        std::uint64_t owner{};std::uint8_t during{};
        if(!object(ui) || !value(ui+0x80,during) || during!=1 || !value(ui+0x78,owner) || !object(owner))return {};
        std::array<unsigned char,0x50> header{};
        if(!value(owner+0x28,header))return {};
        RawArray rows{};std::int32_t free{},bucket_count{};std::uint64_t bucket_data{};
        std::memcpy(&rows,header.data(),16);std::memcpy(&free,header.data()+0x34,4);
        std::memcpy(&bucket_data,header.data()+0x40,8);std::memcpy(&bucket_count,header.data()+0x48,4);
        if(!valid_array(rows,256) || free<0 || free>rows.count || bucket_count<0 || bucket_count>512
            || (bucket_count && (bucket_count&(bucket_count-1))) || (!bucket_data && bucket_count>1)
            || (!bucket_count && rows.count!=free))return {};
        std::vector<unsigned char> payload(static_cast<std::size_t>(rows.count)*24);
        std::vector<std::int32_t> heads(static_cast<std::size_t>(bucket_count));
        if((!payload.empty() && !copy(rows.data,payload.data(),payload.size()))
            || (!heads.empty() && !copy(bucket_data?bucket_data:owner+0x60,heads.data(),heads.size()*4)))return {};
        TimelineLinks result;std::set<std::int32_t> slots,keys,represented;std::set<std::uint64_t> models;
        auto index=[&](std::uint64_t pointer){const auto found=indices.find(pointer);return found==indices.end()?-1:found->second;};
        for(std::size_t bucket=0;bucket<heads.size();++bucket) {auto slot=heads[bucket];while(slot!=-1) {
            if(slot<0 || slot>=rows.count || !slots.insert(slot).second)return {};
            const auto at=static_cast<std::size_t>(slot)*24;std::int32_t key{},next{},stored{};std::uint64_t vm{},source{},cached{};
            std::memcpy(&key,payload.data()+at,4);std::memcpy(&vm,payload.data()+at+8,8);std::memcpy(&next,payload.data()+at+16,4);
            if(key<0 || (static_cast<std::uint32_t>(key)&static_cast<std::uint32_t>(bucket_count-1))!=bucket
                || !keys.insert(key).second || !models.insert(vm).second || !object(vm)
                || !value(vm+0x198,stored) || stored!=key || !value(vm+0x70,source) || !value(vm+0x1a0,cached))return {};
            TimelineUnitLink link;link.unique_id=key;link.source_unit=index(source);link.source_matches_cached=source==cached;
            if(!cached) {link.cached_state="null";link.fallback_relation=0;}
            else {
                std::array<std::uint64_t,4> h{};
                if(!value(cached,h))link.cached_state="unreadable";
                else if(h[1]&0x40000000u) {link.cached_state="invalid-object";link.fallback_relation=0;}
                else if(!valid(h))link.cached_state="unsupported-object-header";
                else {
                    std::int32_t id{};
                    if(!value(cached+0xd0,id))link.cached_state="unreadable";
                    else {
                        link.cached_id=id;link.cached_unit=index(cached);
                        link.cached_state=link.cached_unit<0?"outside-current":"current";
                        if(link.cached_unit>=0)represented.insert(link.cached_unit);
                    }
                }
            }
            result.links.push_back(std::move(link));slot=next;
        }}
        if(slots.size()!=static_cast<std::size_t>(rows.count-free))return {};
        for(std::size_t i=0;i<parameters.size();++i)if(!represented.count(static_cast<std::int32_t>(i)))result.missing_units.push_back(static_cast<std::int32_t>(i));
        for(const auto& region:regions) {
            std::vector<unsigned char> fresh(region.bytes.size());
            if(!read(region.address,fresh.data(),fresh.size()) || fresh!=region.bytes)return {};
        }
        return result;
    }catch(...) {return {};}
}

inline std::string timeline_links_json(const std::optional<TimelineLinks>& observation) {
    if(!observation)return "null";
    std::string out="{\"coverage\":\"capture-local-ui-unit-parameter-links\",\"readOnly\":true,\"restorable\":false,"
        "\"atomicSnapshot\":false,\"relationVerified\":false,\"renderedAnchorsVerified\":false,\"unitsWithoutCurrentViewModel\":"
        +integers(observation->missing_units)+",\"links\":[";
    for(const auto& link:observation->links) {
        if(out.back()!='[')out+=',';
        out+="{\"uniqueId\":"+std::to_string(link.unique_id)+",\"sourceUnit\":"+std::to_string(link.source_unit)
            +",\"cachedUnit\":"+std::to_string(link.cached_unit)+",\"sourceMatchesCached\":"+(link.source_matches_cached?"true":"false")
            +",\"cachedParameterState\":"+quote(link.cached_state)+",\"cachedUnitId\":"+(link.cached_id?std::to_string(*link.cached_id):"null")
            +",\"fallbackRelation\":"+(link.fallback_relation?std::to_string(*link.fallback_relation):"null")+",\"actualRelationVerified\":false}";
    }
    return out+"]}";
}
} // namespace ffr
