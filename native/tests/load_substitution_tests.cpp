#include "load_substitution.hpp"
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#define CHECK(x) do {if(!(x))throw std::runtime_error("Load route check failed at "+std::to_string(__LINE__));}while(false)
int main() {
    try {
        const ffr::LoadSubstitutionConfig config{0x10000,0x20000,0x30000,0x30010,7};
        const ffr::LoadCall valid{config.manager,config.validity_caller,7,ffr::memory_load_private_slot};
        const ffr::LoadCall read{config.manager,config.reader_caller,7,ffr::memory_load_private_slot};
        {
            ffr::ScopedLoadSubstitution scope;CHECK(!scope.validity(valid));CHECK(scope.arm(config));
            for(unsigned mismatch=0;mismatch<4;++mismatch) {
                auto changed=valid;
                if(mismatch==0)changed.manager++;if(mismatch==1)changed.thread++;if(mismatch==2)changed.slot=0;if(mismatch==3)changed.caller++;
                CHECK(!scope.validity(changed));
            }
            CHECK(scope.view().validations==0);
            const auto accepted=scope.validity(valid);CHECK(accepted && *accepted);
            const auto supplied=scope.reader(read,true);CHECK(supplied && supplied->root==config.target_root && supplied->status==0);
            CHECK(scope.view().validations==1 && scope.view().readers==1 && scope.close());
            CHECK(scope.close());CHECK(!scope.reader(read,true));CHECK(!scope.validity(valid));CHECK(!scope.arm(config));
        }
        {
            ffr::ScopedLoadSubstitution scope;CHECK(scope.arm(config));
            const auto refused=scope.reader(read,true);CHECK(refused && refused->root==0 && refused->status==6);
            const auto valid_after=scope.validity(valid);CHECK(valid_after && !*valid_after);CHECK(!scope.close());
        }
        {
            ffr::ScopedLoadSubstitution scope;CHECK(scope.arm(config));CHECK(scope.validity(valid).value());
            CHECK(!scope.validity(valid).value());CHECK(scope.reader(read,true)->root==0);CHECK(!scope.close());
        }
        {
            ffr::ScopedLoadSubstitution scope;CHECK(scope.arm(config));CHECK(scope.validity(valid).value());
            CHECK(scope.reader(read,false)->root==0);CHECK(!scope.close());
        }
        {
            ffr::ScopedLoadSubstitution scope;CHECK(scope.arm(config));CHECK(scope.validity(valid).value());
            CHECK(scope.reader(read,true)->root==config.target_root);CHECK(scope.reader(read,true)->root==0);CHECK(!scope.close());
        }
        {
            ffr::ScopedLoadSubstitution scope;auto bad=config;bad.target_root=0;CHECK(!scope.arm(bad));CHECK(!scope.arm(config));
        }
        {
            ffr::ScopedLoadSubstitution scope;CHECK(scope.arm(config));std::atomic<unsigned> accidental{};
            auto worker=[&]{auto other=valid;other.thread=8;for(unsigned i=0;i<10000;++i)if(scope.validity(other))++accidental;};
            std::thread a(worker),b(worker);CHECK(scope.validity(valid).value());CHECK(scope.reader(read,true)->root==config.target_root);
            CHECK(scope.close());a.join();b.join();CHECK(accidental==0);
        }
        std::cout<<"One-shot native load substitution, caller/thread isolation and closure checks passed.\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
