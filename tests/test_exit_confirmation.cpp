#include "gameplay/ExitConfirmation.hpp"

#include <iostream>
#include <string_view>

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void testSafeConfirmationFlow()
{
    using namespace gdp::gameplay;

    ExitConfirmationState state;
    moveExitConfirmationSelection(state, 1);
    selectExitConfirmationOption(state, ExitConfirmationOption::ExitGame);
    expect(state == ExitConfirmationState{}
            && activateExitConfirmationSelection(state)
                == ExitConfirmationCommand::None,
        "inactive confirmation ignores navigation, selection and activation");

    openExitConfirmation(state);
    expect(state.active
            && state.selection == ExitConfirmationOption::Cancel,
        "opening confirmation always selects the safe Cancel option");
    expect(activateExitConfirmationSelection(state)
            == ExitConfirmationCommand::Cancel
            && state == ExitConfirmationState{},
        "activating Cancel closes and resets confirmation");

    openExitConfirmation(state);
    moveExitConfirmationSelection(state, 5);
    expect(state.selection == ExitConfirmationOption::ExitGame,
        "positive navigation moves one deterministic step to Exit Game");
    moveExitConfirmationSelection(state, -4);
    expect(state.selection == ExitConfirmationOption::Cancel,
        "negative navigation wraps back to Cancel");
    selectExitConfirmationOption(state, ExitConfirmationOption::ExitGame);
    selectExitConfirmationOption(state, ExitConfirmationOption::Count);
    expect(state.selection == ExitConfirmationOption::ExitGame,
        "direct selection targets Exit Game and ignores invalid options");
    expect(activateExitConfirmationSelection(state)
            == ExitConfirmationCommand::ExitGame
            && state == ExitConfirmationState{},
        "confirmed exit emits once and resets the dialog");
}

} // namespace

int main()
{
    testSafeConfirmationFlow();
    if (failures != 0) {
        std::cerr << failures << " exit confirmation test(s) failed\n";
        return 1;
    }
    std::cout << "All GDP exit confirmation tests passed\n";
    return 0;
}
