namespace {
void run_recovery_fixture(const std::filesystem::path& connection,const std::wstring& mode) {
    const bool denied=mode==L"recovery-denied",stale=mode==L"recovery-stale",drift=mode==L"recovery-drift",
        write_failed=mode==L"recovery-write-failed",map_changed=mode==L"recovery-map-changed";
    object(fake_manager,reinterpret_cast<std::uint64_t>(ffr_test_btl_vtable));object(fake_jobs);object(fake_units);object(fake_ui);
    fake_manager[0x1a9]=1;fake_ui[0x80]=1;
    put(fake_manager,0x40,reinterpret_cast<std::uint64_t>(fake_jobs.data()));put(fake_manager,0x48,reinterpret_cast<std::uint64_t>(fake_units.data()));
    put(fake_jobs,0x28,ffr::RawArray{});put(fake_jobs,0x38,ffr::RawArray{});
    put(fake_units,0x28,ffr::RawArray{});put(fake_units,0x38,std::uint64_t{0});put(fake_units,0x48,ffr::RawArray{});
    const auto tick=reinterpret_cast<void(__fastcall*)(void*,float)>(GetProcAddress(GetModuleHandleW(nullptr),"ffr_test_abort_tick"));
    auto pump=[&]{tick(fake_manager.data()+0x30,0.25f);CHECK(GetLastError()==9876);};
    auto wait=[&](auto condition){wait_for([&]{pump();return condition();});};
    const std::string session(32,'a');
    auto lease=[&](char nonce){write(connection/L"observe.txt","FFR_HELPER_OBSERVER_V1\n"+session+'\n'+std::to_string(now()+8)+'\n'+std::string(32,nonce)+'\n');};
    lease('0');wait([&]{return !read(connection/L"observer-status.json").empty();});
    std::vector<std::int32_t> friends{1001,1002},enemies{11};
    ffr::RawEntry entry{};entry.friend_ids=array(friends);entry.enemy_ids=array(enemies);entry.event=-1;entry.can_escape=1;entry.escape_probability=50;
    ffr_test_encounter(fake_map.data(),&entry,false);
    const std::string key="\"openingHash\":\"";
    wait([&]{const auto text=read(connection/L"abort-status.json");const auto at=text.find(key);return at!=std::string::npos && text[at+key.size()]!='"';});
    const auto status=read(connection/L"abort-status.json"),hash=status.substr(status.find(key)+key.size(),64);
    lease('b');wait([&]{return read(connection/L"runtime-report.json").find("\"request\":\""+std::string(32,'b')+'"')!=std::string::npos;});
    write(connection/L"recovery-source-report.json",read(connection/L"runtime-report.json"));
    write(connection/L"recovery-source-progression.gvas",read(connection/L"opening-progression.gvas"));
    CHECK(serialized==1 && released==1);
    fake_manager[0x1a9]=0;fake_ui[0x80]=0;recovery_money=202;
    if(mode==L"recovery-released") {
        put(fake_manager,0x40,std::uint64_t{0});put(fake_manager,0x48,std::uint64_t{0});
    }
    std::vector<std::uint64_t> queued{1};if(denied)put(fake_jobs,0x28,array(queued));
    if(map_changed)fixture_map_id=1001;
    if(write_failed)CHECK(CreateDirectoryW((connection/L"recovery-metadata.json").c_str(),nullptr));
    auto send=[&](char nonce){write(connection/L"recovery-request.txt","FFR_HELPER_PARTIAL_RECOVERY_V1\n"+session+'\n'+std::string(32,nonce)+'\n'+std::to_string(now()+8)+'\n'+(stale?"2":"1")+'\n'+hash+'\n');};
    const auto expected=denied||stale||map_changed?"rejected":drift?"failed":write_failed?"write-failed":"captured";
    send('c');
    wait([&]{const auto text=read(connection/L"recovery-report.json");return text.find("\"request\":\""+std::string(32,'c')+'"')!=std::string::npos
        && text.find("\"phase\":\""+std::string(expected)+'"')!=std::string::npos;});
    write(connection/L"recovery-first-report.json",read(connection/L"recovery-report.json"));
    if(std::string(expected)=="captured") {
        write(connection/L"recovery-first-metadata.json",read(connection/L"recovery-metadata.json"));
        write(connection/L"recovery-first-progression.gvas",read(connection/L"recovery-progression.gvas"));
        CHECK(serialized==3 && released==3);recovery_money=303;send('c');
        for(unsigned i=0;i<8;++i){pump();std::this_thread::sleep_for(std::chrono::milliseconds(50));}
        CHECK(serialized==3 && read(connection/L"recovery-progression.gvas")==read(connection/L"recovery-first-progression.gvas"));
        lease('b');send('d');
        wait([&]{const auto text=read(connection/L"recovery-report.json");return text.find("\"request\":\""+std::string(32,'d')+'"')!=std::string::npos
            && text.find("\"phase\":\"captured\"")!=std::string::npos;});
        CHECK(serialized==5 && released==5);send('c');
        for(unsigned i=0;i<8;++i){pump();std::this_thread::sleep_for(std::chrono::milliseconds(50));}
        CHECK(serialized==5 && read(connection/L"recovery-report.json").find("\"request\":\""+std::string(32,'d')+'"')!=std::string::npos);
    }else CHECK(serialized==(denied||stale||map_changed?1u:3u) && released==serialized);
    CHECK(abort_calls==0 && clone_loads==0 && calls==1 && fake_manager[0x1a9]==0);
    write(connection/L"recovery-final-report.json",read(connection/L"recovery-report.json"));
}
}
