#include "engine_rng_capture.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error("Global RNG capture failed at "+std::to_string(__LINE__));}while(false)
int main(){try{
    unsigned calls{};std::uint32_t value{};
    ffr::Reader read=[&](std::uint64_t address,void* out,std::size_t size){
        ++calls;if(address!=0x11000 || size!=4)return false;std::memcpy(out,&value,4);return true;
    };
    for(auto seed:{0u,1u,0xffffffffu}){value=seed;calls=0;auto sample=ffr::capture_engine_global_seed(read,0x10000,0x1000);CHECK(sample && *sample==seed && calls==2);}
    for(auto basis:{std::pair<std::uint64_t,std::uint64_t>{0,0x1000},{0x10000,0},{0x10000,0x1001},{0x10000,0x1004},{0xfffffffffffffff0,0x20},{0xfffffffffffffff0,12}})
        CHECK(!ffr::capture_engine_global_seed(read,basis.first,basis.second));
    ffr::Reader changing=[&](std::uint64_t,void* out,std::size_t){std::memcpy(out,&value,4);++value;return true;};
    CHECK(!ffr::capture_engine_global_seed(changing,0x10000,0x1000));
    const auto json=ffr::engine_global_rng_json(0xffffffffu);
    CHECK(json.find("4294967295")!=std::string::npos && json.find("\"atomicSnapshot\":false")!=std::string::npos
        && json.find("\"restorable\":false")!=std::string::npos && json.find("address")==std::string::npos);
    std::cout<<"Bounded stable global seed reads preserve unsigned values and refuse drift. No draw/setter callback exists.\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
