#pragma once
#include "abort_trial.hpp"
#include "combat_observation.hpp"
#include <charconv>
#include <set>

namespace ffr {
struct CombatReadRequest {
    std::string session,nonce,opening_hash;
    std::int64_t deadline{};
    std::uint64_t sequence{};
};
inline std::optional<CombatReadRequest> parse_combat_request(const std::string& text,const std::string& session,std::int64_t now) {
    if(text.size()>512||now<0)return {};
    std::istringstream in(text);std::array<std::string,6> lines;std::string extra;
    for(auto& line:lines)if(!std::getline(in,line))return {};
    if(std::getline(in,extra))return {};
    auto hex=[](const std::string& s,std::size_t n){return s.size()==n && s.find_first_not_of("0123456789abcdef")==std::string::npos;};
    if(lines[0]!="FFR_HELPER_COMBAT_OBSERVATION_V1"||lines[1]!=session||!hex(lines[1],32)||!hex(lines[2],32)||!hex(lines[5],64)
        ||lines[3].empty()||lines[3].size()>12||lines[4].empty()||lines[4].size()>18)return {};
    CombatReadRequest out;out.session=lines[1];out.nonce=lines[2];out.opening_hash=lines[5];
    auto number=[](const std::string& s,auto& value){auto r=std::from_chars(s.data(),s.data()+s.size(),value);return r.ec==std::errc{} && r.ptr==s.data()+s.size();};
    if(!number(lines[3],out.deadline)||!number(lines[4],out.sequence)||out.sequence==0||out.deadline<now||out.deadline-now>8)return {};
    return out;
}
struct CombatReadReceipt {
    CombatReadRequest request;
    std::uint64_t number{};
    std::int64_t captured_at{};
    std::string phase="idle",message="No combat observation requested",data="null";
};
class CombatReadController {
    std::set<std::pair<std::string,std::string>> consumed;
    CombatReadReceipt latest;
    std::uint64_t sequence{};
public:
    const CombatReadReceipt& receipt() const {return latest;}
    void clear_latest(){latest=CombatReadReceipt{};}
    template<class Capture>
    bool capture(const CombatReadRequest& request,const AbortGuard& guard,bool mutation_pending,std::int64_t now,Capture detached_read) {
        const auto key=std::pair{request.session,request.nonce};
        if(consumed.count(key))return false;
        latest=CombatReadReceipt{};latest.request=request;latest.number=++sequence;latest.captured_at=now;
        if(consumed.size()>=128) {latest.phase="limit";latest.message="Read-only capture limit reached for this process";return false;}
        consumed.insert(key); // Consume before reading; replay never captures a different moment.
        if(request.deadline<now||request.deadline-now>8||request.session!=guard.session||request.sequence!=guard.sequence
            ||request.opening_hash!=guard.opening_hash||!guard.game_thread||!guard.matching_world||!guard.ordinary_encounter
            ||!guard.battle_active||!guard.command_menu||guard.units<1||guard.units>64||guard.waiting_jobs!=0
            ||guard.working_jobs!=0||guard.friendly_actions<0||mutation_pending) {
            latest.phase="rejected";latest.message="Capture requires a matching ordinary player-command boundary with no pending practice action";return false;
        }
        try {
            auto copied=detached_read();
            if(!copied || copied->units.size()!=static_cast<std::size_t>(guard.units)) {
                latest.phase="failed";latest.message="Native combat components could not be copied completely";return false;
            }
            auto json=to_json(*copied);
            if(json.size()>384*1024) {latest.phase="failed";latest.message="Detached combat observation exceeds its transport bound";return false;}
            latest.data=std::move(json);latest.phase="captured";latest.message="Partial combat observation copied; restoration remains unavailable";return true;
        }catch(...) {latest.phase="failed";latest.message="Combat observation failed; no automatic retry";return false;}
    }
};
} // namespace ffr
