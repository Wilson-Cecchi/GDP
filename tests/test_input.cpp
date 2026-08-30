#include "platform/Input.hpp"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_scancode.h>

#include <iostream>
#include <string_view>

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void testInput()
{
    gdp::platform::Input input;
    input.beginFrame();

    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.scancode = SDL_SCANCODE_W;
    input.handleEvent(event);
    expect(input.isKeyDown(SDL_SCANCODE_W), "key-down state is captured");
    expect(input.wasKeyPressed(SDL_SCANCODE_W), "key press transition is captured");

    input.beginFrame();
    event.key.repeat = true;
    input.handleEvent(event);
    expect(input.isKeyDown(SDL_SCANCODE_W), "repeated key remains down");
    expect(!input.wasKeyPressed(SDL_SCANCODE_W),
        "key repeat does not create a press transition");

    input.beginFrame();
    event = {};
    event.type = SDL_EVENT_KEY_UP;
    event.key.scancode = SDL_SCANCODE_W;
    input.handleEvent(event);
    expect(!input.isKeyDown(SDL_SCANCODE_W), "key-up clears held state");
    expect(input.wasKeyReleased(SDL_SCANCODE_W), "key release transition is captured");

    input.beginFrame();
    event = {};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.x = 120.0F;
    event.motion.y = 90.0F;
    event.motion.xrel = 2.5F;
    event.motion.yrel = -1.0F;
    input.handleEvent(event);
    event.motion.x = 124.0F;
    event.motion.y = 92.0F;
    event.motion.xrel = 1.5F;
    event.motion.yrel = 3.0F;
    input.handleEvent(event);
    expect(input.mouseDelta() == gdp::core::Vec2{4.0F, 2.0F},
        "mouse motion accumulates during a frame");
    expect(input.mousePosition() == gdp::core::Vec2{124.0F, 92.0F},
        "mouse motion keeps the latest absolute position");

    event = {};
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = 124.0F;
    event.button.y = 92.0F;
    input.handleEvent(event);
    expect(input.isMouseButtonDown(SDL_BUTTON_LEFT),
        "mouse button held state is captured");
    expect(input.wasMouseButtonPressed(SDL_BUTTON_LEFT),
        "mouse press transition is captured");

    input.beginFrame();
    expect(input.isMouseButtonDown(SDL_BUTTON_LEFT),
        "beginFrame preserves held state");
    expect(!input.wasMouseButtonPressed(SDL_BUTTON_LEFT),
        "beginFrame clears transitions");
    expect(input.mouseDelta() == gdp::core::Vec2{},
        "beginFrame clears mouse delta");
    expect(input.mousePosition() == gdp::core::Vec2{124.0F, 92.0F},
        "beginFrame preserves absolute mouse position");

    event.type = SDL_EVENT_MOUSE_BUTTON_UP;
    input.handleEvent(event);
    expect(input.wasMouseButtonReleased(SDL_BUTTON_LEFT),
        "mouse release transition is captured");
}

} // namespace

int main()
{
    testInput();

    if (failures != 0) {
        std::cerr << failures << " input test(s) failed\n";
        return 1;
    }

    std::cout << "All GDP platform input tests passed\n";
    return 0;
}
