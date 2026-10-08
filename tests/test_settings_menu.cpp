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

    expect(adjustRuntimeSetting(
               settings, SettingsMenuOption::InvertMouseY, 1)
            && settings.invertMouseY,
        "right enables vertical mouse inversion");
    expect(!adjustRuntimeSetting(
               settings, SettingsMenuOption::InvertMouseY, 1),
        "repeating the enabled value does not report a change");
    expect(adjustRuntimeSetting(
               settings, SettingsMenuOption::InvertMouseY, -1)
            && !settings.invertMouseY,
        "left disables vertical mouse inversion");
    expect(adjustRuntimeSetting(
               settings, SettingsMenuOption::Fullscreen, 1)
            && settings.fullscreen,
        "right enables fullscreen");
    expect(adjustRuntimeSetting(
               settings, SettingsMenuOption::Fullscreen, -1)
            && !settings.fullscreen,
        "left restores windowed mode");
    expect(adjustRuntimeSetting(
               settings, SettingsMenuOption::VerticalSync, -1)
            && !settings.verticalSync,
        "left disables vertical synchronization");
    expect(!adjustRuntimeSetting(
               settings, SettingsMenuOption::VerticalSync, -1),
        "repeating disabled VSync does not report a change");
    expect(adjustRuntimeSetting(
               settings, SettingsMenuOption::VerticalSync, 1)
            && settings.verticalSync,
        "right enables vertical synchronization");

    expect(adjustRuntimeSetting(
               settings, SettingsMenuOption::GraphicsQuality, -1)
            && settings.graphicsQuality == GraphicsQuality::Low,
        "left selects a lighter graphics quality profile");
    for (int index = 0; index < 10; ++index) {
        (void)adjustRuntimeSetting(
            settings, SettingsMenuOption::GraphicsQuality, 1);
    }
    expect(settings.graphicsQuality == GraphicsQuality::Ultra
            && !adjustRuntimeSetting(
                settings, SettingsMenuOption::GraphicsQuality, 1),
        "graphics quality clamps at Ultra");

    expect(graphicsQualitySettings(GraphicsQuality::Potato)
            == GraphicsQualitySettings{50U, 2U, 0U, 0U, 1U, false}
            && graphicsQualitySettings(GraphicsQuality::Low)
                == GraphicsQualitySettings{67U, 4U, 0U, 0U, 1U, false}
            && graphicsQualitySettings(GraphicsQuality::Medium)
                == GraphicsQualitySettings{85U, 6U, 1U, 3U, 2U, true}
            && graphicsQualitySettings(GraphicsQuality::High)
                == GraphicsQualitySettings{100U, 8U, 1U, 6U, 2U, true}
            && graphicsQualitySettings(GraphicsQuality::Ultra)
                == GraphicsQualitySettings{100U, 8U, 2U, 8U, 2U, true},
        "graphics profiles expose deterministic rendering budgets");
    expect(graphicsQualityName(GraphicsQuality::Potato) == "POTATO"
            && graphicsQualityForName("HIGH") == GraphicsQuality::High
            && !graphicsQualityForName("UNKNOWN"),
        "graphics profile names round-trip without accepting unknown values");

    expect(adjustRuntimeSetting(
               settings, SettingsMenuOption::WindowResolution, -1)
            && settings.windowResolution == WindowResolution::R960x540,
        "left selects the previous window resolution preset");
    expect(!adjustRuntimeSetting(
               settings, SettingsMenuOption::WindowResolution, -1),
        "window resolution clamps at the smallest preset");
    for (int index = 0; index < 10; ++index) {
        (void)adjustRuntimeSetting(
            settings, SettingsMenuOption::WindowResolution, 1);
    }
    expect(settings.windowResolution == WindowResolution::R1920x1080,
        "window resolution clamps at the largest preset");
    expect(windowResolutionSize(WindowResolution::R1600x900)
            == WindowResolutionSize{1600U, 900U}
            && windowResolutionForSize(1920U, 1080U)
                == WindowResolution::R1920x1080
            && !windowResolutionForSize(800U, 600U),
        "resolution catalog maps only supported 16:9 sizes");

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

    expect(adjustRuntimeSetting(settings, SettingsMenuOption::GamepadVibration, -1)
            && !settings.gamepadVibration,
        "controller vibration toggle can be disabled");
    for (int index = 0; index < 20; ++index)
        (void)adjustRuntimeSetting(settings, SettingsMenuOption::GamepadVibrationStrength, -1);
    expect(settings.gamepadVibrationPercent == 0U
            && !adjustRuntimeSetting(settings, SettingsMenuOption::GamepadVibrationStrength, -1),
        "controller vibration strength clamps at zero");
    for (int index = 0; index < 20; ++index)
        (void)adjustRuntimeSetting(settings, SettingsMenuOption::GamepadVibrationStrength, 1);
    expect(settings.gamepadVibrationPercent == 100U
            && !adjustRuntimeSetting(settings, SettingsMenuOption::GamepadVibrationStrength, 1),
        "controller vibration strength clamps at one hundred percent");

    expect(adjustRuntimeSetting(settings, SettingsMenuOption::GamepadMoveStick, 1)
            && settings.gamepadSouthpaw
            && adjustRuntimeSetting(settings, SettingsMenuOption::GamepadMoveStick, -1)
            && !settings.gamepadSouthpaw,
        "movement stick can swap between left and right without changing menu controls");
    expect(adjustRuntimeSetting(settings, SettingsMenuOption::GamepadLookHorizontal, 1)
            && settings.gamepadLookHorizontalPercent == 110U
            && adjustRuntimeSetting(settings, SettingsMenuOption::GamepadLookVertical, -1)
            && settings.gamepadLookVerticalPercent == 90U
            && adjustRuntimeSetting(settings, SettingsMenuOption::GamepadAimSensitivity, -1)
            && settings.gamepadAimSensitivityPercent == 90U,
        "controller camera axes and aiming sensitivity adjust independently");
    expect(adjustRuntimeSetting(settings, SettingsMenuOption::GamepadMoveDeadzone, -1)
            && settings.gamepadMoveDeadzonePercent == 16U
            && adjustRuntimeSetting(settings, SettingsMenuOption::GamepadLookDeadzone, 1)
            && settings.gamepadLookDeadzonePercent == 20U
            && adjustRuntimeSetting(settings, SettingsMenuOption::GamepadInvertY, 1)
            && settings.invertGamepadY,
        "controller deadzones and Y inversion have separate settings");

    settings.windowResolution = static_cast<WindowResolution>(99U);
    settings.graphicsQuality = static_cast<GraphicsQuality>(99U);
    settings.gamepadVibrationPercent = 200U;
    settings.gamepadLookHorizontalPercent = 250U;
    settings.gamepadMoveDeadzonePercent = 90U;
    const RuntimeSettings sanitized = sanitizeRuntimeSettings(settings);
    expect(sanitized.windowResolution == WindowResolution::R1280x720
            && sanitized.graphicsQuality == GraphicsQuality::Medium
            && sanitized.gamepadVibrationPercent == 100U
            && sanitized.gamepadLookHorizontalPercent == 200U
            && sanitized.gamepadMoveDeadzonePercent == 40U,
        "invalid video enums recover to their default presets");
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
