#include "core/Math.hpp"
#include "gameplay/Collision.hpp"
#include "gameplay/Player.hpp"
#include "gameplay/RoomTraversal.hpp"
#include "world/RoomModule.hpp"
#include "world/WorldGenerator.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <string_view>
#include <vector>

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

const gdp::world::RoomInstance* findRoom(
    const gdp::world::World& world,
    const gdp::world::RoomId id)
{
    for (const gdp::world::RoomInstance& room : world.rooms) {
        if (room.id == id) {
            return &room;
        }
    }
    return nullptr;
}

const gdp::world::ConnectorInstance* connectorForEdge(
    const gdp::world::RoomInstance& room,
    const gdp::world::EdgeId edge)
{
    for (const gdp::world::ConnectorInstance& connector : room.connectors) {
        if (connector.edge == edge) {
            return &connector;
        }
    }
    return nullptr;
}

bool moveToWaypoint(
    const gdp::gameplay::CollisionWorld& collision,
    gdp::core::Vec3& position,
    const gdp::core::Vec3 target)
{
    constexpr float fixedStep = 1.0F / 120.0F;
    constexpr float speed = 4.5F;
    constexpr int maximumSteps = 2400;
    gdp::core::Vec3 velocity{};
    for (int step = 0; step < maximumSteps; ++step) {
        gdp::core::Vec3 offset = target - position;
        offset.y = 0.0F;
        const float distance = gdp::core::length(offset);
        if (distance <= 0.06F) {
            return true;
        }
        velocity = offset * (speed / distance);
        const gdp::gameplay::CharacterMotionResult motion
            = collision.moveCharacter(position, velocity, fixedStep, {});
        if (gdp::core::nearlyEqual(position, motion.position, 1.0e-6F)) {
            return false;
        }
        position = motion.position;
    }
    return false;
}

void testEveryGeneratedConnectionTraversesBothWays()
{
    using namespace gdp;
    const world::WorldGenerationResult generated = world::generateWorld(0x5452415645525345ULL);
    expect(generated.succeeded(), "representative traversal world generates");
    if (!generated.world) {
        return;
    }

    for (const world::CorridorPath& corridor : generated.world->corridors) {
        const world::RoomId endpoints[2]{corridor.firstRoom, corridor.secondRoom};
        for (const world::RoomId sourceId : endpoints) {
            const world::RoomInstance* source = findRoom(*generated.world, sourceId);
            const world::ConnectorInstance* door
                = source == nullptr ? nullptr : connectorForEdge(*source, corridor.edge);
            expect(source != nullptr && door != nullptr,
                "corridor endpoint owns its assigned connector");
            if (source == nullptr || door == nullptr) {
                continue;
            }
            const core::Vec3 feet = door->worldPosition - (door->worldNormal * 0.55F);
            const auto transition = gameplay::findRoomTransition(
                *generated.world, sourceId, feet, door->worldNormal);
            const world::RoomId expectedDestination = sourceId == corridor.firstRoom
                ? corridor.secondRoom : corridor.firstRoom;
            expect(transition && transition->edge == corridor.edge
                    && transition->sourceRoom == sourceId
                    && transition->destinationRoom == expectedDestination,
                "assigned door resolves to the opposite corridor endpoint");
            const world::RoomInstance* destination
                = findRoom(*generated.world, expectedDestination);
            expect(transition && destination != nullptr
                    && destination->worldBounds.contains(transition->destinationPosition),
                "transition places the player inside the destination room");

            const auto facingAway = gameplay::findRoomTransition(
                *generated.world, sourceId, feet, door->worldNormal * -1.0F);
            expect(!facingAway, "door interaction requires the player to face the door");
        }
    }
}

void testRangeInvalidInputAndPlayerRelocation()
{
    using namespace gdp;
    const world::WorldGenerationResult generated = world::generateWorld(12345U);
    expect(generated.succeeded(), "seeded traversal world generates");
    if (!generated.world || generated.world->corridors.empty()) {
        return;
    }
    const world::CorridorPath& corridor = generated.world->corridors.front();
    const world::RoomInstance* source = findRoom(*generated.world, corridor.firstRoom);
    const world::ConnectorInstance* door
        = source == nullptr ? nullptr : connectorForEdge(*source, corridor.edge);
    if (source == nullptr || door == nullptr) {
        expect(false, "seeded traversal source connector exists");
        return;
    }
    expect(!gameplay::findRoomTransition(
            *generated.world, source->id,
            door->worldPosition - (door->worldNormal * 2.0F), door->worldNormal),
        "door interaction rejects a player beyond the activation range");
    expect(!gameplay::findRoomTransition(
            *generated.world, 0U, {}, {}, -1.0F),
        "invalid traversal input cannot produce a transition");

    gameplay::Player player;
    player.applyDamage(35.0F);
    player.addKeys(2U);
    (void)player.increaseMaximumHealth(15.0F, 10.0F);
    const float health = player.state().health;
    expect(player.relocate({14.0F, 0.0F, -7.0F}, 180.0F),
        "living player accepts a finite room relocation");
    expect(core::nearlyEqual(player.state().position, {14.0F, 0.0F, -7.0F})
            && core::nearlyEqual(std::abs(player.state().yawDegrees), 180.0F)
            && core::nearlyEqual(player.state().health, health)
            && core::nearlyEqual(player.state().maximumHealth, 115.0F)
            && player.state().keys == 2U,
        "relocation preserves health upgrades and inventory");
    const core::Vec3 stablePosition = player.state().position;
    expect(!player.relocate(
            {std::numeric_limits<float>::infinity(), 0.0F, 0.0F}, 0.0F)
            && player.state().position == stablePosition,
        "invalid relocation cannot corrupt player state");
}

void testRotatedRoomCollisionMatchesModuleBounds()
{
    using namespace gdp;
    const world::RoomModule module = world::makeFixedRoomModule();
    constexpr core::Vec3 origin{14.0F, 0.0F, -28.0F};
    for (const core::QuarterTurn rotation : {
             core::QuarterTurn::Degrees0,
             core::QuarterTurn::Degrees90,
             core::QuarterTurn::Degrees180,
             core::QuarterTurn::Degrees270}) {
        const auto room = world::instantiateRoom(
            module, 9U, world::RoomRole::Normal, 99U, origin, rotation);
        expect(room.has_value(), "fixed room instantiates for collision rotation test");
        if (!room) {
            continue;
        }
        const gameplay::CollisionWorld collision
            = gameplay::CollisionWorld::fixedRoom(origin, rotation);
        expect(collision.roomBounds() == room->worldBounds,
            "rotated collision bounds match the instantiated room bounds");
    }
}

void testGeneratedCorridorsSupportContinuousWalkingAndLockdown()
{
    using namespace gdp;
    const world::WorldGenerationResult generated
        = world::generateWorld(0x434F525249444F52ULL);
    expect(generated.succeeded(), "continuous corridor world generates");
    if (!generated.world) {
        return;
    }

    for (const world::CorridorPath& corridor : generated.world->corridors) {
        const world::RoomId endpoints[2]{corridor.firstRoom, corridor.secondRoom};
        for (const world::RoomId sourceId : endpoints) {
            const world::RoomId destinationId = sourceId == corridor.firstRoom
                ? corridor.secondRoom : corridor.firstRoom;
            const world::RoomInstance* source = findRoom(*generated.world, sourceId);
            const world::RoomInstance* destination
                = findRoom(*generated.world, destinationId);
            const world::ConnectorInstance* sourceDoor
                = source == nullptr ? nullptr : connectorForEdge(*source, corridor.edge);
            const world::ConnectorInstance* destinationDoor = destination == nullptr
                ? nullptr : connectorForEdge(*destination, corridor.edge);
            expect(source != nullptr && destination != nullptr
                    && sourceDoor != nullptr && destinationDoor != nullptr,
                "continuous route endpoints and doors exist");
            if (source == nullptr || destination == nullptr
                || sourceDoor == nullptr || destinationDoor == nullptr) {
                continue;
            }

            const gameplay::CollisionWorld navigation
                = gameplay::makeRoomNavigationCollision(
                    *generated.world, sourceId, false);
            std::vector<core::Vec3> waypoints = corridor.points;
            if (sourceId == corridor.secondRoom) {
                std::reverse(waypoints.begin(), waypoints.end());
            }
            waypoints.push_back(destinationDoor->worldPosition
                - (destinationDoor->worldNormal * 0.75F));

            core::Vec3 position = sourceDoor->worldPosition
                - (sourceDoor->worldNormal * 0.55F);
            bool completed = true;
            for (const core::Vec3 waypoint : waypoints) {
                if (!moveToWaypoint(navigation, position, waypoint)) {
                    completed = false;
                    break;
                }
            }
            expect(completed && destination->worldBounds.contains(position),
                "player walks continuously through each corridor in both directions");
            expect(gameplay::roomContainingPosition(*generated.world, position)
                    == std::optional<world::RoomId>{destinationId},
                "entering a destination room resolves its logical room id");

            const gameplay::CollisionWorld lockedNavigation
                = gameplay::makeRoomNavigationCollision(
                    *generated.world, sourceId, true);
            core::Vec3 lockedPosition = sourceDoor->worldPosition
                - (sourceDoor->worldNormal * 0.55F);
            for (int step = 0; step < 120; ++step) {
                const gameplay::CharacterMotionResult motion
                    = lockedNavigation.moveCharacter(
                        lockedPosition, sourceDoor->worldNormal * 4.5F,
                        1.0F / 120.0F, {});
                lockedPosition = motion.position;
            }
            expect(source->worldBounds.contains(lockedPosition)
                    && !destination->worldBounds.contains(lockedPosition),
                "lockdown collision prevents walking out through a corridor");
        }
    }
}

} // namespace

int main()
{
    testEveryGeneratedConnectionTraversesBothWays();
    testRangeInvalidInputAndPlayerRelocation();
    testRotatedRoomCollisionMatchesModuleBounds();
    testGeneratedCorridorsSupportContinuousWalkingAndLockdown();

    if (failures != 0) {
        std::cerr << failures << " room traversal test(s) failed\n";
        return 1;
    }
    std::cout << "Room traversal tests passed\n";
    return 0;
}
