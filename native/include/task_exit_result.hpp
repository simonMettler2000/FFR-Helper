#pragma once
#include <cstdint>
#include <string>
namespace ffr {
// Internal runtime identities. Never serialize these pointers or accept them
// from the desktop client; preparation retains them for exact commit binding.
struct TaskAbortTargets {
    std::uint64_t world{},instance{},manager{},task_manager{},task{},step{},registry{},command_vm{};
    bool operator==(const TaskAbortTargets&) const=default;
};
struct NativeTaskExitResult {
    bool end_issued{},end_flag_observed{},ui_end_issued{},ui_closed_observed{};
    bool finish_check_issued{},finish_check_accepted{},result_task_observed{},result_mode_observed{},uncertain{};
    std::string message="Native task exit has not been requested";
};
}
