#pragma once
#include <cstddef>
#include <cstdint>
namespace ffr::engine_rng_profile {
// Corroborated read/write RIP targets in both complete function bodies. These
// addresses are for fingerprinting/reading only; neither function is invoked.
inline constexpr std::uintptr_t seed_rva=0x9c5f734,draw_rva=0x1208210,initialize_rva=0x1208260;
inline constexpr std::size_t draw_size=76,initialize_size=7;
inline constexpr char draw_sha256[]="9f198484198ff78b417416dbe99b89024f519c934d02c750019e01e018703352";
inline constexpr char initialize_sha256[]="1959ee00fe7a607e9b8d7c3b3429966059a018587a5d8c169fba2c5494f7be66";
}
