// Practice-only bookkeeping between independently verified opening recreations.
// This file does not restore progression, effects, clocks, RNG or save slots.
bool cycle_file_matches(const std::filesystem::path& path,const std::string& digest,std::uint64_t maximum) {
    // Keep a bounded, non-writable/non-deletable handle through readback. Do not
    // follow a substituted leaf junction/link or hash an unbounded replacement
    // on the game thread while deciding whether bookkeeping may rearm.
    const auto handle=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
    if(handle==INVALID_HANDLE_VALUE)return false;
    struct Close {HANDLE h;~Close(){CloseHandle(h);}} close{handle};
    BY_HANDLE_FILE_INFORMATION info{};LARGE_INTEGER size{};
    if(!GetFileInformationByHandle(handle,&info) || (info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))
        || !GetFileSizeEx(handle,&size) || size.QuadPart<=0 || static_cast<std::uint64_t>(size.QuadPart)>maximum)return false;
    std::array<unsigned char,32768> buffer{};Sha256 hash;std::uint64_t remaining=static_cast<std::uint64_t>(size.QuadPart);
    while(remaining) {
        const auto amount=static_cast<DWORD>(std::min<std::uint64_t>(remaining,buffer.size()));DWORD copied{};
        if(!ReadFile(handle,buffer.data(),amount,&copied,nullptr) || copied!=amount)return false;
        hash.add(buffer.data(),copied);remaining-=copied;
    }
    return hash.finish()==digest;
}

bool immutable_cycle_file(const std::filesystem::path& path,std::string_view bytes) {
    if(bytes.size()>ffr::max_progression_bytes)return false;
    auto handle=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr);
    if(handle!=INVALID_HANDLE_VALUE) {
        DWORD written{};
        const bool ok=WriteFile(handle,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)
            && written==bytes.size() && FlushFileBuffers(handle);
        CloseHandle(handle);if(!ok)return false;
    }else if(GetLastError()!=ERROR_FILE_EXISTS && GetLastError()!=ERROR_ALREADY_EXISTS)return false;
    const auto attributes=GetFileAttributesW(path.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES || (attributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY)))return false;
    std::error_code error;const auto size=std::filesystem::file_size(path,error);
    if(error || size!=bytes.size())return false;
    Sha256 hash;hash.add(bytes.data(),bytes.size());return cycle_file_matches(path,hash.finish(),bytes.size());
}

std::filesystem::path cycle_history_directory(const Observer& current,const std::string& session,std::uint64_t generation) {
    return current.connection/L"cycle-history"/std::filesystem::path(session)/(L"generation-"+std::to_wstring(generation));
}

void publish_cycle_history(Observer* current) {
    std::shared_ptr<const Observer::Opening> source,child;
    std::string session,hash,nonce,abort_nonce;std::uint64_t generation{};
    {
        std::lock_guard lock(current->mutex);
        generation=current->recreation_result.generation;
        if(!generation || current->cycle_archive_generation==generation || current->recreation_phase!="verified"
            || !current->recreation_result.new_encounter_verified || !current->recreation_result.cleanup_verified
            || !current->recreation_menu_progression_unchanged || !current->recreation_source || !current->opening)return;
        source=current->recreation_source;child=current->opening;session=current->recreation_session;
        hash=current->recreation_hash;nonce=current->recreation_nonce;abort_nonce=current->trial.active_request().nonce;
        if(child->sequence!=source->sequence+1 || child->progression!=source->progression
            || current->trial.recovery_bytes()!=source->progression || current->after_progression!=source->progression
            || current->recovery_hash!=hash || current->after_hash!=hash || child->entry!=source->entry)return;
    }
    const auto manifest=json_object({{"format",ffr::quote("ffr-helper-verified-opening-cycle")},{"protocol","1"},
        {"restorable","false"},{"generation",std::to_string(generation)},{"session",ffr::quote(session)},
        {"sourceSequence",std::to_string(source->sequence)},{"childSequence",std::to_string(child->sequence)},
        {"openingHash",ffr::quote(hash)},{"recreationNonce",ffr::quote(nonce)},{"abortNonce",ffr::quote(abort_nonce)},
        {"byteLength",std::to_string(source->progression.size())},{"entry",source->entry},
        {"sourceFieldAnchor",source->field_anchor},{"childFieldAnchor",child->field_anchor},
        {"progressionUnchanged","true"},{"childMenuVerified","true"},{"fullRestorationVerified","false"}});
    Sha256 manifest_hash;manifest_hash.add(manifest.data(),manifest.size());const auto digest=manifest_hash.finish();
    bool saved=false;
    try {
        const auto directory=cycle_history_directory(*current,session,generation);
        std::filesystem::create_directories(directory);
        saved=immutable_cycle_file(directory/L"progression.gvas",
            {reinterpret_cast<const char*>(source->progression.data()),source->progression.size()})
            && immutable_cycle_file(directory/L"manifest.json",manifest);
    }catch(...) {saved=false;}
    std::lock_guard lock(current->mutex);
    if(current->recreation_source!=source || current->opening!=child || current->recreation_phase!="verified"
        || current->recreation_result.generation!=generation || current->recreation_nonce!=nonce)return;
    if(saved) {current->cycle_archive_generation=generation;current->cycle_archive_hash=digest;current->cycle_archive_error.clear();}
    else current->cycle_archive_error="Immutable cycle evidence write/readback failed; rearm is unavailable";
}

bool verified_child_context(Observer* current,void* context,const std::shared_ptr<const Observer::Opening>& child,
                            const ffr::AbortRequest& request,std::uint64_t generation) {
    if(!child || request.phase!="rearm" || request.sequence!=child->sequence || request.session!=child->session
        || request.deadline<seconds() || request.deadline>seconds()+8 || current->expires.load()<seconds()
        || child->thread!=GetCurrentThreadId() || current->quit.load())return false;
    auto guard=probe_abort(context,child,request.session,request.opening_hash);
    if(!ffr::ordinary_idle_import_menu(guard) || !guard.command_abort_ready)return false;
    std::string archive_hash;std::uint64_t root{};
    {
        std::lock_guard lock(current->mutex);
        const auto source=current->recreation_source;const auto& result=current->recreation_result;
        if(current->opening!=child || current->session!=request.session || current->opening_hash!=request.opening_hash
            || !source || source==child || source->sequence+1!=child->sequence || source->entry!=child->entry
            || source->context_identity!=child->context_identity || source->thread!=child->thread
            || source->progression!=child->progression || current->recreation_child_sequence!=child->sequence
            || current->recreation_hash!=request.opening_hash || current->recreation_phase!="verified"
            || current->trial.phase()!=ffr::AbortPhase::manager_cleared || current->import_attempted || current->normalization_attempted
            || request.nonce==current->recreation_nonce || request.nonce==current->trial.active_request().nonce
            || !result.attempted || !result.input_transferred || !result.native_returned || !result.cleanup_verified
            || !result.new_encounter_verified || result.generation!=generation
            || !current->recreation_menu_progression_unchanged || !untouched_history_locked(*current,*child)
            || current->cycle_archive_generation!=generation || current->cycle_archive_hash.empty())return false;
        archive_hash=current->cycle_archive_hash;root=current->recreation_root;
    }
    if(!root || reinterpret_cast<std::uint64_t>(root_getter())!=root)return false;
    const auto progression=ffr::capture_progression(read_memory,root_getter,save_memory,memory_free);
    if(!progression || *progression!=child->progression || reinterpret_cast<std::uint64_t>(root_getter())!=root)return false;
    const auto directory=cycle_history_directory(*current,request.session,generation);
    if(!cycle_file_matches(directory/L"manifest.json",archive_hash,131072)
        || !cycle_file_matches(directory/L"progression.gvas",request.opening_hash,child->progression.size()))return false;
    std::lock_guard lock(current->mutex);
    return current->opening==child && current->session==request.session && current->expires.load()>=seconds()
        && current->recreation_phase=="verified" && current->recreation_result.generation==generation
        && untouched_history_locked(*current,*child);
}

void run_recreation_rearm(Observer* current,void* context,const std::shared_ptr<const Observer::Opening>& opening,
                          const ffr::AbortGuard& guard,const std::optional<ffr::AbortRequest>& request,std::int64_t now) {
    if(!request || request->phase!="rearm")return;
    std::uint64_t generation{};
    {
        std::lock_guard lock(current->mutex);
        if(current->rearm_request && current->rearm_request->session==request->session
            && current->rearm_request->nonce==request->nonce)return;
        current->rearm_request=request;current->rearm_phase="refused";
        current->rearm_message="Verified untouched child, durable prior-cycle evidence and fresh request are required";
        generation=current->recreation_result.generation;
    }
    if(!generation || current->recreation_binding.generation()!=generation
        || !verified_child_context(current,context,opening,*request,generation))return;
    if(!current->recreation_binding.rearm_verified_child(generation,opening->thread,
            [&]{return verified_child_context(current,context,opening,*request,generation);}))return;
    std::lock_guard lock(current->mutex);
    const bool accepted=current->trial.rearm_verified_child(*request,guard,now,[&] {
        return current->opening==opening && current->session==request->session && current->expires.load()>=now
            && current->recreation_phase=="verified" && current->recreation_result.generation==generation
            && current->recreation_binding.generation()==generation+1 && untouched_history_locked(*current,*opening);
    });
    if(!accepted) {current->rearm_message="Native bookkeeping advanced but abort transaction did not rearm; fresh launch required";return;}
    current->rearm_previous_generation=generation;current->rearm_generation=generation+1;
    current->rearm_phase="accepted";current->rearm_message="Verified child accepted; no gameplay action issued by rearm";
    current->task_exit={};current->prepared_task_targets.reset();current->after_progression.clear();
    current->after_hash.clear();current->recovery_hash.clear();current->abort_request.reset();
    current->recreation_source.reset();current->recreation_result={};current->recreation_phase="idle";
    current->recreation_message="Waiting for a fresh deliberate opening exit/recreation transaction";
    current->recreation_session.clear();current->recreation_nonce.clear();current->recreation_hash.clear();
    current->recreation_root=0;current->recreation_child_sequence=0;current->recreation_deadline=0;
    current->recreation_active_seconds=0;current->recreation_menu_progression_unchanged=false;
    current->recreation_entry_field.reset();current->recreation_field_stable_samples=0;
}

std::string rearm_status(const Observer& current) {
    const auto& r=current.rearm_request;
    return json_object({{"operation",ffr::quote("verified-child-rearm")},{"restorable","false"},
        {"supported",json_flag(current.recreation_binding.ready())},{"phase",ffr::quote(current.rearm_phase)},
        {"nonce",ffr::quote(r?r->nonce:"")},{"session",ffr::quote(r?r->session:"")},
        {"sequence",std::to_string(r?r->sequence:0)},{"openingHash",ffr::quote(r?r->opening_hash:"")},
        {"deadline",std::to_string(r?r->deadline:0)},{"generation",std::to_string(current.recreation_binding.generation())},
        {"completedGeneration",std::to_string(current.rearm_previous_generation)},
        {"archiveGeneration",std::to_string(current.cycle_archive_generation)},
        {"archiveSha256",ffr::quote(current.cycle_archive_hash)},{"archiveError",ffr::quote(current.cycle_archive_error)},
        {"message",ffr::quote(current.rearm_message)}});
}
