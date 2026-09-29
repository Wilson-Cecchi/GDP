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
            && session.state().mode == gameplay::RunMode::Standard
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

void testBossCanBeDefeatedFromAnOpenCorridor()
{
    using namespace gdp;
    gameplay::SessionSystem closed{7U};
    expect(!closed.update(true, world::RoomRole::Normal, true, false, false).changed
            && !closed.state().bossEngaged,
        "an inaccessible defeated boss does not end an unrelated room");

    gameplay::SessionSystem corridor{8U};
    expect(!corridor.update(true, world::RoomRole::Normal, true, true, true).changed
            && corridor.state().bossEngaged,
        "opening the Boss corridor engages the boss before room entry");
    expect(corridor.update(true, world::RoomRole::Normal, true, false, true).phase
            == gameplay::SessionPhase::Victory,
        "killing the boss from an open corridor immediately completes the run");

    gameplay::SessionSystem singleFrame{9U};
    expect(singleFrame.update(true, world::RoomRole::Normal, true, false, true).phase
            == gameplay::SessionPhase::Victory,
        "a lethal shot on the first accessible frame cannot leave the run stuck");

    gameplay::SessionSystem enterAfterKill{10U};
    expect(enterAfterKill.update(true, world::RoomRole::Boss, true, false).phase
            == gameplay::SessionPhase::Victory,
        "entering a Boss room after an earlier kill still resolves victory");
}

void testDeterministicRunTimer()
{
    using namespace gdp;
    gameplay::SessionSystem session{12345U};
    session.advanceRunTimer(false);
    expect(session.state().elapsedRunTicks == 0U,
        "standard run timer ignores fixed steps while gameplay is paused");
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

void testSpeedrunModeTimerRules()
{
    using namespace gdp;
    gameplay::SessionSystem session{98765U};
    expect(session.selectRunMode(gameplay::RunMode::Speedrun)
            && session.state().mode == gameplay::RunMode::Speedrun
            && std::string_view{gameplay::runModeName(session.state().mode)}
                == "Speedrun",
        "Speedrun can be selected before its first active tick");
    expect(session.selectRunSeed(4321U)
            && session.state().currentSeed == 4321U,
        "a stored challenge seed can be selected before timing begins");
    session.advanceRunTimer(false);
    expect(session.state().elapsedRunTicks == 1U,
        "Speedrun timer continues while gameplay menus freeze the simulation");
    expect(!session.selectRunMode(gameplay::RunMode::Standard)
            && !session.selectRunMode(gameplay::RunMode::Count)
            && !session.selectRunSeed(9999U)
            && session.state().currentSeed == 4321U,
        "mode and challenge seed cannot change after timing starts");

    (void)session.update(false, world::RoomRole::Start, false, false);
    expect(session.restart()
            && session.state().mode == gameplay::RunMode::Speedrun
            && session.state().elapsedRunTicks == 0U,
        "retry preserves Speedrun mode while resetting its timer");
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
            && first.state().seedIndex == 1U
            && !first.state().bossEngaged,
        "restart resets terminal gameplay state and advances the run index");
    expect(gameplay::seedForRun(first.state().initialSeed, 0U)
            == first.state().initialSeed,
        "run zero preserves the user-provided world seed");
}

void testSameSeedReplayPreservesDeterministicSequence()
{
    using namespace gdp;
    gameplay::SessionSystem session{0x5245504C41595F31ULL};
    const wkc::core::Seed originalSeed = session.state().currentSeed;
    (void)session.update(false, world::RoomRole::Start, false, false);
    expect(!session.restart(gameplay::RunRestartMode::Count),
        "invalid restart modes are rejected without changing terminal state");

    const auto replayedSeed
        = session.restart(gameplay::RunRestartMode::SameSeed);
    expect(replayedSeed && *replayedSeed == originalSeed
            && session.state().runIndex == 1U
            && session.state().seedIndex == 0U
            && session.state().phase == gameplay::SessionPhase::Playing,
        "same-seed replay resets gameplay while preserving the world seed");

    (void)session.update(false, world::RoomRole::Start, false, false);
    const auto nextSeed
        = session.restart(gameplay::RunRestartMode::NextSeed);
    expect(nextSeed
            && *nextSeed == gameplay::seedForRun(originalSeed, 1U)
            && session.state().runIndex == 2U
            && session.state().seedIndex == 1U,
        "replays do not skip the next seed in the deterministic sequence");
}

void testCampaignAdvancesThreeDeterministicFloors()
{
    using namespace gdp;
    gameplay::SessionSystem session{0x43414D504149474EULL};
    expect(session.selectRunMode(gameplay::RunMode::Campaign)
            && session.state().campaignFloor == 1U
            && std::string_view{gameplay::runModeName(session.state().mode)}
                == "Campaign",
        "Campaign starts on floor one with an explicit run mode");

    const auto defeat = session.update(
        false, world::RoomRole::Start, false, false);
    expect(defeat.changed && !gameplay::canAdvanceCampaign(session.state())
            && !session.restart(gameplay::RunRestartMode::NextCampaignFloor),
        "Defeat cannot skip directly to the next Campaign floor");
    expect(session.restart(gameplay::RunRestartMode::SameSeed)
            && session.state().campaignFloor == 1U,
        "replaying a Campaign floor keeps its floor and seed");

    const wkc::core::Seed firstSeed = session.state().currentSeed;
    (void)session.update(true, world::RoomRole::Boss, true, true);
    (void)session.update(true, world::RoomRole::Boss, true, false);
    expect(gameplay::canAdvanceCampaign(session.state()),
        "boss victory unlocks the second Campaign floor");
    const auto secondSeed
        = session.restart(gameplay::RunRestartMode::NextCampaignFloor);
    expect(secondSeed && *secondSeed == gameplay::seedForRun(firstSeed, 1U)
            && session.state().campaignFloor == 2U
            && session.state().seedIndex == 1U,
        "second floor advances through the deterministic world sequence");

    (void)session.update(true, world::RoomRole::Boss, true, true);
    (void)session.update(true, world::RoomRole::Boss, true, false);
    const auto thirdSeed
        = session.restart(gameplay::RunRestartMode::NextCampaignFloor);
    expect(thirdSeed && *thirdSeed == gameplay::seedForRun(firstSeed, 2U)
            && session.state().campaignFloor == gameplay::campaignFloorCount,
        "third floor is derived deterministically and reaches the Campaign cap");

    (void)session.update(true, world::RoomRole::Boss, true, true);
    (void)session.update(true, world::RoomRole::Boss, true, false);
    expect(!gameplay::canAdvanceCampaign(session.state())
            && !session.restart(gameplay::RunRestartMode::NextCampaignFloor),
        "final Campaign victory cannot advance beyond the floor cap");
    expect(session.restart(gameplay::RunRestartMode::NextSeed)
            && session.state().campaignFloor == 1U,
        "a new Campaign resets its floor while advancing the seed sequence");
}

void testCampaignCheckpointRestoresOnlyPristineSessions()
{
    using namespace gdp;
    constexpr wkc::core::Seed initialSeed = 0x53415645445F5255ULL;
    constexpr std::uint32_t seedIndex = 2U;
    const wkc::core::Seed currentSeed
        = gameplay::seedForRun(initialSeed, seedIndex);
    gameplay::SessionSystem restored{999U};
    expect(restored.restoreCampaignCheckpoint(
            initialSeed, currentSeed, 4U, seedIndex, 3U)
            && restored.state().mode == gameplay::RunMode::Campaign
            && restored.state().initialSeed == initialSeed
            && restored.state().currentSeed == currentSeed
            && restored.state().runIndex == 4U
            && restored.state().seedIndex == seedIndex
            && restored.state().campaignFloor == 3U,
        "a valid checkpoint restores its deterministic Campaign session");

    gameplay::SessionSystem invalid{123U};
    expect(!invalid.restoreCampaignCheckpoint(
                initialSeed, currentSeed + 1U, 4U, seedIndex, 3U)
            && !invalid.restoreCampaignCheckpoint(
                initialSeed, currentSeed, 1U, seedIndex, 3U)
            && !invalid.restoreCampaignCheckpoint(
                initialSeed, currentSeed, 4U, seedIndex, 4U)
            && invalid.state().mode == gameplay::RunMode::Standard
            && invalid.state().currentSeed == 123U,
        "inconsistent checkpoint metadata is rejected without mutation");

    invalid.advanceRunTimer();
    expect(!invalid.restoreCampaignCheckpoint(
            initialSeed, currentSeed, 4U, seedIndex, 3U),
        "a checkpoint cannot replace a session after gameplay has started");
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
    expect(gameplay::bossStatus(entities, 0U) == gameplay::BossStatus{true, true},
        "global Boss status still tracks a boss that has crossed into another room");
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
    testBossCanBeDefeatedFromAnOpenCorridor();
    testDeterministicRunTimer();
    testSpeedrunModeTimerRules();
    testDefeatAndDeterministicRestart();
    testSameSeedReplayPreservesDeterministicSequence();
    testCampaignAdvancesThreeDeterministicFloors();
    testCampaignCheckpointRestoresOnlyPristineSessions();
    testBossStatusUsesArchetypeAndRoomOwnership();
    testRestartSeedRegeneratesAReproducibleWorld();

    if (failures != 0) {
        std::cerr << failures << " session test(s) failed\n";
        return 1;
    }
    std::cout << "Session tests passed\n";
    return 0;
}
