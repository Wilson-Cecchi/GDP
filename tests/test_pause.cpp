#include "gameplay/Pause.hpp"

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

void testPauseNavigation()
{
    using namespace gdp::gameplay;
    PauseMenuState state;
    expect(activatePauseSelection(state) == PauseMenuCommand::None,
        "inactive pause menu cannot dispatch a command");
    openPauseMenu(state);
    expect(state.paused && state.selection == PauseMenuOption::Resume,
        "opening pause always selects the safe Resume option");
    movePauseSelection(state, 1);
    expect(state.selection == PauseMenuOption::RestartRun,
        "down navigation selects Restart Run");
    movePauseSelection(state, 20);
    expect(state.selection == PauseMenuOption::Settings,
        "navigation magnitude is normalized to one deterministic step");
    movePauseSelection(state, 1);
    expect(state.selection == PauseMenuOption::Controls,
        "down navigation selects Controls after Settings");
    movePauseSelection(state, 1);
    expect(state.selection == PauseMenuOption::MainMenu,
        "down navigation selects Main Menu after Controls");
    movePauseSelection(state, 1);
    expect(state.selection == PauseMenuOption::ExitGame,
        "down navigation selects Exit Game after Main Menu");
    movePauseSelection(state, 1);
    expect(state.selection == PauseMenuOption::Resume,
        "down navigation wraps from Exit Game to Resume");
    movePauseSelection(state, -1);
    expect(state.selection == PauseMenuOption::ExitGame,
        "up navigation wraps from Resume to Exit Game");
}

void testPauseCommands()
{
    using namespace gdp::gameplay;
    PauseMenuState state;
    openPauseMenu(state);
    expect(activatePauseSelection(state) == PauseMenuCommand::Resume
            && !state.paused && state.selection == PauseMenuOption::Resume,
        "Resume closes and resets the pause menu");

    openPauseMenu(state);
    movePauseSelection(state, 1);
    expect(activatePauseSelection(state) == PauseMenuCommand::RestartRun
            && state.paused,
        "Restart Run is reported without mutating external run ownership");
    movePauseSelection(state, 1);
    expect(activatePauseSelection(state) == PauseMenuCommand::Settings
            && state.paused,
        "Settings is reported without mutating pause ownership");
    movePauseSelection(state, 1);
    expect(activatePauseSelection(state) == PauseMenuCommand::Controls
            && state.paused,
        "Controls leaves the run paused");
    movePauseSelection(state, 1);
    expect(activatePauseSelection(state) == PauseMenuCommand::MainMenu
            && state.paused,
        "Main Menu is reported without mutating external run ownership");
    movePauseSelection(state, 1);
    expect(activatePauseSelection(state) == PauseMenuCommand::ExitGame
            && state.paused,
        "Exit Game is reported without hiding the menu before shutdown");
    closePauseMenu(state);
    expect(state == PauseMenuState{},
        "explicit close restores the default pause state");
}

void testDirectPauseSelection()
{
    using namespace gdp::gameplay;
    PauseMenuState state;
    selectPauseMenuOption(state, PauseMenuOption::ExitGame);
    expect(state.selection == PauseMenuOption::Resume,
        "pointer selection is ignored while the pause menu is inactive");

    openPauseMenu(state);
    selectPauseMenuOption(state, PauseMenuOption::RestartRun);
    expect(state.selection == PauseMenuOption::RestartRun,
        "pointer selection can target Restart Run directly");
    selectPauseMenuOption(state, PauseMenuOption::Settings);
    expect(state.selection == PauseMenuOption::Settings,
        "pointer selection can target Settings directly");
    selectPauseMenuOption(state, PauseMenuOption::Controls);
    expect(state.selection == PauseMenuOption::Controls,
        "pointer selection can target Controls directly");
    selectPauseMenuOption(state, PauseMenuOption::MainMenu);
    expect(state.selection == PauseMenuOption::MainMenu,
        "pointer selection can target Main Menu directly");
    selectPauseMenuOption(state, PauseMenuOption::ExitGame);
    expect(state.selection == PauseMenuOption::ExitGame,
        "pointer selection can target Exit Game directly");
    selectPauseMenuOption(state, PauseMenuOption::Resume);
    expect(state.selection == PauseMenuOption::Resume,
        "pointer selection can return to Resume directly");
    selectPauseMenuOption(state, PauseMenuOption::Count);
    expect(state.selection == PauseMenuOption::Resume,
        "invalid pointer selection is ignored");
}

} // namespace

int main()
{
    testPauseNavigation();
    testPauseCommands();
    testDirectPauseSelection();
    if (failures != 0) {
        std::cerr << failures << " pause test(s) failed\n";
        return 1;
    }
    std::cout << "All GDP pause menu tests passed\n";
    return 0;
}
