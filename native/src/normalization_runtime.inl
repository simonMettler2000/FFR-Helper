// A distinct detached-object experiment. Never assigns the live root or invokes
// normal load, clock reset, battle abort/recreation or a game slot writer.
bool normalization_boundary(Observer* current,void* context,const std::shared_ptr<const Observer::Opening>& opening,const ffr::AbortGuard& guard) {
    if(!opening || !opening->clock_value || opening->clock_value->rounding_mode!=0 || opening->clock_value->suspended_start_seconds
        || _MM_GET_ROUNDING_MODE()!=_MM_ROUND_NEAREST || !recovery_field_boundary(context,opening,guard))return false;
    auto clock=capture_live_playtime(root_getter());
    return clock && clock->rounding_mode==0 && clock->suspended_start_seconds==0 && !current->quit.load();
}
void run_normalization_probe(Observer* current,void* context,const std::shared_ptr<const Observer::Opening>& opening,
                             const ffr::AbortGuard& guard,const std::optional<ffr::AbortRequest>& request,std::int64_t now) {
    bool available=false;
    {std::lock_guard lock(current->mutex);available=current->import_available;}
    const bool initialized=available && guard.game_thread && import_loader_initialized();
    const bool boundary=initialized && normalization_boundary(current,context,opening,guard);
    {std::lock_guard lock(current->mutex);current->normalization_loader_ready=initialized;current->normalization_field_ready=boundary;}
    if(!request || request->phase!="probe-normalize")return;
    bool permitted=false;
    {
        std::lock_guard lock(current->mutex);
        if(current->import_attempted || current->trial.phase()!=ffr::AbortPhase::idle)return;
        current->import_attempted=true;current->normalization_attempted=true;
        current->normalization_nonce=request->nonce;current->normalization_session=request->session;
        current->normalization_sequence=request->sequence;current->normalization_hash=request->opening_hash;current->normalization_time=now;
        permitted=boundary && opening && current->opening==opening && request->session==guard.session
            && request->sequence==guard.sequence && request->opening_hash==guard.opening_hash && request->deadline>=now && request->deadline<=now+8;
        current->normalization_phase=permitted?"running":"rejected";
        current->normalization_result.message=permitted?"Detached normalization running":"Normalization requires initialized loader, nearest-even clocks and a matching cleared field";
    }
    if(!permitted)return;
    auto* original=root_getter();
    auto ready=[&] {
        const auto fresh_time=seconds();
        if(current->quit.load() || current->expires.load()<fresh_time || request->deadline<fresh_time || !import_loader_initialized())return false;
        std::string session,hash;
        {std::lock_guard lock(current->mutex);if(current->opening!=opening || current->session!=request->session)return false;session=current->session;hash=current->opening_hash;}
        const auto fresh=probe_abort(context,opening,session,hash);
        return normalization_boundary(current,context,opening,fresh);
    };
    ffr::NormalizedCloneAPI api;api.objects=make_import_api(ready);
    const auto source_bits=std::bit_cast<std::uint32_t>(opening->clock_value->saved_seconds);
    api.write_owned_time=[&](void* clone,float value) {
        const auto address=reinterpret_cast<std::uint64_t>(clone);
        std::uint32_t flags{},before{};
        if(!ready() || !clone || clone==original || root_getter()!=original || !api.objects.same_save_class(clone,original)
            || !object_rooted(clone) || !read_value(address+8,flags)
            // Refuse class/archetype/default templates and destruction states.
            || (flags&0x04018030u) || !read_value(address+0x38,before) || before!=source_bits)return false;
        MEMORY_BASIC_INFORMATION info{};
        if(!VirtualQuery(reinterpret_cast<void*>(address+0x38),&info,sizeof(info)) || info.State!=MEM_COMMIT
            || (info.Protect&(PAGE_GUARD|PAGE_NOACCESS)) || !(info.Protect&(PAGE_READWRITE|PAGE_WRITECOPY)))return false;
        SIZE_T written{};
        return WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address+0x38),&value,sizeof(value),&written) && written==sizeof(value);
    };
    ffr::NormalizedCloneProbe probe;
    auto result=probe.run(opening->progression,*opening->clock_value,api);
    std::lock_guard lock(current->mutex);
    current->normalization_phase=!result.expected.empty() && !result.after.empty()?"pending-write":"failed";
    current->normalization_result=std::move(result);
}

void publish_normalization_files(Observer* current,const Lease& lease) {
    std::vector<unsigned char> expected,clone;std::string nonce;
    {
        std::lock_guard lock(current->mutex);
        if(current->normalization_phase!="pending-write" || current->normalization_files_done || current->normalization_session!=lease.session)return;
        expected=current->normalization_result.expected;clone=current->normalization_result.after;nonce=current->normalization_nonce;
    }
    bool saved=false;std::string expected_hash,clone_hash;
    try {
        Sha256 a;a.add(expected.data(),expected.size());expected_hash=a.finish();Sha256 b;b.add(clone.data(),clone.size());clone_hash=b.finish();
        const auto expected_path=current->connection/L"normalization-expected.gvas",clone_path=current->connection/L"normalization-clone.gvas";
        saved=write_atomic(expected_path,std::string_view(reinterpret_cast<const char*>(expected.data()),expected.size()),ffr::max_progression_bytes)
            && file_hash(expected_path)==expected_hash && write_atomic(clone_path,std::string_view(reinterpret_cast<const char*>(clone.data()),clone.size()),ffr::max_progression_bytes)
            && file_hash(clone_path)==clone_hash;
    }catch(...){saved=false;}
    std::lock_guard lock(current->mutex);
    if(current->normalization_nonce!=nonce || current->normalization_phase!="pending-write")return;
    current->normalization_files_done=true;current->normalization_files_ok=saved;
    if(saved){current->normalization_expected_hash=expected_hash;current->normalization_clone_hash=clone_hash;}
    current->normalization_phase=saved && current->normalization_result.passed?"passed":"failed";
    if(!saved)current->normalization_result.message="Detached comparison completed, but evidence files could not be committed; no retry";
}
std::string normalization_status(const Observer& current,const Lease& lease) {
    const bool same=current.normalization_session==lease.session;
    const auto& result=current.normalization_result;
    auto file=[&](const char* name,const std::string& hash,std::size_t bytes) {
        return same && current.normalization_files_ok?json_object({{"file",ffr::quote(name)},{"sha256",ffr::quote(hash)},{"byteLength",std::to_string(bytes)}}):"null";
    };
    std::ostringstream scalar;scalar.imbue(std::locale::classic());scalar<<std::setprecision(17)<<(same?result.normalized_seconds:0.0f);
    return json_object({{"operation",ffr::quote("detached-playtime-normalization")},{"restorable","false"},
        {"available",json_flag(current.import_available)},{"loaderReady",json_flag(current.normalization_loader_ready)},
        {"fieldReady",json_flag(current.normalization_field_ready)},{"attempted",json_flag(current.normalization_attempted || current.import_attempted)},
        {"phase",ffr::quote(same?current.normalization_phase:"idle")},{"nonce",ffr::quote(same?current.normalization_nonce:"")},
        {"session",ffr::quote(same?current.normalization_session:"")},{"openingHash",ffr::quote(same?current.normalization_hash:"")},
        {"sequence",std::to_string(same?current.normalization_sequence:0)},{"captureTime",std::to_string(same?current.normalization_time:0)},
        {"passed",json_flag(same && current.normalization_phase=="passed")},{"enginePassed",json_flag(same && result.passed)},
        {"sourceValid",json_flag(same && result.source_valid)},{"inputUnchanged",json_flag(same && result.input_unchanged)},
        {"cloneBeforeMatches",json_flag(same && result.clone_before_matches)},{"writeAttempted",json_flag(same && result.write_attempted)},
        {"writeReturned",json_flag(same && result.write_returned)},{"cloneAfterMatches",json_flag(same && result.clone_after_matches)},
        {"liveUnchanged",json_flag(same && result.live_unchanged)},{"rootChanged",json_flag(same && result.root_changed)},
        {"identityUncertain",json_flag(same && result.identity_uncertain)},{"pinsReleased",json_flag(!same || result.pins_released)},
        {"retainedOwnedPins",std::to_string(same?result.retained_owned_pins:0)},{"normalizedSeconds",scalar.str()},
        {"filesCommitted",json_flag(same && current.normalization_files_ok)},
        {"expected",file("normalization-expected.gvas",current.normalization_expected_hash,result.expected.size())},
        {"clone",file("normalization-clone.gvas",current.normalization_clone_hash,result.after.size())},
        {"message",ffr::quote(same?result.message:"No detached normalization requested")}});
}
