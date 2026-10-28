#include "progression_rebind.hpp"
#include "clone_payload.hpp"
#include <iostream>
#include <set>
#include <stdexcept>
#define CHECK(x) do {if(!(x))throw std::runtime_error("Rebind check failed at "+std::to_string(__LINE__));}while(false)
struct Fixture {
    int original{},clone{},other{};
    void* current=&original;
    unsigned applications{},imports{},removes{};
    std::set<void*> roots;
    std::vector<unsigned char> before=clone_fixture_payload(),target=before,cloned;
    ffr::ProgressionRebindAPI api;
    ffr::ProgressionRebindController controller;
    Fixture() {
        target[8]^=1;cloned=target;
        api.objects.ready=[] {return true;};api.objects.current_root=[&]{return current;};
        api.objects.same_save_class=[&](void* a,void* b){return (a==&original||a==&clone) && b==&original;};
        api.objects.is_rooted=[&](void* p){return roots.count(p)!=0;};
        api.objects.add_root=[&](void* p){CHECK(roots.insert(p).second);};
        api.objects.remove_root=[&](void* p){++removes;CHECK(roots.erase(p)==1);};
        api.objects.load_memory=[&](const auto* data)->void* {
            ++imports;CHECK(roots.count(&original));CHECK(data->count==static_cast<int>(target.size()));return &clone;
        };
        api.objects.save_memory=[&](void* p)->std::optional<std::vector<unsigned char>> {
            CHECK(roots.count(p));return p==&original?before:cloned;
        };
        api.native_load=[&](void* p){++applications;CHECK(p==&clone && roots.count(&original) && roots.count(&clone));current=p;return true;};
    }
    ffr::ProgressionRebindResult run(bool durable=true){return controller.run(target,before,durable,api);}
};
int main() {
    try {
        {
            Fixture f;auto result=f.run();CHECK(result.passed && result.verified && result.native_attempted && result.native_returned);
            CHECK(result.clone_matches && result.recovery_matches && result.input_unchanged && result.root_matches && result.bytes_match);
            CHECK(result.pins_released && result.retained_owned_pins==0 && f.roots.empty() && f.current==&f.clone && f.applications==1);
            CHECK(!f.run().native_attempted && f.applications==1 && f.imports==1);
        }
        {
            Fixture f;CHECK(!f.run(false).native_attempted && f.imports==0 && f.roots.empty());
        }
        {
            Fixture f;f.api.objects.ready=[] {return false;};CHECK(!f.run().native_attempted && f.imports==0);
        }
        {
            Fixture f;auto wrong=f.before;wrong[8]^=2;
            auto result=f.controller.run(f.target,wrong,true,f.api);CHECK(!result.native_attempted && !result.recovery_matches && f.imports==0 && f.roots.empty());
        }
        {
            Fixture f;f.cloned[8]^=2;auto result=f.run();CHECK(!result.native_attempted && !result.clone_matches && f.roots.empty());
        }
        {
            Fixture f;auto load=f.api.objects.load_memory;f.api.objects.load_memory=[&](const ffr::RawArray* data)->void*{load(data);reinterpret_cast<unsigned char*>(data->data)[8]^=1;return &f.clone;};
            CHECK(!f.run().native_attempted && f.applications==0 && f.roots.empty());
        }
        for(unsigned mode=0;mode<5;++mode) {
            Fixture f;
            f.api.native_load=[&](void*)->bool {
                ++f.applications;
                if(mode==0)throw std::runtime_error("uncertain native callback");
                f.current=mode==1?static_cast<void*>(&f.other):static_cast<void*>(&f.clone);
                if(mode==2)f.cloned[8]^=2;
                if(mode==3)f.api.objects.ready=[] {return false;};
                return mode!=4;
            };
            auto result=f.run();CHECK(result.native_attempted && !result.verified && !result.passed && result.retained_owned_pins==2);
            CHECK(!result.pins_released && f.roots.size()==2 && f.removes==0 && f.applications==1);
            CHECK(!f.run().native_attempted && f.applications==1);
        }
        {
            Fixture f;f.roots.insert(&f.original);f.roots.insert(&f.clone);CHECK(f.run().passed && f.roots.size()==2 && f.removes==0);
        }
        {
            Fixture f;f.api.objects.remove_root=[&](void*){++f.removes;};auto result=f.run();
            CHECK(result.verified && result.native_attempted && !result.passed && !result.pins_released && f.removes==2);
        }
        {
            Fixture f;auto load=f.api.objects.load_memory;f.api.objects.load_memory=[&](const auto* data)->void*{load(data);f.current=&f.other;return &f.clone;};
            auto result=f.run();CHECK(!result.native_attempted && result.root_changed_before_apply && result.retained_owned_pins==1 && f.applications==0);
        }
        std::cout<<"Prepared recovery, detached target verification and one-shot progression rebind callback checks passed.\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
