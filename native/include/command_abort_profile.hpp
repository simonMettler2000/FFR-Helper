#pragma once
#include <cstddef>
#include <cstdint>
namespace ffr::command_abort_profile {
inline constexpr std::uintptr_t abort_rva=0x5136860;
inline constexpr std::size_t abort_size=69;
inline constexpr char abort_sha256[]="212b45f1e6025c9bfb139bd8d8b655c5a01d2412d4f86b1cd61ac524bb512de8";
inline constexpr std::uintptr_t getter_rva=0x5147ca0;
inline constexpr std::size_t getter_size=43;
inline constexpr char getter_sha256[]="ff8059065db8ff37953e3ea43a2ef411b202bffd95479ebb9547eb16346b208f";
inline constexpr std::uintptr_t state_a_rva=0x4fa9840;
inline constexpr std::size_t state_a_size=73;
inline constexpr char state_a_sha256[]="1f2b11b96ca8add32d6518a2254f58d02c386cd66ed07ec17d3925e96918e1a0";
inline constexpr std::uintptr_t state_b_rva=0x4faa170;
inline constexpr std::size_t state_b_size=80;
inline constexpr char state_b_sha256[]="0c0a478f54ccd3b8be530ba95262f4a750183e7a8d79a816591ac0087ea18f3f";
inline constexpr std::uintptr_t ui_vtable_rva=0x8864710;
}
