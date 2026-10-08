#include "gameplay/ControlBindings.hpp"

namespace gdp::gameplay {
namespace {
using wkc::platform::Key;
using wkc::platform::MouseButton;
using wkc::platform::GamepadButton;
constexpr std::array<std::string_view, static_cast<std::size_t>(Key::Count)> keyNames{
    "W", "A", "S", "D", "Q", "E", "F", "G", "R", "SHIFT", "RSHIFT", "CTRL", "RCTRL",
    "SPACE", "1", "2", "3", "4", "5", "TAB", "F3", "ESC", "ENTER", "UP", "DOWN", "LEFT", "RIGHT",
    "B", "C", "H", "I", "J", "K", "L", "M", "N", "O", "P", "T", "U", "V", "X", "Y", "Z",
    "0", "6", "7", "8", "9", "ALT", "RALT", "BACKSPACE", "DELETE", "HOME", "END", "PAGEUP", "PAGEDOWN",
    "F1", "F2", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
};
constexpr std::array<std::string_view, controlActionCount> actionNames{
    "FORWARD", "BACKWARD", "STRAFE LEFT", "STRAFE RIGHT", "QUIET WALK", "CROUCH", "JUMP",
    "FIRE", "AIM", "RELOAD", "INTERACT", "FLASHLIGHT", "DROP WEAPON", "PRIMARY WEAPON",
    "SECONDARY WEAPON", "MELEE", "EQUIPMENT (RESERVED)", "SPECIAL ITEM (RESERVED)",
    "LAST WEAPON", "INFORMATION", "NEXT WEAPON", "PREVIOUS WEAPON",
};
constexpr std::array<std::string_view, 5> mouseNames{"MOUSE1", "MOUSE3", "MOUSE2", "MOUSE4", "MOUSE5"};
ControlBinding canonical(ControlBinding binding) noexcept {
    if (binding.kind == BindingKind::Key) {
        auto key = static_cast<Key>(binding.code);
        if (key == Key::RightShift) binding.code = static_cast<std::uint8_t>(Key::LeftShift);
        if (key == Key::RightControl) binding.code = static_cast<std::uint8_t>(Key::LeftControl);
        if (key == Key::RightAlt) binding.code = static_cast<std::uint8_t>(Key::LeftAlt);
    }
    return binding;
}
bool read(const ControlBinding b, const wkc::platform::Input& input, const bool pressed) noexcept {
    if (!validControlBinding(b)) return false;
    if (b.kind == BindingKind::WheelUp) return input.mouseWheelDelta() > 0.0F;
    if (b.kind == BindingKind::WheelDown) return input.mouseWheelDelta() < 0.0F;
    if (b.kind == BindingKind::Mouse) {
        auto button = static_cast<MouseButton>(b.code);
        return pressed ? input.wasMouseButtonPressed(button) : input.isMouseButtonDown(button);
    }
    auto key = static_cast<Key>(b.code);
    auto check = [&](Key k) { return pressed ? input.wasKeyPressed(k) : input.isKeyDown(k); };
    return check(key) || (key == Key::LeftShift && check(Key::RightShift))
        || (key == Key::LeftControl && check(Key::RightControl))
        || (key == Key::LeftAlt && check(Key::RightAlt));
}
constexpr std::array<std::string_view, 16> gamepadNames{
    "A / CROSS", "B / CIRCLE", "X / SQUARE", "Y / TRIANGLE", "VIEW / CREATE",
    "MENU / OPTIONS", "L3", "R3", "LB / L1", "RB / R1", "DPAD UP",
    "DPAD DOWN", "DPAD LEFT", "DPAD RIGHT", "LT / L2", "RT / R2",
};
}
std::array<ControlBinding, controlActionCount> defaultControlBindings() noexcept {
    auto key = [](Key k) { return ControlBinding{BindingKind::Key, static_cast<std::uint8_t>(k)}; };
    return {key(Key::W), key(Key::S), key(Key::A), key(Key::D), key(Key::LeftShift), key(Key::LeftControl),
        key(Key::Space), {BindingKind::Mouse, 0U}, {BindingKind::Mouse, 2U}, key(Key::R), key(Key::E),
        key(Key::F), key(Key::G), key(Key::Digit1), key(Key::Digit2), key(Key::Digit3), key(Key::Digit4),
        key(Key::Digit5), key(Key::Q), key(Key::Tab), {BindingKind::WheelUp, 0U}, {BindingKind::WheelDown, 0U}};
}
std::array<GamepadButton, controlActionCount> defaultGamepadBindings() noexcept {
    using B = GamepadButton;
    return {B::Count, B::Count, B::Count, B::Count, B::LeftStick,
        B::East, B::South, B::RightTrigger, B::LeftTrigger, B::West,
        B::DpadUp, B::DpadDown, B::DpadLeft, B::DpadRight, B::Count,
        B::RightStick, B::Count, B::Count, B::North, B::Back,
        B::RightShoulder, B::LeftShoulder};
}
std::string_view controlActionName(ControlAction action) noexcept {
    const auto i = static_cast<std::size_t>(action); return i < actionNames.size() ? actionNames[i] : "UNKNOWN";
}
std::string_view bindingName(ControlBinding b) noexcept {
    if (!validControlBinding(b)) return "INVALID";
    if (b.kind == BindingKind::WheelUp) return "WHEEL UP";
    if (b.kind == BindingKind::WheelDown) return "WHEEL DOWN";
    return b.kind == BindingKind::Mouse ? mouseNames[b.code] : keyNames[b.code];
}
std::optional<ControlBinding> bindingForName(std::string_view name) noexcept {
    for (std::size_t i = 0; i < keyNames.size(); ++i) if (keyNames[i] == name) {
        auto b = canonical({BindingKind::Key, static_cast<std::uint8_t>(i)});
        return validControlBinding(b) ? std::optional{b} : std::nullopt;
    }
    for (std::size_t i = 0; i < mouseNames.size(); ++i) if (mouseNames[i] == name)
        return ControlBinding{BindingKind::Mouse, static_cast<std::uint8_t>(i)};
    if (name == "WHEEL UP") return ControlBinding{BindingKind::WheelUp, 0U};
    if (name == "WHEEL DOWN") return ControlBinding{BindingKind::WheelDown, 0U};
    return std::nullopt;
}
bool validControlBinding(ControlBinding b) noexcept {
    if (b.kind == BindingKind::Key) return b.code < keyNames.size()
        && b.code != static_cast<std::uint8_t>(Key::Escape) && b.code != static_cast<std::uint8_t>(Key::F3);
    if (b.kind == BindingKind::Mouse) return b.code < mouseNames.size();
    return (b.kind == BindingKind::WheelUp || b.kind == BindingKind::WheelDown) && b.code == 0U;
}
bool validateControlBindings(const ControlBindings& bindings) noexcept {
    for (std::size_t i = 0; i < bindings.values.size(); ++i) {
        if (!validControlBinding(bindings.values[i]) || canonical(bindings.values[i]) != bindings.values[i]) return false;
        for (std::size_t j = 0; j < i; ++j) if (bindings.values[i] == bindings.values[j]) return false;
    }
    return true;
}
bool assignControlBinding(ControlBindings& bindings, ControlAction action, ControlBinding binding) noexcept {
    auto i = static_cast<std::size_t>(action); binding = canonical(binding);
    if (i >= controlActionCount || !validControlBinding(binding) || !validateControlBindings(bindings)) return false;
    if (bindings.values[i] == binding) return false;
    for (std::size_t j = 0; j < controlActionCount; ++j)
        if (bindings.values[j] == binding) bindings.values[j] = bindings.values[i];
    bindings.values[i] = binding; return true;
}
std::optional<ControlBinding> pressedControlBinding(const wkc::platform::Input& input) noexcept {
    for (std::size_t i = 0; i < keyNames.size(); ++i) if (input.wasKeyPressed(static_cast<Key>(i))) {
        auto b = canonical({BindingKind::Key, static_cast<std::uint8_t>(i)});
        if (validControlBinding(b)) return b;
    }
    for (std::size_t i = 0; i < mouseNames.size(); ++i)
        if (input.wasMouseButtonPressed(static_cast<MouseButton>(i))) return ControlBinding{BindingKind::Mouse, static_cast<std::uint8_t>(i)};
    if (input.mouseWheelDelta() > 0.0F) return ControlBinding{BindingKind::WheelUp, 0U};
    if (input.mouseWheelDelta() < 0.0F) return ControlBinding{BindingKind::WheelDown, 0U};
    return std::nullopt;
}
std::string_view gamepadBindingName(const GamepadButton button) noexcept {
    const auto index = static_cast<std::size_t>(button);
    return index < gamepadNames.size() ? gamepadNames[index] : "UNBOUND";
}
std::optional<GamepadButton> gamepadBindingForName(const std::string_view name) noexcept {
    if (name == "UNBOUND") return GamepadButton::Count;
    for (std::size_t index = 0; index < gamepadNames.size(); ++index)
        if (name == gamepadNames[index] && index != static_cast<std::size_t>(GamepadButton::Start))
            return static_cast<GamepadButton>(index);
    return std::nullopt;
}
bool validGamepadBinding(const GamepadButton button) noexcept {
    return button == GamepadButton::Count
        || (static_cast<std::size_t>(button) < gamepadNames.size()
            && button != GamepadButton::Start);
}
bool validateGamepadBindings(const GamepadBindings& bindings) noexcept {
    for (std::size_t i = 0; i < bindings.values.size(); ++i) {
        const auto button = bindings.values[i];
        if (!validGamepadBinding(button)) return false;
        if (button == GamepadButton::Count) continue;
        for (std::size_t j = 0; j < i; ++j)
            if (bindings.values[j] == button) return false;
    }
    return true;
}
bool assignGamepadBinding(GamepadBindings& bindings, const ControlAction action,
    const GamepadButton button) noexcept {
    const auto index = static_cast<std::size_t>(action);
    if (index >= controlActionCount || !validGamepadBinding(button)
        || !validateGamepadBindings(bindings) || bindings.values[index] == button) return false;
    if (button != GamepadButton::Count) {
        for (std::size_t other = 0; other < controlActionCount; ++other)
            if (bindings.values[other] == button) bindings.values[other] = bindings.values[index];
    }
    bindings.values[index] = button;
    return true;
}
std::optional<GamepadButton> pressedGamepadBinding(const wkc::platform::Input& input) noexcept {
    for (std::size_t index = 0; index < gamepadNames.size(); ++index) {
        const auto button = static_cast<GamepadButton>(index);
        if (validGamepadBinding(button) && input.wasGamepadButtonPressed(button)) return button;
    }
    return std::nullopt;
}
bool controlDown(const ControlBindings& b, const GamepadBindings& pad, ControlAction a, const wkc::platform::Input& i) noexcept {
    const auto n = static_cast<std::size_t>(a);
    if (n >= controlActionCount) return false;
    const auto button = pad.values[n];
    return read(b.values[n], i, false)
        || (button != GamepadButton::Count && i.isGamepadButtonDown(button));
}
bool controlPressed(const ControlBindings& b, const GamepadBindings& pad, ControlAction a, const wkc::platform::Input& i) noexcept {
    const auto n = static_cast<std::size_t>(a);
    if (n >= controlActionCount) return false;
    const auto button = pad.values[n];
    return read(b.values[n], i, true)
        || (button != GamepadButton::Count && i.wasGamepadButtonPressed(button));
}
}
