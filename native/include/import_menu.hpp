#pragma once
#include "abort_trial.hpp"
namespace ffr {
// Detached import never applies its target. Prior turn beginnings are allowed;
// an actual matching game-thread idle command menu remains mandatory.
inline bool ordinary_idle_import_menu(const AbortGuard& guard) {
    return guard.game_thread && guard.matching_world && guard.ordinary_encounter && guard.battle_active
        && guard.command_menu && guard.units>0 && guard.units<=64 && guard.waiting_jobs==0
        && guard.working_jobs==0 && guard.friendly_actions>=0;
}
}
