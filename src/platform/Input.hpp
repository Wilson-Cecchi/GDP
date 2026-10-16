#pragma once

#include "core/Math.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace wkc::platform {

enum class Key : std::uint8_t {
    W,
    A,
    S,
    D,
    Q,
    E,
    F,
    G,
    R,
    LeftShift,
    RightShift,
    LeftControl,
    RightControl,
    Space,
    Digit1,
    Digit2,
    Digit3,
    Digit4,
    Digit5,
    Tab,
    F3,
    Escape,
    Enter,
    Up,
    Down,
    Left,
    Right,
    B, C, H, I, J, K, L, M, N, O, P, T, U, V, X, Y, Z,
    Digit0, Digit6, Digit7, Digit8, Digit9,
    LeftAlt, RightAlt, Backspace, Delete, Home, End, PageUp, PageDown,
    F1, F2, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    Count,
};

enum class MouseButton : std::uint8_t {
    Primary,
    Middle,
    Secondary,
    Extra1,
    Extra2,
    Count,
};

enum class GamepadButton : std::uint8_t {
    South, East, West, North, Back, Start,
    LeftStick, RightStick, LeftShoulder, RightShoulder,
    DpadUp, DpadDown, DpadLeft, DpadRight,
    LeftTrigger, RightTrigger,
    StickUp, StickDown, StickLeft, StickRight,
    Count,
};

enum class InputDevice : std::uint8_t {
    KeyboardMouse,
    Gamepad,
};

[[nodiscard]] wkc::core::Vec2 gamepadDeadzone(wkc::core::Vec2 value, float threshold) noexcept;

class Input {
public:
    void beginFrame() noexcept;
    void handleKey(Key key, bool down, bool repeat = false) noexcept;
    void handleMouseButton(
        MouseButton button,
        bool down,
        wkc::core::Vec2 position) noexcept;
    void handleMouseMotion(
        wkc::core::Vec2 position,
        wkc::core::Vec2 delta) noexcept;
    void handleMouseWheel(float delta) noexcept;
    void handleGamepadButton(GamepadButton button, bool down) noexcept;
    void handleGamepadSticks(wkc::core::Vec2 move, wkc::core::Vec2 look) noexcept;
    void resetGamepad() noexcept;

    [[nodiscard]] bool isKeyDown(Key key) const noexcept;
    [[nodiscard]] bool wasKeyPressed(Key key) const noexcept;
    [[nodiscard]] bool wasKeyReleased(Key key) const noexcept;
    [[nodiscard]] bool isMouseButtonDown(MouseButton button) const noexcept;
    [[nodiscard]] bool wasMouseButtonPressed(MouseButton button) const noexcept;
    [[nodiscard]] bool wasMouseButtonReleased(MouseButton button) const noexcept;
    [[nodiscard]] wkc::core::Vec2 mouseDelta() const noexcept;
    [[nodiscard]] wkc::core::Vec2 mousePosition() const noexcept;
    [[nodiscard]] float mouseWheelDelta() const noexcept;
    [[nodiscard]] bool isGamepadButtonDown(GamepadButton button) const noexcept;
    [[nodiscard]] bool wasGamepadButtonPressed(GamepadButton button) const noexcept;
    [[nodiscard]] wkc::core::Vec2 gamepadMove() const noexcept;
    [[nodiscard]] wkc::core::Vec2 gamepadLook() const noexcept;
    [[nodiscard]] wkc::core::Vec2 gamepadLeftStick() const noexcept;
    [[nodiscard]] wkc::core::Vec2 gamepadRightStick() const noexcept;
    [[nodiscard]] InputDevice lastActiveDevice() const noexcept;

private:
    static constexpr std::size_t keyCount = static_cast<std::size_t>(Key::Count);
    static constexpr std::size_t mouseButtonCount
        = static_cast<std::size_t>(MouseButton::Count);
    static constexpr std::size_t gamepadButtonCount
        = static_cast<std::size_t>(GamepadButton::Count);

    [[nodiscard]] static bool isValidKey(Key key) noexcept;
    [[nodiscard]] static bool isValidMouseButton(MouseButton button) noexcept;
    [[nodiscard]] static bool isValidGamepadButton(GamepadButton button) noexcept;

    std::array<bool, keyCount> keysDown_{};
    std::array<bool, keyCount> keysPressed_{};
    std::array<bool, keyCount> keysReleased_{};
    std::array<bool, mouseButtonCount> mouseButtonsDown_{};
    std::array<bool, mouseButtonCount> mouseButtonsPressed_{};
    std::array<bool, mouseButtonCount> mouseButtonsReleased_{};
    std::array<bool, gamepadButtonCount> gamepadButtonsDown_{};
    std::array<bool, gamepadButtonCount> gamepadButtonsPressed_{};
    wkc::core::Vec2 gamepadMove_{};
    wkc::core::Vec2 gamepadLook_{};
    wkc::core::Vec2 gamepadLeftStick_{};
    wkc::core::Vec2 gamepadRightStick_{};
    wkc::core::Vec2 gamepadActivityMove_{};
    wkc::core::Vec2 gamepadActivityLook_{};
    wkc::core::Vec2 mouseDelta_{};
    wkc::core::Vec2 mousePosition_{};
    float mouseWheelDelta_{0.0F};
    InputDevice lastActiveDevice_{InputDevice::KeyboardMouse};
};

} // namespace wkc::platform
