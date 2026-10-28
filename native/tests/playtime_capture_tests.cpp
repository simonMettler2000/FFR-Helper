#include "playtime_capture.hpp"
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#define CHECK(x) do {if(!(x))throw std::runtime_error("Playtime check failed at "+std::to_string(__LINE__));}while(false)
struct Fixture {
    std::array<unsigned char,0x40> root{};
    std::array<unsigned char,0x48> clock{};
    ffr::Reader read;
    std::function<void*()> get_root,get_clock;
    std::function<std::int64_t()> get_seconds;
    template<class T> void put(auto& target,std::size_t offset,const T& value){std::memcpy(target.data()+offset,&value,sizeof(value));}
    Fixture() {
        for(auto* buffer:{root.data(),clock.data()}) {
            const std::uint64_t marker=1;std::memcpy(buffer,&marker,8);std::memcpy(buffer+16,&marker,8);
        }
        put(root,0x38,1000.25f);put(clock,0x30,std::array<std::int64_t,3>{9000,0,30});
        read=[&](auto from,auto to,auto size) {
            for(const auto& buffer:{std::pair{root.data(),root.size()},std::pair{clock.data(),clock.size()}}) {
                const auto start=reinterpret_cast<std::uint64_t>(buffer.first);
                if(from>=start && from-start<=buffer.second && size<=buffer.second-(from-start)) {
                    std::memcpy(to,reinterpret_cast<void*>(from),size);return true;
                }
            }return false;
        };
        get_root=[&]()->void* {return root.data();};get_clock=[&]()->void* {return clock.data();};
        get_seconds=[]()->std::int64_t {return 10000;};
    }
    auto capture(){return ffr::capture_playtime_basis(read,root.data(),get_root,get_clock,get_seconds);}
};
int main() {
    try {
        {
            Fixture f;auto result=f.capture();CHECK(result && result->saved_seconds==1000.25f && result->elapsed_seconds==970);
            CHECK(result->start_seconds==9000 && result->sample_seconds==10000 && result->suspended_start_seconds==0);
            const auto json=ffr::to_json(*result);CHECK(json.find("\"restorable\":false")!=std::string::npos && json.find("1000.25")!=std::string::npos);
            f.put(f.clock,0x30,std::int64_t{9500});f.put(f.root,0x38,2000.5f);
            CHECK(result->start_seconds==9000 && result->saved_seconds==1000.25f);
        }
        {
            Fixture f;f.put(f.clock,0x38,std::int64_t{9900});auto result=f.capture();
            CHECK(result && result->suspended_start_seconds==9900 && result->elapsed_seconds==970); // Ongoing suspension not silently subtracted.
        }
        for(const auto& data:{std::array<std::int64_t,3>{0,0,0},{10001,0,0},{9000,8999,0},{9000,10001,0},
                             {9000,0,-1},{9000,0,1001},{std::numeric_limits<std::int64_t>::min(),0,0},
                             {9000,0,std::numeric_limits<std::int64_t>::max()}}) {
            Fixture f;f.put(f.clock,0x30,data);CHECK(!f.capture());
        }
        for(const auto value:{-1.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN(),1.1e12f}) {
            Fixture f;f.put(f.root,0x38,value);CHECK(!f.capture());
        }
        {
            Fixture f;f.get_clock=[]()->void* {return nullptr;};CHECK(!f.capture());
        }
        {
            Fixture f;f.root[11]=0x40;CHECK(!f.capture());
        }
        {
            Fixture f;unsigned calls=0;f.get_root=[&]()->void* {return ++calls==1?f.root.data():nullptr;};CHECK(!f.capture());
        }
        {
            Fixture f;unsigned calls=0;f.get_clock=[&]()->void* {return ++calls==1?f.clock.data():nullptr;};CHECK(!f.capture());
        }
        {
            Fixture f;f.get_seconds=[&]()->std::int64_t {f.put(f.clock,0x40,std::int64_t{31});return 10000;};CHECK(!f.capture());
        }
        {
            Fixture f;f.get_seconds=[&]()->std::int64_t {f.put(f.root,0x38,1001.25f);return 10000;};CHECK(!f.capture());
        }
        {
            Fixture f;f.read=[](auto,auto,auto){return false;};CHECK(!f.capture());
        }
        std::cout<<"Detached playtime basis, ongoing suspension, coherent roots and invalid-clock refusal checks passed.\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
