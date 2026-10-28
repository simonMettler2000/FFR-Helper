// A read-only request channel in the separate practice DLL. It never invokes
// abort/import/rebind commands. The verified native tick supplies the guard.
void run_combat_probe(Observer* current,void* tick_context,const ffr::AbortGuard& guard,std::int64_t now) {
    std::lock_guard lock(current->mutex);
    if(!current->combat_request)return;
    const auto pending=current->trial.phase()!=ffr::AbortPhase::idle||current->import_attempted
        ||(current->abort_request && current->abort_request->deadline>=now);
    current->combat_controller.capture(*current->combat_request,guard,pending,now,[&] {
        const auto address=reinterpret_cast<std::uint64_t>(tick_context);
        if(address<0x10000)return std::optional<ffr::CombatObservation>{};
        // ReadProcessMemory and owned C++ allocations only; no game calls,
        // object setters, serialization, hooks or callbacks occur in this copy.
        return ffr::capture_combat_observation(read_memory,address-0x30);
    });
}

void Observer::publish_combat(const Lease& lease) {
    std::string raw;
    std::optional<ffr::CombatReadRequest> incoming;
    if(read_small(connection/L"combat-request.txt",raw,512))incoming=ffr::parse_combat_request(raw,lease.session,seconds());
    std::string report;
    {
        std::lock_guard lock(mutex);
        if(incoming)combat_request=std::move(incoming);
        const auto& receipt=combat_controller.receipt();
        const bool same=receipt.request.session==lease.session;
        report=json_object({{"protocol","1"},{"version","1"},{"adapter",ffr::quote("ffr-helper-native-combat-observer")},
            {"format",ffr::quote("ffr-helper-combat-report")},{"kind",ffr::quote("diagnostic-observation")},
            {"readOnly","true"},{"restorable","false"},{"gameSha256",ffr::quote(ffr::profile::game_sha256)},
            {"session",ffr::quote(lease.session)},{"time",std::to_string(seconds())},{"available",json_flag(abort_available)},
            {"request",ffr::quote(same?receipt.request.nonce:"")},{"sequence",std::to_string(same?receipt.request.sequence:0)},
            {"openingHash",ffr::quote(same?receipt.request.opening_hash:"")},{"captureNumber",std::to_string(same?receipt.number:0)},
            {"captureTime",std::to_string(same?receipt.captured_at:0)},{"phase",ffr::quote(same?receipt.phase:"idle")},
            {"message",ffr::quote(same?receipt.message:"No combat observation requested")},{"observation",same?receipt.data:"null"}});
    }
    write_atomic(connection/L"combat-report.json",report);
}
