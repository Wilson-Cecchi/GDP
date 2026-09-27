#pragma once

#include "entities/EntityManager.hpp"
#include "gameplay/Collision.hpp"
#include "gameplay/Player.hpp"

#include <array>
#include <cstddef>

namespace gdp::gameplay {

// All channels are in [-1, 1]. Hidden player position is never encoded.
enum class EnemySensor : std::size_t {
    PlayerDistance,
    PlayerAngle,
    PlayerVisible,
    Health,
    WallFront,
    WallLeft,
    WallRight,
    Count,
};

using EnemySensorValues = std::array<float, static_cast<std::size_t>(EnemySensor::Count)>;

[[nodiscard]] EnemySensorValues sampleEnemySensors(
    const gdp::entities::Entity& entity,
    const gdp::entities::EnemyData& enemy,
    const Player& player,
    const CollisionWorld& collisionWorld,
    bool playerVisible,
    float detectionRange) noexcept;

} // namespace gdp::gameplay
