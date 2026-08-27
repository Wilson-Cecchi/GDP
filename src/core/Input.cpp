#include "core/Input.hpp"

#include <SDL3/SDL_events.h>

#include <cstddef>

namespace gdp::core {

void Input::beginFrame() noexcept
{
    keysPressed_.fill(false);
    keysReleased_.fill(false);
    mouseButtonsPressed_.fill(false);
    mouseButtonsReleased_.fill(false);
    mouseDelta_ = {};
}

void Input::handleEvent(const SDL_Event& event) noexcept
{
    if (event.type == SDL_EVENT_KEY_DOWN && isValidScancode(event.key.scancode)) {
        const auto index = static_cast<std::size_t>(event.key.scancode);
        if (!event.key.repeat && !keysDown_[index]) {
            keysPressed_[index] = true;
        }
        keysDown_[index] = true;
        return;
    }

    if (event.type == SDL_EVENT_KEY_UP && isValidScancode(event.key.scancode)) {
        const auto index = static_cast<std::size_t>(event.key.scancode);
        keysReleased_[index] = keysDown_[index];
        keysDown_[index] = false;
        return;
    }

    if (event.type == SDL_EVENT_MOUSE_MOTION) {
        mouseDelta_ = mouseDelta_ + Vec2{event.motion.xrel, event.motion.yrel};
        return;
    }

    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        const auto button = static_cast<unsigned int>(event.button.button);
        if (!isValidMouseButton(button)) {
            return;
        }
        const auto index = static_cast<std::size_t>(button);
        if (!mouseButtonsDown_[index]) {
            mouseButtonsPressed_[index] = true;
        }
        mouseButtonsDown_[index] = true;
        return;
    }

    if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        const auto button = static_cast<unsigned int>(event.button.button);
        if (!isValidMouseButton(button)) {
            return;
        }
        const auto index = static_cast<std::size_t>(button);
        mouseButtonsReleased_[index] = mouseButtonsDown_[index];
        mouseButtonsDown_[index] = false;
    }
}

bool Input::isKeyDown(const SDL_Scancode scancode) const noexcept
{
    return isValidScancode(scancode)
        && keysDown_[static_cast<std::size_t>(scancode)];
}

bool Input::wasKeyPressed(const SDL_Scancode scancode) const noexcept
{
    return isValidScancode(scancode)
        && keysPressed_[static_cast<std::size_t>(scancode)];
}

bool Input::wasKeyReleased(const SDL_Scancode scancode) const noexcept
{
    return isValidScancode(scancode)
        && keysReleased_[static_cast<std::size_t>(scancode)];
}

bool Input::isMouseButtonDown(const unsigned int button) const noexcept
{
    return isValidMouseButton(button)
        && mouseButtonsDown_[static_cast<std::size_t>(button)];
}

bool Input::wasMouseButtonPressed(const unsigned int button) const noexcept
{
    return isValidMouseButton(button)
        && mouseButtonsPressed_[static_cast<std::size_t>(button)];
}

bool Input::wasMouseButtonReleased(const unsigned int button) const noexcept
{
    return isValidMouseButton(button)
        && mouseButtonsReleased_[static_cast<std::size_t>(button)];
}

Vec2 Input::mouseDelta() const noexcept
{
    return mouseDelta_;
}

bool Input::isValidScancode(const SDL_Scancode scancode) noexcept
{
    return scancode > SDL_SCANCODE_UNKNOWN && scancode < SDL_SCANCODE_COUNT;
}

bool Input::isValidMouseButton(const unsigned int button) noexcept
{
    return button > 0U && button < mouseButtonCount;
}

} // namespace gdp::core
