#include "core/Math.hpp"
#include "entities/EntityManager.hpp"
#include "gameplay/Combat.hpp"
#include "gameplay/Enemy.hpp"
#include "gameplay/Player.hpp"
#include "gameplay/Weapon.hpp"

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

    const gdp::core::Vec3 deathPosition = manager.find(worker)->transform.position;
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
            && gdp::core::nearlyEqual(manager.find(guard)->transform.position.z, 8.0F),
        "Guard outside its firing envelope advances to medium range");
    expect(manager.find(worker)->transform.position == gdp::core::Vec3{2.0F, 0.0F, 5.0F},
        "Guard update never moves a Worker archetype");

    const EnemyUpdateSummary shot = updateGuards(manager, player, 1, room, 0.1F, config);
    expect(shot.attacking == 1U && shot.attacksLanded == 1U
            && gdp::core::nearlyEqual(player.state().health, 93.0F),
        "Guard with line of sight fires from its preferred range");

    manager.find(guard)->transform.position = {0.0F, 0.0F, 2.0F};
    const float healthBeforeRetreat = player.state().health;
    const EnemyUpdateSummary retreat = updateGuards(manager, player, 1, room, 0.5F, config);
    expect(retreat.repositioning == 1U
            && gdp::core::nearlyEqual(manager.find(guard)->transform.position.z, 3.0F)
            && player.state().health == healthBeforeRetreat,
        "Guard that is rushed retreats without firing inside its minimum range");

    const gdp::core::Vec3 deathPosition = manager.find(guard)->transform.position;
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
            && gdp::core::nearlyEqual(
                manager.findData<EnemyData>(worker)->attackCooldown, 0.6F)
            && gdp::core::nearlyEqual(
                manager.findData<EnemyData>(guard)->attackCooldown, 0.4F),
        "Officer command accelerates readying Worker and Guard allies in support range");
    expect(gdp::core::nearlyEqual(
               manager.findData<EnemyData>(remoteWorker)->attackCooldown, 1.0F),
        "Officer command never crosses the active room boundary");
    expect(gdp::core::nearlyEqual(
               manager.findData<EnemyData>(officer)->supportCooldown, 2.5F),
        "Officer command starts an independent support cooldown");

    const EnemyUpdateSummary repeated = updateOfficers(manager, player, 1, room, 0.1F, config);
    expect(repeated.commandsIssued == 0U && repeated.alliesSupported == 0U
            && gdp::core::nearlyEqual(
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
            && gdp::core::nearlyEqual(manager.find(officer)->transform.position.z, 10.0F),
        "Officer advances to its longer command-range firing envelope");
    const EnemyUpdateSummary shot = updateOfficers(manager, player, 1, open, 0.1F, config);
    expect(shot.attacking == 1U && shot.attacksLanded == 1U
            && gdp::core::nearlyEqual(player.state().health, 94.0F),
        "Officer fires its sidearm only from the preferred range with line of sight");

    manager.find(officer)->transform.position = {0.0F, 0.0F, 3.0F};
    const float healthBeforeRetreat = player.state().health;
    const EnemyUpdateSummary retreat = updateOfficers(manager, player, 1, open, 0.5F, config);
    expect(retreat.repositioning == 1U
            && gdp::core::nearlyEqual(manager.find(officer)->transform.position.z, 4.0F)
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
    expect(gdp::core::nearlyEqual(
               manager.findData<EnemyData>(officer)->supportCooldown, 0.0F),
        "Officer death clears command state together with attack state");
}

} // namespace

int main()
{
    testWeaponLifecycle();
    testDamageLifecycle();
    testHitscanSelectionAndBlocking();
    testWorkerBehavior();
    testWorkerLineOfSight();
    testGuardRangedBehavior();
    testGuardLineOfSight();
    testOfficerCommandSupport();
    testOfficerRangedBehaviorAndLineOfSight();

    if (failures != 0) {
        std::cerr << failures << " combat test(s) failed\n";
        return 1;
    }

    std::cout << "All GDP combat tests passed\n";
    return 0;
}
