#include "gameplay/MainMenu.hpp"

namespace gdp::gameplay {

void openMainMenu(MainMenuState& state) noexcept
{
    state.active = true;
    state.selection = MainMenuOption::StartRun;
}

void closeMainMenu(MainMenuState& state) noexcept
{
    state.active = false;
    state.selection = MainMenuOption::StartRun;
}

void moveMainMenuSelection(MainMenuState& state, const int direction) noexcept
{
    if (!state.active || direction == 0) {
        return;
    }
    constexpr int optionCount = static_cast<int>(MainMenuOption::Count);
    int selection = static_cast<int>(state.selection) + (direction > 0 ? 1 : -1);
    if (selection < 0) {
        selection = optionCount - 1;
    } else if (selection >= optionCount) {
        selection = 0;
    }
    state.selection = static_cast<MainMenuOption>(selection);
}

void selectMainMenuOption(
    MainMenuState& state,
    const MainMenuOption option) noexcept
{
    if (state.active && option < MainMenuOption::Count) {
        state.selection = option;
    }
}

MainMenuCommand activateMainMenuSelection(MainMenuState& state) noexcept
{
    if (!state.active) {
        return MainMenuCommand::None;
    }
    switch (state.selection) {
    case MainMenuOption::StartRun:
        closeMainMenu(state);
        return MainMenuCommand::StartRun;
    case MainMenuOption::ExitGame:
        return MainMenuCommand::ExitGame;
    case MainMenuOption::Count:
        break;
    }
    return MainMenuCommand::None;
}

} // namespace gdp::gameplay
