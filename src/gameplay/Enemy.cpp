#include "gameplay/Enemy.hpp"

#include "core/Hash.hpp"
#include "gameplay/Combat.hpp"
#include "gameplay/Props.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
#include <vector>

namespace gdp::gameplay {
namespace {

float planarDistanceSquared(
    const wkc::core::Vec3 first,
    const wkc::core::Vec3 second) noexcept
{
    const float x = first.x - second.x;
    const float z = first.z - second.z;
    return (x * x) + (z * z);
}

wkc::core::Vec3 avoidanceDirection(
    const wkc::core::Vec3 desired,
    const std::int8_t side) noexcept
{
    const float sign = side < 0 ? -1.0F : 1.0F;
    const wkc::core::Vec3 lateral{-desired.z * sign, 0.0F, desired.x * sign};
    return wkc::core::normalized((desired * 0.35F) + lateral);
}

float enemyCollisionRadius(
    const gdp::entities::EnemyArchetype archetype) noexcept
{
    return archetype == gdp::entities::EnemyArchetype::IndustrialRobot
        ? 0.52F : 0.35F;
}

wkc::core::Vec3 separationRepulsion(
    const gdp::entities::EntityManager& manager,
    const std::span<const gdp::entities::EntityId> enemyIds,
    const gdp::entities::EntityId id,
    const std::uint32_t roomId,
    const wkc::core::Vec3 position,
    const wkc::core::Vec3 desiredDirection,
    const float ownRadius) noexcept
{
    wkc::core::Vec3 repulsion{};
    for (const gdp::entities::EntityId otherId : enemyIds) {
        if (otherId == id) {
            continue;
        }
        const gdp::entities::Entity* other = manager.find(otherId);
        const gdp::entities::EnemyData* otherEnemy
            = manager.findData<gdp::entities::EnemyData>(otherId);
        if (other == nullptr || otherEnemy == nullptr || other->roomId != roomId
            || !otherEnemy->active
            || otherEnemy->behavior == gdp::entities::EnemyBehaviorState::Dead) {
            continue;
        }
        const float preferredDistance = ownRadius
            + enemyCollisionRadius(otherEnemy->archetype) + 0.20F;
        const wkc::core::Vec3 offset = position - other->transform.position;
        const wkc::core::Vec3 planarOffset{offset.x, 0.0F, offset.z};
        const float distance = wkc::core::length(planarOffset);
        if (distance >= preferredDistance) {
            continue;
        }
        wkc::core::Vec3 away;
        if (distance > 1.0e-5F) {
            away = planarOffset * (1.0F / distance);
        } else {
            const float side = id.packed() < otherId.packed() ? -1.0F : 1.0F;
            away = {-desiredDirection.z * side, 0.0F,
                desiredDirection.x * side};
        }
        const float strength
            = std::clamp((preferredDistance - distance) / preferredDistance,
                0.0F, 1.0F);
        repulsion = repulsion + (away * strength);
    }
    return repulsion;
}

wkc::core::Vec3 separationDirection(
    const gdp::entities::EntityManager& manager,
    const std::span<const gdp::entities::EntityId> enemyIds,
    const gdp::entities::EntityId id,
    const std::uint32_t roomId,
    const wkc::core::Vec3 position,
    const wkc::core::Vec3 desiredDirection,
    const float ownRadius) noexcept
{
    const wkc::core::Vec3 repulsion = separationRepulsion(
        manager, enemyIds, id, roomId, position, desiredDirection, ownRadius);
    if (wkc::core::length(repulsion) <= 1.0e-5F) {
        return desiredDirection;
    }
    return wkc::core::normalized(desiredDirection + (repulsion * 1.4F));
}

bool movementRespectsEnemyVolumes(
    const gdp::entities::EntityManager& manager,
    const std::span<const gdp::entities::EntityId> enemyIds,
    const gdp::entities::EntityId id,
    const std::uint32_t roomId,
    const wkc::core::Vec3 position,
    const wkc::core::Vec3 candidate,
    const float ownRadius) noexcept
{
    const wkc::core::Vec3 movement = candidate - position;
    const float movementLengthSquared
        = (movement.x * movement.x) + (movement.z * movement.z);
    for (const gdp::entities::EntityId otherId : enemyIds) {
        if (otherId == id) {
            continue;
        }
        const gdp::entities::Entity* other = manager.find(otherId);
        const gdp::entities::EnemyData* otherEnemy
            = manager.findData<gdp::entities::EnemyData>(otherId);
        if (other == nullptr || otherEnemy == nullptr || other->roomId != roomId
            || !otherEnemy->active
            || otherEnemy->behavior == gdp::entities::EnemyBehaviorState::Dead) {
            continue;
        }
        const float contactDistance = std::max(
            0.0F, ownRadius + enemyCollisionRadius(otherEnemy->archetype) - 0.01F);
        const float contactDistanceSquared = contactDistance * contactDistance;
        const float currentDistanceSquared
            = planarDistanceSquared(position, other->transform.position);
        const float candidateDistanceSquared
            = planarDistanceSquared(candidate, other->transform.position);
        if (currentDistanceSquared < contactDistanceSquared) {
            if (candidateDistanceSquared <= currentDistanceSquared + 1.0e-8F) {
                return false;
            }
            continue;
        }
        if (movementLengthSquared <= 1.0e-10F) {
            continue;
        }
        const wkc::core::Vec3 towardOther
            = other->transform.position - position;
        const float along = std::clamp(
            ((towardOther.x * movement.x) + (towardOther.z * movement.z))
                / movementLengthSquared,
            0.0F, 1.0F);
        const wkc::core::Vec3 closest{
            position.x + (movement.x * along),
            position.y,
            position.z + (movement.z * along),
        };
        if (planarDistanceSquared(closest, other->transform.position)
            < contactDistanceSquared) {
            return false;
        }
    }
    return true;
}

wkc::core::Vec3 moveEnemy(
    const gdp::entities::EntityManager& manager,
    const std::span<const gdp::entities::EntityId> enemyIds,
    const CollisionWorld& collisionWorld,
    const gdp::entities::EntityId id,
    const std::uint32_t roomId,
    gdp::entities::EnemyData& enemy,
    const wkc::core::Vec3 position,
    const wkc::core::Vec3 desiredDirection,
    const float speed,
    const float step,
    const CharacterCollider collider) noexcept
{
    if (step <= 0.0F || speed <= 0.0F) {
        return position;
    }
    const wkc::core::Vec3 steeringDirection = separationDirection(
        manager, enemyIds, id, roomId, position, desiredDirection, collider.radius);
    enemy.avoidanceTime = std::max(0.0F, enemy.avoidanceTime - step);
    const auto attempt = [&](const wkc::core::Vec3 direction) {
        CharacterMotionResult motion = collisionWorld.moveCharacter(
            position, direction * speed, step, collider);
        if (!movementRespectsEnemyVolumes(
                manager, enemyIds, id, roomId, position, motion.position,
                collider.radius)) {
            motion.position = position;
            motion.velocity = {};
            motion.collided = true;
        }
        return motion;
    };
    if (enemy.avoidanceTime > 0.0F && enemy.avoidanceSide != 0) {
        CharacterMotionResult motion = attempt(
            avoidanceDirection(steeringDirection, enemy.avoidanceSide));
        const float requested = speed * step;
        if (motion.collided
            && planarDistanceSquared(position, motion.position)
                < (requested * requested * 0.04F)) {
            enemy.avoidanceSide = static_cast<std::int8_t>(-enemy.avoidanceSide);
            motion = attempt(avoidanceDirection(
                steeringDirection, enemy.avoidanceSide));
        }
        return motion.position;
    }

    const CharacterMotionResult direct = attempt(steeringDirection);
    if (!direct.collided) {
        enemy.avoidanceSide = 0;
        return direct.position;
    }
    const wkc::core::Vec3 displacement = direct.position - position;
    const float forwardProgress = wkc::core::dot(displacement, steeringDirection);
    if (forwardProgress >= speed * step * 0.45F) {
        return direct.position;
    }

    enemy.avoidanceSide = ((enemy.visualSeed ^ id.packed()) & 1U) == 0U ? -1 : 1;
    enemy.avoidanceTime = 1.6F;
    CharacterMotionResult detour = attempt(
        avoidanceDirection(steeringDirection, enemy.avoidanceSide));
    if (planarDistanceSquared(position, detour.position) <= 1.0e-8F) {
        enemy.avoidanceSide = static_cast<std::int8_t>(-enemy.avoidanceSide);
        detour = attempt(avoidanceDirection(
            steeringDirection, enemy.avoidanceSide));
    }
    return detour.position;
}

constexpr float travelTurnRateDegreesPerSecond = 480.0F;
constexpr float targetTurnRateDegreesPerSecond = 720.0F;

void turnEnemyToward(
    gdp::entities::Entity& entity,
    const wkc::core::Vec3 direction,
    const float step,
    const float turnRateDegreesPerSecond) noexcept
{
    const float planarLengthSquared
        = (direction.x * direction.x) + (direction.z * direction.z);
    if (!std::isfinite(planarLengthSquared) || planarLengthSquared <= 1.0e-10F
        || !std::isfinite(step) || step <= 0.0F
        || !std::isfinite(turnRateDegreesPerSecond)
        || turnRateDegreesPerSecond <= 0.0F) {
        return;
    }
    constexpr float radiansToDegrees = 57.29577951308232F;
    const float target = std::remainder(
        std::atan2(direction.x, direction.z) * radiansToDegrees, 360.0F);
    float current = entity.transform.rotationDegrees.y;
    if (!std::isfinite(current)) {
        current = target;
    }
    const float delta = std::remainder(target - current, 360.0F);
    const float maximumDelta = turnRateDegreesPerSecond * step;
    const float appliedDelta = std::clamp(delta, -maximumDelta, maximumDelta);
    entity.transform.rotationDegrees.y = std::remainder(
        current + appliedDelta, 360.0F);
}

void moveEnemyAndFaceTravel(
    const gdp::entities::EntityManager& manager,
    const std::span<const gdp::entities::EntityId> enemyIds,
    const CollisionWorld& collisionWorld,
    const gdp::entities::EntityId id,
    const std::uint32_t roomId,
    gdp::entities::EnemyData& enemy,
    gdp::entities::Entity& entity,
    const wkc::core::Vec3 desiredDirection,
    const float speed,
    const float step,
    const CharacterCollider collider) noexcept
{
    const wkc::core::Vec3 previous = entity.transform.position;
    entity.transform.position = moveEnemy(
        manager, enemyIds, collisionWorld, id, roomId, enemy,
        previous, desiredDirection, speed, step, collider);
    turnEnemyToward(
        entity, entity.transform.position - previous, step,
        travelTurnRateDegreesPerSecond);
}

void separateEnemyIfNeeded(
    const gdp::entities::EntityManager& manager,
    const std::span<const gdp::entities::EntityId> enemyIds,
    const CollisionWorld& collisionWorld,
    const gdp::entities::EntityId id,
    const std::uint32_t roomId,
    gdp::entities::EnemyData& enemy,
    gdp::entities::Entity& entity,
    const float moveSpeed,
    const float step,
    const CharacterCollider collider) noexcept
{
    constexpr wkc::core::Vec3 stableReference{0.0F, 0.0F, -1.0F};
    const wkc::core::Vec3 repulsion = separationRepulsion(
        manager, enemyIds, id, roomId, entity.transform.position,
        stableReference, collider.radius);
    if (wkc::core::length(repulsion) <= 1.0e-5F) {
        return;
    }
    entity.transform.position = moveEnemy(
        manager, enemyIds, collisionWorld, id, roomId, enemy,
        entity.transform.position, wkc::core::normalized(repulsion),
        moveSpeed * 0.55F, step, collider);
}

float patrolUnit(const std::uint64_t value) noexcept
{
    return static_cast<float>(wkc::core::mix64(value) & 0xffffU) / 65535.0F;
}

wkc::core::Vec3 patrolWaypointPosition(
    const gdp::entities::EntityId id,
    const std::uint32_t roomId,
    const gdp::entities::EnemyData& enemy,
    const PatrolConfig& config) noexcept
{
    constexpr float tau = 6.28318530717958647692F;
    constexpr float goldenAngle = 2.39996322972865332223F;
    const std::uint64_t identity = wkc::core::hashCombine(
        id.packed(), static_cast<std::uint64_t>(roomId));
    const float baseAngle = patrolUnit(identity ^ 0x706174726f6cULL) * tau;
    const std::uint64_t waypointHash = wkc::core::hashCombine(
        identity, static_cast<std::uint64_t>(enemy.patrolWaypoint));
    const float radiusScale = 0.65F + (patrolUnit(waypointHash) * 0.35F);
    const float angle = baseAngle
        + (static_cast<float>(enemy.patrolWaypoint) * goldenAngle);
    return enemy.patrolOrigin + wkc::core::Vec3{
        std::cos(angle) * config.radius * radiusScale,
        0.0F,
        std::sin(angle) * config.radius * radiusScale,
    };
}

bool updateEnemyPatrol(
    const gdp::entities::EntityManager& manager,
    const std::span<const gdp::entities::EntityId> enemyIds,
    const CollisionWorld& collisionWorld,
    const gdp::entities::EntityId id,
    const std::uint32_t roomId,
    gdp::entities::EnemyData& enemy,
    gdp::entities::Entity& entity,
    const float moveSpeed,
    const float step,
    PatrolConfig config,
    const CharacterCollider collider) noexcept
{
    using gdp::entities::EnemyBehaviorState;
    config.radius = std::max(config.radius, 0.0F);
    config.speedScale = std::max(config.speedScale, 0.0F);
    config.waypointTimeout = std::max(config.waypointTimeout, 0.1F);
    config.pauseDuration = std::max(config.pauseDuration, 0.0F);
    config.arrivalDistance = std::max(config.arrivalDistance, 0.05F);

    if (enemy.patrolPauseTime > 0.0F) {
        enemy.patrolPauseTime = std::max(
            0.0F, enemy.patrolPauseTime - step);
        enemy.behavior = EnemyBehaviorState::Idle;
        separateEnemyIfNeeded(
            manager, enemyIds, collisionWorld, id, roomId, enemy, entity,
            moveSpeed, step, collider);
        return false;
    }

    if (enemy.patrolWaypointTime >= config.waypointTimeout) {
        ++enemy.patrolWaypoint;
        enemy.patrolWaypointTime = 0.0F;
        enemy.patrolPauseTime = config.pauseDuration * 0.35F;
        enemy.behavior = EnemyBehaviorState::Idle;
        return false;
    }

    const wkc::core::Vec3 target = patrolWaypointPosition(
        id, roomId, enemy, config);
    const wkc::core::Vec3 offset = target - entity.transform.position;
    const wkc::core::Vec3 planarOffset{offset.x, 0.0F, offset.z};
    const float distance = wkc::core::length(planarOffset);
    if (distance <= config.arrivalDistance) {
        const std::uint64_t pauseIdentity = wkc::core::hashCombine(
            id.packed(), static_cast<std::uint64_t>(enemy.patrolWaypoint));
        const float pauseScale = 0.75F + (patrolUnit(pauseIdentity) * 0.50F);
        ++enemy.patrolWaypoint;
        enemy.patrolWaypointTime = 0.0F;
        enemy.patrolPauseTime = config.pauseDuration * pauseScale;
        enemy.behavior = EnemyBehaviorState::Idle;
        return false;
    }

    enemy.patrolWaypointTime += step;
    if (distance <= 1.0e-7F || step <= 0.0F) {
        enemy.behavior = EnemyBehaviorState::Idle;
        return false;
    }
    const wkc::core::Vec3 previous = entity.transform.position;
    moveEnemyAndFaceTravel(
        manager, enemyIds, collisionWorld, id, roomId, enemy, entity,
        planarOffset * (1.0F / distance), moveSpeed * config.speedScale,
        step, collider);
    const bool moved = planarDistanceSquared(previous, entity.transform.position)
        > 1.0e-8F;
    enemy.behavior = moved
        ? EnemyBehaviorState::Patrol : EnemyBehaviorState::Idle;
    return moved;
}

bool playerVisibleToEnemy(
    const gdp::entities::Entity& enemy,
    const Player& player,
    const float distance,
    const float detectionRange,
    const float eyeHeight,
    const CollisionWorld& collisionWorld) noexcept
{
    return player.state().alive && distance <= detectionRange
        && hasLineOfSight(
            enemy.transform.position + wkc::core::Vec3{0.0F, eyeHeight, 0.0F},
            player.cameraPosition(), collisionWorld);
}

void rememberVisiblePlayer(
    gdp::entities::EnemyData& enemy,
    const Player& player,
    InvestigationConfig config) noexcept
{
    config.duration = std::max(config.duration, 0.0F);
    enemy.lastKnownPlayerPosition = player.state().position;
    enemy.investigationTime = config.duration;
    enemy.recoverTime = 0.0F;
}

bool updateEnemyAlert(
    const gdp::entities::EntityManager& manager,
    const std::span<const gdp::entities::EntityId> enemyIds,
    const CollisionWorld& collisionWorld,
    const gdp::entities::EntityId id,
    const std::uint32_t roomId,
    gdp::entities::EnemyData& enemy,
    gdp::entities::Entity& entity,
    const bool playerAlive,
    const float moveSpeed,
    const float step,
    const CharacterCollider collider) noexcept
{
    using gdp::entities::EnemyBehaviorState;
    if (!playerAlive) {
        enemy.alertTime = 0.0F;
        return false;
    }
    enemy.alertTime = std::max(0.0F, enemy.alertTime - step);
    if (enemy.alertTime <= 0.0F) {
        return false;
    }

    enemy.behavior = EnemyBehaviorState::Alert;
    separateEnemyIfNeeded(
        manager, enemyIds, collisionWorld, id, roomId, enemy, entity,
        moveSpeed, step, collider);
    turnEnemyToward(
        entity, enemy.lastKnownPlayerPosition - entity.transform.position,
        step, targetTurnRateDegreesPerSecond);
    return true;
}

bool updateEnemyRecover(
    const gdp::entities::EntityManager& manager,
    const std::span<const gdp::entities::EntityId> enemyIds,
    const CollisionWorld& collisionWorld,
    const gdp::entities::EntityId id,
    const std::uint32_t roomId,
    gdp::entities::EnemyData& enemy,
    gdp::entities::Entity& entity,
    const float moveSpeed,
    const float step,
    InvestigationConfig config,
    const CharacterCollider collider) noexcept
{
    using gdp::entities::EnemyBehaviorState;
    config.recoverDuration = std::isfinite(config.recoverDuration)
        ? std::max(config.recoverDuration, 0.0F) : 0.0F;
    enemy.recoverTime = std::max(0.0F, enemy.recoverTime - step);
    if (enemy.recoverTime <= 0.0F || config.recoverDuration <= 0.0F) {
        enemy.recoverTime = 0.0F;
        enemy.lastKnownPlayerPosition = {};
        return false;
    }

    enemy.behavior = EnemyBehaviorState::Recover;
    separateEnemyIfNeeded(
        manager, enemyIds, collisionWorld, id, roomId, enemy, entity,
        moveSpeed, step, collider);
    if (step > 0.0F) {
        const float direction
            = ((enemy.visualSeed ^ id.packed()) & 1U) == 0U ? -1.0F : 1.0F;
        const float scanDirection = enemy.recoverTime
                > config.recoverDuration * 0.5F
            ? direction : -direction;
        const float currentYaw = std::isfinite(entity.transform.rotationDegrees.y)
            ? entity.transform.rotationDegrees.y : 0.0F;
        entity.transform.rotationDegrees.y = std::remainder(
            currentYaw + (scanDirection * 70.0F * step), 360.0F);
    }
    return true;
}

bool updateEnemyInvestigation(
    const gdp::entities::EntityManager& manager,
    const std::span<const gdp::entities::EntityId> enemyIds,
    const CollisionWorld& collisionWorld,
    const gdp::entities::EntityId id,
    const std::uint32_t roomId,
    gdp::entities::EnemyData& enemy,
    gdp::entities::Entity& entity,
    const float moveSpeed,
    const float step,
    InvestigationConfig config,
    const CharacterCollider collider) noexcept
{
    using gdp::entities::EnemyBehaviorState;
    config.speedScale = std::max(config.speedScale, 0.0F);
    config.arrivalDistance = std::max(config.arrivalDistance, 0.05F);
    enemy.investigationTime = std::max(
        0.0F, enemy.investigationTime - step);
    if (enemy.investigationTime <= 0.0F) {
        return false;
    }

    const wkc::core::Vec3 offset
        = enemy.lastKnownPlayerPosition - entity.transform.position;
    const wkc::core::Vec3 planarOffset{offset.x, 0.0F, offset.z};
    const float distance = wkc::core::length(planarOffset);
    if (distance <= config.arrivalDistance) {
        enemy.behavior = EnemyBehaviorState::Idle;
        return true;
    }
    if (distance <= 1.0e-7F || step <= 0.0F) {
        enemy.behavior = EnemyBehaviorState::Idle;
        return true;
    }

    const wkc::core::Vec3 previous = entity.transform.position;
    moveEnemyAndFaceTravel(
        manager, enemyIds, collisionWorld, id, roomId, enemy, entity,
        planarOffset * (1.0F / distance), moveSpeed * config.speedScale,
        step, collider);
    enemy.behavior = planarDistanceSquared(previous, entity.transform.position)
            > 1.0e-8F
        ? EnemyBehaviorState::Investigate : EnemyBehaviorState::Idle;
    return true;
}

void updateUnseenEnemy(
    const gdp::entities::EntityManager& manager,
    const std::span<const gdp::entities::EntityId> enemyIds,
    const CollisionWorld& collisionWorld,
    const gdp::entities::EntityId id,
    const std::uint32_t roomId,
    gdp::entities::EnemyData& enemy,
    gdp::entities::Entity& entity,
    const bool playerAlive,
    const float moveSpeed,
    const float step,
    const PatrolConfig patrol,
    const InvestigationConfig investigation,
    const CharacterCollider collider,
    EnemyUpdateSummary& summary) noexcept
{
    using gdp::entities::EnemyBehaviorState;
    if (!playerAlive) {
        enemy.lastKnownPlayerPosition = {};
        enemy.investigationTime = 0.0F;
        enemy.recoverTime = 0.0F;
        enemy.behavior = EnemyBehaviorState::Idle;
        ++summary.idle;
        separateEnemyIfNeeded(
            manager, enemyIds, collisionWorld, id, roomId, enemy, entity,
            moveSpeed, step, collider);
        return;
    }
    const bool investigationWasActive = enemy.investigationTime > 0.0F;
    if (updateEnemyInvestigation(
            manager, enemyIds, collisionWorld, id, roomId, enemy, entity,
            moveSpeed, step, investigation, collider)) {
        ++summary.investigating;
        return;
    }
    if (investigationWasActive) {
        const float recoverDuration = std::isfinite(investigation.recoverDuration)
            ? std::max(investigation.recoverDuration, 0.0F) : 0.0F;
        enemy.recoverTime = recoverDuration;
        if (recoverDuration > 0.0F) {
            enemy.behavior = EnemyBehaviorState::Recover;
            ++summary.recovering;
            return;
        }
        enemy.lastKnownPlayerPosition = {};
    }
    if (updateEnemyRecover(
            manager, enemyIds, collisionWorld, id, roomId, enemy, entity,
            moveSpeed, step, investigation, collider)) {
        ++summary.recovering;
        return;
    }
    if (updateEnemyPatrol(
            manager, enemyIds, collisionWorld, id, roomId, enemy, entity,
            moveSpeed, step, patrol, collider)) {
        ++summary.patrolling;
    } else {
        ++summary.idle;
    }
}

} // namespace

EnemyDifficultyConfigs makeEnemyDifficultyConfigs(const float multiplier) noexcept
{
    const float safeMultiplier = std::clamp(
        std::isfinite(multiplier) ? multiplier : 1.0F, 0.5F, 2.0F);
    EnemyDifficultyConfigs configs;
    configs.worker.moveSpeed *= safeMultiplier;
    configs.worker.attackDamage *= safeMultiplier;
    configs.guard.moveSpeed *= safeMultiplier;
    configs.guard.attackDamage *= safeMultiplier;
    configs.officer.moveSpeed *= safeMultiplier;
    configs.officer.attackDamage *= safeMultiplier;
    configs.engineer.moveSpeed *= safeMultiplier;
    configs.engineer.attackDamage *= safeMultiplier;
    configs.industrialRobot.longRangeMoveSpeed *= safeMultiplier;
    configs.industrialRobot.pressureMoveSpeed *= safeMultiplier;
    configs.industrialRobot.pursuitMoveSpeed *= safeMultiplier;
    configs.industrialRobot.longRangeDamage *= safeMultiplier;
    configs.industrialRobot.pressureDamage *= safeMultiplier;
    configs.industrialRobot.pursuitDamage *= safeMultiplier;
    return configs;
}

std::size_t alertEnemiesToNoise(
    gdp::entities::EntityManager& manager,
    const std::uint32_t roomId,
    const EnemyNoiseStimulus stimulus,
    const EnemyDifficultyConfigs configs) noexcept
{
    using gdp::entities::EnemyArchetype;
    using gdp::entities::EnemyBehaviorState;
    using gdp::entities::EnemyData;
    using gdp::entities::EntityKind;

    const float stimulusRadius = std::isfinite(stimulus.radius)
        ? std::max(stimulus.radius, 0.0F) : 0.0F;
    if (stimulusRadius <= 0.0F
        || !std::isfinite(stimulus.position.x)
        || !std::isfinite(stimulus.position.y)
        || !std::isfinite(stimulus.position.z)) {
        return 0U;
    }

    std::size_t alerted = 0U;
    const std::vector<gdp::entities::EntityId> enemyIds
        = manager.query(EntityKind::Enemy);
    for (const gdp::entities::EntityId id : enemyIds) {
        const gdp::entities::Entity* entity = manager.find(id);
        EnemyData* enemy = manager.findData<EnemyData>(id);
        if (entity == nullptr || enemy == nullptr || entity->roomId != roomId
            || !enemy->active || enemy->behavior == EnemyBehaviorState::Dead) {
            continue;
        }

        InvestigationConfig investigation;
        switch (enemy->archetype) {
        case EnemyArchetype::Worker:
            investigation = configs.worker.investigation;
            break;
        case EnemyArchetype::Guard:
            investigation = configs.guard.investigation;
            break;
        case EnemyArchetype::Officer:
            investigation = configs.officer.investigation;
            break;
        case EnemyArchetype::Engineer:
            investigation = configs.engineer.investigation;
            break;
        case EnemyArchetype::IndustrialRobot:
            investigation = configs.industrialRobot.investigation;
            break;
        }

        const float hearingRange = std::isfinite(investigation.hearingRange)
            ? std::max(investigation.hearingRange, 0.0F) : 0.0F;
        const float investigationDuration = std::isfinite(investigation.duration)
            ? std::max(investigation.duration, 0.0F) : 0.0F;
        const float effectiveRadius = std::min(stimulusRadius, hearingRange);
        if (effectiveRadius <= 0.0F || investigationDuration <= 0.0F
            || planarDistanceSquared(
                   entity->transform.position, stimulus.position)
                > effectiveRadius * effectiveRadius) {
            continue;
        }

        const bool newlyAlerted = enemy->investigationTime <= 0.0F
            && enemy->alertTime <= 0.0F && enemy->recoverTime <= 0.0F;
        enemy->lastKnownPlayerPosition = stimulus.position;
        enemy->investigationTime = investigationDuration;
        enemy->recoverTime = 0.0F;
        if (newlyAlerted) {
            const float alertDuration = std::isfinite(investigation.alertDuration)
                ? std::max(investigation.alertDuration, 0.0F) : 0.0F;
            enemy->alertTime = alertDuration;
            if (alertDuration > 0.0F) {
                enemy->behavior = EnemyBehaviorState::Alert;
            }
        }
        ++alerted;
    }
    return alerted;
}

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

    const std::vector<gdp::entities::EntityId> enemyIds
        = manager.query(EntityKind::Enemy);
    for (const gdp::entities::EntityId id : enemyIds) {
        Entity* entity = manager.find(id);
        EnemyData* enemy = manager.findData<EnemyData>(id);
        if (entity == nullptr || enemy == nullptr || entity->roomId != roomId
            || enemy->archetype != EnemyArchetype::Worker
            || !enemy->active
            || enemy->behavior == EnemyBehaviorState::Dead) {
            continue;
        }

        enemy->attackCooldown = std::max(0.0F, enemy->attackCooldown - step);
        const wkc::core::Vec3 offset = player.state().position - entity->transform.position;
        const wkc::core::Vec3 planarOffset{offset.x, 0.0F, offset.z};
        const float distance = wkc::core::length(planarOffset);
        const bool lineOfSight = playerVisibleToEnemy(
            *entity, player, distance, config.detectionRange, 1.0F,
            collisionWorld);
        if (lineOfSight) {
            rememberVisiblePlayer(*enemy, player, config.investigation);
        }
        if (updateEnemyAlert(
                manager, enemyIds, collisionWorld, id, roomId, *enemy, *entity,
                player.state().alive, config.moveSpeed, step,
                {.radius = 0.35F, .height = 1.8F})) {
            ++summary.alerting;
            continue;
        }
        if (!lineOfSight) {
            updateUnseenEnemy(
                manager, enemyIds, collisionWorld, id, roomId, *enemy, *entity,
                player.state().alive, config.moveSpeed, step, config.patrol,
                config.investigation, {.radius = 0.35F, .height = 1.8F}, summary);
            continue;
        }
        if (distance <= config.attackRange) {
            enemy->behavior = EnemyBehaviorState::Attack;
            ++summary.attacking;
            separateEnemyIfNeeded(
                manager, enemyIds, collisionWorld, id, roomId, *enemy, *entity,
                config.moveSpeed, step, {.radius = 0.35F, .height = 1.8F});
            turnEnemyToward(
                *entity, player.state().position - entity->transform.position,
                step, targetTurnRateDegreesPerSecond);
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
        const wkc::core::Vec3 direction = planarOffset * (1.0F / distance);
        moveEnemyAndFaceTravel(
            manager, enemyIds, collisionWorld, id, roomId, *enemy, *entity,
            direction, config.moveSpeed, step,
            {.radius = 0.35F, .height = 1.8F});
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

    const std::vector<gdp::entities::EntityId> enemyIds
        = manager.query(EntityKind::Enemy);
    for (const gdp::entities::EntityId id : enemyIds) {
        Entity* entity = manager.find(id);
        EnemyData* enemy = manager.findData<EnemyData>(id);
        if (entity == nullptr || enemy == nullptr || entity->roomId != roomId
            || enemy->archetype != EnemyArchetype::Guard
            || !enemy->active
            || enemy->behavior == EnemyBehaviorState::Dead) {
            continue;
        }

        enemy->attackCooldown = std::max(0.0F, enemy->attackCooldown - step);
        const wkc::core::Vec3 offset = player.state().position - entity->transform.position;
        const wkc::core::Vec3 planarOffset{offset.x, 0.0F, offset.z};
        const float distance = wkc::core::length(planarOffset);
        const bool lineOfSight = playerVisibleToEnemy(
            *entity, player, distance, config.detectionRange, 1.35F,
            collisionWorld);
        if (lineOfSight) {
            rememberVisiblePlayer(*enemy, player, config.investigation);
        }
        if (updateEnemyAlert(
                manager, enemyIds, collisionWorld, id, roomId, *enemy, *entity,
                player.state().alive, config.moveSpeed, step,
                {.radius = 0.35F, .height = 1.8F})) {
            ++summary.alerting;
            continue;
        }
        if (!lineOfSight) {
            updateUnseenEnemy(
                manager, enemyIds, collisionWorld, id, roomId, *enemy, *entity,
                player.state().alive, config.moveSpeed, step, config.patrol,
                config.investigation, {.radius = 0.35F, .height = 1.8F}, summary);
            continue;
        }
        const wkc::core::Vec3 towardPlayer = distance > 1.0e-7F
            ? planarOffset * (1.0F / distance)
            : wkc::core::Vec3{1.0F, 0.0F, 0.0F};
        if (distance < config.preferredMinimumRange) {
            enemy->behavior = EnemyBehaviorState::Reposition;
            ++summary.repositioning;
            if (step > 0.0F) {
                entity->transform.position = moveEnemy(
                    manager, enemyIds, collisionWorld, id, roomId, *enemy,
                    entity->transform.position,
                    towardPlayer * -1.0F, config.moveSpeed, step,
                    {.radius = 0.35F, .height = 1.8F});
            }
            turnEnemyToward(
                *entity, player.state().position - entity->transform.position,
                step, targetTurnRateDegreesPerSecond);
            continue;
        }

        if (distance > config.preferredMaximumRange) {
            enemy->behavior = EnemyBehaviorState::Chase;
            ++summary.chasing;
            if (step > 0.0F) {
                moveEnemyAndFaceTravel(
                    manager, enemyIds, collisionWorld, id, roomId, *enemy, *entity,
                    towardPlayer, config.moveSpeed, step,
                    {.radius = 0.35F, .height = 1.8F});
            }
            continue;
        }

        enemy->behavior = EnemyBehaviorState::Attack;
        ++summary.attacking;
        separateEnemyIfNeeded(
            manager, enemyIds, collisionWorld, id, roomId, *enemy, *entity,
            config.moveSpeed, step, {.radius = 0.35F, .height = 1.8F});
        turnEnemyToward(
            *entity, player.state().position - entity->transform.position,
            step, targetTurnRateDegreesPerSecond);
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

    const std::vector<gdp::entities::EntityId> enemyIds
        = manager.query(EntityKind::Enemy);
    for (const gdp::entities::EntityId id : enemyIds) {
        Entity* entity = manager.find(id);
        EnemyData* enemy = manager.findData<EnemyData>(id);
        if (entity == nullptr || enemy == nullptr || entity->roomId != roomId
            || enemy->archetype != EnemyArchetype::Officer
            || !enemy->active
            || enemy->behavior == EnemyBehaviorState::Dead) {
            continue;
        }

        enemy->attackCooldown = std::max(0.0F, enemy->attackCooldown - step);
        enemy->supportCooldown = std::max(0.0F, enemy->supportCooldown - step);
        const wkc::core::Vec3 offset = player.state().position - entity->transform.position;
        const wkc::core::Vec3 planarOffset{offset.x, 0.0F, offset.z};
        const float distance = wkc::core::length(planarOffset);
        const bool lineOfSight = playerVisibleToEnemy(
            *entity, player, distance, config.detectionRange, 1.4F,
            collisionWorld);
        if (lineOfSight) {
            rememberVisiblePlayer(*enemy, player, config.investigation);
        }
        if (updateEnemyAlert(
                manager, enemyIds, collisionWorld, id, roomId, *enemy, *entity,
                player.state().alive, config.moveSpeed, step,
                {.radius = 0.35F, .height = 1.8F})) {
            ++summary.alerting;
            continue;
        }
        if (!lineOfSight) {
            updateUnseenEnemy(
                manager, enemyIds, collisionWorld, id, roomId, *enemy, *entity,
                player.state().alive, config.moveSpeed, step, config.patrol,
                config.investigation, {.radius = 0.35F, .height = 1.8F}, summary);
            continue;
        }

        if (enemy->supportCooldown <= 0.0F && config.allyCooldownReduction > 0.0F) {
            std::size_t supported = 0;
            for (const gdp::entities::EntityId allyId : enemyIds) {
                if (allyId == id) {
                    continue;
                }
                const Entity* allyEntity = manager.find(allyId);
                EnemyData* ally = manager.findData<EnemyData>(allyId);
                if (allyEntity == nullptr || ally == nullptr || allyEntity->roomId != roomId
                    || (ally->archetype != EnemyArchetype::Worker
                        && ally->archetype != EnemyArchetype::Guard)
                    || !ally->active
                    || ally->behavior == EnemyBehaviorState::Dead
                    || ally->attackCooldown <= 0.0F) {
                    continue;
                }
                const wkc::core::Vec3 allyOffset
                    = allyEntity->transform.position - entity->transform.position;
                const float allyDistance = wkc::core::length(
                    wkc::core::Vec3{allyOffset.x, 0.0F, allyOffset.z});
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

        const wkc::core::Vec3 towardPlayer = distance > 1.0e-7F
            ? planarOffset * (1.0F / distance)
            : wkc::core::Vec3{1.0F, 0.0F, 0.0F};
        if (distance < config.preferredMinimumRange) {
            enemy->behavior = EnemyBehaviorState::Reposition;
            ++summary.repositioning;
            if (step > 0.0F) {
                entity->transform.position = moveEnemy(
                    manager, enemyIds, collisionWorld, id, roomId, *enemy,
                    entity->transform.position,
                    towardPlayer * -1.0F, config.moveSpeed, step,
                    {.radius = 0.35F, .height = 1.8F});
            }
            turnEnemyToward(
                *entity, player.state().position - entity->transform.position,
                step, targetTurnRateDegreesPerSecond);
            continue;
        }

        if (distance > config.preferredMaximumRange) {
            enemy->behavior = EnemyBehaviorState::Chase;
            ++summary.chasing;
            if (step > 0.0F) {
                moveEnemyAndFaceTravel(
                    manager, enemyIds, collisionWorld, id, roomId, *enemy, *entity,
                    towardPlayer, config.moveSpeed, step,
                    {.radius = 0.35F, .height = 1.8F});
            }
            continue;
        }

        enemy->behavior = EnemyBehaviorState::Attack;
        ++summary.attacking;
        separateEnemyIfNeeded(
            manager, enemyIds, collisionWorld, id, roomId, *enemy, *entity,
            config.moveSpeed, step, {.radius = 0.35F, .height = 1.8F});
        turnEnemyToward(
            *entity, player.state().position - entity->transform.position,
            step, targetTurnRateDegreesPerSecond);
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

    const std::vector<EntityId> enemyIds = manager.query(EntityKind::Enemy);
    for (const EntityId id : enemyIds) {
        Entity* entity = manager.find(id);
        EnemyData* enemy = manager.findData<EnemyData>(id);
        if (entity == nullptr || enemy == nullptr || entity->roomId != roomId
            || enemy->archetype != EnemyArchetype::Engineer
            || !enemy->active
            || enemy->behavior == EnemyBehaviorState::Dead) {
            continue;
        }

        enemy->attackCooldown = std::max(0.0F, enemy->attackCooldown - step);
        enemy->supportCooldown = std::max(0.0F, enemy->supportCooldown - step);
        const wkc::core::Vec3 offset
            = player.state().position - entity->transform.position;
        const wkc::core::Vec3 planarOffset{offset.x, 0.0F, offset.z};
        const float distance = wkc::core::length(planarOffset);
        const bool lineOfSight = playerVisibleToEnemy(
            *entity, player, distance, config.detectionRange, 1.25F,
            collisionWorld);
        if (lineOfSight) {
            rememberVisiblePlayer(*enemy, player, config.investigation);
        }
        if (updateEnemyAlert(
                manager, enemyIds, collisionWorld, id, roomId, *enemy, *entity,
                player.state().alive, config.moveSpeed, step,
                {.radius = 0.35F, .height = 1.8F})) {
            ++summary.alerting;
            continue;
        }
        if (!lineOfSight) {
            updateUnseenEnemy(
                manager, enemyIds, collisionWorld, id, roomId, *enemy, *entity,
                player.state().alive, config.moveSpeed, step, config.patrol,
                config.investigation, {.radius = 0.35F, .height = 1.8F}, summary);
            continue;
        }
        if (enemy->supportCooldown <= 0.0F && config.repairRange > 0.0F) {
            EntityId repairTarget;
            bool targetIsProp = false;
            float nearestDistance = std::numeric_limits<float>::max();
            const wkc::core::Vec3 repairOrigin
                = entity->transform.position + wkc::core::Vec3{0.0F, 1.0F, 0.0F};
            for (const EntityId allyId : enemyIds) {
                if (allyId == id || config.repairAmount <= 0.0F) {
                    continue;
                }
                const Entity* allyEntity = manager.find(allyId);
                const EnemyData* ally = manager.findData<EnemyData>(allyId);
                if (allyEntity == nullptr || ally == nullptr || allyEntity->roomId != roomId
                    || !ally->active
                    || ally->behavior == EnemyBehaviorState::Dead
                    || ally->health >= ally->maximumHealth) {
                    continue;
                }
                const wkc::core::Vec3 allyOffset
                    = allyEntity->transform.position - entity->transform.position;
                const float allyDistance = wkc::core::length(
                    wkc::core::Vec3{allyOffset.x, 0.0F, allyOffset.z});
                if (allyDistance > config.repairRange
                    || allyDistance >= nearestDistance
                    || !hasLineOfSight(
                        repairOrigin,
                        allyEntity->transform.position + wkc::core::Vec3{0.0F, 0.9F, 0.0F},
                        collisionWorld)) {
                    continue;
                }
                repairTarget = allyId;
                targetIsProp = false;
                nearestDistance = allyDistance;
            }
            for (const EntityId propId : manager.query(EntityKind::Prop)) {
                const Entity* propEntity = manager.find(propId);
                const PropData* prop = manager.findData<PropData>(propId);
                if (propEntity == nullptr || prop == nullptr || propEntity->roomId != roomId
                    || prop->state != PropState::Damaged) {
                    continue;
                }
                const wkc::core::Vec3 propOffset
                    = propEntity->transform.position - entity->transform.position;
                const float propDistance = wkc::core::length(
                    wkc::core::Vec3{propOffset.x, 0.0F, propOffset.z});
                if (propDistance > config.repairRange
                    || propDistance >= nearestDistance
                    || !hasLineOfSightToProp(
                        repairOrigin, manager, propId, collisionWorld)) {
                    continue;
                }
                repairTarget = propId;
                targetIsProp = true;
                nearestDistance = propDistance;
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

        const wkc::core::Vec3 towardPlayer = distance > 1.0e-7F
            ? planarOffset * (1.0F / distance)
            : wkc::core::Vec3{1.0F, 0.0F, 0.0F};
        if (distance < config.preferredMinimumRange) {
            enemy->behavior = EnemyBehaviorState::Reposition;
            ++summary.repositioning;
            if (step > 0.0F) {
                entity->transform.position = moveEnemy(
                    manager, enemyIds, collisionWorld, id, roomId, *enemy,
                    entity->transform.position,
                    towardPlayer * -1.0F, config.moveSpeed, step,
                    {.radius = 0.35F, .height = 1.8F});
            }
            turnEnemyToward(
                *entity, player.state().position - entity->transform.position,
                step, targetTurnRateDegreesPerSecond);
            continue;
        }
        if (distance > config.preferredMaximumRange) {
            enemy->behavior = EnemyBehaviorState::Chase;
            ++summary.chasing;
            if (step > 0.0F) {
                moveEnemyAndFaceTravel(
                    manager, enemyIds, collisionWorld, id, roomId, *enemy, *entity,
                    towardPlayer, config.moveSpeed, step,
                    {.radius = 0.35F, .height = 1.8F});
            }
            continue;
        }

        enemy->behavior = EnemyBehaviorState::Attack;
        ++summary.attacking;
        separateEnemyIfNeeded(
            manager, enemyIds, collisionWorld, id, roomId, *enemy, *entity,
            config.moveSpeed, step, {.radius = 0.35F, .height = 1.8F});
        turnEnemyToward(
            *entity, player.state().position - entity->transform.position,
            step, targetTurnRateDegreesPerSecond);
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

    const std::vector<gdp::entities::EntityId> enemyIds
        = manager.query(EntityKind::Enemy);
    for (const gdp::entities::EntityId id : enemyIds) {
        Entity* entity = manager.find(id);
        EnemyData* enemy = manager.findData<EnemyData>(id);
        if (entity == nullptr || enemy == nullptr || entity->roomId != roomId
            || enemy->archetype != EnemyArchetype::IndustrialRobot
            || !enemy->active
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

        const wkc::core::Vec3 offset = player.state().position - entity->transform.position;
        const wkc::core::Vec3 planarOffset{offset.x, 0.0F, offset.z};
        const float distance = wkc::core::length(planarOffset);
        const bool lineOfSight = playerVisibleToEnemy(
            *entity, player, distance, config.detectionRange, 1.45F,
            collisionWorld);
        if (lineOfSight) {
            rememberVisiblePlayer(*enemy, player, config.investigation);
        }
        if (updateEnemyAlert(
                manager, enemyIds, collisionWorld, id, roomId, *enemy, *entity,
                player.state().alive, moveSpeed, step,
                {.radius = 0.52F, .height = 1.9F})) {
            ++summary.alerting;
            continue;
        }
        if (!lineOfSight) {
            updateUnseenEnemy(
                manager, enemyIds, collisionWorld, id, roomId, *enemy, *entity,
                player.state().alive, moveSpeed, step, config.patrol,
                config.investigation, {.radius = 0.52F, .height = 1.9F}, summary);
            continue;
        }
        const wkc::core::Vec3 towardPlayer = distance > 1.0e-7F
            ? planarOffset * (1.0F / distance)
            : wkc::core::Vec3{1.0F, 0.0F, 0.0F};
        if (phase != IndustrialRobotPhase::Pursuit && distance < minimumRange) {
            enemy->behavior = EnemyBehaviorState::Reposition;
            ++summary.repositioning;
            if (step > 0.0F) {
                entity->transform.position = moveEnemy(
                    manager, enemyIds, collisionWorld, id, roomId, *enemy,
                    entity->transform.position,
                    towardPlayer * -1.0F, moveSpeed, step,
                    {.radius = 0.52F, .height = 1.9F});
            }
            turnEnemyToward(
                *entity, player.state().position - entity->transform.position,
                step, targetTurnRateDegreesPerSecond);
            continue;
        }
        if (distance > maximumRange) {
            enemy->behavior = EnemyBehaviorState::Chase;
            ++summary.chasing;
            if (step > 0.0F) {
                moveEnemyAndFaceTravel(
                    manager, enemyIds, collisionWorld, id, roomId, *enemy, *entity,
                    towardPlayer, moveSpeed, step,
                    {.radius = 0.52F, .height = 1.9F});
            }
            continue;
        }

        enemy->behavior = EnemyBehaviorState::Attack;
        ++summary.attacking;
        separateEnemyIfNeeded(
            manager, enemyIds, collisionWorld, id, roomId, *enemy, *entity,
            moveSpeed, step, {.radius = 0.52F, .height = 1.9F});
        turnEnemyToward(
            *entity, player.state().position - entity->transform.position,
            step, targetTurnRateDegreesPerSecond);
        if (enemy->attackCooldown <= 0.0F) {
            player.applyDamage(attackDamage);
            enemy->attackCooldown = attackInterval;
            ++summary.attacksLanded;
        }
    }
    return summary;
}

} // namespace gdp::gameplay
