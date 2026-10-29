// Compiled only into the separately named experimental abort-trial DLL.
// Included inside native_observer.cpp's anonymous namespace.
using TickFunction=void(__fastcall*)(void*,float);
using ObjectGetter=void*(__fastcall*)(void*);
using EndBattleFunction=void(__fastcall*)();
std::atomic<TickFunction> original_tick{nullptr};
void* tick_address{};
std::atomic<bool> tick_installed{false};

std::string json_object(std::initializer_list<std::pair<std::string,std::string>> fields) {
    std::string out="{";
    for(const auto& [key,value]:fields) {if(out.size()>1)out+=',';out+=ffr::quote(key)+':'+value;}
    return out+'}';
}
std::string json_flag(bool value) {return value?"true":"false";}
template<class T> bool read_value(std::uint64_t address,T& value) {
    return address && read_memory(address,&value,sizeof(value));
}
bool valid_object(std::uint64_t address) {
    std::array<std::uint64_t,4> header{};
    return address && read_memory(address,header.data(),sizeof(header)) && header[0] && header[2]
        && (header[1]&0x40000000u)==0;
}
std::optional<ffr::AbortRequest> read_abort_request(const std::filesystem::path& file,const Lease& lease) {
    std::string raw;if(!read_small(file,raw,512))return {};
    std::istringstream in(raw);std::string version,session,nonce,expiry,sequence,hash,phase,extra;
    if(!std::getline(in,version)||!std::getline(in,session)||!std::getline(in,nonce)||!std::getline(in,expiry)
        ||!std::getline(in,sequence)||!std::getline(in,hash)||!std::getline(in,phase)||std::getline(in,extra))return {};
    if(version!="FFR_HELPER_ABORT_OPENING_V1"||session!=lease.session||!hex_id(session)||!hex_id(nonce)
        ||expiry.empty()||expiry.size()>12||expiry.find_first_not_of("0123456789")!=std::string::npos
        ||sequence.empty()||sequence.size()>18||sequence.find_first_not_of("0123456789")!=std::string::npos
        ||hash.size()!=64||hash.find_first_not_of("0123456789abcdef")!=std::string::npos
        ||(phase!="prepare"&&phase!="commit"&&phase!="probe-import"&&phase!="probe-normalize"))return {};
    const auto deadline=std::stoll(expiry);const auto number=std::stoull(sequence);
    if(deadline<seconds()||deadline>seconds()+8||number==0)return {};
    return ffr::AbortRequest{session,nonce,hash,phase,deadline,number};
}

ffr::AbortGuard probe_abort(void* tick_context,const std::shared_ptr<const Observer::Opening>& opening,
                           const std::string& session,const std::string& opening_hash) {
    ffr::AbortGuard out;out.session=session;out.opening_hash=opening_hash;
    if(opening)out.sequence=opening->sequence;
    const auto address=reinterpret_cast<std::uint64_t>(tick_context);
    if(address<0x10000)return out;
    const auto manager=address-0x30,base=reinterpret_cast<std::uint64_t>(GetModuleHandleW(nullptr));
    std::uint64_t vtable{};
    if(!valid_object(manager)||!read_value(manager,vtable)||vtable!=base+ffr::abort_profile::manager_vtable_rva)return out;
    out.game_thread=opening && opening->thread==GetCurrentThreadId();
    unsigned char active{};
    if(!read_value(manager+0x1a9,active)||active>1)return out;
    out.battle_active=active!=0;
    auto* world=reinterpret_cast<ObjectGetter>(base+ffr::abort_profile::get_world_rva)(reinterpret_cast<void*>(manager));
    if(!valid_object(reinterpret_cast<std::uint64_t>(world)))return out;
    auto* map=reinterpret_cast<ObjectGetter>(base+ffr::abort_profile::get_map_rva)(world);
    out.matching_world=opening && opening->context_identity==reinterpret_cast<std::uint64_t>(map) && valid_object(reinterpret_cast<std::uint64_t>(map));
    auto* ui=reinterpret_cast<ObjectGetter>(base+ffr::abort_profile::get_ui_rva)(world);
    std::uint8_t phase=255,during=255;
    const bool ui_known=valid_object(reinterpret_cast<std::uint64_t>(ui))
        && read_value(reinterpret_cast<std::uint64_t>(ui)+0x50,phase)
        && read_value(reinterpret_cast<std::uint64_t>(ui)+0x80,during) && during<=1;
    if(opening) {
        const auto& entry=opening->encounter;
        out.ordinary_encounter=entry.event==-1 && entry.can_escape && !entry.skip_win_result && !entry.skip_lose_result
            && !entry.bounty && !entry.battle_test && entry.preemption==0 && !entry.skip_intro;
    }
    std::uint64_t jobs{},units{};
    if(!read_value(manager+0x40,jobs)||!read_value(manager+0x48,units))return out;
    if(!jobs && !units && ui_known && during==0 && !out.battle_active) {
        out.units=0;out.waiting_jobs=0;out.working_jobs=0;out.field_ready=true;out.managers_released=true;return out;
    }
    if(!valid_object(jobs)||!valid_object(units))return out;
    ffr::RawArray waiting{},working{},unit_list{};
    if(!read_value(jobs+0x28,waiting)||!read_value(jobs+0x38,working)
        ||!ffr::valid_array(waiting,1024)||!ffr::valid_array(working,1024))return out;
    out.waiting_jobs=waiting.count;out.working_jobs=working.count;
    if(!read_value(units+0x28,unit_list)||!ffr::valid_array(unit_list,64))return out;
    out.units=unit_list.count;
    out.field_ready=ui_known && during==0 && !out.battle_active && out.units==0
        && out.waiting_jobs==0 && out.working_jobs==0;
    if(!opening)return out;
    const auto& friend_ids=opening->encounter.friend_ids;
    std::set<std::int32_t> expected(friend_ids.begin(),friend_ids.end()),found;
    if(expected.empty()||expected.size()!=friend_ids.size())return out;
    std::vector<std::uint64_t> actors;
    if(!ffr::copy_array(read_memory,unit_list,64,actors))return out;
    std::uint64_t selected{};if(!read_value(units+0x38,selected))return out;
    bool selected_friendly=false;int actions=0;
    for(auto actor:actors) {
        std::uint64_t parameter{};
        if(!valid_object(actor)||!read_value(actor+0x838,parameter)||!valid_object(parameter))return out;
        std::int32_t id{},count{};
        if(!read_value(parameter+0xd0,id))return out;
        if(!expected.count(id))continue;
        if(!found.insert(id).second||!read_value(parameter+0x4dc,count)||count<0||count>100000)return out;
        actions+=count;if(actor==selected)selected_friendly=true;
    }
    if(found!=expected)return out;
    out.friendly_actions=actions;
    out.command_menu=ui_known && during==1 && phase==0 && selected_friendly;
    return out;
}

#include "command_wait_runtime.inl"
#include "import_runtime.inl"
#include "combat_runtime.inl"
#include "recovery_runtime.inl"
#include "normalization_runtime.inl"

void abort_tick(void* context) {
    auto* current=published_observer.load(std::memory_order_acquire);
    if(!current||current->quit.load())return;
    std::shared_ptr<const Observer::Opening> opening;
    std::string session,hash;std::optional<ffr::AbortRequest> request;bool available=false;
    bool abort_permitted=false;
    {std::lock_guard lock(current->mutex);opening=current->opening;session=current->session;hash=current->opening_hash;
        request=current->abort_request;available=current->dispatcher_available;abort_permitted=current->abort_available;}
    if(!available)return;
    const auto now=seconds();
    if(current->expires.load()<now) {
        std::lock_guard lock(current->mutex);ffr::AbortGuard expired;expired.session=session;
        current->trial.observe(expired,now);return;
    }
    auto guard=probe_abort(context,opening,session,hash);
    {std::lock_guard lock(current->mutex);
        current->last_tick_context=guard.game_thread && guard.matching_world?reinterpret_cast<std::uint64_t>(context):0;
        guard.opening_history_untouched=opening && current->opening==opening && current->session==session
            && untouched_history_locked(*current,*opening);
        std::uint64_t command_owner{};ffr::RawArray command_requests{};
        const bool matching_wait_owner=current->last_tick_context>=0x10000
            && read_value(current->last_tick_context-0x30+0xb0,command_owner) && command_owner==current->wait_owner
            && valid_object(command_owner) && read_value(command_owner+0x28,command_requests)
            && ffr::valid_array(command_requests,64) && command_requests.count>0;
        guard.command_wait_ready=opening && matching_wait_owner && command_wait_installed.load() && current->wait_sequence==opening->sequence
            && current->wait_observations>0 && current->wait_last_value
            && current->wait_sample_time>=now-2 && current->wait_sample_time<=now+2;
        current->last_guard=guard;current->probe_time=now;current->trial.observe(guard,now);
    }
    run_combat_probe(current,context,guard,now);
    run_recovery_probe(current,context,opening,guard,now);
    run_import_probe(current,context,opening,guard,request,now);
    run_normalization_probe(current,context,opening,guard,request,now);
    // The import-only and abort experiments cannot be combined in one launch.
    {std::lock_guard lock(current->mutex);if(current->import_attempted)return;}
    if(abort_permitted && request && request->phase=="prepare") {
        std::lock_guard lock(current->mutex);
        current->trial.prepare(*request,guard,now,[&] {
            auto copied=ffr::capture_progression(read_memory,root_getter,save_memory,memory_free);
            return copied?std::move(*copied):std::vector<unsigned char>{};
        });
    }
    if(abort_permitted && request && request->phase=="commit") {
        bool send=false;
        {std::lock_guard lock(current->mutex);
            current->trial.commit(*request,guard,now,[&] {send=true;});
        }
        // No observer lock is held while invoking game code.
        if(send) {
            const auto base=reinterpret_cast<std::uint64_t>(GetModuleHandleW(nullptr));
            reinterpret_cast<EndBattleFunction>(base+ffr::abort_profile::end_battle_rva)();
        }
    }
    bool capture_after=false;
    {std::lock_guard lock(current->mutex);
        capture_after=current->trial.phase()==ffr::AbortPhase::manager_cleared && current->after_progression.empty();
    }
    if(capture_after) {
        auto after=ffr::capture_progression(read_memory,root_getter,save_memory,memory_free);
        if(after) {std::lock_guard lock(current->mutex);current->after_progression=std::move(*after);}
    }
}

void __fastcall abort_tick_hook(void* context,float delta) {
    original_tick.load(std::memory_order_acquire)(context,delta);
    const auto error=GetLastError();
    try {abort_tick(context);}catch(...) { /* Never retry a potentially completed game action. */ }
    SetLastError(error);
}

void Observer::publish_abort(const Lease& lease) {
    auto incoming=read_abort_request(connection/L"abort-request.txt",lease);
    ffr::AbortRequest active;ffr::AbortPhase phase;std::vector<unsigned char> payload;
    {std::lock_guard lock(mutex);
        if(incoming)abort_request=std::move(incoming);
        active=trial.active_request();phase=trial.phase();
        if(phase==ffr::AbortPhase::recovery_pending)payload=trial.recovery_bytes();
    }
    if(!payload.empty()) {
        Sha256 hash;hash.add(payload.data(),payload.size());const auto digest=hash.finish();
        const auto path=connection/L"abort-recovery.gvas";
        const bool saved=write_atomic(path,std::string_view(reinterpret_cast<const char*>(payload.data()),payload.size()),ffr::max_progression_bytes)
            && file_hash(path)==digest;
        std::lock_guard lock(mutex);
        if(trial.active_request().nonce==active.nonce && trial.phase()==ffr::AbortPhase::recovery_pending) {
            trial.recovery_written(saved);if(saved)recovery_hash=digest;
        }
    }
    std::vector<unsigned char> after;
    {std::lock_guard lock(mutex);if(after_hash.empty())after=after_progression;}
    if(!after.empty()) {
        Sha256 hash;hash.add(after.data(),after.size());const auto digest=hash.finish();
        const auto path=connection/L"abort-after.gvas";
        if(write_atomic(path,std::string_view(reinterpret_cast<const char*>(after.data()),after.size()),ffr::max_progression_bytes)
            && file_hash(path)==digest) {std::lock_guard lock(mutex);after_hash=digest;}
    }
    publish_normalization_files(this,lease);
    std::string status;
    {std::lock_guard lock(mutex);
        const auto& action=trial.active_request();const auto& g=last_guard;
        const auto guard=json_object({{"gameThread",json_flag(g.game_thread)},{"matchingWorld",json_flag(g.matching_world)},
            {"ordinaryEncounter",json_flag(g.ordinary_encounter)},{"battleActive",json_flag(g.battle_active)},
            {"commandMenu",json_flag(g.command_menu)},{"units",std::to_string(g.units)},{"waitingJobs",std::to_string(g.waiting_jobs)},
            {"workingJobs",std::to_string(g.working_jobs)},{"friendlyActions",std::to_string(g.friendly_actions)},
            {"sequence",std::to_string(g.sequence)},{"openingHash",ffr::quote(g.opening_hash)},{"fieldReady",json_flag(g.field_ready)},
            {"managersReleased",json_flag(g.managers_released)},{"counterMeaning",ffr::quote("turn-beginnings")},
            {"openingHistoryUntouched",json_flag(g.opening_history_untouched)},{"commandWaitReady",json_flag(g.command_wait_ready)}});
        const auto recovery=json_object({{"kind",ffr::quote("progression-only")},{"restorable","false"},
            {"file",ffr::quote("abort-recovery.gvas")},{"sha256",ffr::quote(recovery_hash)},
            {"byteLength",std::to_string(trial.recovery_bytes().size())}});
        const auto after_metadata=json_object({{"kind",ffr::quote("progression-only")},{"restorable","false"},
            {"file",ffr::quote("abort-after.gvas")},{"sha256",ffr::quote(after_hash)},
            {"byteLength",std::to_string(after_progression.size())}});
        const auto imported=json_object({{"available",json_flag(import_available && ffr::abort_profile::detached_import_menu_reviewed && !normalization_attempted)},{"loaderReady",json_flag(import_loader_ready)},
            {"boundaryPolicy",ffr::quote("ordinary-idle-menu")},
            {"phase",ffr::quote(import_phase)},{"nonce",ffr::quote(import_nonce)},{"session",ffr::quote(import_session)},
            {"openingHash",ffr::quote(import_hash)},
            {"sequence",std::to_string(import_sequence)},{"passed",json_flag(import_result.passed)},
            {"cloneMatches",json_flag(import_result.clone_matches)},{"liveUnchanged",json_flag(import_result.live_unchanged)},
            {"inputUnchanged",json_flag(import_result.input_unchanged)},{"pinsReleased",json_flag(import_result.pins_released)},
            {"byteLength",std::to_string(import_result.clone.size())},{"message",ffr::quote(import_result.message)},
            {"restorable","false"}});
        status=json_object({{"protocol","1"},{"adapter",ffr::quote("ffr-helper-opening-abort-trial")},
            {"session",ffr::quote(lease.session)},{"time",std::to_string(seconds())},{"readOnly","false"},{"restorable","false"},
            {"format",ffr::quote("ffr-helper-abort-trial-status")},{"runtimeMode",ffr::quote("experimental-abort-test")},
            {"available",json_flag(abort_available)},{"openingBoundaryPolicy",ffr::quote("ordinary-first-menu-event-history")},
            {"commandWaitGate",json_object({{"available",json_flag(command_wait_installed.load())},
                {"sequence",std::to_string(wait_sequence)},{"sampleTime",std::to_string(wait_sample_time)},
                {"observations",std::to_string(wait_observations)},{"waiting",json_flag(wait_last_value)},
                {"overrides",std::to_string((wait_override_mask&1u)+((wait_override_mask>>1u)&1u))}})},
            {"nonce",ffr::quote(action.nonce)},{"trialSession",ffr::quote(action.session)},
            {"sequence",std::to_string(action.sequence)},{"phase",ffr::quote(ffr::phase_name(trial.phase()))},
            {"message",ffr::quote(abort_available?trial.message():abort_message)},{"probeTime",std::to_string(probe_time)},
            {"guard",guard},{"recovery",recovery},{"after",after_metadata},{"importProbe",imported},{"normalizationProbe",normalization_status(*this,lease)}});
    }
    write_atomic(connection/L"abort-status.json",status);
}

void install_abort_tick_hook() {
    if(!ffr::abort_profile::opening_boundary_reviewed && !ffr::abort_profile::detached_import_menu_reviewed
        && !ffr::abort_profile::normalization_field_reviewed) {
        std::lock_guard lock(observer->mutex);
        observer->abort_message="Opening command boundary is unreviewed; practice tick dispatch disabled";
        return;
    }
    const auto base=reinterpret_cast<std::uint64_t>(GetModuleHandleW(nullptr));
    for(const auto& profile:{std::pair{ffr::abort_profile::tick_rva,ffr::abort_profile::tick_sha256},
            std::pair{ffr::abort_profile::get_world_rva,ffr::abort_profile::get_world_sha256},
            std::pair{ffr::abort_profile::get_map_rva,ffr::abort_profile::get_map_sha256},
            std::pair{ffr::abort_profile::get_ui_rva,ffr::abort_profile::get_ui_sha256},
            std::pair{ffr::abort_profile::end_battle_rva,ffr::abort_profile::end_battle_sha256}}) {
        std::array<unsigned char,64> code{};
        if(!read_memory(base+profile.first,code.data(),code.size()))return;
        Sha256 hash;hash.add(code.data(),code.size());
        if(hash.finish()!=profile.second) {std::lock_guard lock(observer->mutex);observer->abort_message="Abort trial code profile mismatch; no tick hook installed";return;}
    }
    if(ffr::abort_profile::opening_boundary_reviewed) {
        const auto size=ffr::abort_profile::end_battle_full_size;
        if(!size || size>16384)return;
        std::vector<unsigned char> code(size);
        if(!read_memory(base+ffr::abort_profile::end_battle_rva,code.data(),code.size()))return;
        Sha256 hash;hash.add(code.data(),code.size());
        if(hash.finish()!=ffr::abort_profile::end_battle_full_sha256) {
            std::lock_guard lock(observer->mutex);observer->abort_message="Full EndBattle envelope mismatch; no tick hook installed";return;
        }
    }
    if(ffr::abort_profile::detached_import_menu_reviewed || ffr::abort_profile::normalization_field_reviewed)initialize_import_probe();
    if(ffr::abort_profile::opening_boundary_reviewed)install_command_wait_hook();
    tick_address=reinterpret_cast<void*>(base+ffr::abort_profile::tick_rva);void* trampoline{};
    if(MH_CreateHook(tick_address,reinterpret_cast<void*>(&abort_tick_hook),&trampoline)!=MH_OK)return;
    original_tick.store(reinterpret_cast<TickFunction>(trampoline),std::memory_order_release);
    if(MH_EnableHook(tick_address)!=MH_OK){MH_RemoveHook(tick_address);return;}
    tick_installed.store(true);
    std::lock_guard lock(observer->mutex);observer->dispatcher_available=true;
    observer->abort_available=ffr::abort_profile::opening_boundary_reviewed && command_wait_installed.load();
    observer->abort_message=observer->abort_available?"Experimental opening-abort trial available":
        (ffr::abort_profile::opening_boundary_reviewed?"Scoped command wait profile unavailable; abort disabled":
        "Opening abort remains unreviewed; only the separately reviewed detached-import idle menu is enabled");
}
void shutdown_abort_tick_hook() {
    shutdown_command_wait_hook();
    if(tick_installed.exchange(false))MH_DisableHook(tick_address);
}
