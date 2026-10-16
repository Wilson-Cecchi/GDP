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
    const std::vector<EntityId> staged = stageRoomEventEnemies(runtime, manager);
    expect(staged.size() == runtime.plan.spawns.size()
            && runtime.eventEnemies == staged
            && runtime.nextWave == runtime.plan.waveCount
            && runtime.phase == RoomEventPhase::Dormant,
        "terminal enemy is present while the terminal event remains dormant");
    if (!staged.empty()) {
        const auto* entity = manager.find(staged.front());
        const auto* enemy = manager.findData<EnemyData>(staged.front());
        const auto& planned = runtime.plan.spawns.front();
        expect(entity != nullptr && enemy != nullptr && enemy->active
                && entity->roomId == 1U
                && entity->transform.position == planned.position
                && enemy->archetype == planned.archetype
                && enemy->health == planned.health
                && enemy->visualSeed == planned.visualSeed,
            "terminal enemy preserves its planned room, position, archetype, HP and visual seed");
    }
    expect(stageRoomEventEnemies(runtime, manager).empty()
            && manager.queryRoom(1).size() == 2U,
        "preparing the terminal enemy twice does not duplicate it");

    const RoomEventUpdateSummary dormant
        = updateRoomEvent(runtime, manager, true, false, 1.0F / 60.0F);
    expect(!dormant.activated && dormant.createdEntities.empty()
            && runtime.phase == RoomEventPhase::Dormant
            && !isRoomLocked(runtime),
        "START event remains dormant on entry until terminal interaction");

    const RoomEventUpdateSummary activated
        = updateRoomEvent(runtime, manager, true, true, 1.0F / 60.0F);
    expect(activated.activated && !activated.completed
            && activated.wavesSpawned == 0U && activated.enemiesSpawned == 0U
            && activated.createdEntities.empty() && isRoomLocked(runtime),
        "terminal use activates lockdown without adding an enemy");

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

    EntityManager preclearedManager;
    (void)preclearedManager.createEnemy({.roomId = 2, .health = 50.0F});
    RoomEventRuntime precleared = makeRoomEventRuntime(generateRoomEventPlan(
        99, RoomRole::Start, RoomTheme::Industrial, 0, eventZones), 2);
    const auto preclearedEnemies = stageRoomEventEnemies(precleared, preclearedManager);
    killAllEnemies(preclearedManager, 2);
    const RoomEventUpdateSummary beforeUse = updateRoomEvent(
        precleared, preclearedManager, true, false, 1.0F / 60.0F);
    expect(preclearedEnemies.size() == precleared.plan.spawns.size()
            && !beforeUse.activated && !beforeUse.completed
            && preclearedManager.query(EntityKind::Item).empty(),
        "clearing the terminal room before interaction does not complete its event");
    const RoomEventUpdateSummary useAfterClear = updateRoomEvent(
        precleared, preclearedManager, true, true, 1.0F / 60.0F);
    expect(useAfterClear.activated && useAfterClear.completed
            && useAfterClear.enemiesSpawned == 0U
            && useAfterClear.spawnedEnemies.empty()
            && useAfterClear.rewardsSpawned == 1U
            && precleared.phase == RoomEventPhase::Completed,
        "using a cleared terminal completes and rewards it without respawning enemies");
    expect(updateRoomEvent(precleared, preclearedManager, true, true, 1.0F)
            .createdEntities.empty()
            && preclearedManager.query(EntityKind::Item).size() == 1U,
        "precleared terminal event grants its reward only once");
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
    const std::vector<EntityId> arenaEnemies = stageRoomEventEnemies(arena, manager);
    expect(arenaEnemies.size() == 1U
            && manager.findData<EnemyData>(arenaEnemies.front())->active
            && arena.nextWave == arena.plan.waveCount,
        "Arena extra enemy is active alongside the baseline encounter");
    const RoomEventUpdateSummary entered
        = updateRoomEvent(arena, manager, true, false, 0.0F);
    expect(entered.activated && entered.enemiesSpawned == 0U && isRoomLocked(arena),
        "Arena locks on entry without creating a later enemy");
    (void)applyDamage(manager, baseline, 1'000.0F);
    const std::size_t countAfterBaseline = manager.activeCount();
    const RoomEventUpdateSummary afterBaseline
        = updateRoomEvent(arena, manager, true, false, 1.0F);
    expect(!afterBaseline.completed && afterBaseline.enemiesSpawned == 0U
            && afterBaseline.createdEntities.empty()
            && manager.activeCount() == countAfterBaseline,
        "clearing the baseline encounter does not release a late Arena wave");
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
        "clearing the full Arena encounter unlocks the room and spawns its reward");
    expect(updateRoomEvent(arena, manager, true, false, 1.0F).createdEntities.empty()
            && manager.query(EntityKind::Item).size() == 1U,
        "completed Arena event cannot duplicate its reward");

    RoomEventRuntime supply = makeRoomEventRuntime(generateRoomEventPlan(
        888, RoomRole::Hub, RoomTheme::Industrial, 2, eventZones), 9);
    const RoomEventUpdateSummary supplied
        = updateRoomEvent(supply, manager, true, false, 0.0F);
    expect(supplied.activated && supplied.completed
            && supplied.rewardsSpawned == 1U
            && supply.phase == RoomEventPhase::Completed,
        "Hub supply event activates and completes atomically on room entry");
}

void testLockdownEnemyAndPursuit()
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

    const std::vector<EntityId> staged = stageRoomEventEnemies(runtime, manager);
    const EnemyData* stagedEnemy = staged.empty()
        ? nullptr : manager.findData<EnemyData>(staged.front());
    expect(staged.size() == 1U && stagedEnemy != nullptr
            && stagedEnemy->active && runtime.eventEnemies == staged
            && runtime.nextWave == runtime.plan.waveCount
            && runtime.phase == RoomEventPhase::Dormant,
        "Arena event enemy exists and is active before room entry");
    expect(stageRoomEventEnemies(runtime, manager).empty()
            && manager.queryRoom(8).size() == 2U,
        "prepopulating the Arena is idempotent");

    manager.find(baseline)->roomId = 9U;
    expect(manager.findData<EnemyData>(baseline)->originRoomId == 8U,
        "cross-room pursuit retains the enemy's event origin room");

    const RoomEventUpdateSummary entered
        = updateRoomEvent(runtime, manager, true, false, 0.0F);
    expect(entered.activated && entered.enemiesSpawned == 0U
            && runtime.nextWave == runtime.plan.waveCount,
        "enemy chasing into a corridor does not create a late Arena wave");
    expect(isRoomLocked(runtime) && !shouldSealRoomDoors(runtime, manager),
        "lockdown leaves a recoverable exit while its live enemy is outside");
    (void)applyDamage(manager, baseline, 1'000.0F);
    const RoomEventUpdateSummary afterBaseline
        = updateRoomEvent(runtime, manager, true, false, 1.0F);
    expect(!afterBaseline.completed && afterBaseline.spawnedEnemies.empty()
            && afterBaseline.createdEntities.empty(),
        "Arena enemy remains to be defeated after the baseline enemy dies");
    expect(shouldSealRoomDoors(runtime, manager),
        "lockdown can seal again once the escaped enemy is defeated");
    (void)applyDamage(manager, staged.front(), 1'000.0F);
    expect(updateRoomEvent(runtime, manager, true, false, 1.0F).completed,
        "defeating the final Arena enemy completes the event");

    RoomEventRuntime terminal = makeRoomEventRuntime(generateRoomEventPlan(
        99, RoomRole::Start, RoomTheme::Industrial, 0, eventZones), 1);
    const auto terminalEnemies = stageRoomEventEnemies(terminal, manager);
    expect(terminalEnemies.size() == terminal.plan.spawns.size()
            && terminal.phase == RoomEventPhase::Dormant,
        "terminal enemy is present before the terminal event is triggered");
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
    const auto staged = stageRoomEventEnemies(runtime, manager);
    expect(staged.size() == plan.spawns.size() && runtime.eventEnemies == staged
            && runtime.nextWave == plan.waveCount
            && runtime.phase == RoomEventPhase::Dormant,
        "all two-wave reinforcements exist before room entry");
    for (std::size_t i = 0; i < staged.size(); ++i) {
        const auto* entity = manager.find(staged[i]);
        const auto* enemy = manager.findData<gdp::entities::EnemyData>(staged[i]);
        expect(entity != nullptr && enemy != nullptr && enemy->active
                && entity->roomId == 12U
                && entity->transform.position == plan.spawns[i].position
                && enemy->archetype == plan.spawns[i].archetype
                && enemy->health == plan.spawns[i].health
                && enemy->visualSeed == plan.spawns[i].visualSeed,
            "prepopulated enemy preserves planned room, position, archetype, HP and visual seed");
    }
    expect(stageRoomEventEnemies(runtime, manager).empty()
            && manager.queryRoom(12).size() == staged.size(),
        "repeated staging cannot duplicate either reinforcement wave");
    const RoomEventUpdateSummary first
        = updateRoomEvent(runtime, manager, true, false, 0.0F);
    expect(first.activated && first.enemiesSpawned == 0U
            && first.createdEntities.empty() && runtime.nextWave == plan.waveCount,
        "entering the room activates its event without creating enemies");
    (void)gdp::gameplay::applyDamage(manager, staged.front(), 1'000.0F);
    const std::size_t countAfterFirstKill = manager.activeCount();
    const RoomEventUpdateSummary second
        = updateRoomEvent(runtime, manager, true, false, 1.0F);
    expect(!second.completed && second.enemiesSpawned == 0U
            && second.createdEntities.empty()
            && manager.activeCount() == countAfterFirstKill
            && manager.findData<gdp::entities::EnemyData>(staged.back())->active,
        "killing a first-wave enemy does not create a delayed second wave");
    killAllEnemies(manager, 12);
    const RoomEventUpdateSummary completed
        = updateRoomEvent(runtime, manager, true, false, 1.0F);
    expect(completed.completed && completed.rewardsSpawned == 1U,
        "clearing all reinforcements completes with one reward");
    expect(updateRoomEvent(runtime, manager, true, false, 1.0F).createdEntities.empty()
            && manager.query(gdp::entities::EntityKind::Item).size() == 1U,
        "completed reinforcement event cannot duplicate its reward");
}

} // namespace

int main()
{
    testDeterministicEventPlan();
    testEventPlanBatchAndWorlds();
    testTerminalAmbushRuntime();
    testLockdownAndSupplyRuntime();
    testLockdownEnemyAndPursuit();
    testTwoWaveReinforcementRuntime();
    if (failures != 0) {
        std::cerr << failures << " event test(s) failed\n";
        return 1;
    }
    std::cout << "All GDP room event tests passed\n";
    return 0;
}
