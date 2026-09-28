#pragma once

#include "game/visual/GdpRenderData.hpp"
#include "world/World.hpp"

#include <span>

namespace gdp::game {

[[nodiscard]] visual::HudMapData buildHudMap(
    const world::World& world,
    std::span<const world::RoomId> visitedRooms,
    world::RoomId currentRoom,
    wkc::core::Vec3 playerPosition,
    wkc::core::Vec3 playerForward) noexcept;

} // namespace gdp::game
