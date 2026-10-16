#pragma once

#include "platform/Input.hpp"
#include <array>
#include <optional>
#include <string_view>

namespace gdp::gameplay {
enum class ControlAction : std::uint8_t {
    Forward, Backward, Left, Right, QuietWalk, Crouch, Jump, Fire, Aim,
    Reload, Interact, Flashlight, Drop, Primary, Secondary, Melee,
    Equipment, SpecialItem, LastWeapon, Information, NextWeapon, PreviousWeapon,
    QuickMelee, Count,
};
enum class BindingKind : std::uint8_t { Key, Mouse, WheelUp, WheelDown };
struct ControlBinding {
    BindingKind kind{BindingKind::Key};
    std::uint8_t code{0U};
    constexpr bool operator==(const ControlBinding&) const noexcept = default;
};
inline constexpr std::size_t controlActionCount = static_cast<std::size_t>(ControlAction::Count);
[[nodiscard]] std::array<ControlBinding, controlActionCount> defaultControlBindings() noexcept;
struct ControlBindings {
    std::array<ControlBinding, controlActionCount> values{defaultControlBindings()};
    constexpr bool operator==(const ControlBindings&) const noexcept = default;
};
[[nodiscard]] std::array<wkc::platform::GamepadButton, controlActionCount>
defaultGamepadBindings() noexcept;
struct GamepadBindings {
    std::array<wkc::platform::GamepadButton, controlActionCount> values{defaultGamepadBindings()};
    constexpr bool operator==(const GamepadBindings&) const noexcept = default;
};
[[nodiscard]] std::string_view controlActionName(ControlAction action) noexcept;
[[nodiscard]] std::string_view bindingName(ControlBinding binding) noexcept;
[[nodiscard]] std::optional<ControlBinding> bindingForName(std::string_view name) noexcept;
[[nodiscard]] bool validControlBinding(ControlBinding binding) noexcept;
[[nodiscard]] bool validateControlBindings(const ControlBindings& bindings) noexcept;
// Reusing a binding swaps the two actions instead of silently breaking one.
[[nodiscard]] bool assignControlBinding(ControlBindings& bindings, ControlAction action, ControlBinding binding) noexcept;
[[nodiscard]] std::optional<ControlBinding> pressedControlBinding(const wkc::platform::Input& input) noexcept;
[[nodiscard]] std::string_view gamepadBindingName(wkc::platform::GamepadButton button) noexcept;
[[nodiscard]] std::string_view activeBindingName(
    const ControlBindings& keyboard, const GamepadBindings& gamepad,
    ControlAction action, wkc::platform::InputDevice device) noexcept;
[[nodiscard]] std::optional<wkc::platform::GamepadButton> gamepadBindingForName(std::string_view name) noexcept;
[[nodiscard]] bool validGamepadBinding(wkc::platform::GamepadButton button) noexcept;
[[nodiscard]] bool validateGamepadBindings(const GamepadBindings& bindings) noexcept;
[[nodiscard]] bool assignGamepadBinding(GamepadBindings&, ControlAction, wkc::platform::GamepadButton) noexcept;
[[nodiscard]] std::optional<wkc::platform::GamepadButton> pressedGamepadBinding(const wkc::platform::Input&) noexcept;
[[nodiscard]] bool controlDown(const ControlBindings&, const GamepadBindings&, ControlAction, const wkc::platform::Input&) noexcept;
[[nodiscard]] bool controlPressed(const ControlBindings&, const GamepadBindings&, ControlAction, const wkc::platform::Input&) noexcept;
} // namespace gdp::gameplay
