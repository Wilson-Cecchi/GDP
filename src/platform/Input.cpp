#include "platform/Input.hpp"

#include <cmath>
#include <cstddef>
#include <algorithm>

namespace wkc::platform {
wkc::core::Vec2 gamepadDeadzone(const wkc::core::Vec2 value, const float threshold) noexcept
{
    if (!std::isfinite(value.x) || !std::isfinite(value.y)) return {};
    const float minimum = std::clamp(threshold, 0.0F, 0.4F);
    const float length = std::sqrt(value.x * value.x + value.y * value.y);
    if (length <= minimum) return {};
    const float scale = std::clamp((length - minimum) / (1.0F - minimum),
        0.0F, 1.0F) / length;
    return {value.x * scale, value.y * scale};
}

void Input::beginFrame() noexcept
{
    keysPressed_.fill(false);
    keysReleased_.fill(false);
    mouseButtonsPressed_.fill(false);
    mouseButtonsReleased_.fill(false);
    gamepadButtonsPressed_.fill(false);
    mouseDelta_ = {};
    mouseWheelDelta_ = 0.0F;
}

void Input::handleKey(const Key key, const bool down, const bool repeat) noexcept
{
    if (!isValidKey(key)) {
        return;
    }
    const auto index = static_cast<std::size_t>(key);
    if (down) {
        if (!repeat && !keysDown_[index]) {
            keysPressed_[index] = true;
        }
        keysDown_[index] = true;
        return;
    }

    keysReleased_[index] = keysDown_[index];
    keysDown_[index] = false;
}

void Input::handleMouseButton(
    const MouseButton button,
    const bool down,
    const wkc::core::Vec2 position) noexcept
{
    mousePosition_ = position;
    if (!isValidMouseButton(button)) {
        return;
    }
    const auto index = static_cast<std::size_t>(button);
    if (down) {
        if (!mouseButtonsDown_[index]) {
            mouseButtonsPressed_[index] = true;
        }
        mouseButtonsDown_[index] = true;
        return;
    }

    mouseButtonsReleased_[index] = mouseButtonsDown_[index];
    mouseButtonsDown_[index] = false;
}

void Input::handleMouseMotion(
    const wkc::core::Vec2 position,
    const wkc::core::Vec2 delta) noexcept
{
    mouseDelta_ = mouseDelta_ + delta;
    mousePosition_ = position;
}

void Input::handleMouseWheel(const float delta) noexcept
{
    if (std::isfinite(delta)) {
        mouseWheelDelta_ += delta;
    }
}

void Input::handleGamepadButton(const GamepadButton button, const bool down) noexcept
{
    if (!isValidGamepadButton(button)) return;
    const auto index = static_cast<std::size_t>(button);
    gamepadButtonsPressed_[index] = gamepadButtonsPressed_[index]
        || (down && !gamepadButtonsDown_[index]);
    gamepadButtonsDown_[index] = down;
}

void Input::handleGamepadSticks(
    const wkc::core::Vec2 move, const wkc::core::Vec2 look) noexcept
{
    gamepadLeftStick_ = std::isfinite(move.x) && std::isfinite(move.y) ? move : wkc::core::Vec2{};
    gamepadRightStick_ = std::isfinite(look.x) && std::isfinite(look.y) ? look : wkc::core::Vec2{};
    gamepadMove_ = gamepadDeadzone(gamepadLeftStick_, 0.18F);
    gamepadLook_ = gamepadDeadzone(gamepadRightStick_, 0.18F);
    constexpr float navigationThreshold = 0.6F;
    handleGamepadButton(GamepadButton::StickUp,
        gamepadMove_.y < -navigationThreshold);
    handleGamepadButton(GamepadButton::StickDown,
        gamepadMove_.y > navigationThreshold);
    handleGamepadButton(GamepadButton::StickLeft,
        gamepadMove_.x < -navigationThreshold);
    handleGamepadButton(GamepadButton::StickRight,
        gamepadMove_.x > navigationThreshold);
}

void Input::resetGamepad() noexcept
{
    gamepadButtonsDown_.fill(false);
    gamepadButtonsPressed_.fill(false);
    gamepadMove_ = {};
    gamepadLook_ = {};
    gamepadLeftStick_ = {};
    gamepadRightStick_ = {};
}

bool Input::isKeyDown(const Key key) const noexcept
{
    return isValidKey(key) && keysDown_[static_cast<std::size_t>(key)];
}

bool Input::wasKeyPressed(const Key key) const noexcept
{
    return isValidKey(key) && keysPressed_[static_cast<std::size_t>(key)];
}

bool Input::wasKeyReleased(const Key key) const noexcept
{
    return isValidKey(key) && keysReleased_[static_cast<std::size_t>(key)];
}

bool Input::isMouseButtonDown(const MouseButton button) const noexcept
{
    return isValidMouseButton(button)
        && mouseButtonsDown_[static_cast<std::size_t>(button)];
}

bool Input::wasMouseButtonPressed(const MouseButton button) const noexcept
{
    return isValidMouseButton(button)
        && mouseButtonsPressed_[static_cast<std::size_t>(button)];
}

bool Input::wasMouseButtonReleased(const MouseButton button) const noexcept
{
    return isValidMouseButton(button)
        && mouseButtonsReleased_[static_cast<std::size_t>(button)];
}

wkc::core::Vec2 Input::mouseDelta() const noexcept
{
    return mouseDelta_;
}

wkc::core::Vec2 Input::mousePosition() const noexcept
{
    return mousePosition_;
}

float Input::mouseWheelDelta() const noexcept
{
    return mouseWheelDelta_;
}

bool Input::isGamepadButtonDown(const GamepadButton button) const noexcept
{
    return isValidGamepadButton(button)
        && gamepadButtonsDown_[static_cast<std::size_t>(button)];
}

bool Input::wasGamepadButtonPressed(const GamepadButton button) const noexcept
{
    return isValidGamepadButton(button)
        && gamepadButtonsPressed_[static_cast<std::size_t>(button)];
}

wkc::core::Vec2 Input::gamepadMove() const noexcept { return gamepadMove_; }
wkc::core::Vec2 Input::gamepadLook() const noexcept { return gamepadLook_; }
wkc::core::Vec2 Input::gamepadLeftStick() const noexcept { return gamepadLeftStick_; }
wkc::core::Vec2 Input::gamepadRightStick() const noexcept { return gamepadRightStick_; }

bool Input::isValidKey(const Key key) noexcept
{
    return key < Key::Count;
}

bool Input::isValidMouseButton(const MouseButton button) noexcept
{
    return button < MouseButton::Count;
}

bool Input::isValidGamepadButton(const GamepadButton button) noexcept
{
    return button < GamepadButton::Count;
}

} // namespace wkc::platform
