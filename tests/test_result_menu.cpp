#include "gameplay/ResultMenu.hpp"

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

void testResultMenuNavigation()
{
    using namespace gdp::gameplay;
    ResultMenuState state;
    expect(activateResultMenuSelection(state) == ResultMenuCommand::None,
        "inactive result menu cannot dispatch a command");
    moveResultMenuSelection(state, 1);
    selectResultMenuOption(state, ResultMenuOption::ExitGame);
    expect(state == ResultMenuState{},
        "inactive result menu ignores navigation and direct selection");

    openResultMenu(state);
    expect(state.active && state.selection == ResultMenuOption::PlayAgain,
        "opening result menu selects the safe Play Again option");
    moveResultMenuSelection(state, 1);
    expect(state.selection == ResultMenuOption::ReplaySeed,
        "down navigation selects same-seed replay");
    moveResultMenuSelection(state, 8);
    expect(state.selection == ResultMenuOption::MainMenu,
        "positive navigation advances by one deterministic step");
    moveResultMenuSelection(state, 8);
    expect(state.selection == ResultMenuOption::ExitGame,
        "positive navigation reaches Exit Game by one deterministic step");
    moveResultMenuSelection(state, 1);
    expect(state.selection == ResultMenuOption::PlayAgain,
        "down navigation wraps from Exit Game to Play Again");
    moveResultMenuSelection(state, -1);
    expect(state.selection == ResultMenuOption::ExitGame,
        "up navigation wraps from Play Again to Exit Game");
    selectResultMenuOption(state, ResultMenuOption::PlayAgain);
    selectResultMenuOption(state, ResultMenuOption::Count);
    expect(state.selection == ResultMenuOption::PlayAgain,
        "invalid direct selection is ignored");
}

void testResultMenuCommands()
{
    using namespace gdp::gameplay;
    ResultMenuState state;
    openResultMenu(state);
    expect(activateResultMenuSelection(state) == ResultMenuCommand::PlayAgain
            && state == ResultMenuState{},
        "Play Again closes and resets the result menu");

    openResultMenu(state);
    selectResultMenuOption(state, ResultMenuOption::ReplaySeed);
    expect(activateResultMenuSelection(state) == ResultMenuCommand::ReplaySeed
            && state == ResultMenuState{},
        "Replay Seed emits a dedicated command and resets the result menu");

    openResultMenu(state);
    selectResultMenuOption(state, ResultMenuOption::MainMenu);
    expect(activateResultMenuSelection(state) == ResultMenuCommand::MainMenu
            && state == ResultMenuState{},
        "Main Menu closes and resets the result menu");

    openResultMenu(state);
    selectResultMenuOption(state, ResultMenuOption::ExitGame);
    expect(activateResultMenuSelection(state) == ResultMenuCommand::ExitGame
            && state.active,
        "Exit Game stays visible until the application exits");
    closeResultMenu(state);
    expect(state == ResultMenuState{},
        "explicit close restores the default result state");
}

} // namespace

int main()
{
    testResultMenuNavigation();
    testResultMenuCommands();
    if (failures != 0) {
        std::cerr << failures << " result menu test(s) failed\n";
        return 1;
    }
    std::cout << "All GDP result menu tests passed\n";
    return 0;
}
