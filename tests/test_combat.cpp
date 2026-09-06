#include "core/Math.hpp"
#include "entities/EntityManager.hpp"
#include "gameplay/Combat.hpp"
#include "gameplay/Enemy.hpp"
#include "gameplay/Player.hpp"
#include "gameplay/Weapon.hpp"

#include <cmath>
#include <iostream>
#include <limits>
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

bool near(const float first, const float second) noexcept
{
    return std::abs(first - second) < 1.0e-5F;
}

void testEnemyDifficultyConfigs()
{
    using namespace gdp::gameplay;
    const EnemyDifficultyConfigs baseline = makeEnemyDifficultyConfigs(1.0F);
    expect(near(baseline.worker.moveSpeed, WorkerConfig{}.moveSpeed)
            && near(baseline.guard.attackDamage, GuardConfig{}.attackDamage)
            && near(baseline.industrialRobot.pursuitDamage,
                IndustrialRobotConfig{}.pursuitDamage),
        "neutral threat keeps every original enemy tuning value");

    const EnemyDifficultyConfigs harder = makeEnemyDifficultyConfigs(1.2F);
    expect(near(harder.worker.moveSpeed, WorkerConfig{}.moveSpeed * 1.2F)
            && near(harder.worker.attackDamage, WorkerConfig{}.attackDamage * 1.2F)
            && near(harder.officer.moveSpeed, OfficerConfig{}.moveSpeed * 1.2F)
            && near(harder.engineer.attackDamage,
                EngineerConfig{}.attackDamage * 1.2F)
            && near(harder.industrialRobot.longRangeDamage,
                IndustrialRobotConfig{}.longRangeDamage * 1.2F),
        "higher threat scales enemy mobility and damage consistently");
    expect(near(harder.worker.attackInterval, WorkerConfig{}.attackInterval)
            && near(harder.officer.supportInterval, OfficerConfig{}.supportInterval)
            && near(harder.engineer.repairAmount, EngineerConfig{}.repairAmount),
        "threat scaling leaves cadence and support mechanics readable");

    const EnemyDifficultyConfigs invalid = makeEnemyDifficultyConfigs(
        std::numeric_limits<float>::quiet_NaN());
    expect(near(invalid.worker.attackDamage, WorkerConfig{}.attackDamage),
        "non-finite threat falls back to neutral tuning");
}

void testWeaponLifecycle()
{
    using namespace gdp::gameplay;

    Weapon weapon({
        .damage = 20.0F,
        .fireInterval = 0.5F,
        .range = 15.0F,
        .magazineSize = 2,
        .reserveMaximum = 3,
    });
    expect(weapon.tryFire() == FireReadiness::Fired && weapon.magazineAmmo() == 1,
        "weapon fires and consumes exactly one cartridge");
    expect(weapon.tryFire() == FireReadiness::Cooldown && weapon.magazineAmmo() == 1,
        "cooldown rejects fire without consuming ammunition");
    weapon.fixedUpdate(0.5F);
    expect(weapon.tryFire() == FireReadiness::Fired && weapon.magazineAmmo() == 0,
        "weapon fires again after its fixed-time cooldown");
    expect(weapon.reload() && weapon.magazineAmmo() == 2 && weapon.reserveAmmo() == 1,
        "reload transfers only the ammunition needed by the magazine");
    expect(!weapon.reload(), "full magazine rejects a redundant reload");

    Weapon empty({.fireInterval = 0.0F, .magazineSize = 1, .reserveMaximum = 0});
    expect(empty.tryFire() == FireReadiness::Fired, "last cartridge can fire");
    expect(empty.tryFire() == FireReadiness::Empty,
        "empty weapon rejects fire deterministically");
}

void testDamageLifecycle()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    EntityManager manager;
    const EntityId enemy = manager.createEnemy({.health = 50.0F});
    expect(applyDamage(manager, enemy, 20.0F) == DamageOutcome::Damaged,
        "non-lethal damage reports a live damaged enemy");
    expect(manager.findData<EnemyData>(enemy)->health == 30.0F,
        "damage subtracts from enemy HP");
    expect(applyDamage(manager, enemy, 100.0F) == DamageOutcome::Killed,
        "overkill clamps HP and produces one kill");
    const EnemyData* data = manager.findData<EnemyData>(enemy);
    expect(data->health == 0.0F && data->behavior == EnemyBehaviorState::Dead,
        "lethal damage enters the terminal Dead state");
    expect(applyDamage(manager, enemy, 1.0F) == DamageOutcome::Ignored,
        "dead enemy ignores repeated damage and cannot die twice");
    expect(applyDamage(manager, {}, 10.0F) == DamageOutcome::Ignored,
        "invalid target safely ignores damage");
}

void testDormantEnemyIgnoresCombat()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    const CollisionWorld openRoom{{{-10.0F, 0.0F, -10.0F}, {10.0F, 4.0F, 10.0F}}};
    EntityManager manager;
    const EntityId dormant = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, -3.0F}},
        .roomId = 1,
        .active = false,
    });
    expect(applyDamage(manager, dormant, 500.0F) == DamageOutcome::Ignored,
        "dormant preview enemy ignores direct damage before event activation");
    Weapon weapon({.fireInterval = 0.0F, .range = 20.0F});
    const ShotResult shot = fireHitscan(
        weapon, {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, -1.0F},
        1, openRoom, manager);
    expect(shot.outcome == ShotOutcome::ImpactWorld && !shot.hit
            && manager.findData<EnemyData>(dormant)->health == 50.0F,
        "hitscan passes over dormant event previews without changing their health");
    Player player;
    player.spawn({0.0F, 0.0F, 0.0F});
    const wkc::core::Vec3 dormantPosition = manager.find(dormant)->transform.position;
    const EnemyUpdateSummary update
        = updateWorkers(manager, player, 1, openRoom, 1.0F);
    expect(update.idle == 0U && update.chasing == 0U && update.attacking == 0U
            && manager.find(dormant)->transform.position == dormantPosition,
        "dormant preview enemy runs no AI and remains fixed before activation");
}

void testHitscanSelectionAndBlocking()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    const CollisionWorld openRoom{{{-10.0F, 0.0F, -10.0F}, {10.0F, 4.0F, 10.0F}}};
    EntityManager manager;
    const EntityId nearest = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, -3.0F}},
        .roomId = 1,
        .health = 60.0F,
    });
    const EntityId farther = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, -6.0F}},
        .roomId = 1,
        .health = 60.0F,
    });
    (void)manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, -2.0F}},
        .roomId = 2,
        .health = 60.0F,
    });
    Weapon weapon({.damage = 35.0F, .fireInterval = 0.1F, .range = 20.0F});

    const ShotResult first = fireHitscan(
        weapon, {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, -1.0F}, 1, openRoom, manager);
    expect(first.outcome == ShotOutcome::Hit && first.hit && first.hit->target == nearest,
        "hitscan selects the nearest live enemy in the current room");
    expect(manager.findData<EnemyData>(nearest)->health == 25.0F
            && manager.findData<EnemyData>(farther)->health == 60.0F,
        "hitscan damages only the selected target");
    weapon.fixedUpdate(0.1F);
    const ShotResult lethal = fireHitscan(
        weapon, {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, -1.0F}, 1, openRoom, manager);
    expect(lethal.outcome == ShotOutcome::Killed && lethal.hit->target == nearest,
        "lethal hitscan reports the killed target");
    weapon.fixedUpdate(0.1F);
    const ShotResult afterDeath = fireHitscan(
        weapon, {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, -1.0F}, 1, openRoom, manager);
    expect(afterDeath.outcome == ShotOutcome::Hit && afterDeath.hit->target == farther,
        "dead target no longer blocks the next hitscan");

    EntityManager blockedManager;
    const EntityId blockedEnemy = blockedManager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, -3.0F}},
        .roomId = 1,
    });
    const CollisionWorld blockedRoom{
        {{-10.0F, 0.0F, -10.0F}, {10.0F, 4.0F, 10.0F}},
        {{{-1.0F, 0.0F, -2.0F}, {1.0F, 3.0F, -1.5F}}},
    };
    Weapon blockedWeapon({.fireInterval = 0.0F, .range = 20.0F});
    const ShotResult blocked = fireHitscan(
        blockedWeapon,
        {0.0F, 1.0F, 0.0F},
        {0.0F, 0.0F, -1.0F},
        1,
        blockedRoom,
        blockedManager);
    expect(blocked.outcome == ShotOutcome::ImpactWorld
            && blockedManager.findData<EnemyData>(blockedEnemy)->health == 50.0F,
        "solid geometry stops a shot before the enemy");

    Weapon shortRange({.fireInterval = 0.0F, .range = 1.0F});
    const ShotResult miss = fireHitscan(
        shortRange, {0.0F, 1.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, 1, openRoom, manager);
    expect(miss.outcome == ShotOutcome::Miss,
        "shot with no target or world impact inside its range reports a miss");
}

void testWalkableUnionBlocksShotsAcrossDisconnectedSpace()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    const CollisionWorld disconnected = CollisionWorld::fromWalkableVolumes({
        {{-2.0F, 0.0F, -2.0F}, {2.0F, 4.0F, 2.0F}},
        {{5.0F, 0.0F, -2.0F}, {9.0F, 4.0F, 2.0F}},
    });
    EntityManager manager;
    const EntityId enemy = manager.createEnemy({
        .transform = {.position = {6.0F, 0.0F, 0.0F}},
        .roomId = 1,
        .health = 50.0F,
    });
    Weapon weapon({.fireInterval = 0.0F, .range = 20.0F});
    const ShotResult shot = fireHitscan(
        weapon, {0.0F, 1.0F, 0.0F}, {1.0F, 0.0F, 0.0F},
        1, disconnected, manager);
    expect(shot.outcome == ShotOutcome::ImpactWorld
            && manager.findData<EnemyData>(enemy)->health == 50.0F,
        "hitscan stops at the first gap in a walkable-volume union");
    expect(!hasLineOfSight(
            {0.0F, 1.0F, 0.0F}, {6.0F, 1.0F, 0.0F}, disconnected),
        "line of sight cannot cross disconnected navigable volumes");
}

void testWorkerBehavior()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    const CollisionWorld room{{{-10.0F, 0.0F, -10.0F}, {10.0F, 4.0F, 10.0F}}};
    Player player;
    player.spawn({0.0F, 0.0F, 0.0F});
    EntityManager manager;
    const EntityId worker = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 5.0F}},
        .roomId = 1,
        .archetype = EnemyArchetype::Worker,
    });
    const WorkerConfig config{
        .detectionRange = 10.0F,
        .attackRange = 1.1F,
        .moveSpeed = 2.0F,
        .attackDamage = 10.0F,
        .attackInterval = 1.0F,
    };

    const EnemyUpdateSummary chase = updateWorkers(manager, player, 1, room, 1.0F, config);
    expect(chase.chasing == 1 && manager.find(worker)->transform.position.z == 3.0F,
        "detected Worker enters Chase and moves toward the player");
    (void)updateWorkers(manager, player, 1, room, 1.0F, config);
    const EnemyUpdateSummary attack = updateWorkers(manager, player, 1, room, 1.0F, config);
    expect(attack.attacking == 1 && attack.attacksLanded == 1
            && player.state().health == 90.0F,
        "Worker in range attacks and applies damage to the player");

    const wkc::core::Vec3 deathPosition = manager.find(worker)->transform.position;
    expect(applyDamage(manager, worker, 500.0F) == DamageOutcome::Killed,
        "Worker can be killed through the shared damage pipeline");
    const float healthBeforeDeadUpdate = player.state().health;
    const EnemyUpdateSummary dead = updateWorkers(manager, player, 1, room, 2.0F, config);
    expect(dead.chasing == 0 && dead.attacking == 0
            && manager.find(worker)->transform.position == deathPosition
            && player.state().health == healthBeforeDeadUpdate,
        "dead Worker no longer moves or attacks");
}

void testWorkerLineOfSight()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    Player player;
    player.spawn({0.0F, 0.0F, 0.0F});
    EntityManager manager;
    (void)manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 1.0F}},
        .roomId = 1,
        .archetype = EnemyArchetype::Worker,
    });
    const CollisionWorld blocked{
        {{-5.0F, 0.0F, -5.0F}, {5.0F, 4.0F, 5.0F}},
        {{{-0.8F, 0.0F, 0.35F}, {0.8F, 2.5F, 0.65F}}},
    };
    const EnemyUpdateSummary update = updateWorkers(
        manager,
        player,
        1,
        blocked,
        1.0F / 60.0F,
        {.detectionRange = 10.0F, .attackRange = 1.5F});
    expect(update.attacksLanded == 0 && player.state().health == 100.0F,
        "Worker cannot attack through blocking geometry");
}

void testGuardRangedBehavior()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    const CollisionWorld room{{{-12.0F, 0.0F, -12.0F}, {12.0F, 4.0F, 12.0F}}};
    Player player;
    player.spawn({0.0F, 0.0F, 0.0F});
    EntityManager manager;
    const EntityId guard = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 10.0F}},
        .roomId = 1,
        .archetype = EnemyArchetype::Guard,
    });
    const EntityId worker = manager.createEnemy({
        .transform = {.position = {2.0F, 0.0F, 5.0F}},
        .roomId = 1,
        .archetype = EnemyArchetype::Worker,
    });
    const GuardConfig config{
        .detectionRange = 20.0F,
        .preferredMinimumRange = 4.0F,
        .preferredMaximumRange = 8.0F,
        .moveSpeed = 2.0F,
        .attackDamage = 7.0F,
        .attackInterval = 1.0F,
    };

    const EnemyUpdateSummary approach = updateGuards(manager, player, 1, room, 1.0F, config);
    expect(approach.chasing == 1U
            && wkc::core::nearlyEqual(manager.find(guard)->transform.position.z, 8.0F),
        "Guard outside its firing envelope advances to medium range");
    expect(manager.find(worker)->transform.position == wkc::core::Vec3{2.0F, 0.0F, 5.0F},
        "Guard update never moves a Worker archetype");

    const EnemyUpdateSummary shot = updateGuards(manager, player, 1, room, 0.1F, config);
    expect(shot.attacking == 1U && shot.attacksLanded == 1U
            && wkc::core::nearlyEqual(player.state().health, 93.0F),
        "Guard with line of sight fires from its preferred range");

    manager.find(guard)->transform.position = {0.0F, 0.0F, 2.0F};
    const float healthBeforeRetreat = player.state().health;
    const EnemyUpdateSummary retreat = updateGuards(manager, player, 1, room, 0.5F, config);
    expect(retreat.repositioning == 1U
            && wkc::core::nearlyEqual(manager.find(guard)->transform.position.z, 3.0F)
            && player.state().health == healthBeforeRetreat,
        "Guard that is rushed retreats without firing inside its minimum range");

    const wkc::core::Vec3 deathPosition = manager.find(guard)->transform.position;
    expect(applyDamage(manager, guard, 500.0F) == DamageOutcome::Killed,
        "Guard uses the shared damage and death pipeline");
    const EnemyUpdateSummary dead = updateGuards(manager, player, 1, room, 2.0F, config);
    expect(dead.chasing == 0U && dead.repositioning == 0U && dead.attacking == 0U
            && manager.find(guard)->transform.position == deathPosition,
        "dead Guard cannot move, reposition or attack");
}

void testGuardLineOfSight()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    Player player;
    player.spawn({0.0F, 0.0F, 0.0F});
    EntityManager manager;
    (void)manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 6.0F}},
        .roomId = 1,
        .archetype = EnemyArchetype::Guard,
    });
    const CollisionWorld blocked{
        {{-8.0F, 0.0F, -8.0F}, {8.0F, 4.0F, 8.0F}},
        {{{-1.0F, 0.0F, 2.5F}, {1.0F, 3.0F, 3.0F}}},
    };
    const EnemyUpdateSummary update = updateGuards(
        manager,
        player,
        1,
        blocked,
        1.0F / 60.0F,
        {.detectionRange = 12.0F,
            .preferredMinimumRange = 4.0F,
            .preferredMaximumRange = 8.0F});
    expect(update.chasing == 1U && update.attacksLanded == 0U
            && player.state().health == 100.0F,
        "Guard cannot fire through geometry and instead seeks line of sight");
}

void testOfficerCommandSupport()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    const CollisionWorld room{{{-14.0F, 0.0F, -14.0F}, {14.0F, 4.0F, 14.0F}}};
    Player player;
    player.spawn({0.0F, 0.0F, 0.0F});
    EntityManager manager;
    const EntityId officer = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 8.0F}},
        .roomId = 1,
        .archetype = EnemyArchetype::Officer,
    });
    const EntityId worker = manager.createEnemy({
        .transform = {.position = {2.0F, 0.0F, 7.0F}},
        .roomId = 1,
        .archetype = EnemyArchetype::Worker,
    });
    const EntityId guard = manager.createEnemy({
        .transform = {.position = {-2.0F, 0.0F, 7.0F}},
        .roomId = 1,
        .archetype = EnemyArchetype::Guard,
    });
    const EntityId remoteWorker = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 7.0F}},
        .roomId = 2,
        .archetype = EnemyArchetype::Worker,
    });
    manager.findData<EnemyData>(worker)->attackCooldown = 1.0F;
    manager.findData<EnemyData>(guard)->attackCooldown = 0.8F;
    manager.findData<EnemyData>(remoteWorker)->attackCooldown = 1.0F;
    const OfficerConfig config{
        .detectionRange = 20.0F,
        .preferredMinimumRange = 6.0F,
        .preferredMaximumRange = 10.0F,
        .moveSpeed = 1.0F,
        .attackDamage = 0.0F,
        .attackInterval = 1.0F,
        .supportRange = 10.0F,
        .supportInterval = 2.5F,
        .allyCooldownReduction = 0.4F,
    };

    const EnemyUpdateSummary command = updateOfficers(manager, player, 1, room, 0.0F, config);
    expect(command.commandsIssued == 1U && command.alliesSupported == 2U
            && wkc::core::nearlyEqual(
                manager.findData<EnemyData>(worker)->attackCooldown, 0.6F)
            && wkc::core::nearlyEqual(
                manager.findData<EnemyData>(guard)->attackCooldown, 0.4F),
        "Officer command accelerates readying Worker and Guard allies in support range");
    expect(wkc::core::nearlyEqual(
               manager.findData<EnemyData>(remoteWorker)->attackCooldown, 1.0F),
        "Officer command never crosses the active room boundary");
    expect(wkc::core::nearlyEqual(
               manager.findData<EnemyData>(officer)->supportCooldown, 2.5F),
        "Officer command starts an independent support cooldown");

    const EnemyUpdateSummary repeated = updateOfficers(manager, player, 1, room, 0.1F, config);
    expect(repeated.commandsIssued == 0U && repeated.alliesSupported == 0U
            && wkc::core::nearlyEqual(
                manager.findData<EnemyData>(worker)->attackCooldown, 0.6F),
        "Officer cannot repeat a command before its support cooldown expires");
}

void testOfficerRangedBehaviorAndLineOfSight()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    Player player;
    player.spawn({0.0F, 0.0F, 0.0F});
    EntityManager manager;
    const EntityId officer = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 12.0F}},
        .roomId = 1,
        .archetype = EnemyArchetype::Officer,
    });
    const CollisionWorld open{{{-14.0F, 0.0F, -14.0F}, {14.0F, 4.0F, 14.0F}}};
    const OfficerConfig config{
        .detectionRange = 20.0F,
        .preferredMinimumRange = 6.0F,
        .preferredMaximumRange = 10.0F,
        .moveSpeed = 2.0F,
        .attackDamage = 6.0F,
        .attackInterval = 1.2F,
    };

    const EnemyUpdateSummary approach = updateOfficers(manager, player, 1, open, 1.0F, config);
    expect(approach.chasing == 1U
            && wkc::core::nearlyEqual(manager.find(officer)->transform.position.z, 10.0F),
        "Officer advances to its longer command-range firing envelope");
    const EnemyUpdateSummary shot = updateOfficers(manager, player, 1, open, 0.1F, config);
    expect(shot.attacking == 1U && shot.attacksLanded == 1U
            && wkc::core::nearlyEqual(player.state().health, 94.0F),
        "Officer fires its sidearm only from the preferred range with line of sight");

    manager.find(officer)->transform.position = {0.0F, 0.0F, 3.0F};
    const float healthBeforeRetreat = player.state().health;
    const EnemyUpdateSummary retreat = updateOfficers(manager, player, 1, open, 0.5F, config);
    expect(retreat.repositioning == 1U
            && wkc::core::nearlyEqual(manager.find(officer)->transform.position.z, 4.0F)
            && player.state().health == healthBeforeRetreat,
        "Officer retreats instead of firing when the player breaches minimum range");

    manager.find(officer)->transform.position = {0.0F, 0.0F, 8.0F};
    const CollisionWorld blocked{
        {{-14.0F, 0.0F, -14.0F}, {14.0F, 4.0F, 14.0F}},
        {{{-1.0F, 0.0F, 3.0F}, {1.0F, 3.0F, 3.5F}}},
    };
    const EnemyUpdateSummary occluded
        = updateOfficers(manager, player, 1, blocked, 0.1F, config);
    expect(occluded.chasing == 1U && occluded.attacksLanded == 0U,
        "Officer cannot fire through solid geometry and seeks line of sight");

    expect(applyDamage(manager, officer, 500.0F) == DamageOutcome::Killed,
        "Officer uses the shared damage and death pipeline");
    expect(wkc::core::nearlyEqual(
               manager.findData<EnemyData>(officer)->supportCooldown, 0.0F),
        "Officer death clears command state together with attack state");
}

void testEngineerRepairSystem()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    const CollisionWorld room{{{-12.0F, 0.0F, -12.0F}, {12.0F, 4.0F, 12.0F}}};
    Player player;
    player.spawn({0.0F, 0.0F, 0.0F});
    EntityManager manager;
    const EntityId engineer = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 5.0F}},
        .roomId = 1,
        .archetype = EnemyArchetype::Engineer,
    });
    const EntityId worker = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 4.0F}},
        .roomId = 1,
        .archetype = EnemyArchetype::Worker,
    });
    const EntityId remoteWorker = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 4.0F}},
        .roomId = 2,
        .archetype = EnemyArchetype::Worker,
    });
    const EntityId damagedProp = manager.createProp({
        .transform = {.position = {2.0F, 0.0F, 4.0F}},
        .roomId = 1,
        .type = PropType::Crate,
        .state = PropState::Damaged,
    });
    manager.findData<EnemyData>(worker)->health = 30.0F;
    manager.findData<EnemyData>(remoteWorker)->health = 30.0F;
    const EngineerConfig config{
        .detectionRange = 14.0F,
        .preferredMinimumRange = 3.0F,
        .preferredMaximumRange = 6.0F,
        .moveSpeed = 1.0F,
        .attackDamage = 0.0F,
        .attackInterval = 1.0F,
        .repairRange = 7.0F,
        .repairInterval = 2.0F,
        .repairAmount = 12.0F,
    };

    const EnemyUpdateSummary allyRepair
        = updateEngineers(manager, player, 1, room, 0.0F, config);
    expect(allyRepair.repairsPerformed == 1U && allyRepair.alliesRepaired == 1U
            && allyRepair.propsRestored == 0U
            && wkc::core::nearlyEqual(manager.findData<EnemyData>(worker)->health, 42.0F),
        "Engineer repairs the nearest damaged living ally by the configured amount");
    expect(wkc::core::nearlyEqual(
               manager.findData<EnemyData>(remoteWorker)->health, 30.0F),
        "Engineer repair never crosses the active room boundary");
    expect(manager.findData<PropData>(damagedProp)->state == PropState::Damaged
            && wkc::core::nearlyEqual(
                manager.findData<EnemyData>(engineer)->supportCooldown, 2.0F),
        "one Engineer repair starts an independent utility cooldown");

    const EnemyUpdateSummary coolingDown
        = updateEngineers(manager, player, 1, room, 0.5F, config);
    expect(coolingDown.repairsPerformed == 0U
            && manager.findData<PropData>(damagedProp)->state == PropState::Damaged,
        "Engineer cannot repair a second target before its utility cooldown expires");
    manager.findData<EnemyData>(worker)->health
        = manager.findData<EnemyData>(worker)->maximumHealth;
    const EnemyUpdateSummary propRepair
        = updateEngineers(manager, player, 1, room, 1.5F, config);
    expect(propRepair.repairsPerformed == 1U && propRepair.propsRestored == 1U
            && manager.findData<PropData>(damagedProp)->state == PropState::Intact,
        "Engineer restores a damaged prop when no closer ally requires repair");
}

void testEngineerRangedBehaviorAndRepairOcclusion()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    Player player;
    player.spawn({0.0F, 0.0F, 0.0F});
    EntityManager manager;
    const EntityId engineer = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 8.0F}},
        .roomId = 1,
        .archetype = EnemyArchetype::Engineer,
    });
    const EntityId damagedWorker = manager.createEnemy({
        .transform = {.position = {2.0F, 0.0F, 2.0F}},
        .roomId = 1,
        .archetype = EnemyArchetype::Worker,
    });
    manager.findData<EnemyData>(damagedWorker)->health = 20.0F;
    const CollisionWorld blocked{
        {{-12.0F, 0.0F, -12.0F}, {12.0F, 4.0F, 12.0F}},
        {{{-1.0F, 0.0F, 4.0F}, {3.0F, 3.0F, 4.5F}}},
    };
    const EngineerConfig config{
        .detectionRange = 14.0F,
        .preferredMinimumRange = 3.0F,
        .preferredMaximumRange = 6.0F,
        .moveSpeed = 2.0F,
        .attackDamage = 5.0F,
        .attackInterval = 1.0F,
        .repairRange = 7.0F,
        .repairInterval = 2.0F,
        .repairAmount = 12.0F,
    };
    const EnemyUpdateSummary occluded
        = updateEngineers(manager, player, 1, blocked, 0.0F, config);
    expect(occluded.repairsPerformed == 0U && occluded.attacksLanded == 0U
            && wkc::core::nearlyEqual(
                manager.findData<EnemyData>(damagedWorker)->health, 20.0F),
        "Engineer neither repairs nor attacks through blocking geometry");

    const CollisionWorld open{{{-12.0F, 0.0F, -12.0F}, {12.0F, 4.0F, 12.0F}}};
    manager.find(damagedWorker)->transform.position = {10.0F, 0.0F, 10.0F};
    const EnemyUpdateSummary approach
        = updateEngineers(manager, player, 1, open, 1.0F, config);
    expect(approach.chasing == 1U
            && wkc::core::nearlyEqual(manager.find(engineer)->transform.position.z, 6.0F),
        "Engineer advances into its short technical-tool firing envelope");
    const EnemyUpdateSummary shot
        = updateEngineers(manager, player, 1, open, 0.1F, config);
    expect(shot.attacking == 1U && shot.attacksLanded == 1U
            && wkc::core::nearlyEqual(player.state().health, 95.0F),
        "Engineer attacks from preferred range only with line of sight");

    manager.find(engineer)->transform.position = {0.0F, 0.0F, 1.0F};
    const float healthBeforeRetreat = player.state().health;
    const EnemyUpdateSummary retreat
        = updateEngineers(manager, player, 1, open, 0.5F, config);
    expect(retreat.repositioning == 1U
            && wkc::core::nearlyEqual(manager.find(engineer)->transform.position.z, 2.0F)
            && player.state().health == healthBeforeRetreat,
        "Engineer retreats without attacking inside minimum range");
    expect(applyDamage(manager, engineer, 500.0F) == DamageOutcome::Killed,
        "Engineer uses the shared damage and death pipeline");
}

void testIndustrialRobotPhases()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    expect(industrialRobotPhase(250.0F, 250.0F) == IndustrialRobotPhase::LongRange
            && industrialRobotPhase(165.0F, 250.0F) == IndustrialRobotPhase::Pressure
            && industrialRobotPhase(82.5F, 250.0F) == IndustrialRobotPhase::Pursuit,
        "IndustrialRobot phase thresholds use >66%, 33-66% and <=33% HP");

    const CollisionWorld room{{{-16.0F, 0.0F, -16.0F}, {16.0F, 4.0F, 16.0F}}};
    Player player;
    player.spawn({0.0F, 0.0F, 0.0F});
    EntityManager manager;
    const EntityId robot = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 14.0F}},
        .roomId = 7,
        .archetype = EnemyArchetype::IndustrialRobot,
        .health = 250.0F,
    });
    const EnemyUpdateSummary longApproach
        = updateIndustrialRobots(manager, player, 7, room, 1.0F);
    expect(longApproach.bossLongRange == 1U && longApproach.chasing == 1U
            && wkc::core::nearlyEqual(manager.find(robot)->transform.position.z, 13.25F),
        "healthy IndustrialRobot advances slowly into its long-range envelope");
    manager.find(robot)->transform.position = {0.0F, 0.0F, 12.0F};
    const EnemyUpdateSummary longShot
        = updateIndustrialRobots(manager, player, 7, room, 0.1F);
    expect(longShot.attacksLanded == 1U
            && wkc::core::nearlyEqual(player.state().health, 88.0F),
        "phase-one IndustrialRobot fires a high-impact long-range attack");

    EnemyData* data = manager.findData<EnemyData>(robot);
    data->health = 150.0F;
    data->attackCooldown = 0.0F;
    manager.find(robot)->transform.position = {0.0F, 0.0F, 6.0F};
    const EnemyUpdateSummary pressure
        = updateIndustrialRobots(manager, player, 7, room, 0.1F);
    expect(pressure.bossPressure == 1U && pressure.attacksLanded == 1U
            && wkc::core::nearlyEqual(player.state().health, 78.0F)
            && wkc::core::nearlyEqual(data->attackCooldown, 0.9F),
        "mid-health IndustrialRobot increases pressure and firing cadence");

    data->health = 50.0F;
    data->attackCooldown = 0.0F;
    manager.find(robot)->transform.position = {0.0F, 0.0F, 4.0F};
    const EnemyUpdateSummary pursuit
        = updateIndustrialRobots(manager, player, 7, room, 1.5F);
    expect(pursuit.bossPursuit == 1U && pursuit.chasing == 1U
            && wkc::core::nearlyEqual(manager.find(robot)->transform.position.z, 1.975F),
        "critical IndustrialRobot commits to its fastest close pursuit");
    const EnemyUpdateSummary closeStrike
        = updateIndustrialRobots(manager, player, 7, room, 0.1F);
    expect(closeStrike.attacksLanded == 1U
            && wkc::core::nearlyEqual(player.state().health, 62.0F)
            && wkc::core::nearlyEqual(data->attackCooldown, 0.65F),
        "critical IndustrialRobot delivers its strongest, fastest close attack");

    data->health = 250.0F;
    data->attackCooldown = 0.0F;
    manager.find(robot)->transform.position = {0.0F, 0.0F, 2.0F};
    const float healthBeforeRetreat = player.state().health;
    const EnemyUpdateSummary retreat
        = updateIndustrialRobots(manager, player, 7, room, 1.0F);
    expect(retreat.repositioning == 1U
            && wkc::core::nearlyEqual(manager.find(robot)->transform.position.z, 2.75F)
            && player.state().health == healthBeforeRetreat,
        "healthy IndustrialRobot repositions when the player breaches long range");

    const wkc::core::Vec3 deathPosition = manager.find(robot)->transform.position;
    expect(applyDamage(manager, robot, 500.0F) == DamageOutcome::Killed,
        "IndustrialRobot uses the shared damage and death pipeline");
    const EnemyUpdateSummary dead
        = updateIndustrialRobots(manager, player, 7, room, 2.0F);
    expect(dead.attacking == 0U && dead.chasing == 0U && dead.repositioning == 0U
            && manager.find(robot)->transform.position == deathPosition,
        "dead IndustrialRobot cannot move, change phase or attack");
}

void testIndustrialRobotLineOfSightAndRoomIsolation()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;
    Player player;
    player.spawn({0.0F, 0.0F, 0.0F});
    EntityManager manager;
    const EntityId blockedRobot = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 6.0F}},
        .roomId = 3,
        .archetype = EnemyArchetype::IndustrialRobot,
        .health = 250.0F,
    });
    const EntityId otherRoomRobot = manager.createEnemy({
        .transform = {.position = {2.0F, 0.0F, 6.0F}},
        .roomId = 4,
        .archetype = EnemyArchetype::IndustrialRobot,
        .health = 250.0F,
    });
    manager.findData<EnemyData>(blockedRobot)->health = 150.0F;
    manager.findData<EnemyData>(otherRoomRobot)->health = 150.0F;
    const CollisionWorld blocked{
        {{-12.0F, 0.0F, -12.0F}, {12.0F, 4.0F, 12.0F}},
        {{{-1.0F, 0.0F, 3.0F}, {1.0F, 3.0F, 3.5F}}},
    };
    const wkc::core::Vec3 otherPosition = manager.find(otherRoomRobot)->transform.position;
    const EnemyUpdateSummary update
        = updateIndustrialRobots(manager, player, 3, blocked, 0.5F);
    expect(update.chasing == 1U && update.attacksLanded == 0U
            && player.state().health == 100.0F,
        "IndustrialRobot cannot attack through geometry and instead advances");
    expect(manager.find(otherRoomRobot)->transform.position == otherPosition
            && manager.findData<EnemyData>(otherRoomRobot)->behavior
                == EnemyBehaviorState::Idle,
        "IndustrialRobot update never crosses the active room boundary");
    expect(manager.findData<EnemyData>(blockedRobot)->behavior == EnemyBehaviorState::Chase,
        "occluded IndustrialRobot keeps an explicit Chase state");
}

} // namespace

int main()
{
    testEnemyDifficultyConfigs();
    testWeaponLifecycle();
    testDamageLifecycle();
    testDormantEnemyIgnoresCombat();
    testHitscanSelectionAndBlocking();
    testWalkableUnionBlocksShotsAcrossDisconnectedSpace();
    testWorkerBehavior();
    testWorkerLineOfSight();
    testGuardRangedBehavior();
    testGuardLineOfSight();
    testOfficerCommandSupport();
    testOfficerRangedBehaviorAndLineOfSight();
    testEngineerRepairSystem();
    testEngineerRangedBehaviorAndRepairOcclusion();
    testIndustrialRobotPhases();
    testIndustrialRobotLineOfSightAndRoomIsolation();

    if (failures != 0) {
        std::cerr << failures << " combat test(s) failed\n";
        return 1;
    }

    std::cout << "All GDP combat tests passed\n";
    return 0;
}
