// Read-only handler-entry telemetry. It never grants permission to dispatch a
// practice command and does not inspect or alter either native argument.
using DecisionFunction=void(__fastcall*)(void*,void*);
std::atomic<DecisionFunction> original_decision{nullptr};
std::atomic<bool> decision_installed{false};
void* decision_address{};

void __fastcall decision_hook(void* context,void* data) {
    const auto input_error=GetLastError();
    try {
        auto* current=published_observer.load(std::memory_order_acquire);
        if(current && !current->quit.load()) {
            std::lock_guard lock(current->mutex);
            if(current->expires.load()<seconds()) {
                current->decision_sequence=0;current->decision_tracked_from_entry=false;
            }else if(current->decision_sequence && current->decision_thread) {
                if(current->decision_thread!=GetCurrentThreadId())current->decision_foreign_thread=true;
                if(current->decision_entries<1000000)++current->decision_entries;
                else current->decision_overflow=true;
            }
        }
    }catch(...) { /* Failure cannot suppress or repeat the native call. */ }
    SetLastError(input_error);
    original_decision.load(std::memory_order_acquire)(context,data);
}

void install_decision_hook() {
    const auto base=reinterpret_cast<std::uint64_t>(GetModuleHandleW(nullptr));
    if constexpr(!ffr::decision_profile::handler_size || ffr::decision_profile::handler_size>16384)return;
    std::vector<unsigned char> code(ffr::decision_profile::handler_size);
    if(!read_memory(base+ffr::decision_profile::handler_rva,code.data(),code.size()))return;
    Sha256 hash;hash.add(code.data(),code.size());
    if(hash.finish()!=ffr::decision_profile::handler_sha256)return;
    decision_address=reinterpret_cast<void*>(base+ffr::decision_profile::handler_rva);
    void* trampoline{};
    if(MH_CreateHook(decision_address,reinterpret_cast<void*>(&decision_hook),&trampoline)!=MH_OK)return;
    original_decision.store(reinterpret_cast<DecisionFunction>(trampoline),std::memory_order_release);
    if(MH_EnableHook(decision_address)!=MH_OK){MH_RemoveHook(decision_address);return;}
    decision_installed.store(true,std::memory_order_release);
}
void shutdown_decision_hook() {
    if(decision_installed.exchange(false))MH_DisableHook(decision_address);
}
