#include "crt_rand_capture.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error("CRT capture check failed at "+std::to_string(__LINE__));}while(false)
struct Fixture {
    std::array<unsigned char,8192> bytes{};
    ffr::CrtRandLayout layout{0x10000,0xabcdef,8,9,12,16,24};
    std::uint64_t index{},thread=42;bool fiber{};unsigned calls{};
    ffr::CrtRandAPI api;
    template<class T>void put(std::uint64_t at,T value){CHECK(at>=0x10000 && at-0x10000+sizeof(T)<=bytes.size());std::memcpy(bytes.data()+at-0x10000,&value,sizeof(value));}
    bool read(std::uint64_t at,void* out,std::size_t count){if(at<0x10000 || at-0x10000>bytes.size() || count>bytes.size()-(at-0x10000))return false;std::memcpy(out,bytes.data()+at-0x10000,count);return true;}
    Fixture(){
        put(0x10008,std::uint8_t{1});put(0x1000c,std::uint32_t{5});put(0x10010,std::uint32_t{6});put(0x10018,std::uint64_t{0xabcdef});
        put(0x11028,std::uint32_t{0xffffffff});put(0x11000+0x3c8+0x28,std::uint32_t{123});
        api.thread_id=[&]{return thread;};api.fast_index=[&]{return index;};api.is_fiber=[&]{return fiber;};
        api.fls_value=[&](std::uint32_t slot)->std::optional<std::uint64_t>{++calls;if(slot==5)return 0x11000;if(slot==6)return index;return {};};
    }
    auto run(){return ffr::capture_crt_rand([&](auto a,auto b,auto c){return read(a,b,c);},layout,api,42);}
};
int main(){try{
    {Fixture f;auto r=f.run();CHECK(r && r->seed==0xffffffffu && r->state_index==0 && f.calls==4);auto text=ffr::crt_rand_json(*r,std::string(64,'a'));CHECK(text.find("4294967295")!=std::string::npos && text.find("\"restorable\":false")!=std::string::npos && text.find("69632")==std::string::npos);}
    for(unsigned failure=0;failure<8;++failure){Fixture f;
        if(failure==0)f.thread=7;
        if(failure==1)f.put(0x10008,std::uint8_t{0});
        if(failure==2)f.put(0x10009,std::uint8_t{2});
        if(failure==3)f.put(0x1000c,std::uint32_t{0xffffffff});
        if(failure==4)f.put(0x10010,std::uint32_t{5});
        if(failure==5)f.put(0x10018,std::uint64_t{1});
        if(failure==6)f.index=2;
        if(failure==7)f.api.fls_value=[](std::uint32_t)->std::optional<std::uint64_t>{return {};};
        CHECK(!f.run());
    }
    {Fixture f;f.put(0x10009,std::uint8_t{1});f.index=1;auto r=f.run();CHECK(r && r->seed==123 && r->state_index==1 && f.calls==2);}
    {Fixture f;f.put(0x10009,std::uint8_t{1});f.index=1;f.fiber=true;auto r=f.run();CHECK(r && r->state_index==1 && f.calls==4);}
    {Fixture f;auto get=f.api.fls_value;f.api.fls_value=[&](std::uint32_t slot){auto v=get(slot);if(f.calls==3)f.put(0x11028,std::uint32_t{2});return v;};CHECK(!f.run());}
    {Fixture f;f.api.fls_value=[](std::uint32_t slot)->std::optional<std::uint64_t>{return slot==5?std::uint64_t{0xfffffffffffffff0}:0;};CHECK(!f.run());}
    std::cout<<"Existing FLS read, supported state selection, stable identity/seed, wrong thread and bounds checks passed. No RNG calls/setters exist in the API.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
