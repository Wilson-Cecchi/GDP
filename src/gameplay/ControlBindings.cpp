#include "gameplay/ControlBindings.hpp"

namespace gdp::gameplay {
namespace {
using wkc::platform::Key;
using wkc::platform::MouseButton;
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
}
std::array<ControlBinding, controlActionCount> defaultControlBindings() noexcept {
    auto key = [](Key k) { return ControlBinding{BindingKind::Key, static_cast<std::uint8_t>(k)}; };
    return {key(Key::W), key(Key::S), key(Key::A), key(Key::D), key(Key::LeftShift), key(Key::LeftControl),
        key(Key::Space), {BindingKind::Mouse, 0U}, {BindingKind::Mouse, 2U}, key(Key::R), key(Key::E),
        key(Key::F), key(Key::G), key(Key::Digit1), key(Key::Digit2), key(Key::Digit3), key(Key::Digit4),
        key(Key::Digit5), key(Key::Q), key(Key::Tab), {BindingKind::WheelUp, 0U}, {BindingKind::WheelDown, 0U}};
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
bool controlDown(const ControlBindings& b, ControlAction a, const wkc::platform::Input& i) noexcept {
    auto n = static_cast<std::size_t>(a); return n < controlActionCount && read(b.values[n], i, false);
}
bool controlPressed(const ControlBindings& b, ControlAction a, const wkc::platform::Input& i) noexcept {
    auto n = static_cast<std::size_t>(a); return n < controlActionCount && read(b.values[n], i, true);
}
}
