#include "progression_clone.hpp"
#include "clone_payload.hpp"
#include <iostream>
#include <fstream>
#include <set>
#include <stdexcept>
#define CHECK(x) do {if(!(x))throw std::runtime_error("Clone check failed at "+std::to_string(__LINE__));}while(false)

struct Fixture {
    int root{},clone{};
    void* current=&root;
    unsigned loads{},adds{},removes{},saves{};
    std::set<void*> pins;
    std::vector<unsigned char> bytes=clone_fixture_payload();
    ffr::CloneProbeAPI api;
    Fixture() {
        api.ready=[] {return true;};api.current_root=[&] {return current;};
        api.same_save_class=[&](void* a,void* b){return (a==&root||a==&clone) && b==&root;};
        api.is_rooted=[&](void* value){return pins.count(value)!=0;};
        api.add_root=[&](void* value){++adds;pins.insert(value);};
        api.remove_root=[&](void* value){++removes;CHECK(pins.erase(value)==1);};
        api.load_memory=[&](const ffr::RawArray* data)->void* {
            ++loads;CHECK(pins.count(&root));
            CHECK(data->count==static_cast<int>(bytes.size()) && data->capacity==data->count);
            CHECK(std::memcmp(reinterpret_cast<void*>(data->data),bytes.data(),bytes.size())==0);return &clone;
        };
        api.save_memory=[&](void* value)->std::optional<std::vector<unsigned char>> {++saves;CHECK(pins.count(value));return bytes;};
    }
    ffr::CloneProbeResult run(){return ffr::probe_progression_clone(bytes,api);}
};
int main(int argc,char** argv) {
    try {
        if(argc==3 && std::string_view(argv[1])=="--captured-header") {
            std::ifstream file(argv[2],std::ios::binary);CHECK(file.good());
            std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),{});
            CHECK(ffr::captured_progression_header(bytes));
            std::cout<<"Private engine-produced capture passed the native header gate.\n";return 0;
        }
        CHECK(argc==1);
        {
            Fixture f;const auto result=f.run();CHECK(result.passed && result.clone_matches && result.live_unchanged && result.input_unchanged && result.pins_released);
            CHECK(result.clone==f.bytes && f.loads==1 && f.adds==2 && f.removes==2 && f.pins.empty() && f.current==&f.root && f.saves==3);
        }
        {
            Fixture f;f.pins.insert(&f.root);f.pins.insert(&f.clone);CHECK(f.run().passed);
            CHECK(f.adds==0 && f.removes==0 && f.pins.size()==2); // Existing roots remain owned by the engine.
        }
        for(unsigned byte:{0u,4u,12u,16u,18u,20u}) {Fixture f;f.bytes[byte]^=1;CHECK(!f.run().passed && f.loads==0 && f.adds==0);}
        {
            Fixture f;auto short_data=f.bytes;short_data.resize(30);CHECK(!ffr::captured_progression_header(short_data));
            f.bytes.back()=1;CHECK(!f.run().passed && f.loads==0);
        }
        {
            Fixture f;f.api.ready=[] {return false;};CHECK(!f.run().passed && f.loads==0 && f.adds==0);
        }
        {
            Fixture f;f.api.current_root=[]()->void* {return nullptr;};CHECK(!f.run().passed && f.adds==0);
        }
        for(unsigned mode=0;mode<3;++mode) {
            Fixture f;f.api.load_memory=[&](const auto*)->void*{++f.loads;return mode==0?nullptr:mode==1?&f.root:reinterpret_cast<void*>(0x1234);};
            auto result=f.run();CHECK(!result.passed && result.live_unchanged && f.pins.empty() && f.loads==1 && f.removes==1);
        }
        {
            Fixture f;f.api.add_root=[&](void* value){++f.adds;f.pins.insert(value);throw std::runtime_error("partial pin");};
            CHECK(!f.run().passed && f.loads==0 && f.pins.empty() && f.removes==1);
        }
        {
            Fixture f;f.api.add_root=[&](void*){++f.adds;};f.api.remove_root=[&](void*){++f.removes;};
            CHECK(!f.run().passed && f.loads==0 && f.removes==1);
        }
        {
            Fixture f;f.api.load_memory=[&](const ffr::RawArray*)->void*{++f.loads;throw std::runtime_error("load failed");};
            CHECK(!f.run().passed && f.loads==1 && f.pins.empty() && f.removes==1);
        }
        {
            Fixture f;auto original=f.api.load_memory;f.api.load_memory=[&](const ffr::RawArray* data)->void*{
                original(data);reinterpret_cast<unsigned char*>(data->data)[8]^=1;return &f.clone;
            };
            const auto saved=f.bytes;auto result=f.run();CHECK(!result.passed && !result.input_unchanged && result.live_unchanged && f.bytes==saved && f.pins.empty());
        }
        {
            Fixture f;f.api.save_memory=[&](void* value)->std::optional<std::vector<unsigned char>> {
                CHECK(f.pins.count(value));auto result=f.bytes;if(value==&f.clone)result[8]^=1;return result;
            };
            auto result=f.run();CHECK(!result.passed && !result.clone_matches && result.live_unchanged && f.pins.empty());
        }
        {
            Fixture f;auto original=f.api.load_memory;f.api.load_memory=[&](const ffr::RawArray* data)->void*{original(data);f.current=&f.clone;return &f.clone;};
            CHECK(!f.run().passed && f.pins.empty()); // Never attempt to overwrite an unexpected current root.
        }
        {
            Fixture f;f.api.save_memory=[&](void* value)->std::optional<std::vector<unsigned char>> {
                CHECK(f.pins.count(value));if(value==&f.clone)throw std::runtime_error("save failed");return f.bytes;
            };
            CHECK(!f.run().passed && f.pins.empty() && f.removes==2);
        }
        {
            Fixture f;f.api.remove_root=[&](void*){++f.removes;};auto result=f.run();
            CHECK(!result.passed && !result.pins_released && f.removes==2 && f.pins.size()==2); // Bounded retention, no uncertain retries.
        }
        std::cout<<"Detached native import, GC pin ownership and failure cleanup checks passed.\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
