#include "gameplay/SettingsMenu.hpp"
#include "game/MenuAdjustmentRepeat.hpp"

#include <cmath>
#include <chrono>
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
    expect(settings.fieldOfViewDegrees == 100U
            && settings.graphicsQuality == GraphicsQuality::Ultra
            && settings.fullscreen && settings.verticalSync
            && settings.uiScaleAuto && settings.uiScaleManualPercent == 100U,
        "fresh video settings use the specified defaults");
    expect(!adjustRuntimeSetting(
               settings, SettingsMenuOption::MasterVolume, 1),
        "volume cannot exceed 100 percent");
    expect(adjustRuntimeSetting(
               settings, SettingsMenuOption::MasterVolume, -1)
            && settings.masterVolumePercent == 99U,
        "volume decreases in one percent steps");
    for (int index = 0; index < 120; ++index) {
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
               settings, SettingsMenuOption::Fullscreen, -1)
            && !settings.fullscreen,
        "left restores windowed mode");
    expect(adjustRuntimeSetting(
               settings, SettingsMenuOption::Fullscreen, 1)
            && settings.fullscreen,
        "right enables fullscreen");
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
            && settings.graphicsQuality == GraphicsQuality::High,
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
            == GraphicsQualitySettings{50U, 2U, 0U, 0U, 1U, false, false}
            && graphicsQualitySettings(GraphicsQuality::Low)
                == GraphicsQualitySettings{67U, 4U, 0U, 0U, 1U, false, false}
            && graphicsQualitySettings(GraphicsQuality::Medium)
                == GraphicsQualitySettings{85U, 6U, 1U, 3U, 2U, true}
            && graphicsQualitySettings(GraphicsQuality::High)
                == GraphicsQualitySettings{100U, 8U, 8U, 6U, 2U, true}
            && graphicsQualitySettings(GraphicsQuality::Ultra)
                == GraphicsQualitySettings{100U, 8U, 8U, 8U, 2U, true},
        "graphics profiles expose deterministic rendering budgets");
    expect(graphicsQualityName(GraphicsQuality::Potato) == "POTATO"
            && graphicsQualityForName("HIGH") == GraphicsQuality::High
            && graphicsQualityForName("CUSTOM") == GraphicsQuality::Custom
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
    expect(settings.windowResolution == WindowResolution::R3840x2160,
        "window resolution clamps at the largest preset");
    expect(windowResolutionSize(WindowResolution::R1600x900)
            == WindowResolutionSize{1600U, 900U}
            && windowResolutionForSize(2560U, 1440U)
                == WindowResolution::R2560x1440
            && windowResolutionForSize(1920U, 1080U)
                == WindowResolution::R1920x1080
            && !windowResolutionForSize(800U, 600U),
        "resolution catalog maps only supported 16:9 sizes");

    for (int index = 0; index < 250; ++index) {
        (void)adjustRuntimeSetting(
            settings, SettingsMenuOption::MouseSensitivity, 1);
        (void)adjustRuntimeSetting(
            settings, SettingsMenuOption::FieldOfView, 1);
    }
    expect(settings.mouseSensitivityPercent == 300U
            && settings.fieldOfViewDegrees == 120U,
        "sensitivity and FOV clamp at documented maxima");
    expect(std::abs(mouseSensitivity(settings) - 0.36F) < 0.0001F,
        "sensitivity percent maps to the established player baseline");
    expect(adjustRuntimeSetting(settings, SettingsMenuOption::MouseAimSensitivity, -1)
            && settings.mouseAimSensitivityPercent == 99U,
        "mouse aim sensitivity adjusts independently of normal look");
    expect(adjustRuntimeSetting(settings, SettingsMenuOption::UiScaleMode, 1)
            && !settings.uiScaleAuto
            && adjustRuntimeSetting(settings, SettingsMenuOption::UiScaleManual, 1)
            && settings.uiScaleManualPercent == 105U
            && adjustRuntimeSetting(settings, SettingsMenuOption::UiScaleMode, -1)
            && settings.uiScaleAuto
            && settings.uiScaleManualPercent == 105U,
        "Auto keeps the player's separate manual scale preference");
    const auto uiScaleDescriptor = numericSettingDescriptor(
        settings, SettingsMenuOption::UiScaleManual);
    const auto fovDescriptor = numericSettingDescriptor(
        settings, SettingsMenuOption::FieldOfView);
    expect(uiScaleDescriptor && uiScaleDescriptor->defaultValue == 100U
            && uiScaleDescriptor->step == 5U && uiScaleDescriptor->unit == "%"
            && fovDescriptor && fovDescriptor->defaultValue == 100U
            && fovDescriptor->unit == "°",
        "numeric metadata owns defaults, steps and displayed units");
    auto unevenScale = settings;
    unevenScale.uiScaleManualPercent = 123U;
    expect(sanitizeRuntimeSettings(unevenScale).uiScaleManualPercent == 125U,
        "loaded manual scale snaps to the documented five-percent increment");
    expect(resolvedUiScalePercent(settings, 1280, 720, 1280, 720) == 100U
            && resolvedUiScalePercent(settings, 2560, 1440, 2560, 1440) == 125U
            && resolvedUiScalePercent(settings, 2880, 1620, 2880, 1620) == 150U
            && resolvedUiScalePercent(settings, 3840, 2160, 3840, 2160) == 200U
            && resolvedUiScalePercent(settings, 3840, 2160, 1920, 1080) == 100U,
        "Auto scale uses useful size and respects high DPI logical dimensions");
    expect(std::abs(normalizedVolume(250U) - 1.0F) < 0.0001F,
        "volume normalization clamps invalid percentages");

    expect(adjustRuntimeSetting(settings, SettingsMenuOption::GamepadVibration, -1)
            && !settings.gamepadVibration,
        "controller vibration toggle can be disabled");
    for (int index = 0; index < 120; ++index)
        (void)adjustRuntimeSetting(settings, SettingsMenuOption::GamepadVibrationStrength, -1);
    expect(settings.gamepadVibrationPercent == 0U
            && !adjustRuntimeSetting(settings, SettingsMenuOption::GamepadVibrationStrength, -1),
        "controller vibration strength clamps at zero");
    for (int index = 0; index < 120; ++index)
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
            && settings.gamepadLookHorizontalPercent == 101U
            && adjustRuntimeSetting(settings, SettingsMenuOption::GamepadLookVertical, -1)
            && settings.gamepadLookVerticalPercent == 99U
            && adjustRuntimeSetting(settings, SettingsMenuOption::GamepadAimSensitivity, -1)
            && settings.gamepadAimSensitivityPercent == 99U,
        "controller camera axes and aiming sensitivity adjust independently");
    expect(adjustRuntimeSetting(settings, SettingsMenuOption::GamepadMoveDeadzone, -1)
            && settings.gamepadMoveDeadzonePercent == 17U
            && adjustRuntimeSetting(settings, SettingsMenuOption::GamepadLookDeadzone, 1)
            && settings.gamepadLookDeadzonePercent == 19U
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
            && sanitized.graphicsQuality == GraphicsQuality::Ultra
            && sanitized.gamepadVibrationPercent == 100U
            && sanitized.gamepadLookHorizontalPercent == 200U
            && sanitized.gamepadMoveDeadzonePercent == 40U,
        "invalid video enums recover to their default presets");
}

void testCustomGraphics()
{
    using namespace gdp::gameplay;
    RuntimeSettings settings;
    expect(adjustRuntimeSetting(settings, SettingsMenuOption::Shadows, -1)
            && settings.graphicsQuality == GraphicsQuality::Custom
            && !effectiveGraphicsQualitySettings(settings).shadowsEnabled,
        "individual shadow change selects Custom without changing other budgets");
    expect(!adjustRuntimeSetting(settings, SettingsMenuOption::Shadows, -1),
        "unchanged shadow toggle does not report a change");
    expect(adjustRuntimeSetting(settings, SettingsMenuOption::DetailedMaterials, -1)
            && !settings.customGraphics.detailedMaterials
            && settings.customGraphics.renderScalePercent == 100U,
        "material toggle preserves the render scale");
    expect(adjustRuntimeSetting(settings, SettingsMenuOption::RenderScale, 1)
            && settings.customGraphics.renderScalePercent == 101U,
        "render scale adjusts in one percent steps");
    expect(adjustRuntimeSetting(settings, SettingsMenuOption::MaximumLights, 1)
            && settings.customGraphics.maximumLights == 9U,
        "light budget adjusts independently");
    expect(adjustRuntimeSetting(settings, SettingsMenuOption::GraphicsQuality, -1)
            && settings.graphicsQuality == GraphicsQuality::High
            && effectiveGraphicsQualitySettings(settings) == graphicsQualitySettings(GraphicsQuality::High),
        "selecting a preset restores all of its values");
    settings.graphicsQuality = GraphicsQuality::Low;
    expect(adjustRuntimeSetting(settings, SettingsMenuOption::RenderScale, 1)
            && settings.customGraphics.renderScalePercent == 68U,
        "legacy 67 percent Low preset advances one percent");
    settings.graphicsQuality = GraphicsQuality::Potato;
    expect(adjustRuntimeSetting(settings, SettingsMenuOption::Shadows, 1)
            && settings.customGraphics.shadowsEnabled
            && settings.customGraphics.maximumShadowLights >= 1U,
        "turning shadows on from Potato provisions a light");
}

void testNumericRepeatTiming()
{
    using namespace std::chrono_literals;
    gdp::game::MenuAdjustmentRepeat repeat;
    const auto start = gdp::game::MenuAdjustmentRepeat::Clock::time_point{};
    expect(repeat.advance(10U, start, true, true) == 1
            && repeat.advance(10U, start + 319ms, false, true) == 0
            && repeat.advance(10U, start + 320ms, false, true) == 1
            && repeat.advance(10U, start + 374ms, false, true) == 0
            && repeat.advance(10U, start + 375ms, false, true) == 1,
        "numeric hold starts immediately then waits 320ms and repeats every 55ms");
    expect(repeat.advance(10U, start + 500ms, false, false) == 0
            && repeat.advance(10U, start + 1000ms, false, true) == 0
            && repeat.advance(11U, start + 1000ms, true, true) == 1
            && repeat.advance(12U, start + 2000ms, false, true) == 0,
        "release and row change cancel repetition without a phantom step");
}

} // namespace

int main()
{
    testSettingsNavigationAndReturn();
    testRuntimeSettingsClamps();
    testCustomGraphics();
    testNumericRepeatTiming();
    if (failures != 0) {
        std::cerr << failures << " settings menu test(s) failed\n";
        return 1;
    }
    std::cout << "All GDP settings menu tests passed\n";
    return 0;
}
