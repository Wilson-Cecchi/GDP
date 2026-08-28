#include "gameplay/Collision.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace gdp::gameplay {
namespace {

gdp::core::Aabb transformBounds(
    const gdp::core::Aabb localBounds,
    const gdp::core::Vec3 origin,
    const gdp::core::QuarterTurn rotation) noexcept
{
    const std::array corners{
        gdp::core::Vec3{localBounds.minimum.x, localBounds.minimum.y, localBounds.minimum.z},
        gdp::core::Vec3{localBounds.maximum.x, localBounds.minimum.y, localBounds.minimum.z},
        gdp::core::Vec3{localBounds.minimum.x, localBounds.maximum.y, localBounds.minimum.z},
        gdp::core::Vec3{localBounds.maximum.x, localBounds.maximum.y, localBounds.minimum.z},
        gdp::core::Vec3{localBounds.minimum.x, localBounds.minimum.y, localBounds.maximum.z},
        gdp::core::Vec3{localBounds.maximum.x, localBounds.minimum.y, localBounds.maximum.z},
        gdp::core::Vec3{localBounds.minimum.x, localBounds.maximum.y, localBounds.maximum.z},
        gdp::core::Vec3{localBounds.maximum.x, localBounds.maximum.y, localBounds.maximum.z},
    };
    gdp::core::Vec3 minimum
        = origin + gdp::core::rotateQuarterTurns(corners.front(), rotation);
    gdp::core::Vec3 maximum = minimum;
    for (const gdp::core::Vec3 corner : corners) {
        const gdp::core::Vec3 transformed
            = origin + gdp::core::rotateQuarterTurns(corner, rotation);
        minimum.x = std::min(minimum.x, transformed.x);
        minimum.y = std::min(minimum.y, transformed.y);
        minimum.z = std::min(minimum.z, transformed.z);
        maximum.x = std::max(maximum.x, transformed.x);
        maximum.y = std::max(maximum.y, transformed.y);
        maximum.z = std::max(maximum.z, transformed.z);
    }
    return {minimum, maximum};
}

} // namespace

CollisionWorld::CollisionWorld(
    const gdp::core::Aabb roomBounds,
    std::vector<gdp::core::Aabb> solids)
    : roomBounds_(roomBounds)
    , solids_(std::move(solids))
{
}

CollisionWorld CollisionWorld::fixedRoom(
    const gdp::core::Vec3 worldOrigin,
    const gdp::core::QuarterTurn rotation)
{
    constexpr gdp::core::Aabb localRoom{
        {-3.85F, 0.0F, -4.85F}, {3.85F, 4.0F, 4.85F}};
    constexpr std::array localSolids{
        gdp::core::Aabb{{-3.025F, 0.0F, -4.025F}, {-2.575F, 2.0F, -3.575F}},
        gdp::core::Aabb{{2.575F, 0.0F, -4.025F}, {3.025F, 2.0F, -3.575F}},
        gdp::core::Aabb{{-1.1F, 0.0F, -2.8F}, {1.1F, 0.7F, -1.6F}},
        gdp::core::Aabb{{-0.4F, 0.0F, 2.05F}, {0.4F, 2.0F, 2.35F}},
    };
    std::vector<gdp::core::Aabb> solids;
    solids.reserve(localSolids.size());
    for (const gdp::core::Aabb solid : localSolids) {
        solids.push_back(transformBounds(solid, worldOrigin, rotation));
    }
    return {transformBounds(localRoom, worldOrigin, rotation), std::move(solids)};
}

CharacterMotionResult CollisionWorld::moveCharacter(
    gdp::core::Vec3 position,
    gdp::core::Vec3 velocity,
    const float fixedStepSeconds,
    const CharacterCollider collider) const noexcept
{
    CharacterMotionResult result{position, velocity, false, false};
    const float step = std::max(fixedStepSeconds, 0.0F);

    result.position.y += result.velocity.y * step;
    if (result.position.y <= roomBounds_.minimum.y) {
        result.position.y = roomBounds_.minimum.y;
        result.velocity.y = 0.0F;
        result.grounded = true;
    }
    const float maximumFeetHeight = roomBounds_.maximum.y - collider.height;
    if (result.position.y > maximumFeetHeight) {
        result.position.y = maximumFeetHeight;
        result.velocity.y = std::min(result.velocity.y, 0.0F);
        result.collided = true;
    }

    const float originalX = result.position.x;
    result.position.x += result.velocity.x * step;
    result.position.x = std::clamp(
        result.position.x,
        roomBounds_.minimum.x + collider.radius,
        roomBounds_.maximum.x - collider.radius);
    if (overlapsSolid(result.position, collider)) {
        result.position.x = originalX;
    }
    if (!gdp::core::nearlyEqual(
            result.position.x,
            originalX + (result.velocity.x * step))) {
        result.velocity.x = 0.0F;
        result.collided = true;
    }

    const float originalZ = result.position.z;
    result.position.z += result.velocity.z * step;
    result.position.z = std::clamp(
        result.position.z,
        roomBounds_.minimum.z + collider.radius,
        roomBounds_.maximum.z - collider.radius);
    if (overlapsSolid(result.position, collider)) {
        result.position.z = originalZ;
    }
    if (!gdp::core::nearlyEqual(
            result.position.z,
            originalZ + (result.velocity.z * step))) {
        result.velocity.z = 0.0F;
        result.collided = true;
    }

    return result;
}

const gdp::core::Aabb& CollisionWorld::roomBounds() const noexcept
{
    return roomBounds_;
}

const std::vector<gdp::core::Aabb>& CollisionWorld::solids() const noexcept
{
    return solids_;
}

bool CollisionWorld::overlapsSolid(
    const gdp::core::Vec3 feetPosition,
    const CharacterCollider collider) const noexcept
{
    const float characterTop = feetPosition.y + collider.height;
    for (const gdp::core::Aabb& solid : solids_) {
        if (characterTop <= solid.minimum.y || feetPosition.y >= solid.maximum.y) {
            continue;
        }

        const float closestX = std::clamp(feetPosition.x, solid.minimum.x, solid.maximum.x);
        const float closestZ = std::clamp(feetPosition.z, solid.minimum.z, solid.maximum.z);
        const float differenceX = feetPosition.x - closestX;
        const float differenceZ = feetPosition.z - closestZ;
        if ((differenceX * differenceX) + (differenceZ * differenceZ)
            < collider.radius * collider.radius) {
            return true;
        }
    }
    return false;
}

} // namespace gdp::gameplay
