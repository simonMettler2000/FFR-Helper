#pragma once
#include <cstddef>
#include <cstdint>
namespace ffr::decision_profile {
// Native target of CPP_BtlDelEventCmd::OnExecuteDecideCommand's reflected thunk.
// Entire chained-unwind function envelope; game identity is checked separately.
// Handler entries are observations, not verified completed player commands.
inline constexpr std::uintptr_t handler_rva=0x4f5cd60;
inline constexpr std::size_t handler_size=2314;
inline constexpr char handler_sha256[]="a4b7302855baaf985f5eaa4cf0d854df80327241f15be1a9b56cea944df6f20c";
}
