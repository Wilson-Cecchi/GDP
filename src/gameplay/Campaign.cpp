#include "gameplay/Campaign.hpp"

#include "gameplay/Session.hpp"
#include "game/SeedDomains.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace gdp::gameplay {

CampaignFloorLayout campaignFloorLayout(
    const wkc::core::Seed floorSeed, const std::uint32_t floor) noexcept
{
    const std::uint32_t safeFloor = std::clamp(floor, 1U, campaignFloorCount);
    const wkc::core::Seed layout = wkc::core::deriveSeed(
        floorSeed, gdp::game::SeedDomain::CampaignLayout, safeFloor);
    const std::uint32_t base = 7U + (safeFloor - 1U) / 4U;
    return {std::min(10U, base + static_cast<std::uint32_t>(layout % 3U)),
        static_cast<std::uint32_t>((layout >> 8U) & 1U)};
}

CampaignMissionKind campaignMissionKind(
    const wkc::core::Seed initialSeed, const std::uint32_t floor) noexcept
{
    const std::uint32_t safeFloor = std::clamp(floor, 1U, campaignFloorCount);
    if (safeFloor == 1U) return CampaignMissionKind::SecureArena;

    // Two four-floor decks cover every objective. The final floor draws a
    // non-repeating objective from its own seed stream.
    constexpr std::array kinds{
        CampaignMissionKind::SecureArena,
        CampaignMissionKind::RecoverKey,
        CampaignMissionKind::SecureTerminal,
        CampaignMissionKind::RecoverAndSecure,
    };
    CampaignMissionKind previous = CampaignMissionKind::SecureArena;
    const std::uint32_t targetDeck = (safeFloor - 2U) / 4U;
    for (std::uint32_t deck = 0U; deck <= targetDeck; ++deck) {
        auto order = kinds;
        const wkc::core::Seed selection = wkc::core::deriveSeed(
            initialSeed, gdp::game::SeedDomain::CampaignMission, deck + 1U);
        std::swap(order[3], order[selection % 4U]);
        std::swap(order[2], order[(selection >> 8U) % 3U]);
        std::swap(order[1], order[(selection >> 16U) % 2U]);
        if (order[0] == previous) std::rotate(order.begin(), order.begin() + 1, order.end());
        if (deck == targetDeck) return order[(safeFloor - 2U) % 4U];
        previous = order[3];
    }
    return CampaignMissionKind::SecureArena;
}

bool advanceCampaignMission(
    CampaignMissionState& mission, const std::uint32_t roomId,
    const CampaignMissionProgress progress) noexcept
{
    if (mission.completed || mission.targetRoomId == 0U) return false;
    if (mission.kind == CampaignMissionKind::RecoverAndSecure) {
        if (mission.secondaryRoomId == 0U) return false;
        if (roomId == mission.targetRoomId && progress.keysCollected > 0U) {
            mission.keyRecovered = true;
        }
        if (roomId == mission.secondaryRoomId && progress.arenaCleared) {
            mission.arenaSecured = true;
        }
        mission.completed = mission.keyRecovered && mission.arenaSecured;
        return mission.completed;
    }
    if (roomId != mission.targetRoomId) return false;
    switch (mission.kind) {
    case CampaignMissionKind::SecureArena:
        mission.completed = progress.arenaCleared;
        break;
    case CampaignMissionKind::RecoverKey:
        mission.completed = progress.keysCollected > 0U;
        break;
    case CampaignMissionKind::SecureTerminal:
        mission.activated |= progress.terminalActivated;
        mission.completed = mission.activated && progress.terminalAmbushCleared;
        break;
    case CampaignMissionKind::RecoverAndSecure:
        break;
    }
    return mission.completed;
}

std::uint32_t campaignMissionPendingRoom(
    const CampaignMissionState& mission) noexcept
{
    if (mission.kind == CampaignMissionKind::RecoverAndSecure
        && mission.keyRecovered && !mission.arenaSecured) {
        return mission.secondaryRoomId;
    }
    return mission.targetRoomId;
}

const char* campaignMissionName(const CampaignMissionKind kind) noexcept
{
    switch (kind) {
    case CampaignMissionKind::SecureArena: return "SecureArena";
    case CampaignMissionKind::RecoverKey: return "RecoverKey";
    case CampaignMissionKind::SecureTerminal: return "SecureTerminal";
    case CampaignMissionKind::RecoverAndSecure: return "RecoverAndSecure";
    }
    return "Invalid";
}

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
            playerState.grenades,
        },
        weapon.campaignProgress(),
        progression.state(),
    };
}

CampaignCarryover prepareNextCampaignFloorCarryover(
    CampaignCarryover carryover) noexcept
{
    // A floor change grants one tactical refill, not a new inventory capacity.
    // Replay restores the already captured floor-entry state and never calls this.
    if (carryover.player.grenades < 2U) {
        ++carryover.player.grenades;
    }
    return carryover;
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
        && player.grenades <= 2U
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
        && checkpoint.difficulty < Difficulty::Count
        && checkpoint.runIndex >= checkpoint.seedIndex
        && checkpoint.currentSeed
            == seedForRun(checkpoint.initialSeed, checkpoint.seedIndex)
        && validateCampaignCarryover(checkpoint.floorEntry);
}

float campaignDifficultyMultiplier(const std::uint32_t floor, const Difficulty difficulty) noexcept
{
    const std::uint32_t safeFloor = std::clamp(floor, 1U, campaignFloorCount);
    return std::min(2.0F, baseDifficultyMultiplier(difficulty)
        + (static_cast<float>(safeFloor - 1U) * 0.08F));
}

} // namespace gdp::gameplay
