#include "encounter_rebuild.hpp"
#include <cstdlib>
#include <iostream>
#include <map>

#define CHECK(x) do { if (!(x)) throw std::runtime_error("Rebuild check failed at " + std::to_string(__LINE__)); } while (false)
int main() {
    try {
        ffr::Encounter source;
        source.friend_ids={1001,1002};source.enemy_ids={11,22};source.friend_locations={0,1};source.enemy_locations={2,3};
        source.passives={{77,88},{99},{}};source.bgm="Example \xf0\x9f\x98\x83";
        source.group=123;source.stage=456;source.event=-1;source.ap=28;source.region=7;
        source.escape_probability=50;source.can_escape=true;source.notify_preemption=true;
        std::map<void*,std::size_t> allocations;
        std::size_t calls=0, fail_after=1000, released=0;
        auto allocate=[&](std::size_t bytes) -> void* {
            if (++calls>fail_after) return nullptr;
            auto* p=std::malloc(bytes);if(p)allocations[p]=bytes;return p;
        };
        auto release=[&](void* p) noexcept { if(allocations.erase(p)) {std::free(p);++released;} else std::abort(); };
        ffr::Reader reader=[&](auto address,auto destination,auto length) {
            std::memcpy(destination,reinterpret_cast<void*>(address),length);return true;
        };
        for (std::size_t failing=0; failing<8; ++failing) {
            calls=0;fail_after=failing;
            try { ffr::EngineEncounter data(source,allocate,release); CHECK(false); } catch(const std::bad_alloc&) {}
            CHECK(allocations.empty());
        }
        calls=0;fail_after=1000;
        {
            ffr::EngineEncounter data(source,allocate,release);
            auto copied=ffr::capture_entry(reader,reinterpret_cast<std::uint64_t>(&data.view()),true);
            CHECK(copied && ffr::to_json(*copied)==ffr::to_json([&] {auto expected=source;expected.skip_intro=true;return expected;}()));
            CHECK(data.view().friend_ids.data!=reinterpret_cast<std::uint64_t>(source.friend_ids.data()));
            source.friend_ids[0]=9999;
            CHECK((*reinterpret_cast<const std::int32_t*>(data.view().friend_ids.data))==1001);
            auto raw=data.transfer();
            CHECK(!allocations.empty());
            bool denied=false;try {data.transfer();}catch(const std::logic_error&){denied=true;}CHECK(denied);
            // Mirror the game's consuming destructor, using the paired allocator.
            auto nested=reinterpret_cast<ffr::RawArray*>(raw.passives.data);
            for(int i=0;i<raw.passives.count;++i) if(nested[i].data)release(reinterpret_cast<void*>(nested[i].data));
            for(auto array : {raw.friend_ids,raw.friend_locations,raw.enemy_ids,raw.enemy_locations,raw.passives,raw.bgm})
                if(array.data)release(reinterpret_cast<void*>(array.data));
        }
        CHECK(allocations.empty());
        for(const auto& bad : {std::string("\xc0\x80"),std::string("\xed\xa0\x80"),std::string("\xf4\x90\x80\x80"),std::string("\xe2\x82"),std::string("x\0y",3),std::string(256,'x')}) {
            source.bgm=bad;bool denied=false;
            try {ffr::EngineEncounter data(source,allocate,release);} catch(const std::invalid_argument&) {denied=true;}
            CHECK(denied && allocations.empty());
        }
        std::cout<<"Engine-allocator reconstruction, detached data, Unicode and failure cleanup passed.\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
