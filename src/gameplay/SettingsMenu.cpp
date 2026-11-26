#include "gameplay/SettingsMenu.hpp"

#include <algorithm>
#include <array>

namespace gdp::gameplay {
namespace {

constexpr std::array<WindowResolutionSize, 7> windowResolutionSizes{{
    {960U, 540U},
    {1280U, 720U},
    {1600U, 900U},
    {1920U, 1080U},
    {2560U, 1440U},
    {2880U, 1620U},
    {3840U, 2160U},
}};

bool applyGraphicsAdjustment(
    RuntimeSettings& settings,
    const GraphicsQualitySettings& adjusted) noexcept
{
    if (adjusted == effectiveGraphicsQualitySettings(settings)) return false;
    settings.customGraphics = adjusted;
    settings.graphicsQuality = GraphicsQuality::Custom;
    return true;
}

} // namespace

void openSettingsMenu(
    SettingsMenuState& state,
    const SettingsMenuOrigin origin) noexcept
{
    state.active = true;
    state.origin = origin;
    state.selection = SettingsMenuOption::MasterVolume;
}

void closeSettingsMenu(SettingsMenuState& state) noexcept
{
    state = {};
}

void moveSettingsMenuSelection(
    SettingsMenuState& state,
    const int direction) noexcept
{
    if (!state.active || direction == 0) {
        return;
    }
    constexpr int optionCount = static_cast<int>(SettingsMenuOption::Count);
    int selection = static_cast<int>(state.selection);
    selection += direction > 0 ? 1 : -1;
    if (selection < 0) {
        selection = optionCount - 1;
    } else if (selection >= optionCount) {
        selection = 0;
    }
    state.selection = static_cast<SettingsMenuOption>(selection);
}

void selectSettingsMenuOption(
    SettingsMenuState& state,
    const SettingsMenuOption option) noexcept
{
    if (state.active && option < SettingsMenuOption::Count) {
        state.selection = option;
    }
}

std::optional<NumericSettingDescriptor> numericSettingDescriptor(
    const RuntimeSettings& settings, const SettingsMenuOption option) noexcept
{
    using O = SettingsMenuOption;
    const auto quality = effectiveGraphicsQualitySettings(settings);
    const auto percent = [](const std::uint16_t minimum,
        const std::uint16_t maximum, const std::uint16_t step,
        const std::uint16_t value, const std::uint16_t fallback) {
        return NumericSettingDescriptor{minimum, maximum, step, value,
            fallback, "%"};
    };
    switch (option) {
    case O::MasterVolume: return percent(0U, 100U, 1U, settings.masterVolumePercent, 100U);
    case O::EffectsVolume: return percent(0U, 100U, 1U, settings.effectsVolumePercent, 100U);
    case O::AmbientVolume: return percent(0U, 100U, 1U, settings.ambientVolumePercent, 100U);
    case O::UiVolume: return percent(0U, 100U, 1U, settings.uiVolumePercent, 100U);
    case O::MouseSensitivity: return percent(1U, 300U, 1U, settings.mouseSensitivityPercent, 100U);
    case O::MouseAimSensitivity: return percent(1U, 200U, 1U, settings.mouseAimSensitivityPercent, 100U);
    case O::FieldOfView: return NumericSettingDescriptor{60U, 120U, 1U,
        settings.fieldOfViewDegrees, 100U, "°"};
    case O::RenderScale: return percent(50U, 150U, 1U, quality.renderScalePercent, 100U);
    case O::MaximumLights: return NumericSettingDescriptor{1U, 16U, 1U,
        quality.maximumLights, 8U, {}};
    case O::UiScaleManual: return percent(75U, 200U, 5U, settings.uiScaleManualPercent, 100U);
    case O::GamepadVibrationStrength: return percent(0U, 100U, 1U, settings.gamepadVibrationPercent, 50U);
    case O::GamepadLookHorizontal: return percent(1U, 200U, 1U, settings.gamepadLookHorizontalPercent, 100U);
    case O::GamepadLookVertical: return percent(1U, 200U, 1U, settings.gamepadLookVerticalPercent, 100U);
    case O::GamepadAimSensitivity: return percent(1U, 200U, 1U, settings.gamepadAimSensitivityPercent, 100U);
    case O::GamepadMoveDeadzone: return percent(0U, 40U, 1U, settings.gamepadMoveDeadzonePercent, 18U);
    case O::GamepadLookDeadzone: return percent(0U, 40U, 1U, settings.gamepadLookDeadzonePercent, 18U);
    default: return std::nullopt;
    }
}

bool setRuntimeNumericSetting(RuntimeSettings& settings,
    const SettingsMenuOption option, const int value) noexcept
{
    const auto descriptor = numericSettingDescriptor(settings, option);
    if (!descriptor) return false;
    const int bounded = std::clamp(value, static_cast<int>(descriptor->minimum),
        static_cast<int>(descriptor->maximum));
    const int step = descriptor->step;
    const auto rounded = static_cast<std::uint16_t>(
        std::clamp(static_cast<int>(descriptor->minimum)
                + ((bounded - static_cast<int>(descriptor->minimum) + step / 2) / step) * step,
            static_cast<int>(descriptor->minimum), static_cast<int>(descriptor->maximum)));
    if (rounded == descriptor->value) return false;
    using O = SettingsMenuOption;
    switch (option) {
    case O::MasterVolume: settings.masterVolumePercent = rounded; break;
    case O::EffectsVolume: settings.effectsVolumePercent = rounded; break;
    case O::AmbientVolume: settings.ambientVolumePercent = rounded; break;
    case O::UiVolume: settings.uiVolumePercent = rounded; break;
    case O::MouseSensitivity: settings.mouseSensitivityPercent = rounded; break;
    case O::MouseAimSensitivity: settings.mouseAimSensitivityPercent = rounded; break;
    case O::FieldOfView: settings.fieldOfViewDegrees = rounded; break;
    case O::UiScaleManual: settings.uiScaleManualPercent = rounded; break;
    case O::GamepadVibrationStrength: settings.gamepadVibrationPercent = rounded; break;
    case O::GamepadLookHorizontal: settings.gamepadLookHorizontalPercent = rounded; break;
    case O::GamepadLookVertical: settings.gamepadLookVerticalPercent = rounded; break;
    case O::GamepadAimSensitivity: settings.gamepadAimSensitivityPercent = rounded; break;
    case O::GamepadMoveDeadzone: settings.gamepadMoveDeadzonePercent = rounded; break;
    case O::GamepadLookDeadzone: settings.gamepadLookDeadzonePercent = rounded; break;
    case O::RenderScale: {
        auto quality = effectiveGraphicsQualitySettings(settings);
        quality.renderScalePercent = rounded;
        return applyGraphicsAdjustment(settings, quality);
    }
    case O::MaximumLights: {
        auto quality = effectiveGraphicsQualitySettings(settings);
        quality.maximumLights = static_cast<std::uint8_t>(rounded);
        return applyGraphicsAdjustment(settings, quality);
    }
    default: return false;
    }
    return true;
}

bool adjustRuntimeSetting(
    RuntimeSettings& settings,
    const SettingsMenuOption option,
    const int direction) noexcept
{
    if (direction == 0) {
        return false;
    }
    if (const auto descriptor = numericSettingDescriptor(settings, option)) {
        return setRuntimeNumericSetting(settings, option,
            static_cast<int>(descriptor->value)
                + (direction > 0 ? descriptor->step : -static_cast<int>(descriptor->step)));
    }
    switch (option) {
    case SettingsMenuOption::UiScaleMode: {
        const bool previous = settings.uiScaleAuto;
        settings.uiScaleAuto = direction < 0;
        return previous != settings.uiScaleAuto;
    }
    case SettingsMenuOption::InvertMouseY: {
        const bool previous = settings.invertMouseY;
        settings.invertMouseY = direction > 0;
        return settings.invertMouseY != previous;
    }
    case SettingsMenuOption::GraphicsQuality: {
        const GraphicsQuality previous = settings.graphicsQuality;
        const int maximum = static_cast<int>(GraphicsQuality::Ultra);
        const int current = previous == GraphicsQuality::Custom
            ? maximum : static_cast<int>(previous);
        const int adjusted = std::clamp(
            current + (direction > 0 ? 1 : -1),
            0, maximum);
        settings.graphicsQuality = static_cast<GraphicsQuality>(adjusted);
        if (settings.graphicsQuality != previous)
            settings.customGraphics = graphicsQualitySettings(settings.graphicsQuality);
        return settings.graphicsQuality != previous;
    }
    case SettingsMenuOption::Shadows: {
        auto quality = effectiveGraphicsQualitySettings(settings);
        quality.shadowsEnabled = direction > 0;
        if (quality.shadowsEnabled && quality.maximumShadowLights == 0U) {
            quality.maximumShadowLights = 1U;
            quality.maximumShadowOccluders = 3U;
        }
        return applyGraphicsAdjustment(settings, quality);
    }
    case SettingsMenuOption::DetailedMaterials: {
        auto quality = effectiveGraphicsQualitySettings(settings);
        quality.detailedMaterials = direction > 0;
        return applyGraphicsAdjustment(settings, quality);
    }
    case SettingsMenuOption::WindowResolution: {
        const WindowResolution previous = settings.windowResolution;
        const int maximum = static_cast<int>(WindowResolution::Count) - 1;
        const int adjusted = std::clamp(
            static_cast<int>(previous) + (direction > 0 ? 1 : -1),
            0, maximum);
        settings.windowResolution = static_cast<WindowResolution>(adjusted);
        return settings.windowResolution != previous;
    }
    case SettingsMenuOption::Fullscreen: {
        const bool previous = settings.fullscreen;
        settings.fullscreen = direction > 0;
        return settings.fullscreen != previous;
    }
    case SettingsMenuOption::VerticalSync: {
        const bool previous = settings.verticalSync;
        settings.verticalSync = direction > 0;
        return settings.verticalSync != previous;
    }
    case SettingsMenuOption::Language: {
        const auto previous = settings.language;
        // Two choices wrap in either direction; keyboard and both mouse arrows work.
        settings.language = previous == Language::English
            ? Language::PortugueseBrazil : Language::English;
        return settings.language != previous;
    }
    case SettingsMenuOption::GamepadVibration: {
        const bool previous = settings.gamepadVibration;
        settings.gamepadVibration = direction > 0;
        return settings.gamepadVibration != previous;
    }
    case SettingsMenuOption::GamepadMoveStick: {
        const bool previous = settings.gamepadSouthpaw;
        settings.gamepadSouthpaw = direction > 0;
        return settings.gamepadSouthpaw != previous;
    }
    case SettingsMenuOption::GamepadInvertY: {
        const bool previous = settings.invertGamepadY;
        settings.invertGamepadY = direction > 0;
        return settings.invertGamepadY != previous;
    }
    case SettingsMenuOption::Back:
    case SettingsMenuOption::Count:
    case SettingsMenuOption::MasterVolume:
    case SettingsMenuOption::EffectsVolume:
    case SettingsMenuOption::AmbientVolume:
    case SettingsMenuOption::UiVolume:
    case SettingsMenuOption::MouseSensitivity:
    case SettingsMenuOption::MouseAimSensitivity:
    case SettingsMenuOption::UiScaleManual:
    case SettingsMenuOption::FieldOfView:
    case SettingsMenuOption::RenderScale:
    case SettingsMenuOption::MaximumLights:
    case SettingsMenuOption::GamepadVibrationStrength:
    case SettingsMenuOption::GamepadLookHorizontal:
    case SettingsMenuOption::GamepadLookVertical:
    case SettingsMenuOption::GamepadAimSensitivity:
    case SettingsMenuOption::GamepadMoveDeadzone:
    case SettingsMenuOption::GamepadLookDeadzone:
        return false;
    }
    return false;
}

SettingsMenuCommand activateSettingsMenuSelection(
    SettingsMenuState& state) noexcept
{
    if (!state.active || state.selection != SettingsMenuOption::Back) {
        return SettingsMenuCommand::None;
    }
    const SettingsMenuOrigin origin = state.origin;
    closeSettingsMenu(state);
    return origin == SettingsMenuOrigin::PauseMenu
        ? SettingsMenuCommand::BackToPauseMenu
        : SettingsMenuCommand::BackToMainMenu;
}

std::string_view languageName(const Language language) noexcept
{
    return language == Language::PortugueseBrazil ? "pt-BR" : "en";
}

std::optional<Language> languageForName(const std::string_view name) noexcept
{
    if (name == "en") return Language::English;
    if (name == "pt-BR") return Language::PortugueseBrazil;
    return std::nullopt;
}

float normalizedVolume(const std::uint16_t percent) noexcept
{
    return static_cast<float>(std::min<std::uint16_t>(percent, 100U)) / 100.0F;
}

float mouseSensitivity(const RuntimeSettings& settings) noexcept
{
    constexpr float baseSensitivity = 0.12F;
    const std::uint16_t percent = std::clamp<std::uint16_t>(
        settings.mouseSensitivityPercent, 1U, 300U);
    return baseSensitivity * (static_cast<float>(percent) / 100.0F);
}

std::uint16_t resolvedUiScalePercent(const RuntimeSettings& settings,
    const int drawableWidth, const int drawableHeight,
    const int windowWidth, const int windowHeight) noexcept
{
    if (!settings.uiScaleAuto)
        return std::clamp<std::uint16_t>(settings.uiScaleManualPercent, 75U, 200U);
    // SDL reports drawable and logical dimensions separately on high DPI displays.
    // Prefer logical height when the ratio shows OS scaling, then keep the
    // calibrated pixel-height fallback deterministic on ordinary displays.
    const bool highDpi = windowWidth > 0 && windowHeight > 0
        && drawableWidth > windowWidth * 5 / 4
        && drawableHeight > windowHeight * 5 / 4;
    const int usefulHeight = highDpi ? windowHeight : drawableHeight;
    if (usefulHeight >= 1900) return 200U;
    if (usefulHeight >= 1530) return 150U;
    if (usefulHeight >= 1260) return 125U;
    return 100U;
}

GraphicsQualitySettings graphicsQualitySettings(
    const GraphicsQuality quality) noexcept
{
    switch (quality) {
    case GraphicsQuality::Potato:
        return {50U, 2U, 0U, 0U, 1U, false, false};
    case GraphicsQuality::Low:
        return {67U, 4U, 0U, 0U, 1U, false, false};
    case GraphicsQuality::Medium:
        return {85U, 6U, 1U, 3U, 2U, true};
    case GraphicsQuality::High:
        return {100U, 8U, 8U, 6U, 2U, true};
    case GraphicsQuality::Ultra:
        return {100U, 8U, 8U, 8U, 2U, true};
    case GraphicsQuality::Custom:
    case GraphicsQuality::Count:
        break;
    }
    return graphicsQualitySettings(GraphicsQuality::Medium);
}

GraphicsQualitySettings effectiveGraphicsQualitySettings(
    const RuntimeSettings& settings) noexcept
{
    return settings.graphicsQuality == GraphicsQuality::Custom
        ? settings.customGraphics : graphicsQualitySettings(settings.graphicsQuality);
}

std::string_view graphicsQualityName(const GraphicsQuality quality) noexcept
{
    switch (quality) {
    case GraphicsQuality::Potato:
        return "POTATO";
    case GraphicsQuality::Low:
        return "LOW";
    case GraphicsQuality::Medium:
        return "MEDIUM";
    case GraphicsQuality::High:
        return "HIGH";
    case GraphicsQuality::Ultra:
        return "ULTRA";
    case GraphicsQuality::Custom:
        return "CUSTOM";
    case GraphicsQuality::Count:
        break;
    }
    return "MEDIUM";
}

std::optional<GraphicsQuality> graphicsQualityForName(
    const std::string_view name) noexcept
{
    constexpr std::array qualities{
        GraphicsQuality::Potato,
        GraphicsQuality::Low,
        GraphicsQuality::Medium,
        GraphicsQuality::High,
        GraphicsQuality::Ultra,
        GraphicsQuality::Custom,
    };
    for (const GraphicsQuality quality : qualities) {
        if (name == graphicsQualityName(quality)) {
            return quality;
        }
    }
    return std::nullopt;
}

RuntimeSettings sanitizeRuntimeSettings(RuntimeSettings settings) noexcept
{
    settings.masterVolumePercent = std::min<std::uint16_t>(
        settings.masterVolumePercent, 100U);
    settings.effectsVolumePercent = std::min<std::uint16_t>(
        settings.effectsVolumePercent, 100U);
    settings.ambientVolumePercent = std::min<std::uint16_t>(
        settings.ambientVolumePercent, 100U);
    settings.uiVolumePercent = std::min<std::uint16_t>(
        settings.uiVolumePercent, 100U);
    settings.mouseSensitivityPercent = std::clamp<std::uint16_t>(
        settings.mouseSensitivityPercent, 1U, 300U);
    settings.mouseAimSensitivityPercent = std::clamp<std::uint16_t>(
        settings.mouseAimSensitivityPercent, 1U, 200U);
    settings.uiScaleManualPercent = std::clamp<std::uint16_t>(
        settings.uiScaleManualPercent, 75U, 200U);
    settings.uiScaleManualPercent = static_cast<std::uint16_t>(75U
        + ((settings.uiScaleManualPercent - 75U + 2U) / 5U) * 5U);
    settings.fieldOfViewDegrees = std::clamp<std::uint16_t>(
        settings.fieldOfViewDegrees, 60U, 120U);
    if (settings.graphicsQuality >= GraphicsQuality::Count) {
        settings.graphicsQuality = GraphicsQuality::Ultra;
    }
    settings.customGraphics.renderScalePercent = std::clamp<std::uint16_t>(
        settings.customGraphics.renderScalePercent, 50U, 150U);
    settings.customGraphics.maximumLights = std::clamp<std::uint8_t>(
        settings.customGraphics.maximumLights, 1U, 16U);
    settings.customGraphics.maximumShadowLights = std::min<std::uint8_t>(
        settings.customGraphics.maximumShadowLights, 8U);
    settings.customGraphics.maximumShadowOccluders = std::min<std::uint8_t>(
        settings.customGraphics.maximumShadowOccluders, 16U);
    settings.customGraphics.visibleRoomDepth = std::clamp<std::uint8_t>(
        settings.customGraphics.visibleRoomDepth, 1U, 4U);
    if (settings.customGraphics.shadowsEnabled
        && settings.customGraphics.maximumShadowLights == 0U) {
        settings.customGraphics.maximumShadowLights = 1U;
        settings.customGraphics.maximumShadowOccluders = std::max<std::uint8_t>(
            settings.customGraphics.maximumShadowOccluders, 3U);
    }
    if (settings.windowResolution >= WindowResolution::Count) {
        settings.windowResolution = WindowResolution::R1280x720;
    }
    if (settings.language >= Language::Count) settings.language = Language::English;
    if (!validateControlBindings(settings.controls)) settings.controls = {};
    if (!validateGamepadBindings(settings.gamepadControls)) settings.gamepadControls = {};
    settings.gamepadVibrationPercent = std::min<std::uint16_t>(settings.gamepadVibrationPercent, 100U);
    settings.gamepadLookHorizontalPercent = std::clamp<std::uint16_t>(settings.gamepadLookHorizontalPercent, 1U, 200U);
    settings.gamepadLookVerticalPercent = std::clamp<std::uint16_t>(settings.gamepadLookVerticalPercent, 1U, 200U);
    settings.gamepadAimSensitivityPercent = std::clamp<std::uint16_t>(settings.gamepadAimSensitivityPercent, 1U, 200U);
    settings.gamepadMoveDeadzonePercent = std::min<std::uint16_t>(settings.gamepadMoveDeadzonePercent, 40U);
    settings.gamepadLookDeadzonePercent = std::min<std::uint16_t>(settings.gamepadLookDeadzonePercent, 40U);
    return settings;
}

WindowResolutionSize windowResolutionSize(
    const WindowResolution resolution) noexcept
{
    const std::size_t index = static_cast<std::size_t>(resolution);
    return index < windowResolutionSizes.size()
        ? windowResolutionSizes[index]
        : windowResolutionSizes[1U];
}

std::optional<WindowResolution> windowResolutionForSize(
    const std::uint16_t width,
    const std::uint16_t height) noexcept
{
    for (std::size_t index = 0U; index < windowResolutionSizes.size(); ++index) {
        if (windowResolutionSizes[index]
            == WindowResolutionSize{width, height}) {
            return static_cast<WindowResolution>(index);
        }
    }
    return std::nullopt;
}

} // namespace gdp::gameplay
