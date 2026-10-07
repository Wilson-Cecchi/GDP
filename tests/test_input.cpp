#include "platform/Input.hpp"

#include <iostream>
#include <limits>
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

void testInputState()
{
    using wkc::platform::Key;
    using wkc::platform::MouseButton;
    using wkc::platform::GamepadButton;

    wkc::platform::Input input;
    input.beginFrame();

    input.handleKey(Key::W, true);
    expect(input.isKeyDown(Key::W), "key-down state is captured");
    expect(input.wasKeyPressed(Key::W), "key press transition is captured");

    input.beginFrame();
    input.handleKey(Key::W, true, true);
    expect(input.isKeyDown(Key::W), "repeated key remains down");
    expect(!input.wasKeyPressed(Key::W),
        "key repeat does not create a press transition");

    input.beginFrame();
    input.handleKey(Key::W, false);
    expect(!input.isKeyDown(Key::W), "key-up clears held state");
    expect(input.wasKeyReleased(Key::W), "key release transition is captured");

    input.beginFrame();
    input.handleMouseMotion({120.0F, 90.0F}, {2.5F, -1.0F});
    input.handleMouseMotion({124.0F, 92.0F}, {1.5F, 3.0F});
    expect(input.mouseDelta() == wkc::core::Vec2{4.0F, 2.0F},
        "mouse motion accumulates during a frame");
    expect(input.mousePosition() == wkc::core::Vec2{124.0F, 92.0F},
        "mouse motion keeps the latest absolute position");

    input.handleMouseButton(MouseButton::Primary, true, {124.0F, 92.0F});
    expect(input.isMouseButtonDown(MouseButton::Primary),
        "mouse button held state is captured");
    expect(input.wasMouseButtonPressed(MouseButton::Primary),
        "mouse press transition is captured");

    input.beginFrame();
    expect(input.isMouseButtonDown(MouseButton::Primary),
        "beginFrame preserves held state");
    expect(!input.wasMouseButtonPressed(MouseButton::Primary),
        "beginFrame clears transitions");
    expect(input.mouseDelta() == wkc::core::Vec2{},
        "beginFrame clears mouse delta");
    expect(input.mousePosition() == wkc::core::Vec2{124.0F, 92.0F},
        "beginFrame preserves absolute mouse position");

    input.handleMouseButton(MouseButton::Primary, false, {124.0F, 92.0F});
    expect(input.wasMouseButtonReleased(MouseButton::Primary),
        "mouse release transition is captured");

    input.handleMouseWheel(1.0F);
    input.handleMouseWheel(-0.25F);
    input.handleMouseWheel(std::numeric_limits<float>::infinity());
    expect(input.mouseWheelDelta() == 0.75F,
        "mouse wheel movement accumulates finite input during a frame");
    input.beginFrame();
    expect(input.mouseWheelDelta() == 0.0F,
        "beginFrame clears mouse wheel movement");

    input.handleGamepadButton(GamepadButton::South, true);
    expect(input.isGamepadButtonDown(GamepadButton::South)
            && input.wasGamepadButtonPressed(GamepadButton::South),
        "gamepad button press has a one-frame transition");
    input.beginFrame();
    input.handleGamepadButton(GamepadButton::South, true);
    expect(input.isGamepadButtonDown(GamepadButton::South)
            && !input.wasGamepadButtonPressed(GamepadButton::South),
        "held gamepad button does not retrigger each frame");
    input.handleGamepadSticks({0.05F, 0.04F}, {0.0F, 0.0F});
    expect(input.gamepadMove() == wkc::core::Vec2{},
        "radial deadzone ignores stick drift");
    input.handleGamepadSticks({0.9F, 0.0F}, {0.0F, -1.0F});
    expect(input.gamepadMove().x > 0.8F
            && input.gamepadLook().y == -1.0F
            && input.wasGamepadButtonPressed(GamepadButton::StickRight),
        "analog sticks retain strength and drive menu navigation");
    input.resetGamepad();
    expect(!input.isGamepadButtonDown(GamepadButton::South)
            && input.gamepadMove() == wkc::core::Vec2{}
            && input.gamepadLook() == wkc::core::Vec2{},
        "disconnect clears held buttons and axes");
}

} // namespace

int main()
{
    testInputState();

    if (failures != 0) {
        std::cerr << failures << " input test(s) failed\n";
        return 1;
    }

    std::cout << "All platform-independent input tests passed\n";
    return 0;
}
