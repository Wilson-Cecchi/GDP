#include "gameplay/Campaign.hpp"
#include "gameplay/Session.hpp"

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
    sourcePlayer.applyDamage(15.0F);
    expect(sourceWeapon.tryFire() == FireReadiness::Fired,
        "campaign fixture spends the first magazine round");
    sourceWeapon.fixedUpdate(1.0F);
    expect(sourceWeapon.tryFire() == FireReadiness::Fired,
        "campaign fixture spends the second magazine round");

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
            && near(restoredWeapon.cooldownRemaining(), 0.0F),
        "weapon upgrades and ammunition persist while cooldown resets");
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

    CampaignCarryover invalidProgression = valid;
    invalidProgression.progression.vitalityRank = 1U;
    expect(!validateCampaignCarryover(invalidProgression),
        "inconsistent progression ranks invalidate Campaign carry-over");
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
    expect(near(campaignDifficultyMultiplier(2U), 1.1F)
            && near(campaignDifficultyMultiplier(3U), 1.2F),
        "Campaign threat increases by ten percent on each later floor");
    expect(near(campaignDifficultyMultiplier(0U), 1.0F)
            && near(campaignDifficultyMultiplier(99U), 1.2F),
        "Campaign threat clamps invalid floor numbers safely");
}

} // namespace

int main()
{
    testCampaignCarryoverRoundTrip();
    testCampaignCarryoverRejectsInvalidStateAtomically();
    testCampaignCheckpointValidation();
    testCampaignDifficultyProgression();
    if (failures != 0) {
        std::cerr << failures << " campaign test(s) failed\n";
        return 1;
    }
    std::cout << "Campaign carry-over tests passed\n";
    return 0;
}
