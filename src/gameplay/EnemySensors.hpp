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

enum class TacticalEnemySensor : std::size_t {
    PlayerDistance,
    PlayerAngle,
    PlayerVisible,
    Health,
    WallFront,
    WallLeft,
    WallRight,
    PlayerVelocityForward,
    PlayerVelocityRight,
    PlayerAimThreat,
    Ammo,
    Reloading,
    Count,
};

using TacticalEnemySensorValues = std::array<float,
    static_cast<std::size_t>(TacticalEnemySensor::Count)>;

enum class CooperativeEnemySensor : std::size_t {
    PlayerDistance,
    PlayerAngle,
    PlayerVisible,
    Health,
    WallFront,
    WallLeft,
    WallRight,
    PlayerVelocityForward,
    PlayerVelocityRight,
    PlayerAimThreat,
    Ammo,
    Reloading,
    AllyDistance,
    AllyAngle,
    AllyHealth,
    FireLaneBlocked,
    KnownTargetDistance,
    KnownTargetAngle,
    KnownTargetFreshness,
    Count,
};

using CooperativeEnemySensorValues = std::array<float,
    static_cast<std::size_t>(CooperativeEnemySensor::Count)>;

[[nodiscard]] EnemySensorValues sampleEnemySensors(
    const gdp::entities::Entity& entity,
    const gdp::entities::EnemyData& enemy,
    const Player& player,
    const CollisionWorld& collisionWorld,
    bool playerVisible,
    float detectionRange) noexcept;

[[nodiscard]] TacticalEnemySensorValues sampleTacticalEnemySensors(
    const gdp::entities::Entity& entity,
    const gdp::entities::EnemyData& enemy,
    const Player& player,
    const CollisionWorld& collisionWorld,
    bool playerVisible,
    float detectionRange) noexcept;

[[nodiscard]] CooperativeEnemySensorValues sampleCooperativeEnemySensors(
    const gdp::entities::EntityManager& manager,
    const gdp::entities::Entity& entity,
    const gdp::entities::EnemyData& enemy,
    const Player& player,
    const CollisionWorld& collisionWorld,
    bool playerVisible,
    float detectionRange) noexcept;

} // namespace gdp::gameplay
