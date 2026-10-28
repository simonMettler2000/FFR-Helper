#pragma once
#include <cstddef>
#include <cstdint>
namespace ffr::profile {
inline constexpr char game_sha256[] = "b8c588e46ebadbe37feca7bdf2da0ae63a8538099d14a53ec0c2241b086e5ffa";
inline constexpr char loader_sha256[] = "c273e7228687b647226fd63d7ca65888d313ece6be18996ec1320d00a5efe1f8";
inline constexpr char prologue_sha256[] = "d4e8f5b176619c2e749f80a4d641639ece116df817e7956ccf2744213bdbcd79";
inline constexpr std::uintptr_t entry_rva = 0x504ccb0;
inline constexpr std::size_t prologue_size = 64;
inline constexpr std::uintptr_t constructor_rva = 0x2e0720;
inline constexpr std::uintptr_t destructor_rva = 0x2e09a0;
inline constexpr std::size_t host_storage_size = 4096;
inline constexpr char constructor_name[] = "??0CppUserModBase@RC@@QEAA@XZ";
inline constexpr char destructor_name[] = "??1CppUserModBase@RC@@UEAA@XZ";
} // namespace ffr::profile
