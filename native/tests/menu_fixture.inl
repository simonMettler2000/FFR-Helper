namespace {
void run_menu_fixture(const std::filesystem::path& connection,bool available) {
    object(fake_manager,reinterpret_cast<std::uint64_t>(ffr_test_btl_vtable));
    object(fake_jobs);object(fake_units);object(fake_ui);
    fake_manager[0x1a9]=1;fake_ui[0x80]=1;
    put(fake_manager,0x40,reinterpret_cast<std::uint64_t>(fake_jobs.data()));
    put(fake_manager,0x48,reinterpret_cast<std::uint64_t>(fake_units.data()));
    std::vector<std::uint64_t> actors;
    for(unsigned i=0;i<2;++i) {
        object(fake_actors[i]);object(fake_parameters[i]);
        put(fake_actors[i],0x838,reinterpret_cast<std::uint64_t>(fake_parameters[i].data()));
        put(fake_parameters[i],0xd0,std::int32_t{1001+static_cast<std::int32_t>(i)});
        put(fake_parameters[i],0x4dc,std::int32_t{i==0?1:0});
        actors.push_back(reinterpret_cast<std::uint64_t>(fake_actors[i].data()));
    }
    put(fake_units,0x28,array(actors));put(fake_units,0x38,actors[0]);
    auto lease=[&]{write(connection/L"observe.txt","FFR_HELPER_OBSERVER_V1\n"+std::string(32,'a')+"\n"+std::to_string(now()+8)+"\n"+std::string(32,'0')+"\n");};
    lease();wait_for([&]{return !read(connection/L"observer-status.json").empty();});
    std::vector<std::int32_t> friends{1001,1002};ffr::RawEntry entry{};entry.friend_ids=array(friends);entry.event=-1;entry.can_escape=1;
    ffr_test_encounter(fake_map.data(),&entry,false);
    wait_for([&]{return !read(connection/L"opening-progression.gvas").empty();});
    const auto source=read(connection/L"opening-progression.gvas");
    if(!available) {
        CHECK(read(connection/L"observer-status.json").find("\"menuObservation\":null")!=std::string::npos);
        CHECK(serialized==1 && calls==1);return;
    }
    auto tick=reinterpret_cast<void(__fastcall*)(void*,float)>(GetProcAddress(GetModuleHandleW(nullptr),"ffr_test_abort_tick"));
    auto pump=[&]{const auto count=tick_calls;tick(fake_manager.data()+0x30,0.125f);CHECK(tick_calls==count+1 && last_delta==0.125f && GetLastError()==9876);};
    auto wait=[&](auto test){wait_for([&]{lease();pump();return test();});};
    wait([&]{return read(connection/L"observer-status.json").find("\"commandMenu\":true")!=std::string::npos;});
    write(connection/L"menu-first.json",read(connection/L"observer-status.json"));
    CHECK(read(connection/L"observer-status.json").find("\"turnBeginnings\":1")!=std::string::npos);
    ffr_test_decision(fake_manager.data(),fake_ui.data());
    wait([&]{return read(connection/L"observer-status.json").find("\"handlerEntries\":1")!=std::string::npos;});
    const auto before_pause=tick_calls;lease();std::this_thread::sleep_for(std::chrono::milliseconds(3200));
    CHECK(tick_calls==before_pause);write(connection/L"menu-paused.json",read(connection/L"observer-status.json"));
    wait([&]{return read(connection/L"observer-status.json").find("\"sampleTime\":"+std::to_string(now()))!=std::string::npos;});
    write(connection/L"menu-resumed.json",read(connection/L"observer-status.json"));
    CHECK(serialized==1 && read(connection/L"opening-progression.gvas")==source);
    ffr::RawArray invalid{};invalid.count=-1;put(fake_jobs,0x28,invalid);
    wait([&]{return read(connection/L"observer-status.json").find("\"menuObservation\":null")!=std::string::npos;});
    CHECK(read(connection/L"opening-progression.gvas")==source);
    put(fake_jobs,0x28,ffr::RawArray{});
    wait([&]{return read(connection/L"observer-status.json").find("\"commandMenu\":true")!=std::string::npos;});
    CHECK(abort_calls==0 && clone_loads==0 && serialized==1 && decision_calls==1);
    // Actual FFR teardown releases these objects. One missing manager is not
    // enough evidence; both must be null with inactive battle and UI flags.
    put(fake_manager,0x40,std::uint64_t{0});fake_manager[0x1a9]=0;fake_ui[0x80]=0;
    wait([&]{return read(connection/L"observer-status.json").find("\"menuObservation\":null")!=std::string::npos;});
    put(fake_manager,0x48,std::uint64_t{0});
    wait([&]{return read(connection/L"observer-status.json").find("\"managersReleased\":true")!=std::string::npos;});
    write(connection/L"menu-released.json",read(connection/L"observer-status.json"));
    fake_ui[0x80]=1;
    wait([&]{return read(connection/L"observer-status.json").find("\"menuObservation\":null")!=std::string::npos;});
}
}
