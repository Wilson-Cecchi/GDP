#pragma once

#include "core/Math.hpp"
#include "entities/EntityManager.hpp"
#include "gameplay/Collision.hpp"

#include <cstdint>

namespace gdp::gameplay {

enum class PropDamageOutcome : std::uint8_t {
    Ignored,
    Damaged,
    Destroyed,
};

struct PropDamageResult {
    PropDamageOutcome outcome{PropDamageOutcome::Ignored};
    gdp::entities::EntityId spawnedDrop{};
};

[[nodiscard]] gdp::core::Aabb propCollisionBounds(
    const gdp::entities::Entity& entity,
    const gdp::entities::PropData& prop) noexcept;

[[nodiscard]] CollisionWorld collisionWorldWithProps(
    const CollisionWorld& base,
    const gdp::entities::EntityManager& manager,
    std::uint32_t roomId);

[[nodiscard]] PropDamageResult damageProp(
    gdp::entities::EntityManager& manager,
    gdp::entities::EntityId target,
    float amount) noexcept;

[[nodiscard]] bool repairProp(
    gdp::entities::EntityManager& manager,
    gdp::entities::EntityId target,
    float amount) noexcept;

} // namespace gdp::gameplay
