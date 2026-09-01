#include "platform/SdlInput.hpp"

#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_scancode.h>

#include <optional>

namespace wkc::platform {
namespace {

std::optional<Key> keyFromSdl(const SDL_Scancode scancode) noexcept
{
    switch (scancode) {
    case SDL_SCANCODE_W:
        return Key::W;
    case SDL_SCANCODE_A:
        return Key::A;
    case SDL_SCANCODE_S:
        return Key::S;
    case SDL_SCANCODE_D:
        return Key::D;
    case SDL_SCANCODE_E:
        return Key::E;
    case SDL_SCANCODE_R:
        return Key::R;
    case SDL_SCANCODE_ESCAPE:
        return Key::Escape;
    case SDL_SCANCODE_RETURN:
        return Key::Enter;
    case SDL_SCANCODE_UP:
        return Key::Up;
    case SDL_SCANCODE_DOWN:
        return Key::Down;
    case SDL_SCANCODE_LEFT:
        return Key::Left;
    case SDL_SCANCODE_RIGHT:
        return Key::Right;
    default:
        return std::nullopt;
    }
}

std::optional<MouseButton> mouseButtonFromSdl(const unsigned int button) noexcept
{
    switch (button) {
    case SDL_BUTTON_LEFT:
        return MouseButton::Primary;
    case SDL_BUTTON_MIDDLE:
        return MouseButton::Middle;
    case SDL_BUTTON_RIGHT:
        return MouseButton::Secondary;
    case SDL_BUTTON_X1:
        return MouseButton::Extra1;
    case SDL_BUTTON_X2:
        return MouseButton::Extra2;
    default:
        return std::nullopt;
    }
}

} // namespace

bool handleSdlInputEvent(Input& input, const SDL_Event& event) noexcept
{
    if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
        const std::optional<Key> key = keyFromSdl(event.key.scancode);
        if (!key) {
            return false;
        }
        input.handleKey(
            *key,
            event.type == SDL_EVENT_KEY_DOWN,
            event.key.repeat);
        return true;
    }

    if (event.type == SDL_EVENT_MOUSE_MOTION) {
        input.handleMouseMotion(
            {event.motion.x, event.motion.y},
            {event.motion.xrel, event.motion.yrel});
        return true;
    }

    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
        || event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        const std::optional<MouseButton> button
            = mouseButtonFromSdl(event.button.button);
        if (!button) {
            input.handleMouseMotion({event.button.x, event.button.y}, {});
            return false;
        }
        input.handleMouseButton(
            *button,
            event.type == SDL_EVENT_MOUSE_BUTTON_DOWN,
            {event.button.x, event.button.y});
        return true;
    }

    return false;
}

} // namespace wkc::platform
