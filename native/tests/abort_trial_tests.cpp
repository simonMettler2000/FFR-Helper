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
        std::cout<<"Opening-abort guard, durable recovery gate, expiry and exactly-once checks passed.\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
