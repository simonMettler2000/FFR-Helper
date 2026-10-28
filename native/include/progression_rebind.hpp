#pragma once
#include "progression_clone.hpp"
#include <memory>

namespace ffr {
struct ProgressionRebindAPI {
    CloneProbeAPI objects;
    // Future exact-profile adapter: invoke the normal native load path with a
    // scoped in-memory substitution. The core never writes a manager pointer.
    std::function<bool(void*)> native_load;
};
struct ProgressionRebindResult {
    bool native_attempted{},native_returned{},clone_matches{},input_unchanged{},recovery_matches{};
    bool root_matches{},bytes_match{},context_stable{},verified{},pins_released=true,passed{};
    bool root_changed_before_apply{};
    unsigned retained_owned_pins{};
    std::vector<unsigned char> after;
    std::string message="Progression rebind has not run";
};

// Callback-driven prerequisite, NOT connected to a game command. The owner must
// prove the exact game thread/profile, matching cleared field context, and a
// freshly committed/read-back recovery file before invoking this controller.
// One attempt per controller, including rejected/uncertain attempts. No rollback.
class ProgressionRebindController {
    bool attempted=false;
public:
    ProgressionRebindResult run(const std::vector<unsigned char>& target,const std::vector<unsigned char>& recovery,
                               bool recovery_durable,const ProgressionRebindAPI& api) {
        ProgressionRebindResult result;
        if(attempted) {result.message="Rebind was already attempted; no retry is allowed";return result;}
        attempted=true;
        if(!recovery_durable || !captured_progression_header(target) || !captured_progression_header(recovery)) {
            result.message="A verified captured target and durable progression recovery are required";return result;
        }
        void* original{};
        try {
            if(!api.objects.ready()) {result.message="Native load profile, thread or field context is unavailable";return result;}
            original=api.objects.current_root();
            if(!original || !api.objects.same_save_class(original,original)) {result.message="Current save class is unavailable";return result;}
        }catch(...) {result.message="Rebind prerequisites failed before application";return result;}
        ScopedObjectRoot original_pin(api.objects,original);
        std::unique_ptr<ScopedObjectRoot> target_pin;
        auto run=[&] {
            if(!original_pin.acquire()) {result.message="Could not retain the live progression object";return;}
            auto before=api.objects.save_memory(original);
            if(!before) {result.message="Could not verify live progression against recovery";return;}
            if(*before!=recovery) {result.message="Live progression changed after recovery preparation";return;}
            result.recovery_matches=true;
            auto input=target;
            const RawArray descriptor{reinterpret_cast<std::uint64_t>(input.data()),static_cast<std::int32_t>(input.size()),static_cast<std::int32_t>(input.size())};
            auto* clone=api.objects.load_memory(&descriptor);
            result.input_unchanged=input==target;
            if(api.objects.current_root()!=original) {
                result.root_changed_before_apply=true;result.message="Live root changed during detached import; application was not requested";return;
            }
            if(!clone || clone==original || !result.input_unchanged || !api.objects.same_save_class(clone,original)) {
                result.message="Target import did not produce a separate unchanged matching save object";return;
            }
            target_pin=std::make_unique<ScopedObjectRoot>(api.objects,clone);
            if(!target_pin->acquire()) {result.message="Could not retain the target progression object";return;}
            auto copied=api.objects.save_memory(clone);
            result.clone_matches=copied && *copied==target;
            if(!result.clone_matches) {result.message="Detached target failed byte-identical import verification";return;}
            if(api.objects.current_root()!=original) {
                result.root_changed_before_apply=true;result.message="Live root changed before application";return;
            }
            auto latest=api.objects.save_memory(original);
            if(!latest || *latest!=recovery || !api.objects.ready()) {
                result.message="Live progression and field context could not be revalidated";return;
            }
            // Mark BEFORE the potentially mutating callback. An exception may
            // mean partial application, so it must never trigger an auto-retry.
            result.native_attempted=true;
            result.native_returned=api.native_load(clone);
            result.context_stable=api.objects.ready();
            result.root_matches=api.objects.current_root()==clone;
            if(result.root_matches && api.objects.same_save_class(clone,original)) {
                auto after=api.objects.save_memory(clone);
                if(after)result.after=std::move(*after);
                result.bytes_match=result.after==target;
            }
            result.verified=result.native_returned && result.context_stable && result.root_matches && result.bytes_match;
            result.message=result.verified?"Native progression application matched its target; world restoration is not established"
                :"Native load was attempted but its outcome was not fully verified; no automatic retry or rollback";
        };
        try {run();}catch(...) {
            result.message=result.native_attempted?"Native load raised an error; application may be partial. No retry or rollback":
                "Rebind preflight raised an error; native load was not requested";
            if(!result.native_attempted) {
                try {result.root_changed_before_apply=api.objects.current_root()!=original;}
                catch(...) {result.root_changed_before_apply=true;}
            }
        }
        if((result.native_attempted && !result.verified) || result.root_changed_before_apply) {
            // Keep our own roots until process exit when native callbacks may
            // have changed ownership. Never unroot an engine-owned existing pin.
            result.retained_owned_pins=original_pin.retain_until_exit()?1u:0u;
            if(target_pin && target_pin->retain_until_exit())++result.retained_owned_pins;
            result.pins_released=result.retained_owned_pins==0;
        }else {
            if(target_pin)result.pins_released=target_pin->release();
            result.pins_released=original_pin.release() && result.pins_released;
        }
        result.passed=result.verified && result.pins_released;
        if(result.verified && !result.pins_released)result.message="Native application verified, but temporary pin release could not be verified; no retry";
        return result;
    }
};
} // namespace ffr
