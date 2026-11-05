#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <utility>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <limits>

namespace ffr {
struct AbortRequest {
    std::string session, nonce, opening_hash, phase;
    std::int64_t deadline{};
    std::uint64_t sequence{};
};
struct AbortGuard {
    bool game_thread{}, matching_world{}, ordinary_encounter{}, battle_active{}, command_menu{};
    int waiting_jobs=-1, working_jobs=-1, friendly_actions=-1, units=-1;
    std::string session, opening_hash;
    std::uint64_t sequence{};
    bool field_ready{};
    bool managers_released{};
    // Separate from the raw turn-beginning count. Both exact-profile journals
    // must cover this opening/thread with no entries or coverage-loss flags.
    bool opening_history_untouched{};
    bool command_abort_ready{};
};
enum class AbortPhase { idle, recovery_pending, armed, abort_sent, manager_cleared, rejected, timed_out };
inline const char* phase_name(AbortPhase phase) {
    switch(phase) {
    case AbortPhase::idle:return "idle";
    case AbortPhase::recovery_pending:return "recovery-pending";
    case AbortPhase::armed:return "armed";
    case AbortPhase::abort_sent:return "abort-sent";
    case AbortPhase::manager_cleared:return "manager-cleared";
    case AbortPhase::rejected:return "rejected";
    case AbortPhase::timed_out:return "timed-out";
    }
    return "rejected";
}

// Only an opening-abort experiment. No deserialize, rebind, restart, save-state
// restore or arbitrary address/value operation exists in this controller.
class AbortTrial {
    AbortRequest request;
    AbortPhase current=AbortPhase::idle;
    std::vector<unsigned char> recovery;
    std::string note="No abort trial requested";
    double active_cleanup_seconds{};
    bool rearm_active=false;
    bool bound(const AbortRequest& next,const AbortGuard& guard,std::int64_t now) const {
        return next.deadline>=now && next.deadline<=now+8 && next.sequence>0
            && next.session==guard.session && next.sequence==guard.sequence && next.opening_hash==guard.opening_hash;
    }
    bool safe(const AbortGuard& guard) const {
        return guard.game_thread && guard.matching_world && guard.ordinary_encounter
            && guard.battle_active && guard.command_menu && guard.units>0 && guard.units<=64
            && guard.waiting_jobs==0 && guard.working_jobs==0 && guard.friendly_actions>=0
            && guard.opening_history_untouched && guard.command_abort_ready;
    }
public:
    AbortPhase phase() const {return current;}
    const AbortRequest& active_request() const {return request;}
    const std::string& message() const {return note;}
    const std::vector<unsigned char>& recovery_bytes() const {return recovery;}
    std::uint64_t cleanup_active_millis() const {return static_cast<std::uint64_t>(active_cleanup_seconds*1000);}
    // Only a verified recreated child can start a new transaction. This never
    // clears a rejected/timed-out/uncertain native attempt and does not execute
    // an abort. A separate fresh prepare/commit remains mandatory afterwards.
    template<class VerifyChild>
    bool rearm_verified_child(const AbortRequest& next,const AbortGuard& guard,std::int64_t now,
                              VerifyChild verify_child) {
        if(rearm_active)return false;
        rearm_active=true;
        struct Finish {bool& flag;~Finish(){flag=false;}} finish{rearm_active};
        auto eligible=[&] {
            return current==AbortPhase::manager_cleared && next.phase=="rearm"
                && !next.nonce.empty() && next.nonce!=request.nonce
                && next.session==request.session && next.opening_hash==request.opening_hash
                && request.sequence<std::numeric_limits<std::uint64_t>::max()
                && next.sequence==request.sequence+1 && bound(next,guard,now) && safe(guard);
        };
        try {
            if(!eligible() || !verify_child() || !eligible())return false;
            request={};recovery.clear();active_cleanup_seconds=0;current=AbortPhase::idle;
            note="Verified child accepted; waiting for a fresh explicit prepare/commit";
            return true;
        }catch(...) {return false;}
    }
    template<class Capture>
    bool prepare(const AbortRequest& next,const AbortGuard& guard,std::int64_t now,Capture capture) {
        if(current!=AbortPhase::idle) return false;
        request=next;
        if(next.phase!="prepare" || !bound(next,guard,now) || !safe(guard)) {
            current=AbortPhase::rejected;note="Trial requires the first player-command boundary of a matching ordinary encounter";return false;
        }
        try {recovery=capture();}catch(...) {current=AbortPhase::rejected;note="Progression recovery capture failed";return false;}
        if(recovery.empty() || recovery.size()>16*1024*1024) {
            recovery.clear();current=AbortPhase::rejected;note="Progression recovery capture is unavailable";return false;
        }
        current=AbortPhase::recovery_pending;note="Waiting for a verified progression recovery file";return true;
    }
    void recovery_written(bool success) {
        if(current!=AbortPhase::recovery_pending)return;
        current=success?AbortPhase::armed:AbortPhase::rejected;
        note=success?"Recovery file committed; waiting for a matching explicit commit":"Recovery file write failed; no abort was sent";
    }
    template<class EndBattle>
    bool commit(const AbortRequest& next,const AbortGuard& guard,std::int64_t now,EndBattle end_battle) {
        if(current!=AbortPhase::armed)return false;
        if(next.phase!="commit" || next.nonce!=request.nonce || next.session!=request.session
            || next.sequence!=request.sequence || next.opening_hash!=request.opening_hash
            || !bound(next,guard,now) || !safe(guard)) {
            current=AbortPhase::rejected;note="Trial changed or expired before commit; no abort was sent";return false;
        }
        // Mark before calling game code: an exception/reentrant tick cannot
        // issue the abort twice. Never auto-retry a potentially completed call.
        current=AbortPhase::abort_sent;active_cleanup_seconds=0;note="ForceEnd requested once; waiting for manager cleanup";
        end_battle();return true;
    }
    // Request expiry remains wall-clock based. Cleanup alone uses actual game
    // tick delta: backgrounded/paused wall time cannot expire an active wait.
    void observe(const AbortGuard& guard,std::int64_t now,float game_delta=0) {
        if(current==AbortPhase::recovery_pending || current==AbortPhase::armed) {
            if(now>request.deadline || guard.sequence!=request.sequence || guard.session!=request.session) {
                current=AbortPhase::rejected;note="Trial expired or its encounter changed; no abort was sent";
            }
        } else if(current==AbortPhase::abort_sent) {
            if(guard.sequence!=request.sequence || guard.session!=request.session || guard.opening_hash!=request.opening_hash) {
                current=AbortPhase::timed_out;note="Abort was sent, but its encounter context changed; cleanup was not verified";return;
            }
            if(guard.game_thread && guard.matching_world && guard.field_ready && !guard.battle_active && guard.units==0
                && guard.waiting_jobs==0 && guard.working_jobs==0) {
                current=AbortPhase::manager_cleared;note="Battle manager and job queues cleared; user must confirm field return";
            } else if(!std::isfinite(game_delta) || game_delta<0) {
                current=AbortPhase::timed_out;note="Invalid game-tick timing after ForceEnd; outcome uncertain. No retry; close this test launch";
            } else {
                // A large first delta after resuming must not consume the
                // whole budget before the exit state has had another tick.
                active_cleanup_seconds+=std::min(game_delta,0.25f);
                if(active_cleanup_seconds>15) {current=AbortPhase::timed_out;note="ForceEnd may remain pending after 15 active game seconds. No retry; close this test launch before further gameplay";}
            }
        }
    }
};
} // namespace ffr
