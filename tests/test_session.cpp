#include "gameplay/Session.hpp"
#include "world/WorldGenerator.hpp"

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

void testBossVictoryRequiresARealEngagement()
{
    using namespace gdp;
    gameplay::SessionSystem session{12345U};
    expect(session.state().phase == gameplay::SessionPhase::Playing
            && session.state().currentSeed == 12345U
            && session.state().runIndex == 0U,
        "session begins in the first deterministic run");
    expect(!session.restart(), "an active run cannot restart prematurely");
    expect(!session.update(true, world::RoomRole::Normal, false, false).changed,
        "ordinary rooms cannot finish the run");
    expect(!session.update(true, world::RoomRole::Boss, false, false).changed,
        "an invalid empty Boss room cannot grant victory");
    expect(!session.update(true, world::RoomRole::Boss, true, true).changed
            && session.state().bossEngaged,
        "entering the Boss room records a live IndustrialRobot engagement");
    const gameplay::SessionUpdate victory
        = session.update(true, world::RoomRole::Boss, true, false);
    expect(victory.changed && victory.phase == gameplay::SessionPhase::Victory,
        "defeating the engaged IndustrialRobot completes the run");
    expect(!session.update(false, world::RoomRole::Boss, true, false).changed
            && session.state().phase == gameplay::SessionPhase::Victory,
        "a completed run has a stable terminal state");
}

void testDeterministicRunTimer()
{
    using namespace gdp;
    gameplay::SessionSystem session{12345U};
    for (std::uint64_t tick = 0U;
         tick < gameplay::runTimerTicksPerSecond * 10U; ++tick) {
        session.advanceRunTimer();
    }
    expect(session.state().elapsedRunTicks == 600U,
        "run timer advances exactly once per active 60 Hz simulation step");

    (void)session.update(false, world::RoomRole::Start, false, false);
    session.advanceRunTimer();
    expect(session.state().elapsedRunTicks == 600U,
        "terminal sessions freeze the elapsed run timer");
    expect(session.restart() && session.state().elapsedRunTicks == 0U,
        "starting the next deterministic run resets its timer");
}

void testDefeatAndDeterministicRestart()
{
    using namespace gdp;
    gameplay::SessionSystem first{0x52554E5F53454544ULL};
    gameplay::SessionSystem repeated{0x52554E5F53454544ULL};
    const gameplay::SessionUpdate defeat
        = first.update(false, world::RoomRole::Start, false, false);
    (void)repeated.update(false, world::RoomRole::Start, false, false);
    expect(defeat.changed && defeat.phase == gameplay::SessionPhase::Defeat,
        "player death ends an active run in Defeat");

    const auto firstRestart = first.restart();
    const auto repeatedRestart = repeated.restart();
    expect(firstRestart && repeatedRestart && *firstRestart == *repeatedRestart
            && *firstRestart != first.state().initialSeed,
        "restart derives the same distinct second-run seed from the initial seed");
    expect(first.state().phase == gameplay::SessionPhase::Playing
            && first.state().runIndex == 1U
            && !first.state().bossEngaged,
        "restart resets terminal gameplay state and advances the run index");
    expect(gameplay::seedForRun(first.state().initialSeed, 0U)
            == first.state().initialSeed,
        "run zero preserves the user-provided world seed");
}

void testBossStatusUsesArchetypeAndRoomOwnership()
{
    using namespace gdp;
    entities::EntityManager entities;
    (void)entities.createEnemy({
        .roomId = 8U,
        .archetype = entities::EnemyArchetype::Guard,
    });
    const entities::EntityId remoteRobot = entities.createEnemy({
        .roomId = 9U,
        .archetype = entities::EnemyArchetype::IndustrialRobot,
    });
    const entities::EntityId bossRobot = entities.createEnemy({
        .roomId = 8U,
        .archetype = entities::EnemyArchetype::IndustrialRobot,
    });
    expect(gameplay::bossStatus(entities, 8U) == gameplay::BossStatus{true, true},
        "Boss status selects a live IndustrialRobot in the requested room");
    entities.findData<entities::EnemyData>(bossRobot)->behavior
        = entities::EnemyBehaviorState::Dead;
    expect(gameplay::bossStatus(entities, 8U) == gameplay::BossStatus{true, false},
        "dead local Boss is not masked by a live robot in another room");
    expect(gameplay::bossStatus(entities, 7U) == gameplay::BossStatus{},
        "a room without an IndustrialRobot has no Boss status");
    expect(entities.findData<entities::EnemyData>(remoteRobot)->behavior
            != entities::EnemyBehaviorState::Dead,
        "Boss status inspection never mutates remote entities");
}

void testRestartSeedRegeneratesAReproducibleWorld()
{
    using namespace gdp;
    constexpr wkc::core::Seed initialSeed = 0x53455353494F4EULL;
    const wkc::core::Seed restartSeed = gameplay::seedForRun(initialSeed, 1U);
    const world::WorldGenerationResult initial = world::generateWorld(initialSeed);
    const world::WorldGenerationResult restarted = world::generateWorld(restartSeed);
    const world::WorldGenerationResult repeated = world::generateWorld(restartSeed);
    expect(initial.succeeded() && restarted.succeeded() && repeated.succeeded(),
        "initial and restarted session seeds generate valid worlds");
    if (!initial.world || !restarted.world || !repeated.world) {
        return;
    }
    expect(world::worldFingerprint(*restarted.world)
            == world::worldFingerprint(*repeated.world),
        "a restarted run reproduces the same world from its derived seed");
    expect(world::worldFingerprint(*initial.world)
            != world::worldFingerprint(*restarted.world),
        "the next run advances to a structurally distinct deterministic world");
}

} // namespace

int main()
{
    testBossVictoryRequiresARealEngagement();
    testDeterministicRunTimer();
    testDefeatAndDeterministicRestart();
    testBossStatusUsesArchetypeAndRoomOwnership();
    testRestartSeedRegeneratesAReproducibleWorld();

    if (failures != 0) {
        std::cerr << failures << " session test(s) failed\n";
        return 1;
    }
    std::cout << "Session tests passed\n";
    return 0;
}
