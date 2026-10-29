// Observe only the exact Delegate.Battle.Skill.Begin producer's dispatch call.
// No FName decoding, event data reads, setter or practice request is involved.
using SkillDispatchFunction=void(__fastcall*)(void*,std::uint64_t,void*);
std::atomic<SkillDispatchFunction> original_skill_dispatch{nullptr};
std::atomic<bool> skill_dispatch_installed{false};
void* skill_dispatch_address{};
std::uintptr_t skill_return_address{};

void __fastcall skill_dispatch_hook(void* dispatcher,std::uint64_t name,void* data) {
    const auto error=GetLastError();
    if(reinterpret_cast<std::uintptr_t>(_ReturnAddress())==skill_return_address) {
        try {
            auto* current=published_observer.load(std::memory_order_acquire);
            if(current && !current->quit.load()) {
                std::lock_guard lock(current->mutex);
                if(current->expires.load()<seconds()) {current->skill_sequence=0;current->skill_tracked=false;}
                else if(current->skill_sequence && current->skill_thread) {
                    if(current->skill_thread!=GetCurrentThreadId())current->skill_foreign_thread=true;
                    if(current->skill_entries<1000000)++current->skill_entries;else current->skill_overflow=true;
                }
            }
        }catch(...) { /* Never suppress or replay the original event. */ }
    }
    SetLastError(error);
    original_skill_dispatch.load(std::memory_order_acquire)(dispatcher,name,data);
}
void install_skill_dispatch_hook() {
    const auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    for(const auto& item:{std::tuple{ffr::skill_event_profile::dispatcher_rva,ffr::skill_event_profile::dispatcher_size,ffr::skill_event_profile::dispatcher_sha256},
            std::tuple{ffr::skill_event_profile::producer_rva,ffr::skill_event_profile::producer_size,ffr::skill_event_profile::producer_sha256}}) {
        const auto size=std::get<1>(item);if(!size||size>16384)return;
        std::vector<unsigned char> bytes(size);if(!read_memory(base+std::get<0>(item),bytes.data(),size))return;
        Sha256 hash;hash.add(bytes.data(),size);if(hash.finish()!=std::get<2>(item))return;
    }
    const auto site=ffr::skill_event_profile::callsite_rva;
    if constexpr(site<ffr::skill_event_profile::producer_rva || site-ffr::skill_event_profile::producer_rva+5>ffr::skill_event_profile::producer_size)return;
    std::array<unsigned char,5> call{};std::int32_t displacement{};
    if(!read_memory(base+site,call.data(),call.size())||call[0]!=0xe8)return;
    std::memcpy(&displacement,call.data()+1,4);
    if(static_cast<std::intptr_t>(base+site+5)+displacement!=static_cast<std::intptr_t>(base+ffr::skill_event_profile::dispatcher_rva))return;
    skill_dispatch_address=reinterpret_cast<void*>(base+ffr::skill_event_profile::dispatcher_rva);void* trampoline{};
    if(MH_CreateHook(skill_dispatch_address,reinterpret_cast<void*>(&skill_dispatch_hook),&trampoline)!=MH_OK)return;
    original_skill_dispatch.store(reinterpret_cast<SkillDispatchFunction>(trampoline),std::memory_order_release);
    skill_return_address=base+site+5;
    if(MH_EnableHook(skill_dispatch_address)!=MH_OK){MH_RemoveHook(skill_dispatch_address);return;}
    skill_dispatch_installed.store(true);observer->skill_ready.store(true);
}
void shutdown_skill_dispatch_hook() {
    if(skill_dispatch_installed.exchange(false))MH_DisableHook(skill_dispatch_address);
    if(observer)observer->skill_ready.store(false);
}
