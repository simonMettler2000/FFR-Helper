#pragma once
#include <cstddef>
#include <cstdint>
namespace ffr::menu_profile {
inline constexpr std::uintptr_t tick_rva=0x4f66930;
inline constexpr std::size_t tick_size=530;
inline constexpr char tick_sha256[]="177939b581e8b2534646dc79cd587789c443761d9c883a57ff58c1632ed1f316";
inline constexpr std::uintptr_t get_world_rva=0x42fb240;
inline constexpr char get_world_sha256[]="50e2417c926282a47258ec07a5b76cc46b81d361d3bf51c4a3ecd56c5e869051";
inline constexpr std::uintptr_t get_map_rva=0x4fd0210;
inline constexpr char get_map_sha256[]="acecf6e53d26779ef7259aa8d7e1e475396eab7eddaaecf72e52797ab22f03e5";
inline constexpr std::uintptr_t get_ui_rva=0x4f9e330;
inline constexpr char get_ui_sha256[]="b753124279be3841fc59198fb8f5d547361a2807bd82e7bcfbd317acc27fc709";
inline constexpr std::uintptr_t manager_vtable_rva=0x8643ba0;
}
