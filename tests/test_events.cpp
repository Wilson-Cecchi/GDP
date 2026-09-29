#include "gameplay/Combat.hpp"
#include "gameplay/RoomEvents.hpp"
#include "world/EventGenerator.hpp"
#include "world/WorldGenerator.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <unordered_set>

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

bool containsError(
    const gdp::world::EventValidationResult& result,
    const gdp::world::EventValidationError error)
{
    return std::find(result.errors.begin(), result.errors.end(), error)
        != result.errors.end();
}

constexpr std::array eventZones{
    gdp::world::SpawnZone{{{-2.0F, 0.0F, -1.0F}, {2.0F, 0.0F, 1.0F}}},
};

void testDeterministicEventPlan()
{
    using namespace gdp::world;
    constexpr wkc::core::Seed seed = 0x4556454E545F5631ULL;
    const RoomEventPlan first = generateRoomEventPlan(
        seed, RoomRole::Start, RoomTheme::Industrial, 0, eventZones);
    const RoomEventPlan repeated = generateRoomEventPlan(
        seed, RoomRole::Start, RoomTheme::Industrial, 0, eventZones);
    const RoomEventPlan different = generateRoomEventPlan(
        seed + 1U, RoomRole::Start, RoomTheme::Industrial, 0, eventZones);
    expect(validateRoomEventPlan(first, eventZones).isValid(),
        "representative terminal ambush plan validates");
    expect(first.spawns == repeated.spawns && first.planHash == repeated.planHash,
        "same room inputs reproduce event type, wave, enemies and reward");
    expect(first.planHash != different.planHash,
        "different room seed changes the canonical event plan");
    constexpr std::uint64_t goldenHash = 0xB04A437A8394B962ULL;
    expect(first.planHash == goldenHash,
        "golden procedural event hash remains stable across builds");
    expect(first.type == RoomEventType::TerminalAmbush
            && first.trigger == RoomEventTrigger::TerminalUse
            && first.locksRoom && first.requiresRoomClear
            && first.waveCount == 1U && first.spawns.size() == 1U,
        "START event is a deliberate one-wave terminal ambush");

    RoomEventPlan invalid = first;
    invalid.trigger = RoomEventTrigger::RoomEntry;
    expect(containsError(
               validateRoomEventPlan(invalid, eventZones),
               EventValidationError::InvalidTrigger),
        "event validation rejects a trigger incompatible with its type");
    invalid = first;
    invalid.spawns.front().wave = 3U;
    expect(containsError(
               validateRoomEventPlan(invalid, eventZones),
               EventValidationError::InvalidSpawn),
        "event validation rejects a spawn outside the declared wave range");
    invalid = first;
    invalid.planHash ^= 1U;
    expect(containsError(
               validateRoomEventPlan(invalid, eventZones),
               EventValidationError::InvalidPlanHash),
        "event validation detects canonical hash corruption");
}

void testEventPlanBatchAndWorlds()
{
    using namespace gdp::world;
    std::unordered_set<std::uint64_t> hashes;
    std::array<bool, static_cast<std::size_t>(RoomEventType::Count)> found{};
    bool allValid = true;
    for (wkc::core::Seed seed = 0; seed < 1'000; ++seed) {
        const RoomRole role = static_cast<RoomRole>(seed % 7U);
        const RoomTheme theme = static_cast<RoomTheme>((seed / 7U) % 7U);
        const RoomEventPlan plan = generateRoomEventPlan(
            seed, role, theme, static_cast<std::uint32_t>(seed % 6U), eventZones);
        const EventValidationResult validation = validateRoomEventPlan(plan, eventZones);
        if (!validation.isValid()) {
            allValid = false;
            std::cerr << "Event generation failed at room seed " << seed << '\n';
            for (const EventValidationError error : validation.errors) {
                std::cerr << "  validation error " << static_cast<int>(error) << '\n';
            }
            break;
        }
        hashes.insert(plan.planHash);
        found[static_cast<std::size_t>(plan.type)] = true;
    }
    expect(allValid, "1,000 room seeds pass event type, trigger, wave and zone gates");
    expect(hashes.size() == 1'000U,
        "1,000 room seeds produce unique canonical event hashes");
    expect(std::all_of(found.begin(), found.end(), [](const bool value) { return value; }),
        "event batch reaches None, ambush, reinforcements, lockdown and supply");

    const RoomEventPlan missingZone = generateRoomEventPlan(
        12, RoomRole::Arena, RoomTheme::Security, 3, {});
    expect(containsError(
               validateRoomEventPlan(missingZone, {}), EventValidationError::MissingZone),
        "functional event requires an authored candidate zone");

    for (const wkc::core::Seed worldSeed : {wkc::core::Seed{12345},
             wkc::core::Seed{0x4744505F574F524CULL}}) {
        const WorldGenerationResult generated = generateWorld(worldSeed);
        bool worldEventsValid = generated.world.has_value();
        if (generated.world) {
            for (const RoomInstance& room : generated.world->rooms) {
                const auto node = std::find_if(
                    generated.world->graph.nodes.begin(), generated.world->graph.nodes.end(),
                    [&room](const RoomNode& candidate) { return candidate.id == room.id; });
                worldEventsValid = worldEventsValid
                    && node != generated.world->graph.nodes.end();
                if (node != generated.world->graph.nodes.end()) {
                    const RoomEventPlan plan = generateRoomEventPlan(
                        room.seed, room.role, room.theme, node->depth, room.enemyZones);
                    worldEventsValid = worldEventsValid
                        && validateRoomEventPlan(plan, room.enemyZones).isValid();
                }
            }
        }
        expect(worldEventsValid,
            "default and documented world seeds produce valid room event plans");
    }
}

void killAllEnemies(
    gdp::entities::EntityManager& manager,
    const std::uint32_t roomId)
{
    for (const gdp::entities::EntityId id
        : manager.query(gdp::entities::EntityKind::Enemy)) {
        const gdp::entities::Entity* entity = manager.find(id);
        if (entity != nullptr && entity->roomId == roomId) {
            (void)gdp::gameplay::applyDamage(manager, id, 1'000.0F);
        }
    }
}

void testTerminalAmbushRuntime()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;
    using namespace gdp::world;
    EntityManager manager;
    (void)manager.createEnemy({.roomId = 1, .health = 50.0F});
    RoomEventRuntime runtime = makeRoomEventRuntime(generateRoomEventPlan(
        99, RoomRole::Start, RoomTheme::Industrial, 0, eventZones), 1);

    const RoomEventUpdateSummary dormant
        = updateRoomEvent(runtime, manager, true, false, 1.0F / 60.0F);
    expect(!dormant.activated && runtime.phase == RoomEventPhase::Dormant
            && !isRoomLocked(runtime),
        "START event remains dormant on entry until terminal interaction");

    const RoomEventUpdateSummary activated
        = updateRoomEvent(runtime, manager, true, true, 1.0F / 60.0F);
    expect(activated.activated && activated.wavesSpawned == 1U
            && activated.enemiesSpawned == 1U && isRoomLocked(runtime),
        "terminal use activates ambush, spawns its wave and enters lockdown");
    expect(activated.createdEntities == activated.spawnedEnemies
            && manager.queryRoom(1).size() == 2U,
        "event-created enemy remains owned by the active room");

    killAllEnemies(manager, 1);
    const RoomEventUpdateSummary completed
        = updateRoomEvent(runtime, manager, true, false, 1.0F / 60.0F);
    expect(completed.completed && completed.rewardsSpawned == 1U
            && runtime.phase == RoomEventPhase::Completed && !isRoomLocked(runtime),
        "clearing baseline and event enemies completes and unlocks the ambush");
    expect(manager.query(EntityKind::Item).size() == 1U,
        "completed terminal ambush creates exactly one reward pickup");
    const std::size_t count = manager.activeCount();
    const RoomEventUpdateSummary repeated
        = updateRoomEvent(runtime, manager, true, true, 1.0F);
    expect(repeated.createdEntities.empty() && manager.activeCount() == count,
        "completed event cannot reactivate or duplicate its reward");
}

void testLockdownAndSupplyRuntime()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;
    using namespace gdp::world;
    EntityManager manager;
    const EntityId baseline = manager.createEnemy({
        .roomId = 8,
        .archetype = EnemyArchetype::Guard,
        .health = 70.0F,
    });
    RoomEventRuntime arena = makeRoomEventRuntime(generateRoomEventPlan(
        777, RoomRole::Arena, RoomTheme::Security, 4, eventZones), 8);
    const RoomEventUpdateSummary entered
        = updateRoomEvent(arena, manager, true, false, 0.0F);
    expect(entered.activated && entered.enemiesSpawned == 0U && isRoomLocked(arena),
        "Arena lockdown waits for its initial encounter to be cleared");
    (void)applyDamage(manager, baseline, 1'000.0F);
    const RoomEventUpdateSummary wave
        = updateRoomEvent(arena, manager, true, false, 1.0F);
    expect(wave.enemiesSpawned == 1U && wave.wavesSpawned == 1U,
        "cleared Arena initial encounter releases its deterministic event wave");
    killAllEnemies(manager, 8);
    const EntityId intruder = manager.createEnemy({.roomId = 9});
    manager.find(intruder)->roomId = 8U;
    expect(!updateRoomEvent(arena, manager, true, false, 1.0F).completed,
        "a live intruder inside the Arena still prevents premature room clear");
    (void)applyDamage(manager, intruder, 1'000.0F);
    const RoomEventUpdateSummary arenaComplete
        = updateRoomEvent(arena, manager, true, false, 1.0F);
    expect(arenaComplete.completed && arenaComplete.rewardsSpawned == 1U
            && !isRoomLocked(arena),
        "final Arena wave completion unlocks the room and spawns its reward");

    RoomEventRuntime supply = makeRoomEventRuntime(generateRoomEventPlan(
        888, RoomRole::Hub, RoomTheme::Industrial, 2, eventZones), 9);
    const RoomEventUpdateSummary supplied
        = updateRoomEvent(supply, manager, true, false, 0.0F);
    expect(supplied.activated && supplied.completed
            && supplied.rewardsSpawned == 1U
            && supply.phase == RoomEventPhase::Completed,
        "Hub supply event activates and completes atomically on room entry");
}

void testAdjacentEventWaveIsStagedDormant()
{
    using namespace gdp::entities;
    using namespace gdp::gameplay;
    using namespace gdp::world;

    EntityManager manager;
    const EntityId baseline = manager.createEnemy({
        .roomId = 8,
        .archetype = EnemyArchetype::Guard,
    });
    RoomEventRuntime runtime = makeRoomEventRuntime(generateRoomEventPlan(
        777, RoomRole::Arena, RoomTheme::Security, 4, eventZones), 8);

    const std::vector<EntityId> staged = stageRoomEventFirstWave(runtime, manager);
    expect(staged.size() == 1U && runtime.stagedEnemies == staged,
        "room-entry event materializes its first wave before room activation");
    const EnemyData* stagedEnemy = staged.empty()
        ? nullptr : manager.findData<EnemyData>(staged.front());
    expect(stagedEnemy != nullptr && !stagedEnemy->active
            && runtime.nextWave == 0U && runtime.phase == RoomEventPhase::Dormant,
        "materialized event enemy remains dormant without advancing the event");
    expect(stageRoomEventFirstWave(runtime, manager).empty()
            && manager.queryRoom(8).size() == 2U,
        "staging is idempotent and cannot duplicate the previewed wave");

    manager.find(baseline)->roomId = 9U;
    expect(manager.findData<EnemyData>(baseline)->originRoomId == 8U,
        "cross-room pursuit retains the enemy's event origin room");

    const RoomEventUpdateSummary entered
        = updateRoomEvent(runtime, manager, true, false, 0.0F);
    expect(entered.activated && entered.enemiesSpawned == 0U
            && runtime.nextWave == 0U,
        "enemy chasing into a corridor cannot bypass Arena baseline-clear pacing");
    expect(isRoomLocked(runtime) && !shouldSealRoomDoors(runtime, manager),
        "lockdown leaves a recoverable exit while its live enemy is outside");
    (void)applyDamage(manager, baseline, 1'000.0F);
    const RoomEventUpdateSummary released
        = updateRoomEvent(runtime, manager, true, false, 1.0F);
    stagedEnemy = manager.findData<EnemyData>(staged.front());
    expect(released.spawnedEnemies == staged && released.createdEntities.empty()
            && stagedEnemy != nullptr && stagedEnemy->active
            && runtime.nextWave == 1U,
        "event releases the already visible enemy instead of creating a pop-in duplicate");
    expect(shouldSealRoomDoors(runtime, manager),
        "lockdown can seal again once the escaped enemy is defeated");

    RoomEventRuntime terminal = makeRoomEventRuntime(generateRoomEventPlan(
        99, RoomRole::Start, RoomTheme::Industrial, 0, eventZones), 1);
    expect(stageRoomEventFirstWave(terminal, manager).empty(),
        "terminal ambush remains concealed until explicit terminal interaction");
}

void testTwoWaveReinforcementRuntime()
{
    using namespace gdp::gameplay;
    using namespace gdp::world;
    RoomEventPlan plan;
    for (wkc::core::Seed seed = 0; seed < 10'000; ++seed) {
        plan = generateRoomEventPlan(
            seed, RoomRole::Normal, RoomTheme::Security, 5, eventZones);
        if (plan.type == RoomEventType::Reinforcements && plan.waveCount == 2U) {
            break;
        }
    }
    expect(plan.type == RoomEventType::Reinforcements && plan.waveCount == 2U,
        "batch contains a reproducible two-wave reinforcement plan");
    if (plan.type != RoomEventType::Reinforcements || plan.waveCount != 2U) {
        return;
    }

    gdp::entities::EntityManager manager;
    RoomEventRuntime runtime = makeRoomEventRuntime(plan, 12);
    const RoomEventUpdateSummary first
        = updateRoomEvent(runtime, manager, true, false, 0.0F);
    expect(first.enemiesSpawned == 1U && runtime.nextWave == 1U,
        "reinforcement event spawns only its first wave on activation");
    killAllEnemies(manager, 12);
    const RoomEventUpdateSummary second
        = updateRoomEvent(runtime, manager, true, false, 1.0F);
    expect(second.enemiesSpawned == 1U && runtime.nextWave == 2U,
        "clearing first wave and delay releases the second wave");
    killAllEnemies(manager, 12);
    const RoomEventUpdateSummary completed
        = updateRoomEvent(runtime, manager, true, false, 1.0F);
    expect(completed.completed && completed.rewardsSpawned == 1U,
        "clearing final reinforcement wave completes with one reward");
}

} // namespace

int main()
{
    testDeterministicEventPlan();
    testEventPlanBatchAndWorlds();
    testTerminalAmbushRuntime();
    testLockdownAndSupplyRuntime();
    testAdjacentEventWaveIsStagedDormant();
    testTwoWaveReinforcementRuntime();
    if (failures != 0) {
        std::cerr << failures << " event test(s) failed\n";
        return 1;
    }
    std::cout << "All GDP room event tests passed\n";
    return 0;
}
