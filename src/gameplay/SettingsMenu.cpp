#include "gameplay/SettingsMenu.hpp"

#include <algorithm>

namespace gdp::gameplay {
namespace {

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
    case SettingsMenuOption::Fullscreen: {
        const bool previous = settings.fullscreen;
        settings.fullscreen = direction > 0;
        return settings.fullscreen != previous;
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
    return settings;
}

} // namespace gdp::gameplay
