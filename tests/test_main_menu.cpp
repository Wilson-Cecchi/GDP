#include "gameplay/MainMenu.hpp"

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

void testMainMenuNavigation()
{
    using namespace gdp::gameplay;

    MainMenuState menu;
    expect(menu.active, "main menu starts active");
    expect(menu.selection == MainMenuOption::StartRun,
        "main menu starts on Start Run");

    moveMainMenuSelection(menu, 1);
    expect(menu.selection == MainMenuOption::Speedrun,
        "down selects Speedrun");
    moveMainMenuSelection(menu, 4);
    expect(menu.selection == MainMenuOption::Settings,
        "navigation magnitude is normalized to one step");
    moveMainMenuSelection(menu, 4);
    expect(menu.selection == MainMenuOption::ExitGame,
        "down selects Exit Game after Settings");
    moveMainMenuSelection(menu, 1);
    expect(menu.selection == MainMenuOption::StartRun,
        "down wraps from Exit Game to Start Run");
    moveMainMenuSelection(menu, -1);
    expect(menu.selection == MainMenuOption::ExitGame,
        "up wraps to Exit Game");

    selectMainMenuOption(menu, MainMenuOption::StartRun);
    expect(menu.selection == MainMenuOption::StartRun,
        "pointer selection chooses Start Run");
    selectMainMenuOption(menu, MainMenuOption::Count);
    expect(menu.selection == MainMenuOption::StartRun,
        "invalid pointer selection is ignored");
}

void testMainMenuActivation()
{
    using namespace gdp::gameplay;

    MainMenuState menu;
    selectMainMenuOption(menu, MainMenuOption::Settings);
    expect(activateMainMenuSelection(menu) == MainMenuCommand::Settings
            && menu.active,
        "Settings emits its command without closing the main menu");
    selectMainMenuOption(menu, MainMenuOption::ExitGame);
    expect(activateMainMenuSelection(menu) == MainMenuCommand::ExitGame,
        "Exit Game emits its command");
    expect(menu.active, "Exit Game keeps menu visible until application exits");

    selectMainMenuOption(menu, MainMenuOption::Speedrun);
    expect(activateMainMenuSelection(menu) == MainMenuCommand::Speedrun
            && !menu.active,
        "Speedrun emits its dedicated command and closes the menu");
    openMainMenu(menu);

    selectMainMenuOption(menu, MainMenuOption::StartRun);
    expect(activateMainMenuSelection(menu) == MainMenuCommand::StartRun,
        "Start Run emits its command");
    expect(!menu.active, "Start Run closes the menu");
    expect(menu.selection == MainMenuOption::StartRun,
        "closing resets the safe initial selection");
    expect(activateMainMenuSelection(menu) == MainMenuCommand::None,
        "inactive menu cannot emit duplicate commands");

    openMainMenu(menu);
    expect(menu.active && menu.selection == MainMenuOption::StartRun,
        "opening restores the default menu state");
}

} // namespace

int main()
{
    testMainMenuNavigation();
    testMainMenuActivation();
    if (failures != 0) {
        std::cerr << failures << " main menu test(s) failed\n";
        return 1;
    }
    std::cout << "All GDP main menu tests passed\n";
    return 0;
}
