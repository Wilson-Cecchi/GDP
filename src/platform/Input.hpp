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
    E,
    R,
    C,
    Space,
    Digit1,
    Digit2,
    Digit3,
    F3,
    Escape,
    Enter,
    Up,
    Down,
    Left,
    Right,
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

    [[nodiscard]] bool isKeyDown(Key key) const noexcept;
    [[nodiscard]] bool wasKeyPressed(Key key) const noexcept;
    [[nodiscard]] bool wasKeyReleased(Key key) const noexcept;
    [[nodiscard]] bool isMouseButtonDown(MouseButton button) const noexcept;
    [[nodiscard]] bool wasMouseButtonPressed(MouseButton button) const noexcept;
    [[nodiscard]] bool wasMouseButtonReleased(MouseButton button) const noexcept;
    [[nodiscard]] wkc::core::Vec2 mouseDelta() const noexcept;
    [[nodiscard]] wkc::core::Vec2 mousePosition() const noexcept;
    [[nodiscard]] float mouseWheelDelta() const noexcept;

private:
    static constexpr std::size_t keyCount = static_cast<std::size_t>(Key::Count);
    static constexpr std::size_t mouseButtonCount
        = static_cast<std::size_t>(MouseButton::Count);

    [[nodiscard]] static bool isValidKey(Key key) noexcept;
    [[nodiscard]] static bool isValidMouseButton(MouseButton button) noexcept;

    std::array<bool, keyCount> keysDown_{};
    std::array<bool, keyCount> keysPressed_{};
    std::array<bool, keyCount> keysReleased_{};
    std::array<bool, mouseButtonCount> mouseButtonsDown_{};
    std::array<bool, mouseButtonCount> mouseButtonsPressed_{};
    std::array<bool, mouseButtonCount> mouseButtonsReleased_{};
    wkc::core::Vec2 mouseDelta_{};
    wkc::core::Vec2 mousePosition_{};
    float mouseWheelDelta_{0.0F};
};

} // namespace wkc::platform
