#pragma once
#include "encounter_capture.hpp"
#include <cstring>

namespace ffr {
inline constexpr std::int32_t max_progression_bytes = 16*1024*1024;

// The game owns the serializer's output allocation. Copy it into our allocator,
// then return the original buffer through that same game's allocator on every
// success/failure/exception path. Neither a UObject nor an engine TArray survives.
template<class GetRoot, class Serialize, class Release>
std::optional<std::vector<unsigned char>> capture_progression(
        const Reader& read, GetRoot&& get_root, Serialize&& serialize, Release&& release) {
    auto* root = get_root();
    if (!root) return {};
    std::array<unsigned char, 32> object{};
    if (!read(reinterpret_cast<std::uint64_t>(root), object.data(), object.size())) return {};
    std::uint32_t flags{};
    std::uint64_t klass{};
    std::memcpy(&flags, object.data()+8, sizeof(flags));
    std::memcpy(&klass, object.data()+16, sizeof(klass));
    if ((flags & 0x40000000u) != 0 || klass == 0) return {};
    RawArray output{};
    struct Cleanup {
        RawArray& output;
        Release& release;
        ~Cleanup() { if (output.data) release(reinterpret_cast<void*>(output.data)); }
    } cleanup{output, release};
    if (!serialize(root, &output) || output.count < 8 || output.count > max_progression_bytes
        || output.capacity < output.count || output.capacity > 64*1024*1024 || output.data == 0) return {};
    std::vector<unsigned char> copy(static_cast<std::size_t>(output.count));
    if (!read(output.data, copy.data(), copy.size())) return {};
    constexpr unsigned char magic[]{'G','V','A','S',3,0,0,0};
    if (std::memcmp(copy.data(), magic, sizeof(magic)) != 0) return {};
    return copy;
}
} // namespace ffr
