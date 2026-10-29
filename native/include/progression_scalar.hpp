#pragma once
#include "progression_clone.hpp"
#include "playtime_normalization.hpp"

namespace ffr {
namespace scalar_detail {
struct Type {std::string name;std::vector<Type> children;};
struct Property {Type type;std::size_t offset{},size{};std::uint8_t flags{};};
class Tags {
    std::span<const unsigned char> bytes;
    unsigned nodes{};
    template<class T> bool number(std::size_t& position,std::size_t end,T& value) {
        if(position>end || sizeof(T)>end-position)return false;
        std::memcpy(&value,bytes.data()+position,sizeof(T));position+=sizeof(T);return true;
    }
    bool string(std::size_t& position,std::size_t end,std::string& value) {
        std::int32_t size{};
        if(!number(position,end,size) || size<1 || size>4096 || static_cast<std::size_t>(size)>end-position)return false;
        const auto* start=reinterpret_cast<const char*>(bytes.data()+position);position+=size;
        if(start[size-1] || std::memchr(start,0,size-1))return false;
        value.assign(start,size-1);return true;
    }
    bool type(std::size_t& position,std::size_t end,Type& out,unsigned depth=0) {
        std::int32_t count{};
        if(depth>12 || ++nodes>4096 || !string(position,end,out.name) || !number(position,end,count) || count<0 || count>8)return false;
        out.children.resize(count);
        for(auto& child:out.children)if(!type(position,end,child,depth+1))return false;
        return true;
    }
public:
    explicit Tags(std::span<const unsigned char> data):bytes(data){}
    std::optional<Property> unique(std::size_t begin,std::size_t end,const std::string& name) {
        if(begin>end || end>bytes.size())return {};
        std::optional<Property> found;
        auto position=begin;
        for(unsigned i=0;i<4096;++i) {
            std::string label;if(!string(position,end,label))return {};
            if(label=="None")return position==end?found:std::nullopt;
            Property field;std::uint32_t length{};
            if(!type(position,end,field.type) || !number(position,end,length) || !number(position,end,field.flags)
                || (field.flags&~0x1f) || (field.flags&4))return {};
            const auto extra=((field.flags&1)?4u:0u)+((field.flags&2)?16u:0u);
            if(extra>end-position)return {};position+=extra;
            if(length>end-position)return {};
            field.offset=position;field.size=length;position+=length;
            auto lower=[](std::string value){for(auto& c:value)if(c>='A' && c<='Z')c=static_cast<char>(c-'A'+'a');return value;};
            if(lower(label)==lower(name)){if(label!=name || found)return {};found=std::move(field);}
        }
        return {};
    }
};
}
struct ProgressionFloatLocation {std::size_t offset{};std::uint32_t bits{};};
// Locate only the exact native Player.PlayTime tagged scalar. No substring
// search, guessed offset, missing-default insertion or client offset is used.
inline std::optional<ProgressionFloatLocation> locate_saved_playtime(std::span<const unsigned char> source) {
    auto body=captured_progression_body(source);if(!body)return {};
    scalar_detail::Tags tags(source);
    auto player=tags.unique(*body,source.size()-4,"Player");
    if(!player || player->flags || player->type.name!="StructProperty" || player->type.children.size()!=1)return {};
    const auto& structure=player->type.children[0];
    if(structure.name!="SaveData_Player" || structure.children.size()!=1 || structure.children[0].name!="/Script/mucha"
        || !structure.children[0].children.empty())return {};
    auto scalar=tags.unique(player->offset,player->offset+player->size,"PlayTime");
    if(!scalar || scalar->flags || scalar->size!=4 || scalar->type.name!="FloatProperty" || !scalar->type.children.empty())return {};
    ProgressionFloatLocation out;out.offset=scalar->offset;std::memcpy(&out.bits,source.data()+out.offset,4);return out;
}
struct NormalizedProgression {
    std::vector<unsigned char> bytes;
    ProgressionFloatLocation original;
    PlaytimeNormalization clock;
};
inline std::optional<NormalizedProgression> prepare_normalized_progression(const std::vector<unsigned char>& source,const PlaytimeBasis& basis) {
    auto scalar=locate_saved_playtime(source);auto clock=plan_playtime_normalization(basis);
    if(!scalar || !clock || scalar->bits!=std::bit_cast<std::uint32_t>(basis.saved_seconds))return {};
    NormalizedProgression out{source,*scalar,*clock};
    const auto normalized=std::bit_cast<std::uint32_t>(clock->saved_after_normal_load);
    std::memcpy(out.bytes.data()+out.original.offset,&normalized,4);return out;
}
} // namespace ffr
