#include "gameplay/Enemy.hpp"

#include "gameplay/Combat.hpp"
#include "gameplay/Props.hpp"

#include <algorithm>
#include <limits>

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

EnemyUpdateSummary updateEngineers(
    gdp::entities::EntityManager& manager,
    Player& player,
    const std::uint32_t roomId,
    const CollisionWorld& collisionWorld,
    const float fixedStepSeconds,
    EngineerConfig config) noexcept
{
    using gdp::entities::EnemyArchetype;
    using gdp::entities::EnemyBehaviorState;
    using gdp::entities::EnemyData;
    using gdp::entities::Entity;
    using gdp::entities::EntityId;
    using gdp::entities::EntityKind;
    using gdp::entities::PropData;
    using gdp::entities::PropState;

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
    config.repairRange = std::max(config.repairRange, 0.0F);
    config.repairInterval = std::max(config.repairInterval, 0.0F);
    config.repairAmount = std::max(config.repairAmount, 0.0F);

    for (const EntityId id : manager.query(EntityKind::Enemy)) {
        Entity* entity = manager.find(id);
        EnemyData* enemy = manager.findData<EnemyData>(id);
        if (entity == nullptr || enemy == nullptr || entity->roomId != roomId
            || enemy->archetype != EnemyArchetype::Engineer
            || enemy->behavior == EnemyBehaviorState::Dead) {
            continue;
        }

        enemy->attackCooldown = std::max(0.0F, enemy->attackCooldown - step);
        enemy->supportCooldown = std::max(0.0F, enemy->supportCooldown - step);
        if (enemy->supportCooldown <= 0.0F && config.repairRange > 0.0F) {
            EntityId repairTarget;
            bool targetIsProp = false;
            float nearestDistance = std::numeric_limits<float>::max();
            const gdp::core::Vec3 repairOrigin
                = entity->transform.position + gdp::core::Vec3{0.0F, 1.0F, 0.0F};
            for (const EntityId allyId : manager.query(EntityKind::Enemy)) {
                if (allyId == id || config.repairAmount <= 0.0F) {
                    continue;
                }
                const Entity* allyEntity = manager.find(allyId);
                const EnemyData* ally = manager.findData<EnemyData>(allyId);
                if (allyEntity == nullptr || ally == nullptr || allyEntity->roomId != roomId
                    || ally->behavior == EnemyBehaviorState::Dead
                    || ally->health >= ally->maximumHealth) {
                    continue;
                }
                const gdp::core::Vec3 allyOffset
                    = allyEntity->transform.position - entity->transform.position;
                const float distance = gdp::core::length(
                    gdp::core::Vec3{allyOffset.x, 0.0F, allyOffset.z});
                if (distance > config.repairRange || distance >= nearestDistance
                    || !hasLineOfSight(
                        repairOrigin,
                        allyEntity->transform.position + gdp::core::Vec3{0.0F, 0.9F, 0.0F},
                        collisionWorld)) {
                    continue;
                }
                repairTarget = allyId;
                targetIsProp = false;
                nearestDistance = distance;
            }
            for (const EntityId propId : manager.query(EntityKind::Prop)) {
                const Entity* propEntity = manager.find(propId);
                const PropData* prop = manager.findData<PropData>(propId);
                if (propEntity == nullptr || prop == nullptr || propEntity->roomId != roomId
                    || prop->state != PropState::Damaged) {
                    continue;
                }
                const gdp::core::Vec3 propOffset
                    = propEntity->transform.position - entity->transform.position;
                const float distance = gdp::core::length(
                    gdp::core::Vec3{propOffset.x, 0.0F, propOffset.z});
                if (distance > config.repairRange || distance >= nearestDistance
                    || !hasLineOfSightToProp(
                        repairOrigin, manager, propId, collisionWorld)) {
                    continue;
                }
                repairTarget = propId;
                targetIsProp = true;
                nearestDistance = distance;
            }
            if (repairTarget.isValid()) {
                if (targetIsProp) {
                    if (repairProp(
                            manager, repairTarget,
                            manager.findData<PropData>(repairTarget)->maximumHealth)) {
                        ++summary.propsRestored;
                    }
                } else {
                    EnemyData* ally = manager.findData<EnemyData>(repairTarget);
                    ally->health = std::min(
                        ally->maximumHealth, ally->health + config.repairAmount);
                    ++summary.alliesRepaired;
                }
                enemy->supportCooldown = config.repairInterval;
                ++summary.repairsPerformed;
            }
        }

        const gdp::core::Vec3 offset = player.state().position - entity->transform.position;
        const gdp::core::Vec3 planarOffset{offset.x, 0.0F, offset.z};
        const float distance = gdp::core::length(planarOffset);
        if (!player.state().alive || distance > config.detectionRange) {
            enemy->behavior = EnemyBehaviorState::Idle;
            ++summary.idle;
            continue;
        }

        const bool lineOfSight = hasLineOfSight(
            entity->transform.position + gdp::core::Vec3{0.0F, 1.25F, 0.0F},
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

IndustrialRobotPhase industrialRobotPhase(
    const float health,
    const float maximumHealth,
    IndustrialRobotConfig config) noexcept
{
    config.phaseTwoHealthRatio = std::clamp(config.phaseTwoHealthRatio, 0.0F, 1.0F);
    config.phaseThreeHealthRatio = std::clamp(config.phaseThreeHealthRatio, 0.0F, 1.0F);
    if (config.phaseThreeHealthRatio > config.phaseTwoHealthRatio) {
        std::swap(config.phaseThreeHealthRatio, config.phaseTwoHealthRatio);
    }
    const float ratio = maximumHealth > 0.0F
        ? std::clamp(health / maximumHealth, 0.0F, 1.0F)
        : 0.0F;
    if (ratio > config.phaseTwoHealthRatio) {
        return IndustrialRobotPhase::LongRange;
    }
    if (ratio > config.phaseThreeHealthRatio) {
        return IndustrialRobotPhase::Pressure;
    }
    return IndustrialRobotPhase::Pursuit;
}

EnemyUpdateSummary updateIndustrialRobots(
    gdp::entities::EntityManager& manager,
    Player& player,
    const std::uint32_t roomId,
    const CollisionWorld& collisionWorld,
    const float fixedStepSeconds,
    IndustrialRobotConfig config) noexcept
{
    using gdp::entities::EnemyArchetype;
    using gdp::entities::EnemyBehaviorState;
    using gdp::entities::EnemyData;
    using gdp::entities::Entity;
    using gdp::entities::EntityKind;

    EnemyUpdateSummary summary;
    const float step = std::max(fixedStepSeconds, 0.0F);
    config.detectionRange = std::max(config.detectionRange, 0.0F);
    config.phaseTwoHealthRatio = std::clamp(config.phaseTwoHealthRatio, 0.0F, 1.0F);
    config.phaseThreeHealthRatio = std::clamp(config.phaseThreeHealthRatio, 0.0F, 1.0F);
    if (config.phaseThreeHealthRatio > config.phaseTwoHealthRatio) {
        std::swap(config.phaseThreeHealthRatio, config.phaseTwoHealthRatio);
    }
    config.longRangeMinimum = std::max(config.longRangeMinimum, 0.0F);
    config.longRangeMaximum = std::max(config.longRangeMaximum, 0.0F);
    if (config.longRangeMinimum > config.longRangeMaximum) {
        std::swap(config.longRangeMinimum, config.longRangeMaximum);
    }
    config.pressureMinimum = std::max(config.pressureMinimum, 0.0F);
    config.pressureMaximum = std::max(config.pressureMaximum, 0.0F);
    if (config.pressureMinimum > config.pressureMaximum) {
        std::swap(config.pressureMinimum, config.pressureMaximum);
    }
    config.pursuitAttackRange = std::max(config.pursuitAttackRange, 0.0F);
    config.longRangeMoveSpeed = std::max(config.longRangeMoveSpeed, 0.0F);
    config.pressureMoveSpeed = std::max(config.pressureMoveSpeed, 0.0F);
    config.pursuitMoveSpeed = std::max(config.pursuitMoveSpeed, 0.0F);
    config.longRangeDamage = std::max(config.longRangeDamage, 0.0F);
    config.pressureDamage = std::max(config.pressureDamage, 0.0F);
    config.pursuitDamage = std::max(config.pursuitDamage, 0.0F);
    config.longRangeAttackInterval = std::max(config.longRangeAttackInterval, 0.0F);
    config.pressureAttackInterval = std::max(config.pressureAttackInterval, 0.0F);
    config.pursuitAttackInterval = std::max(config.pursuitAttackInterval, 0.0F);

    for (const gdp::entities::EntityId id : manager.query(EntityKind::Enemy)) {
        Entity* entity = manager.find(id);
        EnemyData* enemy = manager.findData<EnemyData>(id);
        if (entity == nullptr || enemy == nullptr || entity->roomId != roomId
            || enemy->archetype != EnemyArchetype::IndustrialRobot
            || enemy->behavior == EnemyBehaviorState::Dead) {
            continue;
        }

        enemy->attackCooldown = std::max(0.0F, enemy->attackCooldown - step);
        const IndustrialRobotPhase phase
            = industrialRobotPhase(enemy->health, enemy->maximumHealth, config);
        float minimumRange = 0.0F;
        float maximumRange = config.pursuitAttackRange;
        float moveSpeed = config.pursuitMoveSpeed;
        float attackDamage = config.pursuitDamage;
        float attackInterval = config.pursuitAttackInterval;
        switch (phase) {
        case IndustrialRobotPhase::LongRange:
            minimumRange = config.longRangeMinimum;
            maximumRange = config.longRangeMaximum;
            moveSpeed = config.longRangeMoveSpeed;
            attackDamage = config.longRangeDamage;
            attackInterval = config.longRangeAttackInterval;
            ++summary.bossLongRange;
            break;
        case IndustrialRobotPhase::Pressure:
            minimumRange = config.pressureMinimum;
            maximumRange = config.pressureMaximum;
            moveSpeed = config.pressureMoveSpeed;
            attackDamage = config.pressureDamage;
            attackInterval = config.pressureAttackInterval;
            ++summary.bossPressure;
            break;
        case IndustrialRobotPhase::Pursuit:
            ++summary.bossPursuit;
            break;
        }

        const gdp::core::Vec3 offset = player.state().position - entity->transform.position;
        const gdp::core::Vec3 planarOffset{offset.x, 0.0F, offset.z};
        const float distance = gdp::core::length(planarOffset);
        if (!player.state().alive || distance > config.detectionRange) {
            enemy->behavior = EnemyBehaviorState::Idle;
            ++summary.idle;
            continue;
        }

        const bool lineOfSight = hasLineOfSight(
            entity->transform.position + gdp::core::Vec3{0.0F, 1.45F, 0.0F},
            player.cameraPosition(), collisionWorld);
        const gdp::core::Vec3 towardPlayer = distance > 1.0e-7F
            ? planarOffset * (1.0F / distance)
            : gdp::core::Vec3{1.0F, 0.0F, 0.0F};
        if (phase != IndustrialRobotPhase::Pursuit
            && lineOfSight && distance < minimumRange) {
            enemy->behavior = EnemyBehaviorState::Reposition;
            ++summary.repositioning;
            if (step > 0.0F) {
                const CharacterMotionResult motion = collisionWorld.moveCharacter(
                    entity->transform.position, towardPlayer * -moveSpeed, step,
                    {.radius = 0.52F, .height = 1.9F});
                entity->transform.position = motion.position;
            }
            continue;
        }
        if (!lineOfSight || distance > maximumRange) {
            enemy->behavior = EnemyBehaviorState::Chase;
            ++summary.chasing;
            if (step > 0.0F) {
                const CharacterMotionResult motion = collisionWorld.moveCharacter(
                    entity->transform.position, towardPlayer * moveSpeed, step,
                    {.radius = 0.52F, .height = 1.9F});
                entity->transform.position = motion.position;
            }
            continue;
        }

        enemy->behavior = EnemyBehaviorState::Attack;
        ++summary.attacking;
        if (enemy->attackCooldown <= 0.0F) {
            player.applyDamage(attackDamage);
            enemy->attackCooldown = attackInterval;
            ++summary.attacksLanded;
        }
    }
    return summary;
}

} // namespace gdp::gameplay
