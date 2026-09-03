#include "gameplay/ResultMenu.hpp"

namespace gdp::gameplay {

void openResultMenu(ResultMenuState& state) noexcept
{
    state.active = true;
    state.selection = ResultMenuOption::PlayAgain;
}

void closeResultMenu(ResultMenuState& state) noexcept
{
    state.active = false;
    state.selection = ResultMenuOption::PlayAgain;
}

void moveResultMenuSelection(
    ResultMenuState& state,
    const int direction) noexcept
{
    if (!state.active || direction == 0) {
        return;
    }
    constexpr int optionCount = static_cast<int>(ResultMenuOption::Count);
    int selection = static_cast<int>(state.selection);
    selection += direction > 0 ? 1 : -1;
    if (selection < 0) {
        selection = optionCount - 1;
    } else if (selection >= optionCount) {
        selection = 0;
    }
    state.selection = static_cast<ResultMenuOption>(selection);
}

void selectResultMenuOption(
    ResultMenuState& state,
    const ResultMenuOption option) noexcept
{
    if (state.active && option < ResultMenuOption::Count) {
        state.selection = option;
    }
}

ResultMenuCommand activateResultMenuSelection(ResultMenuState& state) noexcept
{
    if (!state.active) {
        return ResultMenuCommand::None;
    }
    switch (state.selection) {
    case ResultMenuOption::PlayAgain:
        closeResultMenu(state);
        return ResultMenuCommand::PlayAgain;
    case ResultMenuOption::ReplaySeed:
        closeResultMenu(state);
        return ResultMenuCommand::ReplaySeed;
    case ResultMenuOption::MainMenu:
        closeResultMenu(state);
        return ResultMenuCommand::MainMenu;
    case ResultMenuOption::ExitGame:
        return ResultMenuCommand::ExitGame;
    case ResultMenuOption::Count:
        break;
    }
    return ResultMenuCommand::None;
}

} // namespace gdp::gameplay
