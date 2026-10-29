// Explicit read-only partial recovery in the separate practice DLL. No abort,
// import, root assignment, reset, disk save or restore function is invoked here.
bool recovery_mutation_pending(const Observer& current,std::int64_t now) {
    const auto phase=current.trial.phase();
    return (phase!=ffr::AbortPhase::idle && phase!=ffr::AbortPhase::manager_cleared) || current.import_attempted
        || (phase==ffr::AbortPhase::idle && current.abort_request && current.abort_request->deadline>=now);
}
bool recovery_field_boundary(void* context,const std::shared_ptr<const Observer::Opening>& opening,const ffr::AbortGuard& guard) {
    if(!opening || !opening->field_value || !opening->clock_value || !field_profile_ready || !playtime_profile_ready
        || guard.opening_hash.size()!=64
        || !guard.game_thread || !guard.matching_world || !guard.ordinary_encounter || !guard.field_ready || guard.battle_active
        || guard.units!=0 || guard.waiting_jobs!=0 || guard.working_jobs!=0)return false;
    const auto address=reinterpret_cast<std::uint64_t>(context);if(address<0x10000)return false;
    std::uint64_t units{},selected{};ffr::RawArray targets{};
    if(!read_value(address-0x30+0x48,units))return false;
    if(guard.managers_released) {
        std::uint64_t jobs{};
        if(units || !read_value(address-0x30+0x40,jobs) || jobs)return false;
    }else if(!valid_object(units) || !read_value(units+0x38,selected) || selected
        || !read_value(units+0x48,targets) || !ffr::valid_array(targets,64) || targets.count!=0)return false;
    auto field=capture_live_field(reinterpret_cast<void*>(opening->context_identity));
    return field && field->map_id==opening->field_value->map_id;
}
void run_recovery_probe(Observer* current,void* context,const std::shared_ptr<const Observer::Opening>& opening,
                        const ffr::AbortGuard& guard,std::int64_t now) {
    std::lock_guard lock(current->mutex);
    const bool pending=recovery_mutation_pending(*current,now);
    const bool field=recovery_field_boundary(context,opening,guard);
    current->recovery_boundary_ready=field && !pending;
    if(!current->recovery_request)return;
    current->recovery_controller.capture(*current->recovery_request,guard,field,pending,now,[&]()->std::optional<ffr::PartialRecovery> {
        if(!opening || !opening->field_value)return {};
        auto* root=root_getter();if(!root)return {};
        ffr::PartialRecoveryAPI api;
        api.field_ready=[&] {
            const auto fresh=probe_abort(context,opening,current->session,current->opening_hash);
            return !recovery_mutation_pending(*current,seconds()) && recovery_field_boundary(context,opening,fresh);
        };
        api.current_root=root_getter;
        api.serialize_current=[&] {
            return ffr::capture_progression(read_memory,[&]()->void* {return root_getter()==root?root:nullptr;},save_memory,memory_free);
        };
        api.field=[&]{return capture_live_field(reinterpret_cast<void*>(opening->context_identity));};
        api.clock=[&]{return capture_live_playtime(root);};
        return ffr::capture_partial_recovery(api,opening->field_value->map_id);
    });
}
void Observer::publish_recovery(const Lease& lease) {
    std::string raw;std::optional<ffr::RecoveryReadRequest> incoming;
    if(read_small(connection/L"recovery-request.txt",raw,512))incoming=ffr::parse_recovery_request(raw,lease.session,seconds());
    ffr::RecoveryReadReceipt copied;
    {std::lock_guard lock(mutex);if(incoming)recovery_request=std::move(incoming);copied=recovery_controller.receipt();}
    if(copied.phase=="pending-write" && copied.data && copied.request.session==lease.session) {
        bool saved=false;std::string metadata,digest;
        try {
            const auto& data=*copied.data;
            Sha256 hash;hash.add(data.progression.data(),data.progression.size());const auto progression_hash=hash.finish();
            const auto progression=json_object({{"file",ffr::quote("recovery-progression.gvas")},{"byteLength",std::to_string(data.progression.size())},{"sha256",ffr::quote(progression_hash)}});
            metadata=json_object({{"format",ffr::quote("ffr-helper-partial-recovery")},{"version","1"},{"kind",ffr::quote("diagnostic-field-recovery")},
                {"readOnly","true"},{"restorable","false"},{"gameSha256",ffr::quote(ffr::profile::game_sha256)},
                {"session",ffr::quote(copied.request.session)},{"request",ffr::quote(copied.request.nonce)},
                {"captureNumber",std::to_string(copied.number)},{"sourceSequence",std::to_string(copied.request.sequence)},
                {"sourceOpeningHash",ffr::quote(copied.request.opening_hash)},{"sourceMapId",std::to_string(data.field.map_id)},
                {"captureTime",std::to_string(copied.captured_at)},{"progression",progression},
                {"fieldAnchor",ffr::to_json(data.field)},{"playtimeBasis",ffr::to_json(data.clock)},
                {"coherence","{\"matchingSerializations\":2,\"stableFieldClockSamples\":3,\"rootIdentityUnchanged\":true,\"fieldBoundaryChecked\":true}"},
                {"missing","[\"Complete world/actor/event state\",\"RNG state\",\"Verified restore path\"]"}});
            Sha256 meta_hash;meta_hash.add(metadata.data(),metadata.size());digest=meta_hash.finish();
            const auto progression_path=connection/L"recovery-progression.gvas",metadata_path=connection/L"recovery-metadata.json";
            saved=metadata.size()<=192*1024
                && write_atomic(progression_path,std::string_view(reinterpret_cast<const char*>(data.progression.data()),data.progression.size()),ffr::max_progression_bytes)
                && file_hash(progression_path)==progression_hash && write_atomic(metadata_path,metadata,192*1024) && file_hash(metadata_path)==digest;
        }catch(...) {saved=false;}
        std::lock_guard lock(mutex);recovery_controller.published(copied,saved,digest,metadata.size());
    }
    std::string report;
    {std::lock_guard lock(mutex);
        const auto& receipt=recovery_controller.receipt();const bool same=receipt.request.session==lease.session;
        const bool boundary_bound=last_guard.session==lease.session && last_guard.sequence>0 && last_guard.opening_hash.size()==64;
        const bool captured=same && receipt.phase=="captured";
        const auto descriptor=captured?json_object({{"file",ffr::quote("recovery-metadata.json")},{"byteLength",std::to_string(receipt.metadata_bytes)},
            {"sha256",ffr::quote(receipt.metadata_hash)}}):"null";
        report=json_object({{"protocol","1"},{"version","1"},{"adapter",ffr::quote("ffr-helper-native-recovery-observer")},
            {"format",ffr::quote("ffr-helper-recovery-report")},{"kind",ffr::quote("diagnostic-observation")},
            {"readOnly","true"},{"restorable","false"},{"gameSha256",ffr::quote(ffr::profile::game_sha256)},
            {"session",ffr::quote(lease.session)},{"time",std::to_string(seconds())},
            {"available",json_flag(abort_available && field_profile_ready && playtime_profile_ready && !recovery_controller.exhausted())},
            {"fieldReady",json_flag(recovery_boundary_ready && boundary_bound)},{"boundaryTime",std::to_string(probe_time)},
            {"boundarySequence",std::to_string(boundary_bound?last_guard.sequence:0)},{"boundaryOpeningHash",ffr::quote(boundary_bound?last_guard.opening_hash:"")},
            {"request",ffr::quote(same?receipt.request.nonce:"")},{"sequence",std::to_string(same?receipt.request.sequence:0)},
            {"openingHash",ffr::quote(same?receipt.request.opening_hash:"")},{"captureNumber",std::to_string(same?receipt.number:0)},
            {"captureTime",std::to_string(same?receipt.captured_at:0)},{"phase",ffr::quote(same?receipt.phase:"idle")},
            {"message",ffr::quote(same?receipt.message:"No partial recovery requested")},{"metadata",descriptor}});
    }
    write_atomic(connection/L"recovery-report.json",report);
}
