#pragma once
#include "encounter_capture.hpp"
#include <cstring>

namespace ffr {
struct FieldAnchor {
    std::int32_t map_id{};
    std::array<double,3> position{};
    std::uint8_t direction{};
};
// Read-only layout from the native map save-preparation and actor-location
// thunks. This is position/direction evidence, not a serialized world or restore.
inline std::optional<FieldAnchor> capture_field_anchor(const Reader& read,std::int32_t map_id,std::uint64_t player) {
    auto object=[&](std::uint64_t address) {
        std::array<std::uint64_t,4> header{};
        return address && read(address,header.data(),sizeof(header)) && header[0] && header[2]
            && !(header[1]&0x40000000u);
    };
    if(map_id<0 || !object(player))return {};
    std::uint64_t component{},operation{};
    if(!read(player+0x1b8,&component,sizeof(component))||!object(component)
        ||!read(player+0x778,&operation,sizeof(operation))||!object(operation))return {};
    FieldAnchor value;value.map_id=map_id;
    if(!read(component+0x200,value.position.data(),sizeof(value.position))
        ||!read(operation+0xc9,&value.direction,1)||value.direction>8)return {};
    for(double coordinate:value.position)if(!std::isfinite(coordinate)||std::abs(coordinate)>1e12)return {};
    return value;
}
inline std::string to_json(const FieldAnchor& value) {
    std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17);
    out<<"{\"kind\":\"partial-field-anchor\",\"restorable\":false,\"source\":\"live-player-root-transform\","
          "\"coverage\":\"position-and-direction-only\",\"sameWorldChecked\":true,\"mapId\":"<<value.map_id
        <<",\"position\":["<<value.position[0]<<','<<value.position[1]<<','<<value.position[2]
        <<"],\"direction\":"<<static_cast<unsigned>(value.direction)<<'}';return out.str();
}
} // namespace ffr
