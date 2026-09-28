#include "core/Math.hpp"
#include "world/Connector.hpp"
#include "world/Corridor.hpp"
#include "world/MapGraph.hpp"
#include "world/LootGenerator.hpp"
#include "world/PropGenerator.hpp"
#include "world/RoomModule.hpp"
#include "world/WorldEntities.hpp"
#include "world/WorldGenerator.hpp"
#include "world/WorldValidation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
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

bool containsError(
    const gdp::world::ModuleValidationResult& result,
    const gdp::world::ModuleValidationError error)
{
    return std::find(result.errors.begin(), result.errors.end(), error) != result.errors.end();
}

void testModuleValidation()
{
    using namespace gdp::world;

    const RoomModule module = makeFixedRoomModule();
    expect(validateRoomModule(module).isValid(), "built-in fixed room module validates");
    expect(module.supportsRole(RoomRole::Start), "fixed room supports START role");
    expect(module.supportsRole(RoomRole::Arena), "fixed room supports ARENA role");
    expect(!module.supportsRole(RoomRole::Boss), "fixed room rejects unsupported BOSS role");

    RoomModule duplicate = module;
    duplicate.connectors[1].id = duplicate.connectors[0].id;
    const auto duplicateResult = validateRoomModule(duplicate);
    expect(containsError(duplicateResult, ModuleValidationError::DuplicateConnectorId),
        "duplicate connector IDs are rejected");

    RoomModule offPerimeter = module;
    offPerimeter.connectors[0].position.z = 0.0F;
    const auto perimeterResult = validateRoomModule(offPerimeter);
    expect(containsError(perimeterResult, ModuleValidationError::ConnectorOffPerimeter),
        "connector away from module perimeter is rejected");

    RoomModule outsideZone = module;
    outsideZone.lootZones[0].bounds.maximum.x = 100.0F;
    const auto zoneResult = validateRoomModule(outsideZone);
    expect(containsError(zoneResult, ModuleValidationError::ZoneOutsideBounds),
        "spawn zone outside local bounds is rejected");

    RoomModule mismatchedFootprint = module;
    mismatchedFootprint.footprint.x = 20;
    const auto footprintResult = validateRoomModule(mismatchedFootprint);
    expect(containsError(
               footprintResult,
               ModuleValidationError::BoundsFootprintMismatch),
        "module bounds must agree with the declared structural footprint");

    RoomModule blockedConnector = module;
    blockedConnector.propZones.push_back(
        {{{-0.2F, 0.0F, 4.3F}, {0.2F, 1.0F, 4.8F}}});
    const auto clearanceResult = validateRoomModule(blockedConnector);
    expect(containsError(
               clearanceResult,
               ModuleValidationError::ZoneOverlapsConnectorClearance),
        "spawn zone inside connector clearance is rejected");
}

void testDefaultModuleStructuralVariety()
{
    using namespace wkc::core;
    using namespace gdp::world;

    const std::vector<RoomModule> library = makeDefaultRoomModuleLibrary();
    bool sawCompact = false;
    bool sawMedium = false;
    bool sawLarge = false;
    bool roleSizesValid = true;
    bool rotationsValid = true;
    for (const RoomModule& module : library) {
        expect(validateRoomModule(module).isValid(),
            "every default structural room module validates");
        sawCompact = sawCompact || module.sizeClass == RoomSizeClass::Compact;
        sawMedium = sawMedium || module.sizeClass == RoomSizeClass::Medium;
        sawLarge = sawLarge || module.sizeClass == RoomSizeClass::Large;
        if ((module.supportsRole(RoomRole::Hub)
                || module.supportsRole(RoomRole::Arena)
                || module.supportsRole(RoomRole::Boss))
            && module.sizeClass != RoomSizeClass::Large) {
            roleSizesValid = false;
        }
        if (module.supportsRole(RoomRole::Secret)
            && module.sizeClass != RoomSizeClass::Compact) {
            roleSizesValid = false;
        }

        for (const QuarterTurn rotation : {
                 QuarterTurn::Degrees0,
                 QuarterTurn::Degrees90,
                 QuarterTurn::Degrees180,
                 QuarterTurn::Degrees270}) {
            RoomRole role = RoomRole::Normal;
            for (const RoomRole candidate : {
                     RoomRole::Start, RoomRole::Normal, RoomRole::Hub,
                     RoomRole::Arena, RoomRole::DeadEnd, RoomRole::Secret,
                     RoomRole::Boss}) {
                if (module.supportsRole(candidate)) {
                    role = candidate;
                    break;
                }
            }
            const auto room = instantiateRoom(
                module, module.id, role, 77U, {23.0F, 0.0F, -19.0F}, rotation);
            if (!room || room->sizeClass != module.sizeClass
                || !room->worldBounds.isValid()) {
                rotationsValid = false;
                continue;
            }
            for (const ConnectorInstance& connector : room->connectors) {
                const auto direction = directionFromNormal(connector.worldNormal);
                if (!direction) {
                    rotationsValid = false;
                    continue;
                }
                const float plane = *direction == Direction::North
                    ? room->worldBounds.maximum.z
                    : *direction == Direction::South
                    ? room->worldBounds.minimum.z
                    : *direction == Direction::East
                    ? room->worldBounds.maximum.x
                    : room->worldBounds.minimum.x;
                const float position = *direction == Direction::North
                        || *direction == Direction::South
                    ? connector.worldPosition.z : connector.worldPosition.x;
                rotationsValid = rotationsValid && nearlyEqual(plane, position);
            }
        }
    }
    expect(library.size() >= 20U && sawCompact && sawMedium && sawLarge,
        "default library contains compact, medium and large structural families");
    expect(roleSizesValid,
        "Hub, Arena and Boss use large modules while Secret uses compact modules");
    expect(rotationsValid,
        "every default module keeps bounds and connectors valid in all four rotations");
}

void testInstantiationAndRotation()
{
    using namespace wkc::core;
    using namespace gdp::world;

    const RoomModule module = makeFixedRoomModule();
    const auto room = instantiateRoom(
        module,
        7,
        RoomRole::Normal,
        1234,
        {10.0F, 0.0F, 20.0F},
        QuarterTurn::Degrees90);
    expect(room.has_value(), "supported room instance is created");
    if (!room) {
        return;
    }

    expect(room->moduleId == module.id && room->seed == 1234,
        "room preserves module identity and seed");
    expect(nearlyEqual(room->transform.rotationDegrees.y, 90.0F),
        "room transform records quarter-turn rotation");
    expect(nearlyEqual(room->worldBounds.minimum, {5.15F, 0.0F, 16.15F}),
        "rotated room minimum bounds are correct");
    expect(nearlyEqual(room->worldBounds.maximum, {14.85F, 4.0F, 23.85F}),
        "rotated room maximum bounds are correct");
    expect(nearlyEqual(room->connectors[0].worldPosition, {14.85F, 0.0F, 20.0F}),
        "connector position follows room rotation and translation");
    expect(directionFromNormal(room->connectors[0].worldNormal) == Direction::East,
        "connector normal follows room rotation");
    expect(room->enemyZones.size() == 1U
            && nearlyEqual(room->enemyZones.front().bounds.minimum, {9.0F, 0.0F, 18.0F})
            && nearlyEqual(room->enemyZones.front().bounds.maximum, {11.0F, 0.0F, 22.0F}),
        "enemy/event zones follow room rotation and translation into world space");
    expect(room->propZones.size() == 1U
            && nearlyEqual(room->propZones.front().bounds.minimum, {7.0F, 0.0F, 17.0F})
            && nearlyEqual(room->propZones.front().bounds.maximum, {11.5F, 0.0F, 18.5F}),
        "prop zones follow room rotation and translation into world space");

    constexpr std::array expectedDirections{
        Direction::North,
        Direction::East,
        Direction::South,
        Direction::West,
    };
    constexpr std::array rotations{
        QuarterTurn::Degrees0,
        QuarterTurn::Degrees90,
        QuarterTurn::Degrees180,
        QuarterTurn::Degrees270,
    };
    for (std::size_t index = 0; index < rotations.size(); ++index) {
        const auto rotated = instantiateRoom(
            module, 20 + static_cast<RoomId>(index), RoomRole::Normal, 1, {}, rotations[index]);
        expect(rotated.has_value(), "room instantiates at every quarter turn");
        if (rotated) {
            expect(directionFromNormal(rotated->connectors[0].worldNormal)
                    == expectedDirections[index],
                "connector direction is correct at every quarter turn");
        }
    }

    const auto unsupported = instantiateRoom(
        module, 8, RoomRole::Boss, 1234, {}, QuarterTurn::Degrees0);
    expect(!unsupported, "unsupported room role cannot be instantiated");
}

void testConnectorContracts()
{
    using namespace wkc::core;
    using namespace gdp::world;

    const ConnectorDefinition north{
        1, ConnectorType::Door, {}, {0.0F, 0.0F, 1.0F}, 1.2F, 2.2F, true};
    const ConnectorDefinition south{
        2, ConnectorType::Door, {}, {0.0F, 0.0F, -1.0F}, 1.2F, 2.2F, false};
    expect(areConnectorDefinitionsCompatible(north, south),
        "matching connector definitions are compatible");

    ConnectorDefinition secret = south;
    secret.type = ConnectorType::Secret;
    expect(!areConnectorDefinitionsCompatible(north, secret),
        "different connector types are incompatible");

    ConnectorInstance first = instantiateConnector(north, {}, QuarterTurn::Degrees0);
    ConnectorInstance second = instantiateConnector(south, {}, QuarterTurn::Degrees0);
    expect(canMate(first, second), "coincident opposing connectors can mate");
    expect(assignEdge(first, 99), "available connector accepts an edge");
    expect(!assignEdge(first, 100), "used connector rejects a second edge");
    expect(!canMate(first, second), "used connector is unavailable for mating");
    expect(sealConnector(second), "optional connector can be sealed");
    expect(!sealConnector(first), "used required connector cannot be sealed");

    ConnectorDefinition diagonal = north;
    diagonal.normal = {0.7F, 0.0F, 0.7F};
    expect(validateConnector(diagonal) == ConnectorValidationError::InvalidNormal,
        "non-cardinal connector normal is rejected");
}

void testRequiredConnectorResolution()
{
    using namespace wkc::core;
    using namespace gdp::world;

    RoomModule module = makeFixedRoomModule();
    module.connectors[0].required = true;
    auto room = instantiateRoom(module, 1, RoomRole::Start, 55, {}, QuarterTurn::Degrees0);
    expect(room.has_value(), "module with required connector instantiates");
    if (!room) {
        return;
    }
    expect(hasUnresolvedRequiredConnector(*room),
        "unassigned required connector invalidates room completion");
    expect(assignEdge(room->connectors[0], 7), "required connector accepts graph edge");
    expect(!hasUnresolvedRequiredConnector(*room),
        "assigned required connector resolves room invariant");
}

void testMapGraph()
{
    using namespace gdp::world;

    const MapGraph graph = generateMapGraph(12345, {8, 1});
    expect(validateMapGraph(graph).isValid(), "generated map graph satisfies invariants");
    expect(graph.nodes.size() == 8U, "graph contains requested room count");
    expect(graph.edges.size() == 8U, "graph spends its single loop budget");
    expect(graph.mainPath.front() == 1U, "main path begins at START");
    const MapEdge& loop = graph.edges.back();
    const bool firstIsBranch = loop.first > graph.mainPath.size();
    const bool secondIsBranch = loop.second > graph.mainPath.size();
    const RoomNodeId mainEndpoint = firstIsBranch ? loop.second : loop.first;
    const RoomNodeId branchEndpoint = firstIsBranch ? loop.first : loop.second;
    const auto branchLink = std::find_if(
        graph.edges.begin(), graph.edges.end(),
        [branchEndpoint](const MapEdge& edge) {
            return !edge.mainPath
                && (edge.first == branchEndpoint || edge.second == branchEndpoint);
        });
    const RoomNodeId branchParent = branchLink == graph.edges.end() ? 0U
        : branchLink->first == branchEndpoint ? branchLink->second : branchLink->first;
    expect(firstIsBranch != secondIsBranch && branchParent != 0U
            && (mainEndpoint + 1U == branchParent
                || branchParent + 1U == mainEndpoint),
        "loop closes a local branch instead of creating a map-spanning corridor");
    expect(graphToDot(graph).find("graph GDP") != std::string::npos,
        "graph debug dump uses DOT format");

    MapGraph invalid = graph;
    invalid.edges[0].second = invalid.edges[0].first;
    expect(!validateMapGraph(invalid).isValid(), "self-edge invalidates graph");
}

void testWorldGeneration()
{
    using namespace gdp::world;

    constexpr wkc::core::Seed goldenSeed = 0x123456789ABCDEF0ULL;
    const WorldGenerationResult first = generateWorld(goldenSeed);
    const WorldGenerationResult repeat = generateWorld(goldenSeed);
    const WorldGenerationResult different = generateWorld(goldenSeed + 1U);
    expect(first.succeeded() && repeat.succeeded() && different.succeeded(),
        "representative worlds generate successfully");
    if (!first.world || !repeat.world || !different.world) {
        return;
    }

    const std::uint64_t fingerprint = worldFingerprint(*first.world);
    expect(fingerprint == 0xAB7EF9529E06FBE8ULL,
        "golden world fingerprint remains stable across builds");
    expect(fingerprint == worldFingerprint(*repeat.world),
        "same seed produces the same complete world fingerprint");
    expect(fingerprint != worldFingerprint(*different.world),
        "different seed changes the world fingerprint");
    expect(validateWorld(*first.world).isValid(), "committed world passes every structural gate");

    World overlapping = *first.world;
    overlapping.rooms[1].worldBounds = overlapping.rooms[0].worldBounds;
    expect(!validateWorld(overlapping).isValid(), "overlapping room invalidates candidate world");

    RoomModule invalidModule = makeFixedRoomModule();
    invalidModule.roleCapabilities = 0;
    const std::array invalidLibrary{invalidModule};
    const WorldGenerationResult exhausted = generateWorld(
        7, {.maximumAttempts = 3}, invalidLibrary);
    expect(!exhausted.succeeded() && exhausted.attempts == 3U,
        "invalid library exhausts deterministic attempt budget explicitly");
}

void testCorridorRouting()
{
    using namespace wkc::core;
    using namespace gdp::world;

    ConnectorDefinition east{1, ConnectorType::Door, {}, {1.0F, 0.0F, 0.0F}, 1.2F, 2.2F, false};
    ConnectorDefinition west{2, ConnectorType::Door, {}, {-1.0F, 0.0F, 0.0F}, 1.2F, 2.2F, false};
    ConnectorInstance first = instantiateConnector(east, {}, QuarterTurn::Degrees0);
    ConnectorInstance straightSecond = instantiateConnector(
        west, {5.0F, 0.0F, 0.0F}, QuarterTurn::Degrees0);
    expect(assignEdge(first, 1) && assignEdge(straightSecond, 1),
        "corridor test connectors accept their edge");
    const auto straight = routeCorridor(1, 1, 2, first, straightSecond, {});
    expect(straight && straight->shape == CorridorShape::Straight,
        "aligned connectors produce a straight corridor");

    first = instantiateConnector(east, {}, QuarterTurn::Degrees0);
    ConnectorInstance bentSecond = instantiateConnector(
        west, {5.0F, 0.0F, 5.0F}, QuarterTurn::Degrees0);
    expect(assignEdge(first, 2) && assignEdge(bentSecond, 2),
        "bent corridor connectors accept their edge");
    const auto bend = routeCorridor(2, 1, 2, first, bentSecond, {});
    expect(bend && bend->shape == CorridorShape::L,
        "offset connectors prefer an L corridor");

    RoomInstance obstacleA;
    obstacleA.id = 3;
    obstacleA.worldBounds = {{3.9F, 0.0F, 1.0F}, {4.6F, 3.0F, 3.0F}};
    RoomInstance obstacleB;
    obstacleB.id = 4;
    obstacleB.worldBounds = {{0.4F, 0.0F, 1.0F}, {1.1F, 3.0F, 3.0F}};
    const std::array obstacles{obstacleA, obstacleB};
    first = instantiateConnector(east, {}, QuarterTurn::Degrees0);
    bentSecond = instantiateConnector(west, {5.0F, 0.0F, 5.0F}, QuarterTurn::Degrees0);
    expect(assignEdge(first, 3) && assignEdge(bentSecond, 3),
        "Z corridor connectors accept their edge");
    const auto zigzag = routeCorridor(3, 1, 2, first, bentSecond, obstacles);
    expect(zigzag && zigzag->shape == CorridorShape::Z,
        "obstacles blocking both L routes select a Z corridor");
}

void testInitialEntityPopulation()
{
    using namespace gdp::entities;
    using namespace gdp::world;

    WorldGenerationResult generated = generateWorld(12345);
    expect(generated.succeeded(), "entity population test world generates");
    if (!generated.world) {
        return;
    }

    EntityManager manager;
    const InitialEntitySet initial = populateInitialEntities(
        *generated.world,
        manager,
        {.transform = {.position = {0.0F, 0.0F, 4.0F}}, .health = 100.0F});
    expect(initial.isValid(),
        "eight-room world receives a valid complete initial entity set");
    expect(manager.query(EntityKind::Player) == std::vector<EntityId>{initial.player},
        "population creates exactly one player entity");
    const PropData* terminal = manager.findData<PropData>(initial.terminal);
    expect(terminal != nullptr && terminal->type == PropType::Terminal && terminal->solid,
        "START room receives the solid interaction terminal");

    bool arenaCompositionValid = true;
    std::size_t arenaEnemyCount = 0U;
    bool bossRobotValid = false;
    bool startCoverValid = false;
    bool startCoverSpawnClear = false;
    bool startWorkerFacesPlayer = false;
    for (const RoomInstance& room : generated.world->rooms) {
        for (const EntityId id : room.entities) {
            const EnemyData* enemy = manager.findData<EnemyData>(id);
            const Entity* entity = manager.find(id);
            const PropData* prop = manager.findData<PropData>(id);
            if (room.role == RoomRole::Start && entity != nullptr
                && enemy != nullptr
                && enemy->archetype == EnemyArchetype::Worker) {
                constexpr float degreesToRadians = 0.01745329251994329577F;
                const float yaw
                    = entity->transform.rotationDegrees.y * degreesToRadians;
                const wkc::core::Vec3 forward{std::sin(yaw), 0.0F, std::cos(yaw)};
                const Entity* playerEntity = manager.find(initial.player);
                const wkc::core::Vec3 towardPlayer = playerEntity == nullptr
                    ? wkc::core::Vec3{}
                    : wkc::core::normalized(
                        playerEntity->transform.position - entity->transform.position);
                startWorkerFacesPlayer = wkc::core::dot(forward, towardPlayer) > 0.99F;
            }
            startCoverValid = startCoverValid || (room.role == RoomRole::Start
                && entity != nullptr && prop != nullptr
                && prop->type == PropType::Barricade && prop->solid
                && prop->health == 180.0F && prop->maximumHealth == 180.0F
                && prop->visualSeed != 0U
                && entity->transform.scale.y == 2.25F
                && std::max(
                    entity->transform.scale.x, entity->transform.scale.z) == 2.80F
                && std::min(
                    entity->transform.scale.x, entity->transform.scale.z) == 0.46F);
            if (room.role == RoomRole::Start && entity != nullptr && prop != nullptr
                && prop->type == PropType::Barricade
                && prop->maximumHealth == 180.0F) {
                const wkc::core::Vec3 halfScale{
                    entity->transform.scale.x * 0.5F,
                    0.0F,
                    entity->transform.scale.z * 0.5F,
                };
                const wkc::core::Aabb coverBounds{
                    entity->transform.position - halfScale,
                    entity->transform.position + wkc::core::Vec3{
                        halfScale.x, entity->transform.scale.y, halfScale.z},
                };
                startCoverSpawnClear = room.worldBounds.contains(coverBounds.minimum)
                    && room.worldBounds.contains(coverBounds.maximum);
                for (const EntityId otherId : room.entities) {
                    const Entity* other = manager.find(otherId);
                    startCoverSpawnClear = startCoverSpawnClear
                        && (otherId == id || other == nullptr
                            || !coverBounds.contains(other->transform.position));
                }
            }
            if (room.role == RoomRole::Arena && enemy != nullptr) {
                ++arenaEnemyCount;
                const float expectedHealth = enemy->archetype == EnemyArchetype::Guard
                    ? 75.0F : enemy->archetype == EnemyArchetype::Officer ? 65.0F
                    : enemy->archetype == EnemyArchetype::Engineer ? 58.0F : 50.0F;
                arenaCompositionValid = arenaCompositionValid
                    && enemy->health == expectedHealth
                    && enemy->maximumHealth == expectedHealth
                    && enemy->visualSeed != 0U;
            }
            bossRobotValid = bossRobotValid || (room.role == RoomRole::Boss
                && enemy != nullptr
                && enemy->archetype == EnemyArchetype::IndustrialRobot
                && enemy->health >= 360.0F && enemy->health <= 420.0F
                && enemy->health == enemy->maximumHealth
                && enemy->visualSeed != 0U);
        }
    }
    expect(arenaCompositionValid && arenaEnemyCount >= 2U && arenaEnemyCount <= 4U,
        "Arena population uses two to four healthy seeded enemies");
    expect(bossRobotValid,
        "Boss population includes one healthy IndustrialRobot with an isolated visual seed");
    expect(startCoverValid,
        "START includes one tall and wide cover volume for perception validation");
    expect(startCoverSpawnClear,
        "START cover stays inside the room without enclosing any initial entity");
    expect(startWorkerFacesPlayer,
        "START Worker initially faces the Player for the perception tutorial");

    const auto collectEnemyVisualSeeds = [](const EntityManager& entities) {
        std::vector<wkc::core::Seed> seeds;
        for (const EntityId id : entities.query(EntityKind::Enemy)) {
            const EnemyData* enemy = entities.findData<EnemyData>(id);
            if (enemy != nullptr) {
                seeds.push_back(enemy->visualSeed);
            }
        }
        return seeds;
    };
    const std::vector<wkc::core::Seed> visualSeeds = collectEnemyVisualSeeds(manager);
    expect(!visualSeeds.empty()
            && std::all_of(visualSeeds.begin(), visualSeeds.end(),
                [](const wkc::core::Seed seed) { return seed != 0; }),
        "every populated enemy receives a non-zero isolated visual seed");

    bool roomLinksValid = true;
    bool lootLayoutsValid = true;
    bool propLayoutsValid = true;
    bool proceduralArmorPresent = false;
    bool proceduralWeaponPresent = false;
    bool proceduralSpecialPresent = false;
    for (const RoomInstance& room : generated.world->rooms) {
        const auto node = std::find_if(
            generated.world->graph.nodes.begin(), generated.world->graph.nodes.end(),
            [&room](const RoomNode& candidate) { return candidate.id == room.id; });
        const LootLayout loot = node == generated.world->graph.nodes.end()
            ? LootLayout{}
            : generateLootLayout(
                room.seed, room.role, room.theme, node->depth, room.lootZones);
        const PropLayout props = node == generated.world->graph.nodes.end()
            ? PropLayout{}
            : generatePropLayout(
                room.seed, room.role, room.theme, node->depth, room.propZones);
        const std::size_t baseCount = room.role == RoomRole::Start ? 4U
            : room.role == RoomRole::Boss ? 2U
            : static_cast<std::size_t>(std::count_if(
                room.entities.begin(), room.entities.end(),
                [&manager](const EntityId id) {
                    const Entity* entity = manager.find(id);
                    return entity != nullptr && entity->kind == EntityKind::Enemy;
                }));
        const std::size_t expectedCount
            = baseCount + props.props.size() + loot.drops.size();
        if (room.entities.size() != expectedCount || manager.queryRoom(room.id) != room.entities) {
            roomLinksValid = false;
            break;
        }
        for (const EntityId id : room.entities) {
            const Entity* entity = manager.find(id);
            if (entity == nullptr || entity->roomId != room.id) {
                roomLinksValid = false;
                break;
            }
        }
        std::size_t dropIndex = 0;
        for (const EntityId id : room.entities) {
            const Entity* entity = manager.find(id);
            const ItemData* item = manager.findData<ItemData>(id);
            if (entity == nullptr || item == nullptr) {
                continue;
            }
            proceduralArmorPresent = proceduralArmorPresent
                || item->type == ItemType::Armor;
            proceduralWeaponPresent = proceduralWeaponPresent
                || item->type == ItemType::Weapon;
            proceduralSpecialPresent = proceduralSpecialPresent
                || item->type == ItemType::Special;
            if (dropIndex >= loot.drops.size()
                || item->type != loot.drops[dropIndex].type
                || item->quantity != loot.drops[dropIndex].quantity
                || entity->transform.position != loot.drops[dropIndex].position) {
                lootLayoutsValid = false;
                break;
            }
            ++dropIndex;
        }
        lootLayoutsValid = lootLayoutsValid && dropIndex == loot.drops.size()
            && validateLootLayout(loot, room.lootZones).isValid();
        std::size_t propIndex = 0;
        for (const EntityId id : room.entities) {
            const Entity* entity = manager.find(id);
            const PropData* prop = manager.findData<PropData>(id);
            if (entity == nullptr || prop == nullptr
                || (prop->type != PropType::Crate
                    && prop->type != PropType::Canister
                    && prop->type != PropType::Barricade)) {
                continue;
            }
            if (room.role == RoomRole::Start
                && prop->type == PropType::Barricade
                && prop->maximumHealth == 180.0F) {
                continue;
            }
            if (propIndex >= props.props.size()) {
                propLayoutsValid = false;
                break;
            }
            const PropSpawn& expected = props.props[propIndex];
            if (prop->type != expected.type || prop->state != expected.state
                || prop->solid != expected.solid || prop->health != expected.health
                || prop->maximumHealth != expected.maximumHealth
                || prop->dropType != expected.dropType
                || prop->dropQuantity != expected.dropQuantity
                || prop->visualSeed != expected.visualSeed
                || entity->transform.position != expected.position
                || entity->transform.scale != expected.scale) {
                propLayoutsValid = false;
                break;
            }
            ++propIndex;
        }
        propLayoutsValid = propLayoutsValid && propIndex == props.props.size()
            && validatePropLayout(props, room.propZones).isValid();
    }
    expect(roomLinksValid,
        "every RoomInstance owns its expected valid deterministic content handles");
    expect(lootLayoutsValid,
        "room population preserves each deterministic procedural loot layout exactly");
    expect(proceduralArmorPresent,
        "representative populated world exposes procedural Armor as playable loot");
    expect(proceduralWeaponPresent,
        "representative populated world exposes the Secret weapon pickup");
    expect(proceduralSpecialPresent,
        "representative populated world exposes the Arena Overdrive pickup");
    expect(propLayoutsValid,
        "room population preserves each deterministic procedural prop layout exactly");

    WorldGenerationResult repeated = generateWorld(12345);
    EntityManager repeatedManager;
    if (repeated.world) {
        const InitialEntitySet repeatedInitial = populateInitialEntities(
            *repeated.world,
            repeatedManager,
            {.transform = {.position = {0.0F, 0.0F, 4.0F}}, .health = 100.0F});
        expect(repeatedInitial.player == initial.player
                && repeatedInitial.terminal == initial.terminal,
            "same world populates core entity handles in stable order");
        expect(repeated.world->rooms.front().entities
                == generated.world->rooms.front().entities,
            "same world preserves deterministic room entity ordering");
        expect(collectEnemyVisualSeeds(repeatedManager) == visualSeeds,
            "same world seed reproduces every enemy visual seed");
    } else {
        expect(false, "repeated entity population test world generates");
    }

    WorldGenerationResult harder = generateWorld(12345);
    EntityManager harderManager;
    if (harder.world) {
        const InitialEntitySet harderInitial = populateInitialEntities(
            *harder.world,
            harderManager,
            {.transform = {.position = {0.0F, 0.0F, 4.0F}}, .health = 100.0F},
            1.2F);
        const std::vector<EntityId> baselineEnemies = manager.query(EntityKind::Enemy);
        const std::vector<EntityId> harderEnemies = harderManager.query(EntityKind::Enemy);
        bool enemyHealthScaled = harderInitial.isValid()
            && baselineEnemies.size() == harderEnemies.size();
        for (std::size_t index = 0;
             enemyHealthScaled && index < baselineEnemies.size(); ++index) {
            const EnemyData* baselineEnemy
                = manager.findData<EnemyData>(baselineEnemies[index]);
            const EnemyData* harderEnemy
                = harderManager.findData<EnemyData>(harderEnemies[index]);
            enemyHealthScaled = baselineEnemy != nullptr && harderEnemy != nullptr
                && baselineEnemy->archetype == harderEnemy->archetype
                && std::abs(harderEnemy->maximumHealth
                    - (baselineEnemy->maximumHealth * 1.2F)) < 1.0e-4F;
        }
        expect(enemyHealthScaled,
            "higher threat scales every initial enemy without changing its archetype order");
    } else {
        expect(false, "higher-threat entity population test world generates");
    }

    WorldGenerationResult different = generateWorld(54321);
    EntityManager differentManager;
    if (different.world) {
        (void)populateInitialEntities(
            *different.world,
            differentManager,
            {.transform = {.position = {0.0F, 0.0F, 4.0F}}, .health = 100.0F});
        expect(collectEnemyVisualSeeds(differentManager) != visualSeeds,
            "different world seed changes the enemy visual sequence");
    } else {
        expect(false, "different entity population test world generates");
    }

    World missingStart = *generated.world;
    for (RoomInstance& room : missingStart.rooms) {
        if (room.role == RoomRole::Start) {
            room.role = RoomRole::Normal;
        }
    }
    EntityManager rejectedManager;
    expect(!populateInitialEntities(missingStart, rejectedManager, {}).isValid()
            && rejectedManager.activeCount() == 0,
        "population rejects a world without START before creating partial content");

    EntityManager occupiedManager;
    (void)occupiedManager.createProp({});
    const std::size_t occupiedCount = occupiedManager.activeCount();
    expect(!populateInitialEntities(*generated.world, occupiedManager, {}).isValid()
            && occupiedManager.activeCount() == occupiedCount,
        "population rejects a non-empty manager without orphaning existing entities");
}

void testWorldSeedBatch()
{
    using namespace gdp::world;

    bool allValid = true;
    std::uint32_t maximumAcceptedAttempt = 0;
    float maximumCorridorLength = 0.0F;
    bool sawCompact = false;
    bool sawMedium = false;
    bool sawLarge = false;
    bool roleSizesValid = true;
    std::size_t secretAtHub = 0U;
    std::size_t secretFarther = 0U;
    bool secondBranchAwayFromHub = true;
    std::array<bool, 4> firstDirections{};
    std::array<bool, 4> hubPositions{};
    std::array<bool, 4> arenaPositions{};
    bool mainPathTurns = true;
    bool sawOneTurn = false;
    bool sawTwoTurns = false;
    std::array<bool, 2> normalCounts{};
    std::array<bool, 3> arenaCounts{};
    std::array<bool, 3> bossHealthValues{};
    bool encountersValid = true;
    for (wkc::core::Seed seed = 0; seed < 1'000; ++seed) {
        WorldGenerationResult generated = generateWorld(seed);
        if (!generated.world || !validateWorld(*generated.world).isValid()) {
            allValid = false;
            std::cerr << "World batch failed at seed " << seed << '\n';
            break;
        }
        maximumAcceptedAttempt = std::max(
            maximumAcceptedAttempt,
            generated.world->acceptedAttempt);
        const auto& rooms = generated.world->rooms;
        const auto& nodes = generated.world->graph.nodes;
        const float firstX = rooms[1].transform.position.x - rooms[0].transform.position.x;
        const float firstZ = rooms[1].transform.position.z - rooms[0].transform.position.z;
        firstDirections[std::abs(firstX) > std::abs(firstZ)
            ? (firstX > 0.0F ? 1U : 3U) : (firstZ > 0.0F ? 2U : 0U)] = true;
        std::size_t turns = 0U;
        for (std::size_t index = 2U; index < generated.world->graph.mainPath.size(); ++index) {
            const float previousX = rooms[index - 1U].transform.position.x
                - rooms[index - 2U].transform.position.x;
            const float currentX = rooms[index].transform.position.x
                - rooms[index - 1U].transform.position.x;
            turns += (std::abs(previousX) > 0.01F)
                != (std::abs(currentX) > 0.01F) ? 1U : 0U;
        }
        mainPathTurns = mainPathTurns && turns >= 1U && turns <= 2U;
        sawOneTurn = sawOneTurn || turns == 1U;
        sawTwoTurns = sawTwoTurns || turns == 2U;
        const auto hub = std::find_if(nodes.begin(), nodes.end(),
            [](const RoomNode& room) { return room.role == RoomRole::Hub; });
        const auto arena = std::find_if(nodes.begin(), nodes.end(),
            [](const RoomNode& room) { return room.role == RoomRole::Arena; });
        if (hub != nodes.end() && hub->id >= 2U && hub->id <= 5U) {
            hubPositions[hub->id - 2U] = true;
        }
        if (arena != nodes.end() && arena->id >= 2U && arena->id <= 5U) {
            arenaPositions[arena->id - 2U] = true;
        }
        const RoomNodeId secondBranchId
            = static_cast<RoomNodeId>(generated.world->graph.mainPath.size() + 2U);
        const auto secondBranchLink = std::find_if(
            generated.world->graph.edges.begin(), generated.world->graph.edges.end(),
            [secondBranchId](const MapEdge& edge) {
                return !edge.mainPath && edge.second == secondBranchId;
            });
        secondBranchAwayFromHub = secondBranchAwayFromHub
            && secondBranchLink != generated.world->graph.edges.end()
            && hub != nodes.end() && secondBranchLink->first != hub->id;
        const auto secretNode = std::find_if(
            generated.world->graph.nodes.begin(), generated.world->graph.nodes.end(),
            [](const RoomNode& node) { return node.role == RoomRole::Secret; });
        if (secretNode != generated.world->graph.nodes.end()) {
            const auto secretLink = std::find_if(
                generated.world->graph.edges.begin(), generated.world->graph.edges.end(),
                [secretNode](const MapEdge& edge) {
                    return !edge.mainPath && edge.second == secretNode->id;
                });
            if (secretLink != generated.world->graph.edges.end()) {
                secretAtHub += hub != nodes.end() && secretLink->first == hub->id ? 1U : 0U;
                secretFarther += hub != nodes.end() && secretLink->first != hub->id ? 1U : 0U;
            }
        }
        for (const RoomInstance& room : generated.world->rooms) {
            sawCompact = sawCompact || room.sizeClass == RoomSizeClass::Compact;
            sawMedium = sawMedium || room.sizeClass == RoomSizeClass::Medium;
            sawLarge = sawLarge || room.sizeClass == RoomSizeClass::Large;
            if ((room.role == RoomRole::Hub || room.role == RoomRole::Arena
                    || room.role == RoomRole::Boss)
                && room.sizeClass != RoomSizeClass::Large) {
                roleSizesValid = false;
            }
            if (room.role == RoomRole::Secret
                && room.sizeClass != RoomSizeClass::Compact) {
                roleSizesValid = false;
            }
        }
        for (const CorridorPath& corridor : generated.world->corridors) {
            float length = 0.0F;
            for (std::size_t index = 1U; index < corridor.points.size(); ++index) {
                length += std::abs(corridor.points[index].x - corridor.points[index - 1U].x)
                    + std::abs(corridor.points[index].z - corridor.points[index - 1U].z);
            }
            maximumCorridorLength = std::max(maximumCorridorLength, length);
        }
        if (seed < 64U) {
            gdp::entities::EntityManager manager;
            const InitialEntitySet initial = populateInitialEntities(
                *generated.world, manager,
                {.transform = {.position = {0.0F, 0.0F, 4.0F}}, .health = 100.0F});
            encountersValid = encountersValid && initial.isValid();
            std::size_t secretWeapons = 0U;
            for (const RoomInstance& room : generated.world->rooms) {
                std::size_t enemies = 0U;
                for (const gdp::entities::EntityId id : room.entities) {
                    const auto* enemy = manager.findData<gdp::entities::EnemyData>(id);
                    const auto* item = manager.findData<gdp::entities::ItemData>(id);
                    enemies += enemy != nullptr ? 1U : 0U;
                    secretWeapons += room.role == RoomRole::Secret && item != nullptr
                        && item->type == gdp::entities::ItemType::Weapon ? 1U : 0U;
                    if (room.role == RoomRole::Boss && enemy != nullptr) {
                        const float stepped = (enemy->maximumHealth - 360.0F) / 30.0F;
                        const int index = static_cast<int>(std::lround(stepped));
                        encountersValid = encountersValid && index >= 0 && index < 3
                            && std::abs(stepped - static_cast<float>(index)) < 1.0e-4F;
                        if (index >= 0 && index < 3) {
                            bossHealthValues[static_cast<std::size_t>(index)] = true;
                        }
                    }
                }
                if (room.role == RoomRole::Normal) {
                    encountersValid = encountersValid && enemies >= 1U && enemies <= 2U;
                    if (enemies >= 1U && enemies <= 2U) {
                        normalCounts[enemies - 1U] = true;
                    }
                } else if (room.role == RoomRole::Arena) {
                    encountersValid = encountersValid && enemies >= 2U && enemies <= 4U;
                    if (enemies >= 2U && enemies <= 4U) {
                        arenaCounts[enemies - 2U] = true;
                    }
                } else if (room.role == RoomRole::Boss) {
                    encountersValid = encountersValid && enemies == 1U;
                }
            }
            encountersValid = encountersValid && secretWeapons == 1U;
        }
    }
    expect(allValid, "1,000 deterministic seeds produce valid committed worlds");
    expect(maximumAcceptedAttempt < 32U, "batch stays inside configured attempt budget");
    expect(sawCompact && sawMedium && sawLarge && roleSizesValid,
        "seed batch exercises all size families and preserves role-specific sizing");
    expect(maximumCorridorLength <= 17.0F,
        "local loop policy prevents map-spanning corridors");
    expect(secondBranchAwayFromHub && secretAtHub > 0U
            && secretFarther > secretAtHub,
        "seed batch places the guaranteed secret weapon both at the Hub and elsewhere");
    expect(mainPathTurns && sawOneTurn && sawTwoTurns
            && std::all_of(firstDirections.begin(), firstDirections.end(), [](bool seen) { return seen; })
            && std::count(hubPositions.begin(), hubPositions.end(), true) >= 2
            && std::count(arenaPositions.begin(), arenaPositions.end(), true) >= 2,
        "seed batch varies route direction, turns and room-role order");
    expect(encountersValid
            && std::all_of(normalCounts.begin(), normalCounts.end(), [](bool seen) { return seen; })
            && std::all_of(arenaCounts.begin(), arenaCounts.end(), [](bool seen) { return seen; })
            && std::all_of(bossHealthValues.begin(), bossHealthValues.end(), [](bool seen) { return seen; }),
        "seed batch keeps a weapon and exercises all bounded enemy and boss variants");
}

} // namespace

int main()
{
    testModuleValidation();
    testDefaultModuleStructuralVariety();
    testInstantiationAndRotation();
    testConnectorContracts();
    testRequiredConnectorResolution();
    testMapGraph();
    testWorldGeneration();
    testCorridorRouting();
    testInitialEntityPopulation();
    testWorldSeedBatch();

    if (failures != 0) {
        std::cerr << failures << " world module test(s) failed\n";
        return 1;
    }

    std::cout << "All GDP world module tests passed\n";
    return 0;
}
