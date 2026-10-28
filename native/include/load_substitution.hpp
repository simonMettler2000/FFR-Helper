#pragma once
// One synchronous native-load routing scope. This does not assign a live root,
// deserialize, retain UObjects, invoke game functions or implement restoration.
#include <cstdint>
#include <mutex>
#include <optional>

namespace ffr {
inline constexpr std::int32_t memory_load_private_slot=-2147482477;
struct LoadCall {
    std::uintptr_t manager{},caller{};
    std::uint32_t thread{};
    std::int32_t slot{};
};
struct LoadSubstitutionConfig {
    std::uintptr_t manager{},target_root{},validity_caller{},reader_caller{};
    std::uint32_t thread{};
};
enum class LoadSubstitutionPhase {unused,armed,validated,substituted,failed,closed};
struct LoadSubstitutionView {
    LoadSubstitutionPhase phase{};
    unsigned validations{},readers{},denials{};
};
struct SubstituteRoot {std::uintptr_t root{};std::uint8_t status{};};

// The owner must keep the borrowed target alive (using the verified engine GC
// API) until close(), and restrict the entire invocation to the game thread.
// All hooks obtain actual caller/thread identities internally, never from IPC.
class ScopedLoadSubstitution {
    mutable std::mutex mutex;
    LoadSubstitutionConfig config;
    LoadSubstitutionView state;
    bool attempted=false,completed=false;
    bool matches(const LoadCall& call,std::uintptr_t caller) const {
        return config.manager && call.manager==config.manager && call.thread==config.thread
            && call.slot==memory_load_private_slot && call.caller==caller;
    }
public:
    bool arm(const LoadSubstitutionConfig& next) {
        std::lock_guard lock(mutex);
        if(attempted)return false;
        attempted=true;
        if(!next.manager||!next.target_root||!next.thread||!next.validity_caller||!next.reader_caller
            ||next.validity_caller==next.reader_caller) {state.phase=LoadSubstitutionPhase::failed;return false;}
        config=next;state.phase=LoadSubstitutionPhase::armed;return true;
    }
    std::optional<bool> validity(const LoadCall& call) {
        std::lock_guard lock(mutex);
        if(!matches(call,config.validity_caller))return {}; // Unrelated calls retain their original path.
        if(state.phase!=LoadSubstitutionPhase::armed) {++state.denials;state.phase=LoadSubstitutionPhase::failed;return false;}
        ++state.validations;state.phase=LoadSubstitutionPhase::validated;return true;
    }
    std::optional<SubstituteRoot> reader(const LoadCall& call,bool empty_valid_destination) {
        std::lock_guard lock(mutex);
        if(!matches(call,config.reader_caller))return {};
        if(state.phase!=LoadSubstitutionPhase::validated||!empty_valid_destination) {
            ++state.denials;state.phase=LoadSubstitutionPhase::failed;
            return SubstituteRoot{0,6}; // Verified ESaveLoadStatus::DeserializeFailed.
        }
        ++state.readers;state.phase=LoadSubstitutionPhase::substituted;
        return SubstituteRoot{config.target_root,0}; // Verified ESaveLoadStatus::Success.
    }
    LoadSubstitutionView view() const {std::lock_guard lock(mutex);return state;}
    bool close() {
        std::lock_guard lock(mutex);
        if(state.phase==LoadSubstitutionPhase::closed)return completed;
        completed=state.phase==LoadSubstitutionPhase::substituted;
        config={};state.phase=LoadSubstitutionPhase::closed;return completed;
    }
    ~ScopedLoadSubstitution() {close();}
};
} // namespace ffr
