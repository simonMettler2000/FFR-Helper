#include "recovery_transport.hpp"
#include "clone_payload.hpp"
#include <iostream>
#include <stdexcept>
#define CHECK(x) do {if(!(x))throw std::runtime_error("Recovery transport check failed at "+std::to_string(__LINE__));}while(false)
int main() {
    try {
        const std::string session(32,'a'),nonce(32,'b'),hash(64,'c');
        const auto text="FFR_HELPER_PARTIAL_RECOVERY_V1\n"+session+'\n'+nonce+"\n108\n1\n"+hash+'\n';
        const auto request=ffr::parse_recovery_request(text,session,100);CHECK(request && request->sequence==1);
        CHECK(!ffr::parse_recovery_request(text+"extra\n",session,100));CHECK(!ffr::parse_recovery_request(text,session,99));
        CHECK(!ffr::parse_recovery_request(text,std::string(32,'d'),100));
        ffr::AbortGuard guard;guard.session=session;guard.opening_hash=hash;guard.sequence=1;
        guard.game_thread=guard.matching_world=guard.ordinary_encounter=guard.field_ready=true;
        guard.units=guard.waiting_jobs=guard.working_jobs=0;
        unsigned calls=0;
        auto copy=[&]()->std::optional<ffr::PartialRecovery>{++calls;ffr::PartialRecovery out;out.progression=clone_fixture_payload();return out;};
        ffr::RecoveryReadController controller;
        CHECK(controller.capture(*request,guard,true,false,100,copy) && calls==1 && controller.receipt().phase=="pending-write");
        auto pending=controller.receipt();CHECK(!controller.capture(*request,guard,true,false,100,copy) && calls==1);
        controller.published(pending,true,hash,100);CHECK(controller.receipt().phase=="captured" && controller.receipt().metadata_bytes==100);
        CHECK(!controller.capture(*request,guard,true,false,101,copy) && controller.receipt().phase=="captured");
        auto next=*request;next.nonce=std::string(32,'d');CHECK(controller.capture(next,guard,true,false,101,copy) && calls==2);
        controller.published(pending,true,hash,100);CHECK(controller.receipt().phase=="pending-write");
        controller.published(controller.receipt(),false,"",0);CHECK(controller.receipt().phase=="write-failed");
        CHECK(!controller.capture(next,guard,true,false,102,copy) && calls==2);
        for(unsigned mode=0;mode<6;++mode) {
            ffr::RecoveryReadController denied;auto changed=guard;
            if(mode==0)changed.battle_active=true;if(mode==1)changed.units=1;if(mode==2)changed.waiting_jobs=1;
            if(mode==3)changed.sequence=2;
            CHECK(!denied.capture(*request,changed,mode!=4,mode==5,100,copy));CHECK(denied.receipt().phase=="rejected");
            CHECK(!denied.capture(*request,guard,true,false,100,copy));
        }
        CHECK(calls==2);
        ffr::RecoveryReadController bounded;
        constexpr char hex[]="0123456789abcdef";
        for(unsigned i=0;i<129;++i){auto unique=*request;unique.nonce=std::string(30,'0')+hex[i>>4]+hex[i&15];bounded.capture(unique,guard,true,false,100,[]()->std::optional<ffr::PartialRecovery>{return {};});}
        CHECK(bounded.exhausted() && bounded.receipt().phase=="limit");const auto count=bounded.receipt().number;
        CHECK(!bounded.capture(*request,guard,true,false,100,copy) && bounded.receipt().number==count && calls==2);
        std::cout<<"Recovery request binding, replay, publication and bounded-history checks passed.\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
