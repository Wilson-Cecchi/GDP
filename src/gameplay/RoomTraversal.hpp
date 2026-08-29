#pragma once

#include "core/Math.hpp"
#include "gameplay/Collision.hpp"
#include "world/World.hpp"

#include <optional>
#include <span>

namespace gdp::gameplay {

struct RoomTransition {
    gdp::world::EdgeId edge{0};
    gdp::world::RoomId sourceRoom{0};
    gdp::world::RoomId destinationRoom{0};
    gdp::core::Vec3 destinationPosition{};
    float destinationYawDegrees{0.0F};
};

[[nodiscard]] std::optional<RoomTransition> findRoomTransition(
    const gdp::world::World& world,
    gdp::world::RoomId currentRoom,
    gdp::core::Vec3 playerFeetPosition,
    gdp::core::Vec3 playerForward,
    float maximumDistance = 1.35F,
    float destinationInset = 0.85F) noexcept;

[[nodiscard]] CollisionWorld makeRoomNavigationCollision(
    const gdp::world::World& world,
    gdp::world::RoomId currentRoom,
    bool doorsLocked = false,
    std::span<const gdp::world::EdgeId> blockedEdges = {});

[[nodiscard]] std::optional<gdp::world::RoomId> roomContainingPosition(
    const gdp::world::World& world,
    gdp::core::Vec3 feetPosition) noexcept;

[[nodiscard]] std::optional<gdp::world::RoomId> roomContainingCharacter(
    const gdp::world::World& world,
    gdp::core::Vec3 feetPosition,
    CharacterCollider collider = {}) noexcept;

[[nodiscard]] std::optional<float> corridorProgressAt(
    const gdp::world::CorridorPath& corridor,
    gdp::core::Vec3 feetPosition) noexcept;

} // namespace gdp::gameplay
