#pragma once

#include <cstdint>
#include <optional>

namespace gdp::gameplay {

enum class SettingsMenuOrigin : std::uint8_t {
    MainMenu,
    PauseMenu,
};

enum class SettingsMenuOption : std::uint8_t {
    MasterVolume,
    EffectsVolume,
    AmbientVolume,
    UiVolume,
    MouseSensitivity,
    InvertMouseY,
    FieldOfView,
    WindowResolution,
    Fullscreen,
    Back,
    Count,
};

enum class WindowResolution : std::uint8_t {
    R960x540,
    R1280x720,
    R1600x900,
    R1920x1080,
    Count,
};

struct WindowResolutionSize {
    std::uint16_t width{1280U};
    std::uint16_t height{720U};

    constexpr bool operator==(const WindowResolutionSize&) const noexcept = default;
};

enum class SettingsMenuCommand : std::uint8_t {
    None,
    BackToMainMenu,
    BackToPauseMenu,
};

struct RuntimeSettings {
    std::uint16_t masterVolumePercent{100U};
    std::uint16_t effectsVolumePercent{100U};
    std::uint16_t ambientVolumePercent{100U};
    std::uint16_t uiVolumePercent{100U};
    std::uint16_t mouseSensitivityPercent{100U};
    bool invertMouseY{false};
    std::uint16_t fieldOfViewDegrees{68U};
    WindowResolution windowResolution{WindowResolution::R1280x720};
    bool fullscreen{false};

    constexpr bool operator==(const RuntimeSettings&) const noexcept = default;
};

struct SettingsMenuState {
    bool active{false};
    SettingsMenuOrigin origin{SettingsMenuOrigin::MainMenu};
    SettingsMenuOption selection{SettingsMenuOption::MasterVolume};

    constexpr bool operator==(const SettingsMenuState&) const noexcept = default;
};

void openSettingsMenu(
    SettingsMenuState& state,
    SettingsMenuOrigin origin) noexcept;
void closeSettingsMenu(SettingsMenuState& state) noexcept;
void moveSettingsMenuSelection(SettingsMenuState& state, int direction) noexcept;
void selectSettingsMenuOption(
    SettingsMenuState& state,
    SettingsMenuOption option) noexcept;
[[nodiscard]] bool adjustRuntimeSetting(
    RuntimeSettings& settings,
    SettingsMenuOption option,
    int direction) noexcept;
[[nodiscard]] SettingsMenuCommand activateSettingsMenuSelection(
    SettingsMenuState& state) noexcept;
[[nodiscard]] float normalizedVolume(std::uint16_t percent) noexcept;
[[nodiscard]] float mouseSensitivity(const RuntimeSettings& settings) noexcept;
[[nodiscard]] RuntimeSettings sanitizeRuntimeSettings(
    RuntimeSettings settings) noexcept;
[[nodiscard]] WindowResolutionSize windowResolutionSize(
    WindowResolution resolution) noexcept;
[[nodiscard]] std::optional<WindowResolution> windowResolutionForSize(
    std::uint16_t width,
    std::uint16_t height) noexcept;

} // namespace gdp::gameplay
