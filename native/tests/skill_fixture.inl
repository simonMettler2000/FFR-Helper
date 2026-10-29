namespace {
void run_skill_fixture(const std::filesystem::path& connection,bool available) {
    auto lease=[&](char session,std::int64_t expiry){write(connection/L"observe.txt","FFR_HELPER_OBSERVER_V1\n"+std::string(32,session)+"\n"+std::to_string(expiry)+"\n"+std::string(32,'0')+"\n");};
    lease('a',now()+8);wait_for([&]{return !read(connection/L"observer-status.json").empty();});
    std::vector<std::int32_t> friends{1001};ffr::RawEntry entry{};entry.friend_ids=array(friends);
    ffr_test_encounter(fake_map.data(),&entry,false);
    wait_for([&]{return !read(connection/L"opening-progression.gvas").empty();});
    const auto source=read(connection/L"opening-progression.gvas");
    if(!available) {
        CHECK(read(connection/L"observer-status.json").find("\"skillHistory\":null")!=std::string::npos);
        CHECK(serialized==1);return;
    }
    wait_for([&]{return read(connection/L"observer-status.json").find("\"skillBeginEvents\":0")!=std::string::npos;});
    // Identical arguments from an unrelated event must not count. Only the
    // profiled callsite identifies the Skill.Begin event; no data is decoded.
    SetLastError(1357);ffr_test_unrelated_event(fake_manager.data(),0x123456789abcdef0ull,fake_ui.data());
    CHECK(dispatch_calls==1 && dispatch_input_error==1357 && GetLastError()==8642);
    CHECK(dispatch_context==fake_manager.data() && dispatch_name==0x123456789abcdef0ull && dispatch_data==fake_ui.data());
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    CHECK(read(connection/L"observer-status.json").find("\"skillBeginEvents\":0")!=std::string::npos);
    SetLastError(2468);ffr_test_skill_begin(fake_manager.data(),0x123456789abcdef0ull,fake_ui.data());
    CHECK(dispatch_calls==2 && dispatch_input_error==2468 && skill_producer_calls==1 && GetLastError()==8642);
    wait_for([&]{return read(connection/L"observer-status.json").find("\"skillBeginEvents\":1")!=std::string::npos;});
    write(connection/L"skill-first.json",read(connection/L"observer-status.json"));
    CHECK(read(connection/L"opening-progression.gvas")==source && serialized==1 && decision_calls==0);
    std::thread foreign([]{ffr_test_skill_begin(nullptr,0,nullptr);});foreign.join();
    wait_for([&]{return read(connection/L"observer-status.json").find("\"foreignThreadObserved\":true")!=std::string::npos;});
    write(connection/L"skill-foreign.json",read(connection/L"observer-status.json"));
    lease('a',0);std::this_thread::sleep_for(std::chrono::milliseconds(800));lease('a',now()+8);
    wait_for([&]{return read(connection/L"observer-status.json").find("\"skillHistory\":null")!=std::string::npos;});
    ffr_test_encounter(fake_map.data(),&entry,false);
    wait_for([&]{return read(connection/L"observer-status.json").find("\"skillBeginEvents\":0")!=std::string::npos;});
    lease('b',now()+8);
    wait_for([&]{const auto s=read(connection/L"observer-status.json");return s.find(std::string(32,'b'))!=std::string::npos && s.find("\"skillHistory\":null")!=std::string::npos;});
    CHECK(dispatch_calls==3 && skill_producer_calls==2 && unrelated_producer_calls==1);
}
}
