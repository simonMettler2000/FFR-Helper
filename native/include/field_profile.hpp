#pragma once
#include <cstdint>
namespace ffr::field_profile {
inline constexpr std::uintptr_t map_storage_vtable_rva=0x8619bf8;
inline constexpr std::uintptr_t get_world_rva=0x42fb240;
inline constexpr char get_world_sha256[]="50e2417c926282a47258ec07a5b76cc46b81d361d3bf51c4a3ecd56c5e869051";
inline constexpr std::uintptr_t get_map_rva=0x4fd0210;
inline constexpr char get_map_sha256[]="acecf6e53d26779ef7259aa8d7e1e475396eab7eddaaecf72e52797ab22f03e5";
inline constexpr std::uintptr_t get_player_rva=0x505a0e0;
inline constexpr char get_player_sha256[]="37aa8b9e824f4b825d965183dd450bde3b80e545c3335554d9c80443259d79d3";
inline constexpr std::uintptr_t get_map_id_rva=0x5055070;
inline constexpr char get_map_id_sha256[]="16c67dac444fc285d0268a63abed34710892bf6e55b2d948182b06dadc5d742c";
}
