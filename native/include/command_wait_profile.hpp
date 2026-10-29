#pragma once
#include <cstddef>
#include <cstdint>
namespace ffr::command_wait_profile {
inline constexpr std::uintptr_t gate_rva=0x4f59c00;
inline constexpr std::size_t gate_size=41;
inline constexpr char gate_sha256[]="6eba9ab441a4e58b2b28abd5274fc14a56237b206a24c58c7955cd386b9f232c";
inline constexpr std::uintptr_t state_rva=0x4faace0;
inline constexpr std::size_t state_size=889;
inline constexpr char state_sha256[]="93a5ef0ace960ac5a1e92d99747e34713fac2f4fff1a597c0d36de50b9d1eba7";
inline constexpr std::uintptr_t callsite_rva=0x4faaee4;
inline constexpr std::uintptr_t helper_rva=0x4fc58d0;
inline constexpr std::size_t helper_size=583;
inline constexpr char helper_sha256[]="56ddfdcc22afb07493cd40581598ba4b3c4657fddf98d21071823900fdf297ae";
inline constexpr std::uintptr_t helper_callsite_rva=0x4fc5ad0;
}
