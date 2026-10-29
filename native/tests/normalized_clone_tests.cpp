#include "normalized_clone.hpp"
#include "recovery_payload.hpp"
#include <iostream>
#include <set>
#include <stdexcept>
#define CHECK(x) do {if(!(x))throw std::runtime_error("Normalized clone check failed at "+std::to_string(__LINE__));}while(false)
struct Fixture {
    std::array<unsigned char,0x40> original{},clone{},other{};
    void* current=original.data();
    std::set<void*> roots;
    unsigned imports{},writes{},releases{};
    std::int32_t original_money=303,clone_money=101;
    ffr::PlaytimeBasis basis;
    std::vector<unsigned char> source;
    ffr::NormalizedCloneAPI api;
    ffr::NormalizedCloneProbe probe;
    void set(void* object,float value){std::memcpy(static_cast<unsigned char*>(object)+0x38,&value,4);}
    float get(void* object){float value{};std::memcpy(&value,static_cast<unsigned char*>(object)+0x38,4);return value;}
    Fixture() {
        basis.saved_seconds=1000.25f;basis.start_seconds=9000;basis.sample_seconds=10000;basis.total_suspended_seconds=30;basis.elapsed_seconds=970;basis.rounding_mode=0;
        set(original.data(),2000.5f);set(clone.data(),basis.saved_seconds);source=recovery_fixture_data::payload(basis.saved_seconds,101);
        api.objects.ready=[] {return true;};api.objects.current_root=[&]{return current;};
        api.objects.same_save_class=[&](void* a,void* b){return (a==original.data() || a==clone.data()) && b==original.data();};
        api.objects.is_rooted=[&](void* p){return roots.count(p)!=0;};api.objects.add_root=[&](void* p){CHECK(roots.insert(p).second);};
        api.objects.remove_root=[&](void* p){++releases;CHECK(roots.erase(p)==1);};
        api.objects.save_memory=[&](void* p)->std::optional<std::vector<unsigned char>> {
            CHECK(roots.count(p));return recovery_fixture_data::payload(get(p),p==original.data()?original_money:clone_money);
        };
        api.objects.load_memory=[&](const ffr::RawArray* input)->void* {
            ++imports;CHECK(roots.count(original.data()));CHECK(input->count==static_cast<int>(source.size()));
            CHECK(std::memcmp(reinterpret_cast<void*>(input->data),source.data(),source.size())==0);return clone.data();
        };
        api.write_owned_time=[&](void* p,float value) {
            ++writes;CHECK(p==clone.data() && p!=current && roots.count(p) && roots.count(original.data()));set(p,value);return true;
        };
    }
    auto run(){return probe.run(source,basis,api);}
};
int main() {
    try {
        {
            Fixture f;auto before=f.original;const auto source=f.source;auto result=f.run();
            CHECK(result.passed && result.write_attempted && result.write_returned && result.normalized_seconds==1970.25f);
            CHECK(result.clone_before_matches && result.clone_after_matches && result.live_unchanged && result.pins_released);
            CHECK(f.original==before && f.source==source && f.roots.empty() && f.current==f.original.data() && f.writes==1);
            CHECK(!f.run().passed && f.imports==1 && f.writes==1);
        }
        for(unsigned mode=0;mode<6;++mode) {
            Fixture f;
            if(mode==0)f.basis.rounding_mode=255;
            if(mode==1)f.basis.suspended_start_seconds=9900;
            if(mode==2)f.source[0]='X';
            if(mode==3)f.basis.saved_seconds+=1;
            if(mode==4)f.api.objects.ready=[] {return false;};
            if(mode==5)f.api.objects.same_save_class=[](void*,void*){return false;};
            auto result=f.run();CHECK(!result.passed && f.imports==0 && f.writes==0 && f.roots.empty());
        }
        {
            Fixture f;f.clone_money=999;auto result=f.run();CHECK(!result.passed && !result.clone_before_matches && result.live_unchanged && f.writes==0 && f.roots.empty());
        }
        {
            Fixture f;auto load=f.api.objects.load_memory;f.api.objects.load_memory=[&](const ffr::RawArray* input)->void* {
                auto* result=load(input);reinterpret_cast<unsigned char*>(input->data)[8]^=1;return result;
            };
            auto result=f.run();CHECK(!result.passed && !result.input_unchanged && result.live_unchanged && f.writes==0 && f.roots.empty());
        }
        {
            Fixture f;f.basis.elapsed_seconds=0;f.basis.sample_seconds=f.basis.start_seconds+f.basis.total_suspended_seconds;
            auto result=f.run();CHECK(result.passed && !result.write_attempted && f.writes==0 && f.roots.empty());
        }
        {
            Fixture f;f.roots.insert(f.original.data());f.roots.insert(f.clone.data());
            auto result=f.run();CHECK(result.passed && f.releases==0 && f.roots.size()==2);
        }
        {
            Fixture f;f.api.write_owned_time=[&](void* p,float value){++f.writes;f.set(p,value+1);return true;};
            auto result=f.run();CHECK(!result.passed && !result.clone_after_matches && result.live_unchanged && f.writes==1 && f.roots.empty());
        }
        {
            Fixture f;auto write=f.api.write_owned_time;f.api.write_owned_time=[&](void* p,float value){write(p,value);return false;};
            auto result=f.run();CHECK(!result.passed && result.clone_after_matches && !result.write_returned && result.live_unchanged && f.writes==1);
        }
        {
            Fixture f;f.api.write_owned_time=[&](void* p,float value){++f.writes;f.set(p,value);f.set(f.original.data(),9);return true;};
            auto result=f.run();CHECK(!result.passed && !result.live_unchanged && f.roots.empty());
        }
        {
            Fixture f;auto load=f.api.objects.load_memory;f.api.objects.load_memory=[&](const ffr::RawArray* input)->void*{auto* p=load(input);f.current=p;return p;};
            auto result=f.run();CHECK(!result.passed && result.root_changed && result.retained_owned_pins==2 && f.roots.size()==2 && f.writes==0 && f.releases==0);
        }
        {
            Fixture f;auto save=f.api.objects.save_memory;unsigned reads=0;
            f.api.objects.save_memory=[&](void* p){auto value=save(p);if(p==f.original.data() && ++reads==3)f.current=f.clone.data();return value;};
            auto result=f.run();CHECK(!result.passed && result.root_changed && !result.live_unchanged && result.retained_owned_pins==2 && f.releases==0);
        }
        {
            Fixture f;auto write=f.api.write_owned_time;f.api.write_owned_time=[&](void* p,float value){write(p,value);throw std::runtime_error("write exception");return false;};
            auto result=f.run();CHECK(!result.passed && result.live_unchanged && f.roots.empty() && f.writes==1);
        }
        {
            Fixture f;f.api.objects.remove_root=[&](void*){++f.releases;};
            auto result=f.run();CHECK(!result.passed && !result.pins_released && f.writes==1 && f.releases==2);
        }
        std::cout<<"Detached normalized clone, fixed-member write, original preservation, failure cleanup and no-retry checks passed.\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
