// Practice only. Let the existing ForceEnd check run instead of waiting for a
// player command, at two exact callsites during one committed abort. No command
// record, UI value, skill, queue, root or resource is written here.
using CommandWaitFunction=bool(__fastcall*)(void*);
std::atomic<CommandWaitFunction> original_command_wait{nullptr};
std::atomic<bool> command_wait_installed{false};
void* command_wait_address{};
std::uintptr_t command_wait_return{};
std::uintptr_t command_helper_return{};

bool untouched_history_locked(const Observer& current,const Observer::Opening& opening) {
    return current.decision_tracked_from_entry && current.decision_sequence==opening.sequence
        && current.decision_thread==GetCurrentThreadId() && current.decision_entries==0
        && !current.decision_foreign_thread && !current.decision_overflow
        && current.skill_tracked && current.skill_sequence==opening.sequence
        && current.skill_thread==GetCurrentThreadId() && current.skill_entries==0
        && !current.skill_foreign_thread && !current.skill_overflow;
}

bool __fastcall command_wait_hook(void* command) {
    const auto waiting=original_command_wait.load(std::memory_order_acquire)(command);
    const auto error=GetLastError();bool result=waiting;
    const auto returned=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    const unsigned site_bit=returned==command_wait_return?2u:returned==command_helper_return?1u:0u;
    if(site_bit) {
        try {
            auto* current=published_observer.load(std::memory_order_acquire);
            if(current && !current->quit.load()) {
                std::shared_ptr<const Observer::Opening> opening;
                std::string session,hash;bool pending=false;std::uint64_t tick_context{};const auto now=seconds();
                {
                    std::lock_guard lock(current->mutex);opening=current->opening;
                    if(opening && opening->thread==GetCurrentThreadId() && current->expires.load()>=now
                            && current->wait_sequence==opening->sequence) {
                        if(current->wait_observations<1000000)++current->wait_observations;
                        current->wait_sample_time=now;current->wait_last_value=waiting;current->wait_owner=reinterpret_cast<std::uint64_t>(command);
                        session=current->session;hash=current->opening_hash;
                        tick_context=current->last_tick_context;
                        pending=current->trial.phase()==ffr::AbortPhase::abort_sent && !(current->wait_override_mask&site_bit)
                            && tick_context>=0x10000
                            && current->probe_time>=now-2 && current->probe_time<=now+2;
                    }
                }
                if(waiting && pending) {
                    // State dispatch need not be nested in manager Tick. Recheck
                    // its privately retained context on the captured game thread.
                    const auto guard=probe_abort(reinterpret_cast<void*>(tick_context),opening,session,hash);
                    const auto manager=tick_context-0x30;
                    std::uint64_t owned_command{};std::uint8_t force_end{};ffr::RawArray requests{};
                    const auto pointer=reinterpret_cast<std::uint64_t>(command);
                    const bool matching=ffr::ordinary_idle_import_menu(guard) && valid_object(pointer)
                        && read_value(manager+0xb0,owned_command) && owned_command==pointer
                        && read_value(manager+0x222,force_end) && force_end==1
                        && read_value(pointer+0x28,requests) && ffr::valid_array(requests,64) && requests.count>0;
                    if(matching) {
                        std::lock_guard lock(current->mutex);const auto& active=current->trial.active_request();
                        if(current->opening==opening && current->session==session && current->opening_hash==hash
                                && active.session==session && active.sequence==opening->sequence && active.opening_hash==hash
                                && current->trial.phase()==ffr::AbortPhase::abort_sent && !(current->wait_override_mask&site_bit)
                                && current->expires.load()>=seconds() && untouched_history_locked(*current,*opening)) {
                            // Consume before changing the result. No reentrant
                            // call or later frame can repeat this override.
                            current->wait_override_mask|=site_bit;result=false;
                        }
                    }
                }
            }
        }catch(...) { /* Preserve the engine's original result on uncertainty. */ }
    }
    SetLastError(error);return result;
}

void install_command_wait_hook() {
    const auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    for(const auto& item:{std::tuple{ffr::command_wait_profile::gate_rva,ffr::command_wait_profile::gate_size,ffr::command_wait_profile::gate_sha256},
            std::tuple{ffr::command_wait_profile::state_rva,ffr::command_wait_profile::state_size,ffr::command_wait_profile::state_sha256},
            std::tuple{ffr::command_wait_profile::helper_rva,ffr::command_wait_profile::helper_size,ffr::command_wait_profile::helper_sha256}}) {
        const auto size=std::get<1>(item);if(!size||size>16384)return;
        std::vector<unsigned char> code(size);if(!read_memory(base+std::get<0>(item),code.data(),size))return;
        Sha256 hash;hash.add(code.data(),size);if(hash.finish()!=std::get<2>(item))return;
    }
    for(const auto& item:{std::tuple{ffr::command_wait_profile::callsite_rva,ffr::command_wait_profile::state_rva,ffr::command_wait_profile::state_size},
            std::tuple{ffr::command_wait_profile::helper_callsite_rva,ffr::command_wait_profile::helper_rva,ffr::command_wait_profile::helper_size}}) {
        const auto site=std::get<0>(item),start=std::get<1>(item);const auto size=std::get<2>(item);
        if(site<start || site-start+5>size)return;
        std::array<unsigned char,5> call{};std::int32_t displacement{};
        if(!read_memory(base+site,call.data(),call.size())||call[0]!=0xe8)return;
        std::memcpy(&displacement,call.data()+1,4);
        if(static_cast<std::intptr_t>(base+site+5)+displacement!=static_cast<std::intptr_t>(base+ffr::command_wait_profile::gate_rva))return;
    }
    command_wait_address=reinterpret_cast<void*>(base+ffr::command_wait_profile::gate_rva);void* trampoline{};
    if(MH_CreateHook(command_wait_address,reinterpret_cast<void*>(&command_wait_hook),&trampoline)!=MH_OK)return;
    original_command_wait.store(reinterpret_cast<CommandWaitFunction>(trampoline),std::memory_order_release);
    command_wait_return=base+ffr::command_wait_profile::callsite_rva+5;
    command_helper_return=base+ffr::command_wait_profile::helper_callsite_rva+5;
    if(MH_EnableHook(command_wait_address)!=MH_OK){MH_RemoveHook(command_wait_address);return;}
    command_wait_installed.store(true);
}
void shutdown_command_wait_hook() {
    if(command_wait_installed.exchange(false))MH_DisableHook(command_wait_address);
}
