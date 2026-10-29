#include "partial_recovery.hpp"
#include "clone_payload.hpp"
#include <iostream>
#include <stdexcept>
#define CHECK(x) do {if(!(x))throw std::runtime_error("Recovery check failed at "+std::to_string(__LINE__));}while(false)
struct Fixture {
    int root{},other{};void* current=&root;bool ready=true;
    unsigned serializations{},field_reads{},clock_reads{};
    std::vector<unsigned char> bytes=clone_fixture_payload();
    ffr::FieldAnchor field{1000,{10,20,30},2};
    ffr::PlaytimeBasis clock;
    ffr::PartialRecoveryAPI api;
    Fixture() {
        clock.saved_seconds=1000.25f;clock.start_seconds=9000;clock.sample_seconds=10000;
        clock.total_suspended_seconds=30;clock.elapsed_seconds=970;clock.rounding_mode=0;
        api.field_ready=[&]{return ready;};api.current_root=[&]{return current;};
        api.serialize_current=[&]()->std::optional<std::vector<unsigned char>> {++serializations;return bytes;};
        api.field=[&]()->std::optional<ffr::FieldAnchor>{++field_reads;return field;};
        api.clock=[&]()->std::optional<ffr::PlaytimeBasis>{++clock_reads;return clock;};
    }
    auto capture(){return ffr::capture_partial_recovery(api,1000);}
};
int main() {
    try {
        {
            Fixture f;auto copied=f.capture();CHECK(copied && f.serializations==2 && f.field_reads==3 && f.clock_reads==3);
            f.bytes[8]^=1;f.field.position[0]=99;f.clock.saved_seconds=0;
            CHECK(copied->progression==clone_fixture_payload() && copied->field.position[0]==10 && copied->clock.saved_seconds==1000.25f);
        }
        {
            Fixture f;f.ready=false;CHECK(!f.capture() && f.serializations==0);
        }
        {
            Fixture f;f.current=nullptr;CHECK(!f.capture() && f.serializations==0);
        }
        {
            Fixture f;f.field.map_id=1001;CHECK(!f.capture() && f.serializations==0);
        }
        for(unsigned mode=0;mode<6;++mode) {
            Fixture f;auto serialize=f.api.serialize_current;
            f.api.serialize_current=[&]()->std::optional<std::vector<unsigned char>> {
                auto data=serialize();
                if(mode==0)f.current=&f.other;
                if(mode==1)f.ready=false;
                if(mode==2)f.clock.start_seconds+=1;
                if(mode==3)f.field.position[0]+=1;
                if(mode==4)f.clock.rounding_mode=1;
                if(mode==5)f.bytes[8]^=1;
                return data;
            };
            CHECK(!f.capture());
        }
        {
            Fixture f;f.api.clock=[&]()->std::optional<ffr::PlaytimeBasis> {
                auto value=f.clock;value.sample_seconds+=f.clock_reads;value.elapsed_seconds+=f.clock_reads++;
                return value;
            };auto copied=f.capture();CHECK(copied && copied->clock.sample_seconds==10002 && copied->clock.elapsed_seconds==972);
        }
        for(auto offset:{-1,3}) {
            Fixture f;f.api.clock=[&]()->std::optional<ffr::PlaytimeBasis> {
                auto value=f.clock;if(f.clock_reads++){value.sample_seconds+=offset;value.elapsed_seconds+=offset;}return value;
            };CHECK(!f.capture());
        }
        {
            Fixture f;f.bytes[0]='X';CHECK(!f.capture());
        }
        {
            Fixture f;f.clock.suspended_start_seconds=9900;CHECK(f.capture()); // Evidence is retained; load planning rejects unsupported suspension separately.
        }
        std::cout<<"Detached partial recovery, two matching serializations and stable field/clock/root checks passed.\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
