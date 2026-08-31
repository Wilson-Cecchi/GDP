#pragma once

#include "platform/Input.hpp"

#include <SDL3/SDL_events.h>

namespace wkc::platform {

[[nodiscard]] bool handleSdlInputEvent(
    Input& input,
    const SDL_Event& event) noexcept;

} // namespace wkc::platform
