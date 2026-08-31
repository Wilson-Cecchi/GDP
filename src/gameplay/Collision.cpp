#include "gameplay/Collision.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace gdp::gameplay {
namespace {

wkc::core::Aabb combinedBounds(
    const std::vector<wkc::core::Aabb>& volumes) noexcept
{
    if (volumes.empty()) {
        return {};
    }
    wkc::core::Aabb bounds = volumes.front();
    for (const wkc::core::Aabb volume : volumes) {
        bounds.minimum.x = std::min(bounds.minimum.x, volume.minimum.x);
        bounds.minimum.y = std::min(bounds.minimum.y, volume.minimum.y);
        bounds.minimum.z = std::min(bounds.minimum.z, volume.minimum.z);
        bounds.maximum.x = std::max(bounds.maximum.x, volume.maximum.x);
        bounds.maximum.y = std::max(bounds.maximum.y, volume.maximum.y);
        bounds.maximum.z = std::max(bounds.maximum.z, volume.maximum.z);
    }
    return bounds;
}

wkc::core::Aabb transformBounds(
    const wkc::core::Aabb localBounds,
    const wkc::core::Vec3 origin,
    const wkc::core::QuarterTurn rotation) noexcept
{
    const std::array corners{
        wkc::core::Vec3{localBounds.minimum.x, localBounds.minimum.y, localBounds.minimum.z},
        wkc::core::Vec3{localBounds.maximum.x, localBounds.minimum.y, localBounds.minimum.z},
        wkc::core::Vec3{localBounds.minimum.x, localBounds.maximum.y, localBounds.minimum.z},
        wkc::core::Vec3{localBounds.maximum.x, localBounds.maximum.y, localBounds.minimum.z},
        wkc::core::Vec3{localBounds.minimum.x, localBounds.minimum.y, localBounds.maximum.z},
        wkc::core::Vec3{localBounds.maximum.x, localBounds.minimum.y, localBounds.maximum.z},
        wkc::core::Vec3{localBounds.minimum.x, localBounds.maximum.y, localBounds.maximum.z},
        wkc::core::Vec3{localBounds.maximum.x, localBounds.maximum.y, localBounds.maximum.z},
    };
    wkc::core::Vec3 minimum
        = origin + wkc::core::rotateQuarterTurns(corners.front(), rotation);
    wkc::core::Vec3 maximum = minimum;
    for (const wkc::core::Vec3 corner : corners) {
        const wkc::core::Vec3 transformed
            = origin + wkc::core::rotateQuarterTurns(corner, rotation);
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
    const wkc::core::Aabb roomBounds,
    std::vector<wkc::core::Aabb> solids)
    : roomBounds_(roomBounds)
    , walkableVolumes_{roomBounds}
    , solids_(std::move(solids))
{
}

CollisionWorld::CollisionWorld(
    const WalkableVolumesTag,
    std::vector<wkc::core::Aabb> walkableVolumes,
    std::vector<wkc::core::Aabb> solids)
    : roomBounds_(combinedBounds(walkableVolumes))
    , walkableVolumes_(std::move(walkableVolumes))
    , solids_(std::move(solids))
{
}

CollisionWorld CollisionWorld::fromWalkableVolumes(
    std::vector<wkc::core::Aabb> walkableVolumes,
    std::vector<wkc::core::Aabb> solids)
{
    return {
        WalkableVolumesTag{},
        std::move(walkableVolumes),
        std::move(solids),
    };
}

CollisionWorld CollisionWorld::fixedRoom(
    const wkc::core::Vec3 worldOrigin,
    const wkc::core::QuarterTurn rotation)
{
    constexpr wkc::core::Aabb localRoom{
        {-3.85F, 0.0F, -4.85F}, {3.85F, 4.0F, 4.85F}};
    constexpr std::array localSolids{
        wkc::core::Aabb{{-3.025F, 0.0F, -4.025F}, {-2.575F, 2.0F, -3.575F}},
        wkc::core::Aabb{{2.575F, 0.0F, -4.025F}, {3.025F, 2.0F, -3.575F}},
        wkc::core::Aabb{{-1.1F, 0.0F, -2.8F}, {1.1F, 0.7F, -1.6F}},
        wkc::core::Aabb{{-0.4F, 0.0F, 2.05F}, {0.4F, 2.0F, 2.35F}},
    };
    std::vector<wkc::core::Aabb> solids;
    solids.reserve(localSolids.size());
    for (const wkc::core::Aabb solid : localSolids) {
        solids.push_back(transformBounds(solid, worldOrigin, rotation));
    }
    return {transformBounds(localRoom, worldOrigin, rotation), std::move(solids)};
}

CharacterMotionResult CollisionWorld::moveCharacter(
    wkc::core::Vec3 position,
    wkc::core::Vec3 velocity,
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
    if (!canOccupy(result.position, collider)) {
        result.position.x = originalX;
    }
    if (!wkc::core::nearlyEqual(
            result.position.x,
            originalX + (result.velocity.x * step))) {
        result.velocity.x = 0.0F;
        result.collided = true;
    }

    const float originalZ = result.position.z;
    result.position.z += result.velocity.z * step;
    if (!canOccupy(result.position, collider)) {
        result.position.z = originalZ;
    }
    if (!wkc::core::nearlyEqual(
            result.position.z,
            originalZ + (result.velocity.z * step))) {
        result.velocity.z = 0.0F;
        result.collided = true;
    }

    return result;
}

const wkc::core::Aabb& CollisionWorld::roomBounds() const noexcept
{
    return roomBounds_;
}

const std::vector<wkc::core::Aabb>& CollisionWorld::walkableVolumes() const noexcept
{
    return walkableVolumes_;
}

const std::vector<wkc::core::Aabb>& CollisionWorld::solids() const noexcept
{
    return solids_;
}

bool CollisionWorld::canOccupy(
    const wkc::core::Vec3 feetPosition,
    const CharacterCollider collider) const noexcept
{
    return insideWalkable(feetPosition, collider)
        && !overlapsSolid(feetPosition, collider);
}

bool CollisionWorld::insideWalkable(
    const wkc::core::Vec3 feetPosition,
    const CharacterCollider collider) const noexcept
{
    const float top = feetPosition.y + collider.height;
    for (const wkc::core::Aabb volume : walkableVolumes_) {
        if (feetPosition.x >= volume.minimum.x + collider.radius
            && feetPosition.x <= volume.maximum.x - collider.radius
            && feetPosition.z >= volume.minimum.z + collider.radius
            && feetPosition.z <= volume.maximum.z - collider.radius
            && feetPosition.y >= volume.minimum.y - 1.0e-4F
            && top <= volume.maximum.y + 1.0e-4F) {
            return true;
        }
    }
    return false;
}

bool CollisionWorld::overlapsSolid(
    const wkc::core::Vec3 feetPosition,
    const CharacterCollider collider) const noexcept
{
    const float characterTop = feetPosition.y + collider.height;
    for (const wkc::core::Aabb& solid : solids_) {
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
