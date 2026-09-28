#pragma once

#include "gameplay/Player.hpp"
#include "gameplay/Progression.hpp"
#include "gameplay/Weapon.hpp"

#include <cstdint>

namespace gdp::gameplay {

inline constexpr std::uint32_t currentCampaignRulesVersion = 30U;

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
    std::uint32_t floor) noexcept;

} // namespace gdp::gameplay
