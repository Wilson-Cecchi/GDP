#include "gameplay/ExitConfirmation.hpp"

namespace gdp::gameplay {

void openExitConfirmation(ExitConfirmationState& state) noexcept
{
    state.active = true;
    state.selection = ExitConfirmationOption::Cancel;
}

void closeExitConfirmation(ExitConfirmationState& state) noexcept
{
    state = {};
}

void moveExitConfirmationSelection(
    ExitConfirmationState& state,
    const int direction) noexcept
{
    if (!state.active || direction == 0) {
        return;
    }
    state.selection = state.selection == ExitConfirmationOption::Cancel
        ? ExitConfirmationOption::ExitGame
        : ExitConfirmationOption::Cancel;
}

void selectExitConfirmationOption(
    ExitConfirmationState& state,
    const ExitConfirmationOption option) noexcept
{
    if (state.active && option < ExitConfirmationOption::Count) {
        state.selection = option;
    }
}

ExitConfirmationCommand activateExitConfirmationSelection(
    ExitConfirmationState& state) noexcept
{
    if (!state.active) {
        return ExitConfirmationCommand::None;
    }
    const ExitConfirmationCommand command
        = state.selection == ExitConfirmationOption::ExitGame
        ? ExitConfirmationCommand::ExitGame
        : ExitConfirmationCommand::Cancel;
    closeExitConfirmation(state);
    return command;
}

} // namespace gdp::gameplay
