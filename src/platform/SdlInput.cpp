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
    case SDL_SCANCODE_Q:
        return Key::Q;
    case SDL_SCANCODE_E:
        return Key::E;
    case SDL_SCANCODE_F:
        return Key::F;
    case SDL_SCANCODE_G:
        return Key::G;
    case SDL_SCANCODE_R:
        return Key::R;
    case SDL_SCANCODE_LSHIFT:
        return Key::LeftShift;
    case SDL_SCANCODE_RSHIFT:
        return Key::RightShift;
    case SDL_SCANCODE_LCTRL:
        return Key::LeftControl;
    case SDL_SCANCODE_RCTRL:
        return Key::RightControl;
    case SDL_SCANCODE_SPACE:
        return Key::Space;
    case SDL_SCANCODE_1:
        return Key::Digit1;
    case SDL_SCANCODE_2:
        return Key::Digit2;
    case SDL_SCANCODE_3:
        return Key::Digit3;
    case SDL_SCANCODE_4:
        return Key::Digit4;
    case SDL_SCANCODE_5:
        return Key::Digit5;
    case SDL_SCANCODE_TAB:
        return Key::Tab;
    case SDL_SCANCODE_F3:
        return Key::F3;
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
    case SDL_SCANCODE_B: return Key::B;
    case SDL_SCANCODE_C: return Key::C;
    case SDL_SCANCODE_H: return Key::H;
    case SDL_SCANCODE_I: return Key::I;
    case SDL_SCANCODE_J: return Key::J;
    case SDL_SCANCODE_K: return Key::K;
    case SDL_SCANCODE_L: return Key::L;
    case SDL_SCANCODE_M: return Key::M;
    case SDL_SCANCODE_N: return Key::N;
    case SDL_SCANCODE_O: return Key::O;
    case SDL_SCANCODE_P: return Key::P;
    case SDL_SCANCODE_T: return Key::T;
    case SDL_SCANCODE_U: return Key::U;
    case SDL_SCANCODE_V: return Key::V;
    case SDL_SCANCODE_X: return Key::X;
    case SDL_SCANCODE_Y: return Key::Y;
    case SDL_SCANCODE_Z: return Key::Z;
    case SDL_SCANCODE_0: return Key::Digit0;
    case SDL_SCANCODE_6: return Key::Digit6;
    case SDL_SCANCODE_7: return Key::Digit7;
    case SDL_SCANCODE_8: return Key::Digit8;
    case SDL_SCANCODE_9: return Key::Digit9;
    case SDL_SCANCODE_LALT: return Key::LeftAlt;
    case SDL_SCANCODE_RALT: return Key::RightAlt;
    case SDL_SCANCODE_BACKSPACE: return Key::Backspace;
    case SDL_SCANCODE_DELETE: return Key::Delete;
    case SDL_SCANCODE_HOME: return Key::Home;
    case SDL_SCANCODE_END: return Key::End;
    case SDL_SCANCODE_PAGEUP: return Key::PageUp;
    case SDL_SCANCODE_PAGEDOWN: return Key::PageDown;
    case SDL_SCANCODE_F1: return Key::F1;
    case SDL_SCANCODE_F2: return Key::F2;
    case SDL_SCANCODE_F4: return Key::F4;
    case SDL_SCANCODE_F5: return Key::F5;
    case SDL_SCANCODE_F6: return Key::F6;
    case SDL_SCANCODE_F7: return Key::F7;
    case SDL_SCANCODE_F8: return Key::F8;
    case SDL_SCANCODE_F9: return Key::F9;
    case SDL_SCANCODE_F10: return Key::F10;
    case SDL_SCANCODE_F11: return Key::F11;
    case SDL_SCANCODE_F12: return Key::F12;
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

    if (event.type == SDL_EVENT_MOUSE_WHEEL) {
        const float direction = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED
            ? -1.0F : 1.0F;
        input.handleMouseWheel(event.wheel.y * direction);
        return true;
    }

    return false;
}

} // namespace wkc::platform
