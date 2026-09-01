#pragma once

#include <cstdint>

namespace gdp::gameplay {

enum class PauseMenuOption : std::uint8_t {
    Resume,
    RestartRun,
    MainMenu,
    ExitGame,
    Count,
};

enum class PauseMenuCommand : std::uint8_t {
    None,
    Resume,
    RestartRun,
    MainMenu,
    ExitGame,
};

struct PauseMenuState {
    bool paused{false};
    PauseMenuOption selection{PauseMenuOption::Resume};

    constexpr bool operator==(const PauseMenuState&) const noexcept = default;
};

void openPauseMenu(PauseMenuState& state) noexcept;
void closePauseMenu(PauseMenuState& state) noexcept;
void movePauseSelection(PauseMenuState& state, int direction) noexcept;
void selectPauseMenuOption(
    PauseMenuState& state,
    PauseMenuOption option) noexcept;
[[nodiscard]] PauseMenuCommand activatePauseSelection(
    PauseMenuState& state) noexcept;

} // namespace gdp::gameplay
