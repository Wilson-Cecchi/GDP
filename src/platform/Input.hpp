#pragma once

#include "core/Math.hpp"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_scancode.h>

#include <array>
#include <cstddef>

namespace gdp::platform {

class Input {
public:
    void beginFrame() noexcept;
    void handleEvent(const SDL_Event& event) noexcept;

    [[nodiscard]] bool isKeyDown(SDL_Scancode scancode) const noexcept;
    [[nodiscard]] bool wasKeyPressed(SDL_Scancode scancode) const noexcept;
    [[nodiscard]] bool wasKeyReleased(SDL_Scancode scancode) const noexcept;
    [[nodiscard]] bool isMouseButtonDown(unsigned int button) const noexcept;
    [[nodiscard]] bool wasMouseButtonPressed(unsigned int button) const noexcept;
    [[nodiscard]] bool wasMouseButtonReleased(unsigned int button) const noexcept;
    [[nodiscard]] gdp::core::Vec2 mouseDelta() const noexcept;
    [[nodiscard]] gdp::core::Vec2 mousePosition() const noexcept;

private:
    static constexpr std::size_t keyCount = static_cast<std::size_t>(SDL_SCANCODE_COUNT);
    static constexpr std::size_t mouseButtonCount = 8;

    [[nodiscard]] static bool isValidScancode(SDL_Scancode scancode) noexcept;
    [[nodiscard]] static bool isValidMouseButton(unsigned int button) noexcept;

    std::array<bool, keyCount> keysDown_{};
    std::array<bool, keyCount> keysPressed_{};
    std::array<bool, keyCount> keysReleased_{};
    std::array<bool, mouseButtonCount> mouseButtonsDown_{};
    std::array<bool, mouseButtonCount> mouseButtonsPressed_{};
    std::array<bool, mouseButtonCount> mouseButtonsReleased_{};
    gdp::core::Vec2 mouseDelta_{};
    gdp::core::Vec2 mousePosition_{};
};

} // namespace gdp::platform
