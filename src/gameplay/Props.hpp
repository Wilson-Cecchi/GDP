#pragma once

#include "core/Math.hpp"
#include "entities/EntityManager.hpp"
#include "gameplay/Collision.hpp"

#include <array>
#include <cstdint>
#include <span>

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

[[nodiscard]] wkc::core::Aabb propCollisionBounds(
    const gdp::entities::Entity& entity,
    const gdp::entities::PropData& prop) noexcept;

struct PropSolidBounds {
    std::array<wkc::core::Aabb, 5U> parts{};
    std::size_t count{0U};
};

[[nodiscard]] PropSolidBounds terminalPhysicalBounds(
    wkc::core::Vec3 position, wkc::core::Vec3 scale) noexcept;

// The terminal has open gaps. These parts are the opaque physical surfaces
// used by walking collision, shots and projectile interception.
[[nodiscard]] PropSolidBounds propPhysicalBounds(
    const gdp::entities::Entity& entity,
    const gdp::entities::PropData& prop) noexcept;

[[nodiscard]] CollisionWorld collisionWorldWithProps(
    const CollisionWorld& base,
    const gdp::entities::EntityManager& manager,
    std::uint32_t roomId,
    std::span<const std::uint32_t> additionalRoomIds = {});

[[nodiscard]] PropDamageResult damageProp(
    gdp::entities::EntityManager& manager,
    gdp::entities::EntityId target,
    float amount,
    bool directPotHit = false) noexcept;

[[nodiscard]] bool repairProp(
    gdp::entities::EntityManager& manager,
    gdp::entities::EntityId target,
    float amount) noexcept;

} // namespace gdp::gameplay
