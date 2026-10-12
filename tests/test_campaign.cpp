#include "gameplay/Campaign.hpp"
#include "gameplay/Session.hpp"
#include "gameplay/RoomEvents.hpp"
#include "world/EventGenerator.hpp"
#include "world/WorldEntities.hpp"
#include "world/WorldGenerator.hpp"

#include <algorithm>
#include <array>
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

void testCampaignCarryoverRoundTrip()
{
    using namespace gdp::gameplay;

    Player sourcePlayer;
    sourcePlayer.applyDamage(30.0F);
    sourcePlayer.addKeys(2U);
    Weapon sourceWeapon;
    ProgressionSystem sourceProgression;
    (void)sourceProgression.record(
        ProgressionEvent::IndustrialRobotKilled,
        3U,
        sourcePlayer,
        sourceWeapon);
    expect(sourceWeapon.equip(WeaponArchetype::Carbine),
        "campaign fixture equips the discovered Carbine");
    sourceWeapon.fixedUpdate(
        weaponSwitchDurationSeconds(WeaponArchetype::Carbine));
    sourcePlayer.applyDamage(15.0F);
    expect(sourceWeapon.tryFire() == FireReadiness::Fired,
        "campaign fixture spends the first magazine round");
    sourceWeapon.fixedUpdate(1.0F);
    expect(sourceWeapon.tryFire() == FireReadiness::Fired,
        "campaign fixture spends the second magazine round");
    expect(sourceWeapon.activateOverdrive(),
        "campaign fixture activates a temporary Arena Overdrive");
    expect(sourceWeapon.switchTo(WeaponArchetype::Pistol),
        "campaign fixture begins a transient slot change");
    sourceWeapon.fixedUpdate(
        weaponSwitchDurationSeconds(WeaponArchetype::Pistol));
    expect(sourceWeapon.switchTo(WeaponArchetype::Carbine)
            && sourceWeapon.switching(),
        "campaign fixture captures an active Carbine entry");

    const CampaignCarryover carryover = captureCampaignCarryover(
        sourcePlayer, sourceWeapon, sourceProgression);
    expect(validateCampaignCarryover(carryover),
        "live victorious gameplay state produces a valid Campaign snapshot");

    Player restoredPlayer;
    restoredPlayer.spawn({12.0F, 0.0F, -8.0F}, 90.0F);
    Weapon restoredWeapon;
    ProgressionSystem restoredProgression;
    expect(restoreCampaignCarryover(
            carryover, restoredPlayer, restoredWeapon, restoredProgression),
        "valid Campaign snapshot restores into the next floor systems");

    expect(near(restoredPlayer.state().position.x, 12.0F)
            && near(restoredPlayer.state().position.z, -8.0F)
            && near(restoredPlayer.state().yawDegrees, 90.0F),
        "carry-over preserves the new floor spawn transform");
    expect(near(restoredPlayer.state().health, sourcePlayer.state().health)
            && near(restoredPlayer.state().maximumHealth,
                sourcePlayer.state().maximumHealth)
            && near(restoredPlayer.state().armor, sourcePlayer.state().armor)
            && near(restoredPlayer.state().maximumArmor,
                sourcePlayer.state().maximumArmor)
            && restoredPlayer.state().keys == sourcePlayer.state().keys,
        "health, armor, capacities and keys survive the floor boundary");
    expect(near(restoredWeapon.definition().damage,
                sourceWeapon.definition().damage)
            && restoredWeapon.definition().archetype
                == WeaponArchetype::Carbine
            && near(restoredWeapon.definition().fireInterval,
                sourceWeapon.definition().fireInterval)
            && near(restoredWeapon.definition().range,
                sourceWeapon.definition().range)
            && restoredWeapon.definition().magazineSize
                == sourceWeapon.definition().magazineSize
            && restoredWeapon.reserveCapacity()
                == sourceWeapon.reserveCapacity()
            && restoredWeapon.magazineAmmo() == sourceWeapon.magazineAmmo()
            && restoredWeapon.reserveAmmo() == sourceWeapon.reserveAmmo()
            && restoredWeapon.campaignProgress()
                == sourceWeapon.campaignProgress()
            && restoredWeapon.ownsWeapon(WeaponArchetype::Pistol)
            && near(restoredWeapon.cooldownRemaining(), 0.0F)
            && !restoredWeapon.reloading()
            && !restoredWeapon.switching()
            && !restoredWeapon.overdriveActive(),
        "both weapon slots persist while transient combat state resets");
    expect(restoredProgression.state().level
                == sourceProgression.state().level
            && restoredProgression.state().experienceIntoLevel
                == sourceProgression.state().experienceIntoLevel
            && restoredProgression.state().totalExperience
                == sourceProgression.state().totalExperience
            && restoredProgression.state().score
                == sourceProgression.state().score
            && restoredProgression.state().enemiesKilled
                == sourceProgression.state().enemiesKilled
            && restoredProgression.state().vitalityRank
                == sourceProgression.state().vitalityRank
            && restoredProgression.state().firepowerRank
                == sourceProgression.state().firepowerRank
            && restoredProgression.state().capacityRank
                == sourceProgression.state().capacityRank,
        "level, XP, score, counters and reward ranks survive the floor boundary");

    restoredPlayer.applyDamage(20.0F);
    restoredWeapon.fixedUpdate(1.0F);
    (void)restoredWeapon.tryFire();
    expect(restoreCampaignCarryover(
            carryover, restoredPlayer, restoredWeapon, restoredProgression)
            && near(restoredPlayer.state().health, sourcePlayer.state().health)
            && restoredWeapon.magazineAmmo() == sourceWeapon.magazineAmmo(),
        "restoring the floor-entry snapshot discards resources spent on a replay");
}

void testCampaignCarryoverRejectsInvalidStateAtomically()
{
    using namespace gdp::gameplay;

    Player sourcePlayer;
    Weapon sourceWeapon;
    ProgressionSystem sourceProgression;
    const CampaignCarryover valid = captureCampaignCarryover(
        sourcePlayer, sourceWeapon, sourceProgression);

    Player destinationPlayer;
    destinationPlayer.spawn({3.0F, 0.0F, 4.0F}, 45.0F);
    destinationPlayer.applyDamage(10.0F);
    Weapon destinationWeapon;
    ProgressionSystem destinationProgression;
    const PlayerState playerBefore = destinationPlayer.state();

    CampaignCarryover invalidPlayer = valid;
    invalidPlayer.player.health = std::numeric_limits<float>::quiet_NaN();
    expect(!validateCampaignCarryover(invalidPlayer)
            && !restoreCampaignCarryover(
                invalidPlayer,
                destinationPlayer,
                destinationWeapon,
                destinationProgression)
            && near(destinationPlayer.state().health, playerBefore.health),
        "invalid player data is rejected before mutating destination systems");

    CampaignCarryover invalidAmmo = valid;
    invalidAmmo.weapon.magazineAmmo
        = invalidAmmo.weapon.definition.magazineSize + 1U;
    expect(!validateCampaignCarryover(invalidAmmo),
        "magazine values above capacity invalidate Campaign carry-over");

    CampaignCarryover invalidWeapon = valid;
    invalidWeapon.weapon.definition.archetype
        = static_cast<WeaponArchetype>(99U);
    expect(!validateCampaignCarryover(invalidWeapon),
        "unknown weapon identity invalidates Campaign carry-over");

    CampaignCarryover duplicateSlot = valid;
    duplicateSlot.weapon.stowed = WeaponCampaignProgress::StowedWeapon{
        duplicateSlot.weapon.definition, 0U, 0U};
    expect(!validateCampaignCarryover(duplicateSlot),
        "Campaign carry-over rejects duplicate weapon identities across slots");

    CampaignCarryover invalidStowedAmmo = valid;
    invalidStowedAmmo.weapon.stowed = WeaponCampaignProgress::StowedWeapon{
        baseWeaponDefinition(WeaponArchetype::Carbine), 13U, 0U};
    expect(!validateCampaignCarryover(invalidStowedAmmo),
        "Campaign carry-over validates stowed ammunition against its own capacity");

    CampaignCarryover invalidProgression = valid;
    invalidProgression.progression.vitalityRank = 1U;
    expect(!validateCampaignCarryover(invalidProgression),
        "inconsistent progression ranks invalidate Campaign carry-over");
}

void testNextFloorGrenadeResupply()
{
    using namespace gdp::gameplay;
    Player player;
    Weapon weapon;
    ProgressionSystem progression;
    const CampaignCarryover full = captureCampaignCarryover(
        player, weapon, progression);
    expect(full.player.grenades == 2U
            && prepareNextCampaignFloorCarryover(full).player.grenades == 2U,
        "a full grenade inventory cannot exceed its two-grenade capacity");

    expect(player.useGrenade() && player.useGrenade(),
        "campaign fixture spends both starting grenades");
    const CampaignCarryover empty = captureCampaignCarryover(
        player, weapon, progression);
    const CampaignCarryover nextFloor = prepareNextCampaignFloorCarryover(empty);
    expect(empty.player.grenades == 0U && nextFloor.player.grenades == 1U
            && validateCampaignCarryover(nextFloor),
        "advancing a floor grants one grenade without changing the captured state");
    expect(prepareNextCampaignFloorCarryover(nextFloor).player.grenades == 2U,
        "a later floor can restore the second grenade without exceeding capacity");

    Player replayed;
    Weapon replayedWeapon;
    ProgressionSystem replayedProgression;
    expect(restoreCampaignCarryover(
            nextFloor, replayed, replayedWeapon, replayedProgression)
            && replayed.state().grenades == 1U,
        "replaying the next floor restores its entry stock without another grant");
}

void testCampaignCheckpointValidation()
{
    using namespace gdp::gameplay;
    Player player;
    Weapon weapon;
    ProgressionSystem progression;
    constexpr wkc::core::Seed initialSeed = 0x43414D505F534156ULL;
    constexpr std::uint32_t seedIndex = 1U;
    CampaignCheckpoint checkpoint{
        currentCampaignRulesVersion,
        initialSeed,
        seedForRun(initialSeed, seedIndex),
        2U,
        seedIndex,
        2U,
        captureCampaignCarryover(player, weapon, progression),
    };
    expect(validateCampaignCheckpoint(checkpoint),
        "a deterministic floor-entry snapshot is a valid checkpoint");

    checkpoint.currentSeed += 1U;
    expect(!validateCampaignCheckpoint(checkpoint),
        "checkpoint seed must match its deterministic sequence index");
    checkpoint.currentSeed = seedForRun(initialSeed, seedIndex);
    checkpoint.floor = campaignFloorCount + 1U;
    expect(!validateCampaignCheckpoint(checkpoint),
        "checkpoint floor cannot exceed the Campaign length");
    checkpoint.floor = 2U;
    checkpoint.rulesVersion += 1U;
    expect(!validateCampaignCheckpoint(checkpoint),
        "checkpoint rules version must match the running game");
}

void testCampaignDifficultyProgression()
{
    using namespace gdp::gameplay;
    expect(near(campaignDifficultyMultiplier(1U), 1.0F),
        "Campaign floor one preserves the original gameplay balance");
    expect(near(campaignDifficultyMultiplier(2U), 1.08F)
            && near(campaignDifficultyMultiplier(3U), 1.16F),
        "Campaign threat increases by eight percentage points on each later floor");
    expect(near(campaignDifficultyMultiplier(0U), 1.0F)
            && near(campaignDifficultyMultiplier(99U), 1.72F),
        "Campaign threat clamps invalid floor numbers safely");
}

void testCampaignMissionDecks()
{
    using namespace gdp::gameplay;
    bool sawDifferentSequence = false;
    std::array<CampaignMissionKind, campaignFloorCount> firstSequence{};
    for (wkc::core::Seed seed = 1U; seed <= 1000U; ++seed) {
        std::array<bool, 4> seen{};
        CampaignMissionKind previous = CampaignMissionKind::SecureArena;
        for (std::uint32_t floor = 1U; floor <= campaignFloorCount; ++floor) {
            const auto kind = campaignMissionKind(seed, floor);
            expect(kind == campaignMissionKind(seed, floor),
                "mission deck repeats deterministically for a campaign seed");
            if (floor == 1U) {
                expect(kind == CampaignMissionKind::SecureArena,
                    "first floor keeps the introductory Arena mission");
            } else {
                expect(kind != previous,
                    "adjacent campaign floors never repeat the same mission");
                seen[static_cast<std::size_t>(kind)] = true;
                if (floor == 5U || floor == 9U) {
                    expect(std::all_of(seen.begin(), seen.end(), [](const bool value) {
                            return value;
                        }), "each four-floor deck contains all mission types");
                    seen.fill(false);
                }
            }
            if (seed == 1U) firstSequence[floor - 1U] = kind;
            else sawDifferentSequence |= firstSequence[floor - 1U] != kind;
            previous = kind;
        }
    }
    expect(sawDifferentSequence, "campaign seeds produce different mission orders");
}

void testRecoverAndSecureExitAndCheckpoint()
{
    using namespace gdp::gameplay;
    constexpr wkc::core::Seed initialSeed = 0x524F5554455F5445ULL;
    constexpr std::uint32_t secretRoom = 11U;
    constexpr std::uint32_t arenaRoom = 22U;
    const auto routeFloor = [&] {
        for (std::uint32_t floor = 2U; floor <= 9U; ++floor) {
            if (campaignMissionKind(initialSeed, floor)
                == CampaignMissionKind::RecoverAndSecure) return floor;
        }
        return 0U;
    }();
    expect(routeFloor != 0U, "the fixture has a RecoverAndSecure floor");
    if (routeFloor == 0U) return;

    Player player;
    Weapon weapon;
    ProgressionSystem progression;
    const CampaignCheckpoint checkpoint{
        .initialSeed = initialSeed,
        .currentSeed = seedForRun(initialSeed, routeFloor - 1U),
        .runIndex = routeFloor - 1U,
        .seedIndex = routeFloor - 1U,
        .floor = routeFloor,
        .floorEntry = captureCampaignCarryover(player, weapon, progression),
    };
    expect(validateCampaignCheckpoint(checkpoint),
        "the route floor has a valid deterministic entry checkpoint");

    for (const bool keyFirst : {true, false}) {
        for (const bool bossFirst : {true, false}) {
            SessionSystem session{0U};
            expect(session.restoreCampaignCheckpoint(checkpoint.initialSeed,
                    checkpoint.currentSeed, checkpoint.runIndex,
                    checkpoint.seedIndex, checkpoint.floor, checkpoint.difficulty),
                "checkpoint resumes the selected route floor");
            CampaignMissionState mission{campaignMissionKind(
                session.state().initialSeed, session.state().campaignFloor),
                secretRoom};
            mission.secondaryRoomId = arenaRoom;

            if (bossFirst) {
                (void)session.update(true, gdp::world::RoomRole::Boss,
                    true, false, false, true, mission.completed);
                expect(!session.state().campaignExitReady
                        && session.state().phase == SessionPhase::Playing,
                    "boss-first terminal interaction cannot bypass the route mission");
            }
            const auto firstRoom = keyFirst ? secretRoom : arenaRoom;
            const auto secondRoom = keyFirst ? arenaRoom : secretRoom;
            expect(!advanceCampaignMission(mission, firstRoom,
                    keyFirst ? CampaignMissionProgress{.keysCollected = 1U}
                             : CampaignMissionProgress{.arenaCleared = true})
                    && !mission.completed
                    && campaignMissionPendingRoom(mission) == secondRoom,
                "the first route objective remains incomplete and points to the other room");
            (void)session.update(true, gdp::world::RoomRole::Boss,
                true, !bossFirst, false, true, mission.completed);
            expect(!session.state().campaignExitReady
                    && session.state().phase == SessionPhase::Playing,
                "the exit remains blocked with one route objective pending");
            expect(advanceCampaignMission(mission, secondRoom,
                    keyFirst ? CampaignMissionProgress{.arenaCleared = true}
                             : CampaignMissionProgress{.keysCollected = 1U})
                    && mission.completed,
                "the route completes in either order without repeating a pickup or event");
            (void)session.update(true, gdp::world::RoomRole::Boss,
                true, !bossFirst, false, !bossFirst, mission.completed);
            expect(session.state().campaignExitReady == bossFirst
                    && session.state().phase == SessionPhase::Playing,
                "a boss killed before the mission unlocks now; a living boss still blocks");
            if (!bossFirst) {
                (void)session.update(true, gdp::world::RoomRole::Boss,
                    true, false, false, false, mission.completed);
                expect(session.state().campaignExitReady
                        && session.state().phase == SessionPhase::Playing,
                    "boss death after the mission unlocks without redoing the objectives");
            }
            expect(session.update(true, gdp::world::RoomRole::Boss,
                    true, false, false, true, mission.completed).phase
                    == SessionPhase::Victory,
                "a new terminal use completes either ordering of mission and boss");

            // A checkpoint is the floor entry, so mission progress is lost.
            SessionSystem resumed{0U};
            expect(resumed.restoreCampaignCheckpoint(checkpoint.initialSeed,
                    checkpoint.currentSeed, checkpoint.runIndex,
                    checkpoint.seedIndex, checkpoint.floor, checkpoint.difficulty)
                    && resumed.state().currentSeed == session.state().currentSeed,
                "continue restores the same floor and world seed");
            CampaignMissionState freshMission{campaignMissionKind(
                resumed.state().initialSeed, resumed.state().campaignFloor),
                secretRoom};
            freshMission.secondaryRoomId = arenaRoom;
            expect(!freshMission.completed && !freshMission.keyRecovered
                    && !freshMission.arenaSecured
                    && campaignMissionPendingRoom(freshMission) == secretRoom
                    && !resumed.state().bossEngaged
                    && !resumed.state().campaignExitReady,
                "continue resets mission, boss and exit progress to floor entry");
            expect(!advanceCampaignMission(freshMission, secondRoom,
                    keyFirst ? CampaignMissionProgress{.arenaCleared = true}
                             : CampaignMissionProgress{.keysCollected = 1U})
                    && !freshMission.completed,
                "the prior attempt's first objective does not survive the checkpoint");
            expect(advanceCampaignMission(freshMission, firstRoom,
                    keyFirst ? CampaignMissionProgress{.keysCollected = 1U}
                             : CampaignMissionProgress{.arenaCleared = true})
                    && freshMission.completed,
                "both objectives can complete after either checkpoint order");
            (void)resumed.update(true, gdp::world::RoomRole::Boss,
                true, true, false, true, freshMission.completed);
            expect(!resumed.state().campaignExitReady
                    && resumed.state().phase == SessionPhase::Playing,
                "completed mission alone cannot pass a living boss");
            (void)resumed.update(true, gdp::world::RoomRole::Boss,
                true, false, false, false, freshMission.completed);
            expect(resumed.state().campaignExitReady
                    && resumed.state().phase == SessionPhase::Playing,
                "both objectives and boss death unlock the exit without auto advancing");
            const auto victory = resumed.update(true, gdp::world::RoomRole::Boss,
                true, false, false, true, freshMission.completed);
            expect(victory.changed && victory.phase == SessionPhase::Victory,
                "a fresh terminal interaction advances after all three gates");
        }
    }
}

void testCampaignFloorVarietyAndMissions()
{
    using namespace gdp;
    bool sawSeven = false, sawTen = false, sawLoop = false, sawNoLoop = false;
    bool sawKey = false, sawArena = false, sawTerminal = false, sawRoute = false;
    for (wkc::core::Seed initialSeed = 1U; initialSeed <= 24U; ++initialSeed) {
        for (std::uint32_t floor = 1U; floor <= gameplay::campaignFloorCount; ++floor) {
            const auto floorSeed = gameplay::seedForRun(initialSeed, floor - 1U);
            const auto layout = gameplay::campaignFloorLayout(floorSeed, floor);
            expect(layout == gameplay::campaignFloorLayout(floorSeed, floor)
                    && layout.roomCount >= 7U && layout.roomCount <= 10U
                    && layout.loopBudget <= 1U,
                "floor profile is deterministic and bounded");
            sawSeven |= layout.roomCount == 7U;
            sawTen |= layout.roomCount == 10U;
            sawLoop |= layout.loopBudget == 1U;
            sawNoLoop |= layout.loopBudget == 0U;
            auto generated = world::generateWorld(floorSeed,
                {.roomCount = layout.roomCount, .loopBudget = layout.loopBudget,
                    .maximumAttempts = 64U});
            if (!generated.world) {
                std::cerr << "Campaign map failed seed=" << initialSeed
                    << " floor=" << floor << " rooms=" << layout.roomCount
                    << " loops=" << layout.loopBudget << '\n';
                expect(false, "every sampled campaign floor generates a valid map");
                continue;
            }
            expect(generated.world->rooms.size() == layout.roomCount
                    && world::validateWorld(*generated.world).isValid(),
                "campaign floor geometry and corridors validate");
            const auto kind = gameplay::campaignMissionKind(initialSeed, floor);
            sawKey |= kind == gameplay::CampaignMissionKind::RecoverKey;
            sawArena |= kind == gameplay::CampaignMissionKind::SecureArena;
            sawTerminal |= kind == gameplay::CampaignMissionKind::SecureTerminal;
            sawRoute |= kind == gameplay::CampaignMissionKind::RecoverAndSecure;
            const auto role = (kind == gameplay::CampaignMissionKind::RecoverKey
                    || kind == gameplay::CampaignMissionKind::RecoverAndSecure)
                ? world::RoomRole::Secret
                : kind == gameplay::CampaignMissionKind::SecureTerminal
                    ? world::RoomRole::Start : world::RoomRole::Arena;
            const auto target = std::find_if(generated.world->rooms.begin(),
                generated.world->rooms.end(), [role](const auto& room) {
                    return room.role == role;
                });
            expect(target != generated.world->rooms.end(),
                "mission target always exists on a generated floor");
            if (target == generated.world->rooms.end()) continue;
            auto mission = gameplay::CampaignMissionState{kind, target->id};
            expect(!gameplay::advanceCampaignMission(mission, target->id + 99U,
                        {.keysCollected = 1U, .arenaCleared = true,
                            .terminalActivated = true, .terminalAmbushCleared = true})
                    && !gameplay::advanceCampaignMission(mission, target->id, {}),
                "another room and prior-floor keys cannot satisfy the mission");
            if (kind == gameplay::CampaignMissionKind::RecoverKey
                || kind == gameplay::CampaignMissionKind::RecoverAndSecure) {
                entities::EntityManager entities;
                auto populated = world::populateInitialEntities(*generated.world, entities,
                    {.transform = {.position = {0.0F, 0.0F, 4.0F}}, .health = 100.0F},
                    1.0F, true);
                bool hasKey = false;
                for (const auto id : target->entities) {
                    const auto* item = entities.findData<entities::ItemData>(id);
                    hasKey |= item && item->type == entities::ItemType::Key;
                }
                expect(populated.isValid() && hasKey,
                    "key objective has a real collectible in the reachable Secret room");
                if (kind == gameplay::CampaignMissionKind::RecoverKey) {
                    expect(gameplay::advanceCampaignMission(mission, target->id,
                                {.keysCollected = 1U})
                        && !gameplay::advanceCampaignMission(mission, target->id,
                                {.keysCollected = 1U}),
                        "only a current-floor key pickup completes the key mission once");
                } else {
                    const auto arena = std::find_if(generated.world->rooms.begin(),
                        generated.world->rooms.end(), [](const auto& room) {
                            return room.role == world::RoomRole::Arena;
                        });
                    expect(arena != generated.world->rooms.end(),
                        "route mission has a reachable Arena");
                    if (arena == generated.world->rooms.end()) continue;
                    mission.secondaryRoomId = arena->id;
                    auto reverse = mission;
                    expect(!gameplay::advanceCampaignMission(mission, target->id,
                                {.keysCollected = 1U})
                            && mission.keyRecovered && !mission.completed
                            && gameplay::campaignMissionPendingRoom(mission) == arena->id
                            && !gameplay::advanceCampaignMission(mission, arena->id,
                                {.keysCollected = 1U}),
                        "key alone does not complete route mission or accept a key in Arena");
                    expect(gameplay::advanceCampaignMission(mission, arena->id,
                                {.arenaCleared = true}) && mission.completed,
                        "Arena clear after key completes route mission");
                    expect(!gameplay::advanceCampaignMission(reverse, arena->id,
                                {.arenaCleared = true})
                            && reverse.arenaSecured && !reverse.completed
                            && gameplay::campaignMissionPendingRoom(reverse) == target->id
                            && gameplay::advanceCampaignMission(reverse, target->id,
                                {.keysCollected = 1U}),
                        "Arena-first route remains completable after collecting key");
                }
            } else if (kind == gameplay::CampaignMissionKind::SecureArena) {
                expect(gameplay::advanceCampaignMission(mission, target->id,
                            {.arenaCleared = true})
                    && !gameplay::advanceCampaignMission(mission, target->id,
                            {.arenaCleared = true}),
                    "Arena lockdown completion satisfies its mission exactly once");
            } else {
                entities::EntityManager entities;
                const auto populated = world::populateInitialEntities(
                    *generated.world, entities,
                    {.transform = {.position = {0.0F, 0.0F, 4.0F}}, .health = 100.0F},
                    1.0F, true);
                const auto node = std::find_if(generated.world->graph.nodes.begin(),
                    generated.world->graph.nodes.end(), [target](const auto& candidate) {
                        return candidate.id == target->id;
                    });
                expect(populated.isValid() && entities.find(populated.terminal)
                        && node != generated.world->graph.nodes.end(),
                    "terminal objective has a real interactable in Start");
                if (node == generated.world->graph.nodes.end()) continue;
                const auto plan = world::generateRoomEventPlan(target->seed,
                    target->role, target->theme, node->depth, target->enemyZones, 1.0F);
                expect(plan.type == world::RoomEventType::TerminalAmbush
                        && plan.trigger == world::RoomEventTrigger::TerminalUse
                        && world::validateRoomEventPlan(plan, target->enemyZones).isValid(),
                    "terminal objective has a valid ambush event gated by interaction");
                auto runtime = gameplay::makeRoomEventRuntime(plan, target->id);
                expect(!gameplay::updateRoomEvent(runtime, entities, true, false,
                        1.0F / 60.0F).activated,
                    "entering Start alone cannot complete the terminal mission");
                expect(!gameplay::advanceCampaignMission(mission, target->id,
                        {.terminalAmbushCleared = true}) && !mission.activated,
                    "ambush completion without terminal activation cannot satisfy the mission");
                const auto activated = gameplay::updateRoomEvent(runtime, entities,
                    true, true, 1.0F / 60.0F);
                expect(activated.activated && !gameplay::advanceCampaignMission(
                        mission, target->id, {.terminalActivated = activated.activated})
                        && mission.activated && !mission.completed,
                    "using the Start terminal activates but does not finish the mission");
                for (const auto id : entities.query(entities::EntityKind::Enemy)) {
                    if (auto* enemy = entities.findData<entities::EnemyData>(id)) {
                        enemy->active = false;
                        enemy->behavior = entities::EnemyBehaviorState::Dead;
                    }
                }
                const auto completed = gameplay::updateRoomEvent(runtime, entities,
                    true, false, 1.0F / 60.0F);
                expect(completed.completed && gameplay::advanceCampaignMission(
                        mission, target->id,
                        {.terminalAmbushCleared = completed.completed}),
                    "clearing the activated ambush completes the terminal mission");
            }
        }
    }
    expect(sawSeven && sawTen && sawLoop && sawNoLoop && sawKey && sawArena
            && sawTerminal && sawRoute,
        "campaign seeds cover multiple floor sizes, graph cycles and mission types");
}

} // namespace

int main()
{
    testCampaignCarryoverRoundTrip();
    testCampaignCarryoverRejectsInvalidStateAtomically();
    testNextFloorGrenadeResupply();
    testCampaignCheckpointValidation();
    testCampaignDifficultyProgression();
    testCampaignMissionDecks();
    testRecoverAndSecureExitAndCheckpoint();
    testCampaignFloorVarietyAndMissions();
    if (failures != 0) {
        std::cerr << failures << " campaign test(s) failed\n";
        return 1;
    }
    std::cout << "Campaign carry-over tests passed\n";
    return 0;
}
