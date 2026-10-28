#pragma once
#include "progression_capture.hpp"
#include <cstring>
#include <functional>
#include <span>
#include <string_view>

namespace ffr {
// This is a type gate for an engine-produced, privately retained capture. It is
// deliberately not a general parser or permission to deserialize files supplied
// by a client. No disk path or payload is accepted by the native command.
inline bool captured_progression_header(std::span<const unsigned char> bytes) {
    if (bytes.size()<48 || bytes.size()>max_progression_bytes) return false;
    std::size_t position=0;
    auto number=[&]<class T>(T& value) {
        if (sizeof(T)>bytes.size()-position) return false;
        std::memcpy(&value,bytes.data()+position,sizeof(T));position+=sizeof(T);return true;
    };
    auto string=[&](std::string_view expected,bool compare) {
        std::int32_t count{};
        if (!number(count) || count<1 || count>256 || static_cast<std::size_t>(count)>bytes.size()-position) return false;
        const auto* start=reinterpret_cast<const char*>(bytes.data()+position);position+=count;
        if (start[count-1]!=0 || std::memchr(start,0,count-1)) return false;
        return !compare || std::string_view(start,count-1)==expected;
    };
    std::uint32_t magic{},version{},ue4{},ue5{},changelist{},format{},custom_count{};
    std::uint16_t major{},minor{},patch{};
    if (!number(magic)||magic!=0x53415647||!number(version)||version!=3
        ||!number(ue4)||!number(ue5)||ue5!=1017||!number(major)||major!=5
        ||!number(minor)||minor!=6||!number(patch)||patch!=1||!number(changelist)
        ||!string({},false)||!number(format)||format!=3||!number(custom_count)||custom_count>10000) return false;
    const auto custom_bytes=static_cast<std::size_t>(custom_count)*20;
    if(custom_bytes>bytes.size()-position)return false;
    position+=custom_bytes;
    if(!string("/Script/mucha.CPP_MuchaGameSaveData",true)||position>=bytes.size()||bytes[position++]!=0)return false;
    // An ordinary in-memory object has a None terminator followed by a zero u32.
    constexpr unsigned char footer[]{5,0,0,0,'N','o','n','e',0,0,0,0,0};
    return bytes.size()-position>=sizeof(footer)
        && std::memcmp(bytes.data()+bytes.size()-sizeof(footer),footer,sizeof(footer))==0;
}

struct CloneProbeAPI {
    std::function<bool()> ready;
    std::function<void*()> current_root;
    std::function<bool(void*,void*)> same_save_class;
    std::function<bool(void*)> is_rooted;
    std::function<void(void*)> add_root,remove_root;
    std::function<void*(const RawArray*)> load_memory;
    std::function<std::optional<std::vector<unsigned char>>(void*)> save_memory;
};
struct CloneProbeResult {
    bool passed{},clone_matches{},live_unchanged{},input_unchanged{},pins_released=true;
    std::string message="Import probe has not run";
    std::vector<unsigned char> clone;
};

// Uses the loader's verified primitive rooting API, never guessed UObject flag
// bits. A root that was already pinned by the engine is not ours to release.
class ScopedObjectRoot {
    const CloneProbeAPI& api;
    void* object;
    bool owned=false;
public:
    ScopedObjectRoot(const CloneProbeAPI& functions,void* value):api(functions),object(value){}
    ScopedObjectRoot(const ScopedObjectRoot&)=delete;
    ScopedObjectRoot& operator=(const ScopedObjectRoot&)=delete;
    bool acquire() {
        if(api.is_rooted(object))return true;
        owned=true; // Release even if an API call throws after setting its bit.
        api.add_root(object);
        return api.is_rooted(object);
    }
    bool release() noexcept {
        if(!owned)return true;
        owned=false; // One release attempt; retain a bounded pin on uncertainty.
        try {
            api.remove_root(object);
            if(api.is_rooted(object))return false;
            return true;
        } catch(...) {return false;}
    }
    ~ScopedObjectRoot() {if(owned)release();}
};

// A synchronous, game-thread-only import round trip. It creates a detached save
// UObject and discards it after comparison; it never assigns the live save root,
// calls a load delegate, ends a battle, reconstructs an encounter or writes a slot.
inline CloneProbeResult probe_progression_clone(const std::vector<unsigned char>& captured,const CloneProbeAPI& api) {
    CloneProbeResult result;
    if(!captured_progression_header(captured)) {result.message="Captured progression header is unsupported";return result;}
    try {
        if(!api.ready()) {result.message="Engine profile, thread or loader initialization is not ready";return result;}
        auto* original=api.current_root();
        if(!original || !api.same_save_class(original,original)) {result.message="Current save class is unavailable";return result;}
        ScopedObjectRoot original_pin(api,original);
        auto run=[&] {
            if(!original_pin.acquire()) {result.message="Could not retain the live save object";return;}
            auto before=api.save_memory(original);
            if(!before || !captured_progression_header(*before)) {result.message="Could not serialize the live recovery comparison";return;}
            // The game borrows this descriptor/buffer. It owns neither allocation.
            auto input=captured;
            const RawArray descriptor{reinterpret_cast<std::uint64_t>(input.data()),static_cast<std::int32_t>(input.size()),static_cast<std::int32_t>(input.size())};
            auto* clone=api.load_memory(&descriptor);
            result.input_unchanged=input==captured;
            auto compare_live=[&] {
                if(api.current_root()!=original)return false;
                auto after=api.save_memory(original);
                return after && *after==*before;
            };
            if(!clone || clone==original || !api.same_save_class(clone,original)) {
                result.live_unchanged=compare_live();result.message="Deserializer did not return a separate matching save object";return;
            }
            ScopedObjectRoot clone_pin(api,clone);
            try {
                if(!clone_pin.acquire()) result.message="Could not retain the imported comparison object";
                else {
                    auto serialized=api.save_memory(clone);
                    if(serialized)result.clone=std::move(*serialized);
                    result.clone_matches=result.clone==captured;
                    result.live_unchanged=compare_live();
                    result.passed=result.clone_matches && result.live_unchanged && result.input_unchanged;
                    result.message=result.passed?"Detached import round trip matched; live progression is unchanged"
                        :"Import comparison failed; no live save-root replacement was requested";
                }
            }catch(...) {result.message="Import comparison raised an error; no retry was attempted";}
            result.pins_released=clone_pin.release() && result.pins_released;
        };
        try {run();}catch(...) {result.message="Import probe raised an error; no retry was attempted";}
        result.pins_released=original_pin.release() && result.pins_released;
        result.passed=result.passed && result.pins_released;
        if(!result.pins_released)result.message="Import probe could not verify release of its object pins";
    }catch(...) {result.passed=false;result.message="Import probe prerequisites raised an error";}
    return result;
}
} // namespace ffr
