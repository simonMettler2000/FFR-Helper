#include "abort_trial.hpp"
#include "import_menu.hpp"
#include <iostream>

#define CHECK(x) do {if(!(x))throw std::runtime_error("Abort trial check failed at "+std::to_string(__LINE__));}while(false)
int main() {
    try {
        ffr::AbortRequest request{std::string(32,'a'),std::string(32,'b'),std::string(64,'c'),"prepare",105,1};
        ffr::AbortGuard good{true,true,true,true,true,0,0,0,4,request.session,request.opening_hash,1};
        good.opening_history_untouched=true;good.command_abort_ready=true;good.friendly_actions=1;
        unsigned captures=0,aborts=0;
        auto advanced=good;advanced.friendly_actions=3;
        CHECK(ffr::ordinary_idle_import_menu(advanced));
        auto queued=advanced;queued.working_jobs=1;CHECK(!ffr::ordinary_idle_import_menu(queued));
        auto capture=[&] {++captures;return std::vector<unsigned char>{1,2,3};};
        auto end=[&] {++aborts;};
        for(unsigned failure=0;failure<11;++failure) {
            auto guard=good;
            switch(failure) {
            case 0:guard.game_thread=false;break;case 1:guard.matching_world=false;break;
            case 2:guard.ordinary_encounter=false;break;case 3:guard.battle_active=false;break;
            case 4:guard.command_menu=false;break;case 5:guard.waiting_jobs=1;break;
            case 6:guard.working_jobs=1;break;case 7:guard.opening_history_untouched=false;break;
            case 8:guard.units=0;break;case 9:guard.sequence=2;break;case 10:guard.command_abort_ready=false;break;
            }
            ffr::AbortTrial trial;CHECK(!trial.prepare(request,guard,100,capture));CHECK(trial.phase()==ffr::AbortPhase::rejected);
        }
        CHECK(captures==0 && aborts==0);
        auto commit=request;commit.phase="commit";
        ffr::AbortTrial trial;CHECK(trial.prepare(request,good,100,capture));
        CHECK(!trial.commit(commit,good,100,end));CHECK(aborts==0);
        trial.recovery_written(true);CHECK(trial.commit(commit,good,101,end));CHECK(aborts==1);
        CHECK(!trial.commit(commit,good,102,end));CHECK(aborts==1);
        auto cleared=good;cleared.battle_active=false;cleared.units=0;cleared.working_jobs=1;
        trial.observe(cleared,102);CHECK(trial.phase()==ffr::AbortPhase::abort_sent);
        cleared.working_jobs=0;trial.observe(cleared,103);CHECK(trial.phase()==ffr::AbortPhase::abort_sent);
        cleared.field_ready=true;trial.observe(cleared,103);CHECK(trial.phase()==ffr::AbortPhase::manager_cleared);
        CHECK(!trial.prepare(request,good,103,capture)); // Terminal trial cannot be replayed.
        for(unsigned failure=0;failure<5;++failure) {
            ffr::AbortTrial denied;CHECK(denied.prepare(request,good,100,capture));denied.recovery_written(true);
            auto changed=commit;auto guard=good;auto now=101;
            switch(failure) {case 0:changed.nonce=std::string(32,'d');break;case 1:guard.sequence=2;break;
            case 2:now=106;break;case 3:guard.opening_history_untouched=false;break;case 4:guard.matching_world=false;break;}
            CHECK(!denied.commit(changed,guard,now,end));CHECK(aborts==1);
        }
        ffr::AbortTrial write_failed;CHECK(write_failed.prepare(request,good,100,capture));write_failed.recovery_written(false);
        CHECK(!write_failed.commit(commit,good,101,end));CHECK(aborts==1);
        ffr::AbortTrial expires;CHECK(expires.prepare(request,good,100,capture));expires.observe(good,106);
        CHECK(expires.phase()==ffr::AbortPhase::rejected);expires.recovery_written(true);CHECK(!expires.commit(commit,good,106,end));
        ffr::AbortTrial throws;CHECK(!throws.prepare(request,good,100,[]()->std::vector<unsigned char>{throw std::runtime_error("serialization failed");}));
        CHECK(!throws.commit(commit,good,101,end));CHECK(aborts==1);
        ffr::AbortTrial timeout;CHECK(timeout.prepare(request,good,100,capture));timeout.recovery_written(true);
        CHECK(timeout.commit(commit,good,101,end));
        timeout.observe(good,117,0.125f);CHECK(timeout.phase()==ffr::AbortPhase::abort_sent);
        timeout.observe(good,10000,999.f);CHECK(timeout.phase()==ffr::AbortPhase::abort_sent);
        CHECK(timeout.cleanup_active_millis()==375); // Long pause/large resume delta is capped.
        timeout.observe(good,10001,0);CHECK(timeout.cleanup_active_millis()==375);
        for(unsigned i=0;i<60;++i)timeout.observe(good,10002+i,0.25f);
        CHECK(timeout.phase()==ffr::AbortPhase::timed_out);
        CHECK(!timeout.commit(commit,good,117,end));CHECK(aborts==2);
        ffr::AbortTrial changed;CHECK(changed.prepare(request,good,100,capture));changed.recovery_written(true);
        CHECK(changed.commit(commit,good,101,end));cleared.sequence=2;changed.observe(cleared,102);
        CHECK(changed.phase()==ffr::AbortPhase::timed_out && aborts==3);
        ffr::AbortTrial resumed;CHECK(resumed.prepare(request,good,100,capture));resumed.recovery_written(true);
        CHECK(resumed.commit(commit,good,101,end));cleared.sequence=1;
        resumed.observe(cleared,10000,0.016f);CHECK(resumed.phase()==ffr::AbortPhase::manager_cleared);
        ffr::AbortTrial invalid_time;CHECK(invalid_time.prepare(request,good,100,capture));invalid_time.recovery_written(true);
        CHECK(invalid_time.commit(commit,good,101,end));invalid_time.observe(good,102,-1.f);
        CHECK(invalid_time.phase()==ffr::AbortPhase::timed_out);
        // Verify rearm only after a completed exit and independently accepted
        // next child; no cleanup observation by itself grants another abort.
        auto completed=[&] {
            ffr::AbortTrial value;CHECK(value.prepare(request,good,100,capture));
            value.recovery_written(true);CHECK(value.commit(commit,good,101,end));
            auto field=good;field.battle_active=false;field.units=0;field.field_ready=true;
            value.observe(field,102,0.016f);CHECK(value.phase()==ffr::AbortPhase::manager_cleared);
            return value;
        };
        auto child=good;child.sequence=2;
        auto rearm=request;rearm.phase="rearm";rearm.nonce=std::string(32,'d');rearm.sequence=2;rearm.deadline=109;
        const auto aborts_before_rearm=aborts;
        for(unsigned failure=0;failure<16;++failure) {
            auto value=completed();auto changed_rearm=rearm;auto changed_child=child;unsigned verification=0;
            switch(failure) {
            case 0:changed_rearm.nonce=request.nonce;break;case 1:changed_rearm.nonce.clear();break;
            case 2:changed_rearm.session=std::string(32,'e');break;case 3:changed_rearm.opening_hash=std::string(64,'f');break;
            case 4:changed_rearm.sequence=1;changed_child.sequence=1;break;case 5:changed_rearm.sequence=3;changed_child.sequence=3;break;
            case 6:changed_rearm.deadline=102;break;case 7:changed_child.opening_history_untouched=false;break;
            case 8:changed_child.command_abort_ready=false;break;case 9:changed_child.ordinary_encounter=false;break;
            case 10:changed_child.working_jobs=1;break;case 11:changed_child.game_thread=false;break;
            case 12:changed_child.command_menu=false;break;case 13:changed_rearm.phase="prepare";break;
            }
            const auto count=aborts;
            CHECK(!value.rearm_verified_child(changed_rearm,changed_child,103,[&] {
                ++verification;if(failure==15)throw std::runtime_error("Fixture child read failed");return false;
            }));
            CHECK(value.phase()==ffr::AbortPhase::manager_cleared && value.active_request().nonce==request.nonce);
            CHECK(!value.recovery_bytes().empty() && aborts==count);
            CHECK(verification==(failure>=14?1u:0u));
        }
        for(auto* failed:{&invalid_time,&timeout,&changed,&expires,&write_failed,&throws})
            CHECK(!failed->rearm_verified_child(rearm,child,103,[]{return true;}));
        auto repeated=completed();unsigned verified=0;
        CHECK(repeated.rearm_verified_child(rearm,child,103,[&] {
            ++verified;CHECK(!repeated.rearm_verified_child(rearm,child,103,[]{return true;}));return true;
        }));
        CHECK(verified==1 && repeated.phase()==ffr::AbortPhase::idle && repeated.recovery_bytes().empty());
        CHECK(repeated.active_request().sequence==0 && repeated.cleanup_active_millis()==0);
        CHECK(!repeated.commit(commit,good,103,end)); // Prior commit cannot replay.
        auto next_prepare=rearm;next_prepare.phase="prepare";next_prepare.nonce=std::string(32,'e');
        CHECK(repeated.prepare(next_prepare,child,103,capture));repeated.recovery_written(true);
        auto next_commit=next_prepare;next_commit.phase="commit";CHECK(repeated.commit(next_commit,child,104,end));
        CHECK(aborts==aborts_before_rearm+18); // 16 isolated completions plus this two-cycle trial.
        std::cout<<"Opening-abort guard, durable recovery gate, expiry and exactly-once checks passed.\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
