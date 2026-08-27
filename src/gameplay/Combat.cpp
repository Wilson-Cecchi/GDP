#include "gameplay/Combat.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace gdp::gameplay {
namespace {

struct RayInterval {
    float nearDistance{0.0F};
    float farDistance{0.0F};
};

std::optional<RayInterval> rayAabbInterval(
    const gdp::core::Vec3 origin,
    const gdp::core::Vec3 direction,
    const gdp::core::Aabb bounds,
    const float maximumDistance) noexcept
{
    float nearDistance = 0.0F;
    float farDistance = std::numeric_limits<float>::max();
    const float origins[3]{origin.x, origin.y, origin.z};
    const float directions[3]{direction.x, direction.y, direction.z};
    const float minima[3]{bounds.minimum.x, bounds.minimum.y, bounds.minimum.z};
    const float maxima[3]{bounds.maximum.x, bounds.maximum.y, bounds.maximum.z};

    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(directions[axis]) <= 1.0e-7F) {
            if (origins[axis] < minima[axis] || origins[axis] > maxima[axis]) {
                return std::nullopt;
            }
            continue;
        }
        const float inverseDirection = 1.0F / directions[axis];
        float axisNear = (minima[axis] - origins[axis]) * inverseDirection;
        float axisFar = (maxima[axis] - origins[axis]) * inverseDirection;
        if (axisNear > axisFar) {
            std::swap(axisNear, axisFar);
        }
        nearDistance = std::max(nearDistance, axisNear);
        farDistance = std::min(farDistance, axisFar);
        if (nearDistance > farDistance) {
            return std::nullopt;
        }
    }
    if (farDistance < 0.0F || nearDistance > maximumDistance) {
        return std::nullopt;
    }
    return RayInterval{std::max(nearDistance, 0.0F), farDistance};
}

gdp::core::Aabb enemyBounds(const gdp::entities::Entity& enemy) noexcept
{
    const gdp::core::Vec3 position = enemy.transform.position;
    return {
        {position.x - 0.38F, position.y, position.z - 0.30F},
        {position.x + 0.38F, position.y + 1.80F, position.z + 0.30F},
    };
}

float firstWorldDistance(
    const gdp::core::Vec3 origin,
    const gdp::core::Vec3 direction,
    const float maximumDistance,
    const CollisionWorld& collisionWorld) noexcept
{
    float nearest = maximumDistance + 1.0F;
    for (const gdp::core::Aabb solid : collisionWorld.solids()) {
        if (const auto interval = rayAabbInterval(origin, direction, solid, maximumDistance)) {
            nearest = std::min(nearest, interval->nearDistance);
        }
    }
    if (const auto room = rayAabbInterval(
            origin, direction, collisionWorld.roomBounds(), maximumDistance)) {
        nearest = std::min(nearest, room->farDistance);
    }
    return nearest;
}

} // namespace

DamageOutcome applyDamage(
    gdp::entities::EntityManager& manager,
    const gdp::entities::EntityId target,
    const float amount) noexcept
{
    gdp::entities::EnemyData* enemy = manager.findData<gdp::entities::EnemyData>(target);
    if (enemy == nullptr || enemy->behavior == gdp::entities::EnemyBehaviorState::Dead
        || amount <= 0.0F) {
        return DamageOutcome::Ignored;
    }
    enemy->health = std::max(0.0F, enemy->health - amount);
    if (enemy->health > 0.0F) {
        return DamageOutcome::Damaged;
    }
    enemy->behavior = gdp::entities::EnemyBehaviorState::Dead;
    enemy->attackCooldown = 0.0F;
    enemy->supportCooldown = 0.0F;
    return DamageOutcome::Killed;
}

ShotResult fireHitscan(
    Weapon& weapon,
    const gdp::core::Vec3 origin,
    gdp::core::Vec3 direction,
    const std::uint32_t roomId,
    const CollisionWorld& collisionWorld,
    gdp::entities::EntityManager& manager) noexcept
{
    const FireReadiness readiness = weapon.tryFire();
    if (readiness == FireReadiness::Cooldown) {
        return {ShotOutcome::Cooldown, std::nullopt};
    }
    if (readiness == FireReadiness::Empty) {
        return {ShotOutcome::Empty, std::nullopt};
    }

    direction = gdp::core::normalized(direction);
    const float range = weapon.definition().range;
    if (gdp::core::length(direction) <= 1.0e-7F || range <= 0.0F) {
        return {ShotOutcome::Miss, std::nullopt};
    }

    const float worldDistance = firstWorldDistance(origin, direction, range, collisionWorld);
    std::optional<HitResult> nearestHit;
    for (const gdp::entities::EntityId id : manager.query(gdp::entities::EntityKind::Enemy)) {
        const gdp::entities::Entity* entity = manager.find(id);
        const gdp::entities::EnemyData* enemy = manager.findData<gdp::entities::EnemyData>(id);
        if (entity == nullptr || enemy == nullptr || entity->roomId != roomId
            || enemy->behavior == gdp::entities::EnemyBehaviorState::Dead) {
            continue;
        }
        const auto interval = rayAabbInterval(origin, direction, enemyBounds(*entity), range);
        if (!interval || interval->nearDistance >= worldDistance
            || (nearestHit && interval->nearDistance >= nearestHit->distance)) {
            continue;
        }
        nearestHit = HitResult{
            id,
            origin + (direction * interval->nearDistance),
            interval->nearDistance,
        };
    }

    if (nearestHit) {
        const DamageOutcome damage = applyDamage(manager, nearestHit->target, weapon.definition().damage);
        return {
            damage == DamageOutcome::Killed ? ShotOutcome::Killed : ShotOutcome::Hit,
            nearestHit,
        };
    }
    if (worldDistance <= range) {
        return {ShotOutcome::ImpactWorld, std::nullopt};
    }
    return {ShotOutcome::Miss, std::nullopt};
}

bool hasLineOfSight(
    const gdp::core::Vec3 origin,
    const gdp::core::Vec3 target,
    const CollisionWorld& collisionWorld) noexcept
{
    const gdp::core::Vec3 offset = target - origin;
    const float distance = gdp::core::length(offset);
    if (distance <= 1.0e-7F) {
        return true;
    }
    return firstWorldDistance(
        origin,
        offset * (1.0F / distance),
        distance,
        collisionWorld) >= distance;
}

} // namespace gdp::gameplay
