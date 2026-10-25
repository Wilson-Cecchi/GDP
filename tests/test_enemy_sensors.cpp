#include "gameplay/Enemy.hpp"
#include "gameplay/EnemyFireGeometry.hpp"
#include "gameplay/EnemySensors.hpp"
#include "gameplay/EnemyProjectile.hpp"
#include "gameplay/PlayerGrenade.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

class ReferenceModelsForTest {
public:
    ReferenceModelsForTest()
    {
        const char* current = std::getenv("GDP_ENEMY_REFERENCE_MODELS");
        hadPrevious_ = current != nullptr;
        if (current != nullptr) previous_ = current;
#if defined(_WIN32)
        expect(_putenv_s("GDP_ENEMY_REFERENCE_MODELS", "1") == 0,
            "reference models can be enabled for tests");
#else
        expect(setenv("GDP_ENEMY_REFERENCE_MODELS", "1", 1) == 0,
            "reference models can be enabled for tests");
#endif
    }

    ~ReferenceModelsForTest()
    {
#if defined(_WIN32)
        expect(_putenv_s("GDP_ENEMY_REFERENCE_MODELS",
                   hadPrevious_ ? previous_.c_str() : "") == 0,
            "reference model environment restores after tests");
#else
        expect((hadPrevious_
                    ? setenv("GDP_ENEMY_REFERENCE_MODELS", previous_.c_str(), 1)
                    : unsetenv("GDP_ENEMY_REFERENCE_MODELS")) == 0,
            "reference model environment restores after tests");
#endif
    }

private:
    std::string previous_{};
    bool hadPrevious_{false};
};

float sensor(
    const gdp::gameplay::EnemySensorValues& values,
    const gdp::gameplay::EnemySensor channel)
{
    return values[static_cast<std::size_t>(channel)];
}

void testSensors()
{
    using namespace gdp::gameplay;
    gdp::entities::Entity entity;
    entity.transform.position = {0.0F, 0.0F, 0.0F};
    entity.transform.rotationDegrees.y = 0.0F;
    gdp::entities::EnemyData enemy;
    enemy.health = 50.0F;
    enemy.maximumHealth = 100.0F;
    Player player;
    player.spawn({0.0F, 0.0F, 2.0F});
    const CollisionWorld world{
        {{-5.0F, 0.0F, -5.0F}, {5.0F, 4.0F, 5.0F}},
        {{{-1.0F, 0.0F, 1.0F}, {1.0F, 2.0F, 1.2F}}}};
    const EnemySensorValues visible
        = sampleEnemySensors(entity, enemy, player, world, true, 10.0F);
    expect(std::abs(sensor(visible, EnemySensor::PlayerDistance) - 0.6F) < 1e-5F,
        "visible distance is normalized by detection range");
    expect(sensor(visible, EnemySensor::PlayerAngle) == 0.0F
            && sensor(visible, EnemySensor::PlayerVisible) == 1.0F,
        "visible target straight ahead has zero signed angle");
    expect(sensor(visible, EnemySensor::Health) == 0.0F,
        "half health maps to zero");
    expect(sensor(visible, EnemySensor::WallFront)
            < sensor(visible, EnemySensor::WallLeft)
            && sensor(visible, EnemySensor::WallLeft) == 1.0F
            && sensor(visible, EnemySensor::WallRight) == 1.0F,
        "front obstacle is sensed independently of clear sides");
    expect(std::all_of(visible.begin(), visible.end(), [](const float value) {
        return std::isfinite(value) && value >= -1.0F && value <= 1.0F;
    }), "all sensor channels stay finite and normalized");

    const EnemySensorValues hidden
        = sampleEnemySensors(entity, enemy, player, world, false, 10.0F);
    player.spawn({-3.0F, 0.0F, -2.0F});
    const EnemySensorValues hiddenMoved
        = sampleEnemySensors(entity, enemy, player, world, false, 10.0F);
    expect(hidden == hiddenMoved,
        "hidden target position cannot leak through distance or angle sensors");
    expect(sensor(hidden, EnemySensor::PlayerDistance) == -1.0F
            && sensor(hidden, EnemySensor::PlayerAngle) == 0.0F
            && sensor(hidden, EnemySensor::PlayerVisible) == -1.0F,
        "hidden player channels use neutral or absent values");

    enemy.magazineAmmo = 4U;
    const TacticalEnemySensorValues tactical = sampleTacticalEnemySensors(
        entity, enemy, player, world, true, 10.0F);
    expect(tactical.size() == enemyBrainShape(EnemyBrainContract::Tactical).inputs
            && tactical[static_cast<std::size_t>(TacticalEnemySensor::Ammo)] == 0.0F,
        "tactical brain receives its versioned sensor count and ammo ratio");
    const TacticalEnemySensorValues hiddenTactical = sampleTacticalEnemySensors(
        entity, enemy, player, world, false, 10.0F);
    expect(hiddenTactical[static_cast<std::size_t>(
                TacticalEnemySensor::PlayerVelocityForward)] == 0.0F
            && hiddenTactical[static_cast<std::size_t>(
                TacticalEnemySensor::PlayerVelocityRight)] == 0.0F
            && hiddenTactical[static_cast<std::size_t>(
                TacticalEnemySensor::PlayerAimThreat)] == 0.0F,
        "hidden player velocity and aim do not leak to tactical sensors");
}

void testCooperativeSensorsAndMemory()
{
    using namespace gdp::gameplay;
    using namespace gdp::entities;
    const CollisionWorld arena{{{-12.0F, 0.0F, -12.0F},
        {12.0F, 4.0F, 12.0F}}};
    EntityManager manager;
    Player player;
    player.spawn({0.0F, 0.0F, 0.0F});
    const EntityId guard = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 3.0F},
            .rotationDegrees = {0.0F, 180.0F, 0.0F}},
        .roomId = 1U, .archetype = EnemyArchetype::Guard});
    (void)manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 1.5F}},
        .roomId = 1U, .archetype = EnemyArchetype::Worker});
    EnemyData* enemy = manager.findData<EnemyData>(guard);
    const auto visible = sampleCooperativeEnemySensors(manager,
        *manager.find(guard), *enemy, player, arena, true, 16.0F);
    expect(visible.size()
            == enemyBrainShape(EnemyBrainContract::Cooperative).inputs
            && visible[static_cast<std::size_t>(
                CooperativeEnemySensor::FireLaneBlocked)] == 1.0F
            && visible[static_cast<std::size_t>(
                CooperativeEnemySensor::AllyDistance)] > -1.0F,
        "cooperative brain observes ally and blocked firing lane");

    enemy->lastKnownPlayerPosition = {1.0F, 0.0F, 0.0F};
    enemy->investigationTime = 3.0F;
    player.spawn({0.0F, 0.0F, 8.0F});
    const auto hidden = sampleCooperativeEnemySensors(manager,
        *manager.find(guard), *enemy, player, arena, false, 16.0F);
    player.spawn({2.0F, 0.0F, 8.0F});
    const auto hiddenMoved = sampleCooperativeEnemySensors(manager,
        *manager.find(guard), *enemy, player, arena, false, 16.0F);
    expect(hidden == hiddenMoved
            && hidden[static_cast<std::size_t>(
                CooperativeEnemySensor::KnownTargetDistance)] > -1.0F
            && hidden[static_cast<std::size_t>(
                CooperativeEnemySensor::FireLaneBlocked)] == -1.0F,
        "hidden player position never leaks through cooperative sensors");
    enemy->investigationTime = 0.0F;
    const auto forgotten = sampleCooperativeEnemySensors(manager,
        *manager.find(guard), *enemy, player, arena, false, 16.0F);
    expect(forgotten[static_cast<std::size_t>(
                CooperativeEnemySensor::KnownTargetDistance)] == -1.0F,
        "expired sighting is unavailable to the cooperative brain");

    const auto pursueMemory = [&](const wkc::core::Vec3 hiddenPlayer) {
        EntityManager isolated;
        Player target;
        target.spawn(hiddenPlayer);
        const EntityId id = isolated.createEnemy({
            .transform = {.position = {0.0F, 0.0F, 3.0F},
                .rotationDegrees = {0.0F, 180.0F, 0.0F}},
            .roomId = 1U, .archetype = EnemyArchetype::Guard});
        EnemyData* data = isolated.findData<EnemyData>(id);
        data->lastKnownPlayerPosition = {0.0F, 0.0F, 0.0F};
        data->investigationTime = 3.0F;
        NeuralEnemyExperiment brain{42U, EnemyArchetype::Guard,
            EnemyBrainContract::Cooperative};
        auto parameters = brain.genome.network.parameters();
        std::fill(parameters.begin(), parameters.end(), 0.0F);
        const auto shape = enemyBrainShape(EnemyBrainContract::Cooperative);
        const std::size_t outputBase = shape.hidden * (shape.inputs + 1U);
        parameters[outputBase + shape.hidden] = 1.0F;
        brain.controlAll = true;
        brain.logSelection = false;
        (void)updateGuards(isolated, target, 1U, arena, 0.1F,
            {}, {}, &brain);
        expect(brain.controlledTicks == 1U,
            "cooperative brain remains active during a valid hidden sighting");
        return isolated.find(id)->transform.position;
    };
    const auto first = pursueMemory({0.0F, 0.0F, 8.0F});
    const auto second = pursueMemory({2.0F, 0.0F, 8.0F});
    expect(first == second && first.z < 3.0F,
        "hidden-target movement follows remembered position, not actual player");
}

void testCooperativeFireLaneMatchesShot()
{
    using namespace gdp::gameplay;
    using namespace gdp::entities;
    const ReferenceModelsForTest referenceModels;
    const CollisionWorld arena{{{-12.0F, 0.0F, -12.0F},
        {12.0F, 4.0F, 12.0F}}};
    Player player;
    player.spawn({0.0F, 0.0F, 0.0F});
    const std::array archetypes{EnemyArchetype::Guard,
        EnemyArchetype::Officer, EnemyArchetype::Engineer,
        EnemyArchetype::IndustrialRobot};
    bool differsFromSimplifiedRay = false;
    for (const EnemyArchetype archetype : archetypes) {
        for (const float yawDegrees : {180.0F, 165.0F}) {
            EntityManager manager;
            const EntityId shooter = manager.createEnemy({
                .transform = {.position = {0.0F, 0.0F, 5.0F},
                    .rotationDegrees = {0.0F, yawDegrees, 0.0F}},
                .roomId = 1U, .archetype = archetype});
            const EntityId ally = manager.createEnemy({
                .transform = {.position = {0.0F, 0.0F, 3.0F}},
                .roomId = 1U, .archetype = EnemyArchetype::Worker});
            const Entity* shooterEntity = manager.find(shooter);
            const EnemyData* shooterData = manager.findData<EnemyData>(shooter);
            const EnemyShotRay shot = enemyShotRay(
                *shooterEntity, *shooterData, player);
            const float yaw = yawDegrees * 0.01745329251994329577F;
            const wkc::core::Vec3 forward{
                std::sin(yaw), 0.0F, std::cos(yaw)};
            const wkc::core::Vec3 simpleMuzzle = shooterEntity->transform.position
                + wkc::core::Vec3{0.0F, 1.2F, 0.0F} + forward * 0.45F;
            const wkc::core::Vec3 target = player.cameraPosition()
                - wkc::core::Vec3{0.0F, 0.22F, 0.0F};
            const float simpleHorizontal = std::max(0.1F,
                std::hypot(target.x - simpleMuzzle.x,
                    target.z - simpleMuzzle.z));
            const wkc::core::Vec3 simpleDirection = wkc::core::normalized({
                forward.x, (target.y - simpleMuzzle.y) / simpleHorizontal,
                forward.z});
            bool sawBlocked = false;
            bool sawClear = false;
            bool allMatch = true;
            for (int x = -20; x <= 20; ++x) {
                for (int z = 8; z <= 22; ++z) {
                    manager.find(ally)->transform.position = {
                        static_cast<float>(x) * 0.1F, 0.0F,
                        static_cast<float>(z) * 0.2F};
                    const bool blocked = allyInEnemyFireLane(manager,
                        shooter, shot.muzzle, shot.direction,
                        shot.targetDistance);
                    const auto values = sampleCooperativeEnemySensors(
                        manager, *shooterEntity, *shooterData, player,
                        arena, true, 16.0F);
                    const bool sensed = values[static_cast<std::size_t>(
                        CooperativeEnemySensor::FireLaneBlocked)] > 0.0F;
                    allMatch &= sensed == blocked;
                    sawBlocked |= blocked;
                    sawClear |= !blocked;
                    differsFromSimplifiedRay |= blocked
                        != allyInEnemyFireLane(manager, shooter, simpleMuzzle,
                            simpleDirection,
                            wkc::core::length(target - simpleMuzzle));
                }
            }
            expect(allMatch && sawBlocked && sawClear,
                "cooperative firing-lane sensor matches the actual shot for each ranged archetype and yaw");
        }
    }
    expect(differsFromSimplifiedRay,
        "offset weapon sockets produce firing lanes missed by a simplified central ray");
}

void testTacticalMovement()
{
    using namespace gdp::gameplay;
    using namespace gdp::entities;
    const CollisionWorld room{{{-8.0F, 0.0F, -8.0F}, {8.0F, 4.0F, 8.0F}}};
    const auto simulate = [&](const bool forwardRequested) {
        EntityManager manager;
        Player player;
        player.spawn({0.0F, 0.0F, 0.0F});
        const EntityId id = manager.createEnemy({
            .transform = {.position = {0.0F, 0.0F, 3.0F},
                .rotationDegrees = {0.0F, 180.0F, 0.0F}},
            .roomId = 1U,
            .archetype = EnemyArchetype::Guard,
        });
        NeuralEnemyExperiment brain{42U, EnemyArchetype::Guard,
            EnemyBrainContract::Tactical};
        auto parameters = brain.genome.network.parameters();
        std::fill(parameters.begin(), parameters.end(), 0.0F);
        constexpr std::size_t outputBase = 12U * (12U + 1U);
        constexpr std::size_t outputStride = 12U + 1U;
        parameters[outputBase + outputStride + 12U] = 1.0F;
        if (forwardRequested) {
            parameters[outputBase + 12U] = 1.0F;
        }
        brain.controlAll = true;
        brain.logSelection = false;
        (void)updateGuards(manager, player, 1U, room, 0.1F,
            GuardConfig{}, {}, &brain);
        return manager.find(id)->transform.position;
    };
    const auto strafe = simulate(false);
    const auto diagonal = simulate(true);
    expect(std::abs(strafe.x) > 0.02F && std::abs(strafe.z - 3.0F) < 0.005F,
        "tactical strafe is independent from forward movement");
    expect(std::hypot(diagonal.x, diagonal.z - 3.0F) <= 0.111F
            && std::abs(std::abs(diagonal.x) - std::abs(diagonal.z - 3.0F))
                < 0.01F,
        "tactical diagonal is normalized without slowing either axis to half");
}

void forceAttack(gdp::gameplay::NeuralWorkerExperiment& experiment)
{
    auto parameters = experiment.genome.network.parameters();
    std::fill(parameters.begin(), parameters.end(), 0.0F);
    // Hidden neuron 0: tanh(2). Attack output: tanh(2 * hidden 0).
    parameters[7] = 2.0F;
    parameters[64 + 3 * 9] = 2.0F;
}

void testExperimentalWorker()
{
    using namespace gdp::gameplay;
    using namespace gdp::entities;
    const CollisionWorld room{{{-5.0F, 0.0F, -5.0F}, {5.0F, 4.0F, 5.0F}}};
    EntityManager manager;
    Player player;
    player.spawn({0.0F, 0.0F, 0.0F});
    const EntityId first = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 1.0F},
            .rotationDegrees = {0.0F, 180.0F, 0.0F}},
        .roomId = 1,
        .archetype = EnemyArchetype::Worker,
    });
    const EntityId second = manager.createEnemy({
        .transform = {.position = {2.0F, 0.0F, 2.0F},
            .rotationDegrees = {0.0F, 225.0F, 0.0F}},
        .roomId = 1,
        .archetype = EnemyArchetype::Worker,
    });
    NeuralWorkerExperiment experiment{123};
    NeuralWorkerExperiment repeatedSeed{123};
    expect(std::equal(experiment.genome.network.parameters().begin(),
            experiment.genome.network.parameters().end(),
            repeatedSeed.genome.network.parameters().begin()),
        "experimental brain initialization is repeatable");
    forceAttack(experiment);
    const EnemyUpdateSummary update = updateWorkers(
        manager, player, 1, room, 0.01F, WorkerConfig{}, {}, &experiment);
    expect(experiment.selected == first,
        "one stable Worker is selected for neural control");
    expect(update.attacksLanded == 1 && player.state().health == 90.0F,
        "neural attack command acts through normal damage and cooldown");
    expect(manager.findData<EnemyData>(second)->behavior == EnemyBehaviorState::Chase,
        "other Workers retain their original behavior");
    const auto heldOutputs = experiment.outputs;
    std::fill(experiment.genome.network.parameters().begin(),
        experiment.genome.network.parameters().end(), 0.0F);
    (void)updateWorkers(manager, player, 1, room, 0.05F,
        WorkerConfig{}, {}, &experiment);
    expect(experiment.outputs == heldOutputs,
        "neural outputs are reused between 10 Hz decisions");
    (void)updateWorkers(manager, player, 1, room, 0.05F,
        WorkerConfig{}, {}, &experiment);
    expect(experiment.outputs != heldOutputs,
        "neural outputs refresh after the decision interval");

    const CollisionWorld blocked{
        {{-5.0F, 0.0F, -5.0F}, {5.0F, 4.0F, 5.0F}},
        {{{-0.8F, 0.0F, 0.35F}, {0.8F, 2.5F, 0.65F}}}};
    EntityManager blockedManager;
    const EntityId blockedWorker = blockedManager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 1.0F},
            .rotationDegrees = {0.0F, 180.0F, 0.0F}},
        .roomId = 1,
        .archetype = EnemyArchetype::Worker,
    });
    NeuralWorkerExperiment blockedExperiment{123};
    forceAttack(blockedExperiment);
    Player hiddenPlayer;
    hiddenPlayer.spawn({0.0F, 0.0F, 0.0F});
    const EnemyUpdateSummary hiddenUpdate = updateWorkers(
        blockedManager, hiddenPlayer, 1, blocked, 0.1F,
        WorkerConfig{}, {}, &blockedExperiment);
    expect(blockedExperiment.selected == blockedWorker
            && blockedExperiment.controlledTicks == 0U
            && hiddenUpdate.attacksLanded == 0
            && hiddenPlayer.state().health == 100.0F,
        "hidden neural enemy falls back to safe investigation without attacking");

    EntityManager turnedManager;
    (void)turnedManager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 1.0F},
            .rotationDegrees = {0.0F, 180.0F, 0.0F}},
        .roomId = 1,
        .archetype = EnemyArchetype::Worker,
    });
    Player turnedPlayer;
    turnedPlayer.spawn({0.0F, 0.0F, 0.0F});
    NeuralEnemyExperiment turning{123};
    forceAttack(turning);
    turning.genome.network.parameters()[64 + 2 * 9] = 2.0F;
    const EnemyUpdateSummary turnedUpdate = updateWorkers(
        turnedManager, turnedPlayer, 1, room, 1.0F,
        WorkerConfig{}, {}, &turning);
    expect(turnedUpdate.attacksLanded == 0U
            && turnedPlayer.state().health == 100.0F,
        "attack checks visibility again after the neural turn");
}

void testNeuralWorkerRemembersVisiblePlayer()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;
    const CollisionWorld open{{{-5.0F, 0.0F, -5.0F}, {5.0F, 4.0F, 5.0F}}};
    const CollisionWorld blocked{
        {{-5.0F, 0.0F, -5.0F}, {5.0F, 4.0F, 5.0F}},
        {{{0.0F, 0.0F, -1.0F}, {2.0F, 3.0F, -0.8F}}},
    };
    EntityManager manager;
    Player player;
    player.spawn({1.0F, 0.0F, 0.0F});
    const EntityId worker = manager.createEnemy({
        .transform = {.position = {1.0F, 0.0F, 1.5F},
            .rotationDegrees = {0.0F, 180.0F, 0.0F}},
        .roomId = 1U,
        .archetype = EnemyArchetype::Worker,
    });
    NeuralEnemyExperiment brain{42U, EnemyArchetype::Worker,
        EnemyBrainContract::Tactical};
    std::fill(brain.genome.network.parameters().begin(),
        brain.genome.network.parameters().end(), 0.0F);
    brain.controlAll = true;
    brain.logSelection = false;
    const EnemyUpdateSummary seen = updateWorkers(
        manager, player, 1U, open, 0.1F, {}, {}, &brain);
    const EnemyData* enemy = manager.findData<EnemyData>(worker);
    expect(brain.controlledTicks == 1U && seen.idle == 1U
            && enemy->lastKnownPlayerPosition == wkc::core::Vec3{1.0F, 0.0F, 0.0F}
            && enemy->investigationTime > 0.0F,
        "active neural Worker records a visible player before motor control returns");

    (void)player.relocate({1.0F, 0.0F, -2.0F}, -90.0F);
    const EnemyUpdateSummary hidden = updateWorkers(
        manager, player, 1U, blocked, 0.1F, {}, {}, &brain);
    expect(hidden.investigating == 1U && brain.controlledTicks == 1U
            && enemy->lastKnownPlayerPosition == wkc::core::Vec3{1.0F, 0.0F, 0.0F}
            && enemy->behavior == EnemyBehaviorState::Investigate,
        "neural Worker navigates to its last sighting after line of sight breaks");
}

void testExperimentalArchetypes()
{
    using namespace gdp::gameplay;
    using namespace gdp::entities;
    const CollisionWorld room{{{-16.0F, 0.0F, -16.0F}, {16.0F, 4.0F, 16.0F}}};
    for (const EnemyArchetype archetype : {
             EnemyArchetype::Guard, EnemyArchetype::Officer,
             EnemyArchetype::Engineer, EnemyArchetype::IndustrialRobot}) {
        EntityManager manager;
        Player player;
        player.spawn({0.0F, 0.0F, 0.0F});
        const float distance = archetype == EnemyArchetype::IndustrialRobot
            ? 9.0F : archetype == EnemyArchetype::Officer ? 7.0F : 5.0F;
        const EntityId id = manager.createEnemy({
            .transform = {.position = {0.0F, 0.0F, distance},
                .rotationDegrees = {0.0F, 180.0F, 0.0F}},
            .roomId = 1,
            .archetype = archetype,
        });
        NeuralEnemyExperiment experiment{123, archetype};
        forceAttack(experiment);
        EnemyUpdateSummary update;
        switch (archetype) {
        case EnemyArchetype::Worker:
            break;
        case EnemyArchetype::Guard:
            update = updateGuards(manager, player, 1, room, 0.01F,
                GuardConfig{}, {}, &experiment);
            break;
        case EnemyArchetype::Officer:
            update = updateOfficers(manager, player, 1, room, 0.01F,
                OfficerConfig{}, {}, &experiment);
            break;
        case EnemyArchetype::Engineer:
            update = updateEngineers(manager, player, 1, room, 0.01F,
                EngineerConfig{}, {}, &experiment);
            break;
        case EnemyArchetype::IndustrialRobot:
            update = updateIndustrialRobots(manager, player, 1, room, 0.01F,
                IndustrialRobotConfig{}, {}, &experiment);
            break;
        }
        if (archetype == EnemyArchetype::IndustrialRobot) {
            expect(experiment.selected == id && experiment.controlledTicks == 1U
                    && update.attacksLanded == 0U
                    && player.state().health == 100.0F,
                "neural Boss also respects the first-shot reaction window");
            bool attackedAfterWindup = false;
            for (int tick = 0; tick < 60 && !attackedAfterWindup; ++tick) {
                update = updateIndustrialRobots(manager, player, 1, room, 0.01F,
                    IndustrialRobotConfig{}, {}, &experiment);
                attackedAfterWindup = update.attacksLanded == 1U;
            }
            expect(attackedAfterWindup && player.state().health < 100.0F,
                "neural Boss attacks after its first-shot windup");
        } else {
            expect(experiment.selected == id && experiment.controlledTicks == 1U
                    && update.attacksLanded == 1U
                    && player.state().health < 100.0F,
                "each other non-Worker archetype can execute its neural attack");
        }
    }
}

void testNeuralLocomotionFacing()
{
    using namespace gdp::gameplay;
    using namespace gdp::entities;
    const CollisionWorld room{{{-5.0F, 0.0F, -5.0F}, {5.0F, 4.0F, 5.0F}}};
    EntityManager manager;
    Player player;
    player.spawn({0.0F, 0.0F, 0.0F});
    const EntityId id = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 3.0F},
            .rotationDegrees = {0.0F, 180.0F, 0.0F}},
        .roomId = 1,
        .archetype = EnemyArchetype::Worker,
    });
    NeuralEnemyExperiment experiment{123};
    auto parameters = experiment.genome.network.parameters();
    std::fill(parameters.begin(), parameters.end(), 0.0F);
    parameters[7] = 2.0F;
    parameters[64 + 1 * 9] = 2.0F; // Strong lateral output only.
    const wkc::core::Vec3 start = manager.find(id)->transform.position;
    (void)updateWorkers(manager, player, 1, room, 0.1F,
        WorkerConfig{}, {}, &experiment);
    const wkc::core::Vec3 afterStrafe = manager.find(id)->transform.position;
    const float cautiousTravel = std::hypot(
        afterStrafe.x - start.x, afterStrafe.z - start.z);
    expect(cautiousTravel > 0.01F
            && (start.z - afterStrafe.z) / cautiousTravel > 0.96F,
        "lateral-only output becomes cautious forward motion, not side sliding");

    parameters[64 + 0 * 9] = 2.0F; // Forward and lateral together.
    (void)updateWorkers(manager, player, 1, room, 0.1F,
        WorkerConfig{}, {}, &experiment);
    const wkc::core::Vec3 end = manager.find(id)->transform.position;
    const float dx = end.x - afterStrafe.x;
    const float dz = end.z - afterStrafe.z;
    const float travel = std::hypot(dx, dz);
    expect(travel > 0.01F && -dz / travel > 0.96F,
        "moving neural enemy travels mostly in the direction it faces");
}

void testNeuralRangedAim()
{
    using namespace gdp::gameplay;
    using namespace gdp::entities;
    const CollisionWorld room{{{-8.0F, 0.0F, -8.0F}, {8.0F, 4.0F, 8.0F}}};
    EntityManager manager;
    Player player;
    player.spawn({3.0F, 0.0F, 0.0F});
    const EntityId id = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 5.0F},
            .rotationDegrees = {0.0F, 180.0F, 0.0F}},
        .roomId = 1,
        .archetype = EnemyArchetype::Guard,
    });
    NeuralEnemyExperiment experiment{123, EnemyArchetype::Guard};
    forceAttack(experiment);
    const EnemyUpdateSummary broadVision = updateGuards(
        manager, player, 1, room, 0.01F, GuardConfig{}, {}, &experiment);
    expect(broadVision.attacksLanded == 0U
            && player.state().health == 100.0F,
        "neural ranged enemy cannot hit merely because the target is in vision");
    manager.find(id)->transform.rotationDegrees.y = 149.0F;
    const EnemyUpdateSummary aimed = updateGuards(
        manager, player, 1, room, 0.01F, GuardConfig{}, {}, &experiment);
    expect(aimed.attacksLanded == 1U && player.state().health < 100.0F,
        "neural ranged enemy can hit after visibly aligning with the target");
}

void testNeuralBossNavigatesCover()
{
    using namespace gdp::gameplay;
    using namespace gdp::entities;
    const auto run = [](const bool elevated, const int ticks) {
        const CollisionWorld room{
            {{-8.0F, 0.0F, -8.0F}, {8.0F, 5.0F, 8.0F}},
            {{{-1.0F, 0.0F, 0.5F}, {1.0F, 0.9F, 1.5F}}},
        };
        EntityManager manager;
        Player player;
        player.spawn({0.0F, elevated ? 0.9F : 0.0F,
            elevated ? 1.0F : -1.0F});
        const EntityId id = manager.createEnemy({
            .transform = {.position = {0.0F, 0.0F, 4.0F},
                .rotationDegrees = {0.0F, 180.0F, 0.0F}},
            .roomId = 1,
            .archetype = EnemyArchetype::IndustrialRobot,
            .visualSeed = 0xB055U,
        });
        NeuralEnemyExperiment experiment{123, EnemyArchetype::IndustrialRobot};
        auto parameters = experiment.genome.network.parameters();
        std::fill(parameters.begin(), parameters.end(), 0.0F);
        parameters[7] = 2.0F;
        parameters[64] = 2.0F; // Keep requesting a forward step.
        IndustrialRobotConfig config;
        config.longRangeMoveSpeed = 1.5F;
        config.pressureMoveSpeed = 1.5F;
        config.pursuitMoveSpeed = 1.5F;
        config.longRangeDamage = 0.0F;
        config.pressureDamage = 0.0F;
        config.pursuitDamage = 0.0F;
        for (int tick = 0; tick < ticks; ++tick) {
            (void)updateIndustrialRobots(
                manager, player, 1, room, 1.0F / 60.0F,
                config, {}, &experiment);
        }
        return std::pair{manager.find(id)->transform.position,
            experiment.controlledTicks};
    };

    const auto [behindCover, coveredTicks] = run(false, 270);
    expect(coveredTicks > 0U && std::abs(behindCover.x) > 0.8F
            && behindCover.z < -0.5F,
        "neural boss routes around low visible cover instead of pushing into it");
    const auto [aboveCover, elevatedTicks] = run(true, 960);
    expect(elevatedTicks > 0U && std::abs(aboveCover.x) > 1.4F
            && aboveCover.z < 2.4F,
        "neural boss approaches a flank when the player stands on a prop");
    expect(run(true, 1080).first == aboveCover,
        "neural boss holds its reachable flank instead of oscillating at the prop");
    expect(run(true, 960).first == aboveCover,
        "neural boss obstacle approach remains deterministic");
}

void testNeuralBrainControlsEveryEnemyIndependently()
{
    using namespace gdp::gameplay;
    using namespace gdp::entities;
    const CollisionWorld room{{{-5.0F, 0.0F, -5.0F}, {5.0F, 4.0F, 5.0F}}};
    EntityManager manager;
    Player player;
    player.spawn({0.0F, 0.0F, 0.0F});
    const EntityId left = manager.createEnemy({
        .transform = {.position = {-0.6F, 0.0F, 0.8F},
            .rotationDegrees = {0.0F, 143.13F, 0.0F}},
        .roomId = 1,
        .archetype = EnemyArchetype::Worker,
    });
    const EntityId right = manager.createEnemy({
        .transform = {.position = {0.6F, 0.0F, 0.8F},
            .rotationDegrees = {0.0F, -143.13F, 0.0F}},
        .roomId = 1,
        .archetype = EnemyArchetype::Worker,
    });
    NeuralEnemyExperiment brain{42};
    forceAttack(brain);
    brain.controlAll = true;
    brain.logSelection = false;
    const EnemyUpdateSummary first = updateWorkers(
        manager, player, 1, room, 0.01F, WorkerConfig{}, {}, &brain);
    expect(first.attacksLanded == 2U && player.state().health == 80.0F
            && !brain.selected && brain.controlledTicks == 2U
            && brain.motors.size() == 2U,
        "active brain controls both visible Workers without selecting only one");
    brain.motors.at(left.packed()).outputs.fill(0.0F);
    brain.motors.at(left.packed()).decisionSecondsRemaining = 1.0F;
    (void)updateWorkers(manager, player, 1, room, 0.11F,
        WorkerConfig{}, {}, &brain);
    expect(brain.motors.at(left.packed()).outputs[3] == 0.0F
            && brain.motors.at(right.packed()).outputs[3] > 0.65F,
        "each enemy retains its own decision timer and motor outputs");
}

void testActiveWorkerMaintainsMeleeThreat()
{
    using namespace gdp::gameplay;
    using namespace gdp::entities;
    const CollisionWorld room{{{-5.0F, 0.0F, -5.0F}, {5.0F, 4.0F, 5.0F}}};
    EntityManager manager;
    Player player;
    player.spawn({0.0F, 0.0F, 0.0F});
    const EntityId id = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 1.0F},
            .rotationDegrees = {0.0F, 180.0F, 0.0F}},
        .roomId = 1,
        .archetype = EnemyArchetype::Worker,
    });
    NeuralEnemyExperiment attacking{42};
    forceAttack(attacking);
    attacking.genome.network.parameters()[64] = 2.0F;
    attacking.controlAll = true;
    attacking.logSelection = false;
    const auto initialPosition = manager.find(id)->transform.position;
    const EnemyUpdateSummary hit = updateWorkers(
        manager, player, 1, room, 0.1F, WorkerConfig{}, {}, &attacking);
    expect(hit.attacksLanded == 1U
            && manager.find(id)->transform.position == initialPosition,
        "active melee brain stops inside attack range instead of crossing player");

    EntityManager stalledManager;
    const EntityId stalled = stalledManager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 3.0F},
            .rotationDegrees = {0.0F, 180.0F, 0.0F}},
        .roomId = 1,
        .archetype = EnemyArchetype::Worker,
    });
    NeuralEnemyExperiment inactive{42};
    std::fill(inactive.genome.network.parameters().begin(),
        inactive.genome.network.parameters().end(), 0.0F);
    inactive.controlAll = true;
    inactive.logSelection = false;
    for (int tick = 0; tick < 180; ++tick) {
        (void)updateWorkers(stalledManager, player, 1, room,
            1.0F / 60.0F, WorkerConfig{}, {}, &inactive);
    }
    expect(inactive.controlledTicks < 180U
            && stalledManager.find(stalled)->transform.position.z < 2.5F,
        "unproductive active brain yields temporarily to scripted pursuit");
}

void testEmptyRangedBrainYieldsToScriptedRecovery()
{
    using namespace gdp::gameplay;
    using namespace gdp::entities;
    const CollisionWorld room{{{-8.0F, 0.0F, -8.0F},
        {8.0F, 4.0F, 8.0F}}};
    Player player;
    player.spawn({0.0F, 0.0F, 0.0F});
    EntityManager manager;
    const EntityId id = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 5.0F},
            .rotationDegrees = {0.0F, 180.0F, 0.0F}},
        .roomId = 1U, .archetype = EnemyArchetype::Guard});
    EnemyData* enemy = manager.findData<EnemyData>(id);
    enemy->magazineAmmo = 0U;
    enemy->reserveAmmo = 0U;
    NeuralEnemyExperiment brain{42U, EnemyArchetype::Guard};
    forceAttack(brain);
    brain.controlAll = true;
    brain.logSelection = false;
    std::vector<EnemyProjectile> shots;
    for (int tick = 0; tick < 180; ++tick) {
        (void)updateGuards(manager, player, 1U, room, 1.0F / 60.0F,
            GuardConfig{}, {.projectiles = &shots}, &brain);
    }
    expect(shots.empty() && brain.controlledTicks < 180U,
        "an empty ranged brain cannot count aiming as a successful attack forever");
}

void testEnemyProjectiles()
{
    using namespace gdp::gameplay;
    gdp::entities::EntityManager manager;
    const CollisionWorld clear{{{-10.0F, 0.0F, -10.0F},
        {10.0F, 4.0F, 10.0F}}};
    Player player;
    player.spawn({0.0F, 0.0F, 0.0F});
    const EnemyProjectile incoming{{0.0F, 1.2F, 5.0F},
        {0.0F, 0.0F, -10.0F}, {}, 1U, 8.0F, 0.8F, true, true, 5.0F};
    EnemyProjectileSystem shots;
    shots.spawn(std::span{&incoming, 1U});
    expect(player.state().health == 100.0F && shots.projectiles().size() == 1U,
        "enemy projectile does not damage on firing");
    std::vector<EnemyProjectileEvent> events;
    for (int tick = 0; tick < 60 && events.empty(); ++tick) {
        events = shots.update(1.0F / 60.0F, player, clear, manager);
    }
    expect(events.size() == 1U && events[0].hitPlayer
            && player.state().health == 92.0F,
        "swept projectile hits player exactly once");

    player.spawn({0.0F, 0.0F, 0.0F});
    const CollisionWorld covered{{{-10.0F, 0.0F, -10.0F},
        {10.0F, 4.0F, 10.0F}},
        {{{-1.0F, 0.0F, 2.0F}, {1.0F, 2.0F, 2.2F}}}};
    shots.spawn(std::span{&incoming, 1U});
    events.clear();
    for (int tick = 0; tick < 60 && events.empty(); ++tick) {
        events = shots.update(1.0F / 60.0F, player, covered, manager);
    }
    expect(events.size() == 1U && events[0].blocked
            && player.state().health == 100.0F,
        "solid cover intercepts projectile before player");

    shots.spawn(std::span{&incoming, 1U});
    expect(player.relocate({2.0F, 0.0F, 0.0F}, 0.0F),
        "player relocates out of incoming trajectory");
    events.clear();
    for (int tick = 0; tick < 60 && events.empty(); ++tick) {
        events = shots.update(1.0F / 60.0F, player, clear, manager);
    }
    expect(events.size() == 1U && events[0].dodged
            && player.state().health == 100.0F,
        "moving away from a threatening launch earns real dodge event");
}

void testEnemyFriendlyFire()
{
    using namespace gdp::gameplay;
    using namespace gdp::entities;
    const CollisionWorld clear{{{-10.0F, 0.0F, -10.0F},
        {10.0F, 4.0F, 10.0F}}};
    Player player;
    player.spawn({0.0F, 0.0F, 0.0F});
    EntityManager manager;
    const EntityId shooter = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 5.0F}},
        .roomId = 1U, .archetype = EnemyArchetype::Guard});
    const EntityId front = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 3.0F}},
        .roomId = 1U, .archetype = EnemyArchetype::Guard});
    const EntityId rear = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 2.0F}},
        .roomId = 1U, .archetype = EnemyArchetype::Guard});
    const EnemyProjectile incoming{{0.0F, 1.2F, 5.0F},
        {0.0F, 0.0F, -10.0F}, shooter, 1U, 8.0F, 0.8F,
        true, true, 5.0F};
    expect(allyInEnemyFireLane(manager, shooter, incoming.position,
            incoming.velocity, 5.0F),
        "firing lane detects ally between shooter and player");
    EnemyProjectileSystem shots;
    shots.spawn(std::span{&incoming, 1U});
    std::vector<EnemyProjectileEvent> events;
    for (int tick = 0; tick < 60 && events.empty(); ++tick) {
        events = shots.update(1.0F / 60.0F, player, clear, manager);
    }
    expect(events.size() == 1U && events[0].hitEnemy == front
            && !events[0].hitPlayer && !events[0].killedEnemy
            && events[0].damageApplied == 8.0F
            && manager.findData<EnemyData>(front)->health == 42.0F
            && manager.findData<EnemyData>(rear)->health == 50.0F
            && manager.findData<EnemyData>(shooter)->health == 50.0F
            && player.state().health == 100.0F,
        "first living ally takes real damage; shooter and rear ally are safe");

    manager.findData<EnemyData>(front)->health = 4.0F;
    shots.spawn(std::span{&incoming, 1U});
    events.clear();
    for (int tick = 0; tick < 60 && events.empty(); ++tick) {
        events = shots.update(1.0F / 60.0F, player, clear, manager);
    }
    expect(events.size() == 1U && events[0].hitEnemy == front
            && events[0].killedEnemy && events[0].damageApplied == 4.0F
            && manager.findData<EnemyData>(front)->behavior
                == EnemyBehaviorState::Dead,
        "friendly fire uses normal enemy death state and reports actual damage");

    const CollisionWorld covered{{{-10.0F, 0.0F, -10.0F},
        {10.0F, 4.0F, 10.0F}},
        {{{-1.0F, 0.0F, 4.0F}, {1.0F, 2.0F, 4.2F}}}};
    shots.spawn(std::span{&incoming, 1U});
    events.clear();
    for (int tick = 0; tick < 60 && events.empty(); ++tick) {
        events = shots.update(1.0F / 60.0F, player, covered, manager);
    }
    expect(events.size() == 1U && events[0].blocked
            && manager.findData<EnemyData>(rear)->health == 50.0F,
        "world cover intercepts the shot before a living ally");

    manager.find(rear)->transform.position = {0.0F, 0.0F, -2.0F};
    expect(!allyInEnemyFireLane(manager, shooter, incoming.position,
            incoming.velocity, 5.0F),
        "dead and rear allies do not block a clear shot");
    shots.spawn(std::span{&incoming, 1U});
    events.clear();
    for (int tick = 0; tick < 60 && events.empty(); ++tick) {
        events = shots.update(1.0F / 60.0F, player, clear, manager);
    }
    expect(events.size() == 1U && events[0].hitPlayer
            && player.state().health == 92.0F
            && manager.findData<EnemyData>(rear)->health == 50.0F,
        "dead ally is ignored and player takes the first live impact");
}

void testDifficultyAndFireDiscipline()
{
    using namespace gdp::gameplay;
    using namespace gdp::entities;
    const CollisionWorld arena{{{-12.0F, 0.0F, -12.0F},
        {12.0F, 4.0F, 12.0F}}};
    Player player;
    player.spawn({0.0F, 0.0F, 0.0F});
    const auto sharedAt = [&](const Difficulty difficulty, const float allyX) {
        EntityManager manager;
        (void)manager.createEnemy({
            .transform = {.position = {0.0F, 0.0F, 6.0F},
                .rotationDegrees = {0.0F, 180.0F, 0.0F}},
            .roomId = 1U, .archetype = EnemyArchetype::Guard});
        const EntityId ally = manager.createEnemy({
            .transform = {.position = {allyX, 0.0F, 6.0F},
                .rotationDegrees = {0.0F, 0.0F, 0.0F}},
            .roomId = 1U, .archetype = EnemyArchetype::Worker});
        std::vector<EnemyProjectile> shots;
        (void)updateGuards(manager, player, 1U, arena, 1.0F / 60.0F,
            {}, {.projectiles = &shots, .difficulty = difficulty});
        return manager.findData<EnemyData>(ally)->investigationTime > 0.0F;
    };
    expect(!sharedAt(Difficulty::Easy, 5.0F)
            && sharedAt(Difficulty::Normal, 5.0F)
            && !sharedAt(Difficulty::Normal, 8.0F)
            && sharedAt(Difficulty::Hard, 8.0F),
        "team awareness range scales by difficulty and Easy stays independent");

    EntityManager hiddenManager;
    (void)hiddenManager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 6.0F},
            .rotationDegrees = {0.0F, 180.0F, 0.0F}},
        .roomId = 1U, .archetype = EnemyArchetype::Guard});
    const EntityId hiddenAlly = hiddenManager.createEnemy({
        .transform = {.position = {5.0F, 0.0F, 6.0F}},
        .roomId = 1U, .archetype = EnemyArchetype::Worker});
    const CollisionWorld screened{{{-12.0F, 0.0F, -12.0F},
        {12.0F, 4.0F, 12.0F}},
        {{{-1.0F, 0.0F, 2.0F}, {1.0F, 3.0F, 3.0F}}}};
    std::vector<EnemyProjectile> hiddenShots;
    (void)updateGuards(hiddenManager, player, 1U, screened,
        1.0F / 60.0F, {},
        {.projectiles = &hiddenShots, .difficulty = Difficulty::Hard});
    expect(hiddenManager.findData<EnemyData>(hiddenAlly)->investigationTime
            == 0.0F,
        "Hard cannot share a player position that no ally observed");

    EntityManager manager;
    const EntityId guard = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 6.0F},
            .rotationDegrees = {0.0F, 180.0F, 0.0F}},
        .roomId = 1U, .archetype = EnemyArchetype::Guard});
    (void)manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 3.0F}},
        .roomId = 1U, .archetype = EnemyArchetype::Worker});
    std::vector<EnemyProjectile> shots;
    const EnemyPerception perception{
        .projectiles = &shots, .difficulty = Difficulty::Hard};
    const EnemyUpdateSummary first = updateGuards(
        manager, player, 1U, arena, 0.1F, {}, perception);
    expect(first.friendlyFireHolds == 1U && shots.empty()
            && manager.findData<EnemyData>(guard)->magazineAmmo == 8U
            && std::abs(manager.find(guard)->transform.position.x) > 0.01F,
        "blocked shooter holds fire without spending ammo and sidesteps");
    for (int tick = 0; tick < 120 && shots.empty(); ++tick) {
        (void)updateGuards(manager, player, 1U, arena,
            1.0F / 60.0F, {}, perception);
    }
    expect(!shots.empty() && shots.front().source == guard,
        "shooter eventually finds a clear lane rather than holding forever");
}

void testRangedEnemyFiresDelayedProjectile()
{
    using namespace gdp::gameplay;
    using namespace gdp::entities;
    const CollisionWorld arena{{{-10.0F, 0.0F, -10.0F},
        {10.0F, 4.0F, 10.0F}}};
    Player player;
    player.spawn({0.0F, 0.0F, 0.0F});
    EntityManager manager;
    const EntityId guard = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 6.0F},
            .rotationDegrees = {0.0F, 180.0F, 0.0F}},
        .roomId = 1U,
        .archetype = EnemyArchetype::Guard,
    });
    std::vector<EnemyProjectile> spawned;
    const EnemyPerception perception{.projectiles = &spawned};
    for (int tick = 0; tick < 60 && spawned.empty(); ++tick) {
        (void)updateGuards(manager, player, 1U, arena, 1.0F / 60.0F,
            {}, perception);
    }
    expect(!spawned.empty() && player.state().health == 100.0F
            && manager.findData<EnemyData>(guard)->magazineAmmo == 7U,
        "ranged enemy spends ammunition and does not apply instant damage");
    EnemyProjectileSystem projectiles;
    projectiles.spawn(spawned);
    bool hit = false;
    for (int tick = 0; tick < 120 && !hit; ++tick) {
        for (const auto& event : projectiles.update(1.0F / 60.0F,
                 player, arena, manager)) {
            hit |= event.hitPlayer;
        }
    }
    expect(hit && player.state().health < 100.0F,
        "enemy shot damages only after traveling to the player");
}

void testBossProjectileBreaksCover()
{
    using namespace gdp::gameplay;
    using namespace gdp::entities;
    const CollisionWorld room{{{-8.0F, 0.0F, -8.0F},
        {8.0F, 4.0F, 8.0F}}};
    Player player;
    player.spawn({0.0F, 0.0F, 0.0F});
    EntityManager manager;
    const EntityId boss = manager.createEnemy({
        .transform = {.position = {4.0F, 0.0F, 4.0F}},
        .roomId = 1U, .archetype = EnemyArchetype::IndustrialRobot});
    const EntityId guard = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 4.0F}},
        .roomId = 1U, .archetype = EnemyArchetype::Guard});
    const EntityId crate = manager.createProp({
        .transform = {.position = {0.0F, 0.0F, 2.0F}},
        .roomId = 1U, .type = PropType::Crate, .solid = true,
        .health = 10.0F, .maximumHealth = 10.0F,
        .dropType = ItemType::Ammo, .dropQuantity = 1U});
    const auto fire = [&](const EntityId source, const CollisionWorld& world) {
        const EnemyProjectile shot{{0.0F, 1.0F, 4.0F},
            {0.0F, 0.0F, -20.0F}, source, 1U, 6.0F, 1.0F,
            false, false, 4.0F};
        EnemyProjectileSystem projectiles;
        projectiles.spawn(std::span{&shot, 1U});
        return projectiles.update(0.25F, player, world, manager);
    };
    const auto guardImpact = fire(guard, room);
    expect(guardImpact.size() == 1U && guardImpact[0].hitProp == crate
            && manager.findData<PropData>(crate)->health == 10.0F,
        "ordinary enemy bullets stop at cover without breaking it");
    manager.find(guard)->transform.position.x = 4.0F;
    manager.find(boss)->transform.position.x = 0.0F;
    const EntityId ally = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 3.3F}},
        .roomId = 1U, .archetype = EnemyArchetype::Worker});
    const auto allyImpact = fire(boss, room);
    expect(allyImpact.size() == 1U && allyImpact[0].hitEnemy == ally
            && manager.findData<PropData>(crate)->health == 10.0F,
        "an ally between Boss and cover takes the physical projectile first");
    manager.find(ally)->transform.position.x = 4.0F;
    const CollisionWorld wallBeforeCrate{{{-8.0F, 0.0F, -8.0F},
        {8.0F, 4.0F, 8.0F}},
        {{{-1.0F, 0.0F, 3.0F}, {1.0F, 2.0F, 3.2F}}}};
    const auto wallImpact = fire(boss, wallBeforeCrate);
    expect(wallImpact.size() == 1U && wallImpact[0].blocked
            && !wallImpact[0].hitProp.isValid()
            && manager.findData<PropData>(crate)->health == 10.0F,
        "a wall protects the crate from the Boss projectile");
    const auto first = fire(boss, room);
    const auto second = fire(boss, room);
    expect(first.size() == 1U && first[0].hitProp == crate
            && !first[0].destroyedProp
            && second.size() == 1U && second[0].hitProp == crate
            && second[0].destroyedProp && second[0].spawnedDrop.isValid()
            && manager.findData<PropData>(crate)->state == PropState::Destroyed,
        "two Boss bullets damage then destroy the crate and produce one drop");
    const auto afterDestruction = fire(boss, room);
    expect(afterDestruction.size() == 1U && afterDestruction[0].hitPlayer
            && manager.query(EntityKind::Item).size() == 1U,
        "the next bullet passes destroyed cover without duplicating its drop");
    const EntityId terminal = manager.createProp({
        .transform = {.position = {0.0F, 0.0F, 2.0F}},
        .roomId = 1U, .type = PropType::Terminal, .solid = true,
        .maximumHealth = 0.0F});
    const auto protectedImpact = fire(boss, room);
    expect(protectedImpact.size() == 1U
            && protectedImpact[0].hitProp == terminal
            && !protectedImpact[0].destroyedProp
            && manager.findData<PropData>(terminal)->state != PropState::Destroyed,
        "protected terminal absorbs Boss bullets without being destroyed");

    EntityManager adjacent;
    const EntityId distantBoss = adjacent.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 8.0F}},
        .roomId = 2U, .archetype = EnemyArchetype::IndustrialRobot});
    const EntityId distantCrate = adjacent.createProp({
        .transform = {.position = {0.0F, 0.0F, 6.0F}},
        .roomId = 2U, .type = PropType::Crate, .solid = true,
        .health = 10.0F, .maximumHealth = 10.0F});
    const CollisionWorld openRooms = CollisionWorld::fromWalkableVolumes({
        {{-8.0F, 0.0F, -4.0F}, {8.0F, 4.0F, 4.0F}},
        {{-8.0F, 0.0F, 4.0F}, {8.0F, 4.0F, 10.0F}}});
    const EnemyProjectile acrossDoor{{0.0F, 1.0F, 8.0F},
        {0.0F, 0.0F, -20.0F}, distantBoss, 2U, 6.0F, 1.0F,
        false, false, 8.0F};
    EnemyProjectileSystem across;
    across.spawn(std::span{&acrossDoor, 1U});
    const auto openImpact = across.update(0.25F, player, openRooms, adjacent);
    expect(openImpact.size() == 1U && openImpact[0].hitProp == distantCrate
            && adjacent.findData<PropData>(distantCrate)->health == 4.0F,
        "Boss bullet can damage cover in its adjacent source room");
    const CollisionWorld shutDoor = CollisionWorld::fromWalkableVolumes({
        {{-8.0F, 0.0F, -4.0F}, {8.0F, 4.0F, 4.0F}},
        {{-8.0F, 0.0F, 4.0F}, {8.0F, 4.0F, 10.0F}}},
        {{{-1.0F, 0.0F, 7.0F}, {1.0F, 3.0F, 7.2F}}});
    across.spawn(std::span{&acrossDoor, 1U});
    const auto closedImpact = across.update(0.25F, player, shutDoor, adjacent);
    expect(closedImpact.size() == 1U && closedImpact[0].blocked
            && !closedImpact[0].hitProp.isValid()
            && adjacent.findData<PropData>(distantCrate)->health == 4.0F,
        "closed door stops a Boss bullet before adjacent-room cover");
}

void testReferenceProjectileBodiesAndGrenade()
{
    using namespace gdp::gameplay;
    using namespace gdp::entities;
    ReferenceModelsForTest preview;
    const CollisionWorld arena{{{-10.0F, 0.0F, -10.0F},
        {10.0F, 5.0F, 10.0F}}};
    Player player;
    player.spawn({5.0F, 0.0F, 5.0F});
    EntityManager manager;
    const EntityId shooter = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 4.0F}},
        .roomId = 1U, .archetype = EnemyArchetype::Guard});
    const EntityId boss = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 2.0F}},
        .roomId = 1U, .archetype = EnemyArchetype::IndustrialRobot,
        .health = 1000.0F});
    const EnemyProjectile headShot{{0.0F, 2.05F, 4.0F},
        {0.0F, 0.0F, -20.0F}, shooter, 1U, 8.0F, 0.8F,
        false, false, 4.0F};
    expect(allyInEnemyFireLane(manager, shooter, headShot.position,
            headShot.velocity, 4.0F),
        "visible Boss head blocks an ally's firing lane");
    EnemyProjectileSystem shots;
    shots.spawn(std::span{&headShot, 1U});
    std::vector<EnemyProjectileEvent> events;
    for (int tick = 0; tick < 30 && events.empty(); ++tick)
        events = shots.update(1.0F / 60.0F, player, arena, manager);
    expect(events.size() == 1U && events[0].hitEnemy == boss
            && manager.findData<EnemyData>(boss)->health == 992.0F,
        "enemy projectile hits Boss head above legacy collider height");

    const EnemyProjectile gearMiss{{0.0F, 2.36F, 4.0F},
        {0.0F, 0.0F, -20.0F}, shooter, 1U, 8.0F, 0.8F,
        false, false, 4.0F};
    expect(!allyInEnemyFireLane(manager, shooter, gearMiss.position,
            gearMiss.velocity, 4.0F),
        "rear equipment above Boss head does not block a firing lane");
    shots.spawn(std::span{&gearMiss, 1U});
    for (int tick = 0; tick < 30; ++tick)
        (void)shots.update(1.0F / 60.0F, player, arena, manager);
    expect(manager.findData<EnemyData>(boss)->health == 992.0F,
        "enemy projectile through upper rear equipment does not deal damage");

    manager.find(boss)->transform.rotationDegrees.y = 90.0F;
    const EnemyProjectile sideHead{{3.0F, 2.05F, 2.0F},
        {-20.0F, 0.0F, 0.0F}, shooter, 1U, 8.0F, 0.8F,
        false, false, 4.0F};
    shots.spawn(std::span{&sideHead, 1U});
    events.clear();
    for (int tick = 0; tick < 30 && events.empty(); ++tick)
        events = shots.update(1.0F / 60.0F, player, arena, manager);
    expect(events.size() == 1U && events[0].hitEnemy == boss,
        "projectile collision follows Boss head when yaw is 90 degrees");

    manager.find(boss)->transform.rotationDegrees.y = 0.0F;
    manager.find(boss)->transform.position = {0.0F, 0.0F, 1.0F};
    PlayerGrenadeSystem grenades;
    expect(grenades.throwGrenade({0.0F, 2.05F, 0.0F},
            {0.0F, 0.0F, 1.0F}, grenadeMaximumChargeSeconds,
            1U, arena), "grenade can be thrown toward Boss visible head");
    (void)grenades.update(0.05F, manager);
    expect(!grenades.grenades().empty()
            && grenades.grenades().front().velocity.z < 0.0F,
        "thrown grenade bounces off the visible Boss head");

    Player blastTarget;
    blastTarget.spawn({5.0F, 0.0F, 5.0F});
    const float beforeBlast = manager.findData<EnemyData>(boss)->health;
    const auto blast = resolveGrenadeBlast(
        {{0.0F, 1.95F, 0.0F}, 1U, arena, {}}, blastTarget, manager);
    expect(!blast.hits.empty()
            && manager.findData<EnemyData>(boss)->health < beforeBlast,
        "grenade explosion damages visible Boss body above legacy height");
}

void testReferenceMuzzlesAndMeleeHands()
{
    using namespace gdp::gameplay;
    using namespace gdp::entities;
    ReferenceModelsForTest preview;
    const CollisionWorld arena{{{-20.0F, 0.0F, -20.0F},
        {20.0F, 5.0F, 20.0F}}};
    struct MuzzleCase {
        EnemyArchetype role;
        wkc::core::Vec3 local;
        float distance;
    };
    constexpr std::array<MuzzleCase, 4> muzzles{{
        {EnemyArchetype::Guard, {-.176F, 1.403F, 1.140F}, 6.0F},
        {EnemyArchetype::Officer, {-.431F, 1.267F, .889F}, 8.0F},
        {EnemyArchetype::Engineer, {-.404F, 1.329F, 1.063F}, 5.0F},
        {EnemyArchetype::IndustrialRobot, {-.679F, .998F, .699F}, 10.0F},
    }};
    for (const auto muzzle : muzzles) {
        for (const float yaw : {0.0F, 90.0F, 180.0F}) {
            EntityManager manager;
            Player player;
            const float radians = yaw * 0.01745329251994329577F;
            player.spawn({std::sin(radians) * muzzle.distance, 0.0F,
                std::cos(radians) * muzzle.distance});
            const EntityId id = manager.createEnemy({
                .transform = {.position = {0.0F, 0.0F, 0.0F},
                    .rotationDegrees = {0.0F, yaw, 0.0F}},
                .roomId = 1U, .archetype = muzzle.role,
                .health = 1000.0F});
            std::vector<EnemyProjectile> projectiles;
            const EnemyPerception perception{.projectiles = &projectiles};
            for (int tick = 0; tick < 150 && projectiles.empty(); ++tick) {
                switch (muzzle.role) {
                case EnemyArchetype::Guard:
                    (void)updateGuards(manager, player, 1U, arena,
                        1.0F / 60.0F, {}, perception);
                    break;
                case EnemyArchetype::Officer:
                    (void)updateOfficers(manager, player, 1U, arena,
                        1.0F / 60.0F, {}, perception);
                    break;
                case EnemyArchetype::Engineer:
                    (void)updateEngineers(manager, player, 1U, arena,
                        1.0F / 60.0F, {}, perception);
                    break;
                case EnemyArchetype::IndustrialRobot:
                    (void)updateIndustrialRobots(manager, player, 1U, arena,
                        1.0F / 60.0F, {}, perception);
                    break;
                case EnemyArchetype::Worker: break;
                }
            }
            expect(projectiles.size() == 1U,
                "each armed enemy fires a projectile in the reference preview");
            if (projectiles.empty()) continue;
            const auto& transform = manager.find(id)->transform;
            const float finalYaw = transform.rotationDegrees.y
                * 0.01745329251994329577F;
            const auto expected = transform.position + wkc::core::Vec3{
                std::cos(finalYaw) * muzzle.local.x
                    + std::sin(finalYaw) * muzzle.local.z,
                muzzle.local.y,
                -std::sin(finalYaw) * muzzle.local.x
                    + std::cos(finalYaw) * muzzle.local.z};
            expect(wkc::core::length(projectiles.front().position - expected)
                    < 0.001F
                    && manager.findData<EnemyData>(id)->lastAttackKind == 1U,
                "ranged shot starts at its weapon socket after yaw rotation");
        }
    }

    EntityManager bossManager;
    Player nearby;
    nearby.spawn({0.0F, 0.0F, 1.8F});
    const EntityId boss = bossManager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 0.0F}},
        .roomId = 1U, .archetype = EnemyArchetype::IndustrialRobot,
        .health = 1000.0F});
    bossManager.findData<EnemyData>(boss)->health = 200.0F;
    std::vector<EnemyProjectile> shots;
    IndustrialRobotConfig closeConfig;
    closeConfig.cannonReadySeconds = 0.0F;
    (void)updateIndustrialRobots(bossManager, nearby, 1U, arena,
        0.1F, closeConfig, {.projectiles = &shots});
    expect(nearby.state().health < 100.0F && shots.empty()
            && bossManager.findData<EnemyData>(boss)->lastAttackKind == 2U,
        "Boss in Pursuit strikes with its hand at close range");

    EntityManager workerManager;
    Player victim;
    victim.spawn({0.0F, 0.0F, 0.0F});
    const EntityId worker = workerManager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 1.0F},
            .rotationDegrees = {0.0F, 180.0F, 0.0F}},
        .roomId = 1U, .archetype = EnemyArchetype::Worker});
    (void)updateWorkers(workerManager, victim, 1U, arena, 0.1F);
    expect(victim.state().health < 100.0F
            && workerManager.findData<EnemyData>(worker)->lastAttackKind == 2U,
        "Worker contact attack is marked as a hand strike");
}

// Manual profile only. Both simulations use identical geometry and tick counts;
// the single noise stimulus is the only difference at their starting state.
void profileGrenadeAlert()
{
    using namespace gdp::gameplay;
    using namespace gdp::entities;
    using Clock = std::chrono::steady_clock;

    constexpr float step = 1.0F / 60.0F;
    constexpr int ticks = 900;
    constexpr int repetitions = 4; // One warmup, three measured runs.
    constexpr std::array<std::string_view, 4> phaseNames{
        "alert", "investigate", "recover", "post"};
    struct Sample {
        double baselineMicroseconds{0.0};
        double alertedMicroseconds{0.0};
        std::size_t ticks{0U};
        std::size_t alerting{0U};
        std::size_t investigating{0U};
        std::size_t recovering{0U};
    };
    std::array<Sample, phaseNames.size()> totals{};
    double noiseCallMicroseconds = 0.0;
    std::size_t totalAlerted = 0U;
    std::uint64_t baselineTrajectory = 14695981039346656037ULL;
    std::uint64_t alertedTrajectory = 14695981039346656037ULL;

    const auto recordTrajectory = [](const EntityManager& manager,
                                      std::uint64_t& hash) {
        const auto mix = [&hash](const std::uint64_t value) {
            hash = (hash ^ value) * 1099511628211ULL;
        };
        for (const EntityId id : manager.query(EntityKind::Enemy)) {
            const Entity* entity = manager.find(id);
            const EnemyData* enemy = manager.findData<EnemyData>(id);
            mix(std::bit_cast<std::uint32_t>(entity->transform.position.x));
            mix(std::bit_cast<std::uint32_t>(entity->transform.position.y));
            mix(std::bit_cast<std::uint32_t>(entity->transform.position.z));
            mix(std::bit_cast<std::uint32_t>(entity->transform.rotationDegrees.y));
            mix(static_cast<std::uint64_t>(enemy->behavior));
            mix(std::bit_cast<std::uint32_t>(enemy->alertTime));
            mix(std::bit_cast<std::uint32_t>(enemy->investigationTime));
            mix(std::bit_cast<std::uint32_t>(enemy->recoverTime));
        }
    };

    const CollisionWorld room{
        {{-12.0F, 0.0F, -12.0F}, {12.0F, 4.0F, 12.0F}},
        {{{-10.8F, 0.0F, -0.5F}, {10.8F, 3.0F, 0.5F}}}};
    const std::array<wkc::core::Vec3, 8> starts{{
        {-4.0F, 0.0F, 5.0F}, {-3.0F, 0.0F, 6.0F},
        {-2.0F, 0.0F, 5.0F}, {-1.0F, 0.0F, 6.0F},
        {1.0F, 0.0F, 5.0F}, {2.0F, 0.0F, 6.0F},
        {3.0F, 0.0F, 5.0F}, {4.0F, 0.0F, 6.0F},
    }};
    const auto populate = [&starts](EntityManager& manager) {
        for (std::size_t index = 0U; index < starts.size(); ++index) {
            (void)manager.createEnemy({
                .transform = {.position = starts[index],
                    .rotationDegrees = {0.0F, 180.0F, 0.0F}},
                .roomId = 1U,
                .archetype = EnemyArchetype::Guard,
                .visualSeed = 20261012U + index,
            });
        }
    };

    for (int repetition = 0; repetition < repetitions; ++repetition) {
        EntityManager baseline;
        EntityManager alerted;
        Player baselinePlayer;
        Player alertedPlayer;
        baselinePlayer.spawn({0.0F, 0.0F, -8.0F});
        alertedPlayer.spawn({0.0F, 0.0F, -8.0F});
        populate(baseline);
        populate(alerted);

        const auto noiseStart = Clock::now();
        const std::size_t alertedCount = alertEnemiesToNoise(alerted, 1U,
            {.position = {0.0F, 0.0F, -2.0F}, .radius = 20.0F});
        const auto noiseEnd = Clock::now();
        if (repetition > 0) {
            totalAlerted += alertedCount;
            noiseCallMicroseconds += std::chrono::duration<double, std::micro>(
                noiseEnd - noiseStart).count();
        }

        for (int tick = 0; tick < ticks; ++tick) {
            const auto baselineStart = Clock::now();
            (void)updateGuards(baseline, baselinePlayer, 1U, room, step,
                {}, {.allowAttacks = false});
            const auto baselineEnd = Clock::now();
            const auto alertedStart = Clock::now();
            const EnemyUpdateSummary summary = updateGuards(
                alerted, alertedPlayer, 1U, room, step,
                {}, {.allowAttacks = false});
            const auto alertedEnd = Clock::now();
            if (repetition == repetitions - 1) {
                recordTrajectory(baseline, baselineTrajectory);
                recordTrajectory(alerted, alertedTrajectory);
            }
            if (repetition == 0) continue;

            const std::size_t phase = summary.alerting > 0U ? 0U
                : summary.investigating > 0U ? 1U
                : summary.recovering > 0U ? 2U : 3U;
            Sample& sample = totals[phase];
            sample.baselineMicroseconds += std::chrono::duration<double,
                std::micro>(baselineEnd - baselineStart).count();
            sample.alertedMicroseconds += std::chrono::duration<double,
                std::micro>(alertedEnd - alertedStart).count();
            ++sample.ticks;
            sample.alerting += summary.alerting;
            sample.investigating += summary.investigating;
            sample.recovering += summary.recovering;
        }
    }

    std::cout << std::fixed << std::setprecision(2)
              << "grenade-alert-profile guards=" << starts.size()
              << " ticks=" << ticks << " hz=60 measured_repetitions="
              << repetitions - 1 << " alerted_per_run="
              << totalAlerted / (repetitions - 1)
              << " noise_call_us="
              << noiseCallMicroseconds / (repetitions - 1)
              << " baseline_trajectory=" << baselineTrajectory
              << " alerted_trajectory=" << alertedTrajectory << '\n';
    for (std::size_t phase = 0U; phase < totals.size(); ++phase) {
        const Sample& sample = totals[phase];
        if (sample.ticks == 0U) continue;
        const double baselinePerTick = sample.baselineMicroseconds
            / static_cast<double>(sample.ticks);
        const double alertedPerTick = sample.alertedMicroseconds
            / static_cast<double>(sample.ticks);
        std::cout << phaseNames[phase]
                  << " ticks_per_run=" << sample.ticks / (repetitions - 1)
                  << " baseline_us_tick=" << baselinePerTick
                  << " alerted_us_tick=" << alertedPerTick
                  << " delta_us_tick=" << alertedPerTick - baselinePerTick
                  << " alerting_enemy_ticks=" << sample.alerting
                  << " investigating_enemy_ticks=" << sample.investigating
                  << " recovering_enemy_ticks=" << sample.recovering << '\n';
    }
}

} // namespace

int main(const int argc, char* argv[])
{
    if (argc == 2 && std::string_view{argv[1]} == "--grenade-alert-profile") {
        profileGrenadeAlert();
        return 0;
    }
    if (argc != 1) {
        std::cerr << "Unknown argument\n";
        return 2;
    }
    testSensors();
    testCooperativeSensorsAndMemory();
    testCooperativeFireLaneMatchesShot();
    testTacticalMovement();
    testExperimentalWorker();
    testNeuralWorkerRemembersVisiblePlayer();
    testExperimentalArchetypes();
    testNeuralLocomotionFacing();
    testNeuralRangedAim();
    testNeuralBossNavigatesCover();
    testNeuralBrainControlsEveryEnemyIndependently();
    testActiveWorkerMaintainsMeleeThreat();
    testEmptyRangedBrainYieldsToScriptedRecovery();
    testEnemyProjectiles();
    testEnemyFriendlyFire();
    testDifficultyAndFireDiscipline();
    testRangedEnemyFiresDelayedProjectile();
    testBossProjectileBreaksCover();
    testReferenceProjectileBodiesAndGrenade();
    testReferenceMuzzlesAndMeleeHands();
    return failures == 0 ? 0 : 1;
}
