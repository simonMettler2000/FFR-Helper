#pragma once
#include <cstdint>
#include <cstddef>
namespace ffr::crt_profile {
inline constexpr wchar_t module_name[]=L"ucrtbase.dll";
inline constexpr char module_sha256[]="dedb7611e1abc1ff3852dcac3d4c93a048d729e25b061973b9969f22698f14ca";
inline constexpr std::uintptr_t import_rva=0x7733140,rand_rva=0xdb40,helper_rva=0xc3f0;
inline constexpr std::size_t rand_size=41,helper_size=181;
inline constexpr char rand_sha256[]="cc36d5eac52782cb5e3e0b6c9d1b9d6ad9a17dcc1f3663b6ac498daebf7af611";
inline constexpr char helper_sha256[]="6bfaa7745a109f6d49f367858515c6ca902304c6e8122aed0ed10c8cbe894e71";
inline constexpr std::uintptr_t ready_rva=0x1397b0,fast_rva=0x13975c,data_slot_rva=0x1384e4,state_slot_rva=0x1384e0,getter_rva=0x149018;
}
