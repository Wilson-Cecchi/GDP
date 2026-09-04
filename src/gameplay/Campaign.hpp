#pragma once

#include "gameplay/Player.hpp"
#include "gameplay/Progression.hpp"
#include "gameplay/Weapon.hpp"

#include <cstdint>

namespace gdp::gameplay {

struct CampaignCarryover {
    PlayerCampaignProgress player{};
    WeaponCampaignProgress weapon{};
    ProgressionState progression{};
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

} // namespace gdp::gameplay
