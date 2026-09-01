#include "gameplay/Progression.hpp"

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

void testCurveAndRewards()
{
    using namespace gdp::gameplay;
    expect(experienceForNextLevel(1U) == 100U
            && experienceForNextLevel(2U) == 175U
            && experienceForNextLevel(9U) == 700U
            && experienceForNextLevel(10U) == 0U,
        "level curve starts at 100 XP, grows by 75 and stops at level 10");
    expect(rewardForLevel(2U) == ProgressionReward::Vitality
            && rewardForLevel(3U) == ProgressionReward::Firepower
            && rewardForLevel(4U) == ProgressionReward::Capacity,
        "level rewards cycle through vitality, firepower and capacity");
}

void testFunctionalProgression()
{
    using namespace gdp::gameplay;
    Player player;
    player.applyDamage(50.0F);
    Weapon weapon;
    ProgressionSystem progression;

    const ProgressionUpdate event = progression.record(
        ProgressionEvent::RoomEventCompleted, 1U, player, weapon);
    expect(event.experienceAwarded == 75U && event.scoreAwarded == 1'000U
            && event.levelsGained == 0U,
        "completed room event awards its documented XP and score");
    const ProgressionUpdate worker = progression.record(
        ProgressionEvent::WorkerKilled, 1U, player, weapon);
    expect(worker.levelsGained == 1U
            && worker.rewards.size() == 1U
            && worker.rewards.front() == ProgressionReward::Vitality,
        "START worker plus event completion reaches level 2 and grants vitality");
    expect(progression.state().level == 2U
            && progression.state().experienceIntoLevel == 0U
            && progression.state().totalExperience == 100U
            && progression.state().score == 1'250U,
        "progression state keeps level, current XP, total XP and score");
    expect(std::abs(player.state().maximumHealth - 115.0F) < 1.0e-5F
            && std::abs(player.state().health - 65.0F) < 1.0e-5F
            && std::abs(player.state().maximumArmor - 60.0F) < 1.0e-5F
            && std::abs(player.state().armor - 10.0F) < 1.0e-5F,
        "vitality increases health and armor capacity and restores both");

    const ProgressionUpdate robots = progression.record(
        ProgressionEvent::IndustrialRobotKilled, 2U, player, weapon);
    expect(robots.levelsGained == 1U
            && progression.state().level == 3U
            && progression.state().experienceIntoLevel == 225U
            && std::abs(weapon.definition().damage - 38.0F) < 1.0e-5F,
        "multi-award crosses one threshold, preserves remainder and grants firepower");
    const ProgressionUpdate thirdRobot = progression.record(
        ProgressionEvent::IndustrialRobotKilled, 1U, player, weapon);
    expect(thirdRobot.levelsGained == 1U
            && progression.state().level == 4U
            && progression.state().experienceIntoLevel == 175U
            && weapon.reserveCapacity() == 36U && weapon.reserveAmmo() == 36U,
        "next threshold grants reserve capacity and bonus ammo");
    expect(validateProgressionState(progression.state()),
        "functional progression state validates after multiple reward types");
}

void testCapCountersAndInvalidEvents()
{
    using namespace gdp::gameplay;
    Player player;
    Weapon weapon;
    ProgressionSystem progression;
    (void)progression.record(ProgressionEvent::LootCollected, 3U, player, weapon);
    (void)progression.record(ProgressionEvent::PropDestroyed, 2U, player, weapon);
    const ProgressionUpdate capped = progression.record(
        ProgressionEvent::IndustrialRobotKilled, 100U, player, weapon);
    expect(capped.levelsGained == 9U && progression.state().level == 10U
            && progression.state().experienceIntoLevel == 0U,
        "large deterministic award reaches but never exceeds level cap");
    expect(progression.state().vitalityRank == 3U
            && progression.state().firepowerRank == 3U
            && progression.state().capacityRank == 3U,
        "level cap contains exactly three ranks of each automatic reward");
    expect(progression.state().enemiesKilled == 100U
            && progression.state().propsDestroyed == 2U
            && progression.state().lootCollected == 3U,
        "source counters retain their own progression telemetry");
    expect(std::abs(player.state().maximumHealth - 145.0F) < 1.0e-5F
            && std::abs(player.state().maximumArmor - 80.0F) < 1.0e-5F
            && std::abs(player.state().armor - 30.0F) < 1.0e-5F
            && std::abs(weapon.definition().damage - 46.0F) < 1.0e-5F
            && weapon.reserveCapacity() == 44U,
        "all nine level rewards are applied to real player and weapon stats");
    expect(validateProgressionState(progression.state()),
        "capped progression state validates");

    const ProgressionState before = progression.state();
    const ProgressionUpdate invalid = progression.record(
        ProgressionEvent::Count, 1U, player, weapon);
    const ProgressionUpdate empty = progression.record(
        ProgressionEvent::WorkerKilled, 0U, player, weapon);
    expect(invalid.experienceAwarded == 0U && empty.experienceAwarded == 0U
            && progression.state().score == before.score,
        "invalid or empty events cannot mutate progression");
}

} // namespace

int main()
{
    testCurveAndRewards();
    testFunctionalProgression();
    testCapCountersAndInvalidEvents();

    if (failures != 0) {
        std::cerr << failures << " progression test(s) failed\n";
        return 1;
    }
    std::cout << "Functional progression tests passed\n";
    return 0;
}
