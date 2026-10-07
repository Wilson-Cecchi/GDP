#include "platform/SdlGamepad.hpp"

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_stdinc.h>

#include <array>
#include <utility>

namespace wkc::platform {
namespace {

float stickAxis(const Sint16 value) noexcept
{
    return value < 0 ? static_cast<float>(value) / 32768.0F
                     : static_cast<float>(value) / 32767.0F;
}

constexpr std::array buttonMappings{
    std::pair{GamepadButton::South, SDL_GAMEPAD_BUTTON_SOUTH},
    std::pair{GamepadButton::East, SDL_GAMEPAD_BUTTON_EAST},
    std::pair{GamepadButton::West, SDL_GAMEPAD_BUTTON_WEST},
    std::pair{GamepadButton::North, SDL_GAMEPAD_BUTTON_NORTH},
    std::pair{GamepadButton::Back, SDL_GAMEPAD_BUTTON_BACK},
    std::pair{GamepadButton::Start, SDL_GAMEPAD_BUTTON_START},
    std::pair{GamepadButton::LeftStick, SDL_GAMEPAD_BUTTON_LEFT_STICK},
    std::pair{GamepadButton::RightStick, SDL_GAMEPAD_BUTTON_RIGHT_STICK},
    std::pair{GamepadButton::LeftShoulder, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
    std::pair{GamepadButton::RightShoulder, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
    std::pair{GamepadButton::DpadUp, SDL_GAMEPAD_BUTTON_DPAD_UP},
    std::pair{GamepadButton::DpadDown, SDL_GAMEPAD_BUTTON_DPAD_DOWN},
    std::pair{GamepadButton::DpadLeft, SDL_GAMEPAD_BUTTON_DPAD_LEFT},
    std::pair{GamepadButton::DpadRight, SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
};

} // namespace

SdlGamepad::~SdlGamepad()
{
    if (device_ != nullptr) SDL_CloseGamepad(device_);
}

void SdlGamepad::refresh(const SDL_JoystickID preferredId) noexcept
{
    if (device_ != nullptr) return;
    if (preferredId != 0) {
        device_ = SDL_OpenGamepad(preferredId);
        return;
    }
    int count = 0;
    SDL_JoystickID* const ids = SDL_GetGamepads(&count);
    if (ids == nullptr) return;
    for (int index = 0; index < count && device_ == nullptr; ++index) {
        device_ = SDL_OpenGamepad(ids[index]);
    }
    SDL_free(ids);
}

void SdlGamepad::handleEvent(Input& input, const SDL_Event& event) noexcept
{
    if (event.type == SDL_EVENT_GAMEPAD_REMOVED && device_ != nullptr
        && !SDL_GamepadConnected(device_)) {
        close(input);
        refresh();
    } else if (event.type == SDL_EVENT_GAMEPAD_ADDED && device_ == nullptr) {
        refresh();
    } else if (device_ != nullptr
        && (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN
            || event.type == SDL_EVENT_GAMEPAD_BUTTON_UP)
        && event.gbutton.which == SDL_GetGamepadID(device_)) {
        for (const auto& [button, mapped] : buttonMappings) {
            if (event.gbutton.button == mapped) {
                input.handleGamepadButton(button,
                    event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
                break;
            }
        }
    } else if (device_ != nullptr
        && event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION
        && event.gaxis.which == SDL_GetGamepadID(device_)) {
        if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER) {
            input.handleGamepadButton(GamepadButton::LeftTrigger,
                event.gaxis.value > 14745);
        } else if (event.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
            input.handleGamepadButton(GamepadButton::RightTrigger,
                event.gaxis.value > 14745);
        }
    }
}

void SdlGamepad::poll(Input& input) noexcept
{
    if (device_ != nullptr && !SDL_GamepadConnected(device_)) {
        close(input);
        refresh();
    }
    if (device_ == nullptr) return;

    for (const auto& [button, mapped] : buttonMappings) {
        input.handleGamepadButton(button, SDL_GetGamepadButton(device_, mapped));
    }
    input.handleGamepadButton(GamepadButton::LeftTrigger,
        SDL_GetGamepadAxis(device_, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 14745);
    input.handleGamepadButton(GamepadButton::RightTrigger,
        SDL_GetGamepadAxis(device_, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 14745);
    input.handleGamepadSticks(
        {stickAxis(SDL_GetGamepadAxis(device_, SDL_GAMEPAD_AXIS_LEFTX)),
            stickAxis(SDL_GetGamepadAxis(device_, SDL_GAMEPAD_AXIS_LEFTY))},
        {stickAxis(SDL_GetGamepadAxis(device_, SDL_GAMEPAD_AXIS_RIGHTX)),
            stickAxis(SDL_GetGamepadAxis(device_, SDL_GAMEPAD_AXIS_RIGHTY))});
}

void SdlGamepad::close(Input& input) noexcept
{
    if (device_ != nullptr) SDL_CloseGamepad(device_);
    device_ = nullptr;
    input.resetGamepad();
}

bool SdlGamepad::connected() const noexcept
{
    return device_ != nullptr && SDL_GamepadConnected(device_);
}

const char* SdlGamepad::name() const noexcept
{
    return connected() ? SDL_GetGamepadName(device_) : nullptr;
}

} // namespace wkc::platform
