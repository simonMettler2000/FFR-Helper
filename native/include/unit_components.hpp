#pragma once
#include <cstdint>
#include <utility>
namespace ffr {
// Inherited CPP_UnitParameter raw components. These are not final UI values.
inline constexpr std::pair<const char*,std::uint32_t> basic_unit_fields[]{
    {"unitId",0xd0},{"baseLevel",0x100},{"hpBase",0x10c},{"mp",0x110},{"stagger",0x114},{"limitBurst",0x118},
    {"maxHp",0x120},{"maxMp",0x124},{"maxStagger",0x128},{"attack",0x12c},{"defence",0x130},
    {"intelligence",0x134},{"mind",0x138},{"divine",0x13c},{"agility",0x140},
    {"weaponId",0x1e8},{"armorId",0x1ec},{"visionId",0x200},{"temporaryHp",0x268}};
}
