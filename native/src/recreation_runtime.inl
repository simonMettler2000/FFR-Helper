// Separate current-field recreation experiment, not rollback/restoration.
// The original opening, durable unchanged exit and current field stay bound
// through allocation. Normal engine entry preserves/uses its current effects;
// this path never reconstructs or claims equality of complete effect graphs.
bool recreation_field_ready(Observer* current,void* context,const std::shared_ptr<const Observer::Opening>& source,
                            const std::string& session,const std::string& hash,const ffr::FieldAnchor& entry_field,
                            std::uint64_t expected_root=0) {
    if(!source || source->session!=session || source->thread!=GetCurrentThreadId() || !source->field_value
        || source->map_unit_cache=="null" || current->quit.load() || current->expires.load()<seconds())return false;
    auto guard=probe_abort(context,source,session,hash);
    if(!guard.game_thread || !guard.matching_world || !guard.ordinary_encounter || guard.battle_active
        || !guard.field_ready || !guard.managers_released || guard.units!=0 || guard.waiting_jobs || guard.working_jobs)return false;
    {
        std::lock_guard lock(current->mutex);
        if(current->opening!=source || current->session!=session || current->opening_hash!=hash
            || current->recreation_deadline<seconds()
            || current->trial.phase()!=ffr::AbortPhase::manager_cleared || current->import_attempted
            || current->normalization_attempted || !untouched_history_locked(*current,*source)
            || !current->task_exit.result_task_observed || !current->task_exit.result_mode_observed
            || current->task_exit.uncertain || current->recovery_hash!=hash || current->after_hash!=hash
            || current->after_progression!=source->progression || current->trial.recovery_bytes()!=source->progression)return false;
    }
    if(!current->recreation_binding.ready() || file_hash(current->connection/L"abort-after.gvas")!=hash
        || file_hash(current->connection/L"abort-recovery.gvas")!=hash)return false;
    auto* root=root_getter();
    if(!root || (expected_root && reinterpret_cast<std::uint64_t>(root)!=expected_root))return false;
    const auto saved=ffr::capture_progression(read_memory,root_getter,save_memory,memory_free);
    if(!saved || *saved!=source->progression || root_getter()!=root)return false;
    const auto field=capture_live_field(reinterpret_cast<void*>(source->context_identity));
    const auto& old=*source->field_value;
    // Ordinary encounter entry/exit can settle the player's transform while
    // leaving progression unchanged. Bind the actual post-exit field separately;
    // never rewrite the original capture or treat this as position restoration.
    if(!field || entry_field.map_id!=old.map_id || entry_field.direction!=old.direction
        || field->map_id!=entry_field.map_id || field->position!=entry_field.position
        || field->direction!=entry_field.direction)return false;
    const auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto cache=ffr::capture_map_unit_cache(read_memory,source->context_identity,base+ffr::field_profile::map_storage_vtable_rva);
    if(!cache || ffr::to_json(*cache)!=source->map_unit_cache)return false;
    // Cache comparison covers exactly the recorded raw components and counts.
    // Nonzero field effects are ordinary engine state, not a permission to
    // restore them from incomplete metadata. No cache setter is called here.
    return root_getter()==root;
}

void run_recreation_trial_inner(Observer* current,void* context,const std::shared_ptr<const Observer::Opening>& opening,
                          const ffr::AbortGuard& guard,const std::optional<ffr::AbortRequest>& request,
                          std::int64_t now,float delta) {
    std::string phase;
    {std::lock_guard lock(current->mutex);phase=current->recreation_phase;}
    if(phase=="checking") {
        std::shared_ptr<const Observer::Opening> retained;std::string session,hash;std::uint64_t root{};
        bool matched=false;
        {
            std::lock_guard lock(current->mutex);retained=current->recreation_source;session=current->recreation_session;
            hash=current->recreation_hash;root=current->recreation_root;
            matched=request && request->phase=="prepare-recreate" && request->session==session
                && request->nonce==current->recreation_nonce && request->deadline==current->recreation_deadline
                && retained && request->sequence==retained->sequence && request->opening_hash==hash
                && now<=current->recreation_deadline;
        }
        const auto field=matched?capture_live_field(reinterpret_cast<void*>(retained->context_identity)):
            std::optional<ffr::FieldAnchor>{};
        if(!field || !recreation_field_ready(current,context,retained,session,hash,*field,root)) {
            std::lock_guard lock(current->mutex);current->recreation_phase="refused";
            current->recreation_message="Post-exit preparation expired or its world/progression/cache changed; no entry";return;
        }
        std::lock_guard lock(current->mutex);
        if(current->recreation_entry_field && ffr::to_json(*current->recreation_entry_field)==ffr::to_json(*field))
            ++current->recreation_field_stable_samples;
        else {current->recreation_entry_field=field;current->recreation_field_stable_samples=1;}
        if(current->recreation_field_stable_samples>=3) {
            current->recreation_phase="armed";
            current->recreation_message="Unchanged progression/cache and stable post-exit field bound across three game ticks; waiting for explicit commit";
        }
        return;
    }
    if(phase=="armed") {
        std::lock_guard lock(current->mutex);
        if(now>current->recreation_deadline || current->session!=current->recreation_session || current->opening!=current->recreation_source) {
            current->recreation_phase="refused";current->recreation_message="Recreation preparation expired or its source changed; no entry";return;
        }
    }
    if(phase=="returned") {
        std::lock_guard lock(current->mutex);
        const auto source=current->recreation_source;
        if(!source || current->session!=current->recreation_session || !opening
            || opening==source || opening->sequence!=source->sequence+1
            || opening->context_identity!=source->context_identity || opening->session!=source->session
            || opening->thread!=source->thread || opening->entry!=source->entry || opening->progression!=source->progression
            || !opening->field_value || !current->recreation_entry_field
            || ffr::to_json(*opening->field_value)!=ffr::to_json(*current->recreation_entry_field)) {
            current->recreation_phase="uncertain";current->recreation_message="Native entry returned without the expected independent child capture; no retry";return;
        }
        current->recreation_child_sequence=opening->sequence;
        if(guard.game_thread && guard.matching_world && guard.ordinary_encounter && guard.battle_active
            && guard.command_menu && guard.waiting_jobs==0 && guard.working_jobs==0
            && guard.opening_history_untouched && guard.command_abort_ready) {
            const auto latest=ffr::capture_progression(read_memory,root_getter,save_memory,memory_free);
            current->recreation_menu_progression_unchanged=latest && *latest==source->progression
                && reinterpret_cast<std::uint64_t>(root_getter())==current->recreation_root;
            if(!current->recreation_menu_progression_unchanged) {
                current->recreation_phase="uncertain";current->recreation_message="Child menu reached but current progression/root differs; no retry";return;
            }
            current->recreation_result.new_encounter_verified=true;current->recreation_phase="verified";
            current->recreation_message="Matching child capture and ordinary first command menu observed; current-field recreation only, no restoration";
        }else if(!std::isfinite(delta) || delta<0 || (current->recreation_active_seconds+=std::min(delta,0.25f))>30) {
            current->recreation_phase="uncertain";current->recreation_message="Fresh matching first menu was not verified within the active-game budget; no retry";
        }
        return;
    }
    if(!request)return;
    if(request->phase=="prepare-recreate" && phase=="idle") {
        {
            std::lock_guard lock(current->mutex);
            current->recreation_phase="checking";current->recreation_session=request->session;
            current->recreation_nonce=request->nonce;current->recreation_hash=request->opening_hash;
            current->recreation_deadline=request->deadline;current->recreation_source=opening;
        }
        const auto entry_field=opening?capture_live_field(reinterpret_cast<void*>(opening->context_identity)):
            std::optional<ffr::FieldAnchor>{};
        const bool matched=opening && entry_field && request->session==opening->session && request->sequence==opening->sequence
            && request->opening_hash==guard.opening_hash && request->deadline>=now && request->deadline<=now+8
            && recreation_field_ready(current,context,opening,request->session,request->opening_hash,*entry_field);
        const auto root=matched?reinterpret_cast<std::uint64_t>(root_getter()):0;
        std::lock_guard lock(current->mutex);
        current->recreation_entry_field=entry_field;
        current->recreation_field_stable_samples=matched&&root?1:0;
        current->recreation_root=root;current->recreation_phase=matched&&root?"checking":"refused";
        current->recreation_message=matched&&root?"Verifying unchanged post-exit field across consecutive game ticks":
            "Recreation requires the same verified untouched exit, unchanged recovery/progression, matching field/raw cache and consuming-entry profile";
        return;
    }
    if(request->phase=="commit-recreate" && phase=="armed") {
        std::shared_ptr<const Observer::Opening> retained;std::string session,hash;std::uint64_t root{};
        std::optional<ffr::FieldAnchor> entry_field;
        bool matched=false;
        {
            std::lock_guard lock(current->mutex);retained=current->recreation_source;session=current->recreation_session;
            hash=current->recreation_hash;root=current->recreation_root;
            entry_field=current->recreation_entry_field;
            matched=retained && request->session==session && request->nonce==current->recreation_nonce
                && request->sequence==retained->sequence && request->opening_hash==hash
                && request->deadline==current->recreation_deadline && now<=current->recreation_deadline;
        }
        if(!matched || !entry_field || !recreation_field_ready(current,context,retained,session,hash,*entry_field,root)) {
            std::lock_guard lock(current->mutex);current->recreation_phase="refused";
            current->recreation_message="Recreation commit or current field/progression changed; no entry";return;
        }
        {std::lock_guard lock(current->mutex);current->recreation_phase="invoking";}
        // No observer lock during entry: our ordinary capture hook must be able
        // to retain the child while this immutable source remains alive.
        auto result=current->recreation_binding.invoke(reinterpret_cast<void*>(retained->context_identity),retained->encounter,
            retained->thread,[&]{return recreation_field_ready(current,context,retained,session,hash,*entry_field,root);});
        std::lock_guard lock(current->mutex);current->recreation_result=std::move(result);
        current->recreation_phase=current->recreation_result.native_returned?"returned":
            current->recreation_result.attempted?"uncertain":"refused";
        current->recreation_message=current->recreation_result.message;current->recreation_active_seconds=0;
    }
}

void run_recreation_trial(Observer* current,void* context,const std::shared_ptr<const Observer::Opening>& opening,
                          const ffr::AbortGuard& guard,const std::optional<ffr::AbortRequest>& request,
                          std::int64_t now,float delta) {
    try {run_recreation_trial_inner(current,context,opening,guard,request,now,delta);}
    catch(...) {
        std::lock_guard lock(current->mutex);
        const bool uncertain=current->recreation_phase=="invoking" || current->recreation_result.attempted;
        if(uncertain)current->recreation_result.cleanup_verified=false;
        current->recreation_phase=uncertain?"uncertain":"refused";
        current->recreation_message=uncertain?"Recreation outcome is uncertain after native entry; no retry":
            "Recreation preflight failed before native entry; no retry";
    }
}

std::string recreation_status(const Observer& current) {
    // Caller holds its observer lock. No engine call occurs in publication.
    const auto& r=current.recreation_result;
    return json_object({{"operation",ffr::quote("same-setup-current-field")},{"restorable","false"},
        {"restoresInvoked","false"},{"rngRestored","false"},{"effectGraphsVerified","false"},{"fullCacheCoherenceVerified","false"},
        {"available",json_flag(current.recreation_binding.ready())},{"phase",ffr::quote(current.recreation_phase)},
        {"nonce",ffr::quote(current.recreation_nonce)},{"session",ffr::quote(current.recreation_session)},
        {"sourceSequence",std::to_string(current.recreation_source?current.recreation_source->sequence:0)},
        {"sourceHash",ffr::quote(current.recreation_hash)},{"deadline",std::to_string(current.recreation_deadline)},
        {"childSequence",std::to_string(current.recreation_child_sequence)},
        {"entryFieldAnchor",current.recreation_entry_field?ffr::to_json(*current.recreation_entry_field):"null"},
        {"entryAttempted",json_flag(r.attempted)},{"inputTransferred",json_flag(r.input_transferred)},
        {"nativeReturned",json_flag(r.native_returned)},{"cleanupVerified",json_flag(r.cleanup_verified)},
        {"newEncounterVerified",json_flag(r.new_encounter_verified)},
        {"firstMenuProgressionUnchanged",json_flag(current.recreation_menu_progression_unchanged)},
        {"message",ffr::quote(current.recreation_message)}});
}
