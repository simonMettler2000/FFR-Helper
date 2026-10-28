#pragma once
// Own native encounter layout recovered from build 24250053. No game code or
// pointers are exported. Capturing this struct does not capture a full battle.
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace ffr {

struct RawArray {
    std::uint64_t data;
    std::int32_t count;
    std::int32_t capacity;
};
struct RawEntry {
    RawArray friend_ids, friend_locations, enemy_ids, enemy_locations, passives;
    std::uint8_t can_escape;
    std::array<std::uint8_t, 3> padding1;
    float escape_probability;
    std::uint8_t skip_win_result, skip_lose_result;
    std::array<std::uint8_t, 2> padding2;
    std::int32_t group, stage, event;
    RawArray bgm;
    std::int32_t ap, region;
    std::uint8_t preemption, notify_preemption, battle_test, continue_bgm, bounty;
    std::array<std::uint8_t, 3> padding3;
};
static_assert(sizeof(RawArray) == 16);
static_assert(sizeof(RawEntry) == 136);
static_assert(offsetof(RawEntry, can_escape) == 80);
static_assert(offsetof(RawEntry, escape_probability) == 84);
static_assert(offsetof(RawEntry, group) == 92);
static_assert(offsetof(RawEntry, bgm) == 104);
static_assert(offsetof(RawEntry, ap) == 120);
static_assert(offsetof(RawEntry, preemption) == 128);

struct Encounter {
    std::vector<std::int32_t> friend_ids, friend_locations, enemy_ids, enemy_locations;
    std::vector<std::vector<std::int32_t>> passives;
    std::string bgm;
    std::int32_t group{}, stage{}, event{}, ap{}, region{};
    float escape_probability{};
    std::uint8_t preemption{};
    bool can_escape{}, skip_win_result{}, skip_lose_result{}, notify_preemption{};
    bool battle_test{}, continue_bgm{}, bounty{}, skip_intro{};
};

using Reader = std::function<bool(std::uint64_t, void*, std::size_t)>;

template<class Observe, class Forward>
inline void observe_and_forward(Observe&& observe, Forward&& forward) {
    try { observe(); } catch (...) { /* Observation must never suppress gameplay. */ }
    forward();
}

inline bool valid_array(const RawArray& array, std::int32_t maximum) {
    return array.count >= 0 && array.count <= maximum && array.capacity >= array.count
        && array.capacity <= 1'048'576 && (array.count == 0 || array.data != 0);
}

template <class T>
inline bool copy_array(const Reader& read, RawArray source, std::int32_t maximum, std::vector<T>& out) {
    if (!valid_array(source, maximum)) return false;
    out.resize(static_cast<std::size_t>(source.count));
    return out.empty() || read(source.data, out.data(), out.size()*sizeof(T));
}

inline std::optional<std::string> utf8(const std::vector<char16_t>& units) {
    if (!units.empty() && units.back() != 0) return {};
    std::string out;
    for (std::size_t i=0; i+1<units.size(); ++i) {
        std::uint32_t code = units[i];
        if (code == 0) return {}; // FString length must describe the actual value.
        if (code >= 0xd800 && code <= 0xdbff) {
            if (i+2 >= units.size() || units[i+1] < 0xdc00 || units[i+1] > 0xdfff) return {};
            code = 0x10000 + ((code-0xd800)<<10) + (units[++i]-0xdc00);
        } else if (code >= 0xdc00 && code <= 0xdfff) return {};
        if (code < 0x80) out += static_cast<char>(code);
        else if (code < 0x800) {
            out += static_cast<char>(0xc0 | code>>6);
            out += static_cast<char>(0x80 | (code&0x3f));
        } else if (code < 0x10000) {
            out += static_cast<char>(0xe0 | code>>12);
            out += static_cast<char>(0x80 | ((code>>6)&0x3f));
            out += static_cast<char>(0x80 | (code&0x3f));
        } else {
            out += static_cast<char>(0xf0 | code>>18);
            out += static_cast<char>(0x80 | ((code>>12)&0x3f));
            out += static_cast<char>(0x80 | ((code>>6)&0x3f));
            out += static_cast<char>(0x80 | (code&0x3f));
        }
    }
    return out;
}

inline std::optional<Encounter> capture_entry(const Reader& read, std::uint64_t address, bool skip_intro) {
    RawEntry raw{};
    if (!address || !read(address, &raw, sizeof(raw)) || !std::isfinite(raw.escape_probability)) return {};
    for (auto flag : {raw.can_escape, raw.skip_win_result, raw.skip_lose_result, raw.notify_preemption,
                      raw.battle_test, raw.continue_bgm, raw.bounty}) if (flag > 1) return {};
    Encounter out{};
    if (!copy_array(read, raw.friend_ids, 64, out.friend_ids)
        || !copy_array(read, raw.friend_locations, 64, out.friend_locations)
        || !copy_array(read, raw.enemy_ids, 64, out.enemy_ids)
        || !copy_array(read, raw.enemy_locations, 64, out.enemy_locations)) return {};
    std::vector<RawArray> passive_groups;
    if (!copy_array(read, raw.passives, 64, passive_groups)) return {};
    for (const auto& group : passive_groups) {
        std::vector<std::int32_t> skills;
        if (!copy_array(read, group, 128, skills)) return {};
        out.passives.push_back(std::move(skills));
    }
    std::vector<char16_t> text;
    if (!copy_array(read, raw.bgm, 256, text)) return {};
    auto name = utf8(text);
    if (!name) return {};
    out.bgm = std::move(*name);
    out.group = raw.group; out.stage = raw.stage; out.event = raw.event;
    out.ap = raw.ap; out.region = raw.region; out.preemption = raw.preemption;
    out.escape_probability = raw.escape_probability;
    out.can_escape = raw.can_escape != 0;
    out.skip_win_result = raw.skip_win_result != 0;
    out.skip_lose_result = raw.skip_lose_result != 0;
    out.notify_preemption = raw.notify_preemption != 0;
    out.battle_test = raw.battle_test != 0;
    out.continue_bgm = raw.continue_bgm != 0;
    out.bounty = raw.bounty != 0;
    out.skip_intro = skip_intro;
    return out;
}

inline std::string quote(const std::string& value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result = "\"";
    for (unsigned char byte : value) {
        if (byte == '"' || byte == '\\') { result += '\\'; result += static_cast<char>(byte); }
        else if (byte < 0x20) { result += "\\u00"; result += digits[byte>>4]; result += digits[byte&15]; }
        else result += static_cast<char>(byte);
    }
    return result+'"';
}

inline std::string integers(const std::vector<std::int32_t>& values) {
    std::string out = "[";
    for (std::size_t i=0; i<values.size(); ++i) {
        if (i) out += ',';
        out += std::to_string(values[i]);
    }
    return out+']';
}

inline std::string to_json(const Encounter& value) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::boolalpha << std::setprecision(9);
    out << "{\"friendliesUnitIdList\":" << integers(value.friend_ids)
        << ",\"friendliesLocationIdList\":" << integers(value.friend_locations)
        << ",\"enemiesUnitIdList\":" << integers(value.enemy_ids)
        << ",\"enemiesLocationIdList\":" << integers(value.enemy_locations)
        << ",\"addPassiveDataList\":[";
    for (std::size_t i=0; i<value.passives.size(); ++i) {
        if (i) out << ',';
        out << integers(value.passives[i]);
    }
    out << "],\"bgmCueName\":" << quote(value.bgm)
        << ",\"battleGroupId\":" << value.group << ",\"battleStageId\":" << value.stage
        << ",\"battleEventId\":" << value.event << ",\"ap\":" << value.ap << ",\"regionId\":" << value.region
        << ",\"preemptiveRelation\":" << static_cast<unsigned>(value.preemption)
        << ",\"canEscape\":" << value.can_escape << ",\"probabilityOfSuccessfulEscape\":" << value.escape_probability
        << ",\"isResultSkipOnWin\":" << value.skip_win_result << ",\"isResultSkipOnLose\":" << value.skip_lose_result
        << ",\"isNotifyPreemtiveAttack\":" << value.notify_preemption << ",\"isStartupByBattleTest\":" << value.battle_test
        << ",\"isContinueBGM\":" << value.continue_bgm << ",\"isBounty\":" << value.bounty
        << ",\"skipIntro\":" << value.skip_intro << '}';
    return out.str();
}

} // namespace ffr
