#include "combat_transport.hpp"
#include <iostream>
#include <stdexcept>
#define CHECK(x) do {if(!(x))throw std::runtime_error("Combat transport check failed at "+std::to_string(__LINE__));}while(false)
int main() {
    try {
        const auto session=std::string(32,'a'),nonce=std::string(32,'b'),hash=std::string(64,'c');
        const auto text="FFR_HELPER_COMBAT_OBSERVATION_V1\n"+session+'\n'+nonce+"\n108\n1\n"+hash+'\n';
        auto request=ffr::parse_combat_request(text,session,100);CHECK(request && request->sequence==1);
        CHECK(!ffr::parse_combat_request(text,session,99));CHECK(!ffr::parse_combat_request(text,session,109));
        CHECK(!ffr::parse_combat_request(text,nonce,100));CHECK(!ffr::parse_combat_request(text+"extra\n",session,100));
        CHECK(!ffr::parse_combat_request(text.substr(0,text.size()-66),session,100));
        auto bad=text;bad[bad.find("108")]='-';CHECK(!ffr::parse_combat_request(bad,session,100));
        ffr::AbortGuard guard;guard.session=session;guard.opening_hash=hash;guard.sequence=1;
        guard.game_thread=guard.matching_world=guard.ordinary_encounter=guard.battle_active=guard.command_menu=true;
        guard.units=1;guard.waiting_jobs=guard.working_jobs=0;guard.friendly_actions=3;
        unsigned calls{};
        auto capture=[&]()->std::optional<ffr::CombatObservation>{++calls;ffr::CombatObservation out;out.units.resize(1);return out;};
        ffr::CombatReadController controller;
        CHECK(controller.capture(*request,guard,false,100,capture));CHECK(calls==1 && controller.receipt().phase=="captured");
        CHECK(!controller.capture(*request,guard,false,101,capture) && calls==1); // Same nonce never moves to a later frame.
        controller.clear_latest();CHECK(controller.receipt().phase=="idle");
        CHECK(!controller.capture(*request,guard,false,101,capture) && calls==1); // Cache survives a session/report reset.
        for(unsigned mode=0;mode<8;++mode) {
            auto changed=guard;bool mutation=false;
            switch(mode) {
            case 0:changed.game_thread=false;break;case 1:changed.matching_world=false;break;
            case 2:changed.command_menu=false;break;case 3:changed.waiting_jobs=1;break;
            case 4:changed.sequence=2;break;case 5:changed.opening_hash=std::string(64,'0');break;
            case 6:changed.friendly_actions=-1;break;case 7:mutation=true;break;
            }
            ffr::CombatReadController denied;CHECK(!denied.capture(*request,changed,mutation,100,capture));
            CHECK(denied.receipt().phase=="rejected" && calls==1);
        }
        ffr::CombatReadController failed;
        CHECK(!failed.capture(*request,guard,false,100,[]()->std::optional<ffr::CombatObservation>{return {};}));
        CHECK(failed.receipt().phase=="failed");
        ffr::CombatReadController throwing;
        CHECK(!throwing.capture(*request,guard,false,100,[]()->std::optional<ffr::CombatObservation>{throw std::runtime_error("fixture");}));
        CHECK(throwing.receipt().phase=="failed");
        ffr::CombatReadController limit;
        for(unsigned i=0;i<128;++i){auto next=*request;next.nonce=std::to_string(i);CHECK(limit.capture(next,guard,false,100,capture));}
        auto extra=*request;extra.nonce="beyond-limit";CHECK(!limit.capture(extra,guard,false,100,capture));CHECK(limit.receipt().phase=="limit");
        std::cout<<"Bounded read-only combat protocol, command boundary and replay guards passed.\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
