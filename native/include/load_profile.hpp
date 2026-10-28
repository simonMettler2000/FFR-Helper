#pragma once
#include "native_load_binding.hpp"
#include "build_profile.hpp"
namespace ffr {
inline NativeLoadProfile supported_native_load_profile() {
    return {profile::game_sha256,
        {0x5123ab0,505,"46b522e05e11b1759e5a4772f32810ddbfec2f7b4b179e4a7c1e67f3d8ad0f30"},
        {0x5123680,198,"e730ff8576c0cc9bac6dc2af78c64816e68cd5da0f062b93a911d4e1fafdff2b"},
        {0x5123cb0,205,"b30f87666d3011015cd8833b4c46e6ac17fae40ff722feecbedfd398ad9a1677"},
        0x5123ac7,0x5123b31,0x8801530,0x87fed08,
        {{0x44ac040,3,"b6cbececf1b07e74f37d57710c68c31f0b92cb08d51093252b321b07542e177b"}}};
}
} // namespace ffr
