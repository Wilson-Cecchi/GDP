#include "gameplay/EnemySensors.hpp"

#include <algorithm>
#include <cmath>

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

} // namespace gdp::gameplay
