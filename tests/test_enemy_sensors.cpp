#include "gameplay/Enemy.hpp"
#include "gameplay/EnemySensors.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
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
        expect(experiment.selected == id && experiment.controlledTicks == 1U
                && update.attacksLanded == 1U
                && player.state().health < 100.0F,
            "each non-Worker archetype can execute its neural attack");
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

} // namespace

int main()
{
    testSensors();
    testTacticalMovement();
    testExperimentalWorker();
    testExperimentalArchetypes();
    testNeuralLocomotionFacing();
    testNeuralRangedAim();
    testNeuralBossNavigatesCover();
    testNeuralBrainControlsEveryEnemyIndependently();
    testActiveWorkerMaintainsMeleeThreat();
    return failures == 0 ? 0 : 1;
}
