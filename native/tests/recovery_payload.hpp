#pragma once
// Original synthetic fixture data, not copied from an installed game or save.
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
namespace recovery_fixture_data {
using Bytes=std::vector<unsigned char>;
template<class T> void number(Bytes& out,T value){const auto* raw=reinterpret_cast<const unsigned char*>(&value);out.insert(out.end(),raw,raw+sizeof(value));}
inline void string(Bytes& out,const std::string& value){number(out,static_cast<std::int32_t>(value.size()+1));out.insert(out.end(),value.begin(),value.end());out.push_back(0);}
struct Type {std::string name;std::vector<Type> children;};
inline void type(Bytes& out,const Type& value){string(out,value.name);number(out,static_cast<std::int32_t>(value.children.size()));for(const auto& child:value.children)type(out,child);}
inline Type structure(const std::string& name){return {"StructProperty",{{name,{{"/Script/mucha",{}}}}}};}
inline void property(Bytes& out,const std::string& name,const Type& kind,const Bytes& value){string(out,name);type(out,kind);number(out,static_cast<std::int32_t>(value.size()));out.push_back(0);out.insert(out.end(),value.begin(),value.end());}
template<class T> Bytes scalar(T value){Bytes out;number(out,value);return out;}
inline Bytes payload(float playtime,std::int32_t money) {
    Bytes out{'G','V','A','S'};number(out,std::int32_t{3});number(out,std::int32_t{522});number(out,std::int32_t{1017});
    number(out,std::uint16_t{5});number(out,std::uint16_t{6});number(out,std::uint16_t{1});number(out,std::uint32_t{0});
    string(out,"UE5");number(out,std::int32_t{3});number(out,std::int32_t{0});string(out,"/Script/mucha.CPP_MuchaGameSaveData");out.push_back(0);
    Bytes player;property(player,"PlayTime",{"FloatProperty",{}},scalar(playtime));property(player,"currentMoney",{"IntProperty",{}},scalar(money));
    string(player,"None");property(out,"Player",structure("SaveData_Player"),player);
    for(const auto& pair:{std::pair{"unitParameterList","SaveData_UnitParameter"},std::pair{"inventory","SaveData_Inventory"},std::pair{"visionList","SaveData_Vision"}})
        property(out,pair.first,{"ArrayProperty",{structure(pair.second)}},scalar(std::int32_t{0}));
    property(out,"UnknownFixtureField",{"IntProperty",{}},scalar(std::int32_t{123456}));
    string(out,"None");number(out,std::uint32_t{0});return out;
}
}
