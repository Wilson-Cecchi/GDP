#include "gameplay/SettingsMenu.hpp"

#include <cmath>
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

void testSettingsNavigationAndReturn()
{
    using namespace gdp::gameplay;
    SettingsMenuState state;
    moveSettingsMenuSelection(state, 1);
    expect(state == SettingsMenuState{},
        "inactive settings ignore navigation");

    openSettingsMenu(state, SettingsMenuOrigin::MainMenu);
    expect(state.active && state.selection == SettingsMenuOption::MasterVolume,
        "settings open on the first safe row");
    moveSettingsMenuSelection(state, -1);
    expect(state.selection == SettingsMenuOption::Back,
        "up navigation wraps to Back");
    expect(activateSettingsMenuSelection(state)
            == SettingsMenuCommand::BackToMainMenu
            && state == SettingsMenuState{},
        "Back restores the main menu origin");

    openSettingsMenu(state, SettingsMenuOrigin::PauseMenu);
    selectSettingsMenuOption(state, SettingsMenuOption::Back);
    expect(activateSettingsMenuSelection(state)
            == SettingsMenuCommand::BackToPauseMenu,
        "Back preserves the pause menu origin");
}

void testRuntimeSettingsClamps()
{
    using namespace gdp::gameplay;
    RuntimeSettings settings;
    expect(!adjustRuntimeSetting(
               settings, SettingsMenuOption::MasterVolume, 1),
        "volume cannot exceed 100 percent");
    expect(adjustRuntimeSetting(
               settings, SettingsMenuOption::MasterVolume, -1)
            && settings.masterVolumePercent == 90U,
        "volume decreases in ten percent steps");
    for (int index = 0; index < 20; ++index) {
        (void)adjustRuntimeSetting(
            settings, SettingsMenuOption::MasterVolume, -1);
    }
    expect(settings.masterVolumePercent == 0U,
        "volume clamps at zero");

    for (int index = 0; index < 30; ++index) {
        (void)adjustRuntimeSetting(
            settings, SettingsMenuOption::MouseSensitivity, 1);
        (void)adjustRuntimeSetting(
            settings, SettingsMenuOption::FieldOfView, 1);
    }
    expect(settings.mouseSensitivityPercent == 200U
            && settings.fieldOfViewDegrees == 100U,
        "sensitivity and FOV clamp at documented maxima");
    expect(std::abs(mouseSensitivity(settings) - 0.24F) < 0.0001F,
        "sensitivity percent maps to the established player baseline");
    expect(std::abs(normalizedVolume(250U) - 1.0F) < 0.0001F,
        "volume normalization clamps invalid percentages");
}

} // namespace

int main()
{
    testSettingsNavigationAndReturn();
    testRuntimeSettingsClamps();
    if (failures != 0) {
        std::cerr << failures << " settings menu test(s) failed\n";
        return 1;
    }
    std::cout << "All GDP settings menu tests passed\n";
    return 0;
}
