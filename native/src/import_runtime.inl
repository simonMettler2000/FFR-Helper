// Included only in the explicit experimental practice build. No live-root
// setter, load delegate, file import or restoration call is exposed here.
using LoadMemoryFunction=void*(__fastcall*)(const ffr::RawArray*);
using RootedFunction=bool(__fastcall*)(void*);
using RootingFunction=void(__fastcall*)(void*);
using InitializedFunction=const void*(__cdecl*)();
LoadMemoryFunction load_memory{};
RootedFunction object_rooted{};
RootingFunction object_add_root{},object_remove_root{};
InitializedFunction loader_initialized{};

bool import_loader_initialized() {
    if(!loader_initialized)return false;
    const auto address=reinterpret_cast<std::uint64_t>(loader_initialized());
    unsigned char value{};
    return read_value(address,value) && value==1;
}

void initialize_import_probe() {
    const auto base=reinterpret_cast<std::uint64_t>(GetModuleHandleW(nullptr));
    const auto loader=GetModuleHandleW(L"UE4SS.dll");
    if(!loader)return;
    std::array<unsigned char,64> code{};
    if(!read_memory(base+ffr::import_profile::load_memory_rva,code.data(),code.size()))return;
    Sha256 hash;hash.add(code.data(),code.size());
    if(hash.finish()!=ffr::import_profile::load_memory_sha256)return;
    auto resolve=[&](const char* name,std::uintptr_t rva)->void* {
        auto* address=reinterpret_cast<void*>(GetProcAddress(loader,name));
        return reinterpret_cast<std::uintptr_t>(address)==reinterpret_cast<std::uintptr_t>(loader)+rva?address:nullptr;
    };
    loader_initialized=reinterpret_cast<InitializedFunction>(resolve(ffr::import_profile::loader_ready_name,ffr::import_profile::loader_ready_rva));
    object_rooted=reinterpret_cast<RootedFunction>(resolve(ffr::import_profile::is_rooted_name,ffr::import_profile::is_rooted_rva));
    object_add_root=reinterpret_cast<RootingFunction>(resolve(ffr::import_profile::add_root_name,ffr::import_profile::add_root_rva));
    object_remove_root=reinterpret_cast<RootingFunction>(resolve(ffr::import_profile::remove_root_name,ffr::import_profile::remove_root_rva));
    if(!loader_initialized||!object_rooted||!object_add_root||!object_remove_root)return;
    load_memory=reinterpret_cast<LoadMemoryFunction>(base+ffr::import_profile::load_memory_rva);
    std::lock_guard lock(observer->mutex);observer->import_available=true;
}

ffr::CloneProbeAPI make_import_api(const std::function<bool()>& readiness) {
    ffr::CloneProbeAPI api;
    api.ready=readiness;
    api.current_root=[] {return root_getter();};
    api.same_save_class=[](void* a,void* b) {
        const auto aa=reinterpret_cast<std::uint64_t>(a),bb=reinterpret_cast<std::uint64_t>(b);
        const auto base=reinterpret_cast<std::uint64_t>(GetModuleHandleW(nullptr));
        std::uint64_t av{},bv{},ac{},bc{};
        return valid_object(aa)&&valid_object(bb)&&read_value(aa,av)&&read_value(bb,bv)
            && av==base+ffr::import_profile::save_vtable_rva && bv==av
            && read_value(aa+16,ac)&&read_value(bb+16,bc)&&ac==bc;
    };
    api.is_rooted=[](void* value){return object_rooted(value);};
    api.add_root=[](void* value){object_add_root(value);};
    api.remove_root=[](void* value){object_remove_root(value);};
    api.load_memory=[](const ffr::RawArray* value){return load_memory(value);};
    api.save_memory=[](void* object){return ffr::capture_progression(read_memory,[&]{return object;},save_memory,memory_free);};
    return api;
}

void run_import_probe(Observer* current,void* context,const std::shared_ptr<const Observer::Opening>& opening,
                      const ffr::AbortGuard& guard,const std::optional<ffr::AbortRequest>& request,std::int64_t now) {
    bool available=false;
    {std::lock_guard lock(current->mutex);available=current->import_available;}
    const bool initialized=available && guard.game_thread && import_loader_initialized();
    {std::lock_guard lock(current->mutex);current->import_loader_ready=initialized;}
    if(!request || request->phase!="probe-import")return;
    bool permitted=false;
    {
        std::lock_guard lock(current->mutex);
        if(current->import_attempted || current->trial.phase()!=ffr::AbortPhase::idle)return;
        current->import_attempted=true;current->import_nonce=request->nonce;current->import_session=request->session;
        current->import_sequence=request->sequence;current->import_hash=request->opening_hash;
        permitted=ffr::abort_profile::detached_import_menu_reviewed && initialized && opening && current->opening==opening && request->session==guard.session
            && request->sequence==guard.sequence && request->opening_hash==guard.opening_hash
            && request->deadline>=now && request->deadline<=now+8 && ffr::ordinary_idle_import_menu(guard);
        current->import_phase=permitted?"running":"rejected";
        current->import_result.message=permitted?"Detached import comparison running":"Import probe requires initialized loader and a matching ordinary idle command menu";
    }
    if(!permitted)return;
    // No observer lock across UObject/allocator calls. All calls below are on the
    // encounter's recorded game thread and use only exact-profile primitive ABIs.
    auto api=make_import_api([&] {
        const auto fresh=seconds();
        if(current->quit.load() || current->expires.load()<fresh || request->deadline<fresh || !import_loader_initialized())return false;
        {std::lock_guard lock(current->mutex);if(current->opening!=opening || current->session!=request->session || current->opening_hash!=request->opening_hash)return false;}
        return ffr::ordinary_idle_import_menu(probe_abort(context,opening,request->session,request->opening_hash));
    });
    auto result=ffr::probe_progression_clone(opening->progression,api);
    std::lock_guard lock(current->mutex);
    current->import_phase=result.passed?"passed":"failed";current->import_result=std::move(result);
}
