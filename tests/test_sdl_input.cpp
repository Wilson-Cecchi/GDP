#include "platform/SdlInput.hpp"
#include "platform/SdlGamepad.hpp"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_joystick.h>
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
    using wkc::platform::Key;
    using wkc::platform::MouseButton;

    wkc::platform::Input input;
    input.beginFrame();

    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.scancode = SDL_SCANCODE_W;
    expect(wkc::platform::handleSdlInputEvent(input, event),
        "mapped SDL key event is handled");
    expect(input.isKeyDown(Key::W) && input.wasKeyPressed(Key::W),
        "SDL W maps to the neutral W key");

    input.beginFrame();
    event.key.repeat = true;
    expect(wkc::platform::handleSdlInputEvent(input, event),
        "repeated SDL key event is handled");
    expect(input.isKeyDown(Key::W) && !input.wasKeyPressed(Key::W),
        "SDL repeat preserves neutral transition semantics");

    input.beginFrame();
    event.type = SDL_EVENT_KEY_UP;
    event.key.repeat = false;
    expect(wkc::platform::handleSdlInputEvent(input, event),
        "SDL key release is handled");
    expect(!input.isKeyDown(Key::W) && input.wasKeyReleased(Key::W),
        "SDL key release maps to the neutral transition");

    constexpr std::array controlMappings{
        std::pair{SDL_SCANCODE_W, Key::W},
        std::pair{SDL_SCANCODE_A, Key::A},
        std::pair{SDL_SCANCODE_S, Key::S},
        std::pair{SDL_SCANCODE_D, Key::D},
        std::pair{SDL_SCANCODE_Q, Key::Q},
        std::pair{SDL_SCANCODE_E, Key::E},
        std::pair{SDL_SCANCODE_F, Key::F},
        std::pair{SDL_SCANCODE_G, Key::G},
        std::pair{SDL_SCANCODE_R, Key::R},
        std::pair{SDL_SCANCODE_LSHIFT, Key::LeftShift},
        std::pair{SDL_SCANCODE_RSHIFT, Key::RightShift},
        std::pair{SDL_SCANCODE_LCTRL, Key::LeftControl},
        std::pair{SDL_SCANCODE_RCTRL, Key::RightControl},
        std::pair{SDL_SCANCODE_SPACE, Key::Space},
        std::pair{SDL_SCANCODE_1, Key::Digit1},
        std::pair{SDL_SCANCODE_2, Key::Digit2},
        std::pair{SDL_SCANCODE_3, Key::Digit3},
        std::pair{SDL_SCANCODE_4, Key::Digit4},
        std::pair{SDL_SCANCODE_5, Key::Digit5},
        std::pair{SDL_SCANCODE_TAB, Key::Tab},
        std::pair{SDL_SCANCODE_F3, Key::F3},
        std::pair{SDL_SCANCODE_ESCAPE, Key::Escape},
        std::pair{SDL_SCANCODE_RETURN, Key::Enter},
        std::pair{SDL_SCANCODE_UP, Key::Up},
        std::pair{SDL_SCANCODE_DOWN, Key::Down},
        std::pair{SDL_SCANCODE_LEFT, Key::Left},
        std::pair{SDL_SCANCODE_RIGHT, Key::Right},
        std::pair{SDL_SCANCODE_C, Key::C},
        std::pair{SDL_SCANCODE_P, Key::P},
        std::pair{SDL_SCANCODE_9, Key::Digit9},
        std::pair{SDL_SCANCODE_LALT, Key::LeftAlt},
        std::pair{SDL_SCANCODE_PAGEUP, Key::PageUp},
    };
    for (const auto& [scancode, key] : controlMappings) {
        wkc::platform::Input mappedInput;
        SDL_Event mappedEvent{};
        mappedEvent.type = SDL_EVENT_KEY_DOWN;
        mappedEvent.key.scancode = scancode;
        expect(wkc::platform::handleSdlInputEvent(mappedInput, mappedEvent)
                && mappedInput.isKeyDown(key),
            "every GDP control has an SDL mapping");
    }

    wkc::platform::Input crouchInput;
    SDL_Event crouchEvent{};
    crouchEvent.type = SDL_EVENT_KEY_DOWN;
    crouchEvent.key.scancode = SDL_SCANCODE_LCTRL;
    expect(wkc::platform::handleSdlInputEvent(crouchInput, crouchEvent)
            && crouchInput.isKeyDown(Key::LeftControl),
        "left Control activates the crouch binding");
    crouchEvent.key.scancode = SDL_SCANCODE_RCTRL;
    expect(wkc::platform::handleSdlInputEvent(crouchInput, crouchEvent)
            && crouchInput.isKeyDown(Key::RightControl),
        "right Control can be held independently");
    crouchEvent.type = SDL_EVENT_KEY_UP;
    crouchEvent.key.scancode = SDL_SCANCODE_LCTRL;
    expect(wkc::platform::handleSdlInputEvent(crouchInput, crouchEvent)
            && !crouchInput.isKeyDown(Key::LeftControl)
            && crouchInput.isKeyDown(Key::RightControl),
        "releasing one Control leaves the other held");
    crouchEvent.key.scancode = SDL_SCANCODE_RCTRL;
    expect(wkc::platform::handleSdlInputEvent(crouchInput, crouchEvent)
            && !crouchInput.isKeyDown(Key::RightControl),
        "releasing both Control keys ends the crouch binding");

    crouchEvent = {};
    crouchEvent.type = SDL_EVENT_KEY_DOWN;
    crouchEvent.key.scancode = SDL_SCANCODE_C;
    expect(wkc::platform::handleSdlInputEvent(crouchInput, crouchEvent)
            && crouchInput.isKeyDown(Key::C) && !crouchInput.isKeyDown(Key::LeftControl),
        "C is a neutral remappable key, not the default crouch binding");

    event = {};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.scancode = SDL_SCANCODE_UNKNOWN;
    expect(!wkc::platform::handleSdlInputEvent(input, event),
        "unmapped SDL key is ignored");

    event = {};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.x = 320.0F;
    event.motion.y = 180.0F;
    event.motion.xrel = 3.0F;
    event.motion.yrel = -2.0F;
    expect(wkc::platform::handleSdlInputEvent(input, event),
        "SDL mouse motion is handled");
    expect(input.mousePosition() == wkc::core::Vec2{320.0F, 180.0F}
            && input.mouseDelta() == wkc::core::Vec2{3.0F, -2.0F},
        "SDL mouse motion maps position and delta");

    event = {};
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = 320.0F;
    event.button.y = 180.0F;
    expect(wkc::platform::handleSdlInputEvent(input, event),
        "SDL primary button is handled");
    expect(input.isMouseButtonDown(MouseButton::Primary)
            && input.wasMouseButtonPressed(MouseButton::Primary),
        "SDL left button maps to neutral primary button");

    input.beginFrame();
    event.type = SDL_EVENT_MOUSE_BUTTON_UP;
    expect(wkc::platform::handleSdlInputEvent(input, event),
        "SDL primary button release is handled");
    expect(!input.isMouseButtonDown(MouseButton::Primary)
            && input.wasMouseButtonReleased(MouseButton::Primary),
        "SDL primary release maps to the neutral transition");

    event = {};
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button = SDL_BUTTON_RIGHT;
    expect(wkc::platform::handleSdlInputEvent(input, event)
            && input.isMouseButtonDown(MouseButton::Secondary),
        "SDL right button is available for aiming");

    input.beginFrame();
    event = {};
    event.type = SDL_EVENT_MOUSE_WHEEL;
    event.wheel.y = 2.0F;
    event.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
    expect(wkc::platform::handleSdlInputEvent(input, event)
            && input.mouseWheelDelta() == 2.0F,
        "SDL normal mouse wheel preserves its direction");
    event.wheel.y = 1.0F;
    event.wheel.direction = SDL_MOUSEWHEEL_FLIPPED;
    expect(wkc::platform::handleSdlInputEvent(input, event)
            && input.mouseWheelDelta() == 1.0F,
        "SDL flipped mouse wheel is normalized before accumulation");
}

void testVirtualGamepad()
{
    using wkc::platform::GamepadButton;
    if (!SDL_Init(SDL_INIT_GAMEPAD | SDL_INIT_EVENTS)) {
        expect(false, "SDL gamepad subsystem initializes without a window");
        return;
    }
    SDL_VirtualJoystickDesc description;
    SDL_INIT_INTERFACE(&description);
    description.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    description.naxes = SDL_GAMEPAD_AXIS_COUNT;
    description.nbuttons = SDL_GAMEPAD_BUTTON_DPAD_RIGHT + 1;
    description.name = "GDP virtual gamepad test";
    const SDL_JoystickID id = SDL_AttachVirtualJoystick(&description);
    expect(id != 0 && SDL_IsGamepad(id),
        "SDL recognizes a connected virtual gamepad");
    if (id == 0) {
        SDL_Quit();
        return;
    }
    SDL_Joystick* const joystick = SDL_OpenJoystick(id);
    expect(joystick != nullptr, "virtual gamepad can receive simulated input");
    if (joystick != nullptr) {
        wkc::platform::Input input;
        wkc::platform::SdlGamepad gamepad;
        gamepad.refresh(id);
        expect(gamepad.connected(), "gamepad connected before launch is detected");
        input.beginFrame();
        expect(SDL_SetJoystickVirtualButton(
                joystick, SDL_GAMEPAD_BUTTON_SOUTH, true)
                && SDL_SetJoystickVirtualAxis(
                    joystick, SDL_GAMEPAD_AXIS_LEFTY, -32768)
                && SDL_SetJoystickVirtualAxis(
                    joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 32767),
            "virtual gamepad accepts button, stick and trigger updates");
        SDL_PumpEvents();
        gamepad.poll(input);
        expect(input.wasGamepadButtonPressed(GamepadButton::South)
                && input.wasGamepadButtonPressed(GamepadButton::RightTrigger)
                && input.gamepadMove().y < -0.9F,
            "SDL adapter translates gameplay controls from a virtual device");
        input.beginFrame();
        gamepad.poll(input);
        expect(input.isGamepadButtonDown(GamepadButton::South)
                && !input.wasGamepadButtonPressed(GamepadButton::South),
            "polling preserves held gamepad state without repeat presses");
        expect(SDL_SetJoystickVirtualButton(
                joystick, SDL_GAMEPAD_BUTTON_SOUTH, false),
            "virtual gamepad button can be released");
        SDL_PumpEvents();
        input.beginFrame();
        gamepad.poll(input);
        input.beginFrame();
        SDL_Event tap{};
        tap.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        tap.gbutton.which = id;
        tap.gbutton.button = SDL_GAMEPAD_BUTTON_SOUTH;
        gamepad.handleEvent(input, tap);
        tap.type = SDL_EVENT_GAMEPAD_BUTTON_UP;
        gamepad.handleEvent(input, tap);
        expect(input.wasGamepadButtonPressed(GamepadButton::South)
                && !input.isGamepadButtonDown(GamepadButton::South),
            "a short button tap survives even when released before the next poll");
        expect(SDL_DetachVirtualJoystick(id),
            "virtual gamepad can be disconnected during play");
        SDL_PumpEvents();
        SDL_Event disconnected{};
        disconnected.type = SDL_EVENT_GAMEPAD_REMOVED;
        disconnected.gdevice.which = id;
        gamepad.handleEvent(input, disconnected);
        expect(!input.isGamepadButtonDown(GamepadButton::South)
                && input.gamepadMove() == wkc::core::Vec2{},
            "hot unplug clears controls so the player cannot keep moving");
        gamepad.close(input);
        SDL_CloseJoystick(joystick);
    } else {
        (void)SDL_DetachVirtualJoystick(id);
    }
    SDL_Quit();
}

} // namespace

int main()
{
    testSdlTranslation();
    testVirtualGamepad();

    if (failures != 0) {
        std::cerr << failures << " SDL input adapter test(s) failed\n";
        return 1;
    }

    std::cout << "All SDL input adapter tests passed\n";
    return 0;
}
