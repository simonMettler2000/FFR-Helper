#pragma once
#include "abort_trial.hpp"
#include "partial_recovery.hpp"
#include <charconv>
#include <memory>
#include <set>

namespace ffr {
struct RecoveryReadRequest {
    std::string session,nonce,opening_hash;
    std::int64_t deadline{};
    std::uint64_t sequence{};
};
inline std::optional<RecoveryReadRequest> parse_recovery_request(const std::string& text,const std::string& session,std::int64_t now) {
    if(text.size()>512 || now<0)return {};
    std::istringstream input(text);std::array<std::string,6> lines;std::string extra;
    for(auto& line:lines)if(!std::getline(input,line))return {};
    if(std::getline(input,extra))return {};
    auto hex=[](const std::string& value,std::size_t size){return value.size()==size && value.find_first_not_of("0123456789abcdef")==std::string::npos;};
    if(lines[0]!="FFR_HELPER_PARTIAL_RECOVERY_V1" || lines[1]!=session || !hex(lines[1],32) || !hex(lines[2],32) || !hex(lines[5],64)
        || lines[3].empty() || lines[3].size()>12 || lines[4].empty() || lines[4].size()>18)return {};
    RecoveryReadRequest out;out.session=lines[1];out.nonce=lines[2];out.opening_hash=lines[5];
    auto number=[](const std::string& value,auto& target){auto result=std::from_chars(value.data(),value.data()+value.size(),target);return result.ec==std::errc{} && result.ptr==value.data()+value.size();};
    if(!number(lines[3],out.deadline) || !number(lines[4],out.sequence) || !out.sequence || out.deadline<now || out.deadline-now>8)return {};
    return out;
}
struct RecoveryReadReceipt {
    RecoveryReadRequest request;
    std::uint64_t number{};
    std::int64_t captured_at{};
    std::string phase="idle",message="No partial recovery requested",metadata_hash;
    std::size_t metadata_bytes{};
    std::shared_ptr<const PartialRecovery> data;
};
class RecoveryReadController {
    std::set<std::pair<std::string,std::string>> consumed;
    RecoveryReadReceipt latest;
    std::uint64_t number{};
    bool limit_reached=false;
public:
    const RecoveryReadReceipt& receipt() const {return latest;}
    bool exhausted() const {return limit_reached;}
    void clear_latest(){latest=RecoveryReadReceipt{};}
    template<class Capture>
    bool capture(const RecoveryReadRequest& request,const AbortGuard& guard,bool strict_field_ready,bool mutation_pending,
                 std::int64_t now,Capture detached_read) {
        const auto key=std::pair{request.session,request.nonce};
        if(consumed.count(key) || limit_reached)return false;
        latest=RecoveryReadReceipt{};latest.request=request;latest.number=++number;latest.captured_at=now;
        if(consumed.size()>=128) {limit_reached=true;latest.phase="limit";latest.message="Partial recovery limit reached for this process";return false;}
        consumed.insert(key); // Replay must never capture another moment, even after rejection or failure.
        if(request.deadline<now || request.deadline-now>8 || request.session!=guard.session || request.sequence!=guard.sequence
            || request.opening_hash!=guard.opening_hash || !guard.game_thread || !guard.matching_world || !guard.ordinary_encounter
            || guard.battle_active || !guard.field_ready || guard.units!=0 || guard.waiting_jobs!=0 || guard.working_jobs!=0
            || !strict_field_ready || mutation_pending) {
            latest.phase="rejected";latest.message="Recovery requires a matching cleared field boundary without pending practice work";return false;
        }
        try {
            auto copied=detached_read();
            if(!copied) {latest.phase="failed";latest.message="Progression, field or clock changed during recovery copying";return false;}
            latest.data=std::make_shared<const PartialRecovery>(std::move(*copied));latest.phase="pending-write";
            latest.message="Partial recovery copied; waiting for verified durable files";return true;
        }catch(...) {latest.phase="failed";latest.message="Partial recovery copy failed; no automatic retry";return false;}
    }
    void published(const RecoveryReadReceipt& copied,bool success,const std::string& digest,std::size_t bytes) {
        if(latest.phase!="pending-write" || latest.number!=copied.number || latest.request.session!=copied.request.session
            || latest.request.nonce!=copied.request.nonce || latest.data!=copied.data)return;
        success=success && digest.size()==64 && digest.find_first_not_of("0123456789abcdef")==std::string::npos && bytes>0 && bytes<=192*1024;
        latest.phase=success?"captured":"write-failed";
        latest.message=success?"Partial recovery files committed and read back; restoration remains unavailable":"Recovery publication failed; no committed recovery or retry";
        if(success){latest.metadata_hash=digest;latest.metadata_bytes=bytes;}
    }
};
} // namespace ffr
