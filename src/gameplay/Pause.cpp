#include "gameplay/Pause.hpp"

namespace gdp::gameplay {

void openPauseMenu(PauseMenuState& state) noexcept
{
    state.paused = true;
    state.selection = PauseMenuOption::Resume;
}

void closePauseMenu(PauseMenuState& state) noexcept
{
    state.paused = false;
    state.selection = PauseMenuOption::Resume;
}

void movePauseSelection(PauseMenuState& state, const int direction) noexcept
{
    if (!state.paused || direction == 0) {
        return;
    }
    constexpr int optionCount = static_cast<int>(PauseMenuOption::Count);
    int selection = static_cast<int>(state.selection);
    selection += direction > 0 ? 1 : -1;
    if (selection < 0) {
        selection = optionCount - 1;
    } else if (selection >= optionCount) {
        selection = 0;
    }
    state.selection = static_cast<PauseMenuOption>(selection);
}

void selectPauseMenuOption(
    PauseMenuState& state,
    const PauseMenuOption option) noexcept
{
    if (state.paused && option < PauseMenuOption::Count) {
        state.selection = option;
    }
}

PauseMenuCommand activatePauseSelection(PauseMenuState& state) noexcept
{
    if (!state.paused) {
        return PauseMenuCommand::None;
    }
    switch (state.selection) {
    case PauseMenuOption::Resume:
        closePauseMenu(state);
        return PauseMenuCommand::Resume;
    case PauseMenuOption::RestartRun:
        return PauseMenuCommand::RestartRun;
    case PauseMenuOption::MainMenu:
        return PauseMenuCommand::MainMenu;
    case PauseMenuOption::ExitGame:
        return PauseMenuCommand::ExitGame;
    case PauseMenuOption::Count:
        break;
    }
    return PauseMenuCommand::None;
}

} // namespace gdp::gameplay
