// No wait-result detour or direct game-field write. Use the game's native
// UI_BtlCmdManager::AbortCommand only after a verified own ForceEnd request.
std::atomic<bool> command_abort_profile_ready{false};
using NativeCommandAbort=void(__fastcall*)(void*);

void* command_object_world(std::uint64_t object) {
    std::uint64_t vtable{},method{};
    if(!valid_object(object)||!read_value(object,vtable)||!read_value(vtable+0x180,method))return nullptr;
    MEMORY_BASIC_INFORMATION info{};
    if(!VirtualQuery(reinterpret_cast<void*>(method),&info,sizeof(info)) || info.State!=MEM_COMMIT || info.Type!=MEM_IMAGE
        || info.AllocationBase!=GetModuleHandleW(nullptr) || (info.Protect&(PAGE_GUARD|PAGE_NOACCESS))
        || !(info.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return nullptr;
    // This is the exact UObject virtual slot used by the fingerprinted native
    // AbortCommand body. The subsystem's direct-outer getter is not assumed to
    // implement the nested UI command object's World lookup.
    return reinterpret_cast<ObjectGetter>(method)(reinterpret_cast<void*>(object));
}

bool command_abort_code_matches() {
    const auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    for(const auto& item:{std::tuple{ffr::command_abort_profile::abort_rva,ffr::command_abort_profile::abort_size,ffr::command_abort_profile::abort_sha256},
            std::tuple{ffr::command_abort_profile::getter_rva,ffr::command_abort_profile::getter_size,ffr::command_abort_profile::getter_sha256},
            std::tuple{ffr::command_abort_profile::state_a_rva,ffr::command_abort_profile::state_a_size,ffr::command_abort_profile::state_a_sha256},
            std::tuple{ffr::command_abort_profile::state_b_rva,ffr::command_abort_profile::state_b_size,ffr::command_abort_profile::state_b_sha256}}) {
        const auto size=std::get<1>(item);if(!size||size>16384)return false;
        std::vector<unsigned char> code(size);if(!read_memory(base+std::get<0>(item),code.data(),size))return false;
        Sha256 hash;hash.add(code.data(),size);if(hash.finish()!=std::get<2>(item))return false;
    }
    return true;
}

void* native_command_abort_target(void* context,const ffr::AbortGuard& guard) {
    if(!command_abort_profile_ready.load() || !ffr::ordinary_idle_import_menu(guard))return nullptr;
    const auto pointer=reinterpret_cast<std::uint64_t>(context),base=reinterpret_cast<std::uint64_t>(GetModuleHandleW(nullptr));
    if(pointer<0x10000)return nullptr;
    const auto manager=pointer-0x30;
    std::uint64_t vtable{},delegate{};std::array<std::uint8_t,4> flags{};ffr::RawArray requests{};
    if(!valid_object(manager)||!read_value(manager,vtable)||vtable!=base+ffr::abort_profile::manager_vtable_rva
        ||!read_memory(manager+0x220,flags.data(),flags.size()) || flags!=std::array<std::uint8_t,4>{1,0,0,0}
        ||!read_value(manager+0xb0,delegate)||!valid_object(delegate)
        ||!read_value(delegate+0x28,requests)||!ffr::valid_array(requests,64)||requests.count!=0)return nullptr;
    const auto world=reinterpret_cast<ObjectGetter>(base+ffr::abort_profile::get_world_rva)(reinterpret_cast<void*>(manager));
    if(!valid_object(reinterpret_cast<std::uint64_t>(world)))return nullptr;
    const auto ui=reinterpret_cast<std::uint64_t>(reinterpret_cast<ObjectGetter>(base+ffr::abort_profile::get_ui_rva)(world));
    std::uint64_t ui_vtable{},target{};
    if(!valid_object(ui)||!read_value(ui,ui_vtable)||ui_vtable!=base+ffr::command_abort_profile::ui_vtable_rva
        ||!read_value(ui+0x70,target)||!valid_object(target))return nullptr;
    // The full native getter proves ownership at UI+0x70; the command object's
    // own World must also match the already checked battle context.
    const auto target_world=command_object_world(target);
    std::uint64_t again{};
    return target_world==world && read_value(ui+0x70,again) && again==target?reinterpret_cast<void*>(target):nullptr;
}

bool untouched_history_locked(const Observer& current,const Observer::Opening& opening) {
    return current.decision_tracked_from_entry && current.decision_sequence==opening.sequence
        && current.decision_thread==GetCurrentThreadId() && current.decision_entries==0
        && !current.decision_foreign_thread && !current.decision_overflow
        && current.skill_tracked && current.skill_sequence==opening.sequence
        && current.skill_thread==GetCurrentThreadId() && current.skill_entries==0
        && !current.skill_foreign_thread && !current.skill_overflow;
}
