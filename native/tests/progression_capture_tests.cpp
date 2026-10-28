#include "progression_capture.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>

#define CHECK(x) do { if (!(x)) throw std::runtime_error("Progression check failed at " + std::to_string(__LINE__)); } while (false)
int main() {
    try {
        std::array<unsigned char, 32> root{};
        std::uint64_t klass=1; std::memcpy(root.data()+16, &klass, sizeof(klass));
        std::vector<unsigned char> bytes{'G','V','A','S',3,0,0,0,42,7,9,0};
        ffr::Reader reader = [](auto address, auto destination, auto length) {
            std::memcpy(destination, reinterpret_cast<void*>(address), length); return true;
        };
        unsigned releases=0, serializations=0;
        auto get = [&]() -> void* { return root.data(); };
        auto release = [&](void* pointer) noexcept { if (pointer==bytes.data()) ++releases; };
        auto serialize = [&](void* object, ffr::RawArray* result) {
            CHECK(object==root.data()); ++serializations;
            *result={reinterpret_cast<std::uint64_t>(bytes.data()), static_cast<std::int32_t>(bytes.size()), static_cast<std::int32_t>(bytes.size())};
            return true;
        };
        auto copied=ffr::capture_progression(reader, get, serialize, release);
        CHECK(copied && *copied==bytes && releases==1 && serializations==1);
        bytes[8]=99; CHECK((*copied)[8]==42);
        CHECK(!ffr::capture_progression(reader, []() -> void* { return nullptr; }, serialize, release));
        root[11]=0x40;
        CHECK(!ffr::capture_progression(reader, get, serialize, release));
        CHECK(serializations==1 && releases==1); root[11]=0;
        auto failed=[&](void* object, ffr::RawArray* result) { serialize(object,result); return false; };
        CHECK(!ffr::capture_progression(reader,get,failed,release)); CHECK(releases==2);
        auto throwing=[&](void* object, ffr::RawArray* result) -> bool { serialize(object,result); throw std::runtime_error("serializer failure"); };
        bool caught=false;
        try { ffr::capture_progression(reader,get,throwing,release); } catch (...) { caught=true; }
        CHECK(caught && releases==3);
        for (auto count : {-1, 0, 7, ffr::max_progression_bytes+1}) {
            auto invalid=[&](void* object, ffr::RawArray* result) { serialize(object,result); result->count=count; return true; };
            CHECK(!ffr::capture_progression(reader,get,invalid,release));
        }
        CHECK(releases==7);
        auto unreadable=[&](auto address, auto destination, auto length) {
            if (address==reinterpret_cast<std::uint64_t>(bytes.data())) return false;
            return reader(address,destination,length);
        };
        CHECK(!ffr::capture_progression(unreadable,get,serialize,release)); CHECK(releases==8);
        bytes[0]='X';
        CHECK(!ffr::capture_progression(reader,get,serialize,release)); CHECK(releases==9);
        unsigned calls=0;
        ffr::observe_and_forward([&] { ffr::capture_progression(reader,get,throwing,release); }, [&] { ++calls; });
        CHECK(calls==1 && releases==10);
        std::cout << "Detached progression capture and allocator cleanup checks passed.\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
