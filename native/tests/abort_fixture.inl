// Test only: fake game objects expose the exact profiled field layout.
namespace {
template<class T,std::size_t N> void put(std::array<unsigned char,N>& memory,std::size_t offset,const T& value) {
    CHECK(offset+sizeof(value)<=memory.size());std::memcpy(memory.data()+offset,&value,sizeof(value));
}
template<std::size_t N> void object(std::array<unsigned char,N>& memory,std::uint64_t vtable=1) {
    put(memory,0,vtable);put(memory,16,std::uint64_t{1});
}
void setup_task_fixture(const std::wstring& mode) {
    task_fixture_mode=mode;
    ffr_test_task_vtable[48]=reinterpret_cast<std::uint64_t>(&ffr_test_task_get_world);
    for(unsigned i=0;i<fake_task_classes.size();++i) {
        auto& klass=fake_task_classes[i];object(klass);
        fake_class_chains[i][0]=reinterpret_cast<std::uint64_t>(klass.data())+0x30;
        put(klass,0x30,reinterpret_cast<std::uint64_t>(fake_class_chains[i].data()));put(klass,0x38,std::int32_t{0});
    }
    ffr_test_task_class=reinterpret_cast<std::uint64_t>(fake_task_classes[0].data());
    ffr_test_root_vm_class=reinterpret_cast<std::uint64_t>(fake_task_classes[1].data());
    ffr_test_command_vm_class=reinterpret_cast<std::uint64_t>(fake_task_classes[2].data());
    ffr_test_talk_class=reinterpret_cast<std::uint64_t>(fake_task_classes[3].data());
    // The actual registry accepts a derived class key, with value at +16.
    fake_class_chains[4][0]=ffr_test_command_vm_class+0x30;
    fake_class_chains[4][1]=reinterpret_cast<std::uint64_t>(fake_task_classes[4].data())+0x30;
    put(fake_task_classes[4],0x38,std::int32_t{1});
    object(fake_instance);object(fake_task_manager);object(fake_vm_subsystem);object(fake_talk);object(fake_registry);
    put(fake_task_manager,0x10,ffr_test_task_class);put(fake_vm_subsystem,0x10,ffr_test_root_vm_class);put(fake_talk,0x10,ffr_test_talk_class);
    fake_subsystem_rows={{{ffr_test_task_class,reinterpret_cast<std::uint64_t>(fake_task_manager.data()),0},
        {ffr_test_root_vm_class,reinterpret_cast<std::uint64_t>(fake_vm_subsystem.data()),0},
        {ffr_test_talk_class,reinterpret_cast<std::uint64_t>(fake_talk.data()),0}}};
    put(fake_instance,0x108,ffr::RawArray{reinterpret_cast<std::uint64_t>(fake_subsystem_rows.data()),3,3});
    put(fake_world,0x228,reinterpret_cast<std::uint64_t>(fake_instance.data()));
    object(fake_task,reinterpret_cast<std::uint64_t>(ffr_test_task_vtable));object(fake_other_task,reinterpret_cast<std::uint64_t>(ffr_test_task_vtable));
    object(fake_result_task,reinterpret_cast<std::uint64_t>(ffr_test_result_skip_vtable));
    object(fake_step,reinterpret_cast<std::uint64_t>(ffr_test_step_vtable));
    put(fake_task_manager,0x38,reinterpret_cast<std::uint64_t>(fake_task.data()));put(fake_task,0x28,reinterpret_cast<std::uint64_t>(fake_step.data()));
    fake_task[0xaa]=1;
    put(fake_other_task,0x28,reinterpret_cast<std::uint64_t>(fake_step.data()));
    fake_other_task[0xaa]=1;
    put(fake_step,0xc0,std::int32_t{2});put(fake_step,0x30,reinterpret_cast<std::uint64_t>(&ffr_test_input_begin));
    put(fake_step,0x60,reinterpret_cast<std::uint64_t>(&ffr_test_empty_update));
    object(fake_command_vm,reinterpret_cast<std::uint64_t>(ffr_test_command_vm_vtable));
    object(fake_other_command_vm,reinterpret_cast<std::uint64_t>(ffr_test_command_vm_vtable));
    const auto derived=reinterpret_cast<std::uint64_t>(fake_task_classes[4].data());
    put(fake_command_vm,0x10,derived);put(fake_other_command_vm,0x10,derived);
    fake_command_vm[0x199]=1;fake_other_command_vm[0x199]=1;
    fake_vm_row={derived,0,reinterpret_cast<std::uint64_t>(fake_command_vm.data())};
    put(fake_registry,0x28,ffr::RawArray{reinterpret_cast<std::uint64_t>(fake_vm_row.data()),1,1});
    put(fake_vm_subsystem,0x30,reinterpret_cast<std::uint64_t>(fake_registry.data()));
}
void run_abort_fixture(const std::filesystem::path& connection,const std::wstring& mode) {
    const bool deny=mode.find(L"denied")!=std::wstring::npos;
    recreation_fixture=mode.starts_with(L"abort-recreate");
    recreation_menu_drift=mode==L"abort-recreate-menu-drift";
    release_managers_on_abort=mode==L"abort-released" || recreation_fixture;
    object(fake_manager,reinterpret_cast<std::uint64_t>(ffr_test_btl_vtable));
    object(fake_jobs);object(fake_units);object(fake_ui,reinterpret_cast<std::uint64_t>(ffr_test_ui_vtable));object(fake_world);object(fake_map);
    object(fake_command);object(fake_ui_command,reinterpret_cast<std::uint64_t>(ffr_test_ui_command_vtable));
    setup_task_fixture(mode);
    if(mode==L"abort-recreate-effects")put(fake_map_parameters[0],0xb8,
        ffr::RawArray{reinterpret_cast<std::uint64_t>(fake_command_request.data()),3,3});
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
    if(ready.find("\"commandAbortReady\":true")==std::string::npos)std::cerr<<ready<<'\n';
    CHECK(ready.find("\"commandAbortReady\":true")!=std::string::npos);
    CHECK(abort_calls==0 && ui_command_abort_calls==0 && !fake_manager[0x222] && !fake_manager[0x223]);
    if(deny) {
        if(mode==L"abort-queued-command-denied")put(fake_command,0x28,ffr::RawArray{reinterpret_cast<std::uint64_t>(fake_command_request.data()),1,1});
        else if(mode==L"abort-flagged-denied")fake_manager[0x223]=1;
        else if(mode==L"abort-task-world-denied")task_wrong_world=true;
        else if(mode==L"abort-step-denied")put(fake_step,0xc0,std::int32_t{3});
        else if(mode==L"abort-turn-event-denied")fake_task[0xaa]=0;
        else if(mode==L"abort-vm-denied")fake_command_vm[0x199]=0;
        else if(mode==L"abort-talk-denied") {object(fake_talk_player);put(fake_talk,0x58,reinterpret_cast<std::uint64_t>(fake_talk_player.data()));}
        else if(mode==L"abort-skill-denied")ffr_test_skill_begin(nullptr,123,nullptr);
        else if(mode==L"abort-foreign-denied") {std::thread other([]{ffr_test_decision(nullptr,nullptr);});other.join();}
        else ffr_test_decision(nullptr,nullptr);
        const bool layout_denied=mode==L"abort-queued-command-denied"||mode==L"abort-flagged-denied"||mode==L"abort-task-world-denied"||mode==L"abort-step-denied"||mode==L"abort-turn-event-denied"||mode==L"abort-vm-denied"||mode==L"abort-talk-denied";
        const auto field=layout_denied?"\"commandAbortReady\":false":"\"openingHistoryUntouched\":false";
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
        if(mode==L"abort-wrong-owner" || mode==L"abort-wrong-task") {
            if(mode==L"abort-wrong-owner")fake_vm_row[2]=reinterpret_cast<std::uint64_t>(fake_other_command_vm.data());
            else put(fake_task_manager,0x38,reinterpret_cast<std::uint64_t>(fake_other_task.data()));
            request("commit");
            wait([&] {return read(connection/L"abort-status.json").find("\"phase\":\"rejected\"")!=std::string::npos;});
            CHECK(abort_calls==0 && ui_command_abort_calls==0 && fake_manager[0x222]==0 && fake_manager[0x1a9]==1 && serialized==2);
            std::string terminal;wait([&]{terminal=read(connection/L"abort-status.json");return !terminal.empty();});
            write(connection/L"abort-final-status.json",terminal);return;
        }
        request("commit");
        wait([&] {return abort_calls==1 && vm_end_calls==1;});
        CHECK(abort_thread==GetCurrentThreadId() && vm_end_thread==GetCurrentThreadId());
        CHECK(ui_command_abort_calls==0 && fake_manager[0x223]==0);
        if(mode==L"abort-ui-fails" || mode==L"abort-ui-task-drift" || mode==L"abort-ui-job-drift" || mode==L"abort-ui-code-drift" || mode==L"abort-check-false" || mode==L"abort-check-mode") {
            const bool checker=mode==L"abort-check-false" || mode==L"abort-check-mode";
            for(unsigned i=0;i<130;++i){pump();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
            wait([&] {return read(connection/L"abort-status.json").find("\"phase\":\"timed-out\"")!=std::string::npos;});
            CHECK(abort_calls==1 && vm_end_calls==1 && finish_check_calls==(checker?1u:0u));
            CHECK(fake_manager[0x1a9]==1 && serialized==2 && !std::filesystem::exists(connection/L"abort-after.gvas"));
            write(connection/L"abort-final-status.json",read(connection/L"abort-status.json"));return;
        }
        CHECK(finish_check_calls==1 && finish_check_thread==GetCurrentThreadId());
        wait([&] {const auto s=read(connection/L"abort-status.json");return s.find("\"phase\":\"manager-cleared\"")!=std::string::npos
            && s.find("\"file\":\"abort-after.gvas\",\"sha256\":\"\"")==std::string::npos;});
        CHECK(abort_calls==1 && vm_end_calls==1 && finish_check_calls==1 && serialized==released && serialized==3 && fake_manager[0x1a9]==0);
        CHECK(decision_calls==0 && skill_producer_calls==0);
        CHECK(std::filesystem::exists(connection/L"abort-after.gvas"));
        for(unsigned i=0;i<12;++i){pump();std::this_thread::sleep_for(std::chrono::milliseconds(25));}
        CHECK(abort_calls==1 && vm_end_calls==1 && finish_check_calls==1);
        if(mode==L"abort-released")CHECK(read(connection/L"abort-status.json").find("\"managersReleased\":true")!=std::string::npos);
        if(recreation_fixture) {
            const auto recreate_deadline=now()+8;const auto recreate_nonce=std::string(32,'c');
            auto recreate=[&](const std::string& phase) {
                write(connection/L"abort-request.txt","FFR_HELPER_ABORT_OPENING_V1\n"+session+"\n"+recreate_nonce+"\n"
                    +std::to_string(recreate_deadline)+"\n1\n"+hash+"\n"+phase+"\n");
            };
            recreate("commit-recreate");for(unsigned i=0;i<8;++i){pump();std::this_thread::sleep_for(std::chrono::milliseconds(25));}
            CHECK(calls==1 && recreation_allocations==0);
            if(mode==L"abort-recreate-exit-anchor")put(fake_field_component,0x200,170.125);
            recreate("prepare-recreate");
            wait([&] {return read(connection/L"abort-status.json").find("\"phase\":\"armed\"")!=std::string::npos;});
            CHECK(calls==1 && recreation_allocations==0);
            if(mode==L"abort-recreate-root-drift")recreation_progression_drift=true;
            if(mode==L"abort-recreate-field-drift")put(fake_field_component,0x200,999.0);
            if(mode==L"abort-recreate-cache-drift")put(fake_map_parameters[0],0x10c,std::int32_t{999});
            recreation_allocate_drift=mode==L"abort-recreate-allocate-drift";
            recreate("commit-recreate");
            const bool success=mode==L"abort-recreate" || mode==L"abort-recreate-effects" || mode==L"abort-recreate-exit-anchor";
            const bool entered=success || recreation_menu_drift;
            const auto expected=success?"\"phase\":\"verified\"":recreation_menu_drift?"\"phase\":\"uncertain\"":"\"phase\":\"refused\"";
            wait([&] {return read(connection/L"abort-status.json").find(expected)!=std::string::npos;});
            CHECK(calls==(entered?2u:1u) && recreation_blocks.empty() && recreation_allocations==recreation_frees);
            CHECK(abort_calls==1 && vm_end_calls==1 && finish_check_calls==1 && decision_calls==0 && skill_producer_calls==0);
            for(unsigned i=0;i<8;++i){pump();std::this_thread::sleep_for(std::chrono::milliseconds(25));}
            CHECK(calls==(entered?2u:1u));
            write(connection/L"recreation-final-status.json",read(connection/L"abort-status.json"));
        }
    }
    write(connection/L"abort-final-status.json",read(connection/L"abort-status.json"));
}
}
