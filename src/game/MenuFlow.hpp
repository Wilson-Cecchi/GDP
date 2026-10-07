#pragma once

#include "gameplay/Campaign.hpp"
#include "gameplay/SettingsMenu.hpp"
#include "gameplay/Speedrun.hpp"
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace gdp::game {
enum class MenuPage : std::uint8_t {
    Root, CampaignSlots, CampaignSlot, Difficulty, Overwrite, Speedrun, Leaderboard,
    Settings, Display, Audio, Controls, KeyboardControls, GamepadControls,
    Bindings, MoreBindings, GamepadBindings, MoreGamepadBindings,
};
enum class MenuCommand : std::uint8_t {
    None, TestMode, ContinueCampaign, NewCampaign, StartSpeedrun, BestSeed,
    OpenSettings, CloseSettings, ExitGame,
};
struct CampaignSlotInfo {
    std::optional<gameplay::CampaignCheckpoint> checkpoint;
    bool occupied{false};
};
struct MenuFlowState {
    MenuPage page{MenuPage::Root};
    std::size_t selection{0U};
    std::size_t slot{0U};
    gameplay::Difficulty difficulty{gameplay::Difficulty::Normal};
    std::optional<gameplay::ControlAction> capture;
    bool captureGamepad{false};
    std::string notice;
};
struct MenuRow {
    std::string text;
    bool enabled{true};
    std::optional<gameplay::SettingsMenuOption> setting;
    std::optional<gameplay::ControlAction> binding;
    std::optional<MenuPage> destination;
    MenuCommand command{MenuCommand::None};
};
struct MenuView {
    std::string title;
    std::string subtitle;
    std::vector<MenuRow> rows;
};
[[nodiscard]] MenuView makeMenuView(const MenuFlowState&, const std::array<CampaignSlotInfo, 3>&,
    const std::vector<gameplay::SpeedrunRecord>& leaderboard, const gameplay::RuntimeSettings&, bool testMode);
void moveMenuSelection(MenuFlowState&, const MenuView&, int direction) noexcept;
[[nodiscard]] MenuCommand activateMenu(MenuFlowState&, const MenuView&, const std::array<CampaignSlotInfo, 3>&);
[[nodiscard]] MenuCommand backMenu(MenuFlowState&) noexcept;
} // namespace gdp::game
