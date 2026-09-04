#pragma once

#include <cstdint>

namespace gdp::gameplay {

enum class MainMenuOption : std::uint8_t {
    StartRun,
    Campaign,
    Speedrun,
    BestSeed,
    Settings,
    ExitGame,
    Count,
};

enum class MainMenuCommand : std::uint8_t {
    None,
    StartRun,
    Campaign,
    Speedrun,
    BestSeed,
    Settings,
    ExitGame,
};

struct MainMenuState {
    bool active{true};
    MainMenuOption selection{MainMenuOption::StartRun};

    constexpr bool operator==(const MainMenuState&) const noexcept = default;
};

void openMainMenu(MainMenuState& state) noexcept;
void closeMainMenu(MainMenuState& state) noexcept;
void moveMainMenuSelection(MainMenuState& state, int direction) noexcept;
void selectMainMenuOption(MainMenuState& state, MainMenuOption option) noexcept;
[[nodiscard]] MainMenuCommand activateMainMenuSelection(
    MainMenuState& state) noexcept;

} // namespace gdp::gameplay
