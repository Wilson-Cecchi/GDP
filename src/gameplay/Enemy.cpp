#include "gameplay/Enemy.hpp"

#include "gameplay/Combat.hpp"

#include <algorithm>

namespace gdp::gameplay {

EnemyUpdateSummary updateWorkers(
    gdp::entities::EntityManager& manager,
    Player& player,
    const std::uint32_t roomId,
    const CollisionWorld& collisionWorld,
    const float fixedStepSeconds,
    WorkerConfig config) noexcept
{
    using gdp::entities::EnemyArchetype;
    using gdp::entities::EnemyBehaviorState;
    using gdp::entities::EnemyData;
    using gdp::entities::Entity;
    using gdp::entities::EntityKind;

    EnemyUpdateSummary summary;
    const float step = std::max(fixedStepSeconds, 0.0F);
    config.detectionRange = std::max(config.detectionRange, 0.0F);
    config.attackRange = std::max(config.attackRange, 0.0F);
    config.moveSpeed = std::max(config.moveSpeed, 0.0F);
    config.attackDamage = std::max(config.attackDamage, 0.0F);
    config.attackInterval = std::max(config.attackInterval, 0.0F);

    for (const gdp::entities::EntityId id : manager.query(EntityKind::Enemy)) {
        Entity* entity = manager.find(id);
        EnemyData* enemy = manager.findData<EnemyData>(id);
        if (entity == nullptr || enemy == nullptr || entity->roomId != roomId
            || enemy->archetype != EnemyArchetype::Worker
            || enemy->behavior == EnemyBehaviorState::Dead) {
            continue;
        }

        enemy->attackCooldown = std::max(0.0F, enemy->attackCooldown - step);
        const gdp::core::Vec3 offset = player.state().position - entity->transform.position;
        const gdp::core::Vec3 planarOffset{offset.x, 0.0F, offset.z};
        const float distance = gdp::core::length(planarOffset);
        if (!player.state().alive || distance > config.detectionRange) {
            enemy->behavior = EnemyBehaviorState::Idle;
            ++summary.idle;
            continue;
        }

        const bool lineOfSight = hasLineOfSight(
            entity->transform.position + gdp::core::Vec3{0.0F, 1.0F, 0.0F},
            player.cameraPosition(),
            collisionWorld);
        if (distance <= config.attackRange && lineOfSight) {
            enemy->behavior = EnemyBehaviorState::Attack;
            ++summary.attacking;
            if (enemy->attackCooldown <= 0.0F) {
                player.applyDamage(config.attackDamage);
                enemy->attackCooldown = config.attackInterval;
                ++summary.attacksLanded;
            }
            continue;
        }

        enemy->behavior = EnemyBehaviorState::Chase;
        ++summary.chasing;
        if (distance <= 1.0e-7F || step <= 0.0F) {
            continue;
        }
        const gdp::core::Vec3 direction = planarOffset * (1.0F / distance);
        const CharacterMotionResult motion = collisionWorld.moveCharacter(
            entity->transform.position,
            direction * config.moveSpeed,
            step,
            {.radius = 0.35F, .height = 1.8F});
        entity->transform.position = motion.position;
    }
    return summary;
}

EnemyUpdateSummary updateGuards(
    gdp::entities::EntityManager& manager,
    Player& player,
    const std::uint32_t roomId,
    const CollisionWorld& collisionWorld,
    const float fixedStepSeconds,
    GuardConfig config) noexcept
{
    using gdp::entities::EnemyArchetype;
    using gdp::entities::EnemyBehaviorState;
    using gdp::entities::EnemyData;
    using gdp::entities::Entity;
    using gdp::entities::EntityKind;

    EnemyUpdateSummary summary;
    const float step = std::max(fixedStepSeconds, 0.0F);
    config.detectionRange = std::max(config.detectionRange, 0.0F);
    config.preferredMinimumRange = std::max(config.preferredMinimumRange, 0.0F);
    config.preferredMaximumRange = std::max(config.preferredMaximumRange, 0.0F);
    if (config.preferredMinimumRange > config.preferredMaximumRange) {
        std::swap(config.preferredMinimumRange, config.preferredMaximumRange);
    }
    config.moveSpeed = std::max(config.moveSpeed, 0.0F);
    config.attackDamage = std::max(config.attackDamage, 0.0F);
    config.attackInterval = std::max(config.attackInterval, 0.0F);

    for (const gdp::entities::EntityId id : manager.query(EntityKind::Enemy)) {
        Entity* entity = manager.find(id);
        EnemyData* enemy = manager.findData<EnemyData>(id);
        if (entity == nullptr || enemy == nullptr || entity->roomId != roomId
            || enemy->archetype != EnemyArchetype::Guard
            || enemy->behavior == EnemyBehaviorState::Dead) {
            continue;
        }

        enemy->attackCooldown = std::max(0.0F, enemy->attackCooldown - step);
        const gdp::core::Vec3 offset = player.state().position - entity->transform.position;
        const gdp::core::Vec3 planarOffset{offset.x, 0.0F, offset.z};
        const float distance = gdp::core::length(planarOffset);
        if (!player.state().alive || distance > config.detectionRange) {
            enemy->behavior = EnemyBehaviorState::Idle;
            ++summary.idle;
            continue;
        }

        const bool lineOfSight = hasLineOfSight(
            entity->transform.position + gdp::core::Vec3{0.0F, 1.35F, 0.0F},
            player.cameraPosition(),
            collisionWorld);
        const gdp::core::Vec3 towardPlayer = distance > 1.0e-7F
            ? planarOffset * (1.0F / distance)
            : gdp::core::Vec3{1.0F, 0.0F, 0.0F};
        if (lineOfSight && distance < config.preferredMinimumRange) {
            enemy->behavior = EnemyBehaviorState::Reposition;
            ++summary.repositioning;
            if (step > 0.0F) {
                const CharacterMotionResult motion = collisionWorld.moveCharacter(
                    entity->transform.position,
                    towardPlayer * -config.moveSpeed,
                    step,
                    {.radius = 0.35F, .height = 1.8F});
                entity->transform.position = motion.position;
            }
            continue;
        }

        if (!lineOfSight || distance > config.preferredMaximumRange) {
            enemy->behavior = EnemyBehaviorState::Chase;
            ++summary.chasing;
            if (step > 0.0F) {
                const CharacterMotionResult motion = collisionWorld.moveCharacter(
                    entity->transform.position,
                    towardPlayer * config.moveSpeed,
                    step,
                    {.radius = 0.35F, .height = 1.8F});
                entity->transform.position = motion.position;
            }
            continue;
        }

        enemy->behavior = EnemyBehaviorState::Attack;
        ++summary.attacking;
        if (enemy->attackCooldown <= 0.0F) {
            player.applyDamage(config.attackDamage);
            enemy->attackCooldown = config.attackInterval;
            ++summary.attacksLanded;
        }
    }
    return summary;
}

EnemyUpdateSummary updateOfficers(
    gdp::entities::EntityManager& manager,
    Player& player,
    const std::uint32_t roomId,
    const CollisionWorld& collisionWorld,
    const float fixedStepSeconds,
    OfficerConfig config) noexcept
{
    using gdp::entities::EnemyArchetype;
    using gdp::entities::EnemyBehaviorState;
    using gdp::entities::EnemyData;
    using gdp::entities::Entity;
    using gdp::entities::EntityKind;

    EnemyUpdateSummary summary;
    const float step = std::max(fixedStepSeconds, 0.0F);
    config.detectionRange = std::max(config.detectionRange, 0.0F);
    config.preferredMinimumRange = std::max(config.preferredMinimumRange, 0.0F);
    config.preferredMaximumRange = std::max(config.preferredMaximumRange, 0.0F);
    if (config.preferredMinimumRange > config.preferredMaximumRange) {
        std::swap(config.preferredMinimumRange, config.preferredMaximumRange);
    }
    config.moveSpeed = std::max(config.moveSpeed, 0.0F);
    config.attackDamage = std::max(config.attackDamage, 0.0F);
    config.attackInterval = std::max(config.attackInterval, 0.0F);
    config.supportRange = std::max(config.supportRange, 0.0F);
    config.supportInterval = std::max(config.supportInterval, 0.0F);
    config.allyCooldownReduction = std::max(config.allyCooldownReduction, 0.0F);

    for (const gdp::entities::EntityId id : manager.query(EntityKind::Enemy)) {
        Entity* entity = manager.find(id);
        EnemyData* enemy = manager.findData<EnemyData>(id);
        if (entity == nullptr || enemy == nullptr || entity->roomId != roomId
            || enemy->archetype != EnemyArchetype::Officer
            || enemy->behavior == EnemyBehaviorState::Dead) {
            continue;
        }

        enemy->attackCooldown = std::max(0.0F, enemy->attackCooldown - step);
        enemy->supportCooldown = std::max(0.0F, enemy->supportCooldown - step);
        const gdp::core::Vec3 offset = player.state().position - entity->transform.position;
        const gdp::core::Vec3 planarOffset{offset.x, 0.0F, offset.z};
        const float distance = gdp::core::length(planarOffset);
        if (!player.state().alive || distance > config.detectionRange) {
            enemy->behavior = EnemyBehaviorState::Idle;
            ++summary.idle;
            continue;
        }

        if (enemy->supportCooldown <= 0.0F && config.allyCooldownReduction > 0.0F) {
            std::size_t supported = 0;
            for (const gdp::entities::EntityId allyId : manager.query(EntityKind::Enemy)) {
                if (allyId == id) {
                    continue;
                }
                const Entity* allyEntity = manager.find(allyId);
                EnemyData* ally = manager.findData<EnemyData>(allyId);
                if (allyEntity == nullptr || ally == nullptr || allyEntity->roomId != roomId
                    || (ally->archetype != EnemyArchetype::Worker
                        && ally->archetype != EnemyArchetype::Guard)
                    || ally->behavior == EnemyBehaviorState::Dead
                    || ally->attackCooldown <= 0.0F) {
                    continue;
                }
                const gdp::core::Vec3 allyOffset
                    = allyEntity->transform.position - entity->transform.position;
                const float allyDistance = gdp::core::length(
                    gdp::core::Vec3{allyOffset.x, 0.0F, allyOffset.z});
                if (allyDistance > config.supportRange) {
                    continue;
                }
                ally->attackCooldown = std::max(
                    0.0F, ally->attackCooldown - config.allyCooldownReduction);
                ++supported;
            }
            if (supported > 0U) {
                enemy->supportCooldown = config.supportInterval;
                ++summary.commandsIssued;
                summary.alliesSupported += supported;
            }
        }

        const bool lineOfSight = hasLineOfSight(
            entity->transform.position + gdp::core::Vec3{0.0F, 1.4F, 0.0F},
            player.cameraPosition(),
            collisionWorld);
        const gdp::core::Vec3 towardPlayer = distance > 1.0e-7F
            ? planarOffset * (1.0F / distance)
            : gdp::core::Vec3{1.0F, 0.0F, 0.0F};
        if (lineOfSight && distance < config.preferredMinimumRange) {
            enemy->behavior = EnemyBehaviorState::Reposition;
            ++summary.repositioning;
            if (step > 0.0F) {
                const CharacterMotionResult motion = collisionWorld.moveCharacter(
                    entity->transform.position,
                    towardPlayer * -config.moveSpeed,
                    step,
                    {.radius = 0.35F, .height = 1.8F});
                entity->transform.position = motion.position;
            }
            continue;
        }

        if (!lineOfSight || distance > config.preferredMaximumRange) {
            enemy->behavior = EnemyBehaviorState::Chase;
            ++summary.chasing;
            if (step > 0.0F) {
                const CharacterMotionResult motion = collisionWorld.moveCharacter(
                    entity->transform.position,
                    towardPlayer * config.moveSpeed,
                    step,
                    {.radius = 0.35F, .height = 1.8F});
                entity->transform.position = motion.position;
            }
            continue;
        }

        enemy->behavior = EnemyBehaviorState::Attack;
        ++summary.attacking;
        if (enemy->attackCooldown <= 0.0F) {
            player.applyDamage(config.attackDamage);
            enemy->attackCooldown = config.attackInterval;
            ++summary.attacksLanded;
        }
    }
    return summary;
}

} // namespace gdp::gameplay
