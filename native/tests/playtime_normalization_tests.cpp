#include "playtime_normalization.hpp"
#include <cfenv>
#include <iostream>
#include <stdexcept>
#define CHECK(x) do {if(!(x))throw std::runtime_error("Normalization check failed at "+std::to_string(__LINE__));}while(false)
ffr::PlaytimeBasis basis(float saved,std::int64_t elapsed) {
    ffr::PlaytimeBasis value;value.saved_seconds=saved;value.start_seconds=1000;value.sample_seconds=1000+elapsed;
    value.elapsed_seconds=elapsed;value.rounding_mode=0;return value;
}
int main() {
    try {
        CHECK(std::fesetround(FE_TONEAREST)==0);
        for(const auto& sample:{std::array<double,3>{1000.25,970,1970.25},{16777216,1,16777216},
                               {16777218,1,16777220},{1,16777217,16777216},{1,16777219,16777220}}) {
            auto value=basis(static_cast<float>(sample[0]),static_cast<std::int64_t>(sample[1]));
            auto result=ffr::plan_playtime_normalization(value);CHECK(result && result->saved_after_normal_load==sample[2]);
        }
        // Compare the explicit bit algorithm with two distinct native binary32
        // stages, including int64 values that cannot be represented exactly.
        std::uint64_t seed=71;
        for(unsigned i=0;i<2000;++i) {
            seed=seed*6364136223846793005ULL+1;const auto elapsed=static_cast<std::int64_t>((seed>>12)%1000000000);
            const float saved=static_cast<float>((seed>>33)%1000000000);
            volatile float converted=static_cast<float>(elapsed);volatile float added=saved+converted;
            auto result=ffr::plan_playtime_normalization(basis(saved,elapsed));
            CHECK(result && std::bit_cast<std::uint32_t>(result->saved_after_normal_load)==std::bit_cast<std::uint32_t>(static_cast<float>(added)));
        }
        for(std::uint8_t mode:{1,2,3,255}) {auto value=basis(1000,10);value.rounding_mode=mode;CHECK(!ffr::plan_playtime_normalization(value));}
        auto paused=basis(1000,100);paused.suspended_start_seconds=1050;CHECK(!ffr::plan_playtime_normalization(paused));
        CHECK(!ffr::plan_playtime_normalization(basis(std::numeric_limits<float>::denorm_min(),0)));
        CHECK(!ffr::plan_playtime_normalization(basis(600000000000.0f,500000000000LL)));
        CHECK(!ffr::playtime_nearest_float32(-1));CHECK(!ffr::playtime_nearest_float32(std::numeric_limits<double>::infinity()));
        CHECK(ffr::plan_playtime_normalization(basis(0,0))->saved_after_normal_load==0);
        std::cout<<"Explicit binary32 conversion/add planning, tie cases and unsupported-clock refusal passed without game writes.\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
