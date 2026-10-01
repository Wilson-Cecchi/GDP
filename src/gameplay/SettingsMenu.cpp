#include "gameplay/SettingsMenu.hpp"

#include <algorithm>
#include <array>

namespace gdp::gameplay {
namespace {

constexpr std::array<WindowResolutionSize, 4> windowResolutionSizes{{
    {960U, 540U},
    {1280U, 720U},
    {1600U, 900U},
    {1920U, 1080U},
}};

bool adjustPercent(
    std::uint16_t& value,
    const int direction,
    const std::uint16_t minimum,
    const std::uint16_t maximum,
    const std::uint16_t step) noexcept
{
    const std::uint16_t previous = value;
    const int adjusted = static_cast<int>(value)
        + (direction > 0 ? static_cast<int>(step) : -static_cast<int>(step));
    value = static_cast<std::uint16_t>(std::clamp(
        adjusted, static_cast<int>(minimum), static_cast<int>(maximum)));
    return value != previous;
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

bool adjustRuntimeSetting(
    RuntimeSettings& settings,
    const SettingsMenuOption option,
    const int direction) noexcept
{
    if (direction == 0) {
        return false;
    }
    switch (option) {
    case SettingsMenuOption::MasterVolume:
        return adjustPercent(
            settings.masterVolumePercent, direction, 0U, 100U, 10U);
    case SettingsMenuOption::EffectsVolume:
        return adjustPercent(
            settings.effectsVolumePercent, direction, 0U, 100U, 10U);
    case SettingsMenuOption::AmbientVolume:
        return adjustPercent(
            settings.ambientVolumePercent, direction, 0U, 100U, 10U);
    case SettingsMenuOption::UiVolume:
        return adjustPercent(
            settings.uiVolumePercent, direction, 0U, 100U, 10U);
    case SettingsMenuOption::MouseSensitivity:
        return adjustPercent(
            settings.mouseSensitivityPercent, direction, 30U, 200U, 10U);
    case SettingsMenuOption::InvertMouseY: {
        const bool previous = settings.invertMouseY;
        settings.invertMouseY = direction > 0;
        return settings.invertMouseY != previous;
    }
    case SettingsMenuOption::FieldOfView:
        return adjustPercent(
            settings.fieldOfViewDegrees, direction, 60U, 100U, 2U);
    case SettingsMenuOption::GraphicsQuality: {
        const GraphicsQuality previous = settings.graphicsQuality;
        const int maximum = static_cast<int>(GraphicsQuality::Count) - 1;
        const int adjusted = std::clamp(
            static_cast<int>(previous) + (direction > 0 ? 1 : -1),
            0, maximum);
        settings.graphicsQuality = static_cast<GraphicsQuality>(adjusted);
        return settings.graphicsQuality != previous;
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
    case SettingsMenuOption::Back:
    case SettingsMenuOption::Count:
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
        settings.mouseSensitivityPercent, 30U, 200U);
    return baseSensitivity * (static_cast<float>(percent) / 100.0F);
}

GraphicsQualitySettings graphicsQualitySettings(
    const GraphicsQuality quality) noexcept
{
    switch (quality) {
    case GraphicsQuality::Potato:
        return {50U, 2U, 0U, 0U, 1U, false};
    case GraphicsQuality::Low:
        return {67U, 4U, 0U, 0U, 1U, false};
    case GraphicsQuality::Medium:
        return {85U, 6U, 1U, 3U, 2U, true};
    case GraphicsQuality::High:
        return {100U, 8U, 1U, 6U, 2U, true};
    case GraphicsQuality::Ultra:
        return {100U, 8U, 2U, 8U, 2U, true};
    case GraphicsQuality::Count:
        break;
    }
    return graphicsQualitySettings(GraphicsQuality::Medium);
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
        settings.mouseSensitivityPercent, 30U, 200U);
    settings.fieldOfViewDegrees = std::clamp<std::uint16_t>(
        settings.fieldOfViewDegrees, 60U, 100U);
    if (settings.graphicsQuality >= GraphicsQuality::Count) {
        settings.graphicsQuality = GraphicsQuality::Medium;
    }
    if (settings.windowResolution >= WindowResolution::Count) {
        settings.windowResolution = WindowResolution::R1280x720;
    }
    if (settings.language >= Language::Count) settings.language = Language::English;
    if (!validateControlBindings(settings.controls)) settings.controls = {};
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
