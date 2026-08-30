#include "platform/SdlInput.hpp"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_scancode.h>

#include <array>
#include <iostream>
#include <string_view>
#include <utility>

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void testSdlTranslation()
{
    using gdp::platform::Key;
    using gdp::platform::MouseButton;

    gdp::platform::Input input;
    input.beginFrame();

    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.scancode = SDL_SCANCODE_W;
    expect(gdp::platform::handleSdlInputEvent(input, event),
        "mapped SDL key event is handled");
    expect(input.isKeyDown(Key::W) && input.wasKeyPressed(Key::W),
        "SDL W maps to the neutral W key");

    input.beginFrame();
    event.key.repeat = true;
    expect(gdp::platform::handleSdlInputEvent(input, event),
        "repeated SDL key event is handled");
    expect(input.isKeyDown(Key::W) && !input.wasKeyPressed(Key::W),
        "SDL repeat preserves neutral transition semantics");

    input.beginFrame();
    event.type = SDL_EVENT_KEY_UP;
    event.key.repeat = false;
    expect(gdp::platform::handleSdlInputEvent(input, event),
        "SDL key release is handled");
    expect(!input.isKeyDown(Key::W) && input.wasKeyReleased(Key::W),
        "SDL key release maps to the neutral transition");

    constexpr std::array controlMappings{
        std::pair{SDL_SCANCODE_W, Key::W},
        std::pair{SDL_SCANCODE_A, Key::A},
        std::pair{SDL_SCANCODE_S, Key::S},
        std::pair{SDL_SCANCODE_D, Key::D},
        std::pair{SDL_SCANCODE_E, Key::E},
        std::pair{SDL_SCANCODE_R, Key::R},
        std::pair{SDL_SCANCODE_ESCAPE, Key::Escape},
        std::pair{SDL_SCANCODE_RETURN, Key::Enter},
        std::pair{SDL_SCANCODE_UP, Key::Up},
        std::pair{SDL_SCANCODE_DOWN, Key::Down},
    };
    for (const auto& [scancode, key] : controlMappings) {
        gdp::platform::Input mappedInput;
        SDL_Event mappedEvent{};
        mappedEvent.type = SDL_EVENT_KEY_DOWN;
        mappedEvent.key.scancode = scancode;
        expect(gdp::platform::handleSdlInputEvent(mappedInput, mappedEvent)
                && mappedInput.isKeyDown(key),
            "every GDP control has an SDL mapping");
    }

    event = {};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.scancode = SDL_SCANCODE_F12;
    expect(!gdp::platform::handleSdlInputEvent(input, event),
        "unmapped SDL key is ignored");

    event = {};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.x = 320.0F;
    event.motion.y = 180.0F;
    event.motion.xrel = 3.0F;
    event.motion.yrel = -2.0F;
    expect(gdp::platform::handleSdlInputEvent(input, event),
        "SDL mouse motion is handled");
    expect(input.mousePosition() == gdp::core::Vec2{320.0F, 180.0F}
            && input.mouseDelta() == gdp::core::Vec2{3.0F, -2.0F},
        "SDL mouse motion maps position and delta");

    event = {};
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = 320.0F;
    event.button.y = 180.0F;
    expect(gdp::platform::handleSdlInputEvent(input, event),
        "SDL primary button is handled");
    expect(input.isMouseButtonDown(MouseButton::Primary)
            && input.wasMouseButtonPressed(MouseButton::Primary),
        "SDL left button maps to neutral primary button");

    input.beginFrame();
    event.type = SDL_EVENT_MOUSE_BUTTON_UP;
    expect(gdp::platform::handleSdlInputEvent(input, event),
        "SDL primary button release is handled");
    expect(!input.isMouseButtonDown(MouseButton::Primary)
            && input.wasMouseButtonReleased(MouseButton::Primary),
        "SDL primary release maps to the neutral transition");
}

} // namespace

int main()
{
    testSdlTranslation();

    if (failures != 0) {
        std::cerr << failures << " SDL input adapter test(s) failed\n";
        return 1;
    }

    std::cout << "All SDL input adapter tests passed\n";
    return 0;
}
