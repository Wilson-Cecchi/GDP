#include "core/Math.hpp"
#include "gameplay/Collision.hpp"
#include "gameplay/Player.hpp"
#include "gameplay/RoomTraversal.hpp"
#include "world/RoomModule.hpp"
#include "world/WorldGenerator.hpp"

#include <cmath>
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

} // namespace

int main()
{
    testEveryGeneratedConnectionTraversesBothWays();
    testRangeInvalidInputAndPlayerRelocation();
    testRotatedRoomCollisionMatchesModuleBounds();

    if (failures != 0) {
        std::cerr << failures << " room traversal test(s) failed\n";
        return 1;
    }
    std::cout << "Room traversal tests passed\n";
    return 0;
}
