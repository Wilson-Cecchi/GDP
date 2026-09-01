#pragma once

#include <cstdint>

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
    FieldOfView,
    Back,
    Count,
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
    std::uint16_t fieldOfViewDegrees{68U};

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

} // namespace gdp::gameplay
