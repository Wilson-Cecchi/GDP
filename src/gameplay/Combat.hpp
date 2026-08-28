#pragma once

#include "core/Math.hpp"
#include "entities/EntityManager.hpp"
#include "gameplay/Collision.hpp"
#include "gameplay/Weapon.hpp"

#include <cstdint>
#include <optional>

namespace gdp::gameplay {

enum class DamageOutcome : std::uint8_t {
    Ignored,
    Damaged,
    Killed,
};

enum class ShotOutcome : std::uint8_t {
    Cooldown,
    Empty,
    Miss,
    ImpactWorld,
    ImpactProp,
    Hit,
    Killed,
    Destroyed,
};

struct HitResult {
    gdp::entities::EntityId target{};
    gdp::core::Vec3 point{};
    float distance{0.0F};
};

struct ShotResult {
    ShotOutcome outcome{ShotOutcome::Miss};
    std::optional<HitResult> hit;
    gdp::entities::EntityId spawnedItem{};
};

[[nodiscard]] DamageOutcome applyDamage(
    gdp::entities::EntityManager& manager,
    gdp::entities::EntityId target,
    float amount) noexcept;

[[nodiscard]] ShotResult fireHitscan(
    Weapon& weapon,
    gdp::core::Vec3 origin,
    gdp::core::Vec3 direction,
    std::uint32_t roomId,
    const CollisionWorld& collisionWorld,
    gdp::entities::EntityManager& manager) noexcept;

[[nodiscard]] bool hasLineOfSight(
    gdp::core::Vec3 origin,
    gdp::core::Vec3 target,
    const CollisionWorld& collisionWorld) noexcept;

[[nodiscard]] bool hasLineOfSightToProp(
    gdp::core::Vec3 origin,
    const gdp::entities::EntityManager& manager,
    gdp::entities::EntityId target,
    const CollisionWorld& collisionWorld) noexcept;

} // namespace gdp::gameplay
