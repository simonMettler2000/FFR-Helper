#include "progression_scalar.hpp"
#include "recovery_payload.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#define CHECK(x) do {if(!(x))throw std::runtime_error("Scalar check failed at "+std::to_string(__LINE__));}while(false)
int main(int argc,char** argv) {
    try {
        if(argc==2) {
            std::ifstream file(argv[1],std::ios::binary);const std::vector<unsigned char> bytes(std::istreambuf_iterator<char>(file),{});
            auto value=ffr::locate_saved_playtime(bytes);CHECK(value);
            std::cout<<"{\"offset\":"<<value->offset<<",\"bits\":"<<value->bits<<"}\n";return 0;
        }
        auto source=recovery_fixture_data::payload(1000.25f,101);auto scalar=ffr::locate_saved_playtime(source);
        CHECK(scalar && scalar->bits==std::bit_cast<std::uint32_t>(1000.25f));
        ffr::PlaytimeBasis basis;basis.saved_seconds=1000.25f;basis.start_seconds=9000;basis.sample_seconds=10000;
        basis.total_suspended_seconds=30;basis.elapsed_seconds=970;basis.rounding_mode=0;
        auto plan=ffr::prepare_normalized_progression(source,basis);CHECK(plan && plan->clock.saved_after_normal_load==1970.25f);
        CHECK(ffr::locate_saved_playtime(source)->bits==std::bit_cast<std::uint32_t>(1000.25f));
        CHECK(ffr::locate_saved_playtime(plan->bytes)->bits==std::bit_cast<std::uint32_t>(1970.25f));
        auto restored=plan->bytes;std::memcpy(restored.data()+scalar->offset,&scalar->bits,4);CHECK(restored==source);
        for(std::size_t length:std::array<std::size_t,4>{0,47,100,source.size()-1})CHECK(!ffr::locate_saved_playtime(std::span(source).first(length)));
        auto broken=source;broken[0]='X';CHECK(!ffr::locate_saved_playtime(broken));
        // A PlayTime-looking byte sequence in another scalar is never selected.
        auto missing=source;
        const std::string label="PlayTime";
        for(std::size_t i=0;i+label.size()<=missing.size();++i)if(std::memcmp(missing.data()+i,label.data(),label.size())==0){missing[i]='X';break;}
        CHECK(!ffr::locate_saved_playtime(missing));
        auto case_changed=source;
        for(std::size_t i=0;i+label.size()<=case_changed.size();++i)if(std::memcmp(case_changed.data()+i,label.data(),label.size())==0){case_changed[i]='p';break;}
        CHECK(!ffr::locate_saved_playtime(case_changed));
        auto wrong_type=source;const std::string float_type="FloatProperty",other_type="OtherProperty";
        for(std::size_t i=0;i+float_type.size()<=wrong_type.size();++i)if(std::memcmp(wrong_type.data()+i,float_type.data(),float_type.size())==0){std::memcpy(wrong_type.data()+i,other_type.data(),other_type.size());break;}
        CHECK(!ffr::locate_saved_playtime(wrong_type));
        const auto body=*ffr::captured_progression_body(source);
        ffr::scalar_detail::Tags reader(source);const auto player=*reader.unique(body,source.size()-4,"Player");
        const std::vector<unsigned char> player_tag(source.begin()+body,source.begin()+player.offset+player.size);
        auto duplicate=source;duplicate.insert(duplicate.begin()+body,player_tag.begin(),player_tag.end());
        CHECK(!ffr::locate_saved_playtime(duplicate));
        auto untagged=source;untagged[player.offset-1]=8;CHECK(!ffr::locate_saved_playtime(untagged));
        basis.saved_seconds=1001.25f;CHECK(!ffr::prepare_normalized_progression(source,basis));
        std::cout<<"Bounded typed GVAS scalar lookup and exact detached byte normalization passed.\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
