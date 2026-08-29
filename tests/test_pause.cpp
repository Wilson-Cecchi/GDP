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
    expect(state.selection == PauseMenuOption::ExitGame,
        "navigation magnitude is normalized to one deterministic step");
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
    expect(activatePauseSelection(state) == PauseMenuCommand::ExitGame
            && state.paused,
        "Exit Game is reported without hiding the menu before shutdown");
    closePauseMenu(state);
    expect(state == PauseMenuState{},
        "explicit close restores the default pause state");
}

} // namespace

int main()
{
    testPauseNavigation();
    testPauseCommands();
    if (failures != 0) {
        std::cerr << failures << " pause test(s) failed\n";
        return 1;
    }
    std::cout << "All GDP pause menu tests passed\n";
    return 0;
}
