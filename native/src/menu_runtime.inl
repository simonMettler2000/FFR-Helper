// Compiled only in the ordinary read-only observer. No request or mutation path.
using MenuTickFunction=void(__fastcall*)(void*,float);
using MenuObjectGetter=void*(__fastcall*)(void*);
std::atomic<MenuTickFunction> original_menu_tick{nullptr};
std::atomic<bool> menu_installed{false};
void* menu_address{};

template<class T> bool menu_read(std::uint64_t address,T& value) {
    return address && read_memory(address,&value,sizeof(value));
}
bool menu_object(std::uint64_t address) {
    std::array<std::uint64_t,4> header{};
    return address && read_memory(address,header.data(),sizeof(header)) && header[0] && header[2]
        && (header[1]&0x40000000u)==0;
}
std::string sample_menu(void* context,const Observer::Opening& opening) {
    if(opening.thread!=GetCurrentThreadId())return "null";
    const auto tick=reinterpret_cast<std::uint64_t>(context);
    if(tick<0x10000)return "null";
    const auto manager=tick-0x30,base=reinterpret_cast<std::uint64_t>(GetModuleHandleW(nullptr));
    std::uint64_t vtable{};std::uint8_t active{};
    if(!menu_object(manager)||!menu_read(manager,vtable)||vtable!=base+ffr::menu_profile::manager_vtable_rva
        ||!menu_read(manager+0x1a9,active)||active>1)return "null";
    auto get=[&](std::uintptr_t rva,void* object){return reinterpret_cast<MenuObjectGetter>(base+rva)(object);};
    auto* world=get(ffr::menu_profile::get_world_rva,reinterpret_cast<void*>(manager));
    if(!menu_object(reinterpret_cast<std::uint64_t>(world)))return "null";
    auto* map=get(ffr::menu_profile::get_map_rva,world);
    if(!menu_object(reinterpret_cast<std::uint64_t>(map))||opening.context_identity!=reinterpret_cast<std::uint64_t>(map))return "null";
    const auto ui=reinterpret_cast<std::uint64_t>(get(ffr::menu_profile::get_ui_rva,world));
    std::uint8_t phase{},during{};std::uint64_t jobs{},units{},selected{};
    if(!menu_object(ui)||!menu_read(ui+0x50,phase)||!menu_read(ui+0x80,during)||during>1
        ||!menu_read(manager+0x40,jobs)||!menu_read(manager+0x48,units))return "null";
    const auto& entry=opening.encounter;
    const bool ordinary=entry.event==-1 && entry.can_escape && !entry.skip_win_result && !entry.skip_lose_result
        && !entry.bounty && !entry.battle_test && entry.preemption==0 && !entry.skip_intro;
    auto flag=[](bool value){return value?"true":"false";};
    auto prefix=[&] {return "{\"kind\":\"read-only-menu-frame\",\"readOnly\":true,\"restorable\":false,\"boundaryReviewed\":false,\"atomicSnapshot\":false,"
        "\"openingSequence\":"+std::to_string(opening.sequence)+",\"sampleTime\":"+std::to_string(seconds())
        +",\"gameThread\":true,\"matchingWorld\":true,\"ordinaryEncounter\":"+flag(ordinary);};
    if(!jobs && !units) {
        // Verified teardown case: both owned managers are released, rather than
        // live objects containing empty arrays. Never infer this from one null.
        if(active || during)return "null";
        return prefix()+",\"battleActive\":false,\"commandMenu\":false,\"fieldReady\":true,\"managersReleased\":true,"
            "\"units\":0,\"waitingJobs\":0,\"workingJobs\":0,\"turnBeginnings\":-1}";
    }
    if(!menu_object(jobs)||!menu_object(units))return "null";
    ffr::RawArray waiting{},working{},unit_list{};
    if(!menu_read(jobs+0x28,waiting)||!menu_read(jobs+0x38,working)||!ffr::valid_array(waiting,1024)
        ||!ffr::valid_array(working,1024)||!menu_read(units+0x28,unit_list)||!ffr::valid_array(unit_list,64)
        ||!menu_read(units+0x38,selected))return "null";
    const auto& ids=opening.encounter.friend_ids;
    std::set<std::int32_t> expected(ids.begin(),ids.end()),found;
    if(expected.empty()||expected.size()!=ids.size())return "null";
    std::vector<std::uint64_t> actors;if(!ffr::copy_array(read_memory,unit_list,64,actors))return "null";
    bool selected_friendly=false;std::int32_t turns{};
    for(auto actor:actors) {
        std::uint64_t parameter{};std::int32_t id{},count{};
        if(!menu_object(actor)||!menu_read(actor+0x838,parameter)||!menu_object(parameter)||!menu_read(parameter+0xd0,id))return "null";
        if(!expected.count(id))continue;
        if(!found.insert(id).second||!menu_read(parameter+0x4dc,count)||count<0||count>100000)return "null";
        turns+=count;if(actor==selected)selected_friendly=true;
    }
    const bool all_friends=found==expected;
    return prefix()
        +",\"battleActive\":"+flag(active!=0)+",\"commandMenu\":"+flag(active && during==1 && phase==0 && selected_friendly && all_friends)
        +",\"fieldReady\":"+flag(during==0 && !active && unit_list.count==0 && waiting.count==0 && working.count==0)
        +",\"managersReleased\":false,\"units\":"+std::to_string(unit_list.count)+",\"waitingJobs\":"+std::to_string(waiting.count)
        +",\"workingJobs\":"+std::to_string(working.count)+",\"turnBeginnings\":"+std::to_string(all_friends?turns:-1)+'}';
}
void __fastcall menu_tick_hook(void* context,float delta) {
    original_menu_tick.load(std::memory_order_acquire)(context,delta);
    const auto error=GetLastError();
    try {
        auto* current=published_observer.load(std::memory_order_acquire);
        if(current && !current->quit.load() && current->expires.load()>=seconds()) {
            std::shared_ptr<const Observer::Opening> opening;
            {std::lock_guard lock(current->mutex);const auto tick=GetTickCount64();
                if(tick>=current->menu_sample_after){opening=current->opening;current->menu_sample_after=tick+100;}}
            if(opening) {
                auto sampled=sample_menu(context,*opening);
                std::lock_guard lock(current->mutex);
                if(current->opening==opening && current->session==opening->session && current->expires.load()>=seconds()) {
                    current->menu_sequence=opening->sequence;current->menu_frame=std::move(sampled);
                }
            }
        }
    }catch(...) { /* Observation cannot repeat the game tick. */ }
    SetLastError(error);
}
void install_menu_hook() {
    const auto base=reinterpret_cast<std::uint64_t>(GetModuleHandleW(nullptr));
    if constexpr(!ffr::menu_profile::tick_size || ffr::menu_profile::tick_size>16384)return;
    std::vector<unsigned char> code(ffr::menu_profile::tick_size);
    if(!read_memory(base+ffr::menu_profile::tick_rva,code.data(),code.size()))return;
    Sha256 tick_hash;tick_hash.add(code.data(),code.size());if(tick_hash.finish()!=ffr::menu_profile::tick_sha256)return;
    for(const auto& profile:{std::pair{ffr::menu_profile::get_world_rva,ffr::menu_profile::get_world_sha256},
            std::pair{ffr::menu_profile::get_map_rva,ffr::menu_profile::get_map_sha256},std::pair{ffr::menu_profile::get_ui_rva,ffr::menu_profile::get_ui_sha256}}) {
        std::array<unsigned char,64> bytes{};if(!read_memory(base+profile.first,bytes.data(),bytes.size()))return;
        Sha256 hash;hash.add(bytes.data(),bytes.size());if(hash.finish()!=profile.second)return;
    }
    menu_address=reinterpret_cast<void*>(base+ffr::menu_profile::tick_rva);void* trampoline{};
    if(MH_CreateHook(menu_address,reinterpret_cast<void*>(&menu_tick_hook),&trampoline)!=MH_OK)return;
    original_menu_tick.store(reinterpret_cast<MenuTickFunction>(trampoline),std::memory_order_release);
    if(MH_EnableHook(menu_address)!=MH_OK){MH_RemoveHook(menu_address);return;}
    menu_installed.store(true);
    observer->menu_ready.store(true);
}
void shutdown_menu_hook(){if(menu_installed.exchange(false))MH_DisableHook(menu_address);if(observer)observer->menu_ready.store(false);}
