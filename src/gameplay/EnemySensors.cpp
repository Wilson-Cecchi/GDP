#include "gameplay/EnemySensors.hpp"

#include "gameplay/EnemyFireGeometry.hpp"
#include "gameplay/EnemyProjectile.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace gdp::gameplay {
namespace {

float wallClearance(
    const CollisionWorld& world,
    const wkc::core::Vec3 position,
    const wkc::core::Vec3 direction) noexcept
{
    constexpr float maximumDistance = 2.0F;
    constexpr float sampleStep = 0.2F;
    float clearDistance = 0.0F;
    for (int sample = 1; sample <= 10; ++sample) {
        const float distance = static_cast<float>(sample) * sampleStep;
        if (!world.canOccupy(position + (direction * distance),
                {.radius = 0.35F, .height = 1.8F})) {
            break;
        }
        clearDistance = distance;
    }
    return (clearDistance / maximumDistance) * 2.0F - 1.0F;
}

} // namespace

EnemySensorValues sampleEnemySensors(
    const gdp::entities::Entity& entity,
    const gdp::entities::EnemyData& enemy,
    const Player& player,
    const CollisionWorld& collisionWorld,
    const bool playerVisible,
    const float detectionRange) noexcept
{
    constexpr float radiansPerDegree = 0.01745329251994329577F;
    const float yaw = std::isfinite(entity.transform.rotationDegrees.y)
        ? entity.transform.rotationDegrees.y * radiansPerDegree : 0.0F;
    const wkc::core::Vec3 forward{std::sin(yaw), 0.0F, std::cos(yaw)};
    const wkc::core::Vec3 left{-forward.z, 0.0F, forward.x};
    const wkc::core::Vec3 right{-left.x, 0.0F, -left.z};
    const float healthRatio = std::isfinite(enemy.health)
        && std::isfinite(enemy.maximumHealth) && enemy.maximumHealth > 0.0F
        ? std::clamp(enemy.health / enemy.maximumHealth, 0.0F, 1.0F) : 0.0F;

    float distanceSignal = -1.0F;
    float angleSignal = 0.0F;
    if (playerVisible && player.state().alive) {
        const wkc::core::Vec3 difference
            = player.state().position - entity.transform.position;
        const wkc::core::Vec3 planar{difference.x, 0.0F, difference.z};
        const float distance = wkc::core::length(planar);
        const float safeRange = std::isfinite(detectionRange)
            ? std::max(detectionRange, 0.001F) : 0.001F;
        if (std::isfinite(distance)) {
            distanceSignal = 1.0F
                - 2.0F * std::clamp(distance / safeRange, 0.0F, 1.0F);
        }
        if (std::isfinite(distance) && distance > 1.0e-6F) {
            angleSignal = std::clamp(
                wkc::core::dot(left, planar * (1.0F / distance)), -1.0F, 1.0F);
        }
    }

    return {
        distanceSignal,
        angleSignal,
        playerVisible && player.state().alive ? 1.0F : -1.0F,
        healthRatio * 2.0F - 1.0F,
        wallClearance(collisionWorld, entity.transform.position, forward),
        wallClearance(collisionWorld, entity.transform.position, left),
        wallClearance(collisionWorld, entity.transform.position, right),
    };
}

TacticalEnemySensorValues sampleTacticalEnemySensors(
    const gdp::entities::Entity& entity,
    const gdp::entities::EnemyData& enemy,
    const Player& player,
    const CollisionWorld& collisionWorld,
    const bool playerVisible,
    const float detectionRange) noexcept
{
    const EnemySensorValues legacy = sampleEnemySensors(
        entity, enemy, player, collisionWorld, playerVisible, detectionRange);
    TacticalEnemySensorValues values{};
    std::copy(legacy.begin(), legacy.end(), values.begin());
    if (playerVisible && player.state().alive) {
        constexpr float radiansPerDegree = 0.01745329251994329577F;
        const float yaw = std::isfinite(entity.transform.rotationDegrees.y)
            ? entity.transform.rotationDegrees.y * radiansPerDegree : 0.0F;
        const wkc::core::Vec3 forward{std::sin(yaw), 0.0F, std::cos(yaw)};
        const wkc::core::Vec3 right{forward.z, 0.0F, -forward.x};
        const wkc::core::Vec3 velocity = player.state().velocity;
        values[static_cast<std::size_t>(
            TacticalEnemySensor::PlayerVelocityForward)] = std::clamp(
                wkc::core::dot(velocity, forward) / 7.2F, -1.0F, 1.0F);
        values[static_cast<std::size_t>(
            TacticalEnemySensor::PlayerVelocityRight)] = std::clamp(
                wkc::core::dot(velocity, right) / 7.2F, -1.0F, 1.0F);
        const wkc::core::Vec3 towardEnemy = wkc::core::normalized(
            entity.transform.position + wkc::core::Vec3{0.0F, 1.0F, 0.0F}
                - player.cameraPosition());
        values[static_cast<std::size_t>(
            TacticalEnemySensor::PlayerAimThreat)] = std::clamp(
                wkc::core::dot(player.forwardDirection(), towardEnemy),
                -1.0F, 1.0F);
    }
    values[static_cast<std::size_t>(TacticalEnemySensor::Ammo)] = std::clamp(
        static_cast<float>(enemy.magazineAmmo) / 8.0F * 2.0F - 1.0F,
        -1.0F, 1.0F);
    values[static_cast<std::size_t>(TacticalEnemySensor::Reloading)]
        = enemy.reloadRemaining > 0.0F ? 1.0F : -1.0F;
    return values;
}

CooperativeEnemySensorValues sampleCooperativeEnemySensors(
    const gdp::entities::EntityManager& manager,
    const gdp::entities::Entity& entity,
    const gdp::entities::EnemyData& enemy,
    const Player& player,
    const CollisionWorld& collisionWorld,
    const bool playerVisible,
    const float detectionRange) noexcept
{
    const TacticalEnemySensorValues tactical = sampleTacticalEnemySensors(
        entity, enemy, player, collisionWorld, playerVisible, detectionRange);
    CooperativeEnemySensorValues values{};
    std::copy(tactical.begin(), tactical.end(), values.begin());
    constexpr float radiansPerDegree = 0.01745329251994329577F;
    const float yaw = std::isfinite(entity.transform.rotationDegrees.y)
        ? entity.transform.rotationDegrees.y * radiansPerDegree : 0.0F;
    const wkc::core::Vec3 forward{std::sin(yaw), 0.0F, std::cos(yaw)};
    const wkc::core::Vec3 left{-forward.z, 0.0F, forward.x};
    const float safeRange = std::isfinite(detectionRange)
        ? std::max(detectionRange, 0.001F) : 0.001F;
    values[static_cast<std::size_t>(CooperativeEnemySensor::AllyDistance)] = -1.0F;
    values[static_cast<std::size_t>(CooperativeEnemySensor::AllyHealth)] = -1.0F;
    values[static_cast<std::size_t>(CooperativeEnemySensor::FireLaneBlocked)] = -1.0F;
    values[static_cast<std::size_t>(CooperativeEnemySensor::KnownTargetDistance)] = -1.0F;
    values[static_cast<std::size_t>(CooperativeEnemySensor::KnownTargetFreshness)] = -1.0F;

    const gdp::entities::Entity* nearestAlly = nullptr;
    const gdp::entities::EnemyData* nearestData = nullptr;
    float nearestDistance = std::numeric_limits<float>::infinity();
    for (const gdp::entities::EntityId id
        : manager.query(gdp::entities::EntityKind::Enemy)) {
        if (id == entity.id) {
            continue;
        }
        const auto* candidate = manager.find(id);
        const auto* data = manager.findData<gdp::entities::EnemyData>(id);
        if (candidate == nullptr || data == nullptr || !data->active
            || data->behavior == gdp::entities::EnemyBehaviorState::Dead
            || candidate->roomId != entity.roomId) {
            continue;
        }
        const wkc::core::Vec3 delta
            = candidate->transform.position - entity.transform.position;
        const float distance = std::hypot(delta.x, delta.z);
        if (distance < nearestDistance && distance <= 12.0F) {
            nearestDistance = distance;
            nearestAlly = candidate;
            nearestData = data;
        }
    }
    if (nearestAlly != nullptr) {
        const wkc::core::Vec3 delta
            = nearestAlly->transform.position - entity.transform.position;
        values[static_cast<std::size_t>(CooperativeEnemySensor::AllyDistance)]
            = 1.0F - 2.0F * std::clamp(nearestDistance / 12.0F, 0.0F, 1.0F);
        if (nearestDistance > 1.0e-6F) {
            values[static_cast<std::size_t>(CooperativeEnemySensor::AllyAngle)]
                = std::clamp(wkc::core::dot(left,
                    wkc::core::Vec3{delta.x, 0.0F, delta.z}
                        * (1.0F / nearestDistance)), -1.0F, 1.0F);
        }
        values[static_cast<std::size_t>(CooperativeEnemySensor::AllyHealth)]
            = nearestData->maximumHealth > 0.0F
            ? 2.0F * std::clamp(nearestData->health
                / nearestData->maximumHealth, 0.0F, 1.0F) - 1.0F : -1.0F;
    }
    if (playerVisible && player.state().alive) {
        const EnemyShotRay ray = enemyShotRay(entity, enemy, player);
        values[static_cast<std::size_t>(CooperativeEnemySensor::FireLaneBlocked)]
            = allyInEnemyFireLane(manager, entity.id, ray.muzzle,
                ray.direction, ray.targetDistance) ? 1.0F : -1.0F;
    } else if (enemy.investigationTime > 0.0F) {
        const wkc::core::Vec3 delta = enemy.lastKnownPlayerPosition
            - entity.transform.position;
        const wkc::core::Vec3 planar{delta.x, 0.0F, delta.z};
        const float distance = wkc::core::length(planar);
        values[static_cast<std::size_t>(CooperativeEnemySensor::KnownTargetDistance)]
            = 1.0F - 2.0F * std::clamp(distance / safeRange, 0.0F, 1.0F);
        if (distance > 1.0e-6F) {
            values[static_cast<std::size_t>(CooperativeEnemySensor::KnownTargetAngle)]
                = std::clamp(wkc::core::dot(left, planar * (1.0F / distance)),
                    -1.0F, 1.0F);
        }
        values[static_cast<std::size_t>(CooperativeEnemySensor::KnownTargetFreshness)]
            = 2.0F * std::clamp(enemy.investigationTime / 6.0F,
                0.0F, 1.0F) - 1.0F;
    }
    return values;
}

} // namespace gdp::gameplay
