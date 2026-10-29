namespace {
void run_clone_fixture(const std::filesystem::path& connection,const std::wstring& mode,HMODULE loader) {
    clone_fixture=true;clone_mismatch=mode==L"clone-mismatch";clone_mutates=mode==L"clone-mutates";
    const bool unready=mode==L"clone-unready",deny=mode==L"clone-denied";
    const bool import_only=mode==L"clone-import-only",advanced=mode==L"clone-after-action";
    auto set_ready=reinterpret_cast<void(*)(bool)>(GetProcAddress(loader,"ffr_test_loader_set_ready"));CHECK(set_ready);
    auto root_count=reinterpret_cast<unsigned(*)()>(GetProcAddress(loader,"ffr_test_root_count"));CHECK(root_count);
    set_ready(!unready);
    object(fake_root,reinterpret_cast<std::uint64_t>(ffr_test_save_vtable));
    object(fake_clone,reinterpret_cast<std::uint64_t>(ffr_test_save_vtable));
    object(fake_manager,reinterpret_cast<std::uint64_t>(ffr_test_btl_vtable));
    object(fake_jobs);object(fake_units);object(fake_ui);object(fake_world);object(fake_map);
    fake_manager[0x1a9]=1;fake_ui[0x80]=1;
    put(fake_manager,0x40,reinterpret_cast<std::uint64_t>(fake_jobs.data()));
    put(fake_manager,0x48,reinterpret_cast<std::uint64_t>(fake_units.data()));
    put(fake_jobs,0x28,ffr::RawArray{});put(fake_jobs,0x38,ffr::RawArray{});
    std::vector<std::uint64_t> actors;
    for(unsigned i=0;i<2;++i) {
        object(fake_actors[i]);object(fake_parameters[i]);
        put(fake_actors[i],0x838,reinterpret_cast<std::uint64_t>(fake_parameters[i].data()));
        put(fake_parameters[i],0xd0,std::int32_t{1001+static_cast<std::int32_t>(i)});
        put(fake_parameters[i],0x4dc,std::int32_t{advanced?3:deny?1:0});actors.push_back(reinterpret_cast<std::uint64_t>(fake_actors[i].data()));
    }
    put(fake_units,0x28,array(actors));put(fake_units,0x38,actors[0]);
    std::vector<std::uint64_t> queued{actors[0]};if(deny)put(fake_jobs,0x28,array(queued));
    const auto tick=reinterpret_cast<void(__fastcall*)(void*,float)>(GetProcAddress(GetModuleHandleW(nullptr),"ffr_test_abort_tick"));
    auto pump=[&]{const auto before=tick_calls;tick(fake_manager.data()+0x30,0.25f);CHECK(tick_calls==before+1 && last_delta==0.25f && GetLastError()==9876);};
    auto wait=[&](auto condition){wait_for([&]{pump();return condition();});};
    const auto session=std::string(32,'a'),nonce=std::string(32,'b');
    auto lease=[&]{write(connection/L"observe.txt","FFR_HELPER_OBSERVER_V1\n"+session+"\n"+std::to_string(now()+8)+"\n"+std::string(32,'0')+"\n");};
    lease();wait([&]{return !read(connection/L"observer-status.json").empty();});
    std::vector<std::int32_t> friends{1001,1002},enemies{11};
    ffr::RawEntry entry{};entry.friend_ids=array(friends);entry.enemy_ids=array(enemies);
    entry.event=-1;entry.can_escape=1;entry.escape_probability=50;
    ffr_test_encounter(fake_map.data(),&entry,false);
    const std::string key="\"openingHash\":\"";
    wait([&]{const auto s=read(connection/L"abort-status.json");const auto pos=s.find(key);return pos!=std::string::npos && s[pos+key.size()]!='"';});
    const auto ready=read(connection/L"abort-status.json");const auto hash=ready.substr(ready.find(key)+key.size(),64);
    for(unsigned i=0;i<6;++i)pump();CHECK(clone_loads==0 && abort_calls==0 && root_count()==0);
    if(import_only) {
        for(const auto* phase:{"prepare","commit","probe-normalize"}) {
            write(connection/L"abort-request.txt","FFR_HELPER_ABORT_OPENING_V1\n"+session+"\n"+nonce+"\n"+std::to_string(now()+8)+"\n1\n"+hash+'\n'+phase+'\n');
            for(unsigned i=0;i<8;++i){pump();std::this_thread::sleep_for(std::chrono::milliseconds(50));}
            CHECK(clone_loads==0 && abort_calls==0 && root_count()==0 && serialized==1);
        }
        write(connection/L"import-only-policy-status.json",read(connection/L"abort-status.json"));
    }
    write(connection/L"abort-request.txt","FFR_HELPER_ABORT_OPENING_V1\n"+session+"\n"+nonce+"\n"+std::to_string(now()+8)+"\n1\n"+hash+"\nprobe-import\n");
    const std::string outcome=unready||deny?"rejected":clone_mismatch||clone_mutates?"failed":"passed";
    wait([&]{return read(connection/L"abort-status.json").find("\"phase\":\""+outcome+"\"")!=std::string::npos;});
    CHECK(clone_loads==((unready||deny)?0u:1u) && root_count()==0 && abort_calls==0 && serialized==released);
    // A repeated import request and a later abort command cannot execute again.
    lease();write(connection/L"abort-request.txt","FFR_HELPER_ABORT_OPENING_V1\n"+session+"\n"+nonce+"\n"+std::to_string(now()+8)+"\n1\n"+hash+"\nprepare\n");
    for(unsigned i=0;i<10;++i){pump();std::this_thread::sleep_for(std::chrono::milliseconds(50));}
    CHECK(clone_loads==((unready||deny)?0u:1u) && abort_calls==0 && root_count()==0);
    write(connection/L"clone-final-status.json",read(connection/L"abort-status.json"));
}
}
