#pragma once
#include <cstdint>
#include <cstddef>
namespace ffr::abort_profile {
// Live testing showed +0x4dc increments at Turn.Begin, before command entry.
// Supervised normal-menu/cancel/action samples verified both event journals.
// Abort also requires a fresh ordinary idle menu and untouched same-thread
// histories. This permits the controlled ForceEnd test, not restoration.
inline constexpr bool opening_boundary_reviewed=true;
// Independently reviewed against source31d's supervised idle-menu samples.
// This only permits detached clone comparison; it does not apply live state.
inline constexpr bool detached_import_menu_reviewed=true;
inline constexpr bool normalization_field_reviewed=false;
inline constexpr std::uintptr_t tick_rva=0x4f66930;
inline constexpr char tick_sha256[]="aea50882c7c5ebfbfdbfdc67acb027058f860552ae780d662efb44395f4350d7";
inline constexpr std::uintptr_t get_world_rva=0x42fb240;
inline constexpr char get_world_sha256[]="50e2417c926282a47258ec07a5b76cc46b81d361d3bf51c4a3ecd56c5e869051";
inline constexpr std::uintptr_t get_map_rva=0x4fd0210;
inline constexpr char get_map_sha256[]="acecf6e53d26779ef7259aa8d7e1e475396eab7eddaaecf72e52797ab22f03e5";
inline constexpr std::uintptr_t get_ui_rva=0x4f9e330;
inline constexpr char get_ui_sha256[]="b753124279be3841fc59198fb8f5d547361a2807bd82e7bcfbd317acc27fc709";
inline constexpr std::uintptr_t end_battle_rva=0x4f4a520;
inline constexpr char end_battle_sha256[]="024d0434363d426923fa8b711ba2d890c085b8b82347780bedf942b99f80b640";
inline constexpr std::size_t end_battle_full_size=112;
inline constexpr char end_battle_full_sha256[]="87ec212fc06b0a58329648d10c80a9febe058f3b707ad8e64178d8ae5de01c28";
inline constexpr std::uintptr_t manager_vtable_rva=0x8643ba0;
}
