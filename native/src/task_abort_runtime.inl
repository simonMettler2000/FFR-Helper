// Experimental binding for the actual ordinary command-input task. Compiled
// only by the separately named practice build, never the read-only observer.
// Uses existing engine EndCommand and finish-check functions; no result setter,
// fake command payload, direct step write or attack is exposed.
std::atomic<bool> task_abort_profile_ready{false};
std::atomic<unsigned> task_code_mismatch{0};
thread_local const char* task_abort_reason="Task target has not been sampled";
using TaskAbortTargets=ffr::TaskAbortTargets;
void* task_object_world(std::uint64_t object) {
    std::uint64_t vtable{},method{};
    if(!valid_object(object)||!read_value(object,vtable)||!read_value(vtable+0x180,method))return nullptr;
    MEMORY_BASIC_INFORMATION info{};
    if(!VirtualQuery(reinterpret_cast<void*>(method),&info,sizeof(info))||info.State!=MEM_COMMIT||info.Type!=MEM_IMAGE
        ||info.AllocationBase!=GetModuleHandleW(nullptr)||(info.Protect&(PAGE_GUARD|PAGE_NOACCESS))
        ||!(info.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return nullptr;
    return reinterpret_cast<ObjectGetter>(method)(reinterpret_cast<void*>(object));
}
bool untouched_history_locked(const Observer& current,const Observer::Opening& opening) {
    return current.decision_tracked_from_entry && current.decision_sequence==opening.sequence
        && current.decision_thread==GetCurrentThreadId() && current.decision_entries==0
        && !current.decision_foreign_thread && !current.decision_overflow
        && current.skill_tracked && current.skill_sequence==opening.sequence
        && current.skill_thread==GetCurrentThreadId() && current.skill_entries==0
        && !current.skill_foreign_thread && !current.skill_overflow;
}
bool task_abort_code_matches() {
    const auto base=reinterpret_cast<std::uint64_t>(GetModuleHandleW(nullptr));
    unsigned index=0;
    for(const auto& part:ffr::task_abort_profile::regions) {
        task_code_mismatch.store(++index);
        if(!part.size || part.size>16384)return false;
        std::vector<unsigned char> bytes(part.size);
        if(!read_memory(base+part.rva,bytes.data(),bytes.size()))return false;
        Sha256 hash;hash.add(bytes.data(),bytes.size());
        if(hash.finish()!=part.sha256)return false;
    }
    task_code_mismatch.store(0);return true;
}
std::uint64_t task_cached_class(std::uintptr_t rva) {
    const auto base=reinterpret_cast<std::uint64_t>(GetModuleHandleW(nullptr));
    std::uint64_t value{};
    return read_value(base+rva,value) && valid_object(value)?value:0;
}
bool task_class_is_a(std::uint64_t klass,std::uint64_t parent) {
    if(!valid_object(klass)||!valid_object(parent))return false;
    std::int32_t depth{},maximum{};std::uint64_t list{},ancestor{};
    return read_value(parent+0x38,depth) && depth>=0 && depth<=256
        && read_value(klass+0x38,maximum) && maximum>=depth && maximum<=256
        && read_value(klass+0x30,list) && read_value(list+static_cast<std::uint64_t>(depth)*8,ancestor)
        && ancestor==parent+0x30;
}
bool same_task_array(const ffr::RawArray& a,const ffr::RawArray& b) {
    return a.data==b.data && a.count==b.count && a.capacity==b.capacity;
}
struct TaskSubsystemRow {std::uint64_t klass,value,hash_links;};
static_assert(sizeof(TaskSubsystemRow)==24);
std::uint64_t task_find_subsystem(std::uint64_t instance,std::uint64_t klass) {
    if(!valid_object(instance)||!valid_object(klass))return 0;
    ffr::RawArray list{},after{};std::vector<TaskSubsystemRow> rows;std::uint64_t found{};
    if(!read_value(instance+0x108,list)||!ffr::copy_array(read_memory,list,512,rows))return 0;
    for(const auto& row:rows)if(row.klass==klass && row.value) {
        std::uint64_t actual{};
        if(found || !valid_object(row.value)||!read_value(row.value+0x10,actual)||actual!=klass)return 0;
        found=row.value;
    }
    return read_value(instance+0x108,after) && same_task_array(list,after)?found:0;
}
bool task_talk_quiet(std::uint64_t instance) {
    const auto klass=task_cached_class(ffr::task_abort_profile::talk_class_cache_rva);
    const auto subsystem=task_find_subsystem(instance,klass);
    std::uint64_t player{},again{};std::uint8_t finished{};
    if(!subsystem || !read_value(subsystem+0x58,player))return false;
    if(!player)return true;
    return valid_object(player) && read_value(player+0x150,finished) && finished==1
        && read_value(subsystem+0x58,again) && again==player;
}
struct TaskViewModelRow {std::uint64_t klass,auxiliary,value;};
static_assert(sizeof(TaskViewModelRow)==24);
std::optional<std::pair<std::uint64_t,std::uint64_t>> task_command_view_model(std::uint64_t instance) {
    const auto root_class=task_cached_class(ffr::task_abort_profile::root_vm_class_cache_rva);
    const auto vm_class=task_cached_class(ffr::task_abort_profile::command_vm_class_cache_rva);
    const auto subsystem=task_find_subsystem(instance,root_class);
    std::uint64_t registry{},again{},found{};
    ffr::RawArray list{},after{};std::vector<TaskViewModelRow> rows;
    if(!subsystem || !vm_class || !read_value(subsystem+0x30,registry)||!valid_object(registry)
        ||!read_value(registry+0x28,list)||!ffr::copy_array(read_memory,list,2048,rows))return {};
    for(const auto& row:rows)if(row.klass && row.value && task_class_is_a(row.klass,vm_class)) {
        std::uint64_t actual{};
        if(found || !valid_object(row.value)||!read_value(row.value+0x10,actual)||!task_class_is_a(actual,vm_class))return {};
        found=row.value;
    }
    if(!found || !read_value(registry+0x28,after)||!same_task_array(list,after)
        ||!read_value(subsystem+0x30,again)||again!=registry)return {};
    return std::pair{registry,found};
}
std::optional<TaskAbortTargets> task_abort_targets(void* context,const ffr::AbortGuard& guard,bool after_ui_end=false) {
    task_abort_reason="Task profile or ordinary idle menu unavailable";
    if(!task_abort_profile_ready.load()||!ffr::ordinary_idle_import_menu(guard))return {};
    const auto address=reinterpret_cast<std::uint64_t>(context),base=reinterpret_cast<std::uint64_t>(GetModuleHandleW(nullptr));
    if(address<0x10000)return {};
    TaskAbortTargets out;out.manager=address-0x30;
    std::uint64_t vtable{},delegate{};std::array<std::uint8_t,4> flags{};ffr::RawArray requests{};
    const std::array<std::uint8_t,4> expected{1,0,static_cast<std::uint8_t>(after_ui_end?1:0),0};
    task_abort_reason="Manager flags or command requests differ";
    if(!valid_object(out.manager)||!read_value(out.manager,vtable)||vtable!=base+ffr::abort_profile::manager_vtable_rva
        ||!read_memory(out.manager+0x220,flags.data(),flags.size())||flags!=expected
        ||!read_value(out.manager+0xb0,delegate)||!valid_object(delegate)
        ||!read_value(delegate+0x28,requests)||!ffr::valid_array(requests,64)||requests.count)return {};
    out.world=reinterpret_cast<std::uint64_t>(reinterpret_cast<ObjectGetter>(base+ffr::abort_profile::get_world_rva)(reinterpret_cast<void*>(out.manager)));
    task_abort_reason="Game instance or quiet talk subsystem unavailable";
    if(!valid_object(out.world)||!read_value(out.world+0x228,out.instance)||!valid_object(out.instance)
        ||!task_talk_quiet(out.instance))return {};
    const auto task_class=task_cached_class(ffr::task_abort_profile::task_class_cache_rva);
    out.task_manager=task_find_subsystem(out.instance,task_class);
    std::uint64_t begin{},update{},end{},again{};std::int32_t ordinal{};std::uint8_t turn_events_done{};
    task_abort_reason="Owned Turn task or exact input step2 differs";
    if(!out.task_manager || !read_value(out.task_manager+0x38,out.task)||!valid_object(out.task)
        ||!read_value(out.task,vtable)||vtable!=base+ffr::task_abort_profile::task_vtable_rva
        ||!read_value(out.task+0x28,out.step)||!valid_object(out.step)
        // The finish checker otherwise schedules turn-begin event work before
        // inspecting ForceEnd. Never run it before that native phase completed.
        ||!read_value(out.task+0xaa,turn_events_done)||turn_events_done!=1
        ||!read_value(out.step,vtable)||vtable!=base+ffr::task_abort_profile::step_vtable_rva
        ||!read_value(out.step+0xc0,ordinal)||ordinal!=2
        ||!read_value(out.step+0x30,begin)||begin!=base+ffr::task_abort_profile::step_begin_rva
        ||!read_value(out.step+0x60,update)||update!=base+ffr::task_abort_profile::empty_update_rva
        ||!read_value(out.step+0x90,end)||end)return {};
    // Unlike the VM's UI-only EndCommand, the finish checker invokes this
    // Task's own GetWorld. Validate that exact context before it can be called.
    task_abort_reason="Turn task virtual World differs";
    if(reinterpret_cast<std::uint64_t>(task_object_world(out.task))!=out.world)return {};
    task_abort_reason="Owned root-registry command view model unavailable";
    auto vm=task_command_view_model(out.instance);if(!vm)return {};
    out.registry=vm->first;out.command_vm=vm->second;
    std::uint8_t opened{};
    task_abort_reason="Command view-model type/open flag or retained task changed";
    if(!read_value(out.command_vm,vtable)||vtable!=base+ffr::task_abort_profile::command_vm_vtable_rva
        ||!read_value(out.command_vm+0x199,opened)||opened!=(after_ui_end?0:1)
        ||!read_value(out.task_manager+0x38,again)||again!=out.task
        ||!read_value(out.task+0x28,again)||again!=out.step)return {};
    task_abort_reason="Owned input task and command view model match";return out;
}

using NativeTaskExitResult=ffr::NativeTaskExitResult;
// still_safe must refresh the matching ordinary idle battle boundary, empty
// jobs and unchanged source/history; checking the event counters alone is not
// sufficient after a UI callback. It must not depend on the VM still being open.
template<class Publish,class StillSafe>
NativeTaskExitResult perform_native_task_exit(void* context,const ffr::AbortGuard& guard,
        const TaskAbortTargets& retained,Publish publish,StillSafe still_safe) {
    NativeTaskExitResult result;
    const auto base=reinterpret_cast<std::uint64_t>(GetModuleHandleW(nullptr));
    try {
        const auto fresh=task_abort_targets(context,guard);
        if(!fresh || *fresh!=retained || !still_safe() || !task_abort_code_matches()) {
            result.message="Task exit context changed before the first game call; nothing invoked";
            publish(result);return result;
        }
        result.end_issued=true;result.message="Requesting native ForceEnd once";publish(result);
        reinterpret_cast<EndBattleFunction>(base+ffr::abort_profile::end_battle_rva)();
        std::uint8_t flag{};
        result.end_flag_observed=read_value(retained.manager+0x222,flag) && flag==1;
        if(!result.end_flag_observed) {
            result.uncertain=true;result.message="Native ForceEnd flag was not verified; no UI end or finish evaluation. No retry";
            publish(result);return result;
        }
        result.ui_end_issued=true;result.message="Closing the owned command view model with native EndCommand";publish(result);
        reinterpret_cast<void(__fastcall*)(void*)>(base+ffr::task_abort_profile::end_command_rva)(reinterpret_cast<void*>(retained.command_vm));
        std::uint8_t opened=255;
        result.ui_closed_observed=read_value(retained.command_vm+0x199,opened) && opened==0;
        if(!result.ui_closed_observed) {
            result.uncertain=true;result.message="Native command view model did not confirm closure; no finish evaluation. No retry";
            publish(result);return result;
        }
        const auto after=task_abort_targets(context,guard,true);
        if(!after || *after!=retained || !still_safe() || !task_abort_code_matches()) {
            // EndCommand may already have released the task through a UI
            // notification. Never evaluate a different/stale task or wait for
            // a later opportunity to send a second mutation.
            result.message="Command UI closed; task/readiness changed. Observing only; no explicit finish evaluation";
            publish(result);return result;
        }
        result.finish_check_issued=true;result.message="Evaluating this same Turn task's native finish checker once";publish(result);
        result.finish_check_accepted=reinterpret_cast<bool(__fastcall*)(void*)>(base+ffr::task_abort_profile::finish_check_rva)(reinterpret_cast<void*>(retained.task));
        std::uint8_t mode=255;std::uint64_t task{},vtable{};
        result.result_mode_observed=read_value(retained.manager+0x1a8,mode) && mode==5;
        result.result_task_observed=result.finish_check_accepted && result.result_mode_observed
            && read_value(retained.task_manager+0x38,task) && task!=retained.task && valid_object(task)
            && read_value(task,vtable) && vtable==base+ffr::task_abort_profile::result_skip_vtable_rva;
        result.message=result.result_task_observed?"Native ForceEnd result task observed; field cleanup and progression comparison still required":
            "Finish evaluation returned without the expected ForceEnd result task; observing only. No retry";
        publish(result);return result;
    }catch(...) {
        result.uncertain=result.end_issued;
        result.message=result.end_issued?"Native task-exit outcome is uncertain after a game call. No retry":
            "Task-exit preparation failed before any game call";
        try {publish(result);}catch(...) {}
        return result;
    }
}
