#include "gameplay/RoomEvents.hpp"

#include <algorithm>
#include <utility>

namespace gdp::gameplay {
namespace {

bool enemyIsAlive(
    const gdp::entities::EntityManager& manager,
    const gdp::entities::EntityId id) noexcept
{
    const gdp::entities::EnemyData* enemy
        = manager.findData<gdp::entities::EnemyData>(id);
    return enemy != nullptr
        && enemy->active
        && enemy->behavior != gdp::entities::EnemyBehaviorState::Dead;
}

std::size_t liveEnemiesInRoom(
    const gdp::entities::EntityManager& manager,
    const std::uint32_t roomId)
{
    std::size_t count = 0;
    for (const gdp::entities::EntityId id
        : manager.query(gdp::entities::EntityKind::Enemy)) {
        const gdp::entities::Entity* entity = manager.find(id);
        if (entity != nullptr && entity->roomId == roomId && enemyIsAlive(manager, id)) {
            ++count;
        }
    }
    return count;
}

gdp::entities::EntityId spawnReward(
    RoomEventRuntime& runtime,
    gdp::entities::EntityManager& manager,
    const std::uint32_t roomId)
{
    if (runtime.rewardSpawned || runtime.plan.rewardQuantity == 0U) {
        return {};
    }
    runtime.rewardSpawned = true;
    return manager.createItem({
        .transform = {.position = runtime.plan.rewardPosition},
        .roomId = roomId,
        .type = runtime.plan.rewardType,
        .quantity = runtime.plan.rewardQuantity,
    });
}

} // namespace

RoomEventRuntime makeRoomEventRuntime(
    gdp::world::RoomEventPlan plan,
    const std::uint32_t roomId)
{
    RoomEventRuntime runtime;
    runtime.roomId = roomId;
    runtime.phase = plan.type == gdp::world::RoomEventType::None
        ? RoomEventPhase::Completed : RoomEventPhase::Dormant;
    runtime.plan = std::move(plan);
    return runtime;
}

std::vector<gdp::entities::EntityId> stageRoomEventFirstWave(
    RoomEventRuntime& runtime,
    gdp::entities::EntityManager& manager)
{
    using gdp::world::RoomEventTrigger;
    std::vector<gdp::entities::EntityId> staged;
    if (runtime.phase != RoomEventPhase::Dormant
        || runtime.plan.trigger != RoomEventTrigger::RoomEntry
        || runtime.plan.waveCount == 0U || !runtime.stagedEnemies.empty()) {
        return staged;
    }

    for (const gdp::world::EventEnemySpawn& spawn : runtime.plan.spawns) {
        if (spawn.wave != 0U) {
            continue;
        }
        const gdp::entities::EntityId enemy = manager.createEnemy({
            .transform = {.position = spawn.position},
            .roomId = runtime.roomId,
            .archetype = spawn.archetype,
            .health = spawn.health,
            .visualSeed = spawn.visualSeed,
            .active = false,
        });
        runtime.stagedEnemies.push_back(enemy);
        staged.push_back(enemy);
    }
    return staged;
}

RoomEventUpdateSummary updateRoomEvent(
    RoomEventRuntime& runtime,
    gdp::entities::EntityManager& manager,
    const bool roomEntered,
    const bool terminalUsed,
    const float fixedStepSeconds)
{
    using gdp::world::RoomEventTrigger;
    using gdp::world::RoomEventType;
    RoomEventUpdateSummary summary;
    if (runtime.phase == RoomEventPhase::Completed
        || runtime.plan.type == RoomEventType::None) {
        return summary;
    }

    if (runtime.phase == RoomEventPhase::Dormant) {
        const bool triggered = runtime.plan.trigger == RoomEventTrigger::TerminalUse
            ? terminalUsed : roomEntered;
        if (!triggered) {
            return summary;
        }
        runtime.phase = RoomEventPhase::Active;
        summary.activated = true;
    }

    if (runtime.plan.type == RoomEventType::SupplyDrop) {
        const gdp::entities::EntityId reward
            = spawnReward(runtime, manager, runtime.roomId);
        if (reward.isValid()) {
            summary.createdEntities.push_back(reward);
            summary.rewardsSpawned = 1U;
        }
        runtime.phase = RoomEventPhase::Completed;
        summary.completed = true;
        return summary;
    }

    runtime.cooldownRemaining = std::max(
        0.0F, runtime.cooldownRemaining - std::max(fixedStepSeconds, 0.0F));
    const std::size_t liveEventEnemies = static_cast<std::size_t>(std::count_if(
        runtime.eventEnemies.begin(), runtime.eventEnemies.end(),
        [&manager](const gdp::entities::EntityId id) {
            return enemyIsAlive(manager, id);
        }));
    const std::size_t liveRoomEnemies = liveEnemiesInRoom(manager, runtime.roomId);
    const bool lockdownWaitingForInitialClear
        = runtime.plan.type == RoomEventType::Lockdown
        && runtime.nextWave == 0U && liveRoomEnemies > 0U;

    if (runtime.nextWave < runtime.plan.waveCount
        && liveEventEnemies == 0U && runtime.cooldownRemaining <= 0.0F
        && !lockdownWaitingForInitialClear) {
        const std::uint32_t wave = runtime.nextWave;
        if (wave == 0U && !runtime.stagedEnemies.empty()) {
            for (const gdp::entities::EntityId id : runtime.stagedEnemies) {
                gdp::entities::EnemyData* enemy
                    = manager.findData<gdp::entities::EnemyData>(id);
                if (enemy == nullptr
                    || enemy->behavior == gdp::entities::EnemyBehaviorState::Dead) {
                    continue;
                }
                enemy->active = true;
                runtime.eventEnemies.push_back(id);
                summary.spawnedEnemies.push_back(id);
            }
            runtime.stagedEnemies.clear();
        } else {
            for (const gdp::world::EventEnemySpawn& spawn : runtime.plan.spawns) {
                if (spawn.wave != wave) {
                    continue;
                }
                const gdp::entities::EntityId enemy = manager.createEnemy({
                    .transform = {.position = spawn.position},
                    .roomId = runtime.roomId,
                    .archetype = spawn.archetype,
                    .health = spawn.health,
                    .visualSeed = spawn.visualSeed,
                });
                runtime.eventEnemies.push_back(enemy);
                summary.spawnedEnemies.push_back(enemy);
                summary.createdEntities.push_back(enemy);
            }
        }
        ++runtime.nextWave;
        runtime.cooldownRemaining = runtime.plan.interWaveDelay;
        summary.wavesSpawned = 1U;
        summary.enemiesSpawned = summary.spawnedEnemies.size();
        return summary;
    }

    const bool allWavesSpawned = runtime.nextWave >= runtime.plan.waveCount;
    const bool eventWaveClear = liveEventEnemies == 0U;
    const bool completionClear = runtime.plan.requiresRoomClear
        ? liveRoomEnemies == 0U : eventWaveClear;
    if (allWavesSpawned && completionClear) {
        const gdp::entities::EntityId reward
            = spawnReward(runtime, manager, runtime.roomId);
        if (reward.isValid()) {
            summary.createdEntities.push_back(reward);
            summary.rewardsSpawned = 1U;
        }
        runtime.phase = RoomEventPhase::Completed;
        summary.completed = true;
    }
    return summary;
}

bool isRoomLocked(const RoomEventRuntime& runtime) noexcept
{
    return runtime.plan.locksRoom && runtime.phase == RoomEventPhase::Active;
}

std::string_view roomEventTypeName(const gdp::world::RoomEventType type) noexcept
{
    using gdp::world::RoomEventType;
    switch (type) {
    case RoomEventType::None:
        return "None";
    case RoomEventType::TerminalAmbush:
        return "TerminalAmbush";
    case RoomEventType::Reinforcements:
        return "Reinforcements";
    case RoomEventType::Lockdown:
        return "Lockdown";
    case RoomEventType::SupplyDrop:
        return "SupplyDrop";
    case RoomEventType::Count:
        break;
    }
    return "Invalid";
}

std::string_view roomEventPhaseName(const RoomEventPhase phase) noexcept
{
    switch (phase) {
    case RoomEventPhase::Dormant:
        return "Dormant";
    case RoomEventPhase::Active:
        return "Active";
    case RoomEventPhase::Completed:
        return "Completed";
    }
    return "Invalid";
}

} // namespace gdp::gameplay
