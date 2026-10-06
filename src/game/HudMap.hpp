#pragma once

#include "game/visual/GdpRenderData.hpp"
#include "world/World.hpp"

#include <span>

namespace gdp::game {

struct HudMapTarget {
    world::RoomId roomId{0U};
    visual::HudMapMarker marker{visual::HudMapMarker::None};
};

[[nodiscard]] visual::HudMapData buildHudMap(
    const world::World& world,
    std::span<const world::RoomId> visitedRooms,
    world::RoomId currentRoom,
    wkc::core::Vec3 playerPosition,
    wkc::core::Vec3 playerForward,
    HudMapTarget target = {}) noexcept;

} // namespace gdp::game
