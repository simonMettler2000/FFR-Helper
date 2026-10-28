#pragma once
#include <cstdint>
namespace ffr::import_profile {
inline constexpr std::uintptr_t load_memory_rva=0x3caf450;
inline constexpr char load_memory_sha256[]="3c79d075630d786adf4f26ff67821be6ca459f006f440a46333897c85739d125";
inline constexpr std::uintptr_t save_vtable_rva=0x87fed08;
inline constexpr char loader_ready_name[]="?IsInitialized@UnrealInitializer@Unreal@RC@@YAAEAU?$atomic@_N@std@@XZ";
inline constexpr std::uintptr_t loader_ready_rva=0x42b0c0;
inline constexpr char is_rooted_name[]="?IsRootSet@UObject@Unreal@RC@@QEAA_NXZ";
inline constexpr std::uintptr_t is_rooted_rva=0x3eb8e0;
inline constexpr char add_root_name[]="?SetRootSet@UObject@Unreal@RC@@QEAAXXZ";
inline constexpr std::uintptr_t add_root_rva=0x3ee2a0;
inline constexpr char remove_root_name[]="?ClearRootSet@UObject@Unreal@RC@@QEAAXXZ";
inline constexpr std::uintptr_t remove_root_rva=0x3e3f10;
}
