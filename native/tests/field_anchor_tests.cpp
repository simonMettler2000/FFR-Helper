#include "field_anchor.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
#define CHECK(x) do {if(!(x))throw std::runtime_error("Field anchor check failed at "+std::to_string(__LINE__));}while(false)
int main() {
    try {
        std::array<unsigned char,0x800> actor{};
        std::array<unsigned char,0x230> component{};
        std::array<unsigned char,0xd0> operation{};
        auto write=[](auto& buffer,std::size_t offset,const auto& value){std::memcpy(buffer.data()+offset,&value,sizeof(value));};
        for(auto* data:{actor.data(),component.data(),operation.data()}) {
            const std::uint64_t marker=1;std::memcpy(data,&marker,8);std::memcpy(data+16,&marker,8);
        }
        write(actor,0x1b8,reinterpret_cast<std::uint64_t>(component.data()));
        write(actor,0x778,reinterpret_cast<std::uint64_t>(operation.data()));
        const std::array<double,3> location{123456.125,-9876.25,10.000001};
        write(component,0x200,location);operation[0xc9]=8;
        auto address=reinterpret_cast<std::uint64_t>(actor.data());
        ffr::Reader reader=[&](auto from,auto to,auto size){
            for(const auto& buffer:{std::pair{actor.data(),actor.size()},std::pair{component.data(),component.size()},std::pair{operation.data(),operation.size()}}) {
                const auto begin=reinterpret_cast<std::uint64_t>(buffer.first);
                if(from>=begin && from-begin<=buffer.second && size<=buffer.second-(from-begin)) {std::memcpy(to,reinterpret_cast<void*>(from),size);return true;}
            }return false;
        };
        auto copied=ffr::capture_field_anchor(reader,1000,address);
        CHECK(copied && copied->position==location && copied->direction==8 && copied->map_id==1000);
        const auto json=ffr::to_json(*copied);CHECK(json.find("\"restorable\":false")!=std::string::npos);
        CHECK(json.find("123456.125")!=std::string::npos);
        write(component,0x200,std::array<double,3>{1,2,3});CHECK(copied->position==location);
        CHECK(!ffr::capture_field_anchor(reader,-1,address));CHECK(!ffr::capture_field_anchor(reader,1,0));
        operation[0xc9]=9;CHECK(!ffr::capture_field_anchor(reader,1,address));operation[0xc9]=0;
        for(auto value:{std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN(),1.1e12}) {
            write(component,0x200,value);CHECK(!ffr::capture_field_anchor(reader,1,address));
        }
        write(component,0x200,location);actor[11]=0x40;CHECK(!ffr::capture_field_anchor(reader,1,address));actor[11]=0;
        write(actor,0x778,std::uint64_t{0});CHECK(!ffr::capture_field_anchor(reader,1,address));
        CHECK(!ffr::capture_field_anchor([](auto,auto,auto){return false;},1,address));
        std::cout<<"Detached live field position/direction and refusal checks passed.\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
