#include "platform/Input.hpp"

#include <cmath>
#include <cstddef>

namespace wkc::platform {

void Input::beginFrame() noexcept
{
    keysPressed_.fill(false);
    keysReleased_.fill(false);
    mouseButtonsPressed_.fill(false);
    mouseButtonsReleased_.fill(false);
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

bool Input::isValidKey(const Key key) noexcept
{
    return key < Key::Count;
}

bool Input::isValidMouseButton(const MouseButton button) noexcept
{
    return button < MouseButton::Count;
}

} // namespace wkc::platform
