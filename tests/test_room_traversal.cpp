#include "core/Math.hpp"
#include "gameplay/Collision.hpp"
#include "gameplay/Combat.hpp"
#include "gameplay/Door.hpp"
#include "gameplay/Enemy.hpp"
#include "gameplay/Player.hpp"
#include "gameplay/RoomTraversal.hpp"
#include "gameplay/Session.hpp"
#include "world/RoomModule.hpp"
#include "world/WorldGenerator.hpp"

#include <algorithm>
#include <array>
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
    wkc::core::Vec3& position,
    const wkc::core::Vec3 target,
    const gdp::gameplay::CharacterCollider collider = {})
{
    constexpr float fixedStep = 1.0F / 120.0F;
    constexpr float speed = 4.5F;
    constexpr int maximumSteps = 2400;
    wkc::core::Vec3 velocity{};
    for (int step = 0; step < maximumSteps; ++step) {
        wkc::core::Vec3 offset = target - position;
        offset.y = 0.0F;
        const float distance = wkc::core::length(offset);
        if (distance <= 0.06F) {
            return true;
        }
        velocity = offset * (speed / distance);
        const gdp::gameplay::CharacterMotionResult motion
            = collision.moveCharacter(position, velocity, fixedStep, collider);
        if (wkc::core::nearlyEqual(position, motion.position, 1.0e-6F)) {
            return false;
        }
        position = motion.position;
    }
    return false;
}

bool everyCorridorTraversesBothWays(
    const gdp::world::World& world,
    const gdp::gameplay::CharacterCollider collider = {})
{
    using namespace gdp;
    for (const world::CorridorPath& corridor : world.corridors) {
        const world::RoomId endpoints[2]{corridor.firstRoom, corridor.secondRoom};
        for (const world::RoomId sourceId : endpoints) {
            const world::RoomId destinationId = sourceId == corridor.firstRoom
                ? corridor.secondRoom : corridor.firstRoom;
            const world::RoomInstance* source = findRoom(world, sourceId);
            const world::RoomInstance* destination = findRoom(world, destinationId);
            const world::ConnectorInstance* sourceDoor
                = source == nullptr ? nullptr : connectorForEdge(*source, corridor.edge);
            const world::ConnectorInstance* destinationDoor = destination == nullptr
                ? nullptr : connectorForEdge(*destination, corridor.edge);
            if (source == nullptr || destination == nullptr
                || sourceDoor == nullptr || destinationDoor == nullptr) {
                return false;
            }

            const gameplay::CollisionWorld navigation
                = gameplay::makeRoomNavigationCollision(world, sourceId, false);
            std::vector<wkc::core::Vec3> waypoints = corridor.points;
            if (sourceId == corridor.secondRoom) {
                std::reverse(waypoints.begin(), waypoints.end());
            }
            waypoints.push_back(destinationDoor->worldPosition
                - (destinationDoor->worldNormal * 0.75F));
            wkc::core::Vec3 position = sourceDoor->worldPosition
                - (sourceDoor->worldNormal * 0.55F);
            for (const wkc::core::Vec3 waypoint : waypoints) {
                if (!moveToWaypoint(navigation, position, waypoint, collider)) {
                    return false;
                }
            }
            if (!destination->worldBounds.contains(position)) {
                return false;
            }
        }
    }
    return true;
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
            const wkc::core::Vec3 feet = door->worldPosition - (door->worldNormal * 0.55F);
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
    (void)player.increaseMaximumArmor(10.0F, 25.0F);
    const float health = player.state().health;
    expect(player.relocate({14.0F, 0.0F, -7.0F}, 180.0F),
        "living player accepts a finite room relocation");
    expect(wkc::core::nearlyEqual(player.state().position, {14.0F, 0.0F, -7.0F})
            && wkc::core::nearlyEqual(std::abs(player.state().yawDegrees), 180.0F)
            && wkc::core::nearlyEqual(player.state().health, health)
            && wkc::core::nearlyEqual(player.state().maximumHealth, 115.0F)
            && wkc::core::nearlyEqual(player.state().armor, 25.0F)
            && wkc::core::nearlyEqual(player.state().maximumArmor, 60.0F)
            && player.state().keys == 2U,
        "relocation preserves health, armor upgrades and inventory");
    const wkc::core::Vec3 stablePosition = player.state().position;
    expect(!player.relocate(
            {std::numeric_limits<float>::infinity(), 0.0F, 0.0F}, 0.0F)
            && player.state().position == stablePosition,
        "invalid relocation cannot corrupt player state");
}

void testCharacterRoomAdoptionRequiresFullClearance()
{
    using namespace gdp;
    const auto room = world::instantiateRoom(
        world::makeFixedRoomModule(), 1U, world::RoomRole::Normal,
        99U, {}, wkc::core::QuarterTurn::Degrees0);
    expect(room.has_value(), "fixed room instantiates for adoption clearance test");
    if (!room) {
        return;
    }
    world::World generated;
    generated.rooms.push_back(*room);
    const wkc::core::Vec3 thresholdPosition{0.0F, 0.0F, 4.75F};
    const wkc::core::Vec3 safePosition{0.0F, 0.0F, 4.50F};
    expect(gameplay::roomContainingPosition(generated, thresholdPosition) == 1U
            && !gameplay::roomContainingCharacter(generated, thresholdPosition),
        "a point across the threshold does not adopt the room before collider clearance");
    expect(gameplay::roomContainingCharacter(generated, safePosition) == 1U,
        "a fully cleared character collider adopts the destination room");
    expect(!gameplay::roomContainingCharacter(
            generated, safePosition, {.radius = -1.0F, .height = 1.8F}),
        "invalid character collider cannot adopt a room");
}

void testRotatedRoomCollisionMatchesModuleBounds()
{
    using namespace gdp;
    const world::RoomModule module = world::makeFixedRoomModule();
    constexpr wkc::core::Vec3 origin{14.0F, 0.0F, -28.0F};
    for (const wkc::core::QuarterTurn rotation : {
             wkc::core::QuarterTurn::Degrees0,
             wkc::core::QuarterTurn::Degrees90,
             wkc::core::QuarterTurn::Degrees180,
             wkc::core::QuarterTurn::Degrees270}) {
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

void testDoorRuntimeOpensHoldsClosesAndLocks()
{
    using namespace gdp;
    gameplay::DoorRuntime door = gameplay::makeDoorRuntime(17U);
    expect(!gameplay::isDoorPassable(door)
            && door.state == world::DoorState::Closed,
        "new corridor door starts closed and blocks traversal");
    expect(gameplay::requestDoorOpen(door, false)
            && door.state == world::DoorState::Opening,
        "door interaction starts physical opening without relocation");
    (void)gameplay::updateDoor(door, 0.20F, false, true);
    expect(door.openness > 0.0F && door.openness < 1.0F
            && !gameplay::isDoorPassable(door),
        "partially opened door remains a physical blocker");
    (void)gameplay::updateDoor(door, 0.30F, false, true);
    expect(door.state == world::DoorState::Open
            && gameplay::isDoorPassable(door),
        "fully opened door releases its corridor collision");
    (void)gameplay::updateDoor(door, 5.0F, false, true);
    expect(door.state == world::DoorState::Open,
        "occupied door cannot close on a player in its threshold or corridor");
    const gameplay::DoorUpdate closing
        = gameplay::updateDoor(door, 2.0F, false, false);
    expect(door.state == world::DoorState::Closing && closing.startedClosing
            && !closing.fullyClosed,
        "unoccupied door starts closing after its hold interval");
    expect(gameplay::requestDoorOpen(door, false),
        "interaction reverses a closing door safely");
    (void)gameplay::updateDoor(door, 1.0F, false, false);
    expect(door.state == world::DoorState::Open,
        "reopened door returns to its fully open state");
    (void)gameplay::updateDoor(door, 1.0F, true, true);
    expect(door.state == world::DoorState::Open
            && gameplay::isDoorPassable(door),
        "lockdown holds an occupied passable door open until the player clears it");
    const gameplay::DoorUpdate locking
        = gameplay::updateDoor(door, 1.0F, true, false);
    expect(door.state == world::DoorState::Locked
            && locking.startedClosing && locking.fullyClosed
            && !gameplay::isDoorPassable(door)
            && !gameplay::requestDoorOpen(door, true),
        "lockdown reports close and lock transitions then rejects interaction");
}

void testDoorPanelTravelMatchesPassability()
{
    using namespace gdp;
    expect(gameplay::doorPanelTravel(-1.0F) == 0.0F
            && gameplay::doorPanelTravel(0.0F) == 0.0F
            && gameplay::doorPanelTravel(0.5F) == 0.5F
            && gameplay::doorPanelTravel(1.0F) == 1.0F
            && gameplay::doorPanelTravel(2.0F) == 1.0F,
        "door panel easing clamps and preserves exact endpoints");
    expect(gameplay::doorPanelTravel(0.25F) < 0.25F
            && gameplay::doorPanelTravel(0.75F) > 0.75F
            && gameplay::doorPanelTravel(
                std::numeric_limits<float>::quiet_NaN()) == 0.0F,
        "door panel easing accelerates smoothly and sanitizes invalid progress");

    gameplay::DoorRuntime door = gameplay::makeDoorRuntime(31U);
    door.state = world::DoorState::Opening;
    door.openness = 0.80F;
    expect(!gameplay::isDoorPassable(door),
        "collision remains closed before the visible aperture reaches ninety percent");
    door.openness = 0.81F;
    expect(gameplay::isDoorPassable(door),
        "collision releases when the same eased aperture visibly reaches ninety percent");

    door.state = world::DoorState::Closing;
    door.openness = 0.70F;
    const gameplay::DoorUpdate reversal
        = gameplay::updateDoor(door, 1.0F / 60.0F, false, true);
    expect(reversal.startedOpening
            && door.state == world::DoorState::Opening
            && door.openness > 0.70F,
        "an occupied threshold reverses a closing panel without trapping its occupant");
}

void testPreviouslyBrokenCorridorSeedsTraverseBothWays()
{
    using namespace gdp;
    constexpr std::array<wkc::core::Seed, 9> regressionSeeds{
        9U, 48U, 49U, 52U, 102U, 147U, 152U, 180U,
        17'590'678'094'381'256'222ULL,
    };
    for (const wkc::core::Seed seed : regressionSeeds) {
        const world::WorldGenerationResult generated = world::generateWorld(seed);
        expect(generated.succeeded() && generated.world
                && everyCorridorTraversesBothWays(*generated.world),
            "previously broken corridor seed generates only traversable routes");
    }
}

void testCorridorTraversalSeedBatch()
{
    using namespace gdp;
    bool allTraversable = true;
    for (wkc::core::Seed seed = 0; seed < 500U; ++seed) {
        const world::WorldGenerationResult generated = world::generateWorld(seed);
        if (!generated.world || !everyCorridorTraversesBothWays(*generated.world)) {
            std::cerr << "Corridor traversal batch failed at seed " << seed << '\n';
            allTraversable = false;
            break;
        }
    }
    expect(allTraversable,
        "500 deterministic worlds keep every corridor traversable in both directions");
}

void testGeneratedDoorsFitTheLargestEnemyCollider()
{
    using namespace gdp;
    bool allTraversable = true;
    for (wkc::core::Seed seed = 0U; seed < 32U; ++seed) {
        const world::WorldGenerationResult generated = world::generateWorld(seed);
        if (!generated.world || !everyCorridorTraversesBothWays(
                *generated.world, {.radius = 0.52F, .height = 1.9F})) {
            std::cerr << "Robot corridor clearance failed at seed "
                      << seed << '\n';
            allTraversable = false;
            break;
        }
    }
    expect(allTraversable,
        "generated straight and bent passages fit the boss collider in both directions");
}

void testOpenCorridorShotsReachNextRoom()
{
    using namespace gdp;
    bool allReachable = true;
    for (wkc::core::Seed seed = 0U; seed < 64U; ++seed) {
        const world::WorldGenerationResult generated = world::generateWorld(seed);
        if (!generated.world || generated.world->corridors.empty()) {
            allReachable = false;
            break;
        }
        const world::World& world = *generated.world;
        const world::CorridorPath& corridor = world.corridors.front();
        const world::RoomInstance* destination = findRoom(world, corridor.secondRoom);
        const world::ConnectorInstance* destinationDoor = destination == nullptr
            ? nullptr : connectorForEdge(*destination, corridor.edge);
        if (destinationDoor == nullptr || corridor.points.size() < 3U) {
            allReachable = false;
            break;
        }
        wkc::core::Vec3 origin = corridor.points[corridor.points.size() - 2U];
        origin.y = 1.0F;
        wkc::core::Vec3 target = destinationDoor->worldPosition
            - destinationDoor->worldNormal * 1.4F;
        target.y = 1.0F;
        if (!gameplay::corridorProgressAt(corridor, origin)
            || gameplay::roomContainingCharacter(world, origin)) {
            allReachable = false;
            break;
        }
        entities::EntityManager manager;
        const entities::EntityId enemy = manager.createEnemy({
            .transform = {.position = {target.x, 0.0F, target.z}},
            .roomId = destination->id,
            .health = 100.0F,
        });
        gameplay::Weapon weapon({.fireInterval = 0.0F, .range = 20.0F});
        const auto navigation = gameplay::makeRoomNavigationCollision(
            world, corridor.firstRoom, false);
        const std::array reachableRoom{destination->id};
        const gameplay::ShotResult shot = gameplay::fireHitscan(
            weapon, origin, wkc::core::normalized(target - origin),
            corridor.firstRoom, navigation, manager, 1.0F, reachableRoom);
        if (!shot.hit || shot.hit->target != enemy) {
            std::cerr << "Corridor combat failed at seed " << seed << '\n';
            allReachable = false;
            break;
        }
    }
    expect(allReachable,
        "shots from generated corridors reach enemies just inside the next room");
}

void testBossKillBeforeRoomEntryFinishesRun()
{
    using namespace gdp;
    const world::WorldGenerationResult generated = world::generateWorld(0U);
    expect(generated.succeeded(), "corridor boss-fight world generates");
    if (!generated.world) {
        return;
    }
    const world::World& world = *generated.world;
    const auto bossRoom = std::find_if(world.rooms.begin(), world.rooms.end(),
        [](const world::RoomInstance& room) {
            return room.role == world::RoomRole::Boss;
        });
    expect(bossRoom != world.rooms.end(),
        "boss-fight world contains the Boss room");
    if (bossRoom == world.rooms.end()) {
        return;
    }
    const auto route = std::find_if(world.corridors.begin(), world.corridors.end(),
        [&bossRoom](const world::CorridorPath& corridor) {
            return corridor.firstRoom == bossRoom->id
                || corridor.secondRoom == bossRoom->id;
        });
    expect(bossRoom != world.rooms.end() && route != world.corridors.end(),
        "boss room is connected to a playable corridor");
    if (bossRoom == world.rooms.end() || route == world.corridors.end()) {
        return;
    }
    const world::ConnectorInstance* door = connectorForEdge(*bossRoom, route->edge);
    expect(door != nullptr && route->points.size() >= 3U,
        "boss entry has a door and corridor approach");
    if (door == nullptr || route->points.size() < 3U) {
        return;
    }
    const world::RoomId sourceRoom = route->firstRoom == bossRoom->id
        ? route->secondRoom : route->firstRoom;
    wkc::core::Vec3 origin = route->firstRoom == bossRoom->id
        ? route->points[1U] : route->points[route->points.size() - 2U];
    origin.y = 1.0F;
    wkc::core::Vec3 target
        = door->worldPosition - door->worldNormal * 0.75F;
    target.y = 1.0F;
    entities::EntityManager manager;
    (void)manager.createEnemy({
        .transform = {.position = {target.x, 0.0F, target.z}},
        .roomId = bossRoom->id,
        .archetype = entities::EnemyArchetype::IndustrialRobot,
        .health = 220.0F,
    });
    gameplay::Weapon weapon({.damage = 500.0F, .fireInterval = 0.0F,
        .range = 30.0F});
    const gameplay::CollisionWorld navigation
        = gameplay::makeRoomNavigationCollision(world, sourceRoom, false);
    const std::array reachableRoom{bossRoom->id};
    const gameplay::ShotResult shot = gameplay::fireHitscan(
        weapon, origin, wkc::core::normalized(target - origin),
        sourceRoom, navigation, manager, 1.0F, reachableRoom);
    const gameplay::BossStatus status = gameplay::bossStatus(manager, 0U);
    gameplay::SessionSystem session{0U};
    const gameplay::SessionUpdate resolved = session.update(
        true, world::RoomRole::Normal, status.exists, status.alive, true);
    expect(shot.outcome == gameplay::ShotOutcome::Killed
            && status == gameplay::BossStatus{true, false}
            && resolved.phase == gameplay::SessionPhase::Victory,
        "a lethal shot from the corridor resolves victory without entering Boss");
}

void testEnemiesUseOpenCorridorsButDoNotOpenClosedDoors()
{
    using namespace gdp;
    const world::WorldGenerationResult generated = world::generateWorld(0U);
    expect(generated.succeeded(), "enemy corridor regression world generates");
    if (!generated.world) {
        return;
    }
    const world::World& world = *generated.world;
    const auto route = std::find_if(world.corridors.begin(), world.corridors.end(),
        [](const world::CorridorPath& corridor) {
            return corridor.shape == world::CorridorShape::Straight;
        });
    expect(route != world.corridors.end(),
        "enemy corridor regression world has a straight route");
    if (route == world.corridors.end()) {
        return;
    }
    const world::RoomInstance* source = findRoom(world, route->firstRoom);
    const world::RoomInstance* destination = findRoom(world, route->secondRoom);
    const world::ConnectorInstance* sourceDoor = source == nullptr
        ? nullptr : connectorForEdge(*source, route->edge);
    const world::ConnectorInstance* destinationDoor = destination == nullptr
        ? nullptr : connectorForEdge(*destination, route->edge);
    expect(sourceDoor != nullptr && destinationDoor != nullptr,
        "enemy corridor route has both door endpoints");
    if (sourceDoor == nullptr || destinationDoor == nullptr) {
        return;
    }

    const wkc::core::Vec3 playerPosition
        = sourceDoor->worldPosition - sourceDoor->worldNormal * 0.80F;
    const wkc::core::Vec3 enemyPosition
        = destinationDoor->worldPosition - destinationDoor->worldNormal * 0.55F;
    const wkc::core::Vec3 towardPlayer
        = wkc::core::normalized(playerPosition - enemyPosition);
    const float enemyYaw = std::atan2(towardPlayer.x, towardPlayer.z)
        * (180.0F / std::acos(-1.0F));
    gameplay::Player player;
    player.spawn(playerPosition);
    gameplay::WorkerConfig config;
    config.detectionRange = 100.0F;
    config.investigation.visionHalfAngleDegrees = 180.0F;
    config.moveSpeed = 3.0F;
    config.attackRange = 0.10F;
    config.attackDamage = 0.0F;

    entities::EntityManager closedManager;
    const entities::EntityId blocked = closedManager.createEnemy({
        .transform = {.position = enemyPosition,
            .rotationDegrees = {0.0F, enemyYaw, 0.0F}},
        .roomId = destination->id,
    });
    const std::array blockedEdges{route->edge};
    const gameplay::CollisionWorld closedNavigation
        = gameplay::makeRoomNavigationCollision(
            world, destination->id, false, blockedEdges);
    gameplay::DoorRuntime closedDoor = gameplay::makeDoorRuntime(route->edge);
    for (int tick = 0; tick < 240; ++tick) {
        (void)gameplay::updateWorkers(closedManager, player,
            destination->id, closedNavigation, 1.0F / 60.0F, config);
        gameplay::updateEnemyRoomMembership(world, closedManager);
        (void)gameplay::updateDoor(closedDoor, 1.0F / 60.0F, false, false);
    }
    const entities::Entity* blockedEnemy = closedManager.find(blocked);
    expect(blockedEnemy != nullptr
            && blockedEnemy->roomId == destination->id
            && gameplay::roomContainingCharacter(
                world, blockedEnemy->transform.position,
                {.radius = 0.35F, .height = 1.8F}) == destination->id
            && !gameplay::isDoorPassable(closedDoor),
        "enemy cannot open or pass a closed corridor door");

    entities::EntityManager openManager;
    const entities::EntityId pursuing = openManager.createEnemy({
        .transform = {.position = enemyPosition,
            .rotationDegrees = {0.0F, enemyYaw, 0.0F}},
        .roomId = destination->id,
    });
    const gameplay::CollisionWorld openNavigation
        = gameplay::makeRoomNavigationCollision(world, destination->id, false);
    gameplay::DoorRuntime openDoor = gameplay::makeDoorRuntime(route->edge);
    expect(gameplay::requestDoorOpen(openDoor, false),
        "player-opened door is the prerequisite for enemy corridor pursuit");
    (void)gameplay::updateDoor(openDoor, 1.0F, false, true);
    bool enteredCorridor = false;
    bool reachedSource = false;
    bool stayedPassableWhileOccupied = true;
    for (int tick = 0; tick < 1800; ++tick) {
        (void)gameplay::updateWorkers(openManager, player,
            destination->id, openNavigation, 1.0F / 60.0F, config);
        gameplay::updateEnemyRoomMembership(world, openManager);
        const entities::Entity* enemy = openManager.find(pursuing);
        if (enemy == nullptr) {
            break;
        }
        const bool inCorridor = !gameplay::roomContainingCharacter(
            world, enemy->transform.position,
            {.radius = 0.35F, .height = 1.8F})
            && gameplay::corridorProgressAt(*route, enemy->transform.position);
        enteredCorridor = enteredCorridor || inCorridor;
        if (inCorridor) {
            (void)gameplay::updateDoor(openDoor, 2.0F, false,
                gameplay::livingEnemyOccupiesCorridor(world, *route, openManager));
            stayedPassableWhileOccupied = stayedPassableWhileOccupied
                && gameplay::isDoorPassable(openDoor);
        }
        if (enemy->roomId == source->id) {
            reachedSource = true;
            break;
        }
    }
    if (!reachedSource) {
        const entities::Entity* enemy = openManager.find(pursuing);
        const entities::EnemyData* data
            = openManager.findData<entities::EnemyData>(pursuing);
        if (enemy != nullptr && data != nullptr) {
            std::cerr << "Enemy corridor diagnosis: shape="
                      << static_cast<int>(route->shape)
                      << " player=(" << playerPosition.x << ',' << playerPosition.z
                      << ") enemy=(" << enemy->transform.position.x << ','
                      << enemy->transform.position.z << ") start=("
                      << enemyPosition.x << ',' << enemyPosition.z
                      << ") behavior=" << static_cast<int>(data->behavior)
                      << " corridor=" << enteredCorridor << '\n';
        }
    }
    expect(enteredCorridor && stayedPassableWhileOccupied,
        "enemy enters the open corridor and keeps its door passable throughout transit");
    expect(reachedSource,
        "enemy reaches the player room and adopts its new room ownership");

    // The biggest enemy follows the same open passage rather than treating
    // the connector as a player-only transition.
    entities::EntityManager robotManager;
    const entities::EntityId robot = robotManager.createEnemy({
        .transform = {.position = enemyPosition,
            .rotationDegrees = {0.0F, enemyYaw, 0.0F}},
        .roomId = destination->id,
        .archetype = entities::EnemyArchetype::IndustrialRobot,
        .health = 220.0F,
    });
    auto* robotData = robotManager.findData<entities::EnemyData>(robot);
    robotData->health = 20.0F; // Pursuit phase: the boss actively closes distance.
    gameplay::IndustrialRobotConfig robotConfig;
    robotConfig.detectionRange = 100.0F;
    robotConfig.investigation.visionHalfAngleDegrees = 180.0F;
    robotConfig.pursuitMoveSpeed = 3.0F;
    robotConfig.pursuitAttackRange = 0.10F;
    robotConfig.pursuitDamage = 0.0F;
    bool robotReachedSource = false;
    for (int tick = 0; tick < 1800; ++tick) {
        (void)gameplay::updateIndustrialRobots(robotManager, player,
            destination->id, openNavigation, 1.0F / 60.0F, robotConfig);
        gameplay::updateEnemyRoomMembership(world, robotManager);
        const entities::Entity* enemy = robotManager.find(robot);
        if (enemy != nullptr && enemy->roomId == source->id) {
            robotReachedSource = true;
            break;
        }
    }
    if (!robotReachedSource) {
        const entities::Entity* enemy = robotManager.find(robot);
        if (enemy != nullptr) {
            std::cerr << "Robot corridor diagnosis: start=("
                      << enemyPosition.x << ',' << enemyPosition.z
                      << ") final=(" << enemy->transform.position.x << ','
                      << enemy->transform.position.z << ") room="
                      << enemy->roomId << '\n';
        }
    }
    expect(robotReachedSource,
        "boss crosses an open doorway and corridor without sticking at the threshold");

    gameplay::Player corridorPlayer;
    corridorPlayer.spawn(destinationDoor->worldPosition
        + destinationDoor->worldNormal * 0.25F);
    expect(!gameplay::roomContainingCharacter(
            world, corridorPlayer.state().position, corridorPlayer.collider()),
        "combat fixture places the player physically inside the corridor");
    entities::EntityManager attackManager;
    (void)attackManager.createEnemy({
        .transform = {.position = enemyPosition,
            .rotationDegrees = {0.0F, enemyYaw, 0.0F}},
        .roomId = destination->id,
    });
    gameplay::WorkerConfig attackConfig;
    attackConfig.investigation.visionHalfAngleDegrees = 180.0F;
    const auto attack = gameplay::updateWorkers(attackManager, corridorPlayer,
        destination->id, openNavigation, 1.0F / 60.0F, attackConfig);
    expect(attack.attacksLanded == 1U
            && corridorPlayer.state().health < 100.0F,
        "enemy in the next room can see and damage the player in an open corridor");

    gameplay::Player bossTarget;
    bossTarget.spawn(corridorPlayer.state().position);
    entities::EntityManager bossAttackManager;
    (void)bossAttackManager.createEnemy({
        .transform = {.position = enemyPosition,
            .rotationDegrees = {0.0F, enemyYaw, 0.0F}},
        .roomId = destination->id,
        .archetype = entities::EnemyArchetype::IndustrialRobot,
        .health = 220.0F,
    });
    gameplay::IndustrialRobotConfig bossAttackConfig;
    bossAttackConfig.investigation.visionHalfAngleDegrees = 180.0F;
    bool bossAttackedCorridor = false;
    for (int tick = 0; tick < 600 && bossTarget.state().alive; ++tick) {
        const auto update = gameplay::updateIndustrialRobots(
            bossAttackManager, bossTarget, destination->id,
            openNavigation, 1.0F / 60.0F, bossAttackConfig);
        bossAttackedCorridor = bossAttackedCorridor
            || update.attacksLanded > 0U;
        if (bossAttackedCorridor) {
            break;
        }
    }
    expect(bossAttackedCorridor,
        "full-health boss can attack a visible player in the corridor");
}

void testLockdownEntryKeepsPlayerMobile()
{
    using namespace gdp;
    const world::WorldGenerationResult generated = world::generateWorld(0U);
    expect(generated.succeeded(), "lockdown entry regression world generates");
    if (!generated.world) {
        return;
    }
    const world::CorridorPath* route = nullptr;
    const world::RoomInstance* source = nullptr;
    const world::RoomInstance* destination = nullptr;
    for (const world::CorridorPath& corridor : generated.world->corridors) {
        const world::RoomInstance* first = findRoom(*generated.world, corridor.firstRoom);
        const world::RoomInstance* second = findRoom(*generated.world, corridor.secondRoom);
        if (first != nullptr && second != nullptr
            && (second->role == world::RoomRole::Arena
                || second->role == world::RoomRole::Boss)) {
            route = &corridor;
            source = first;
            destination = second;
            break;
        }
        if (first != nullptr && second != nullptr
            && (first->role == world::RoomRole::Arena
                || first->role == world::RoomRole::Boss)) {
            route = &corridor;
            source = second;
            destination = first;
            break;
        }
    }
    expect(route != nullptr && source != nullptr && destination != nullptr,
        "regression world exposes a corridor entering a lockdown room");
    if (route == nullptr || source == nullptr || destination == nullptr) {
        return;
    }
    const world::ConnectorInstance* entry = connectorForEdge(*destination, route->edge);
    expect(entry != nullptr, "lockdown destination owns its entry connector");
    if (entry == nullptr) {
        return;
    }

    const gameplay::CollisionWorld openNavigation
        = gameplay::makeRoomNavigationCollision(*generated.world, source->id, false);
    wkc::core::Vec3 position = entry->worldPosition + (entry->worldNormal * 0.50F);
    const wkc::core::Vec3 inward = entry->worldNormal * -1.0F;
    for (int step = 0; step < 120
         && gameplay::roomContainingCharacter(*generated.world, position)
            != destination->id;
         ++step) {
        position = openNavigation.moveCharacter(
            position, inward * 4.5F, 1.0F / 120.0F, {}).position;
    }
    expect(gameplay::roomContainingCharacter(*generated.world, position)
            == destination->id,
        "destination is adopted only after the character fully clears the threshold");

    gameplay::DoorRuntime door = gameplay::makeDoorRuntime(route->edge);
    expect(gameplay::requestDoorOpen(door, false),
        "lockdown regression door starts opening");
    (void)gameplay::updateDoor(door, 1.0F, false, true);
    (void)gameplay::updateDoor(door, 1.0F / 60.0F, true, true);
    expect(gameplay::isDoorPassable(door),
        "new lockdown does not close its occupied entry door");

    const gameplay::CollisionWorld lockedNavigation
        = gameplay::makeRoomNavigationCollision(*generated.world, destination->id, false);
    expect(lockedNavigation.canOccupy(position),
        "newly adopted lockdown position remains valid navigation space");
    const wkc::core::Vec3 beforeMove = position;
    for (int step = 0; step < 30; ++step) {
        position = lockedNavigation.moveCharacter(
            position, inward * 4.5F, 1.0F / 120.0F, {}).position;
    }
    expect(!wkc::core::nearlyEqual(position, beforeMove, 1.0e-5F),
        "player can continue into the lockdown room instead of becoming immobilized");
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
            std::vector<wkc::core::Vec3> waypoints = corridor.points;
            if (sourceId == corridor.secondRoom) {
                std::reverse(waypoints.begin(), waypoints.end());
            }
            waypoints.push_back(destinationDoor->worldPosition
                - (destinationDoor->worldNormal * 0.75F));

            wkc::core::Vec3 position = sourceDoor->worldPosition
                - (sourceDoor->worldNormal * 0.55F);
            bool completed = true;
            for (const wkc::core::Vec3 waypoint : waypoints) {
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
            const std::optional<float> firstProgress
                = gameplay::corridorProgressAt(corridor, corridor.points.front());
            const std::optional<float> lastProgress
                = gameplay::corridorProgressAt(corridor, corridor.points.back());
            expect(firstProgress && lastProgress
                    && *firstProgress <= 1.0e-4F && *lastProgress >= 0.9999F,
                "corridor projection provides stable first-to-second blend progress");

            const std::array blockedEdge{corridor.edge};
            const gameplay::CollisionWorld closedDoorNavigation
                = gameplay::makeRoomNavigationCollision(
                    *generated.world, sourceId, false, blockedEdge);
            wkc::core::Vec3 closedDoorPosition = sourceDoor->worldPosition
                - (sourceDoor->worldNormal * 0.55F);
            for (int step = 0; step < 120; ++step) {
                const gameplay::CharacterMotionResult motion
                    = closedDoorNavigation.moveCharacter(
                        closedDoorPosition, sourceDoor->worldNormal * 4.5F,
                        1.0F / 120.0F, {});
                closedDoorPosition = motion.position;
            }
            expect(source->worldBounds.contains(closedDoorPosition)
                    && !destination->worldBounds.contains(closedDoorPosition),
                "a closed individual door blocks only its corridor route");

            const gameplay::CollisionWorld lockedNavigation
                = gameplay::makeRoomNavigationCollision(
                    *generated.world, sourceId, true);
            wkc::core::Vec3 lockedPosition = sourceDoor->worldPosition
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
    testOpenCorridorShotsReachNextRoom();
    testBossKillBeforeRoomEntryFinishesRun();
    testEnemiesUseOpenCorridorsButDoNotOpenClosedDoors();
    testRangeInvalidInputAndPlayerRelocation();
    testCharacterRoomAdoptionRequiresFullClearance();
    testRotatedRoomCollisionMatchesModuleBounds();
    testDoorRuntimeOpensHoldsClosesAndLocks();
    testDoorPanelTravelMatchesPassability();
    testPreviouslyBrokenCorridorSeedsTraverseBothWays();
    testCorridorTraversalSeedBatch();
    testGeneratedDoorsFitTheLargestEnemyCollider();
    testLockdownEntryKeepsPlayerMobile();
    testGeneratedCorridorsSupportContinuousWalkingAndLockdown();

    if (failures != 0) {
        std::cerr << failures << " room traversal test(s) failed\n";
        return 1;
    }
    std::cout << "Room traversal tests passed\n";
    return 0;
}
