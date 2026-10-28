#pragma once
#include <cstdint>
#include <string>
#include <vector>
// Synthetic empty save used only by isolated tests. Not a captured player save.
inline std::vector<unsigned char> clone_fixture_payload() {
    std::vector<unsigned char> out;
    auto append=[&]<class T>(T value) {const auto* p=reinterpret_cast<const unsigned char*>(&value);out.insert(out.end(),p,p+sizeof(value));};
    auto string=[&](const std::string& value) {append(static_cast<std::int32_t>(value.size()+1));out.insert(out.end(),value.begin(),value.end());out.push_back(0);};
    append(std::uint32_t{0x53415647});append(std::uint32_t{3});append(std::uint32_t{522});append(std::uint32_t{1017});
    append(std::uint16_t{5});append(std::uint16_t{6});append(std::uint16_t{1});append(std::uint32_t{0});
    string("++UE5+Release-5.6");append(std::uint32_t{3});append(std::uint32_t{0});
    string("/Script/mucha.CPP_MuchaGameSaveData");out.push_back(0);string("None");append(std::uint32_t{0});return out;
}
