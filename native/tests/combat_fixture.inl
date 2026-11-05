namespace {
void run_combat_fixture(const std::filesystem::path& connection,const std::wstring& mode) {
    const bool denied=mode==L"combat-denied",stale=mode==L"combat-stale",after_action=mode==L"combat-after-action";
    object(fake_manager,reinterpret_cast<std::uint64_t>(ffr_test_btl_vtable));
    object(fake_jobs);object(fake_units);object(fake_ui);object(fake_world);object(fake_map);object(fake_scheduler);
    fake_manager[0x1a9]=1;fake_ui[0x80]=1;
    put(fake_manager,0x40,reinterpret_cast<std::uint64_t>(fake_jobs.data()));
    put(fake_manager,0x48,reinterpret_cast<std::uint64_t>(fake_units.data()));
    put(fake_manager,0x200,std::int32_t{3});
    put(fake_jobs,0x28,ffr::RawArray{});put(fake_jobs,0x38,ffr::RawArray{});
    std::vector<std::uint64_t> actors;
    for(unsigned i=0;i<2;++i) {
        object(fake_actors[i]);object(fake_parameters[i]);
        put(fake_actors[i],0x838,reinterpret_cast<std::uint64_t>(fake_parameters[i].data()));
        put(fake_parameters[i],0x360,reinterpret_cast<std::uint64_t>(fake_actors[i].data()));
        put(fake_parameters[i],0xd0,std::int32_t{1001+static_cast<std::int32_t>(i)});
        put(fake_parameters[i],0x550,static_cast<std::int32_t>(i));
        put(fake_parameters[i],0x10c,std::int32_t{150});put(fake_parameters[i],0x268,std::int32_t{25});
        put(fake_parameters[i],0x4dc,std::int32_t{after_action?2:0});
        actors.push_back(reinterpret_cast<std::uint64_t>(fake_actors[i].data()));
    }
    put(fake_units,0x28,array(actors));put(fake_units,0x38,actors[0]);
    put(fake_units,0x40,reinterpret_cast<std::uint64_t>(fake_scheduler.data()));
    put(fake_units,0x48,ffr::RawArray{});put(fake_scheduler,0x28,reinterpret_cast<std::uint64_t>(fake_units.data()));
    put(fake_scheduler,0x30,ffr::RawArray{});
    std::array<unsigned char,0x90> timeline_owner{};
    std::array<std::array<unsigned char,0x1c0>,2> timeline_models{};
    std::array<std::array<std::uint64_t,3>,2> timeline_rows{};
    object(timeline_owner);put(fake_ui,0x78,reinterpret_cast<std::uint64_t>(timeline_owner.data()));
    for(unsigned i=0;i<2;++i) {
        object(timeline_models[i]);put(timeline_models[i],0x198,static_cast<std::int32_t>(i));
        const auto parameter=reinterpret_cast<std::uint64_t>(fake_parameters[i].data());
        put(timeline_models[i],0x70,parameter);put(timeline_models[i],0x1a0,parameter);
        timeline_rows[i]={i,reinterpret_cast<std::uint64_t>(timeline_models[i].data()),i==0?1ull:0xffffffffull};
    }
    put(timeline_owner,0x28,ffr::RawArray{reinterpret_cast<std::uint64_t>(timeline_rows.data()),2,2});
    put(timeline_owner,0x60,std::int32_t{0});put(timeline_owner,0x70,std::int32_t{1});
    std::vector<std::uint64_t> damage_sources{actors[1],actors[1],0},skill_targets{reinterpret_cast<std::uint64_t>(fake_parameters[1].data()),0};
    put(fake_parameters[0],0x368,actors[1]);put(fake_parameters[1],0x370,actors[0]);put(fake_parameters[0],0x380,actors[1]);
    put(fake_parameters[0],0x388,array(damage_sources));put(fake_parameters[0],0x270,array(skill_targets));
    std::array<unsigned char,0xa0> effect_a{},effect_b{};object(effect_a);object(effect_b);
    std::vector<std::int32_t> effect_values{11,-22,33};
    for(auto* effect:{&effect_a,&effect_b}) {
        put(*effect,0x28,reinterpret_cast<std::uint64_t>(fake_parameters[1].data()));put(*effect,0x30,reinterpret_cast<std::uint64_t>(fake_parameters[0].data()));
        put(*effect,0x50,std::int32_t{77});put(*effect,0x78,std::uint8_t{5});
    }
    put(effect_a,0x80,array(effect_values));put(effect_a,0x98,std::int32_t{3});put(effect_b,0x98,std::int32_t{-1});
    if(mode==L"combat-effects-unavailable")put(effect_a,0x28,std::uint64_t{0x99999999});
    std::vector<std::uint64_t> effects_a{reinterpret_cast<std::uint64_t>(effect_a.data()),reinterpret_cast<std::uint64_t>(effect_a.data())},
        effects_b{reinterpret_cast<std::uint64_t>(effect_b.data())};
    put(fake_parameters[0],0xb8,array(effects_a));put(fake_parameters[1],0xb8,array(effects_b));
    std::vector<std::uint64_t> queued{actors[0]};if(denied)put(fake_jobs,0x28,array(queued));
    const auto tick=reinterpret_cast<void(__fastcall*)(void*,float)>(GetProcAddress(GetModuleHandleW(nullptr),"ffr_test_abort_tick"));
    auto pump=[&]{const auto count=tick_calls;tick(fake_manager.data()+0x30,0.5f);CHECK(tick_calls==count+1 && last_delta==0.5f && GetLastError()==9876);};
    auto wait=[&](auto condition){wait_for([&]{pump();return condition();});};
    const auto session=std::string(32,'a');
    auto lease=[&]{write(connection/L"observe.txt","FFR_HELPER_OBSERVER_V1\n"+session+"\n"+std::to_string(now()+8)+"\n"+std::string(32,'0')+"\n");};
    lease();wait([&]{return !read(connection/L"observer-status.json").empty();});
    std::vector<std::int32_t> friends{1001,1002},enemies{11};
    ffr::RawEntry entry{};entry.friend_ids=array(friends);entry.enemy_ids=array(enemies);entry.event=-1;entry.can_escape=1;entry.escape_probability=50;
    ffr_test_encounter(fake_map.data(),&entry,false);
    const std::string key="\"openingHash\":\"";
    wait([&]{const auto text=read(connection/L"abort-status.json");const auto pos=text.find(key);return pos!=std::string::npos && text[pos+key.size()]!='"';});
    const auto status=read(connection/L"abort-status.json");const auto hash=status.substr(status.find(key)+key.size(),64);
    auto send=[&](char nonce) {
        write(connection/L"combat-request.txt","FFR_HELPER_COMBAT_OBSERVATION_V1\n"+session+"\n"+std::string(32,nonce)+"\n"+std::to_string(now()+8)+'\n'+(stale?"2":"1")+'\n'+hash+'\n');
    };
    CHECK(read(connection/L"combat-report.json").find("\"phase\":\"idle\"")!=std::string::npos);
    send('c');
    wait([&]{return read(connection/L"combat-report.json").find("\"request\":\""+std::string(32,'c')+'"')!=std::string::npos;});
    auto first=read(connection/L"combat-report.json");
    CHECK(first.find("\"phase\":\""+std::string(denied||stale?"rejected":"captured")+'"')!=std::string::npos);
    if(!denied && !stale) {
        CHECK(first.find("\"hpBase\":150")!=std::string::npos && first.find("\"temporaryHp\":25")!=std::string::npos);
        write(connection/L"combat-first-report.json",first);
        put(fake_parameters[0],0x10c,std::int32_t{7});put(fake_parameters[0],0x368,actors[0]);
        put(effect_a,0x98,std::int32_t{2});effect_values[0]=99;send('c');
        for(unsigned i=0;i<8;++i){pump();std::this_thread::sleep_for(std::chrono::milliseconds(50));}
        CHECK(read(connection/L"combat-report.json").find("\"hpBase\":7")==std::string::npos);
        lease();send('d');wait([&]{return read(connection/L"combat-report.json").find("\"request\":\""+std::string(32,'d')+'"')!=std::string::npos;});
        CHECK(read(connection/L"combat-report.json").find("\"hpBase\":7")!=std::string::npos);
        send('c');for(unsigned i=0;i<8;++i){pump();std::this_thread::sleep_for(std::chrono::milliseconds(50));}
        CHECK(read(connection/L"combat-report.json").find("\"request\":\""+std::string(32,'d')+'"')!=std::string::npos);
    }
    CHECK(abort_calls==0 && clone_loads==0 && serialized==1 && released==1 && fake_manager[0x1a9]==1);
    write(connection/L"combat-final-report.json",read(connection/L"combat-report.json"));
}
}
