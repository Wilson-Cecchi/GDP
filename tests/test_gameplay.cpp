#include "core/Math.hpp"
#include "gameplay/Collision.hpp"
#include "gameplay/Player.hpp"

#include <array>
#include <cmath>
#include <cstdint>
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

    player.applyDamage(35.0F);
    expect(wkc::core::nearlyEqual(player.state().health, 65.0F),
        "damage reduces player health");
    player.heal(100.0F);
    expect(wkc::core::nearlyEqual(player.state().health, 100.0F),
        "healing clamps to maximum health");
    player.applyDamage(500.0F);
    expect(!player.state().alive && wkc::core::nearlyEqual(player.state().health, 0.0F),
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
    testCollisionAndGravity();
    testLookAndCamera();
    testInteractionAndLife();

    if (failures != 0) {
        std::cerr << failures << " gameplay test(s) failed\n";
        return 1;
    }

    std::cout << "All GDP player tests passed\n";
    return 0;
}
