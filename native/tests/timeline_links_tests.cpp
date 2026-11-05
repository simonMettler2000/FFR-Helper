#include "timeline_links.hpp"
#include <iostream>
#include <stdexcept>
#define CHECK(x) do {if(!(x))throw std::runtime_error("Timeline link check failed at "+std::to_string(__LINE__));}while(false)
struct Memory {
    std::vector<unsigned char> bytes=std::vector<unsigned char>(128*1024);std::size_t used=16;
    std::uint64_t object(std::size_t size) {used=(used+7)&~std::size_t(7);CHECK(used+size<bytes.size());auto at=used;used+=size;auto p=at+0x10000;put(p,std::uint64_t{1});put(p+16,std::uint64_t{1});return p;}
    template<class T> void put(std::uint64_t p,const T& v) {CHECK(p>=0x10000 && p-0x10000+sizeof(T)<=bytes.size());std::memcpy(bytes.data()+p-0x10000,&v,sizeof(T));}
    bool read(std::uint64_t p,void* output,std::size_t size) const {if(p<0x10000 || p-0x10000>bytes.size() || size>bytes.size()-(p-0x10000))return false;std::memcpy(output,bytes.data()+p-0x10000,size);return true;}
};
struct Row {std::int32_t key{},padding{};std::uint64_t model{};std::int32_t next=-1,hash{};};
static_assert(sizeof(Row)==24);
struct Fixture {
    Memory m;std::uint64_t ui=m.object(0x90),owner=m.object(0x90),rows=m.object(24*3);
    std::vector<std::uint64_t> parameters{m.object(0x560),m.object(0x560)},models{m.object(0x1c0),m.object(0x1c0)};
    Fixture() {
        m.put(ui+0x80,std::uint8_t{1});m.put(ui+0x78,owner);m.put(owner+0x28,ffr::RawArray{rows,3,3});
        m.put(owner+0x5c,std::int32_t{1});m.put(owner+0x60,std::int32_t{0});m.put(owner+0x70,std::int32_t{1});
        m.put(rows,Row{0,0,models[0],2,0});m.put(rows+24,Row{-999,0,1,-1,0});m.put(rows+48,Row{1,0,models[1],-1,0});
        for(unsigned i=0;i<2;++i) {m.put(parameters[i]+0xd0,std::int32_t{1001+static_cast<std::int32_t>(i)});
            m.put(models[i]+0x198,static_cast<std::int32_t>(i));m.put(models[i]+0x70,parameters[i]);m.put(models[i]+0x1a0,parameters[i]);}
    }
    std::optional<ffr::TimelineLinks> capture() {return ffr::capture_timeline_links([&](auto p,auto b,auto s){return m.read(p,b,s);},ui,parameters);}
};
int main() {
    try {
        {Fixture f;auto r=f.capture();CHECK(r && r->links.size()==2 && r->missing_units.empty());
            CHECK(r->links[0].cached_unit==0 && r->links[1].source_unit==1 && r->links[0].source_matches_cached);
            CHECK(!r->links[0].fallback_relation && r->links[0].cached_id==1001);
            auto json=ffr::timeline_links_json(r);CHECK(json.find("\"renderedAnchorsVerified\":false")!=std::string::npos);
            CHECK(json.find("\"current\"")!=std::string::npos && json.find("\"cachedUnit\":1")!=std::string::npos);}
        for(unsigned mode=0;mode<5;++mode) {
            Fixture f;auto outside=f.m.object(0x560);f.m.put(outside+0xd0,std::int32_t{1001});
            if(mode==0)outside=0;
            if(mode==1)f.m.put(outside+8,std::uint64_t{0x40000000});
            if(mode==3)f.m.put(outside,std::uint64_t{0});
            if(mode==4)outside=0x99999999;
            f.m.put(f.models[0]+0x1a0,outside);auto r=f.capture();CHECK(r && r->missing_units==std::vector<std::int32_t>{0});
            const std::string expected[]={"null","invalid-object","outside-current","unsupported-object-header","unreadable"};
            CHECK(r->links[0].cached_state==expected[mode] && r->links[0].source_unit==0 && !r->links[0].source_matches_cached);
            CHECK(r->links[0].fallback_relation.has_value()==(mode<2));
            if(mode<2)CHECK(*r->links[0].fallback_relation==0);
        }
        for(unsigned mode=0;mode<11;++mode) {
            Fixture f;
            switch(mode) {
            case 0:f.m.put(f.rows+16,std::int32_t{0});break;
            case 1:f.m.put(f.rows+48,std::int32_t{0});break;
            case 2:f.m.put(f.owner+0x5c,std::int32_t{0});break;
            case 3:f.m.put(f.owner+0x70,std::int32_t{3});break;
            case 4:f.m.put(f.models[0]+0x198,std::int32_t{99});break;
            case 5:f.m.put(f.ui+0x80,std::uint8_t{0});break;
            case 6:f.m.put(f.rows+48+8,f.models[0]);break;
            case 7:f.m.put(f.owner+0x30,std::int32_t{257});break;
            case 8:f.m.put(f.owner+0x5c,std::int32_t{-1});break;
            case 9:f.parameters[1]=f.parameters[0];break;
            case 10:f.m.put(f.rows+16,std::int32_t{3});break;
            }
            CHECK(!f.capture());
        }
        {Fixture f;f.m.put(f.owner+0x28,ffr::RawArray{});f.m.put(f.owner+0x5c,std::int32_t{0});f.m.put(f.owner+0x70,std::int32_t{0});
            auto r=f.capture();CHECK(r && r->links.empty() && r->missing_units==std::vector<std::int32_t>({0,1}));}
        {Fixture f;auto heads=f.m.object(0x40);f.m.put(heads,std::int32_t{0});f.m.put(heads+4,std::int32_t{2});
            f.m.put(f.owner+0x68,heads);f.m.put(f.owner+0x70,std::int32_t{2});f.m.put(f.rows+16,std::int32_t{-1});
            CHECK(f.capture());
            f.m.put(heads,std::int32_t{-1});f.m.put(heads+4,std::int32_t{0});f.m.put(f.rows+16,std::int32_t{2});
            CHECK(!f.capture());}
        {Fixture f;unsigned visits{};auto r=ffr::capture_timeline_links([&](auto p,void* b,std::size_t s) {
                if(!f.m.read(p,b,s))return false;
                if(p==f.models[0]+0x70 && ++visits>1)std::memset(b,0,s);return true;
            },f.ui,f.parameters);CHECK(!r);}
        {Fixture f;CHECK(!ffr::capture_timeline_links([](auto,auto,auto)->bool {throw std::runtime_error("read");},f.ui,f.parameters));}
        std::cout<<"Read-only timeline links, sparse maps, stale/null references and drift guards passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
