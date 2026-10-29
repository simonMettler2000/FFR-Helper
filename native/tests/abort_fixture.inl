// Test only: fake game objects expose the exact profiled field layout.
namespace {
template<class T,std::size_t N> void put(std::array<unsigned char,N>& memory,std::size_t offset,const T& value) {
    CHECK(offset+sizeof(value)<=memory.size());std::memcpy(memory.data()+offset,&value,sizeof(value));
}
template<std::size_t N> void object(std::array<unsigned char,N>& memory,std::uint64_t vtable=1) {
    put(memory,0,vtable);put(memory,16,std::uint64_t{1});
}
void run_abort_fixture(const std::filesystem::path& connection,const std::wstring& mode) {
    const bool deny=mode.find(L"denied")!=std::wstring::npos;
    release_managers_on_abort=mode==L"abort-released";
    object(fake_manager,reinterpret_cast<std::uint64_t>(ffr_test_btl_vtable));
    object(fake_jobs);object(fake_units);object(fake_ui,reinterpret_cast<std::uint64_t>(ffr_test_ui_vtable));object(fake_world);object(fake_map);
    object(fake_command);object(fake_ui_command,reinterpret_cast<std::uint64_t>(ffr_test_ui_command_vtable));
    put(fake_command,0x28,ffr::RawArray{});
    put(fake_manager,0xb0,reinterpret_cast<std::uint64_t>(fake_command.data()));
    put(fake_ui,0x70,reinterpret_cast<std::uint64_t>(fake_ui_command.data()));
    fake_manager[0x1a9]=1;fake_manager[0x220]=1;fake_ui[0x80]=1;
    put(fake_manager,0x40,reinterpret_cast<std::uint64_t>(fake_jobs.data()));
    put(fake_manager,0x48,reinterpret_cast<std::uint64_t>(fake_units.data()));
    put(fake_jobs,0x28,ffr::RawArray{});put(fake_jobs,0x38,ffr::RawArray{});
    std::vector<std::uint64_t> actors;
    for(unsigned i=0;i<2;++i) {
        object(fake_actors[i]);object(fake_parameters[i]);
        put(fake_actors[i],0x838,reinterpret_cast<std::uint64_t>(fake_parameters[i].data()));
        put(fake_parameters[i],0xd0,std::int32_t{1001+static_cast<std::int32_t>(i)});
        put(fake_parameters[i],0x4dc,std::int32_t{i==0?1:0});
        actors.push_back(reinterpret_cast<std::uint64_t>(fake_actors[i].data()));
    }
    put(fake_units,0x28,array(actors));put(fake_units,0x38,actors[0]);
    const auto tick=reinterpret_cast<void(__fastcall*)(void*,float)>(GetProcAddress(GetModuleHandleW(nullptr),"ffr_test_abort_tick"));
    auto pump=[&] {
        const auto count=tick_calls;tick(fake_manager.data()+0x30,0.125f);
        CHECK(tick_calls==count+1 && last_delta==0.125f && GetLastError()==9876);
    };
    auto wait=[&](auto condition) {wait_for([&] {pump();return condition();});};
    const auto session=std::string(32,'a'),nonce=std::string(32,'b');
    write(connection/L"observe.txt","FFR_HELPER_OBSERVER_V1\n"+session+"\n"+std::to_string(now()+8)+"\n"+std::string(32,'0')+"\n");
    wait([&] {return !read(connection/L"observer-status.json").empty();});
    std::vector<std::int32_t> friends{1001,1002},enemies{11},positions{0,1};
    ffr::RawEntry entry{};entry.friend_ids=array(friends);entry.friend_locations=array(positions);
    entry.enemy_ids=array(enemies);entry.group=123;entry.stage=456;entry.event=-1;entry.can_escape=1;entry.escape_probability=50;
    ffr_test_encounter(fake_map.data(),&entry,false);
    const std::string key="\"openingHash\":\"";
    wait([&] {const auto s=read(connection/L"abort-status.json");const auto pos=s.find(key);
        return s.find("\"commandMenu\":true")!=std::string::npos && pos!=std::string::npos
            && s.size()>pos+key.size() && s[pos+key.size()]!='"';});
    const auto ready=read(connection/L"abort-status.json");CHECK(ready.find("\"available\":true")!=std::string::npos);
    const auto start=ready.find(key);CHECK(start!=std::string::npos);
    const auto hash=ready.substr(start+key.size(),64);CHECK(hash.size()==64 && hash.find('"')==std::string::npos);
    CHECK(ready.find("\"openingHistoryUntouched\":true")!=std::string::npos);
    CHECK(ready.find("\"commandAbortReady\":true")!=std::string::npos);
    CHECK(abort_calls==0 && ui_command_abort_calls==0 && !fake_manager[0x222] && !fake_manager[0x223]);
    if(deny) {
        if(mode==L"abort-queued-command-denied")put(fake_command,0x28,ffr::RawArray{reinterpret_cast<std::uint64_t>(fake_command_request.data()),1,1});
        else if(mode==L"abort-flagged-denied")fake_manager[0x223]=1;
        else if(mode==L"abort-skill-denied")ffr_test_skill_begin(nullptr,123,nullptr);
        else if(mode==L"abort-foreign-denied") {std::thread other([]{ffr_test_decision(nullptr,nullptr);});other.join();}
        else ffr_test_decision(nullptr,nullptr);
        const auto field=mode==L"abort-queued-command-denied"||mode==L"abort-flagged-denied"?"\"commandAbortReady\":false":"\"openingHistoryUntouched\":false";
        wait([&] {return read(connection/L"abort-status.json").find(field)!=std::string::npos;});
    }
    const auto deadline=now()+8;
    auto request=[&](const std::string& phase) {
        write(connection/L"abort-request.txt","FFR_HELPER_ABORT_OPENING_V1\n"+session+"\n"+nonce+"\n"+std::to_string(deadline)+"\n1\n"+hash+"\n"+phase+"\n");
    };
    request("commit");for(unsigned i=0;i<8;++i){pump();std::this_thread::sleep_for(std::chrono::milliseconds(50));}
    CHECK(abort_calls==0);
    request("prepare");
    if(deny) {
        wait([&] {return read(connection/L"abort-status.json").find("\"phase\":\"rejected\"")!=std::string::npos;});
        request("commit");for(unsigned i=0;i<8;++i){pump();std::this_thread::sleep_for(std::chrono::milliseconds(50));}
        CHECK(abort_calls==0 && ui_command_abort_calls==0 && !std::filesystem::exists(connection/L"abort-recovery.gvas"));
    } else {
        wait([&] {return read(connection/L"abort-status.json").find("\"phase\":\"armed\"")!=std::string::npos;});
        CHECK(abort_calls==0 && std::filesystem::exists(connection/L"abort-recovery.gvas"));
        CHECK(serialized==released && serialized==2);
        if(mode==L"abort-wrong-owner") {
            object(fake_other_ui_command,reinterpret_cast<std::uint64_t>(ffr_test_ui_command_vtable));object(fake_other_world);
            put(fake_ui,0x70,reinterpret_cast<std::uint64_t>(fake_other_ui_command.data()));
            request("commit");
            wait([&] {return read(connection/L"abort-status.json").find("\"phase\":\"rejected\"")!=std::string::npos;});
            CHECK(abort_calls==0 && ui_command_abort_calls==0 && fake_manager[0x222]==0 && fake_manager[0x1a9]==1 && serialized==2);
            std::string terminal;wait([&]{terminal=read(connection/L"abort-status.json");return !terminal.empty();});
            write(connection/L"abort-final-status.json",terminal);return;
        }
        request("commit");
        wait([&] {return abort_calls==1 && ui_command_abort_calls==1;});
        CHECK(fake_manager[0x222] && fake_manager[0x223]);
        CHECK(abort_thread==GetCurrentThreadId() && ui_command_abort_thread==GetCurrentThreadId());
        wait([&] {const auto s=read(connection/L"abort-status.json");return s.find("\"phase\":\"manager-cleared\"")!=std::string::npos
            && s.find("\"file\":\"abort-after.gvas\",\"sha256\":\"\"")==std::string::npos;});
        CHECK(abort_calls==1 && ui_command_abort_calls==1 && serialized==released && serialized==3 && fake_manager[0x1a9]==0);
        CHECK(decision_calls==0 && skill_producer_calls==0);
        CHECK(std::filesystem::exists(connection/L"abort-after.gvas"));
        for(unsigned i=0;i<12;++i){pump();std::this_thread::sleep_for(std::chrono::milliseconds(25));}
        CHECK(abort_calls==1 && ui_command_abort_calls==1);
        if(mode==L"abort-released")CHECK(read(connection/L"abort-status.json").find("\"managersReleased\":true")!=std::string::npos);
    }
    write(connection/L"abort-final-status.json",read(connection/L"abort-status.json"));
}
}
