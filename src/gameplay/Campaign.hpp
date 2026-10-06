#pragma once

#include "gameplay/Player.hpp"
#include "gameplay/Progression.hpp"
#include "gameplay/Weapon.hpp"
#include "gameplay/Session.hpp"

#include <cstdint>

namespace gdp::gameplay {

inline constexpr std::uint32_t currentCampaignRulesVersion = 39U;

struct CampaignFloorLayout {
    std::uint32_t roomCount{8U};
    std::uint32_t loopBudget{1U};

    constexpr bool operator==(const CampaignFloorLayout&) const noexcept = default;
};

enum class CampaignMissionKind : std::uint8_t {
    SecureArena,
    RecoverKey,
    SecureTerminal,
};

struct CampaignMissionState {
    CampaignMissionKind kind{CampaignMissionKind::SecureArena};
    std::uint32_t targetRoomId{0U};
    bool activated{false};
    bool completed{false};
};

struct CampaignMissionProgress {
    std::uint32_t keysCollected{0U};
    bool arenaCleared{false};
    bool terminalActivated{false};
    bool terminalAmbushCleared{false};
};

[[nodiscard]] CampaignFloorLayout campaignFloorLayout(
    wkc::core::Seed floorSeed, std::uint32_t floor) noexcept;
[[nodiscard]] CampaignMissionKind campaignMissionKind(
    wkc::core::Seed floorSeed, std::uint32_t floor) noexcept;
[[nodiscard]] bool advanceCampaignMission(
    CampaignMissionState& mission, std::uint32_t roomId,
    CampaignMissionProgress progress) noexcept;
[[nodiscard]] const char* campaignMissionName(CampaignMissionKind kind) noexcept;

struct CampaignCarryover {
    PlayerCampaignProgress player{};
    WeaponCampaignProgress weapon{};
    ProgressionState progression{};

    constexpr bool operator==(const CampaignCarryover&) const noexcept = default;
};

struct CampaignCheckpoint {
    std::uint32_t rulesVersion{currentCampaignRulesVersion};
    wkc::core::Seed initialSeed{0U};
    wkc::core::Seed currentSeed{0U};
    std::uint32_t runIndex{0U};
    std::uint32_t seedIndex{0U};
    std::uint32_t floor{1U};
    CampaignCarryover floorEntry{};
    Difficulty difficulty{Difficulty::Normal};

    constexpr bool operator==(const CampaignCheckpoint&) const noexcept = default;
};

[[nodiscard]] CampaignCarryover captureCampaignCarryover(
    const Player& player,
    const Weapon& weapon,
    const ProgressionSystem& progression) noexcept;
[[nodiscard]] bool validateCampaignCarryover(
    const CampaignCarryover& carryover) noexcept;
[[nodiscard]] bool restoreCampaignCarryover(
    const CampaignCarryover& carryover,
    Player& player,
    Weapon& weapon,
    ProgressionSystem& progression) noexcept;
[[nodiscard]] bool validateCampaignCheckpoint(
    const CampaignCheckpoint& checkpoint) noexcept;
[[nodiscard]] float campaignDifficultyMultiplier(
    std::uint32_t floor,
    Difficulty difficulty = Difficulty::Normal) noexcept;

} // namespace gdp::gameplay
