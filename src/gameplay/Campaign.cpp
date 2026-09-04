#include "gameplay/Campaign.hpp"

#include "gameplay/Session.hpp"

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
        {
            weapon.definition(),
            weapon.magazineAmmo(),
            weapon.reserveAmmo(),
        },
        progression.state(),
    };
}

bool validateCampaignCarryover(const CampaignCarryover& carryover) noexcept
{
    const PlayerCampaignProgress& player = carryover.player;
    const WeaponCampaignProgress& weapon = carryover.weapon;
    return std::isfinite(player.health) && player.health > 0.0F
        && std::isfinite(player.maximumHealth) && player.maximumHealth > 0.0F
        && player.health <= player.maximumHealth
        && std::isfinite(player.armor) && player.armor >= 0.0F
        && std::isfinite(player.maximumArmor) && player.maximumArmor >= 0.0F
        && player.armor <= player.maximumArmor
        && std::isfinite(weapon.definition.damage)
        && weapon.definition.damage > 0.0F
        && std::isfinite(weapon.definition.fireInterval)
        && weapon.definition.fireInterval >= 0.0F
        && std::isfinite(weapon.definition.range)
        && weapon.definition.range > 0.0F
        && weapon.definition.magazineSize > 0U
        && weapon.magazineAmmo <= weapon.definition.magazineSize
        && weapon.reserveAmmo <= weapon.definition.reserveMaximum
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

} // namespace gdp::gameplay
