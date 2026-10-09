#include "core/Math.hpp"
#include "entities/EntityManager.hpp"
#include "gameplay/Combat.hpp"
#include "gameplay/Enemy.hpp"
#include "gameplay/Player.hpp"
#include "gameplay/Weapon.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <string_view>
#include <tuple>

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

void testCombatSignalBatch()
{
    using namespace gdp::gameplay;
    const gdp::entities::EntityId target{7U, 2U};
    const HitResult hit{target, {1.0F, 2.0F, 3.0F}, 4.0F, HitZone::WeakPoint};
    const ImpactResult impact{{1.0F, 2.0F, 3.0F}, {0.0F, 0.0F, 1.0F},
        ImpactMaterial::Metal};

    for (const ShotOutcome rejected : {ShotOutcome::Cooldown,
             ShotOutcome::Reloading, ShotOutcome::Switching, ShotOutcome::Empty}) {
        const ShotResult shot{rejected, std::nullopt, {}, std::nullopt};
        expect(combatSignalsForShot(shot).count == 0U,
            "rejected shots do not emit fire or contact signals");
    }
    const ShotResult miss{ShotOutcome::Miss, std::nullopt, {}, std::nullopt};
    const CombatSignalBatch missed = combatSignalsForShot(miss);
    expect(missed.count == 1U
            && missed.signals[0].kind == CombatSignalKind::WeaponFired,
        "accepted miss emits only weapon fired");

    const ShotResult world{ShotOutcome::ImpactWorld, std::nullopt, {}, impact};
    const CombatSignalBatch wall = combatSignalsForShot(world);
    expect(wall.count == 2U
            && wall.signals[0].kind == CombatSignalKind::WeaponFired
            && wall.signals[1].kind == CombatSignalKind::ImpactWorld
            && wall.signals[1].point == impact.point
            && wall.signals[1].impact
            && wall.signals[1].impact->material == ImpactMaterial::Metal,
        "world contact carries position and material after weapon fired");

    for (const ShotOutcome outcome : {ShotOutcome::ImpactProp, ShotOutcome::Hit,
             ShotOutcome::Killed, ShotOutcome::Destroyed}) {
        const ShotResult shot{outcome, hit, {}, impact};
        const CombatSignalBatch batch = combatSignalsForShot(shot);
        const bool damaged = outcome == ShotOutcome::Hit;
        const bool killed = outcome == ShotOutcome::Killed
            || outcome == ShotOutcome::Destroyed;
        expect(batch.count == (damaged || killed ? 3U : 2U)
                && batch.signals[0].kind == CombatSignalKind::WeaponFired
                && batch.signals[1].kind == CombatSignalKind::EntityHit
                && batch.signals[1].target == target
                && batch.signals[1].zone == HitZone::WeakPoint
                && batch.signals[1].point == hit.point
                && batch.signals[1].impact,
            "entity contact preserves target, weak point and impact in order");
        if (damaged || killed) {
            expect(batch.signals[2].kind == (damaged
                    ? CombatSignalKind::EntityDamaged
                    : CombatSignalKind::EntityKilled)
                    && batch.signals[2].outcome == outcome
                    && batch.signals[2].target == target,
                "damage and destruction append the correct terminal signal");
        }
    }
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
    const float reloadDuration = weaponReloadDurationSeconds(
        WeaponArchetype::Pistol);
    expect(reloadDuration > 0.0F
            && weaponReloadDurationSeconds(WeaponArchetype::Carbine)
                > reloadDuration,
        "heavier Carbine keeps a longer positive reload duration than Pistol");
    expect(weapon.reload() && weapon.reloading()
            && weapon.magazineAmmo() == 0 && weapon.reserveAmmo() == 3
            && near(weapon.reloadProgress(), 0.0F),
        "reload starts without transferring ammunition early");
    expect(weapon.tryFire() == FireReadiness::Reloading && !weapon.reload(),
        "active reload blocks fire and duplicate reload requests");
    weapon.fixedUpdate(reloadDuration * 0.5F);
    expect(weapon.reloading() && weapon.magazineAmmo() == 0
            && weapon.reserveAmmo() == 3
            && near(weapon.reloadProgress(), 0.5F),
        "reload advances deterministically without an early ammo grant");
    weapon.fixedUpdate(reloadDuration * 0.5F);
    expect(!weapon.reloading() && near(weapon.reloadProgress(), 0.0F)
            && weapon.magazineAmmo() == 2 && weapon.reserveAmmo() == 2,
        "reload completion consumes exactly one complete spare magazine");
    expect(!weapon.reload(), "full magazine rejects a redundant reload");

    Weapon empty({.fireInterval = 0.0F, .magazineSize = 1, .reserveMaximum = 0});
    expect(empty.tryFire() == FireReadiness::Fired, "last cartridge can fire");
    expect(empty.tryFire() == FireReadiness::Empty,
        "empty weapon rejects fire deterministically");
}

void testDiscardedMagazineReloads()
{
    using namespace gdp::gameplay;
    for (const auto type : {WeaponArchetype::Pistol, WeaponArchetype::Carbine,
             WeaponArchetype::SubmachineGun, WeaponArchetype::Sniper}) {
        Weapon gun(baseWeaponDefinition(type));
        const auto size = gun.definition().magazineSize;
        const auto reserve = gun.reserveAmmo();
        const auto duration = weaponReloadDurationSeconds(type);
        expect(gun.tryFire() == FireReadiness::Fired && gun.reload(), "partial magazine starts replacement");
        gun.fixedUpdate(duration * 0.20F);
        expect(gun.magazineAmmo() == size - 1U && gun.switchTo(WeaponArchetype::Knife),
            "switch before ejection retains the old magazine");
        (void)gun.switchTo(type); gun.fixedUpdate(1.0F);
        expect(gun.reload(), "interrupted reload can restart");
        gun.fixedUpdate(duration * 0.40F);
        expect(gun.magazineAmmo() == 0U && gun.reserveAmmo() == reserve,
            "ejection discards all remaining rounds without spending an uninserted spare");
        const auto dropped = gun.dropSelectedFirearm();
        expect(dropped && dropped->magazineAmmo == 0U && dropped->reserveAmmo == reserve,
            "dropping after ejection cannot recover discarded bullets");
        gun.fixedUpdate(1.0F);
        expect(dropped && gun.equipRecovered(type, dropped->magazineAmmo, dropped->reserveAmmo),
            "recovered weapon preserves interrupted ammo state");
        gun.fixedUpdate(1.0F);
        expect(gun.reload(), "empty recovered magazine reloads");
        gun.fixedUpdate(duration + 1.0F);
        expect(gun.magazineAmmo() == size && gun.reserveAmmo() == reserve - 1U,
            "large fixed step consumes precisely one full magazine");
        gun.fixedUpdate(duration);
        expect(gun.reserveAmmo() == reserve - 1U && !gun.reload(), "completion and full-mag reload never duplicate ammo");
        while (gun.reserveAmmo() > 0U) {
            (void)gun.tryFire(); (void)gun.reload(); gun.fixedUpdate(duration + 1.0F);
        }
        (void)gun.tryFire();
        expect(!gun.reload() && gun.magazineAmmo() == size - 1U,
            "zero spare magazines reject reload without discarding the loaded magazine");
    }
    Weapon shotgun(baseWeaponDefinition(WeaponArchetype::Shotgun));
    (void)shotgun.tryFire(); (void)shotgun.reload(); shotgun.fixedUpdate(3.0F);
    expect(shotgun.magazineAmmo() == 6U && shotgun.reserveAmmo() == 23U,
        "shotgun reload retains loaded shells and uses only the missing shell");
    expect(reserveUnitsFromRounds(baseWeaponDefinition(WeaponArchetype::Pistol), 9U) == 2U
        && reserveUnitsFromRounds(baseWeaponDefinition(WeaponArchetype::Shotgun), 9U) == 9U,
        "round budgets become complete magazines but stay individual shotgun shells");
    Weapon switched;
    (void)switched.tryFire(); (void)switched.reload(); switched.fixedUpdate(0.40F);
    (void)switched.switchTo(WeaponArchetype::Knife);
    (void)switched.switchTo(WeaponArchetype::Pistol); switched.fixedUpdate(1.0F);
    expect(switched.magazineAmmo() == 0U && switched.reserveAmmo() == 4U,
        "switch after ejection cannot restore discarded rounds or spend a spare");
    (void)switched.reload(); switched.fixedUpdate(1.0F);
    expect(switched.magazineAmmo() == 8U && switched.reserveAmmo() == 3U,
        "restart after switch commits only one spare magazine");
    Weapon upgraded;
    expect(upgraded.increaseReserveCapacity(4U, 4U)
        && upgraded.equip(WeaponArchetype::Shotgun), "capacity rank follows weapon type");
    upgraded.fixedUpdate(1.0F);
    expect(upgraded.reserveCapacity() == 28U && upgraded.campaignProgress().stowed->definition.reserveMaximum == 5U,
        "one extra magazine upgrade becomes four shells, not four magazines");
}

void testPerShellShotgunReload()
{
    using namespace gdp::gameplay;
    for (std::uint32_t missing = 1U; missing <= 6U; ++missing) {
        Weapon gun(baseWeaponDefinition(WeaponArchetype::Shotgun));
        auto state = gun.campaignProgress();
        state.magazineAmmo = 6U - missing;
        (void)gun.restoreCampaignProgress(state);
        expect(gun.reload() && gun.reloadRoundCount() == missing
            && near(gun.reloadRemaining(), static_cast<float>(missing) * shotgunShellReloadSeconds + shotgunReadySeconds)
            && near(gun.reloadProgress(), 0.0F), "shotgun duration scales with missing shells");
        gun.fixedUpdate(shotgunShellReloadSeconds - 0.01F);
        expect(gun.magazineAmmo() == 6U - missing && gun.reserveAmmo() == 24U,
            "no shell is granted before its individual insertion time");
        gun.fixedUpdate(0.01F);
        expect(gun.magazineAmmo() == 7U - missing && gun.reserveAmmo() == 23U
            && gun.reloadRoundCount() == missing && gun.tryFire() == FireReadiness::Reloading,
            "first shell consumes one reserve and visual count stays stable");
        (void)gun.switchTo(WeaponArchetype::Knife); (void)gun.switchTo(WeaponArchetype::Shotgun);
        gun.fixedUpdate(1.0F);
        expect(gun.magazineAmmo() == 7U - missing && gun.reserveAmmo() == 23U,
            "interrupting shell reload preserves only inserted shells");
        if (missing > 1U) {
            expect(gun.reload(), "partially loaded shotgun resumes");
            gun.fixedUpdate(10.0F);
            expect(gun.magazineAmmo() == 6U && gun.reserveAmmo() == 24U - missing,
                "large update inserts all remaining shells without duplication");
        }
    }
    Weapon limited(baseWeaponDefinition(WeaponArchetype::Shotgun));
    auto state = limited.campaignProgress(); state.magazineAmmo = 0U; state.reserveAmmo = 2U;
    (void)limited.restoreCampaignProgress(state);
    expect(limited.reload() && near(limited.reloadRemaining(), 1.0F),
        "shotgun reload duration also respects available reserve shells");
    limited.fixedUpdate(0.70F);
    expect(limited.magazineAmmo() == 2U && limited.reserveAmmo() == 0U && limited.reloading(),
        "two shells inserted before final pump ready time");
    limited.fixedUpdate(0.31F);
    expect(!limited.reloading() && limited.magazineAmmo() == 2U && !limited.reload(),
        "shotgun finishes with available shells, no invented cartridges");
}

void testDeterministicWeaponSpread()
{
    using namespace gdp::gameplay;

    const wkc::core::Vec3 forward{0.0F, 0.0F, -1.0F};
    const wkc::core::Vec3 centered = applyDeterministicWeaponSpread(
        forward, WeaponArchetype::Pistol, 0U);
    const wkc::core::Vec3 pistol = applyDeterministicWeaponSpread(
        forward, WeaponArchetype::Pistol, 10U);
    const wkc::core::Vec3 repeated = applyDeterministicWeaponSpread(
        forward, WeaponArchetype::Pistol, 10U);
    const wkc::core::Vec3 carbine = applyDeterministicWeaponSpread(
        forward, WeaponArchetype::Carbine, 10U);
    expect(centered == forward,
        "first shot remains exactly aligned with the reticle");
    const float stationary = weaponMovementInaccuracyDegrees(
        WeaponArchetype::Carbine, {}, true, false, 0.0F);
    const float walking = weaponMovementInaccuracyDegrees(
        WeaponArchetype::Carbine, {4.5F, 0.0F, 0.0F}, true, false, 0.0F);
    const float quiet = weaponMovementInaccuracyDegrees(
        WeaponArchetype::Carbine, {2.25F, 0.0F, 0.0F}, true, false, 0.0F);
    const float airborne = weaponMovementInaccuracyDegrees(
        WeaponArchetype::Carbine, {4.5F, 5.0F, 0.0F}, false, false, 0.0F);
    expect(near(stationary, 0.0F) && quiet > 0.0F
            && quiet < walking && walking < airborne,
        "walking, quiet walking and jumping have distinct accuracy costs");
    expect(weaponMovementInaccuracyDegrees(
               WeaponArchetype::Knife, {4.5F, 0.0F, 0.0F}, false, false, 0.0F) == 0.0F
            && weaponMovementInaccuracyDegrees(
                WeaponArchetype::Carbine, {4.5F, 0.0F, 0.0F}, true, true, 0.0F) < walking
            && weaponMovementInaccuracyDegrees(
                WeaponArchetype::Carbine, {4.5F, 0.0F, 0.0F}, true, false, 1.0F) < walking,
        "knife remains exact while crouch and aiming mitigate movement spread");
    const auto movingFirst = applyDeterministicWeaponSpread(
        forward, WeaponArchetype::Carbine, 0U, 1.0F, walking);
    const auto jumpingFirst = applyDeterministicWeaponSpread(
        forward, WeaponArchetype::Carbine, 0U, 1.0F, airborne);
    expect(movingFirst != forward
            && movingFirst == applyDeterministicWeaponSpread(
                forward, WeaponArchetype::Carbine, 0U, 1.0F, walking)
            && wkc::core::dot(jumpingFirst, forward)
                < wkc::core::dot(movingFirst, forward),
        "moving first shot has deterministic drift and jumping worsens it");
    expect(applyDeterministicWeaponSpread(
               forward, WeaponArchetype::Carbine, 0U, 1.0F, -1.0F) == forward,
        "invalid negative movement penalty cannot alter a stationary first shot");
    const CollisionWorld movementRoom{
        {{-10.0F, 0.0F, -10.0F}, {10.0F, 4.0F, 10.0F}}};
    gdp::entities::EntityManager movementEntities;
    Weapon stillGun(baseWeaponDefinition(WeaponArchetype::Carbine));
    Weapon movingGun(baseWeaponDefinition(WeaponArchetype::Carbine));
    const auto stillShot = fireHitscan(stillGun, {0.0F, 1.0F, 0.0F},
        forward, 1U, movementRoom, movementEntities);
    const auto movingShot = fireHitscan(movingGun, {0.0F, 1.0F, 0.0F},
        forward, 1U, movementRoom, movementEntities, 1.0F, {}, walking);
    expect(stillShot.impact && movingShot.impact
            && near(stillShot.impact->point.x, 0.0F)
            && movingShot.impact->point.x > stillShot.impact->point.x,
        "hitscan uses movement inaccuracy for the actual world impact");
    expect(pistol == repeated && !near(pistol.x, 0.0F)
            && !near(pistol.y, 0.0F),
        "later shots use a repeatable two-axis pattern without RNG");
    const wkc::core::Vec3 aimed = applyDeterministicWeaponSpread(
        forward, WeaponArchetype::Pistol, 10U, 0.40F);
    expect(wkc::core::dot(aimed, forward) > wkc::core::dot(pistol, forward)
            && aimed == applyDeterministicWeaponSpread(
                forward, WeaponArchetype::Pistol, 10U, 0.40F),
        "aiming narrows deterministic spread without a new RNG stream");
    expect(wkc::core::dot(carbine, forward)
            < wkc::core::dot(pistol, forward),
        "Carbine reaches a wider spread envelope than Pistol");
    expect(applyDeterministicWeaponSpread(
               {}, WeaponArchetype::Carbine, 4U) == wkc::core::Vec3{},
        "invalid direction remains a safe miss after spread sampling");

    Weapon gated({
        .fireInterval = 0.20F,
        .magazineSize = 2U,
        .reserveMaximum = 0U,
    });
    expect(gated.tryFire() == FireReadiness::Fired
            && gated.spreadShotIndex() == 0U,
        "gated weapon accepts the first centered shot");
    const float gatedStrength = gated.spreadStrength();
    expect(gated.tryFire() == FireReadiness::Cooldown
            && gated.spreadShotIndex() == 0U
            && near(gated.spreadStrength(), gatedStrength),
        "cooldown rejection cannot advance spread or crosshair bloom");

    Weapon weapon({
        .fireInterval = 0.0F,
        .magazineSize = 4U,
        .reserveMaximum = 4U,
    });
    expect(weapon.tryFire() == FireReadiness::Fired
            && weapon.spreadShotIndex() == 0U
            && weapon.spreadStrength() > 0.0F,
        "accepted first shot starts a fresh spread sequence");
    expect(weapon.tryFire() == FireReadiness::Fired
            && weapon.spreadShotIndex() == 1U,
        "a rapid accepted shot advances the spread sequence once");
    expect(weapon.reload()
            && weapon.tryFire() == FireReadiness::Reloading,
        "reload blocks firing until its fixed-time transaction completes");
    weapon.fixedUpdate(weaponReloadDurationSeconds(WeaponArchetype::Pistol));
    expect(weapon.tryFire() == FireReadiness::Fired
            && weapon.spreadShotIndex() == 0U,
        "reload returns the next shot to centered accuracy");
    expect(weapon.tryFire() == FireReadiness::Fired
            && weapon.spreadShotIndex() == 1U,
        "spread starts building again after the centered reload shot");
    weapon.fixedUpdate(weaponSpreadProfile(WeaponArchetype::Pistol).recoverySeconds);
    expect(near(weapon.spreadStrength(), 0.0F),
        "crosshair bloom closes when the logical spread sequence recovers");
    expect(weapon.tryFire() == FireReadiness::Fired
            && weapon.spreadShotIndex() == 0U
            && weapon.spreadRecoveryRemaining() > 0.0F,
        "fixed-time recovery restores first-shot accuracy independently of FPS");

    const CollisionWorld room{{{-10.0F, 0.0F, -10.0F}, {10.0F, 4.0F, 10.0F}}};
    gdp::entities::EntityManager manager;
    Weapon integration({
        .fireInterval = 0.0F,
        .range = 20.0F,
        .magazineSize = 2U,
        .reserveMaximum = 0U,
    });
    const ShotResult first = fireHitscan(
        integration, {0.0F, 1.0F, 0.0F}, forward, 1U, room, manager);
    const ShotResult second = fireHitscan(
        integration, {0.0F, 1.0F, 0.0F}, forward, 1U, room, manager);
    expect(first.outcome == ShotOutcome::ImpactWorld && first.impact
            && near(first.impact->point.x, 0.0F)
            && second.outcome == ShotOutcome::ImpactWorld && second.impact
            && !near(second.impact->point.x, 0.0F),
        "hitscan consumes the centered shot followed by the patterned shot");

    Weapon reloading({
        .fireInterval = 0.0F,
        .range = 20.0F,
        .magazineSize = 2U,
        .reserveMaximum = 2U,
    });
    expect(reloading.tryFire() == FireReadiness::Fired && reloading.reload(),
        "hitscan reload fixture begins from a partially depleted magazine");
    const ShotResult blockedByReload = fireHitscan(
        reloading, {0.0F, 1.0F, 0.0F}, forward, 1U, room, manager);
    expect(blockedByReload.outcome == ShotOutcome::Reloading
            && !blockedByReload.hit && !blockedByReload.impact
            && reloading.magazineAmmo() == 1U,
        "hitscan reports an active reload without fabricating impact or ammo use");

    Weapon switching;
    expect(switching.equip(WeaponArchetype::Carbine),
        "hitscan switch fixture equips its second weapon");
    const ShotResult blockedBySwitch = fireHitscan(
        switching, {0.0F, 1.0F, 0.0F}, forward, 1U, room, manager);
    expect(blockedBySwitch.outcome == ShotOutcome::Switching
            && !blockedBySwitch.hit && !blockedBySwitch.impact
            && switching.magazineAmmo()
                == switching.definition().magazineSize,
        "hitscan reports weapon entry without fabricating impact or ammo use");
}

void testWeaponCameraRecoil()
{
    using namespace gdp::gameplay;

    expect(weaponRecoilKickDegrees(WeaponArchetype::Knife, 0U)
            == wkc::core::Vec2{},
        "knife never kicks the camera");
    const auto first = weaponRecoilKickDegrees(WeaponArchetype::Carbine, 0U);
    const auto second = weaponRecoilKickDegrees(WeaponArchetype::Carbine, 1U);
    const auto aimed = weaponRecoilKickDegrees(WeaponArchetype::Carbine, 1U, 1.0F);
    const auto crouched = weaponRecoilKickDegrees(WeaponArchetype::Carbine, 1U, 0.0F, true);
    expect(near(first.x, 0.0F) && first.y > 0.0F
            && second.x > 0.0F && second.y > 0.0F,
        "accepted carbine shots climb and later shots shift horizontally");
    expect(second == weaponRecoilKickDegrees(WeaponArchetype::Carbine, 1U)
            && weaponRecoilKickDegrees(WeaponArchetype::Carbine, 5U).x < 0.0F,
        "spray is repeatable and changes horizontal direction");
    expect(aimed.y < second.y && crouched.y < second.y,
        "aiming and crouching reduce but do not erase recoil");
    expect(weaponRecoilKickDegrees(WeaponArchetype::Sniper, 0U).y > first.y
            && weaponRecoilKickDegrees(WeaponArchetype::Shotgun, 0U).y > first.y
            && weaponRecoilKickDegrees(WeaponArchetype::SubmachineGun, 0U).y < first.y,
        "each firearm has a distinct camera kick");

    Weapon gun(baseWeaponDefinition(WeaponArchetype::Carbine));
    expect(gun.tryFire() == FireReadiness::Fired
            && gun.spreadShotIndex() == 0U,
        "the first accepted shot selects the first recoil step");
    expect(gun.tryFire() == FireReadiness::Cooldown
            && gun.spreadShotIndex() == 0U,
        "a rejected shot cannot advance the recoil sequence");
    gun.fixedUpdate(gun.definition().fireInterval);
    expect(gun.tryFire() == FireReadiness::Fired
            && gun.spreadShotIndex() == 1U,
        "the next accepted shot advances the recoil sequence");
    gun.fixedUpdate(weaponSpreadProfile(WeaponArchetype::Carbine).recoverySeconds);
    expect(gun.tryFire() == FireReadiness::Fired
            && gun.spreadShotIndex() == 0U,
        "a firing pause resets the recoil pattern");
}

void testPracticeRestock()
{
    using namespace gdp::gameplay;

    Weapon carbine(baseWeaponDefinition(WeaponArchetype::Carbine));
    expect(carbine.tryFire() == FireReadiness::Fired
            && carbine.activateOverdrive(),
        "practice restock fixture spends a round during overdrive");
    const float cooldown = carbine.cooldownRemaining();
    carbine.restockAllFirearms();
    expect(carbine.magazineAmmo() == carbine.definition().magazineSize
            && carbine.reserveAmmo() == carbine.definition().reserveMaximum
            && carbine.overdriveActive() && near(carbine.cooldownRemaining(), cooldown),
        "restock fills ammo without changing overdrive or bypassing fire cadence");

    Weapon loadout;
    expect(loadout.tryFire() == FireReadiness::Fired
            && loadout.equip(WeaponArchetype::Carbine),
        "practice fixture stows a used pistol behind a carbine");
    loadout.fixedUpdate(1.0F);
    expect(loadout.tryFire() == FireReadiness::Fired
            && loadout.switchTo(WeaponArchetype::Knife),
        "practice fixture selects the knife after spending carbine ammo");
    loadout.restockAllFirearms();
    const auto progress = loadout.campaignProgress();
    expect(loadout.definition().archetype == WeaponArchetype::Knife
            && progress.magazineAmmo == progress.definition.magazineSize
            && progress.reserveAmmo == progress.definition.reserveMaximum
            && progress.stowed
            && progress.stowed->magazineAmmo == progress.stowed->definition.magazineSize
            && progress.stowed->reserveAmmo == progress.stowed->definition.reserveMaximum,
        "restock fills both firearms without switching away from the knife");

    Weapon shotgun(baseWeaponDefinition(WeaponArchetype::Shotgun));
    expect(shotgun.tryFire() == FireReadiness::Fired, "shotgun restock fixture fires");
    shotgun.fixedUpdate(shotgun.definition().fireInterval);
    expect(shotgun.reload(), "shotgun restock fixture begins a shell reload");
    shotgun.restockAllFirearms();
    shotgun.fixedUpdate(10.0F);
    expect(!shotgun.reloading()
            && shotgun.magazineAmmo() == shotgun.definition().magazineSize
            && shotgun.reserveAmmo() == shotgun.definition().reserveMaximum,
        "restock cancels an unfinished reload without double-consuming shells");
}

void testWeaponPickupEquipment()
{
    using namespace gdp::gameplay;
    expect(weaponArchetypeForSlot(1U) == WeaponArchetype::Carbine
            && weaponArchetypeForSlot(2U) == WeaponArchetype::Pistol
            && weaponArchetypeForSlot(3U) == WeaponArchetype::Knife
            && !weaponArchetypeForSlot(0U)
            && !weaponArchetypeForSlot(4U),
        "weapon slots map Carbine, Pistol and Knife to 1, 2 and 3");
    Weapon weapon;
    expect(weapon.ownsWeapon(WeaponArchetype::Pistol)
            && weapon.ownsWeapon(WeaponArchetype::Knife)
            && !weapon.ownsWeapon(WeaponArchetype::Carbine),
        "new loadout owns its initial Pistol and permanent Knife fallback");
    expect(weapon.increaseDamage(4.0F)
            && weapon.increaseReserveCapacity(4U),
        "weapon pickup fixture applies existing progression bonuses");
    (void)weapon.tryFire();
    expect(weapon.equip(WeaponArchetype::Carbine),
        "Carbine pickup adds the long gun and equips it");
    expect(weapon.definition().archetype == WeaponArchetype::Carbine
            && weapon.ownsWeapon(WeaponArchetype::Pistol)
            && weapon.ownsWeapon(WeaponArchetype::Carbine)
            && near(weapon.definition().damage, 30.0F)
            && near(weapon.definition().fireInterval, 0.13F)
            && weapon.definition().magazineSize == 12U
            && weapon.reserveCapacity() == 5U
            && weapon.magazineAmmo() == 12U
            && weapon.reserveAmmo() == 5U
            && near(weapon.cooldownRemaining(), 0.0F)
            && weapon.switching()
            && near(weapon.switchStrength(), 1.0F),
        "Carbine keeps upgrades and starts with its own complete ammunition profile");
    const float carbineSwitchDuration
        = weaponSwitchDurationSeconds(WeaponArchetype::Carbine);
    const float pistolSwitchDuration
        = weaponSwitchDurationSeconds(WeaponArchetype::Pistol);
    expect(carbineSwitchDuration > pistolSwitchDuration
            && pistolSwitchDuration > 0.0F
            && weapon.tryFire() == FireReadiness::Switching
            && !weapon.reload()
            && !weaponFireRequested(weapon, true, true),
        "weapon entry blocks fire and reload authoritatively");
    weapon.fixedUpdate(carbineSwitchDuration * 0.5F);
    expect(weapon.switching()
            && near(weapon.switchRemaining(), carbineSwitchDuration * 0.5F)
            && near(weapon.switchStrength(), 0.5F),
        "weapon entry advances from the fixed update with render-ready strength");
    expect(weapon.switchTo(WeaponArchetype::Pistol)
            && weapon.definition().archetype == WeaponArchetype::Pistol
            && near(weapon.switchStrength(), 1.0F)
            && weapon.switchTo(WeaponArchetype::Carbine)
            && weapon.definition().archetype == WeaponArchetype::Carbine
            && near(weapon.switchStrength(), 1.0F)
            && weapon.tryFire() == FireReadiness::Switching,
        "new slot requests interrupt entry and restart the incoming weapon animation");
    weapon.fixedUpdate(carbineSwitchDuration);
    expect(!weapon.switching() && near(weapon.switchStrength(), 0.0F)
            && weapon.tryFire() == FireReadiness::Fired,
        "weapon becomes fire-ready only after its complete entry duration");
    expect(weapon.reload() && weapon.switchTo(WeaponArchetype::Pistol)
            && !weapon.reloading() && weapon.magazineAmmo() == 7U
            && weapon.switching(),
        "switching restores Pistol ammunition and cancels transient Carbine reload");
    expect(!weapon.equip(WeaponArchetype::Carbine)
            && weapon.definition().archetype == WeaponArchetype::Pistol,
        "duplicate Carbine pickups cannot refill a stowed weapon");
    expect(weapon.increaseDamage(4.0F)
            && weapon.increaseReserveCapacity(4U, 4U),
        "progression upgrades remain available to both weapon slots");
    weapon.fixedUpdate(pistolSwitchDuration);
    expect(weapon.cycleWeapon()
            && weapon.definition().archetype == WeaponArchetype::Knife
            && near(weapon.definition().damage, 56.0F)
            && weapon.magazineAmmo() == 0U
            && weapon.reserveAmmo() == 0U,
        "cycling reaches the permanent Knife with loadout-wide damage upgrades");
    expect(weapon.switchTo(WeaponArchetype::Carbine)
            && near(weapon.definition().damage, 34.0F)
            && weapon.reserveCapacity() == 6U
            && weapon.magazineAmmo() == 11U
            && weapon.reserveAmmo() == 6U
            && weapon.switchTo(WeaponArchetype::Pistol)
            && near(weapon.definition().damage, 42.0F)
            && weapon.reserveCapacity() == 6U
            && weapon.switching()
            && !weapon.switchTo(WeaponArchetype::Pistol),
        "an interrupting direct selection restores Pistol and rejects redundancy");
    weapon.fixedUpdate(pistolSwitchDuration);
    expect(weapon.activateOverdrive() && weapon.cycleWeapon()
            && weapon.overdriveActive()
            && weapon.definition().archetype == WeaponArchetype::Knife,
        "weapon switching preserves the loadout-wide Overdrive timer");
}

void testKnifeFallback()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    Weapon weapon;
    const std::uint32_t pistolMagazine = weapon.magazineAmmo();
    const std::uint32_t pistolReserve = weapon.reserveAmmo();
    expect(weapon.nextWeaponArchetype() == WeaponArchetype::Knife
            && weapon.switchTo(WeaponArchetype::Knife)
            && weapon.definition().archetype == WeaponArchetype::Knife
            && weapon.switching() && !weapon.reload()
            && weapon.magazineAmmo() == 0U && weapon.reserveAmmo() == 0U,
        "Knife is always selectable after Pistol and never exposes ammunition");
    weapon.fixedUpdate(weaponSwitchDurationSeconds(WeaponArchetype::Knife));
    expect(!weapon.dropSelectedFirearm()
            && weapon.ownsWeapon(WeaponArchetype::Pistol),
        "equipped Knife cannot be dropped while a firearm is owned");
    expect(!weaponUsesAutomaticFire(WeaponArchetype::Knife)
            && weaponFireRequested(weapon, true, true)
            && !weaponFireRequested(weapon, false, true),
        "Knife attacks once per press instead of repeating while held");

    const CollisionWorld room{{{-5.0F, 0.0F, -5.0F}, {5.0F, 4.0F, 5.0F}}};
    EntityManager closeManager;
    const EntityId closeEnemy = closeManager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, -1.4F}},
        .roomId = 1U,
        .health = 60.0F,
    });
    const ShotResult close = fireHitscan(
        weapon, {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, -1.0F},
        1U, room, closeManager);
    expect(close.outcome == ShotOutcome::Hit && close.hit
            && close.hit->target == closeEnemy
            && near(closeManager.findData<EnemyData>(closeEnemy)->health, 12.0F)
            && weapon.tryFire() == FireReadiness::Cooldown,
        "Knife deals 48 damage inside its short reach and enforces swing cadence");

    weapon.fixedUpdate(baseWeaponDefinition(WeaponArchetype::Knife).fireInterval);
    EntityManager farManager;
    const EntityId farEnemy = farManager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, -2.3F}},
        .roomId = 1U,
        .health = 60.0F,
    });
    const ShotResult far = fireHitscan(
        weapon, {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, -1.0F},
        1U, room, farManager);
    expect(far.outcome == ShotOutcome::Miss && !far.hit
            && near(farManager.findData<EnemyData>(farEnemy)->health, 60.0F),
        "Knife cannot damage a target beyond its authored melee reach");

    const WeaponCampaignProgress progress = weapon.campaignProgress();
    expect(progress.definition.archetype == WeaponArchetype::Pistol
            && weapon.switchTo(WeaponArchetype::Pistol)
            && weapon.magazineAmmo() == pistolMagazine
            && weapon.reserveAmmo() == pistolReserve,
        "Knife selection stays transient and preserves the underlying gun save and ammo");
}

void testDirectionalAndLastWeaponSelection()
{
    using namespace gdp::gameplay;
    Weapon weapon;
    expect(!weapon.lastWeaponArchetype()
            && weapon.nextWeaponArchetype() == WeaponArchetype::Knife
            && weapon.previousWeaponArchetype() == WeaponArchetype::Knife,
        "single firearm and Knife wrap in both wheel directions");
    expect(weapon.switchTo(WeaponArchetype::Knife)
            && weapon.lastWeaponArchetype() == WeaponArchetype::Pistol,
        "Q remembers the last selected firearm after drawing Knife");
    expect(weapon.switchTo(*weapon.lastWeaponArchetype())
            && weapon.lastWeaponArchetype() == WeaponArchetype::Knife,
        "Q can toggle back to the previous selection");
    weapon.fixedUpdate(weaponSwitchDurationSeconds(WeaponArchetype::Pistol));
    expect(weapon.equip(WeaponArchetype::Carbine)
            && weapon.lastWeaponArchetype() == WeaponArchetype::Pistol
            && weapon.nextWeaponArchetype() == WeaponArchetype::Pistol
            && weapon.previousWeaponArchetype() == WeaponArchetype::Knife,
        "newly equipped Carbine preserves Q and opposite wheel directions");
    const WeaponCampaignProgress progress = weapon.campaignProgress();
    expect(weapon.restoreCampaignProgress(progress)
            && !weapon.lastWeaponArchetype(),
        "checkpoint restore does not invent a previous transient weapon");
}

void testDropAndRecoverFirearm()
{
    using namespace gdp::gameplay;
    Weapon weapon;
    expect(weapon.equip(WeaponArchetype::Carbine),
        "second firearm is acquired before drop testing");
    weapon.fixedUpdate(weaponSwitchDurationSeconds(WeaponArchetype::Carbine));
    expect(weapon.switchTo(WeaponArchetype::Pistol),
        "Pistol can become the selected drop target");
    weapon.fixedUpdate(weaponSwitchDurationSeconds(WeaponArchetype::Pistol));
    expect(weapon.tryFire() == FireReadiness::Fired,
        "selected Pistol consumes ammunition before dropping");
    const auto dropped = weapon.dropSelectedFirearm();
    expect(dropped && dropped->definition.archetype == WeaponArchetype::Pistol
            && dropped->magazineAmmo == 7U
            && weapon.definition().archetype == WeaponArchetype::Carbine
            && !weapon.ownsWeapon(WeaponArchetype::Pistol)
            && weapon.ownsWeapon(WeaponArchetype::Knife)
            && !weapon.lastWeaponArchetype()
            && validateWeaponCampaignProgress(weapon.campaignProgress()),
        "drop keeps exact ammo and selects the remaining firearm without losing Knife");
    weapon.fixedUpdate(weaponSwitchDurationSeconds(WeaponArchetype::Carbine));
    expect(!weapon.equipRecovered(WeaponArchetype::Pistol, 9U, 0U)
            && !weapon.ownsWeapon(WeaponArchetype::Pistol),
        "invalid recovered ammo cannot mutate the loadout");
    expect(weapon.equipRecovered(WeaponArchetype::Pistol,
            dropped->magazineAmmo, dropped->reserveAmmo)
            && weapon.magazineAmmo() == 7U
            && weapon.reserveAmmo() == dropped->reserveAmmo
            && weapon.ownsWeapon(WeaponArchetype::Carbine),
        "recovering the dropped Pistol restores ammo without refilling it");
}

void testKnifeOnlyLoadout()
{
    using namespace gdp::gameplay;
    Weapon weapon;
    expect(weapon.tryFire() == FireReadiness::Fired,
        "last firearm drop fixture uses one Pistol round");
    const auto dropped = weapon.dropSelectedFirearm();
    expect(dropped && dropped->definition.archetype == WeaponArchetype::Pistol
            && dropped->magazineAmmo == 7U
            && !weapon.hasFirearm()
            && weapon.definition().archetype == WeaponArchetype::Knife
            && !weapon.ownsWeapon(WeaponArchetype::Pistol)
            && weapon.ownsWeapon(WeaponArchetype::Knife)
            && !weapon.lastWeaponArchetype()
            && !weapon.switchTo(WeaponArchetype::Pistol)
            && !weapon.dropSelectedFirearm()
            && validateWeaponCampaignProgress(weapon.campaignProgress()),
        "dropping the last gun leaves only permanent Knife and no ghost slot");
    weapon.fixedUpdate(weaponSwitchDurationSeconds(WeaponArchetype::Knife));
    expect(!weapon.cycleWeapon()
            && weapon.nextWeaponArchetype() == WeaponArchetype::Knife
            && weapon.previousWeaponArchetype() == WeaponArchetype::Knife
            && weapon.addReserveAmmo(8U) == 0U
            && weapon.tryFire() == FireReadiness::Fired,
        "Knife-only loadout keeps melee usable and does not absorb ammo");
    expect(weapon.increaseDamage(4.0F)
            && weapon.increaseReserveCapacity(4U, 4U)
            && near(weapon.definition().damage, 52.0F),
        "Knife-only progression retains firepower and capacity upgrades");
    const WeaponCampaignProgress knifeOnly = weapon.campaignProgress();
    Weapon restored;
    expect(restored.restoreCampaignProgress(knifeOnly)
            && !restored.hasFirearm()
            && restored.definition().archetype == WeaponArchetype::Knife
            && !restored.ownsWeapon(WeaponArchetype::Pistol),
        "Knife-only campaign state restores without resurrecting a gun");
    expect(!restored.equipRecovered(WeaponArchetype::Pistol, 9U, 0U)
            && !restored.hasFirearm(),
        "invalid recovered ammo does not invent a gun");
    expect(restored.equipRecovered(WeaponArchetype::Pistol,
            dropped->magazineAmmo, dropped->reserveAmmo)
            && restored.hasFirearm()
            && restored.magazineAmmo() == 7U
            && near(restored.definition().damage, 38.0F)
            && restored.reserveCapacity() == 5U
            && restored.ownsWeapon(WeaponArchetype::Pistol)
            && !restored.ownsWeapon(WeaponArchetype::Carbine),
        "recovering from Knife-only restores exact ammo and only one gun");

    Weapon twoGuns;
    expect(twoGuns.equip(WeaponArchetype::Carbine),
        "two-gun fixture equips Carbine");
    twoGuns.fixedUpdate(weaponSwitchDurationSeconds(WeaponArchetype::Carbine));
    const auto first = twoGuns.dropSelectedFirearm();
    twoGuns.fixedUpdate(weaponSwitchDurationSeconds(WeaponArchetype::Pistol));
    const auto second = twoGuns.dropSelectedFirearm();
    expect(first && second
            && first->definition.archetype == WeaponArchetype::Carbine
            && second->definition.archetype == WeaponArchetype::Pistol
            && !twoGuns.hasFirearm()
            && twoGuns.definition().archetype == WeaponArchetype::Knife,
        "both firearms can be dropped in sequence");
    WeaponCampaignProgress invalid = twoGuns.campaignProgress();
    invalid.magazineAmmo = 1U;
    expect(!validateWeaponCampaignProgress(invalid),
        "Knife-only progress rejects phantom ammunition");
    invalid = twoGuns.campaignProgress();
    invalid.stowed = WeaponCampaignProgress::StowedWeapon{
        baseWeaponDefinition(WeaponArchetype::Carbine), 0U, 0U};
    expect(!validateWeaponCampaignProgress(invalid),
        "Knife-only progress rejects a phantom stowed gun");

    twoGuns.fixedUpdate(weaponSwitchDurationSeconds(WeaponArchetype::Knife));
    expect(twoGuns.equip(WeaponArchetype::Carbine)
            && twoGuns.hasFirearm()
            && twoGuns.ownsWeapon(WeaponArchetype::Carbine)
            && !twoGuns.ownsWeapon(WeaponArchetype::Pistol)
            && !twoGuns.campaignProgress().stowed,
        "a fresh firearm pickup after dropping both creates only one slot");
}

void testWeaponTriggerModes()
{
    using namespace gdp::gameplay;

    Weapon pistol({
        .fireInterval = 0.0F,
        .magazineSize = 2U,
        .reserveMaximum = 0U,
        .archetype = WeaponArchetype::Pistol,
    });
    expect(!weaponUsesAutomaticFire(WeaponArchetype::Pistol)
            && weaponFireRequested(pistol, true, true)
            && !weaponFireRequested(pistol, false, true),
        "Pistol fires on a new press but never repeats from a held trigger");

    Weapon carbine(baseWeaponDefinition(WeaponArchetype::Carbine));
    expect(weaponUsesAutomaticFire(WeaponArchetype::Carbine)
            && weaponFireRequested(carbine, true, true)
            && weaponFireRequested(carbine, false, true)
            && !weaponFireRequested(carbine, false, false),
        "Carbine continues requesting shots only while its trigger is held");
    for (std::uint32_t shot = 0U;
         shot < carbine.definition().magazineSize; ++shot) {
        carbine.fixedUpdate(carbine.definition().fireInterval);
        expect(carbine.tryFire() == FireReadiness::Fired,
            "automatic-fire fixture consumes each available cartridge");
    }
    expect(carbine.magazineAmmo() == 0U
            && !weaponFireRequested(carbine, false, true)
            && weaponFireRequested(carbine, true, true),
        "empty Carbine suppresses repeated dry fire but preserves one click per press");

    Weapon reloading(baseWeaponDefinition(WeaponArchetype::Carbine));
    expect(reloading.tryFire() == FireReadiness::Fired && reloading.reload()
            && !weaponFireRequested(reloading, true, true),
        "an active reload suppresses both pressed and held trigger requests");

    Weapon switching;
    expect(switching.equip(WeaponArchetype::Carbine)
            && !weaponFireRequested(switching, true, true)
            && !weaponFireRequested(switching, false, true),
        "weapon entry suppresses both pressed and held trigger requests");
}

void testWeaponOverdrive()
{
    using namespace gdp::gameplay;
    Weapon weapon({
        .damage = 20.0F,
        .fireInterval = 0.20F,
        .magazineSize = 3,
        .reserveMaximum = 0,
    });
    expect(weapon.activateOverdrive(2.0F) && weapon.overdriveActive()
            && near(weapon.damage(), 30.0F)
            && near(weapon.overdriveRemaining(), 2.0F),
        "Overdrive activates once and boosts weapon damage by fifty percent");
    expect(!weapon.activateOverdrive(5.0F),
        "active Overdrive rejects duplicate pickups instead of refreshing duration");
    expect(weapon.tryFire() == FireReadiness::Fired
            && near(weapon.cooldownRemaining(), 0.13F),
        "Overdrive reduces the firing interval to sixty-five percent");
    weapon.fixedUpdate(0.13F);
    expect(near(weapon.cooldownRemaining(), 0.0F) && weapon.overdriveActive(),
        "weapon cooldown and temporary effect advance on the same fixed step");
    weapon.fixedUpdate(2.0F);
    expect(!weapon.overdriveActive() && near(weapon.overdriveRemaining(), 0.0F)
            && near(weapon.damage(), 20.0F),
        "expired Overdrive restores the base weapon profile deterministically");
}

void testDamageLifecycle()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    EntityManager manager;
    const EntityId enemy = manager.createEnemy({.health = 50.0F});
    expect(applyDamage(manager, enemy, 20.0F) == DamageOutcome::Damaged,
        "non-lethal damage reports a live damaged enemy");
    EnemyData* data = manager.findData<EnemyData>(enemy);
    expect(data->health == 30.0F
            && near(data->hitReactionTime, enemyHitReactionDuration),
        "damage subtracts enemy HP and starts the transient hit reaction");
    updateEnemyHitReactions(manager, 0.05F);
    expect(near(data->hitReactionTime, enemyHitReactionDuration - 0.05F),
        "hit reaction advances once from the deterministic fixed step");
    expect(applyDamage(manager, enemy, 5.0F) == DamageOutcome::Damaged
            && near(data->hitReactionTime, enemyHitReactionDuration),
        "a repeated live hit restarts the short visual reaction window");
    updateEnemyHitReactions(manager, 1.0F);
    expect(near(data->hitReactionTime, 0.0F),
        "hit reaction expires with a zero clamp");
    updateEnemyHitReactions(manager, -1.0F);
    expect(near(data->hitReactionTime, 0.0F),
        "negative elapsed time cannot revive a hit reaction");
    data->lastKnownPlayerPosition = {2.0F, 0.0F, 3.0F};
    data->investigationTime = 1.0F;
    data->alertTime = 0.2F;
    data->recoverTime = 0.4F;
    expect(applyDamage(manager, enemy, 100.0F) == DamageOutcome::Killed,
        "overkill clamps HP and produces one kill");
    expect(data->health == 0.0F && data->behavior == EnemyBehaviorState::Dead
            && near(data->hitReactionTime, 0.0F)
            && data->lastKnownPlayerPosition == wkc::core::Vec3{}
            && near(data->investigationTime, 0.0F)
            && near(data->alertTime, 0.0F)
            && near(data->recoverTime, 0.0F),
        "lethal damage enters Dead and clears transient reaction and awareness");
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
    expect(shot.outcome == ShotOutcome::ImpactWorld && !shot.hit && shot.impact
            && shot.impact->material == ImpactMaterial::Metal
            && near(shot.impact->point.z, -10.0F)
            && manager.findData<EnemyData>(dormant)->health == 50.0F,
        "hitscan passes dormant previews and reports the actual wall impact");
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
    const CombatSignalBatch firstSignals = combatSignalsForShot(first);
    expect(firstSignals.count == 3U
            && firstSignals.signals[1].kind == CombatSignalKind::EntityHit
            && firstSignals.signals[2].kind == CombatSignalKind::EntityDamaged
            && firstSignals.signals[1].target == nearest,
        "real hitscan emits contact then damage for the selected enemy");
    expect(first.impact && first.impact->material == ImpactMaterial::Flesh
            && first.impact->point == first.hit->point
            && first.impact->normal == wkc::core::Vec3{0.0F, 0.0F, 1.0F},
        "organic enemy hits expose a positioned flesh impact to presentation");
    expect(manager.findData<EnemyData>(nearest)->health == 25.0F
            && manager.findData<EnemyData>(farther)->health == 60.0F,
        "hitscan damages only the selected target");
    weapon.fixedUpdate(0.1F);
    const ShotResult lethal = fireHitscan(
        weapon, {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, -1.0F}, 1, openRoom, manager);
    expect(lethal.outcome == ShotOutcome::Killed && lethal.hit->target == nearest,
        "lethal hitscan reports the killed target");
    const CombatSignalBatch lethalSignals = combatSignalsForShot(lethal);
    expect(lethalSignals.count == 3U
            && lethalSignals.signals[2].kind == CombatSignalKind::EntityKilled
            && lethalSignals.signals[2].target == nearest,
        "real lethal hitscan emits one death signal for the selected enemy");
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
    expect(blocked.outcome == ShotOutcome::ImpactWorld && blocked.impact
            && blocked.impact->material == ImpactMaterial::Metal
            && near(blocked.impact->point.z, -1.5F)
            && blockedManager.findData<EnemyData>(blockedEnemy)->health == 50.0F,
        "solid geometry stops a shot and reports its exact surface impact");
    const CombatSignalBatch blockedSignals = combatSignalsForShot(blocked);
    expect(blockedSignals.count == 2U
            && blockedSignals.signals[1].kind == CombatSignalKind::ImpactWorld
            && blockedSignals.signals[1].point == blocked.impact->point,
        "real blocked hitscan publishes wall contact without entity damage");

    Weapon shortRange({.fireInterval = 0.0F, .range = 1.0F});
    const ShotResult miss = fireHitscan(
        shortRange, {0.0F, 1.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, 1, openRoom, manager);
    expect(miss.outcome == ShotOutcome::Miss && !miss.impact,
        "shot with no contact inside its range reports a miss without impact");

    EntityManager robotManager;
    (void)robotManager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, -3.0F}},
        .roomId = 1,
        .archetype = EnemyArchetype::IndustrialRobot,
    });
    Weapon robotWeapon({.fireInterval = 0.0F, .range = 20.0F});
    const ShotResult robot = fireHitscan(
        robotWeapon, {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, -1.0F},
        1, openRoom, robotManager);
    expect(robot.outcome == ShotOutcome::Hit && robot.impact
            && robot.impact->material == ImpactMaterial::Metal,
        "IndustrialRobot exposes a metal impact instead of an organic one");
}

void testWeakPointDamageAndClassification()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    const CollisionWorld room{{{-10.0F, 0.0F, -10.0F}, {10.0F, 4.0F, 10.0F}}};
    EntityManager manager;
    const EntityId organic = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, -3.0F}},
        .roomId = 1,
        .health = 100.0F,
    });
    Weapon weakPointWeapon({
        .damage = 40.0F,
        .fireInterval = 0.0F,
        .range = 20.0F,
    });
    const ShotResult weakPoint = fireHitscan(
        weakPointWeapon, {0.0F, 1.62F, 0.0F}, {0.0F, 0.0F, -1.0F},
        1, room, manager);
    expect(weakPoint.outcome == ShotOutcome::Hit && weakPoint.hit
            && weakPoint.hit->target == organic
            && weakPoint.hit->zone == HitZone::WeakPoint
            && near(manager.findData<EnemyData>(organic)->health,
                100.0F - (40.0F * weakPointDamageMultiplier)),
        "head-sized enemy weak point reports its zone and applies the documented multiplier");

    EntityManager standardManager;
    const EntityId standardTarget = standardManager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, -3.0F}},
        .roomId = 1,
        .health = 100.0F,
    });
    Weapon standardWeapon({
        .damage = 40.0F,
        .fireInterval = 0.0F,
        .range = 20.0F,
    });
    const ShotResult standard = fireHitscan(
        standardWeapon, {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, -1.0F},
        1, room, standardManager);
    expect(standard.outcome == ShotOutcome::Hit && standard.hit
            && standard.hit->zone == HitZone::Standard
            && near(standardManager.findData<EnemyData>(standardTarget)->health,
                60.0F),
        "torso hit keeps the base damage and standard hit classification");

    EntityManager robotManager;
    const EntityId robot = robotManager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, -3.0F}},
        .roomId = 1,
        .archetype = EnemyArchetype::IndustrialRobot,
        .health = 100.0F,
    });
    Weapon robotWeapon({
        .damage = 40.0F,
        .fireInterval = 0.0F,
        .range = 20.0F,
    });
    const ShotResult robotCore = fireHitscan(
        robotWeapon, {0.0F, 1.48F, 0.0F}, {0.0F, 0.0F, -1.0F},
        1, room, robotManager);
    expect(robotCore.outcome == ShotOutcome::Hit && robotCore.hit
            && robotCore.hit->target == robot
            && robotCore.hit->zone == HitZone::WeakPoint
            && robotCore.impact
            && robotCore.impact->material == ImpactMaterial::Metal,
        "IndustrialRobot sensor core is a metal weak point");
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
    expect(shot.outcome == ShotOutcome::ImpactWorld && shot.impact
            && near(shot.impact->point.x, 2.0F)
            && manager.findData<EnemyData>(enemy)->health == 50.0F,
        "hitscan stops at and reports the first gap in a walkable-volume union");
    expect(!hasLineOfSight(
            {0.0F, 1.0F, 0.0F}, {6.0F, 1.0F, 0.0F}, disconnected),
        "line of sight cannot cross disconnected navigable volumes");
}

void testShotsThroughOpenCorridor()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    const CollisionWorld openPath = CollisionWorld::fromWalkableVolumes({
        {{-4.0F, 0.0F, 0.0F}, {4.0F, 4.0F, 5.0F}},
        {{-0.6F, 0.0F, -3.0F}, {0.6F, 2.2F, 0.0F}},
        {{-4.0F, 0.0F, -8.0F}, {4.0F, 4.0F, -3.0F}},
    });
    const CollisionWorld closedPath{{{-4.0F, 0.0F, 0.0F}, {4.0F, 4.0F, 5.0F}}};
    EntityManager manager;
    const EntityId enemy = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, -5.5F}},
        .roomId = 2U,
        .health = 100.0F,
    });
    constexpr std::array reachableRoom{2U};
    const wkc::core::Vec3 direction{0.0F, 0.0F, -1.0F};
    const WeaponDefinition definition{.fireInterval = 0.0F, .range = 20.0F};

    Weapon isolated(definition);
    const ShotResult withoutNeighbor = fireHitscan(isolated,
        {0.0F, 1.0F, -1.5F}, direction, 1U, openPath, manager);
    expect(!withoutNeighbor.hit && manager.findData<EnemyData>(enemy)->health == 100.0F,
        "corridor target is not damaged without an explicitly reachable room");

    Weapon openWeapon(definition);
    const ShotResult throughCorridor = fireHitscan(openWeapon,
        {0.0F, 1.0F, -1.5F}, direction, 1U, openPath, manager,
        1.0F, reachableRoom);
    expect(throughCorridor.outcome == ShotOutcome::Hit && throughCorridor.hit
            && throughCorridor.hit->target == enemy
            && manager.findData<EnemyData>(enemy)->health < 100.0F,
        "shot from a corridor damages a visible enemy in its open neighbor room");

    const float damagedHealth = manager.findData<EnemyData>(enemy)->health;
    Weapon closedWeapon(definition);
    const ShotResult throughClosedDoor = fireHitscan(closedWeapon,
        {0.0F, 1.0F, 2.0F}, direction, 1U, closedPath, manager,
        1.0F, reachableRoom);
    expect(throughClosedDoor.outcome == ShotOutcome::ImpactWorld
            && !throughClosedDoor.hit
            && manager.findData<EnemyData>(enemy)->health == damagedHealth,
        "closed geometry still blocks a cross-room shot even if room selection is stale");

    EntityManager propManager;
    const EntityId crate = propManager.createProp({
        .transform = {.position = {0.0F, 0.0F, -4.2F},
            .scale = {1.0F, 1.6F, 1.0F}},
        .roomId = 2U,
        .type = PropType::Crate,
        .solid = true,
        .health = 80.0F,
        .maximumHealth = 80.0F,
    });
    const EntityId behindCrate = propManager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, -5.5F}},
        .roomId = 2U,
        .health = 100.0F,
    });
    Weapon propWeapon(definition);
    const ShotResult propShot = fireHitscan(propWeapon,
        {0.0F, 1.0F, -1.5F}, direction, 1U, openPath, propManager,
        1.0F, reachableRoom);
    expect(propShot.hit && propShot.hit->target == crate
            && propManager.findData<EnemyData>(behindCrate)->health == 100.0F,
        "an adjacent room prop absorbs the shot before an enemy behind it");
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
        .transform = {.position = {0.0F, 0.0F, 5.0F},
            .rotationDegrees = {0.0F, 180.0F, 0.0F}},
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

void testDirectionalEnemyVision()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    expect(WorkerConfig{}.investigation.visionHalfAngleDegrees == 65.0F
            && GuardConfig{}.investigation.visionHalfAngleDegrees == 75.0F
            && OfficerConfig{}.investigation.visionHalfAngleDegrees == 80.0F
            && EngineerConfig{}.investigation.visionHalfAngleDegrees == 70.0F
            && IndustrialRobotConfig{}.investigation.visionHalfAngleDegrees
                == 85.0F,
        "every enemy role has an explicit directional vision profile");

    const CollisionWorld room{{{-8.0F, 0.0F, -8.0F}, {8.0F, 4.0F, 8.0F}}};
    const auto updateRole = [&room](const EnemyArchetype archetype,
                                    EntityManager& manager,
                                    Player& player) {
        switch (archetype) {
        case EnemyArchetype::Worker: {
            WorkerConfig config;
            config.detectionRange = 20.0F;
            config.attackDamage = 0.0F;
            return updateWorkers(manager, player, 1, room, 0.0F, config);
        }
        case EnemyArchetype::Guard: {
            GuardConfig config;
            config.detectionRange = 20.0F;
            config.attackDamage = 0.0F;
            return updateGuards(manager, player, 1, room, 0.0F, config);
        }
        case EnemyArchetype::Officer: {
            OfficerConfig config;
            config.detectionRange = 20.0F;
            config.attackDamage = 0.0F;
            config.allyCooldownReduction = 0.0F;
            return updateOfficers(manager, player, 1, room, 0.0F, config);
        }
        case EnemyArchetype::Engineer: {
            EngineerConfig config;
            config.detectionRange = 20.0F;
            config.attackDamage = 0.0F;
            config.repairAmount = 0.0F;
            return updateEngineers(manager, player, 1, room, 0.0F, config);
        }
        case EnemyArchetype::IndustrialRobot: {
            IndustrialRobotConfig config;
            config.detectionRange = 20.0F;
            config.longRangeDamage = 0.0F;
            return updateIndustrialRobots(
                manager, player, 1, room, 0.0F, config);
        }
        }
        return EnemyUpdateSummary{};
    };

    constexpr std::array archetypes{
        EnemyArchetype::Worker,
        EnemyArchetype::Guard,
        EnemyArchetype::Officer,
        EnemyArchetype::Engineer,
        EnemyArchetype::IndustrialRobot,
    };
    for (const EnemyArchetype archetype : archetypes) {
        Player player;
        player.spawn({0.0F, 0.0F, -4.0F});
        EntityManager manager;
        const EntityId enemyId = manager.createEnemy({
            .transform = {
                .position = {0.0F, 0.0F, 0.0F},
                .rotationDegrees = {0.0F, 0.0F, 0.0F},
            },
            .roomId = 1,
            .archetype = archetype,
        });
        const EnemyUpdateSummary rear = updateRole(archetype, manager, player);
        expect(manager.findData<EnemyData>(enemyId)->investigationTime == 0.0F
                && rear.chasing == 0U && rear.attacking == 0U,
            "an unaware enemy does not acquire a visible player behind it");

        (void)player.relocate({0.0F, 0.0F, 4.0F}, 0.0F);
        const EnemyUpdateSummary front = updateRole(archetype, manager, player);
        expect(manager.findData<EnemyData>(enemyId)->investigationTime > 0.0F
                && front.chasing + front.repositioning + front.attacking == 1U,
            "every role acquires a player inside its forward vision cone");
    }
}

void testFlashlightEnemyVisibility()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    const CollisionWorld open{{{-12.0F, 0.0F, -12.0F},
                                   {12.0F, 4.0F, 12.0F}}};
    const CollisionWorld blocked{
        {{-12.0F, 0.0F, -12.0F}, {12.0F, 4.0F, 12.0F}},
        {{{-1.0F, 0.0F, 3.0F}, {1.0F, 3.0F, 4.0F}}},
    };
    const auto detected = [&](const EnemyArchetype archetype,
                              const CollisionWorld& world,
                              const float playerZ,
                              const bool flashlightOn) {
        Player player;
        player.spawn({0.0F, 0.0F, playerZ});
        EntityManager manager;
        const EntityId id = manager.createEnemy({
            .transform = {
                .position = {0.0F, 0.0F, 0.0F},
                .rotationDegrees = {0.0F, 0.0F, 0.0F},
            },
            .roomId = 1,
            .archetype = archetype,
        });
        const EnemyPerception perception{.playerFlashlightOn = flashlightOn};
        switch (archetype) {
        case EnemyArchetype::Worker: {
            WorkerConfig config;
            config.detectionRange = 6.0F;
            (void)updateWorkers(manager, player, 1, world, 0.0F,
                config, perception);
            break;
        }
        case EnemyArchetype::Guard: {
            GuardConfig config;
            config.detectionRange = 6.0F;
            (void)updateGuards(manager, player, 1, world, 0.0F,
                config, perception);
            break;
        }
        case EnemyArchetype::Officer: {
            OfficerConfig config;
            config.detectionRange = 6.0F;
            (void)updateOfficers(manager, player, 1, world, 0.0F,
                config, perception);
            break;
        }
        case EnemyArchetype::Engineer: {
            EngineerConfig config;
            config.detectionRange = 6.0F;
            (void)updateEngineers(manager, player, 1, world, 0.0F,
                config, perception);
            break;
        }
        case EnemyArchetype::IndustrialRobot: {
            IndustrialRobotConfig config;
            config.detectionRange = 6.0F;
            (void)updateIndustrialRobots(manager, player, 1, world, 0.0F,
                config, perception);
            break;
        }
        }
        const EnemyData* enemy = manager.findData<EnemyData>(id);
        return enemy != nullptr && enemy->investigationTime > 0.0F;
    };

    constexpr std::array archetypes{
        EnemyArchetype::Worker,
        EnemyArchetype::Guard,
        EnemyArchetype::Officer,
        EnemyArchetype::Engineer,
        EnemyArchetype::IndustrialRobot,
    };
    for (const EnemyArchetype archetype : archetypes) {
        expect(!detected(archetype, open, 7.0F, false)
                && detected(archetype, open, 7.0F, true),
            "flashlight extends frontal acquisition without changing the dark range");
        expect(!detected(archetype, open, 8.0F, true),
            "flashlight does not grant unlimited detection range");
        expect(!detected(archetype, open, -7.0F, true),
            "flashlight does not reveal the player behind an enemy");
        expect(!detected(archetype, blocked, 7.0F, true),
            "flashlight does not reveal the player through a solid wall");
    }
}

void testDeterministicEnemyObstacleAvoidance()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    const CollisionWorld blocked{
        {{-8.0F, 0.0F, -8.0F}, {8.0F, 4.0F, 8.0F}},
        {{{-1.0F, 0.0F, 2.4F}, {1.0F, 3.0F, 3.6F}}},
    };
    const auto runFixture = [&blocked] {
        Player player;
        player.spawn({0.0F, 0.0F, 0.0F});
        EntityManager manager;
        const EntityId worker = manager.createEnemy({
            .transform = {.position = {0.0F, 0.0F, 6.0F},
                .rotationDegrees = {0.0F, 180.0F, 0.0F}},
            .roomId = 1,
            .archetype = EnemyArchetype::Worker,
            .visualSeed = 0xA11CEU,
        });
        const WorkerConfig config{
            .detectionRange = 12.0F,
            .attackRange = 1.15F,
            .moveSpeed = 1.35F,
            .attackDamage = 0.0F,
            .attackInterval = 1.0F,
            .investigation = {10.0F, 0.78F, 0.35F},
        };
        const CollisionWorld open{
            {{-8.0F, 0.0F, -8.0F}, {8.0F, 4.0F, 8.0F}}};
        (void)updateWorkers(manager, player, 1, open, 0.0F, config);
        for (std::size_t step = 0; step < 360U; ++step) {
            (void)updateWorkers(
                manager, player, 1, blocked, 1.0F / 60.0F, config);
        }
        return manager.find(worker)->transform.position;
    };

    const wkc::core::Vec3 first = runFixture();
    const wkc::core::Vec3 repeated = runFixture();
    expect(first.z < 2.0F,
        "Worker routes around an occluding obstacle instead of remaining stuck");
    expect(first == repeated,
        "obstacle avoidance reproduces the same route for the same entity seed");
}

void testDeterministicEnemyGlobalPathfinding()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    const CollisionWorld trapped{
        {{-9.0F, 0.0F, -9.0F}, {9.0F, 4.0F, 9.0F}},
        {
            {{-3.2F, 0.0F, 3.5F}, {3.2F, 3.0F, 4.0F}},
            {{-3.2F, 0.0F, 3.5F}, {-2.8F, 3.0F, 7.0F}},
            {{2.8F, 0.0F, 3.5F}, {3.2F, 3.0F, 7.0F}},
        },
    };
    const auto runFixture = [&trapped] {
        Player player;
        player.spawn({0.0F, 0.0F, 0.0F});
        EntityManager manager;
        const EntityId worker = manager.createEnemy({
            .transform = {.position = {0.0F, 0.0F, 6.0F},
                .rotationDegrees = {0.0F, 180.0F, 0.0F}},
            .roomId = 1,
            .archetype = EnemyArchetype::Worker,
            .visualSeed = 0x106U,
        });
        WorkerConfig config;
        config.detectionRange = 20.0F;
        config.attackRange = 1.15F;
        config.moveSpeed = 2.0F;
        config.attackDamage = 0.0F;
        config.investigation.duration = 20.0F;
        const CollisionWorld open{
            {{-9.0F, 0.0F, -9.0F}, {9.0F, 4.0F, 9.0F}}};
        (void)updateWorkers(manager, player, 1, open, 0.0F, config);

        bool exitedTrap = false;
        for (std::size_t tick = 0U; tick < 720U; ++tick) {
            (void)updateWorkers(
                manager, player, 1, trapped, 1.0F / 60.0F, config);
            const wkc::core::Vec3 position
                = manager.find(worker)->transform.position;
            exitedTrap = exitedTrap
                || (std::abs(position.x) > 3.55F && position.z > 3.0F);
        }
        return std::pair{manager.find(worker)->transform.position, exitedTrap};
    };

    const auto first = runFixture();
    const auto repeated = runFixture();
    expect(first.second && first.first.z < 3.2F,
        "global pathfinding leaves a U-shaped trap before approaching the target");
    expect(first == repeated,
        "global pathfinding keeps route selection deterministic");
}

void testDeterministicGroupRouteDiversity()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    const CollisionWorld blocked{
        {{-8.0F, 0.0F, -8.0F}, {8.0F, 4.0F, 8.0F}},
        {{{-2.0F, 0.0F, 2.7F}, {2.0F, 3.0F, 3.3F}}},
    };
    struct RouteResult {
        std::array<wkc::core::Vec3, 2> positions{};
        bool usedLeft{false};
        bool usedRight{false};

        constexpr bool operator==(const RouteResult&) const noexcept = default;
    };
    const auto runFixture = [&blocked] {
        Player player;
        player.spawn({0.0F, 0.0F, 0.0F});
        EntityManager manager;
        std::array<EntityId, 2> workers{
            manager.createEnemy({
                .transform = {.position = {-0.4F, 0.0F, 6.0F},
                    .rotationDegrees = {0.0F, 180.0F, 0.0F}},
                .roomId = 1,
                .archetype = EnemyArchetype::Worker,
                .visualSeed = 0x108U,
            }),
            manager.createEnemy({
                .transform = {.position = {0.4F, 0.0F, 6.0F},
                    .rotationDegrees = {0.0F, 180.0F, 0.0F}},
                .roomId = 1,
                .archetype = EnemyArchetype::Worker,
                .visualSeed = 0x108U,
            }),
        };
        WorkerConfig config;
        config.detectionRange = 20.0F;
        config.attackRange = 1.15F;
        config.moveSpeed = 2.0F;
        config.attackDamage = 0.0F;
        config.investigation.duration = 12.0F;
        const CollisionWorld open{
            {{-8.0F, 0.0F, -8.0F}, {8.0F, 4.0F, 8.0F}}};
        (void)updateWorkers(manager, player, 1, open, 0.0F, config);

        RouteResult result;
        for (std::size_t tick = 0U; tick < 300U; ++tick) {
            (void)updateWorkers(
                manager, player, 1, blocked, 1.0F / 60.0F, config);
            for (const EntityId worker : workers) {
                const float x = manager.find(worker)->transform.position.x;
                result.usedLeft = result.usedLeft || x < -2.30F;
                result.usedRight = result.usedRight || x > 2.30F;
            }
        }
        for (std::size_t index = 0U; index < workers.size(); ++index) {
            result.positions[index] = manager.find(workers[index])->transform.position;
        }
        return result;
    };

    const RouteResult first = runFixture();
    const RouteResult repeated = runFixture();
    expect(first.usedLeft && first.usedRight,
        "group pathfinding distributes equivalent routes across both obstacle sides");
    expect(first == repeated,
        "group route preferences remain deterministic for stable entity identities");
}

void testDeterministicEnemySeparation()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    const CollisionWorld room{{{-8.0F, 0.0F, -8.0F}, {8.0F, 4.0F, 8.0F}}};
    const auto runFixture = [&room] {
        Player player;
        player.spawn({0.0F, 0.0F, 0.0F});
        EntityManager manager;
        const EntityId first = manager.createEnemy({
            .transform = {.position = {0.0F, 0.0F, 6.0F}},
            .roomId = 1,
            .archetype = EnemyArchetype::Worker,
            .visualSeed = 101U,
        });
        const EntityId second = manager.createEnemy({
            .transform = {.position = {0.0F, 0.0F, 6.0F}},
            .roomId = 1,
            .archetype = EnemyArchetype::Worker,
            .visualSeed = 202U,
        });
        const WorkerConfig config{
            .detectionRange = 12.0F,
            .attackRange = 1.15F,
            .moveSpeed = 1.35F,
            .attackDamage = 0.0F,
            .attackInterval = 1.0F,
        };
        for (std::size_t step = 0; step < 360U; ++step) {
            (void)updateWorkers(
                manager, player, 1, room, 1.0F / 60.0F, config);
        }
        return std::array{
            manager.find(first)->transform.position,
            manager.find(second)->transform.position,
        };
    };

    const auto positions = runFixture();
    const auto repeated = runFixture();
    const wkc::core::Vec3 separation = positions[0] - positions[1];
    expect(wkc::core::length(
               {separation.x, 0.0F, separation.z}) >= 0.65F,
        "Workers converging on one target preserve readable personal space");
    expect(positions == repeated,
        "enemy separation reproduces exactly for the same identities and seed");
}

void testDenseEnemyVolumesDoNotCross()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    struct FixtureResult {
        std::array<wkc::core::Vec3, 8> positions{};
        bool respectedVolumes{true};
    };

    const CollisionWorld room{{{-12.0F, 0.0F, -12.0F}, {12.0F, 4.0F, 12.0F}}};
    const auto runFixture = [&room] {
        Player player;
        player.spawn({0.0F, 0.0F, 0.0F});
        EntityManager manager;
        constexpr std::array<wkc::core::Vec3, 8> starts{{
            {0.0F, 0.0F, 6.0F},
            {4.25F, 0.0F, 4.25F},
            {6.0F, 0.0F, 0.0F},
            {4.25F, 0.0F, -4.25F},
            {0.0F, 0.0F, -6.0F},
            {-4.25F, 0.0F, -4.25F},
            {-6.0F, 0.0F, 0.0F},
            {-4.25F, 0.0F, 4.25F},
        }};
        std::array<EntityId, starts.size()> enemies{};
        for (std::size_t index = 0; index < starts.size(); ++index) {
            enemies[index] = manager.createEnemy({
                .transform = {.position = starts[index]},
                .roomId = 1,
                .archetype = EnemyArchetype::Worker,
                .visualSeed = static_cast<std::uint32_t>(100U + index),
            });
        }
        const WorkerConfig config{
            .detectionRange = 12.0F,
            .attackRange = 1.15F,
            .moveSpeed = 1.35F,
            .attackDamage = 0.0F,
            .attackInterval = 1.0F,
        };
        FixtureResult result;
        for (std::size_t step = 0; step < 600U; ++step) {
            (void)updateWorkers(
                manager, player, 1, room, 1.0F / 60.0F, config);
            for (std::size_t first = 0; first < enemies.size(); ++first) {
                for (std::size_t second = first + 1; second < enemies.size(); ++second) {
                    const wkc::core::Vec3 offset
                        = manager.find(enemies[first])->transform.position
                        - manager.find(enemies[second])->transform.position;
                    if (wkc::core::length({offset.x, 0.0F, offset.z}) < 0.68F) {
                        result.respectedVolumes = false;
                    }
                }
            }
        }
        for (std::size_t index = 0; index < enemies.size(); ++index) {
            result.positions[index] = manager.find(enemies[index])->transform.position;
        }
        return result;
    };

    const FixtureResult first = runFixture();
    const FixtureResult repeated = runFixture();
    expect(first.respectedVolumes && repeated.respectedVolumes,
        "eight Workers preserve physical contact volumes throughout a dense encounter");
    expect(first.positions == repeated.positions,
        "dense enemy contact produces the same positions on repeated simulations");
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
        .transform = {.position = {0.0F, 0.0F, 10.0F},
            .rotationDegrees = {0.0F, 180.0F, 0.0F}},
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
    expect(update.chasing == 0U && update.investigating == 0U
            && update.patrolling + update.idle == 1U
            && update.attacksLanded == 0U
            && player.state().health == 100.0F,
        "unaware Guard cannot fire or track the player through geometry");
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
        .transform = {.position = {0.0F, 0.0F, 8.0F},
            .rotationDegrees = {0.0F, 180.0F, 0.0F}},
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
        .transform = {.position = {0.0F, 0.0F, 12.0F},
            .rotationDegrees = {0.0F, 180.0F, 0.0F}},
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
    expect(occluded.investigating == 1U && occluded.attacksLanded == 0U,
        "alerted Officer investigates its last sighting instead of tracking through geometry");

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
        .transform = {.position = {0.0F, 0.0F, 5.0F},
            .rotationDegrees = {0.0F, 180.0F, 0.0F}},
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
        .transform = {.position = {0.0F, 0.0F, 8.0F},
            .rotationDegrees = {0.0F, 180.0F, 0.0F}},
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

    manager.find(damagedWorker)->transform.position = {0.0F, 0.0F, 7.0F};
    EnemyData* engineerData = manager.findData<EnemyData>(engineer);
    engineerData->behavior = EnemyBehaviorState::Recover;
    engineerData->recoverTime = 0.4F;
    engineerData->supportCooldown = 0.0F;
    const EnemyUpdateSummary recovering
        = updateEngineers(manager, player, 1, blocked, 0.1F, config);
    expect(recovering.recovering == 1U && recovering.repairsPerformed == 0U
            && wkc::core::nearlyEqual(
                manager.findData<EnemyData>(damagedWorker)->health, 20.0F),
        "Engineer suspends repairs while Recover scans after a lost target");

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
        .transform = {.position = {0.0F, 0.0F, 14.0F},
            .rotationDegrees = {0.0F, 180.0F, 0.0F}},
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
    expect(update.chasing == 0U && update.investigating == 0U
            && update.patrolling + update.idle == 1U
            && update.attacksLanded == 0U
            && player.state().health == 100.0F,
        "unaware IndustrialRobot cannot attack or track through geometry");
    expect(manager.find(otherRoomRobot)->transform.position == otherPosition
            && manager.findData<EnemyData>(otherRoomRobot)->behavior
                == EnemyBehaviorState::Idle,
        "IndustrialRobot update never crosses the active room boundary");
    expect(manager.findData<EnemyData>(blockedRobot)->behavior
            != EnemyBehaviorState::Chase,
        "occluded IndustrialRobot remains unaware without a prior sighting");
}

void testEnemyFacingLifecycle()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    const CollisionWorld room{{{-12.0F, 0.0F, -12.0F}, {12.0F, 4.0F, 12.0F}}};
    Player player;
    player.spawn({0.0F, 0.0F, 0.0F});
    EntityManager manager;
    const EntityId worker = manager.createEnemy({
        .transform = {
            .position = {5.0F, 0.0F, 0.0F},
            .rotationDegrees = {0.0F, 17.0F, 0.0F},
        },
        .roomId = 1,
        .archetype = EnemyArchetype::Worker,
    });
    WorkerConfig workerConfig{
        .detectionRange = 10.0F,
        .attackRange = 1.1F,
        .moveSpeed = 1.0F,
        .attackDamage = 0.0F,
        .attackInterval = 1.0F,
    };
    workerConfig.investigation.visionHalfAngleDegrees = 180.0F;

    (void)updateWorkers(manager, player, 1, room, 0.05F, workerConfig);
    expect(near(manager.find(worker)->transform.rotationDegrees.y, -7.0F),
        "Chase advances yaw at the fixed 480-degree-per-second turn rate");

    manager.find(worker)->transform.position = {-5.0F, 0.0F, 0.0F};
    (void)updateWorkers(manager, player, 1, room, 1.0F, workerConfig);
    expect(near(manager.find(worker)->transform.rotationDegrees.y, 90.0F),
        "smoothed enemy facing converges exactly when the fixed-step budget allows it");

    manager.find(worker)->transform.position = {0.86824F, 0.0F, 4.92404F};
    manager.find(worker)->transform.rotationDegrees.y = 170.0F;
    (void)updateWorkers(manager, player, 1, room, 0.05F, workerConfig);
    expect(std::abs(std::remainder(
               manager.find(worker)->transform.rotationDegrees.y + 170.0F,
               360.0F)) < 0.05F,
        "yaw smoothing crosses the 180-degree boundary by the shortest arc");

    manager.find(worker)->transform.position = {0.0F, 0.0F, 1.0F};
    manager.find(worker)->transform.rotationDegrees.y = 23.0F;
    (void)updateWorkers(manager, player, 1, room, 0.05F, workerConfig);
    expect(near(manager.find(worker)->transform.rotationDegrees.y, 59.0F),
        "Attack uses the faster 720-degree-per-second target turn rate");
    (void)updateWorkers(manager, player, 1, room, 1.0F, workerConfig);
    expect(near(std::abs(manager.find(worker)->transform.rotationDegrees.y), 180.0F),
        "Attack converges toward the player without translating the enemy");

    player.applyDamage(1'000.0F);
    manager.find(worker)->transform.rotationDegrees.y = 42.0F;
    (void)updateWorkers(manager, player, 1, room, 1.0F, workerConfig);
    expect(near(manager.find(worker)->transform.rotationDegrees.y, 42.0F),
        "Idle preserves the last valid enemy facing");
    expect(applyDamage(manager, worker, 1'000.0F) == DamageOutcome::Killed,
        "facing fixture can enter its terminal state");
    (void)updateWorkers(manager, player, 1, room, 1.0F, workerConfig);
    expect(near(manager.find(worker)->transform.rotationDegrees.y, 42.0F),
        "Dead freezes the last valid enemy facing");

    Player retreatPlayer;
    retreatPlayer.spawn({0.0F, 0.0F, 0.0F});
    EntityManager retreatManager;
    const EntityId guard = retreatManager.createEnemy({
        .transform = {.position = {1.0F, 0.0F, 0.0F}},
        .roomId = 2,
        .archetype = EnemyArchetype::Guard,
    });
    GuardConfig retreatConfig{
        .detectionRange = 20.0F,
        .preferredMinimumRange = 4.0F,
        .preferredMaximumRange = 8.0F,
        .moveSpeed = 1.0F,
        .attackDamage = 0.0F,
    };
    retreatConfig.investigation.visionHalfAngleDegrees = 180.0F;
    (void)updateGuards(
        retreatManager, retreatPlayer, 2, room, 0.05F, retreatConfig);
    expect(retreatManager.find(guard)->transform.position.x > 1.01F
            && near(retreatManager.find(guard)->transform.rotationDegrees.y, -36.0F),
        "Reposition backs away while turning smoothly toward the player");
    (void)updateGuards(
        retreatManager, retreatPlayer, 2, room, 0.2F, retreatConfig);
    expect(near(retreatManager.find(guard)->transform.rotationDegrees.y, -90.0F),
        "Reposition converges to the target after enough deterministic time");
}

void testDeterministicLocalPatrol()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    struct PatrolResult {
        wkc::core::Vec3 position{};
        float yaw{0.0F};
        std::uint32_t waypoint{0U};
        bool moved{false};
        bool paused{false};
        bool stayedInside{true};

        constexpr bool operator==(const PatrolResult&) const noexcept = default;
    };

    const CollisionWorld room{{{-6.0F, 0.0F, -6.0F}, {6.0F, 4.0F, 6.0F}}};
    const auto runFixture = [&room](const EnemyArchetype archetype) {
        Player player;
        player.spawn({5.0F, 0.0F, 5.0F});
        EntityManager manager;
        const EntityId id = manager.createEnemy({
            .transform = {.position = {0.0F, 0.0F, 0.0F}},
            .roomId = 4,
            .archetype = archetype,
        });
        PatrolResult result;
        const PatrolConfig patrol{1.5F, 0.5F, 2.5F, 0.25F, 0.16F};
        for (std::size_t tick = 0U; tick < 240U; ++tick) {
            EnemyUpdateSummary update;
            switch (archetype) {
            case EnemyArchetype::Worker: {
                WorkerConfig config;
                config.detectionRange = 0.5F;
                config.moveSpeed = 2.0F;
                config.attackDamage = 0.0F;
                config.patrol = patrol;
                update = updateWorkers(
                    manager, player, 4, room, 1.0F / 60.0F, config);
                break;
            }
            case EnemyArchetype::Guard: {
                GuardConfig config;
                config.detectionRange = 0.5F;
                config.moveSpeed = 2.0F;
                config.attackDamage = 0.0F;
                config.patrol = patrol;
                update = updateGuards(
                    manager, player, 4, room, 1.0F / 60.0F, config);
                break;
            }
            case EnemyArchetype::Officer: {
                OfficerConfig config;
                config.detectionRange = 0.5F;
                config.moveSpeed = 2.0F;
                config.attackDamage = 0.0F;
                config.allyCooldownReduction = 0.0F;
                config.patrol = patrol;
                update = updateOfficers(
                    manager, player, 4, room, 1.0F / 60.0F, config);
                break;
            }
            case EnemyArchetype::Engineer: {
                EngineerConfig config;
                config.detectionRange = 0.5F;
                config.moveSpeed = 2.0F;
                config.attackDamage = 0.0F;
                config.repairAmount = 0.0F;
                config.patrol = patrol;
                update = updateEngineers(
                    manager, player, 4, room, 1.0F / 60.0F, config);
                break;
            }
            case EnemyArchetype::IndustrialRobot: {
                IndustrialRobotConfig config;
                config.detectionRange = 0.5F;
                config.longRangeMoveSpeed = 2.0F;
                config.longRangeDamage = 0.0F;
                config.patrol = patrol;
                update = updateIndustrialRobots(
                    manager, player, 4, room, 1.0F / 60.0F, config);
                break;
            }
            }
            result.moved = result.moved || update.patrolling == 1U;
            result.paused = result.paused
                || (update.idle == 1U
                    && manager.findData<EnemyData>(id)->patrolWaypoint > 0U);
            const wkc::core::Vec3 position = manager.find(id)->transform.position;
            result.stayedInside = result.stayedInside
                && std::abs(position.x) <= 5.66F
                && std::abs(position.z) <= 5.66F;
        }
        result.position = manager.find(id)->transform.position;
        result.yaw = manager.find(id)->transform.rotationDegrees.y;
        result.waypoint = manager.findData<EnemyData>(id)->patrolWaypoint;
        return result;
    };

    constexpr std::array archetypes{
        EnemyArchetype::Worker,
        EnemyArchetype::Guard,
        EnemyArchetype::Officer,
        EnemyArchetype::Engineer,
        EnemyArchetype::IndustrialRobot,
    };
    for (const EnemyArchetype archetype : archetypes) {
        const PatrolResult first = runFixture(archetype);
        const PatrolResult repeated = runFixture(archetype);
        expect(first == repeated && first.moved && first.paused
                && first.stayedInside && first.waypoint > 0U,
            "local patrol is bounded and deterministic for every enemy role");
    }

    Player deadPlayer;
    deadPlayer.spawn({5.0F, 0.0F, 5.0F});
    deadPlayer.applyDamage(1'000.0F);
    EntityManager manager;
    const EntityId worker = manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 0.0F}},
        .roomId = 4,
        .archetype = EnemyArchetype::Worker,
    });
    manager.findData<EnemyData>(worker)->lastKnownPlayerPosition
        = {3.0F, 0.0F, 3.0F};
    manager.findData<EnemyData>(worker)->investigationTime = 2.0F;
    manager.findData<EnemyData>(worker)->alertTime = 0.2F;
    manager.findData<EnemyData>(worker)->recoverTime = 0.4F;
    manager.findData<EnemyData>(worker)->behavior = EnemyBehaviorState::Alert;
    const EnemyUpdateSummary inactivePatrol = updateWorkers(
        manager, deadPlayer, 4, room, 1.0F,
        {.detectionRange = 0.5F, .moveSpeed = 2.0F, .attackDamage = 0.0F});
    expect(inactivePatrol.idle == 1U && inactivePatrol.patrolling == 0U
            && manager.find(worker)->transform.position
                == wkc::core::Vec3{0.0F, 0.0F, 0.0F}
            && manager.findData<EnemyData>(worker)->patrolWaypointTime == 0.0F
            && manager.findData<EnemyData>(worker)->lastKnownPlayerPosition
                == wkc::core::Vec3{}
            && manager.findData<EnemyData>(worker)->investigationTime == 0.0F
            && manager.findData<EnemyData>(worker)->alertTime == 0.0F
            && manager.findData<EnemyData>(worker)->recoverTime == 0.0F,
        "patrol and awareness remain cleared after player death");
}

void testEnemyInvestigationLifecycle()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    struct InvestigationResult {
        wkc::core::Vec3 investigationPosition{};
        wkc::core::Vec3 rememberedPosition{};
        float remainingTime{0.0F};
        bool acquired{false};
        bool investigated{false};
        bool reacquired{false};
        bool enteredRecover{false};
        bool scanned{false};
        bool recoveredIntoPatrol{false};

        constexpr bool operator==(const InvestigationResult&) const noexcept = default;
    };

    const CollisionWorld open{
        {{-8.0F, 0.0F, -8.0F}, {8.0F, 4.0F, 8.0F}}};
    const CollisionWorld blocked{
        {{-8.0F, 0.0F, -8.0F}, {8.0F, 4.0F, 8.0F}},
        {{{-1.0F, 0.0F, 3.0F}, {1.0F, 3.0F, 3.5F}}},
    };
    const auto runFixture = [&open, &blocked](const EnemyArchetype archetype) {
        Player player;
        player.spawn({0.0F, 0.0F, 0.0F});
        EntityManager manager;
        const EntityId id = manager.createEnemy({
            .transform = {.position = {0.0F, 0.0F, 6.0F},
                .rotationDegrees = {0.0F, 180.0F, 0.0F}},
            .roomId = 5,
            .archetype = archetype,
        });
        const InvestigationConfig investigation{
            2.0F, 0.75F, 0.2F, 12.0F, 0.20F, 0.60F};
        const auto updateRole = [&](const CollisionWorld& world,
                                    const float step,
                                    const float detectionRange) {
            switch (archetype) {
            case EnemyArchetype::Worker: {
                WorkerConfig config;
                config.detectionRange = detectionRange;
                config.moveSpeed = 1.0F;
                config.attackDamage = 0.0F;
                config.investigation = investigation;
                return updateWorkers(manager, player, 5, world, step, config);
            }
            case EnemyArchetype::Guard: {
                GuardConfig config;
                config.detectionRange = detectionRange;
                config.moveSpeed = 1.0F;
                config.attackDamage = 0.0F;
                config.investigation = investigation;
                return updateGuards(manager, player, 5, world, step, config);
            }
            case EnemyArchetype::Officer: {
                OfficerConfig config;
                config.detectionRange = detectionRange;
                config.moveSpeed = 1.0F;
                config.attackDamage = 0.0F;
                config.allyCooldownReduction = 0.0F;
                config.investigation = investigation;
                return updateOfficers(manager, player, 5, world, step, config);
            }
            case EnemyArchetype::Engineer: {
                EngineerConfig config;
                config.detectionRange = detectionRange;
                config.moveSpeed = 1.0F;
                config.attackDamage = 0.0F;
                config.repairAmount = 0.0F;
                config.investigation = investigation;
                return updateEngineers(manager, player, 5, world, step, config);
            }
            case EnemyArchetype::IndustrialRobot: {
                IndustrialRobotConfig config;
                config.detectionRange = detectionRange;
                config.longRangeMoveSpeed = 1.0F;
                config.pressureMoveSpeed = 1.0F;
                config.pursuitMoveSpeed = 1.0F;
                config.longRangeDamage = 0.0F;
                config.pressureDamage = 0.0F;
                config.pursuitDamage = 0.0F;
                config.investigation = investigation;
                return updateIndustrialRobots(manager, player, 5, world, step, config);
            }
            }
            return EnemyUpdateSummary{};
        };

        InvestigationResult result;
        (void)updateRole(open, 0.0F, 20.0F);
        EnemyData* enemy = manager.findData<EnemyData>(id);
        result.acquired = enemy->lastKnownPlayerPosition
                == wkc::core::Vec3{0.0F, 0.0F, 0.0F}
            && near(enemy->investigationTime, investigation.duration);

        (void)player.relocate({0.0F, 0.0F, -2.0F}, -90.0F);
        const EnemyUpdateSummary search = updateRole(blocked, 0.25F, 20.0F);
        result.investigationPosition = manager.find(id)->transform.position;
        result.rememberedPosition = enemy->lastKnownPlayerPosition;
        result.remainingTime = enemy->investigationTime;
        result.investigated = search.investigating == 1U
            && enemy->behavior == EnemyBehaviorState::Investigate
            && result.investigationPosition.z < 6.0F
            && result.rememberedPosition == wkc::core::Vec3{0.0F, 0.0F, 0.0F}
            && near(result.remainingTime, 1.75F);

        (void)player.relocate({2.0F, 0.0F, 0.0F}, -90.0F);
        (void)updateRole(open, 0.0F, 20.0F);
        result.reacquired = enemy->behavior != EnemyBehaviorState::Investigate
            && enemy->lastKnownPlayerPosition == wkc::core::Vec3{2.0F, 0.0F, 0.0F}
            && near(enemy->investigationTime, investigation.duration);

        (void)player.relocate({0.0F, 0.0F, -2.0F}, -90.0F);
        const EnemyUpdateSummary expired = updateRole(blocked, 2.1F, 0.1F);
        const wkc::core::Vec3 recoverPosition
            = manager.find(id)->transform.position;
        const float recoverYaw = manager.find(id)->transform.rotationDegrees.y;
        result.enteredRecover = expired.investigating == 0U
            && expired.recovering == 1U
            && near(enemy->investigationTime, 0.0F)
            && near(enemy->recoverTime, investigation.recoverDuration)
            && enemy->behavior == EnemyBehaviorState::Recover;
        const EnemyUpdateSummary scan = updateRole(blocked, 0.2F, 0.1F);
        result.scanned = scan.recovering == 1U
            && manager.find(id)->transform.position == recoverPosition
            && manager.find(id)->transform.rotationDegrees.y != recoverYaw
            && near(enemy->recoverTime, 0.4F)
            && enemy->behavior == EnemyBehaviorState::Recover;
        const EnemyUpdateSummary patrol = updateRole(blocked, 0.5F, 0.1F);
        result.recoveredIntoPatrol = patrol.recovering == 0U
            && patrol.patrolling + patrol.idle == 1U
            && near(enemy->recoverTime, 0.0F)
            && enemy->lastKnownPlayerPosition == wkc::core::Vec3{}
            && (enemy->behavior == EnemyBehaviorState::Patrol
                || enemy->behavior == EnemyBehaviorState::Idle);
        return result;
    };

    constexpr std::array archetypes{
        EnemyArchetype::Worker,
        EnemyArchetype::Guard,
        EnemyArchetype::Officer,
        EnemyArchetype::Engineer,
        EnemyArchetype::IndustrialRobot,
    };
    for (const EnemyArchetype archetype : archetypes) {
        const InvestigationResult first = runFixture(archetype);
        const InvestigationResult repeated = runFixture(archetype);
        expect(first == repeated && first.acquired && first.investigated
                && first.reacquired && first.enteredRecover && first.scanned
                && first.recoveredIntoPatrol,
            "investigation, Recover scan, and patrol return are deterministic for every role");
    }
}

void testDistanceAwareInvestigationWindow()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;
    const CollisionWorld open{
        {{-8.0F, 0.0F, -8.0F}, {8.0F, 4.0F, 8.0F}}};
    const CollisionWorld divided{
        {{-8.0F, 0.0F, -8.0F}, {8.0F, 4.0F, 8.0F}},
        {{{-8.0F, 0.0F, 2.7F}, {8.0F, 3.0F, 3.0F}}},
    };
    const EnemyDifficultyConfigs configs;
    const EnemyPerception noAttacks{.allowAttacks = false};
    for (const EnemyArchetype archetype : {
             EnemyArchetype::Worker, EnemyArchetype::Guard,
             EnemyArchetype::Officer, EnemyArchetype::Engineer,
             EnemyArchetype::IndustrialRobot}) {
        Player player;
        player.spawn({1.0F, 0.0F, 0.0F});
        EntityManager manager;
        const EntityId id = manager.createEnemy({
            .transform = {.position = {1.0F, 0.0F, 6.0F},
                .rotationDegrees = {0.0F, 180.0F, 0.0F}},
            .roomId = 5U,
            .archetype = archetype,
        });
        InvestigationConfig investigation;
        float moveSpeed = 0.0F;
        const auto updateRole = [&](const CollisionWorld& world,
                                    const float step) {
            switch (archetype) {
            case EnemyArchetype::Worker:
                return updateWorkers(
                    manager, player, 5U, world, step, configs.worker, noAttacks);
            case EnemyArchetype::Guard:
                return updateGuards(
                    manager, player, 5U, world, step, configs.guard, noAttacks);
            case EnemyArchetype::Officer:
                return updateOfficers(
                    manager, player, 5U, world, step, configs.officer, noAttacks);
            case EnemyArchetype::Engineer:
                return updateEngineers(
                    manager, player, 5U, world, step, configs.engineer, noAttacks);
            case EnemyArchetype::IndustrialRobot:
                return updateIndustrialRobots(manager, player, 5U, world,
                    step, configs.industrialRobot, noAttacks);
            }
            return EnemyUpdateSummary{};
        };
        switch (archetype) {
        case EnemyArchetype::Worker:
            investigation = configs.worker.investigation;
            moveSpeed = configs.worker.moveSpeed;
            break;
        case EnemyArchetype::Guard:
            investigation = configs.guard.investigation;
            moveSpeed = configs.guard.moveSpeed;
            break;
        case EnemyArchetype::Officer:
            investigation = configs.officer.investigation;
            moveSpeed = configs.officer.moveSpeed;
            break;
        case EnemyArchetype::Engineer:
            investigation = configs.engineer.investigation;
            moveSpeed = configs.engineer.moveSpeed;
            break;
        case EnemyArchetype::IndustrialRobot:
            investigation = configs.industrialRobot.investigation;
            moveSpeed = configs.industrialRobot.longRangeMoveSpeed;
            break;
        }
        const float estimatedTravel = (6.0F - investigation.arrivalDistance)
            / (moveSpeed * investigation.speedScale);
        const float expectedWindow = investigation.duration + std::clamp(
            estimatedTravel - investigation.duration,
            0.0F, investigation.maximumTravelBonus);
        (void)updateRole(open, 0.0F);
        EnemyData* enemy = manager.findData<EnemyData>(id);
        const bool rememberedSight = near(enemy->investigationTime, expectedWindow)
            && expectedWindow > investigation.duration
            && expectedWindow <= investigation.duration
                + investigation.maximumTravelBonus
            && enemy->lastKnownPlayerPosition
                == wkc::core::Vec3{1.0F, 0.0F, 0.0F};
        const bool rememberedNoise = alertEnemiesToNoise(
            manager, 5U, {{1.0F, 0.0F, 0.0F}, 20.0F}, configs) == 1U
            && near(enemy->investigationTime, expectedWindow);
        (void)player.relocate({4.0F, 0.0F, -30.0F}, -90.0F);
        const EnemyUpdateSummary searching = updateRole(
            divided, investigation.duration + 0.25F);
        const bool stillSearching = searching.investigating == 1U
            && enemy->investigationTime > 0.0F
            && enemy->lastKnownPlayerPosition
                == wkc::core::Vec3{1.0F, 0.0F, 0.0F};
        const EnemyUpdateSummary expired = updateRole(
            divided, investigation.maximumTravelBonus + 1.0F);
        expect(rememberedSight && rememberedNoise && stillSearching
                && expired.recovering == 1U
                && near(enemy->investigationTime, 0.0F),
            "each role gets bounded travel time for sightings and noise, then gives up");
    }

    Player nearbyPlayer;
    nearbyPlayer.spawn({0.0F, 0.0F, 0.0F});
    EntityManager nearbyManager;
    const EntityId nearby = nearbyManager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 1.0F},
            .rotationDegrees = {0.0F, 180.0F, 0.0F}},
        .roomId = 5U,
        .archetype = EnemyArchetype::Worker,
    });
    (void)updateWorkers(nearbyManager, nearbyPlayer, 5U, open, 0.0F,
        configs.worker, noAttacks);
    expect(near(nearbyManager.findData<EnemyData>(nearby)->investigationTime,
            configs.worker.investigation.duration),
        "nearby sightings retain the original short investigation window");

    for (const EnemyArchetype archetype : {
             EnemyArchetype::Worker, EnemyArchetype::IndustrialRobot}) {
        Player player;
        player.spawn({1.0F, 0.0F, 0.0F});
        EntityManager manager;
        const float startZ = archetype == EnemyArchetype::Worker
            ? 6.0F : 5.0F;
        const float arrivalDistance = archetype == EnemyArchetype::Worker
            ? configs.worker.investigation.arrivalDistance
            : configs.industrialRobot.investigation.arrivalDistance;
        const EntityId id = manager.createEnemy({
            .transform = {.position = {1.0F, 0.0F, startZ},
                .rotationDegrees = {0.0F, 180.0F, 0.0F}},
            .roomId = 5U,
            .archetype = archetype,
        });
        const auto updateRole = [&](const float step) {
            if (archetype == EnemyArchetype::Worker) {
                return updateWorkers(manager, player, 5U, open, step,
                    configs.worker, noAttacks);
            }
            return updateIndustrialRobots(manager, player, 5U, open, step,
                configs.industrialRobot, noAttacks);
        };
        (void)updateRole(0.0F);
        const float window = manager.findData<EnemyData>(id)->investigationTime;
        (void)player.relocate({4.0F, 0.0F, -30.0F}, -90.0F);
        bool reachedSighting = false;
        const int ticks = static_cast<int>(std::ceil(window * 60.0F)) + 2;
        for (int tick = 0; tick < ticks; ++tick) {
            (void)updateRole(1.0F / 60.0F);
            const wkc::core::Vec3 position = manager.find(id)->transform.position;
            reachedSighting |= std::hypot(position.x - 1.0F, position.z)
                <= arrivalDistance + 0.1F;
        }
        expect(reachedSighting,
            "Worker and boss can physically reach a nearby last sighting before forgetting it");
    }
}

void testEnemySearchWithoutHiddenPlayerTracking()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    const CollisionWorld open{
        {{-8.0F, 0.0F, -8.0F}, {8.0F, 4.0F, 8.0F}}};
    const CollisionWorld blocked{
        {{-8.0F, 0.0F, -8.0F}, {8.0F, 4.0F, 8.0F}},
        {{{-7.0F, 0.0F, -2.0F}, {7.0F, 3.0F, -1.8F}}},
    };
    const auto runFixture = [&](const EnemyArchetype archetype,
                                const float hiddenX) {
        EntityManager manager;
        Player player;
        player.spawn({1.0F, 0.0F, 0.0F});
        const EntityId id = manager.createEnemy({
            .transform = {.position = {1.0F, 0.0F, 1.5F},
                .rotationDegrees = {0.0F, 180.0F, 0.0F}},
            .roomId = 5,
            .archetype = archetype,
        });
        const InvestigationConfig investigation{
            2.0F, 0.75F, 0.2F, 12.0F, 0.0F, 3.0F, 90.0F, 1.0F};
        const auto updateRole = [&](const CollisionWorld& world,
                                    const float step) {
            switch (archetype) {
            case EnemyArchetype::Worker: {
                WorkerConfig config;
                config.moveSpeed = 1.5F;
                config.attackDamage = 0.0F;
                config.investigation = investigation;
                return updateWorkers(manager, player, 5, world, step, config);
            }
            case EnemyArchetype::Guard: {
                GuardConfig config;
                config.moveSpeed = 1.5F;
                config.attackDamage = 0.0F;
                config.investigation = investigation;
                return updateGuards(manager, player, 5, world, step, config);
            }
            case EnemyArchetype::Officer: {
                OfficerConfig config;
                config.moveSpeed = 1.5F;
                config.attackDamage = 0.0F;
                config.allyCooldownReduction = 0.0F;
                config.investigation = investigation;
                return updateOfficers(manager, player, 5, world, step, config);
            }
            case EnemyArchetype::Engineer: {
                EngineerConfig config;
                config.moveSpeed = 1.5F;
                config.attackDamage = 0.0F;
                config.repairAmount = 0.0F;
                config.investigation = investigation;
                return updateEngineers(manager, player, 5, world, step, config);
            }
            case EnemyArchetype::IndustrialRobot: {
                IndustrialRobotConfig config;
                config.longRangeMoveSpeed = 1.5F;
                config.pressureMoveSpeed = 1.5F;
                config.pursuitMoveSpeed = 1.5F;
                config.longRangeDamage = 0.0F;
                config.pressureDamage = 0.0F;
                config.pursuitDamage = 0.0F;
                config.investigation = investigation;
                return updateIndustrialRobots(manager, player, 5, world, step, config);
            }
            }
            return EnemyUpdateSummary{};
        };

        (void)updateRole(open, 0.0F);
        (void)player.relocate({hiddenX, 0.0F, -3.0F}, -90.0F);
        (void)updateRole(blocked, 2.1F);
        const wkc::core::Vec3 start = manager.find(id)->transform.position;
        const EnemyUpdateSummary first = updateRole(blocked, 0.5F);
        const wkc::core::Vec3 firstPosition = manager.find(id)->transform.position;
        (void)updateRole(blocked, 0.5F);
        (void)updateRole(blocked, 1.0F);
        const wkc::core::Vec3 secondPosition = manager.find(id)->transform.position;
        const EnemyData* enemy = manager.findData<EnemyData>(id);
        const bool searched = first.recovering == 1U
            && enemy->behavior == EnemyBehaviorState::Recover
            && firstPosition != start && secondPosition != firstPosition
            && enemy->lastKnownPlayerPosition
                == wkc::core::Vec3{1.0F, 0.0F, 0.0F};
        (void)updateRole(blocked, 1.1F);
        const bool stopped = enemy->recoverTime == 0.0F
            && enemy->lastKnownPlayerPosition == wkc::core::Vec3{};
        return std::tuple{firstPosition, secondPosition, searched, stopped};
    };

    for (const EnemyArchetype archetype : {
             EnemyArchetype::Worker, EnemyArchetype::Guard,
             EnemyArchetype::Officer, EnemyArchetype::Engineer,
             EnemyArchetype::IndustrialRobot}) {
        const auto first = runFixture(archetype, 1.0F);
        const auto relocated = runFixture(archetype, 4.0F);
        expect(first == relocated && std::get<2>(first) && std::get<3>(first),
            "all roles search the remembered area deterministically without tracking a hidden player");
    }
}

void testEnemySearchRejectsUnreachableFlanks()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;
    const CollisionWorld divided{
        {{-5.0F, 0.0F, -5.0F}, {5.0F, 4.0F, 5.0F}},
        {{{-5.0F, 0.0F, -0.25F}, {5.0F, 3.0F, 0.25F}}},
    };
    EntityManager manager;
    Player player;
    player.spawn({1.0F, 0.0F, -2.0F});
    const EntityId worker = manager.createEnemy({
        .transform = {.position = {1.0F, 0.0F, 1.5F},
            .rotationDegrees = {0.0F, 180.0F, 0.0F}},
        .roomId = 1U,
        .archetype = EnemyArchetype::Worker,
    });
    EnemyData* enemy = manager.findData<EnemyData>(worker);
    enemy->lastKnownPlayerPosition = {1.0F, 0.0F, -1.0F};
    enemy->recoverTime = 3.0F;
    WorkerConfig config;
    config.investigation.recoverDuration = 3.0F;
    config.investigation.searchRadius = 1.0F;
    const wkc::core::Vec3 start = manager.find(worker)->transform.position;
    const EnemyUpdateSummary update = updateWorkers(
        manager, player, 1U, divided, 0.5F, config);
    expect(update.recovering == 1U
            && enemy->behavior == EnemyBehaviorState::Recover
            && manager.find(worker)->transform.position == start
            && enemy->lastKnownPlayerPosition
                == wkc::core::Vec3{1.0F, 0.0F, -1.0F},
        "search scans instead of walking into a wall when no flank is reachable");
}

void testEnemyNoisePerception()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;

    struct NoiseResult {
        wkc::core::Vec3 investigationPosition{};
        wkc::core::Vec3 rememberedPosition{};
        float remainingTime{0.0F};
        bool heard{false};
        bool alerted{false};
        bool repeatedNoiseDidNotRestartAlert{false};
        bool investigated{false};
        bool visionOverrodeNoise{false};

        constexpr bool operator==(const NoiseResult&) const noexcept = default;
    };

    const CollisionWorld open{
        {{-8.0F, 0.0F, -8.0F}, {8.0F, 4.0F, 8.0F}}};
    const CollisionWorld blocked{
        {{-8.0F, 0.0F, -8.0F}, {8.0F, 4.0F, 8.0F}},
        {{{-1.5F, 0.0F, 2.5F}, {1.5F, 3.0F, 3.5F}}},
    };
    const auto runFixture = [&open, &blocked](const EnemyArchetype archetype) {
        Player player;
        player.spawn({0.0F, 0.0F, 0.0F});
        EntityManager manager;
        const EntityId id = manager.createEnemy({
            .transform = {.position = {0.0F, 0.0F, 6.0F}},
            .roomId = 5,
            .archetype = archetype,
        });
        EnemyDifficultyConfigs configs;
        configs.worker.investigation = {2.0F, 0.75F, 0.2F, 12.0F, 0.20F};
        configs.guard.investigation = configs.worker.investigation;
        configs.officer.investigation = configs.worker.investigation;
        configs.engineer.investigation = configs.worker.investigation;
        configs.industrialRobot.investigation = configs.worker.investigation;

        NoiseResult result;
        result.heard = alertEnemiesToNoise(
            manager, 5, {.position = player.state().position, .radius = 12.0F},
            configs) == 1U;

        const auto updateRole = [&](const CollisionWorld& world, const float step) {
            switch (archetype) {
            case EnemyArchetype::Worker:
                configs.worker.detectionRange = 20.0F;
                configs.worker.moveSpeed = 1.0F;
                configs.worker.attackDamage = 0.0F;
                return updateWorkers(
                    manager, player, 5, world, step, configs.worker);
            case EnemyArchetype::Guard:
                configs.guard.detectionRange = 20.0F;
                configs.guard.moveSpeed = 1.0F;
                configs.guard.attackDamage = 0.0F;
                return updateGuards(
                    manager, player, 5, world, step, configs.guard);
            case EnemyArchetype::Officer:
                configs.officer.detectionRange = 20.0F;
                configs.officer.moveSpeed = 1.0F;
                configs.officer.attackDamage = 0.0F;
                configs.officer.allyCooldownReduction = 0.0F;
                return updateOfficers(
                    manager, player, 5, world, step, configs.officer);
            case EnemyArchetype::Engineer:
                configs.engineer.detectionRange = 20.0F;
                configs.engineer.moveSpeed = 1.0F;
                configs.engineer.attackDamage = 0.0F;
                configs.engineer.repairAmount = 0.0F;
                return updateEngineers(
                    manager, player, 5, world, step, configs.engineer);
            case EnemyArchetype::IndustrialRobot:
                configs.industrialRobot.detectionRange = 20.0F;
                configs.industrialRobot.longRangeMoveSpeed = 1.0F;
                configs.industrialRobot.pressureMoveSpeed = 1.0F;
                configs.industrialRobot.pursuitMoveSpeed = 1.0F;
                configs.industrialRobot.longRangeDamage = 0.0F;
                configs.industrialRobot.pressureDamage = 0.0F;
                configs.industrialRobot.pursuitDamage = 0.0F;
                return updateIndustrialRobots(
                    manager, player, 5, world, step,
                    configs.industrialRobot);
            }
            return EnemyUpdateSummary{};
        };

        const wkc::core::Vec3 initialPosition
            = manager.find(id)->transform.position;
        const EnemyUpdateSummary alert = updateRole(blocked, 0.05F);
        EnemyData* enemy = manager.findData<EnemyData>(id);
        result.alerted = alert.alerting == 1U
            && enemy->behavior == EnemyBehaviorState::Alert
            && manager.find(id)->transform.position == initialPosition
            && near(enemy->alertTime, 0.15F)
            && near(std::abs(manager.find(id)->transform.rotationDegrees.y), 36.0F);

        const float alertTimeBeforeRepeatedNoise = enemy->alertTime;
        result.repeatedNoiseDidNotRestartAlert = alertEnemiesToNoise(
            manager, 5,
            {.position = {1.0F, 0.0F, 0.0F}, .radius = 12.0F},
            configs) == 1U
            && near(enemy->alertTime, alertTimeBeforeRepeatedNoise)
            && enemy->lastKnownPlayerPosition
                == wkc::core::Vec3{1.0F, 0.0F, 0.0F};

        const EnemyUpdateSummary search = updateRole(blocked, 0.16F);
        result.investigationPosition = manager.find(id)->transform.position;
        result.rememberedPosition = enemy->lastKnownPlayerPosition;
        result.remainingTime = enemy->investigationTime;
        result.investigated = search.investigating == 1U
            && enemy->behavior == EnemyBehaviorState::Investigate
            && result.investigationPosition.z < 6.0F
            && result.investigationPosition.x > 0.0F
            && result.rememberedPosition == wkc::core::Vec3{1.0F, 0.0F, 0.0F}
            && near(result.remainingTime, 1.84F);

        (void)player.relocate({3.0F, 0.0F, 0.0F}, -90.0F);
        (void)updateRole(open, 0.0F);
        result.visionOverrodeNoise = enemy->lastKnownPlayerPosition
                == wkc::core::Vec3{3.0F, 0.0F, 0.0F}
            && near(enemy->investigationTime, 2.0F)
            && enemy->behavior != EnemyBehaviorState::Investigate;
        return result;
    };

    constexpr std::array archetypes{
        EnemyArchetype::Worker,
        EnemyArchetype::Guard,
        EnemyArchetype::Officer,
        EnemyArchetype::Engineer,
        EnemyArchetype::IndustrialRobot,
    };
    for (const EnemyArchetype archetype : archetypes) {
        const NoiseResult first = runFixture(archetype);
        const NoiseResult repeated = runFixture(archetype);
        expect(first == repeated && first.heard && first.alerted
                && first.repeatedNoiseDidNotRestartAlert && first.investigated
                && first.visionOverrodeNoise,
            "combat noise produces bounded Alert then deterministic investigation for every role");
    }

    EntityManager isolated;
    const EntityId nearby = isolated.createEnemy({
        .transform = {.position = {5.0F, 0.0F, 0.0F}},
        .roomId = 7,
        .archetype = EnemyArchetype::Worker,
    });
    const EntityId tooFar = isolated.createEnemy({
        .transform = {.position = {7.0F, 0.0F, 0.0F}},
        .roomId = 7,
        .archetype = EnemyArchetype::Worker,
    });
    const EntityId otherRoom = isolated.createEnemy({
        .transform = {.position = {5.0F, 0.0F, 0.0F}},
        .roomId = 8,
        .archetype = EnemyArchetype::Worker,
    });
    const EntityId dormant = isolated.createEnemy({
        .transform = {.position = {5.0F, 0.0F, 0.0F}},
        .roomId = 7,
        .archetype = EnemyArchetype::Worker,
        .active = false,
    });
    const EntityId dead = isolated.createEnemy({
        .transform = {.position = {5.0F, 0.0F, 0.0F}},
        .roomId = 7,
        .archetype = EnemyArchetype::Worker,
    });
    isolated.findData<EnemyData>(dead)->behavior = EnemyBehaviorState::Dead;
    EnemyDifficultyConfigs limited;
    limited.worker.investigation = {2.0F, 0.75F, 0.2F, 6.0F, 0.20F};
    const std::size_t heard = alertEnemiesToNoise(
        isolated, 7, {.position = {}, .radius = 8.0F}, limited);
    expect(heard == 1U
            && near(isolated.findData<EnemyData>(nearby)->investigationTime, 2.0F)
            && near(isolated.findData<EnemyData>(tooFar)->investigationTime, 0.0F)
            && near(isolated.findData<EnemyData>(otherRoom)->investigationTime, 0.0F)
            && near(isolated.findData<EnemyData>(dormant)->investigationTime, 0.0F)
            && near(isolated.findData<EnemyData>(dead)->investigationTime, 0.0F),
        "noise respects hearing range, active room, dormancy, and death");
    expect(alertEnemiesToNoise(
               isolated, 7,
               {.position = {},
                   .radius = std::numeric_limits<float>::quiet_NaN()},
               limited) == 0U,
        "invalid noise stimuli are ignored safely");

    Player hiddenPlayer;
    hiddenPlayer.spawn({0.0F, 0.0F, 0.0F});
    EntityManager recoveringManager;
    const EntityId recoveringWorker = recoveringManager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 6.0F}},
        .roomId = 5,
        .archetype = EnemyArchetype::Worker,
    });
    EnemyData* recovering
        = recoveringManager.findData<EnemyData>(recoveringWorker);
    recovering->behavior = EnemyBehaviorState::Recover;
    recovering->recoverTime = 0.4F;
    EnemyDifficultyConfigs resumeConfigs;
    resumeConfigs.worker.detectionRange = 20.0F;
    resumeConfigs.worker.moveSpeed = 1.0F;
    resumeConfigs.worker.attackDamage = 0.0F;
    resumeConfigs.worker.investigation
        = {2.0F, 0.75F, 0.2F, 12.0F, 0.20F, 0.60F};
    expect(alertEnemiesToNoise(
               recoveringManager, 5,
               {.position = hiddenPlayer.state().position, .radius = 12.0F},
               resumeConfigs) == 1U
            && recovering->recoverTime == 0.0F
            && recovering->alertTime == 0.0F,
        "new noise interrupts Recover without repeating Alert");
    const EnemyUpdateSummary resumed = updateWorkers(
        recoveringManager, hiddenPlayer, 5, blocked, 0.1F,
        resumeConfigs.worker);
    expect(resumed.investigating == 1U && resumed.alerting == 0U
            && recovering->behavior == EnemyBehaviorState::Investigate,
        "interrupted Recover resumes investigation immediately");
}

void testExpandedWeaponProfilesAndInventory()
{
    using namespace gdp::gameplay;
    const auto sniperType = WeaponArchetype::Sniper;
    expect(near(advanceWeaponAimBlend(0.0F, 0.175F, sniperType, true), 0.5F)
        && near(advanceWeaponAimBlend(0.5F, 0.175F, sniperType, true), 1.0F)
        && near(advanceWeaponAimBlend(1.0F, 0.175F, sniperType, false), 0.5F),
        "sniper aim preserves its 0.35 second entry and release transition");
    expect(advanceWeaponAimBlend(1.0F, 0.01F, sniperType, true, true) == 0.0F
        && advanceWeaponAimBlend(1.0F, 0.01F, WeaponArchetype::Knife, true) == 0.0F,
        "blocked aim or knife immediately removes stale zoom");
    expect(advanceWeaponAimBlend(0.5F, -1.0F, sniperType, true) == 0.5F
        && advanceWeaponAimBlend(0.5F, std::numeric_limits<float>::infinity(), sniperType, true) == 0.5F
        && advanceWeaponAimBlend(std::numeric_limits<float>::quiet_NaN(), 0.0F, sniperType, true) == 0.0F,
        "aim transition sanitizes invalid clocks and stale non-finite state");
    expect(near(weaponAimFovScale(sniperType), 0.35F)
        && near(weaponAimDurationSeconds(sniperType), 0.35F), "scope adds no extra zoom");
    for (const auto type : {WeaponArchetype::Sniper, WeaponArchetype::Shotgun, WeaponArchetype::SubmachineGun}) {
        Weapon weapon;
        expect(weapon.equip(type), "new primary equips beside starter pistol");
        weapon.fixedUpdate(1.0F);
        expect(weapon.archetypeForSlot(1U) == type && weapon.ownsWeapon(WeaponArchetype::Pistol)
            && !weapon.equip(WeaponArchetype::Carbine), "slot one selects owned primary and cannot silently replace it");
        expect(weapon.tryFire() == FireReadiness::Fired && weapon.tryFire() == FireReadiness::Cooldown,
            "new weapons consume ammo once and enforce their fire interval");
        const auto progress = weapon.campaignProgress();
        expect(weapon.switchTo(WeaponArchetype::Pistol) && weapon.switchTo(WeaponArchetype::Knife)
            && weapon.previousWeaponArchetype() == WeaponArchetype::Pistol
            && weapon.nextWeaponArchetype() == type && weapon.switchTo(type),
            "new primary participates in rapid scroll/knife/last-weapon navigation");
        weapon.fixedUpdate(1.0F);
        expect(weapon.magazineAmmo() == progress.magazineAmmo && weapon.reload(),
            "switching does not refill the primary and timed reload begins");
        weapon.fixedUpdate(weaponReloadDurationSeconds(type));
        const auto saved = weapon.campaignProgress();
        Weapon restored;
        expect(restored.restoreCampaignProgress(saved) && restored.campaignProgress() == saved,
            "new primary ammo and pistol survive campaign restore exactly");
        expect(restored.dropSelectedFirearm().has_value() && restored.ownsWeapon(WeaponArchetype::Pistol)
            && !restored.ownsWeapon(type), "dropping a new primary preserves secondary");
        restored.fixedUpdate(1.0F);
        expect(restored.equipRecovered(type, saved.magazineAmmo, saved.reserveAmmo),
            "new primary can be recovered without regenerating its ammunition");
        expect(weaponUsesAutomaticFire(type) == (type == WeaponArchetype::SubmachineGun),
            "only SMG supports held-trigger fire among the new weapons");
    }
    Weapon shotgun(baseWeaponDefinition(WeaponArchetype::Shotgun));
    expect(shotgun.increaseDamage(12.0F)
        && near((shotgun.damage() - baseWeaponDefinition(WeaponArchetype::Shotgun).damage) * 6.0F, 12.0F),
        "shotgun damage upgrade is shared across pellets, not multiplied six times");
    const auto dropped = shotgun.dropSelectedFirearm();
    shotgun.fixedUpdate(1.0F);
    expect(dropped && shotgun.equip(WeaponArchetype::Sniper)
        && near(shotgun.damage() - baseWeaponDefinition(WeaponArchetype::Sniper).damage, 12.0F),
        "damage bonus transfers between shotgun and single-projectile primary without amplification");
    expect(weaponAimFovScale(WeaponArchetype::Sniper) < weaponAimFovScale(WeaponArchetype::Carbine)
        && weaponAimDurationSeconds(WeaponArchetype::Sniper) > weaponAimDurationSeconds(WeaponArchetype::Carbine),
        "sniper has stronger optical zoom and a slower aim transition");
    expect(!isFirearmArchetype(WeaponArchetype::Knife)
        && !isFirearmArchetype(static_cast<WeaponArchetype>(255U)), "invalid weapon IDs never become guns");
    Weapon slow;
    (void)slow.equip(WeaponArchetype::Sniper); slow.fixedUpdate(1.0F);
    expect(slow.tryFire() == FireReadiness::Fired, "sniper accepts first shot");
    (void)slow.switchTo(WeaponArchetype::Pistol);
    slow.fixedUpdate(weaponSwitchDurationSeconds(WeaponArchetype::Pistol));
    (void)slow.switchTo(WeaponArchetype::Sniper);
    slow.fixedUpdate(weaponSwitchDurationSeconds(WeaponArchetype::Sniper));
    expect(slow.tryFire() == FireReadiness::Cooldown,
        "rapid secondary/primary switching cannot bypass sniper bolt cooldown");
}

void testShotgunPelletsAndOcclusion()
{
    using namespace gdp::gameplay;
    using namespace gdp::entities;
    const CollisionWorld open{{{-30.0F, 0.0F, -100.0F}, {30.0F, 5.0F, 100.0F}}};
    const auto fireAt = [&](float distance) {
        EntityManager manager;
        const auto enemy = manager.createEnemy({.transform = {.position = {0.0F, 0.0F, distance}},
            .roomId = 1U, .health = 1000.0F});
        Weapon weapon(baseWeaponDefinition(WeaponArchetype::Shotgun));
        const auto shot = fireHitscan(weapon, {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, 1U, open, manager);
        const auto signals = combatSignalsForShot(shot);
        expect(weapon.magazineAmmo() == 5U && shot.extraContactCount == 5U
            && std::count_if(signals.signals.begin(), signals.signals.begin() + signals.count,
                [](const auto& signal) { return signal.kind == CombatSignalKind::WeaponFired; }) == 1,
            "six pellets consume one shell and emit one firing event");
        return 1000.0F - manager.findData<EnemyData>(enemy)->health;
    };
    const float nearDamage = fireAt(2.0F);
    const float farDamage = fireAt(12.0F);
    expect(nearDamage > farDamage && farDamage >= 0.0F && nearDamage <= 84.0F,
        "shotgun dispersion and damage falloff favor close range");
    expect(near(nearDamage, fireAt(2.0F)), "shotgun pattern and damage repeat deterministically");
    EntityManager manager;
    const auto enemy = manager.createEnemy({.transform = {.position = {0.0F, 0.0F, 3.0F}},
        .roomId = 1U, .health = 1.0F});
    Weapon weapon(baseWeaponDefinition(WeaponArchetype::Shotgun));
    const auto lethal = combatSignalsForShot(fireHitscan(weapon,
        {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, 1U, open, manager));
    expect(manager.findData<EnemyData>(enemy)->health == 0.0F
        && std::count_if(lethal.signals.begin(), lethal.signals.begin() + lethal.count,
            [](const auto& signal) { return signal.kind == CombatSignalKind::EntityKilled; }) == 1,
        "several pellets cannot credit the same death multiple times");
    EntityManager group;
    std::array<EntityId, 3> victims{};
    for (std::size_t i = 0; i < victims.size(); ++i) {
        victims[i] = group.createEnemy({.transform = {.position = {0.0F, 0.0F, 2.0F + float(i)}},
            .roomId = 1U, .health = 1.0F});
    }
    Weapon groupGun(baseWeaponDefinition(WeaponArchetype::Shotgun));
    const auto groupSignals = combatSignalsForShot(fireHitscan(groupGun,
        {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, 1U, open, group, 0.0F));
    for (const auto victim : victims) {
        expect(std::count_if(groupSignals.signals.begin(), groupSignals.signals.begin() + groupSignals.count,
            [victim](const auto& signal) {
                return signal.kind == CombatSignalKind::EntityKilled && signal.target == victim;
            }) == 1, "each pellet victim receives exactly one death signal");
    }
    ShotResult bounded{ShotOutcome::Hit, HitResult{enemy}, {}, std::nullopt};
    bounded.extraContacts.fill({ShotOutcome::Hit, HitResult{enemy}, {}, std::nullopt});
    bounded.extraContactCount = 255U;
    expect(combatSignalsForShot(bounded).count == 13U,
        "signal batching clamps malformed contact counts to its six-contact capacity");
    EntityManager blockedManager;
    const auto blocked = blockedManager.createEnemy({.transform = {.position = {0.0F, 0.0F, 3.0F}},
        .roomId = 2U, .health = 1000.0F});
    const CollisionWorld wall{open.roomBounds(), {{{-30.0F, 0.0F, 1.0F}, {30.0F, 5.0F, 1.2F}}}};
    Weapon blockedGun(baseWeaponDefinition(WeaponArchetype::Shotgun));
    const std::array<std::uint32_t, 1> neighbors{2U};
    (void)fireHitscan(blockedGun, {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, 1U, wall, blockedManager, 1.0F, neighbors);
    expect(blockedManager.findData<EnemyData>(blocked)->health == 1000.0F,
        "each shotgun pellet respects cover/closed-door solids even for neighboring rooms");
}

} // namespace

int main()
{
    testCombatSignalBatch();
    testExpandedWeaponProfilesAndInventory();
    testShotgunPelletsAndOcclusion();
    testEnemyDifficultyConfigs();
    testWeaponLifecycle();
    testDiscardedMagazineReloads();
    testPerShellShotgunReload();
    testDeterministicWeaponSpread();
    testWeaponCameraRecoil();
    testPracticeRestock();
    testWeaponPickupEquipment();
    testKnifeFallback();
    testDirectionalAndLastWeaponSelection();
    testDropAndRecoverFirearm();
    testKnifeOnlyLoadout();
    testWeaponTriggerModes();
    testWeaponOverdrive();
    testDamageLifecycle();
    testDormantEnemyIgnoresCombat();
    testHitscanSelectionAndBlocking();
    testWeakPointDamageAndClassification();
    testWalkableUnionBlocksShotsAcrossDisconnectedSpace();
    testShotsThroughOpenCorridor();
    testWorkerBehavior();
    testWorkerLineOfSight();
    testDirectionalEnemyVision();
    testFlashlightEnemyVisibility();
    testDeterministicEnemyObstacleAvoidance();
    testDeterministicEnemyGlobalPathfinding();
    testDeterministicGroupRouteDiversity();
    testDeterministicEnemySeparation();
    testDenseEnemyVolumesDoNotCross();
    testGuardRangedBehavior();
    testGuardLineOfSight();
    testOfficerCommandSupport();
    testOfficerRangedBehaviorAndLineOfSight();
    testEngineerRepairSystem();
    testEngineerRangedBehaviorAndRepairOcclusion();
    testIndustrialRobotPhases();
    testIndustrialRobotLineOfSightAndRoomIsolation();
    testEnemyFacingLifecycle();
    testDeterministicLocalPatrol();
    testEnemyInvestigationLifecycle();
    testDistanceAwareInvestigationWindow();
    testEnemySearchWithoutHiddenPlayerTracking();
    testEnemySearchRejectsUnreachableFlanks();
    testEnemyNoisePerception();

    if (failures != 0) {
        std::cerr << failures << " combat test(s) failed\n";
        return 1;
    }

    std::cout << "All GDP combat tests passed\n";
    return 0;
}
