#pragma once

#include "platform/Input.hpp"

#include <SDL3/SDL_events.h>

struct SDL_Gamepad;

namespace wkc::platform {

// Uses the first SDL-mapped controller. SDL normalizes Xbox and PlayStation
// layouts to positional face buttons and common stick/trigger axes.
class SdlGamepad {
public:
    SdlGamepad() = default;
    ~SdlGamepad();
    SdlGamepad(const SdlGamepad&) = delete;
    SdlGamepad& operator=(const SdlGamepad&) = delete;

    void refresh(SDL_JoystickID preferredId = 0) noexcept;
    void handleEvent(Input& input, const SDL_Event& event) noexcept;
    void poll(Input& input) noexcept;
    void close(Input& input) noexcept;
    [[nodiscard]] bool connected() const noexcept;
    [[nodiscard]] const char* name() const noexcept;

private:
    SDL_Gamepad* device_{nullptr};
};

} // namespace wkc::platform
