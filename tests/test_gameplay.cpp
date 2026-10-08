#include "core/Math.hpp"
#include "gameplay/Collision.hpp"
#include "gameplay/Footstep.hpp"
#include "gameplay/Player.hpp"
#include "gameplay/ViewMotion.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
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

float horizontalDistance(
    const wkc::core::Vec3 first,
    const wkc::core::Vec3 second) noexcept
{
    const float differenceX = first.x - second.x;
    const float differenceZ = first.z - second.z;
    return std::sqrt((differenceX * differenceX) + (differenceZ * differenceZ));
}

void updateMany(
    gdp::gameplay::Player& player,
    const gdp::gameplay::PlayerCommand command,
    const gdp::gameplay::CollisionWorld& world,
    const int updateCount)
{
    for (int update = 0; update < updateCount; ++update) {
        player.fixedUpdate(command, 1.0F / 60.0F, world);
    }
}

void testMovement()
{
    using namespace gdp::gameplay;

    const CollisionWorld world = CollisionWorld::fixedRoom();
    Player cardinal;
    Player diagonal;
    const auto cardinalStart = cardinal.state().position;
    const auto diagonalStart = diagonal.state().position;

    updateMany(cardinal, {.moveForward = 1.0F}, world, 15);
    updateMany(diagonal, {.moveForward = 1.0F, .moveRight = 1.0F}, world, 15);
    const float cardinalDistance = horizontalDistance(cardinal.state().position, cardinalStart);
    const float diagonalDistance = horizontalDistance(diagonal.state().position, diagonalStart);
    expect(wkc::core::nearlyEqual(cardinalDistance, diagonalDistance, 1.0e-4F),
        "diagonal movement is normalized");
    expect(wkc::core::nearlyEqual(cardinalDistance, 1.125F, 1.0e-4F),
        "movement distance follows speed and fixed time");

    Player firstRun;
    Player secondRun;
    updateMany(firstRun, {.moveRight = -1.0F}, world, 24);
    updateMany(secondRun, {.moveRight = -1.0F}, world, 24);
    expect(firstRun.state().position == secondRun.state().position,
        "fixed movement is deterministic");

    Player quiet;
    const auto quietStart = quiet.state().position;
    updateMany(quiet, {.moveForward = 1.0F, .quietWalk = true}, world, 15);
    expect(wkc::core::nearlyEqual(
            horizontalDistance(quiet.state().position, quietStart),
            cardinalDistance * 0.52F, 1.0e-4F),
        "quiet walk reduces grounded movement without changing direction");
}

void testGamepadLook()
{
    using gdp::gameplay::gamepadLookDegrees;
    const auto normal = gamepadLookDegrees({1.0F, -1.0F}, 1.0F / 60.0F, 1.0F, 1.0F);
    expect(wkc::core::nearlyEqual(normal.x, 5.0F, 1.0e-4F)
            && wkc::core::nearlyEqual(normal.y, -5.0F, 1.0e-4F),
        "full gamepad stick turns at 300 degrees per second by default");
    const auto aimed = gamepadLookDegrees({1.0F, 0.5F}, 1.0F / 60.0F, 0.7F, 1.4F);
    expect(wkc::core::nearlyEqual(aimed.x, 3.5F, 1.0e-4F)
            && wkc::core::nearlyEqual(aimed.y, 3.5F, 1.0e-4F),
        "aim and independent axis sensitivity scale the turn");
    const auto capped = gamepadLookDegrees({1.0F, 0.0F}, 1.0F, 1.0F, 1.0F);
    expect(wkc::core::nearlyEqual(capped.x, 15.0F, 1.0e-4F),
        "a long frame cannot cause a sudden camera spin");
}

void testBunnyHopMovement()
{
    using namespace gdp::gameplay;
    constexpr float step = 1.0F / 60.0F;
    const CollisionWorld openRoom{
        {{-50.0F, 0.0F, -50.0F}, {50.0F, 5.0F, 50.0F}}};
    const auto speed = [](const Player& player) {
        return std::hypot(
            player.state().velocity.x, player.state().velocity.z);
    };

    Player diagonalLaunch;
    diagonalLaunch.spawn({0.0F, 0.0F, 0.0F});
    diagonalLaunch.fixedUpdate({.moveForward = 1.0F,
        .moveRight = 1.0F, .jump = true}, step, openRoom);
    expect(wkc::core::nearlyEqual(speed(diagonalLaunch), 4.5F, 1.0e-4F),
        "diagonal jump starts at normal walking speed, not a diagonal boost");

    Player straightJump;
    straightJump.spawn({0.0F, 0.0F, 0.0F});
    straightJump.fixedUpdate({.moveForward = 1.0F, .jump = true},
        step, openRoom);
    updateMany(straightJump, {.moveForward = 1.0F}, openRoom, 25);
    expect(wkc::core::nearlyEqual(speed(straightJump), 4.5F, 1.0e-4F),
        "holding forward in the air does not generate free bunny-hop speed");

    Player hopper;
    hopper.spawn({0.0F, 0.0F, 0.0F});
    hopper.fixedUpdate({.moveForward = 1.0F, .jump = true}, step, openRoom);
    for (int tick = 0; tick < 18; ++tick) {
        hopper.fixedUpdate({.moveRight = 1.0F}, step, openRoom);
        expect(speed(hopper) <= 7.2F + 1.0e-4F,
            "air strafing never exceeds the bunny-hop speed cap");
    }
    expect(speed(hopper) > 5.0F && !hopper.state().grounded,
        "air strafing builds useful speed beyond ordinary walking");

    bool buffered = false;
    bool chained = false;
    for (int tick = 0; tick < 90 && !chained; ++tick) {
        const bool pressJump = !buffered
            && hopper.state().velocity.y < 0.0F
            && hopper.state().position.y < 0.30F;
        hopper.fixedUpdate(
            {.moveRight = 1.0F, .jump = pressJump}, step, openRoom);
        buffered = buffered || pressJump;
        chained = buffered && hopper.state().velocity.y > 0.0F
            && !hopper.state().grounded;
    }
    expect(buffered && chained && speed(hopper) > 4.5F,
        "a jump pressed shortly before landing chains without erasing momentum");
    updateMany(hopper, {.moveForward = 1.0F}, openRoom, 120);
    expect(hopper.state().grounded
            && wkc::core::nearlyEqual(speed(hopper), 4.5F, 1.0e-4F),
        "landing without another jump returns to ordinary walking speed");

    Player earlyRequest;
    earlyRequest.spawn({0.0F, 0.0F, 0.0F});
    earlyRequest.fixedUpdate({.jump = true}, step, openRoom);
    updateMany(earlyRequest, {}, openRoom, 10);
    earlyRequest.fixedUpdate({.jump = true}, step, openRoom);
    updateMany(earlyRequest, {}, openRoom, 120);
    expect(earlyRequest.state().grounded,
        "an early airborne jump request expires before landing");

    Player quiet;
    quiet.spawn({0.0F, 0.0F, 0.0F});
    quiet.fixedUpdate({.moveForward = 1.0F,
        .quietWalk = true, .jump = true}, step, openRoom);
    updateMany(quiet, {.moveRight = 1.0F, .quietWalk = true}, openRoom, 18);
    expect(speed(quiet) <= 4.5F * 0.52F + 1.0e-4F,
        "quiet walking cannot retain a fast air-strafe speed");
}

void testFootstepCadence()
{
    using namespace gdp::gameplay;
    FootstepState state;
    wkc::core::Vec3 previous{};
    std::optional<std::uint64_t> emitted;
    for (int segment = 0; segment < 4; ++segment) {
        const wkc::core::Vec3 current
            = previous + wkc::core::Vec3{0.40F, 0.0F, 0.0F};
        emitted = updateFootsteps(state, previous, current, true, true);
        previous = current;
    }
    expect(emitted && *emitted == 0U
            && wkc::core::nearlyEqual(state.accumulatedDistance, 0.0F),
        "grounded horizontal travel emits one step at the configured stride");

    const auto second = updateFootsteps(
        state, previous, previous + wkc::core::Vec3{0.40F, 0.0F, 0.0F}, true, true,
        {.stepDistance = 0.40F, .maximumSegmentDistance = 0.30F});
    expect(!second && state.nextEventIndex == 1U,
        "segments above the safety limit do not create teleport footsteps");

    state.accumulatedDistance = 1.2F;
    expect(!updateFootsteps(state, previous, previous, false, true)
            && state.accumulatedDistance == 0.0F,
        "airborne movement clears cadence and emits no footsteps");
    expect(!updateFootsteps(state, previous, previous, true, false)
            && state.accumulatedDistance == 0.0F,
        "dead or stationary players emit no footsteps");

    state.accumulatedDistance = std::numeric_limits<float>::quiet_NaN();
    expect(!updateFootsteps(state, previous, previous, true, true)
            && state.accumulatedDistance == 0.0F,
        "invalid accumulated distance recovers without emitting audio");

    resetFootsteps(state);
    expect(state.accumulatedDistance == 0.0F && state.nextEventIndex == 0U,
        "run reset restores the deterministic footstep sequence");

    Player walker;
    const CollisionWorld openRoom{
        {{-10.0F, 0.0F, -10.0F}, {10.0F, 4.0F, 10.0F}}};
    std::uint32_t stepCount = 0U;
    for (int update = 0; update < 60; ++update) {
        const wkc::core::Vec3 before = walker.state().position;
        walker.fixedUpdate({.moveRight = 1.0F}, 1.0F / 60.0F, openRoom);
        if (updateFootsteps(state, before, walker.state().position,
                walker.state().grounded, walker.state().alive)) {
            ++stepCount;
        }
    }
    expect(stepCount == 2U && state.nextEventIndex == 2U,
        "one second of real Player movement emits distance-based cadence");
}

void testCosmeticViewMotion()
{
    using namespace gdp::gameplay;
    ViewMotionState first;
    ViewMotionState repeated;
    wkc::core::Vec3 previous{};
    for (int update = 0; update < 30; ++update) {
        const wkc::core::Vec3 current
            = previous + wkc::core::Vec3{0.075F, 0.0F, 0.0F};
        updateViewMotion(
            first, previous, current, true, true, 1.0F / 60.0F);
        updateViewMotion(
            repeated, previous, current, true, true, 1.0F / 60.0F);
        previous = current;
    }
    const wkc::core::Vec3 offset
        = viewMotionOffset(first, {0.0F, 0.0F, 1.0F});
    expect(first.weight == 1.0F && first.phaseRadians == repeated.phaseRadians
            && offset == viewMotionOffset(repeated, {0.0F, 0.0F, 1.0F}),
        "actual grounded travel produces deterministic cosmetic view motion");
    expect(std::abs(offset.y) <= 0.0351F
            && std::sqrt((offset.x * offset.x) + (offset.z * offset.z)) <= 0.0181F,
        "view motion remains inside its comfort amplitudes");
    Player logicalCameraOwner;
    const wkc::core::Vec3 logicalCamera = logicalCameraOwner.cameraPosition();
    const wkc::core::Vec3 presentationCamera = logicalCamera + offset;
    expect(logicalCameraOwner.cameraPosition() == logicalCamera
            && presentationCamera != logicalCamera,
        "presentation offset leaves the Player camera used by gameplay unchanged");

    const float walkingPhase = first.phaseRadians;
    for (int update = 0; update < 12; ++update) {
        updateViewMotion(first, previous, previous, true, true, 1.0F / 60.0F);
    }
    expect(first.phaseRadians == walkingPhase && first.weight == 0.0F
            && viewMotionOffset(first, {1.0F, 0.0F, 0.0F}) == wkc::core::Vec3{},
        "stopping fades the render offset without advancing gait phase");

    first = {.phaseRadians = 1.0F, .weight = 1.0F};
    updateViewMotion(first, previous,
        previous + wkc::core::Vec3{1.0F, 0.0F, 0.0F},
        true, true, 1.0F / 60.0F);
    expect(first.phaseRadians == 0.0F && first.weight == 0.0F,
        "teleport-sized segments reset view motion instead of jolting the camera");

    first = {.phaseRadians = 1.0F, .weight = 1.0F};
    updateViewMotion(first, previous, previous, false, true, 1.0F / 60.0F);
    expect(first.phaseRadians == 1.0F && first.weight < 1.0F,
        "airborne state fades bob while preserving a continuous gait phase");
    resetViewMotion(first);
    expect(first.phaseRadians == 0.0F && first.weight == 0.0F,
        "run reset clears all cosmetic view motion");
}

void testCollisionAndGravity()
{
    using namespace gdp::gameplay;

    const CollisionWorld world = CollisionWorld::fixedRoom();
    Player terminalCollision;
    updateMany(terminalCollision, {.moveForward = 1.0F}, world, 180);
    expect(terminalCollision.state().position.z >= 2.64F,
        "player cannot pass through the terminal");

    Player wallCollision;
    updateMany(wallCollision, {.moveRight = 1.0F}, world, 180);
    expect(wallCollision.state().position.x <= 3.551F,
        "player radius remains inside the room wall");

    const CollisionWorld emptyRoom{{{-3.0F, 0.0F, -3.0F}, {3.0F, 4.0F, 3.0F}}};
    Player corner;
    corner.spawn({0.0F, 0.0F, 0.0F}, 0.0F);
    updateMany(corner, {.moveForward = 1.0F, .moveRight = 1.0F}, emptyRoom, 180);
    expect(corner.state().position.x <= 2.701F && corner.state().position.z <= 2.701F,
        "corner collision constrains both horizontal axes");

    Player falling;
    falling.spawn({0.0F, 2.0F, 0.0F});
    updateMany(falling, {}, emptyRoom, 120);
    expect(wkc::core::nearlyEqual(falling.state().position.y, 0.0F),
        "gravity settles the player on the floor");
    expect(falling.state().grounded, "floor collision marks the player grounded");
}

void testCrouchAndJumpMovement()
{
    using namespace gdp::gameplay;

    const CollisionWorld openRoom{
        {{-10.0F, 0.0F, -10.0F}, {10.0F, 4.0F, 10.0F}}};
    Player crouched;
    crouched.spawn({0.0F, 0.0F, 0.0F});
    const float standingEye = crouched.cameraPosition().y;
    updateMany(crouched, {.crouch = true}, openRoom, 8);
    expect(crouched.state().crouching
            && wkc::core::nearlyEqual(crouched.state().crouchAmount, 1.0F)
            && wkc::core::nearlyEqual(crouched.collider().height, 1.15F)
            && wkc::core::nearlyEqual(crouched.cameraPosition().y, 1.02F)
            && crouched.cameraPosition().y < standingEye,
        "crouch smoothly lowers the camera and authoritatively shortens the collider");
    crouched.fixedUpdate(
        {.crouch = true, .jump = true}, 1.0F / 60.0F, openRoom);
    expect(crouched.state().grounded
            && wkc::core::nearlyEqual(crouched.state().position.y, 0.0F),
        "crouched player cannot launch a jump into low cover");

    Player standingTravel;
    Player crouchedTravel;
    standingTravel.spawn({0.0F, 0.0F, 0.0F});
    crouchedTravel.spawn({0.0F, 0.0F, 0.0F});
    updateMany(standingTravel, {.moveRight = 1.0F}, openRoom, 60);
    updateMany(crouchedTravel,
        {.moveRight = 1.0F, .crouch = true}, openRoom, 60);
    expect(wkc::core::nearlyEqual(standingTravel.state().position.x, 4.5F, 1.0e-4F)
            && wkc::core::nearlyEqual(
                crouchedTravel.state().position.x, 2.61F, 1.0e-4F),
        "crouched travel uses its slower deterministic movement speed");

    const CollisionWorld lowClearance(
        {{-4.0F, 0.0F, -4.0F}, {4.0F, 4.0F, 4.0F}},
        {{{-0.5F, 1.20F, -0.5F}, {0.5F, 1.45F, 0.5F}}});
    Player approaching;
    approaching.spawn({-1.0F, 0.0F, 0.0F});
    for (int update = 0; update < 20; ++update) {
        approaching.fixedUpdate(
            {.moveRight = 1.0F, .crouch = true},
            1.0F / 60.0F, lowClearance);
        expect(approaching.cameraPosition().y
                < approaching.state().position.y
                    + approaching.collider().height,
            "camera stays below the interpolating crouch collider");
    }
    expect(approaching.state().position.x > -0.5F
            && approaching.state().crouching,
        "crouch movement passes beneath low cover after clearance opens");
    Player clearance;
    clearance.spawn({0.0F, 0.0F, 0.0F});
    updateMany(clearance, {.crouch = true}, lowClearance, 8);
    updateMany(clearance, {}, lowClearance, 8);
    expect(clearance.state().crouching
            && wkc::core::nearlyEqual(clearance.collider().height, 1.15F),
        "releasing crouch under an obstruction cannot expand the collider into it");
    updateMany(clearance, {.moveRight = 1.0F}, lowClearance, 30);
    expect(!clearance.state().crouching
            && wkc::core::nearlyEqual(clearance.collider().height, 1.8F)
            && clearance.state().position.x > 0.8F,
        "player stands automatically after leaving low clearance");

    Player jumping;
    jumping.spawn({0.0F, 0.0F, 0.0F});
    jumping.fixedUpdate({.jump = true}, 1.0F / 60.0F, openRoom);
    float peak = jumping.state().position.y;
    expect(!jumping.state().grounded && jumping.state().velocity.y > 0.0F,
        "jump leaves the floor with an upward fixed-step impulse");
    const float firstJumpVelocity = jumping.state().velocity.y;
    jumping.fixedUpdate({.jump = true}, 1.0F / 60.0F, openRoom);
    expect(jumping.state().velocity.y < firstJumpVelocity,
        "a second airborne jump request cannot reset upward velocity");
    for (int update = 0; update < 120; ++update) {
        jumping.fixedUpdate({}, 1.0F / 60.0F, openRoom);
        peak = std::max(peak, jumping.state().position.y);
    }
    expect(peak > 0.85F && peak < 1.05F
            && jumping.state().grounded
            && wkc::core::nearlyEqual(jumping.state().position.y, 0.0F),
        "jump follows a bounded ballistic arc and settles back on the floor");

    const CollisionWorld lowCeiling(
        {{-4.0F, 0.0F, -4.0F}, {4.0F, 4.0F, 4.0F}},
        {{{-1.0F, 2.0F, -1.0F}, {1.0F, 2.25F, 1.0F}}});
    Player headBlocked;
    headBlocked.spawn({0.0F, 0.0F, 0.0F});
    headBlocked.fixedUpdate({.jump = true}, 1.0F / 60.0F, lowCeiling);
    float blockedPeak = headBlocked.state().position.y;
    for (int update = 0; update < 45; ++update) {
        headBlocked.fixedUpdate({}, 1.0F / 60.0F, lowCeiling);
        blockedPeak = std::max(blockedPeak, headBlocked.state().position.y);
    }
    expect(blockedPeak <= 0.2001F && headBlocked.state().grounded,
        "jumping into a low ceiling stops the head before returning to ground");

    const CollisionWorld platformRoom(
        {{-4.0F, 0.0F, -4.0F}, {4.0F, 4.0F, 4.0F}},
        {{{-1.0F, 0.0F, -1.0F}, {1.0F, 0.70F, 1.0F}}});
    Player landing;
    landing.spawn({0.0F, 2.0F, 0.0F});
    updateMany(landing, {}, platformRoom, 120);
    expect(landing.state().grounded
            && wkc::core::nearlyEqual(landing.state().position.y, 0.70F),
        "falling onto solid geometry lands on its upper surface");
}

void testLookAndCamera()
{
    using namespace gdp::gameplay;

    Player player;
    player.applyLook({750.0F, -10'000.0F});
    expect(wkc::core::nearlyEqual(player.state().pitchDegrees, 89.0F),
        "pitch is clamped at the upper limit");
    expect(player.state().yawDegrees >= -180.0F && player.state().yawDegrees <= 180.0F,
        "yaw remains in a stable wrapped range");
    expect(wkc::core::nearlyEqual(wkc::core::length(player.forwardDirection()), 1.0F),
        "camera forward direction is normalized");

    Player once;
    Player manyUpdates;
    once.applyLook({20.0F, 10.0F});
    manyUpdates.applyLook({20.0F, 10.0F});
    updateMany(manyUpdates, {}, CollisionWorld::fixedRoom(), 10);
    expect(wkc::core::nearlyEqual(once.state().yawDegrees, manyUpdates.state().yawDegrees),
        "fixed updates do not reapply mouse delta");
    expect(wkc::core::nearlyEqual(once.state().pitchDegrees, manyUpdates.state().pitchDegrees),
        "mouse sensitivity is independent of fixed-update count");

    Player configured;
    configured.setMouseSensitivity(0.24F);
    configured.applyLook({10.0F, 0.0F});
    expect(wkc::core::nearlyEqual(configured.mouseSensitivity(), 0.24F)
            && wkc::core::nearlyEqual(configured.state().yawDegrees, -87.6F),
        "runtime sensitivity immediately changes frame-driven look");
    configured.setMouseSensitivity(9.0F);
    expect(wkc::core::nearlyEqual(configured.mouseSensitivity(), 0.24F),
        "runtime sensitivity clamps at its documented maximum");

    Player padA;
    Player padB;
    padB.setMouseSensitivity(0.24F);
    padB.setInvertMouseY(true);
    padA.applyGamepadLook({15.0F, 10.0F}, false);
    padB.applyGamepadLook({15.0F, 10.0F}, false);
    expect(wkc::core::nearlyEqual(padA.state().yawDegrees, padB.state().yawDegrees)
            && wkc::core::nearlyEqual(padA.state().pitchDegrees, padB.state().pitchDegrees),
        "controller look is independent of mouse sensitivity and inversion");
    padB.applyGamepadLook({0.0F, 10.0F}, true);
    expect(wkc::core::nearlyEqual(padB.state().pitchDegrees, 0.0F),
        "controller Y inversion reverses the vertical direction");

    Player regularVertical;
    Player invertedVertical;
    invertedVertical.setInvertMouseY(true);
    regularVertical.applyLook({0.0F, 10.0F});
    invertedVertical.applyLook({0.0F, 10.0F});
    expect(!regularVertical.invertMouseY() && invertedVertical.invertMouseY()
            && regularVertical.state().pitchDegrees < 0.0F
            && invertedVertical.state().pitchDegrees > 0.0F
            && wkc::core::nearlyEqual(
                regularVertical.state().pitchDegrees,
                -invertedVertical.state().pitchDegrees),
        "invert Y reverses only the vertical look direction");
}

void testInteractionAndLife()
{
    using namespace gdp::gameplay;

    Player player;
    const gdp::entities::EntityId playerEntity{5, 2};
    player.bindEntity(playerEntity);
    expect(player.entity() == playerEntity, "player retains its manager entity handle");
    constexpr std::array targets{
        InteractionTarget{{11, 1}, {{-0.4F, 0.0F, 2.05F}, {0.4F, 2.0F, 2.35F}}},
        InteractionTarget{{22, 1}, {{2.0F, 0.0F, 2.0F}, {2.5F, 2.0F, 2.5F}}},
    };
    expect(player.findInteraction(targets) == std::optional<gdp::entities::EntityId>{{11, 1}},
        "interaction ray selects the visible in-range target");
    expect(!player.findInteraction(targets, 1.0F),
        "interaction ray rejects targets beyond its range");
    constexpr std::array ownSolid{targets[0].bounds};
    expect(player.findInteraction(targets, 2.25F, ownSolid) == targets[0].id,
        "interaction allows the target's own solid surface");
    constexpr std::array blockingCover{
        wkc::core::Aabb{{-0.5F, 0.0F, 2.8F}, {0.5F, 2.0F, 3.0F}}};
    expect(!player.findInteraction(targets, 2.25F, blockingCover),
        "a covered exit terminal cannot be activated through a wall or prop");

    player.applyDamage(35.0F);
    expect(wkc::core::nearlyEqual(player.state().health, 65.0F),
        "damage reduces player health");
    player.heal(100.0F);
    expect(wkc::core::nearlyEqual(player.state().health, 100.0F),
        "healing clamps to maximum health");
    player.addArmor(30.0F);
    player.applyDamage(20.0F);
    expect(wkc::core::nearlyEqual(player.state().armor, 10.0F)
            && wkc::core::nearlyEqual(player.state().health, 100.0F),
        "armor is consumed before damage reaches health");
    player.applyDamage(25.0F);
    expect(wkc::core::nearlyEqual(player.state().armor, 0.0F)
            && wkc::core::nearlyEqual(player.state().health, 85.0F),
        "damage overflow consumes remaining armor and reaches health");
    player.heal(100.0F);
    player.addArmor(500.0F);
    expect(wkc::core::nearlyEqual(player.state().armor, 50.0F),
        "armor pickups clamp to maximum armor");
    player.applyDamage(500.0F);
    expect(!player.state().alive && wkc::core::nearlyEqual(player.state().health, 0.0F)
            && wkc::core::nearlyEqual(player.state().armor, 0.0F),
        "lethal damage changes alive state once");
    player.applyDamage(10.0F);
    player.heal(10.0F);
    expect(wkc::core::nearlyEqual(player.state().health, 0.0F),
        "dead player ignores repeated damage and healing");

    const auto deadPosition = player.state().position;
    updateMany(player, {.moveForward = 1.0F}, CollisionWorld::fixedRoom(), 60);
    expect(player.state().position == deadPosition, "dead player cannot move");
}

} // namespace

int main()
{
    testMovement();
    testGamepadLook();
    testBunnyHopMovement();
    testFootstepCadence();
    testCosmeticViewMotion();
    testCollisionAndGravity();
    testCrouchAndJumpMovement();
    testLookAndCamera();
    testInteractionAndLife();

    if (failures != 0) {
        std::cerr << failures << " gameplay test(s) failed\n";
        return 1;
    }

    std::cout << "All GDP player tests passed\n";
    return 0;
}
