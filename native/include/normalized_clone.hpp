#pragma once
#include "progression_scalar.hpp"
#include <memory>

namespace ffr {
struct NormalizedCloneAPI {
    CloneProbeAPI objects;
    // Future profiled adapter: fixed native Player.PlayTime member of the
    // verified detached object only. Never accept a member address from IPC.
    std::function<bool(void*,float)> write_owned_time;
};
struct NormalizedCloneResult {
    bool source_valid{},input_unchanged{},clone_before_matches{},write_attempted{},write_returned{},clone_after_matches{};
    bool live_unchanged{},root_changed{},identity_uncertain{},pins_released=true,passed{};
    unsigned retained_owned_pins{};
    float normalized_seconds{};
    std::vector<unsigned char> expected,after;
    std::string message="Detached normalization has not run";
};

// Prerequisite only, not connected to a game command. No live-root assignment,
// normal load, reset, abort, encounter or slot writer exists in this controller.
class NormalizedCloneProbe {
    bool attempted=false;
public:
    NormalizedCloneResult run(const std::vector<unsigned char>& source,const PlaytimeBasis& basis,const NormalizedCloneAPI& api) {
        NormalizedCloneResult result;
        if(attempted){result.message="Detached normalization was already attempted; no retry";return result;}
        attempted=true;
        auto plan=prepare_normalized_progression(source,basis);
        if(!plan){result.message="Source scalar, clock or rounding coverage is unsupported";return result;}
        result.source_valid=true;result.expected=plan->bytes;result.normalized_seconds=plan->clock.saved_after_normal_load;
        void* original{};
        try {
            if(!api.objects.ready()){result.message="Engine/profile/thread/field prerequisites are unavailable";return result;}
            original=api.objects.current_root();
            if(!original || !api.objects.same_save_class(original,original)){result.message="Current save class is unavailable";return result;}
        }catch(...){result.message="Detached normalization prerequisites failed";return result;}
        ScopedObjectRoot original_pin(api.objects,original);
        std::unique_ptr<ScopedObjectRoot> clone_pin;
        std::optional<std::vector<unsigned char>> before;
        auto run=[&] {
            if(!original_pin.acquire()){result.message="Could not retain live progression for comparison";return;}
            before=api.objects.save_memory(original);
            if(!before || !captured_progression_header(*before)){result.message="Live progression comparison is unavailable";return;}
            auto input=source;
            const RawArray descriptor{reinterpret_cast<std::uint64_t>(input.data()),static_cast<std::int32_t>(input.size()),static_cast<std::int32_t>(input.size())};
            auto* clone=api.objects.load_memory(&descriptor);result.input_unchanged=input==source;
            if(clone && clone!=original && api.objects.same_save_class(clone,original)) {
                clone_pin=std::make_unique<ScopedObjectRoot>(api.objects,clone);
                if(!clone_pin->acquire()){result.message="Could not retain the detached clone";return;}
            }
            if(api.objects.current_root()!=original){result.root_changed=true;result.message="Live root changed during import; no property write requested";return;}
            if(!clone_pin || !result.input_unchanged){result.message="Import did not produce an unchanged separate matching clone";return;}
            auto cloned_before=api.objects.save_memory(clone);
            result.clone_before_matches=cloned_before && *cloned_before==source;
            if(!result.clone_before_matches || !api.objects.ready()){result.message="Detached source comparison or field gate failed";return;}
            auto latest=api.objects.save_memory(original);
            if(!latest || *latest!=*before || api.objects.current_root()!=original){result.message="Live progression changed before the detached write";return;}
            if(plan->original.bits!=std::bit_cast<std::uint32_t>(result.normalized_seconds)) {
                result.write_attempted=true;result.write_returned=api.write_owned_time(clone,result.normalized_seconds);
            }else result.write_returned=true; // No effective change is required.
            auto after=api.objects.save_memory(clone);
            if(after)result.after=std::move(*after);
            result.clone_after_matches=result.after==result.expected;
            result.message="Detached normalized clone compared; no live load or clock reset was requested";
        };
        try {run();}catch(...){result.message="Detached normalization raised an error; no retry";}
        try {
            result.root_changed=result.root_changed || api.objects.current_root()!=original;
            if(!result.root_changed && before) {
                auto after=api.objects.save_memory(original);
                result.live_unchanged=after && *after==*before && api.objects.ready();
            }
        }catch(...){result.identity_uncertain=true;result.live_unchanged=false;}
        if(result.root_changed || result.identity_uncertain) {
            result.retained_owned_pins=original_pin.retain_until_exit()?1u:0u;
            if(clone_pin && clone_pin->retain_until_exit())++result.retained_owned_pins;
            result.pins_released=result.retained_owned_pins==0;
            result.message="Live root changed or its comparison became uncertain; owned pins retained until exit, no retry or load";
        }else {
            if(clone_pin)result.pins_released=clone_pin->release();
            result.pins_released=original_pin.release() && result.pins_released;
        }
        result.passed=result.source_valid && result.input_unchanged && result.clone_before_matches && result.write_returned
            && result.clone_after_matches && result.live_unchanged && result.pins_released && !result.root_changed && !result.identity_uncertain;
        if(!result.passed && !result.root_changed && !result.identity_uncertain && result.message=="Detached normalized clone compared; no live load or clock reset was requested")
            result.message="Detached normalization comparison or cleanup failed; no live load requested";
        return result;
    }
};
} // namespace ffr
