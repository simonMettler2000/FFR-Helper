#pragma once
#include "encounter_capture.hpp"
#include <cstring>
#include <stdexcept>
#include <utility>

namespace ffr {
inline std::vector<char16_t> utf16(const std::string& value) {
    std::vector<char16_t> out;
    for (std::size_t i=0; i<value.size();) {
        const auto lead=static_cast<unsigned char>(value[i++]);
        std::uint32_t code=lead, minimum=0;
        unsigned continuation=0;
        if (lead==0) throw std::invalid_argument("Embedded NUL in encounter string");
        if (lead>=0xc2 && lead<=0xdf) { code=lead&31; continuation=1; minimum=0x80; }
        else if (lead>=0xe0 && lead<=0xef) { code=lead&15; continuation=2; minimum=0x800; }
        else if (lead>=0xf0 && lead<=0xf4) { code=lead&7; continuation=3; minimum=0x10000; }
        else if (lead>=0x80) throw std::invalid_argument("Invalid UTF-8 encounter string");
        if (i+continuation>value.size()) throw std::invalid_argument("Truncated UTF-8 encounter string");
        for (unsigned n=0; n<continuation; ++n) {
            const auto next=static_cast<unsigned char>(value[i++]);
            if ((next&0xc0)!=0x80) throw std::invalid_argument("Invalid UTF-8 continuation");
            code=(code<<6)|(next&63);
        }
        if (code<minimum || code>0x10ffff || (code>=0xd800 && code<=0xdfff))
            throw std::invalid_argument("Invalid UTF-8 scalar");
        if (code<0x10000) out.push_back(static_cast<char16_t>(code));
        else { code-=0x10000; out.push_back(static_cast<char16_t>(0xd800+(code>>10))); out.push_back(static_cast<char16_t>(0xdc00+(code&1023))); }
        if (out.size()>255) throw std::invalid_argument("Encounter string exceeds its bound");
    }
    if (!out.empty()) out.push_back(0);
    return out;
}

// Rebuilds owned engine arrays, never pointers into captured vectors. Allocate
// and Release MUST be paired functions from the same verified game allocator.
// Before transfer(), this scope frees every allocation on failure/destruction.
// After transfer(), the caller must use the verified consuming MAP entry path,
// which deep-copies the descriptor and destroys all its input arrays on return.
// Calling its downstream battle consumer alone does not perform that cleanup.
// Never helper-free transferred blocks after an uncertain native invocation.
template<class Allocate, class Release>
class EngineEncounter {
    Allocate allocate;
    Release release;
    std::vector<void*> blocks;
    RawEntry entry{};
    bool transferred=false;
    template<class T> RawArray array(const std::vector<T>& source, std::size_t maximum) {
        if (source.size()>maximum) throw std::invalid_argument("Encounter array exceeds its bound");
        if (source.empty()) return {};
        auto* memory=allocate(source.size()*sizeof(T));
        if (!memory) throw std::bad_alloc();
        try { blocks.push_back(memory); } catch (...) { release(memory); throw; }
        std::memcpy(memory,source.data(),source.size()*sizeof(T));
        return {reinterpret_cast<std::uint64_t>(memory),static_cast<std::int32_t>(source.size()),static_cast<std::int32_t>(source.size())};
    }
    void clear() noexcept { for (auto it=blocks.rbegin(); it!=blocks.rend(); ++it) release(*it); blocks.clear(); }
public:
    EngineEncounter(const Encounter& source, Allocate allocator, Release releaser) : allocate(std::move(allocator)),release(std::move(releaser)) {
        try {
            if (!std::isfinite(source.escape_probability) || source.passives.size()>64)
                throw std::invalid_argument("Invalid encounter scalar or passive groups");
            entry.friend_ids=array(source.friend_ids,64); entry.friend_locations=array(source.friend_locations,64);
            entry.enemy_ids=array(source.enemy_ids,64); entry.enemy_locations=array(source.enemy_locations,64);
            std::vector<RawArray> nested;
            for (const auto& skills : source.passives) nested.push_back(array(skills,128));
            entry.passives=array(nested,64); entry.bgm=array(utf16(source.bgm),256);
            entry.group=source.group; entry.stage=source.stage; entry.event=source.event;
            entry.ap=source.ap; entry.region=source.region; entry.preemption=source.preemption;
            entry.escape_probability=source.escape_probability; entry.can_escape=source.can_escape;
            entry.skip_win_result=source.skip_win_result; entry.skip_lose_result=source.skip_lose_result;
            entry.notify_preemption=source.notify_preemption; entry.battle_test=source.battle_test;
            entry.continue_bgm=source.continue_bgm; entry.bounty=source.bounty;
        } catch (...) { clear(); throw; }
    }
    EngineEncounter(const EngineEncounter&)=delete;
    EngineEncounter& operator=(const EngineEncounter&)=delete;
    ~EngineEncounter() { clear(); }
    const RawEntry& view() const { return entry; }
    RawEntry transfer() {
        if (transferred) throw std::logic_error("Encounter ownership was already transferred");
        transferred=true; blocks.clear(); return entry;
    }
};
} // namespace ffr
