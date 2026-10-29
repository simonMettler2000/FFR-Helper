namespace {
void run_normalization_fixture(const std::filesystem::path& connection,const std::wstring& mode,HMODULE loader) {
    const bool unready=mode==L"normalize-unready",stale=mode==L"normalize-stale",write_failed=mode==L"normalize-write-failed";
    auto set_ready=reinterpret_cast<void(*)(bool)>(GetProcAddress(loader,"ffr_test_loader_set_ready"));CHECK(set_ready);
    auto root_count=reinterpret_cast<unsigned(*)()>(GetProcAddress(loader,"ffr_test_root_count"));CHECK(root_count);
    set_ready(!unready);
    object(fake_root,reinterpret_cast<std::uint64_t>(ffr_test_save_vtable));
    object(fake_clone,reinterpret_cast<std::uint64_t>(ffr_test_save_vtable));
    object(fake_manager,reinterpret_cast<std::uint64_t>(ffr_test_btl_vtable));object(fake_jobs);object(fake_units);object(fake_ui);
    fake_manager[0x1a9]=1;fake_ui[0x80]=1;
    put(fake_manager,0x40,reinterpret_cast<std::uint64_t>(fake_jobs.data()));put(fake_manager,0x48,reinterpret_cast<std::uint64_t>(fake_units.data()));
    put(fake_jobs,0x28,ffr::RawArray{});put(fake_jobs,0x38,ffr::RawArray{});
    put(fake_units,0x28,ffr::RawArray{});put(fake_units,0x38,std::uint64_t{});put(fake_units,0x48,ffr::RawArray{});
    const auto tick=reinterpret_cast<void(__fastcall*)(void*,float)>(GetProcAddress(GetModuleHandleW(nullptr),"ffr_test_abort_tick"));
    auto pump=[&]{tick(fake_manager.data()+0x30,0.25f);CHECK(GetLastError()==9876);};
    auto wait=[&](auto condition){wait_for([&]{pump();return condition();});};
    const std::string session(32,'a'),nonce(32,'c');
    auto lease=[&](char request){write(connection/L"observe.txt","FFR_HELPER_OBSERVER_V1\n"+session+'\n'+std::to_string(now()+8)+'\n'+std::string(32,request)+'\n');};
    lease('0');wait([&]{return !read(connection/L"observer-status.json").empty();});
    std::vector<std::int32_t> friends{1001,1002},enemies{11};
    ffr::RawEntry entry{};entry.friend_ids=array(friends);entry.enemy_ids=array(enemies);entry.event=-1;entry.can_escape=1;entry.escape_probability=50;
    ffr_test_encounter(fake_map.data(),&entry,false);
    const std::string key="\"openingHash\":\"";
    wait([&]{const auto text=read(connection/L"abort-status.json");const auto at=text.find(key);return at!=std::string::npos && text[at+key.size()]!='"';});
    const auto status=read(connection/L"abort-status.json"),hash=status.substr(status.find(key)+key.size(),64);
    lease('b');wait([&]{return read(connection/L"runtime-report.json").find("\"request\":\""+std::string(32,'b')+'"')!=std::string::npos;});
    write(connection/L"normalization-source-report.json",read(connection/L"runtime-report.json"));
    write(connection/L"normalization-source-progression.gvas",read(connection/L"opening-progression.gvas"));
    CHECK(serialized==1 && released==1 && clone_loads==0 && root_count()==0);
    // Simulate a normal encounter exit with genuinely different live progression.
    fake_manager[0x1a9]=0;fake_ui[0x80]=0;recovery_money=202;put(fake_root,0x38,2000.5f);
    if(write_failed)CHECK(CreateDirectoryW((connection/L"normalization-clone.gvas").c_str(),nullptr));
    auto send=[&](const char* phase){write(connection/L"abort-request.txt","FFR_HELPER_ABORT_OPENING_V1\n"+session+'\n'+nonce+'\n'+std::to_string(now()+8)+'\n'+(stale?"2":"1")+'\n'+hash+'\n'+phase+'\n');};
    send("probe-normalize");
    const std::string outcome=unready||stale?"rejected":normalization_template||write_failed?"failed":"passed";
    wait([&]{const auto text=read(connection/L"abort-status.json");return text.find("\"operation\":\"detached-playtime-normalization\"")!=std::string::npos
        && text.find("\"phase\":\""+outcome+'"')!=std::string::npos;});
    CHECK(clone_loads==(unready||stale?0u:1u) && root_count()==0 && abort_calls==0 && calls==1 && serialized==released);
    float live{},cloned{};std::memcpy(&live,fake_root.data()+0x38,4);std::memcpy(&cloned,fake_clone.data()+0x38,4);
    CHECK(live==2000.5f && recovery_money==202);
    if(!unready && !stale)CHECK(cloned==(normalization_template?1000.25f:1970.25f));
    const auto serials=serialized;
    send("probe-normalize");for(unsigned i=0;i<6;++i){pump();std::this_thread::sleep_for(std::chrono::milliseconds(50));}
    send("probe-import");for(unsigned i=0;i<6;++i){pump();std::this_thread::sleep_for(std::chrono::milliseconds(50));}
    send("prepare");for(unsigned i=0;i<6;++i){pump();std::this_thread::sleep_for(std::chrono::milliseconds(50));}
    CHECK(serialized==serials && abort_calls==0 && clone_loads==(unready||stale?0u:1u) && root_count()==0);
    write(connection/L"normalization-final-status.json",read(connection/L"abort-status.json"));
}
}
