#include "gameplay/Campaign.hpp"

#include "gameplay/Session.hpp"

#include <algorithm>
#include <cmath>

namespace gdp::gameplay {

CampaignCarryover captureCampaignCarryover(
    const Player& player,
    const Weapon& weapon,
    const ProgressionSystem& progression) noexcept
{
    const PlayerState& playerState = player.state();
    return {
        {
            playerState.health,
            playerState.maximumHealth,
            playerState.armor,
            playerState.maximumArmor,
            playerState.keys,
        },
        weapon.campaignProgress(),
        progression.state(),
    };
}

bool validateCampaignCarryover(const CampaignCarryover& carryover) noexcept
{
    const PlayerCampaignProgress& player = carryover.player;
    return std::isfinite(player.health) && player.health > 0.0F
        && std::isfinite(player.maximumHealth) && player.maximumHealth > 0.0F
        && player.health <= player.maximumHealth
        && std::isfinite(player.armor) && player.armor >= 0.0F
        && std::isfinite(player.maximumArmor) && player.maximumArmor >= 0.0F
        && player.armor <= player.maximumArmor
        && validateWeaponCampaignProgress(carryover.weapon)
        && validateProgressionState(carryover.progression);
}

bool restoreCampaignCarryover(
    const CampaignCarryover& carryover,
    Player& player,
    Weapon& weapon,
    ProgressionSystem& progression) noexcept
{
    if (!validateCampaignCarryover(carryover)) {
        return false;
    }
    return player.restoreCampaignProgress(carryover.player)
        && weapon.restoreCampaignProgress(carryover.weapon)
        && progression.restoreCampaignProgress(carryover.progression);
}

bool validateCampaignCheckpoint(
    const CampaignCheckpoint& checkpoint) noexcept
{
    return checkpoint.rulesVersion == currentCampaignRulesVersion
        && checkpoint.floor >= 1U
        && checkpoint.floor <= campaignFloorCount
        && checkpoint.runIndex >= checkpoint.seedIndex
        && checkpoint.currentSeed
            == seedForRun(checkpoint.initialSeed, checkpoint.seedIndex)
        && validateCampaignCarryover(checkpoint.floorEntry);
}

float campaignDifficultyMultiplier(const std::uint32_t floor) noexcept
{
    const std::uint32_t safeFloor = std::clamp(floor, 1U, campaignFloorCount);
    return 1.0F + (static_cast<float>(safeFloor - 1U) * 0.10F);
}

} // namespace gdp::gameplay
