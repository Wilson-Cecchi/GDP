#include "gameplay/Collision.hpp"
#include "world/RoomFootprint.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace gdp::gameplay {
namespace {

constexpr float maximumWalkableStep = 0.24F;

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

bool overlapsHorizontally(
    const wkc::core::Vec3 feetPosition,
    const CharacterCollider collider,
    const wkc::core::Aabb solid) noexcept
{
    const float closestX = std::clamp(
        feetPosition.x, solid.minimum.x, solid.maximum.x);
    const float closestZ = std::clamp(
        feetPosition.z, solid.minimum.z, solid.maximum.z);
    const float differenceX = feetPosition.x - closestX;
    const float differenceZ = feetPosition.z - closestZ;
    return (differenceX * differenceX) + (differenceZ * differenceZ)
        < collider.radius * collider.radius;
}

bool fitsHorizontally(
    const wkc::core::Vec3 feetPosition,
    const CharacterCollider collider,
    const wkc::core::Aabb volume) noexcept
{
    return feetPosition.x >= volume.minimum.x + collider.radius
        && feetPosition.x <= volume.maximum.x - collider.radius
        && feetPosition.z >= volume.minimum.z + collider.radius
        && feetPosition.z <= volume.maximum.z - collider.radius;
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
    std::vector<wkc::core::Aabb> solids,
    std::vector<wkc::core::Vec2> walkableOutline,
    std::vector<std::vector<wkc::core::Vec2>> volumeOutlines,
    std::vector<wkc::core::Aabb> octagonalSolids)
    : roomBounds_(combinedBounds(walkableVolumes))
    , walkableVolumes_(std::move(walkableVolumes))
    , walkableOutline_(std::move(walkableOutline))
    , volumeOutlines_(std::move(volumeOutlines))
    , solids_(std::move(solids))
    , octagonalSolids_(std::move(octagonalSolids))
{
    if (volumeOutlines_.empty()) {
        volumeOutlines_.resize(walkableVolumes_.size());
        if (!volumeOutlines_.empty()) volumeOutlines_[0] = walkableOutline_;
    }
}

CollisionWorld CollisionWorld::fromWalkableVolumes(
    std::vector<wkc::core::Aabb> walkableVolumes,
    std::vector<wkc::core::Aabb> solids,
    std::vector<wkc::core::Vec2> walkableOutline,
    std::vector<std::vector<wkc::core::Vec2>> volumeOutlines,
    std::vector<wkc::core::Aabb> octagonalSolids)
{
    return {
        WalkableVolumesTag{},
        std::move(walkableVolumes),
        std::move(solids),
        std::move(walkableOutline),
        std::move(volumeOutlines),
        std::move(octagonalSolids),
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

    const float originalY = result.position.y;
    const float requestedY = originalY + (result.velocity.y * step);
    if (result.velocity.y <= 0.0F) {
        float landingHeight = roomBounds_.minimum.y;
        for (const wkc::core::Aabb solid : solids_) {
            if (overlapsHorizontally(result.position, collider, solid)
                && originalY >= solid.maximum.y - 1.0e-4F
                && requestedY <= solid.maximum.y) {
                landingHeight = std::max(landingHeight, solid.maximum.y);
            }
        }
        if (requestedY <= landingHeight) {
            result.position.y = landingHeight;
            result.velocity.y = 0.0F;
            result.grounded = true;
            result.collided = requestedY < landingHeight - 1.0e-4F;
        } else {
            result.position.y = requestedY;
        }
    } else {
        float ceilingHeight = roomBounds_.maximum.y;
        bool localCeilingFound = false;
        const float originalTop = originalY + collider.height;
        const float requestedTop = requestedY + collider.height;
        for (const wkc::core::Aabb volume : walkableVolumes_) {
            if (fitsHorizontally(result.position, collider, volume)
                && originalY >= volume.minimum.y - 1.0e-4F) {
                ceilingHeight = localCeilingFound
                    ? std::max(ceilingHeight, volume.maximum.y)
                    : volume.maximum.y;
                localCeilingFound = true;
            }
        }
        for (const wkc::core::Aabb solid : solids_) {
            if (overlapsHorizontally(result.position, collider, solid)
                && originalTop <= solid.minimum.y + 1.0e-4F
                && requestedTop >= solid.minimum.y) {
                ceilingHeight = std::min(ceilingHeight, solid.minimum.y);
            }
        }
        if (requestedTop >= ceilingHeight) {
            result.position.y = ceilingHeight - collider.height;
            result.velocity.y = 0.0F;
            result.collided = true;
        } else {
            result.position.y = requestedY;
        }
    }

    const float displacementX = result.velocity.x * step;
    const float displacementZ = result.velocity.z * step;
    const float maximumSubstep = std::max(0.05F, collider.radius * 0.5F);
    const float requiredSteps = std::max(std::abs(displacementX),
        std::abs(displacementZ)) / maximumSubstep;
    const int substeps = std::clamp(
        static_cast<int>(std::ceil(std::min(requiredSteps, 256.0F))), 1, 256);
    const float originalX = result.position.x;
    const float originalZ = result.position.z;
    const auto tryStepUp = [this, collider, &result]() noexcept {
        if (!result.grounded || result.velocity.y > 0.0F) return false;
        float stepTop = result.position.y + maximumWalkableStep + 1.0F;
        for (const wkc::core::Aabb solid : solids_) {
            if (solid.maximum.y <= result.position.y + 1.0e-4F
                || solid.maximum.y > result.position.y + maximumWalkableStep
                || !overlapsHorizontally(result.position, collider, solid)) {
                continue;
            }
            const wkc::core::Vec3 raised{
                result.position.x, solid.maximum.y, result.position.z};
            if (solid.maximum.y < stepTop && canOccupy(raised, collider)) {
                stepTop = solid.maximum.y;
            }
        }
        if (stepTop > result.position.y + maximumWalkableStep) return false;
        result.position.y = stepTop;
        result.velocity.y = 0.0F;
        return true;
    };
    for (int index = 0; index < substeps; ++index) {
        const float fraction = static_cast<float>(index + 1)
            / static_cast<float>(substeps);
        if (result.velocity.x != 0.0F) {
            const float previousX = result.position.x;
            result.position.x = originalX + displacementX * fraction;
            if (!canOccupy(result.position, collider) && !tryStepUp()) {
                result.position.x = previousX;
                result.velocity.x = 0.0F;
                result.collided = true;
            }
        }
        if (result.velocity.z != 0.0F) {
            const float previousZ = result.position.z;
            result.position.z = originalZ + displacementZ * fraction;
            if (!canOccupy(result.position, collider) && !tryStepUp()) {
                result.position.z = previousZ;
                result.velocity.z = 0.0F;
                result.collided = true;
            }
        }
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

const std::vector<wkc::core::Vec2>& CollisionWorld::walkableOutline() const noexcept
{
    return walkableOutline_;
}

const std::vector<std::vector<wkc::core::Vec2>>& CollisionWorld::volumeOutlines() const noexcept
{
    return volumeOutlines_;
}

const std::vector<wkc::core::Aabb>& CollisionWorld::solids() const noexcept
{
    return solids_;
}

const std::vector<wkc::core::Aabb>& CollisionWorld::octagonalSolids() const noexcept
{
    return octagonalSolids_;
}

bool CollisionWorld::projectileSolidContains(
    const wkc::core::Vec3 point) const noexcept
{
    for (const wkc::core::Aabb& solid : solids_) {
        if (!solid.contains(point)) continue;
        if (std::find(octagonalSolids_.begin(), octagonalSolids_.end(), solid)
            == octagonalSolids_.end()) return true;
        const wkc::core::Vec3 center
            = (solid.minimum + solid.maximum) * 0.5F;
        const float halfWidth = (solid.maximum.x - solid.minimum.x) * 0.5F;
        const float x = std::abs(point.x - center.x);
        const float z = std::abs(point.z - center.z);
        if (x + z <= halfWidth * 1.41421356237F) return true;
    }
    return false;
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
    for (std::size_t index = 0U; index < walkableVolumes_.size(); ++index) {
        const auto volume = walkableVolumes_[index];
        if (fitsHorizontally(feetPosition, collider, volume)
            && feetPosition.y >= volume.minimum.y - 1.0e-4F
            && top <= volume.maximum.y + 1.0e-4F
            && (index >= volumeOutlines_.size() || volumeOutlines_[index].empty()
                || gdp::world::footprintContainsDisk(
                    volumeOutlines_[index], feetPosition, collider.radius))) {
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

        if (overlapsHorizontally(feetPosition, collider, solid)) {
            return true;
        }
    }
    return false;
}

} // namespace gdp::gameplay
