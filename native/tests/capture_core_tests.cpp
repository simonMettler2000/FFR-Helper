#include "encounter_capture.hpp"
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

#define CHECK(condition) do { if (!(condition)) throw std::runtime_error("Check failed at line " + std::to_string(__LINE__)); } while (false)

struct Memory {
    std::vector<std::byte> bytes = std::vector<std::byte>(65536);
    std::size_t used = 16;
    template<class T> std::uint64_t put(const std::vector<T>& value) {
        used = (used+7)&~std::size_t(7);
        auto address = used+0x10000;
        CHECK(used+value.size()*sizeof(T) <= bytes.size());
        if (!value.empty()) std::memcpy(bytes.data()+used, value.data(), value.size()*sizeof(T));
        used += value.size()*sizeof(T);
        return address;
    }
    template<class T> ffr::RawArray array(const std::vector<T>& value) {
        return {put(value), static_cast<std::int32_t>(value.size()), static_cast<std::int32_t>(value.size())};
    }
    bool read(std::uint64_t address, void* destination, std::size_t length) const {
        if (address < 0x10000 || address-0x10000 > bytes.size() || length > bytes.size()-(address-0x10000)) return false;
        std::memcpy(destination, bytes.data()+address-0x10000, length);
        return true;
    }
    template<class T> void overwrite(std::uint64_t address, const T& value) {
        CHECK(address >= 0x10000 && address-0x10000+sizeof(value) <= bytes.size());
        std::memcpy(bytes.data()+address-0x10000, &value, sizeof(value));
    }
};

int main() {
    try {
        Memory memory;
        ffr::RawEntry entry{};
        entry.friend_ids = memory.array<std::int32_t>({1001, 1002});
        entry.friend_locations = memory.array<std::int32_t>({0, 1});
        entry.enemy_ids = memory.array<std::int32_t>({11, 11, 22});
        entry.enemy_locations = memory.array<std::int32_t>({2, 3, 4});
        auto first_passives = memory.array<std::int32_t>({77, 88});
        entry.passives = memory.array<ffr::RawArray>({first_passives, memory.array<std::int32_t>({99})});
        entry.bgm = memory.array<char16_t>({u'T', u'e', u's', u't', u'"', u'\n', u'\\', 0xd83d, 0xde03, 0});
        entry.can_escape = 1; entry.escape_probability = 0.75f; entry.group = 12; entry.stage = 34;
        entry.event = -1; entry.ap = 5; entry.region = 6; entry.preemption = 2;
        entry.notify_preemption = 1; entry.bounty = 1;
        auto address = memory.put<ffr::RawEntry>({entry});
        ffr::Reader reader = [&](auto a, auto d, auto l) { return memory.read(a, d, l); };
        auto copied = ffr::capture_entry(reader, address, true);
        CHECK(copied.has_value());
        CHECK(copied->friend_ids == std::vector<std::int32_t>({1001, 1002}));
        CHECK(copied->enemy_ids == std::vector<std::int32_t>({11, 11, 22}));
        CHECK(copied->passives.size() == 2 && copied->passives[0][1] == 88);
        CHECK(copied->group == 12 && copied->stage == 34 && copied->event == -1);
        CHECK(copied->preemption == 2 && copied->can_escape && copied->skip_intro && copied->bounty);
        const auto original_json = ffr::to_json(*copied);
        memory.overwrite<std::int32_t>(entry.friend_ids.data, 9999);
        memory.overwrite<std::int32_t>(first_passives.data, 123456);
        CHECK(ffr::to_json(*copied) == original_json); // No remote references survived.
        CHECK(original_json.find("\\\"") != std::string::npos);
        CHECK(original_json.find("\\u000a") != std::string::npos);

        auto rejected = [&](ffr::RawEntry invalid) {
            memory.overwrite(address, invalid);
            CHECK(!ffr::capture_entry(reader, address, false));
        };
        auto invalid = entry; invalid.enemy_ids.count = -1; rejected(invalid);
        invalid = entry; invalid.enemy_ids.count = 65; rejected(invalid);
        invalid = entry; invalid.enemy_ids.capacity = 1; rejected(invalid);
        invalid = entry; invalid.enemy_ids.data = 0; rejected(invalid);
        invalid = entry; invalid.enemy_ids.data = std::numeric_limits<std::uint64_t>::max()-10; rejected(invalid);
        invalid = entry; invalid.passives.count = 65; rejected(invalid);
        invalid = entry; invalid.bgm.count = 257; rejected(invalid);
        invalid = entry; invalid.bgm = memory.array<char16_t>({u'N', u'o'}); rejected(invalid);
        invalid = entry; invalid.bgm = memory.array<char16_t>({0xd800, u'x', 0}); rejected(invalid);
        invalid = entry; invalid.bgm = memory.array<char16_t>({0xdc00, 0}); rejected(invalid);
        invalid = entry; invalid.can_escape = 2; rejected(invalid);
        invalid = entry; invalid.escape_probability = std::numeric_limits<float>::quiet_NaN(); rejected(invalid);
        invalid = entry; invalid.escape_probability = std::numeric_limits<float>::infinity(); rejected(invalid);
        CHECK(!ffr::capture_entry(reader, 0, false));
        CHECK(!ffr::capture_entry(reader, 0x100000000, false));
        CHECK(!ffr::capture_entry([](auto, auto, auto) { return false; }, address, false));
        memory.overwrite(address, ffr::RawEntry{});
        auto empty = ffr::capture_entry(reader, address, false);
        CHECK(empty && empty->bgm.empty() && empty->enemy_ids.empty());
        int forwarded = 0;
        ffr::observe_and_forward([] { throw std::runtime_error("Capture failure"); }, [&] { ++forwarded; });
        CHECK(forwarded == 1);
        bool propagated = false;
        try {
            ffr::observe_and_forward([] {}, [&] { ++forwarded; throw std::runtime_error("Original failure"); });
        } catch (const std::runtime_error&) { propagated = true; }
        CHECK(propagated && forwarded == 2);
        std::cout << original_json << '\n';
        std::cout << "Detached native encounter capture checks passed.\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
