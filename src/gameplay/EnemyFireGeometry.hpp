#pragma once

#include "entities/EntityManager.hpp"
#include "gameplay/Player.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace gdp::gameplay {

struct EnemyShotRay {
    wkc::core::Vec3 muzzle{};
    wkc::core::Vec3 direction{};
    float targetDistance{};
};

[[nodiscard]] inline bool referenceEnemyModelsEnabled() noexcept
{
    const char* const value = std::getenv("GDP_ENEMY_REFERENCE_MODELS");
    return value == nullptr || value[0] != '0' || value[1] != '\0';
}

[[nodiscard]] inline wkc::core::Vec3 referenceMuzzleOffset(
    const gdp::entities::EnemyArchetype archetype) noexcept
{
    using gdp::entities::EnemyArchetype;
    switch (archetype) {
    case EnemyArchetype::Guard: return {-0.176F, 1.403F, 1.140F};
    case EnemyArchetype::Officer: return {-0.431F, 1.267F, 0.889F};
    case EnemyArchetype::Engineer: return {-0.404F, 1.329F, 1.063F};
    case EnemyArchetype::IndustrialRobot: return {-0.679F, 0.998F, 0.699F};
    case EnemyArchetype::Worker: break;
    }
    return {0.0F, 1.2F, 0.45F};
}

[[nodiscard]] inline EnemyShotRay enemyShotRay(
    const gdp::entities::Entity& entity,
    const gdp::entities::EnemyData& enemy,
    const Player& player) noexcept
{
    constexpr float radiansPerDegree = 0.01745329251994329577F;
    const float yaw = entity.transform.rotationDegrees.y * radiansPerDegree;
    const bool referenceModels = referenceEnemyModelsEnabled();
    const wkc::core::Vec3 socket = referenceModels
        ? referenceMuzzleOffset(enemy.archetype)
        : wkc::core::Vec3{0.0F, 1.2F, 0.45F};
    const wkc::core::Vec3 muzzle = entity.transform.position
        + wkc::core::Vec3{
            std::cos(yaw) * socket.x + std::sin(yaw) * socket.z,
            socket.y,
            -std::sin(yaw) * socket.x + std::cos(yaw) * socket.z};
    const wkc::core::Vec3 target = player.cameraPosition()
        - wkc::core::Vec3{0.0F, 0.22F, 0.0F};
    const float horizontalDistance = std::max(0.1F, std::hypot(
        target.x - muzzle.x, target.z - muzzle.z));
    const float centerTargetYaw = std::atan2(
        target.x - entity.transform.position.x,
        target.z - entity.transform.position.z);
    const float muzzleTargetYaw = std::atan2(
        target.x - muzzle.x, target.z - muzzle.z);
    const float shotYaw = referenceModels
        ? muzzleTargetYaw + std::remainder(
            yaw - centerTargetYaw, 6.28318530717958647692F)
        : yaw;
    return {muzzle, wkc::core::normalized({
        std::sin(shotYaw), (target.y - muzzle.y) / horizontalDistance,
        std::cos(shotYaw)}), wkc::core::length(target - muzzle)};
}

} // namespace gdp::gameplay
