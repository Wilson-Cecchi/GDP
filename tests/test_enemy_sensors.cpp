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
            && hiddenUpdate.attacksLanded == 0
            && hiddenPlayer.state().health == 100.0F,
        "neural command cannot attack through blocked line of sight");

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

} // namespace

int main()
{
    testSensors();
    testExperimentalWorker();
    testExperimentalArchetypes();
    return failures == 0 ? 0 : 1;
}
