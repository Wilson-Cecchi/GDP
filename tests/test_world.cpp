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
    expect(firstIsBranch != secondIsBranch
            && (mainEndpoint == 1U || mainEndpoint == 3U),
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
    expect(fingerprint == 0xFEF4A385CE034FE6ULL,
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
    expect(initial.isValid() && manager.activeCount() == 40,
        "eight-room world receives its complete initial entity set");
    expect(manager.query(EntityKind::Player) == std::vector<EntityId>{initial.player},
        "population creates exactly one player entity");
    const PropData* terminal = manager.findData<PropData>(initial.terminal);
    expect(terminal != nullptr && terminal->type == PropType::Terminal && terminal->solid,
        "START room receives the solid interaction terminal");

    bool arenaGuardValid = false;
    bool arenaOfficerValid = false;
    bool arenaEngineerValid = false;
    bool bossRobotValid = false;
    for (const RoomInstance& room : generated.world->rooms) {
        for (const EntityId id : room.entities) {
            const EnemyData* enemy = manager.findData<EnemyData>(id);
            arenaGuardValid = arenaGuardValid || (room.role == RoomRole::Arena
                && enemy != nullptr
                && enemy->archetype == EnemyArchetype::Guard
                && enemy->health == 75.0F && enemy->maximumHealth == 75.0F
                && enemy->visualSeed != 0U);
            arenaOfficerValid = arenaOfficerValid || (room.role == RoomRole::Arena
                && enemy != nullptr
                && enemy->archetype == EnemyArchetype::Officer
                && enemy->health == 65.0F && enemy->maximumHealth == 65.0F
                && enemy->visualSeed != 0U);
            arenaEngineerValid = arenaEngineerValid || (room.role == RoomRole::Arena
                && enemy != nullptr
                && enemy->archetype == EnemyArchetype::Engineer
                && enemy->health == 58.0F && enemy->maximumHealth == 58.0F
                && enemy->visualSeed != 0U);
            bossRobotValid = bossRobotValid || (room.role == RoomRole::Boss
                && enemy != nullptr
                && enemy->archetype == EnemyArchetype::IndustrialRobot
                && enemy->health == 250.0F && enemy->maximumHealth == 250.0F
                && enemy->visualSeed != 0U);
        }
    }
    expect(arenaGuardValid,
        "Arena population includes one healthy Guard with an isolated visual seed");
    expect(arenaOfficerValid,
        "Arena population includes one healthy Officer with an isolated visual seed");
    expect(arenaEngineerValid,
        "Arena population includes one healthy Engineer with an isolated visual seed");
    expect(bossRobotValid,
        "Boss population includes one healthy IndustrialRobot with an isolated visual seed");

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
        const std::size_t baseCount = room.role == RoomRole::Arena ? 4U
            : room.role == RoomRole::Start ? 3U
            : room.role == RoomRole::Normal ? 1U
            : room.role == RoomRole::Boss ? 2U : 0U;
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
    for (wkc::core::Seed seed = 0; seed < 1'000; ++seed) {
        const WorldGenerationResult generated = generateWorld(seed);
        if (!generated.world || !validateWorld(*generated.world).isValid()) {
            allValid = false;
            std::cerr << "World batch failed at seed " << seed << '\n';
            break;
        }
        maximumAcceptedAttempt = std::max(
            maximumAcceptedAttempt,
            generated.world->acceptedAttempt);
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
    }
    expect(allValid, "1,000 deterministic seeds produce valid committed worlds");
    expect(maximumAcceptedAttempt < 32U, "batch stays inside configured attempt budget");
    expect(sawCompact && sawMedium && sawLarge && roleSizesValid,
        "seed batch exercises all size families and preserves role-specific sizing");
    expect(maximumCorridorLength <= 17.0F,
        "local loop policy prevents map-spanning corridors");
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
