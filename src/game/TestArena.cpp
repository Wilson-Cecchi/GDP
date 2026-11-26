#include "game/TestArena.hpp"
#include "gameplay/Weapon.hpp"
#include "world/RoomArchitecture.hpp"
#include "world/RoomFootprint.hpp"
#include "world/PropGenerator.hpp"

#include <algorithm>
#include <array>
#include <tuple>
#include <utility>

namespace gdp::game {
world::World makeTestArena(const wkc::core::Seed seed)
{
    world::World arena;
    arena.masterSeed = seed; arena.generationSeed = seed;
    arena.graph.nodes.push_back({1U, world::RoomRole::Start, world::RoomTheme::Industrial, 0U});
    arena.graph.mainPath.push_back(1U);
    world::RoomInstance room;
    room.id = 1U; room.role = world::RoomRole::Start; room.seed = seed;
    room.sizeClass = world::RoomSizeClass::Large;
    room.worldBounds = {{-16.0F, 0.0F, -36.0F}, {16.0F, 5.0F, 12.0F}};
    arena.rooms.push_back(std::move(room));
    return arena;
}

world::World makeStorageRoomPreview(const wkc::core::Seed seed)
{
    world::World preview;
    preview.masterSeed = seed;
    preview.generationSeed = seed;
    preview.graph.nodes.push_back({
        1U, world::RoomRole::Start, world::RoomTheme::Storage, 0U});
    preview.graph.mainPath.push_back(1U);
    world::RoomInstance room;
    room.id = 1U;
    room.role = world::RoomRole::Start;
    room.theme = world::RoomTheme::Storage;
    room.seed = seed;
    room.sizeClass = world::RoomSizeClass::Large;
    room.worldBounds = {{-9.0F, 0.0F, -10.0F}, {9.0F, 6.0F, 10.0F}};
    preview.rooms.push_back(std::move(room));
    return preview;
}

world::InitialEntitySet populateStorageRoomPreview(
    world::World& preview,
    entities::EntityManager& manager,
    entities::PlayerCreateInfo player)
{
    using namespace entities;
    if (preview.rooms.size() != 1U || manager.activeCount() != 0U) {
        return {};
    }
    auto& room = preview.rooms.front();
    player.roomId = room.id;
    world::InitialEntitySet result;
    result.player = manager.createPlayer(player);
    // The terminal gives the existing Test Mode a valid return interaction.
    // Keep the warehouse floor clear for inspection of the architecture.
    result.terminal = manager.createProp({
        .transform = {.position = {0.0F, 0.0F, 8.5F},
            .scale = {1.15F, 1.80F, 0.60F}},
        .roomId = room.id, .type = PropType::Terminal, .solid = true});
    room.entities = {result.player, result.terminal};
    return result;
}

world::World makeCombatRoomPreview(const wkc::core::Seed seed)
{
    world::World preview;
    preview.masterSeed = seed;
    preview.generationSeed = seed;
    preview.graph.nodes.push_back({
        1U, world::RoomRole::Start, world::RoomTheme::Industrial, 0U});
    preview.graph.mainPath.push_back(1U);
    world::RoomInstance room;
    room.id = 1U;
    room.role = world::RoomRole::Start;
    room.theme = world::RoomTheme::Industrial;
    room.moduleId = world::combatRoomPreviewModuleId;
    room.seed = seed;
    room.sizeClass = world::RoomSizeClass::Large;
    room.worldBounds = {{-10.0F, 0.0F, -11.0F}, {10.0F, 6.0F, 11.0F}};
    // These pockets are reserved for the future encounter. The architecture
    // validator keeps cover and supports out of their occupied space.
    room.enemyZones = {
        {{{-0.8F, 0.0F, -8.4F}, {0.8F, 0.0F, -6.9F}}},
        {{{-0.8F, 0.0F, 6.9F}, {0.8F, 0.0F, 8.4F}}},
    };
    preview.rooms.push_back(std::move(room));
    return preview;
}

world::InitialEntitySet populateCombatRoomPreview(
    world::World& preview,
    entities::EntityManager& manager,
    entities::PlayerCreateInfo player)
{
    using namespace entities;
    if (preview.rooms.size() != 1U || manager.activeCount() != 0U) {
        return {};
    }
    auto& room = preview.rooms.front();
    player.roomId = room.id;
    world::InitialEntitySet result;
    result.player = manager.createPlayer(player);
    result.terminal = manager.createProp({
        .transform = {.position = {0.0F, 0.0F, 9.5F},
            .scale = {1.15F, 1.80F, 0.60F}},
        .roomId = room.id, .type = PropType::Terminal, .solid = true});
    room.entities = {result.player, result.terminal};
    return result;
}

world::World makeBossRoomPreview(const wkc::core::Seed seed)
{
    world::World preview;
    preview.masterSeed = seed;
    preview.generationSeed = seed;
    preview.graph.nodes.push_back({
        1U, world::RoomRole::Start, world::RoomTheme::Maintenance, 0U});
    preview.graph.mainPath.push_back(1U);
    const auto library = world::makeDefaultRoomModuleLibrary();
    const auto module = std::find_if(library.begin(), library.end(),
        [](const world::RoomModule& candidate) { return candidate.id == 504U; });
    if (module == library.end()) return preview;
    auto room = world::instantiateRoom(*module, 1U, world::RoomRole::Boss,
        seed, {}, wkc::core::QuarterTurn::Degrees0);
    if (!room) return preview;
    room->role = world::RoomRole::Start;
    room->moduleId = world::bossRoomPreviewModuleId;
    preview.rooms.push_back(std::move(*room));
    return preview;
}

world::InitialEntitySet populateBossRoomPreview(
    world::World& preview,
    entities::EntityManager& manager,
    entities::PlayerCreateInfo player)
{
    using namespace entities;
    if (preview.rooms.size() != 1U || manager.activeCount() != 0U) return {};
    auto& room = preview.rooms.front();
    const wkc::core::Seed seed = preview.masterSeed;
    player.roomId = room.id;
    world::InitialEntitySet result;
    result.player = manager.createPlayer(player);
    result.terminal = manager.createProp({
        .transform = {.position = {0.0F, 0.0F, 2.2F},
            .scale = {1.15F, 1.80F, 0.60F}},
        .roomId = room.id, .type = PropType::Terminal, .solid = true});
    room.entities = {result.player, result.terminal};
    room.entities.push_back(manager.createEnemy({
        .transform = {.position = {0.0F, 0.0F, 0.0F}},
        .roomId = room.id, .archetype = EnemyArchetype::IndustrialRobot,
        .health = 741.0F, .visualSeed = seed + 1U}));
    room.entities.push_back(manager.createEnemy({
        .transform = {.position = {-5.0F, 0.0F, -4.0F}},
        .roomId = room.id, .archetype = EnemyArchetype::Officer,
        .health = 65.0F, .visualSeed = seed + 2U}));
    room.entities.push_back(manager.createEnemy({
        .transform = {.position = {-5.0F, 0.0F, 4.0F}},
        .roomId = room.id, .archetype = EnemyArchetype::Guard,
        .health = 75.0F, .visualSeed = seed + 3U}));
    for (const auto& [z, scale, health, ordinal] : {
             std::tuple{-8.5F, wkc::core::Vec3{2.4F, 1.35F, 0.6F}, 120.0F, 901U},
             std::tuple{8.5F, wkc::core::Vec3{0.6F, 2.25F, 2.4F}, 180.0F, 902U}}) {
        room.entities.push_back(manager.createProp({
            .transform = {.position = {-8.5F, 0.0F, z}, .scale = scale},
            .roomId = room.id, .type = PropType::Barricade, .solid = true,
            .health = health, .maximumHealth = health,
            .visualSeed = seed + ordinal}));
    }
    const auto props = world::generatePropLayout(
        room.seed, world::RoomRole::Boss, room.theme, 5U, room.propZones, 1.0F);
    for (const world::PropSpawn& prop : props.props) {
        room.entities.push_back(manager.createProp({
            .transform = {.position = prop.position, .scale = prop.scale},
            .roomId = room.id, .type = prop.type, .state = prop.state,
            .solid = prop.solid, .health = prop.health,
            .maximumHealth = prop.maximumHealth,
            .dropType = prop.dropType, .dropQuantity = prop.dropQuantity,
            .visualSeed = prop.visualSeed}));
    }
    return result;
}

world::World makeCafeteriaPreview(const wkc::core::Seed seed)
{
    world::World preview;
    preview.masterSeed = seed;
    preview.generationSeed = seed;
    preview.graph.nodes.push_back({
        1U, world::RoomRole::Start, world::RoomTheme::Cafeteria, 0U});
    preview.graph.mainPath.push_back(1U);
    world::RoomInstance room;
    room.id = 1U;
    room.role = world::RoomRole::Start;
    room.theme = world::RoomTheme::Cafeteria;
    room.moduleId = world::cafeteriaPreviewModuleId;
    room.seed = seed;
    room.sizeClass = world::RoomSizeClass::Large;
    room.worldBounds = {{-9.0F, 0.0F, -10.0F}, {9.0F, 6.0F, 10.0F}};
    room.enemyZones = {
        {{{-0.8F, 0.0F, -6.5F}, {0.8F, 0.0F, -5.3F}}},
        {{{-0.8F, 0.0F, 5.3F}, {0.8F, 0.0F, 6.5F}}},
    };
    preview.rooms.push_back(std::move(room));
    return preview;
}

world::World makeCafeteriaLongPreview(const wkc::core::Seed seed)
{
    world::World preview = makeCafeteriaPreview(seed);
    auto& room = preview.rooms.front();
    room.moduleId = world::cafeteriaLongPreviewModuleId;
    room.worldBounds.minimum.z = -12.0F;
    room.worldBounds.maximum.z = 12.0F;
    room.enemyZones = {
        {{{-0.8F, 0.0F, -9.7F}, {0.8F, 0.0F, -8.8F}}},
        {{{-0.8F, 0.0F, 8.8F}, {0.8F, 0.0F, 9.7F}}},
    };
    return preview;
}

world::InitialEntitySet populateCafeteriaPreview(
    world::World& preview,
    entities::EntityManager& manager,
    entities::PlayerCreateInfo player)
{
    using namespace entities;
    if (preview.rooms.size() != 1U || manager.activeCount() != 0U) {
        return {};
    }
    auto& room = preview.rooms.front();
    player.roomId = room.id;
    world::InitialEntitySet result;
    result.player = manager.createPlayer(player);
    result.terminal = manager.createProp({
        .transform = {.position = {0.0F, 0.0F,
            room.moduleId == world::cafeteriaLongPreviewModuleId ? 10.5F : 8.5F},
            .scale = {1.15F, 1.80F, 0.60F}},
        .roomId = room.id, .type = PropType::Terminal, .solid = true});
    room.entities = {result.player, result.terminal};
    return result;
}

world::World makeLaboratoryPreview(const wkc::core::Seed seed)
{
    world::World preview;
    preview.masterSeed = seed;
    preview.generationSeed = seed;
    preview.graph.nodes.push_back({
        1U, world::RoomRole::Start, world::RoomTheme::Laboratory, 0U});
    preview.graph.mainPath.push_back(1U);
    world::RoomInstance room;
    room.id = 1U;
    room.role = world::RoomRole::Start;
    room.theme = world::RoomTheme::Laboratory;
    room.moduleId = world::laboratoryPreviewModuleId;
    room.seed = seed;
    room.sizeClass = world::RoomSizeClass::Large;
    room.worldBounds = {{-9.0F, 0.0F, -10.0F}, {9.0F, 6.0F, 10.0F}};
    room.enemyZones = {
        {{{-0.8F, 0.0F, -6.5F}, {0.8F, 0.0F, -5.3F}}},
        {{{-0.8F, 0.0F, 5.3F}, {0.8F, 0.0F, 6.5F}}},
    };
    preview.rooms.push_back(std::move(room));
    return preview;
}

world::InitialEntitySet populateLaboratoryPreview(
    world::World& preview,
    entities::EntityManager& manager,
    entities::PlayerCreateInfo player)
{
    using namespace entities;
    if (preview.rooms.size() != 1U || manager.activeCount() != 0U) {
        return {};
    }
    auto& room = preview.rooms.front();
    player.roomId = room.id;
    world::InitialEntitySet result;
    result.player = manager.createPlayer(player);
    result.terminal = manager.createProp({
        .transform = {.position = {0.0F, 0.0F, 8.5F},
            .scale = {1.15F, 1.80F, 0.60F}},
        .roomId = room.id, .type = PropType::Terminal, .solid = true});
    room.entities = {result.player, result.terminal};
    return result;
}

world::World makeTechnicalPreview(const wkc::core::Seed seed)
{
    world::World preview;
    preview.masterSeed = seed;
    preview.generationSeed = seed;
    preview.graph.nodes.push_back({
        1U, world::RoomRole::Start, world::RoomTheme::Maintenance, 0U});
    preview.graph.mainPath.push_back(1U);
    world::RoomInstance room;
    room.id = 1U;
    room.role = world::RoomRole::Start;
    room.theme = world::RoomTheme::Maintenance;
    room.moduleId = world::technicalPreviewModuleId;
    room.seed = seed;
    room.sizeClass = world::RoomSizeClass::Large;
    room.worldBounds = {{-8.0F, 0.0F, -10.0F}, {8.0F, 5.5F, 10.0F}};
    room.enemyZones = {
        {{{-0.8F, 0.0F, -6.5F}, {0.8F, 0.0F, -5.3F}}},
        {{{-0.8F, 0.0F, 5.3F}, {0.8F, 0.0F, 6.5F}}},
    };
    room.propZones = {
        {{{3.0F, 0.0F, -0.8F}, {4.0F, 0.0F, 0.8F}}},
    };
    room.lootZones = {
        {{{-4.0F, 0.0F, -0.8F}, {-3.0F, 0.0F, 0.8F}}},
    };
    preview.rooms.push_back(std::move(room));
    return preview;
}

world::World makeTechnicalLPreview(const wkc::core::Seed seed)
{
    world::World preview = makeTechnicalPreview(seed);
    auto& room = preview.rooms.front();
    room.moduleId = world::technicalLPreviewModuleId;
    // The north-east corner is removed; the inner two walls form the L.
    room.walkableOutline = world::technicalLPreviewFootprint();
    return preview;
}

world::InitialEntitySet populateTechnicalPreview(
    world::World& preview,
    entities::EntityManager& manager,
    entities::PlayerCreateInfo player)
{
    using namespace entities;
    if (preview.rooms.size() != 1U || manager.activeCount() != 0U) {
        return {};
    }
    auto& room = preview.rooms.front();
    player.roomId = room.id;
    world::InitialEntitySet result;
    result.player = manager.createPlayer(player);
    result.terminal = manager.createProp({
        .transform = {.position = {0.0F, 0.0F, 8.5F},
            .scale = {1.15F, 1.80F, 0.60F}},
        .roomId = room.id, .type = PropType::Terminal, .solid = true});
    room.entities = {result.player, result.terminal};
    return result;
}

world::World makeSecurityPreview(const wkc::core::Seed seed)
{
    world::World preview;
    preview.masterSeed = seed;
    preview.generationSeed = seed;
    preview.graph.nodes.push_back({
        1U, world::RoomRole::Start, world::RoomTheme::Security, 0U});
    preview.graph.mainPath.push_back(1U);
    world::RoomInstance room;
    room.id = 1U;
    room.role = world::RoomRole::Start;
    room.theme = world::RoomTheme::Security;
    room.moduleId = world::securityPreviewModuleId;
    room.seed = seed;
    room.sizeClass = world::RoomSizeClass::Large;
    room.worldBounds = {{-9.0F, 0.0F, -9.0F}, {9.0F, 6.0F, 9.0F}};
    room.enemyZones = {
        {{{-0.8F, 0.0F, -6.4F}, {0.8F, 0.0F, -5.4F}}},
        {{{-0.8F, 0.0F, 5.4F}, {0.8F, 0.0F, 6.4F}}},
    };
    room.propZones = {
        {{{3.0F, 0.0F, -0.8F}, {4.0F, 0.0F, 0.8F}}},
    };
    room.lootZones = {
        {{{-4.0F, 0.0F, -0.8F}, {-3.0F, 0.0F, 0.8F}}},
    };
    preview.rooms.push_back(std::move(room));
    return preview;
}

world::World makeSecurityTPreview(const wkc::core::Seed seed)
{
    world::World preview = makeSecurityPreview(seed);
    auto& room = preview.rooms.front();
    room.moduleId = world::securityTPreviewModuleId;
    room.worldBounds = {{-10.0F, 0.0F, -10.0F}, {10.0F, 6.0F, 10.0F}};
    room.walkableOutline = world::securityTPreviewFootprint();
    return preview;
}

world::InitialEntitySet populateSecurityPreview(
    world::World& preview,
    entities::EntityManager& manager,
    entities::PlayerCreateInfo player)
{
    using namespace entities;
    if (preview.rooms.size() != 1U || manager.activeCount() != 0U) {
        return {};
    }
    auto& room = preview.rooms.front();
    player.roomId = room.id;
    world::InitialEntitySet result;
    result.player = manager.createPlayer(player);
    result.terminal = manager.createProp({
        .transform = {.position = {0.0F, 0.0F, 7.5F},
            .scale = {1.15F, 1.80F, 0.60F}},
        .roomId = room.id, .type = PropType::Terminal, .solid = true});
    room.entities = {result.player, result.terminal};
    return result;
}

world::World makeCentralHubPreview(const wkc::core::Seed seed)
{
    world::World preview;
    preview.masterSeed = seed;
    preview.generationSeed = seed;
    preview.graph.nodes.push_back({
        1U, world::RoomRole::Start, world::RoomTheme::Industrial, 0U});
    preview.graph.mainPath.push_back(1U);
    world::RoomInstance room;
    room.id = 1U;
    room.role = world::RoomRole::Start;
    room.theme = world::RoomTheme::Industrial;
    room.moduleId = world::centralHubPreviewModuleId;
    room.seed = seed;
    room.sizeClass = world::RoomSizeClass::Large;
    room.worldBounds = {{-10.0F, 0.0F, -10.0F},
        {10.0F, 6.0F, 10.0F}};
    // Eight equal faces, with the cardinal four kept flat for future doors.
    room.walkableOutline = world::centralHubPreviewFootprint();
    room.enemyZones = {
        {{{-6.0F, 0.0F, -6.0F}, {-5.0F, 0.0F, -5.0F}}},
        {{{5.0F, 0.0F, 5.0F}, {6.0F, 0.0F, 6.0F}}},
    };
    room.propZones = {
        {{{5.0F, 0.0F, -1.0F}, {6.0F, 0.0F, 1.0F}}},
    };
    room.lootZones = {
        {{{-6.0F, 0.0F, -1.0F}, {-5.0F, 0.0F, 1.0F}}},
    };
    preview.rooms.push_back(std::move(room));
    return preview;
}

world::InitialEntitySet populateCentralHubPreview(
    world::World& preview,
    entities::EntityManager& manager,
    entities::PlayerCreateInfo player,
    const bool shadowPilot)
{
    using namespace entities;
    if (preview.rooms.size() != 1U || manager.activeCount() != 0U) {
        return {};
    }
    auto& room = preview.rooms.front();
    player.roomId = room.id;
    world::InitialEntitySet result;
    result.player = manager.createPlayer(player);
    result.terminal = manager.createProp({
        .transform = {.position = {0.0F, 0.0F, 8.5F},
            .scale = {1.15F, 1.80F, 0.60F}},
        .roomId = room.id, .type = PropType::Terminal, .solid = true});
    room.entities = {result.player, result.terminal};
    if (shadowPilot) {
        room.entities.push_back(manager.createEnemy({
            .transform = {.position = {-5.2F, 0.0F, -4.0F}},
            .roomId = room.id, .archetype = EnemyArchetype::Worker,
            .health = 75.0F, .visualSeed = preview.masterSeed + 7U}));
        room.entities.push_back(manager.createProp({
            .transform = {.position = {4.5F, 0.0F, -1.5F},
                .scale = {1.8F, 1.25F, 0.55F}},
            .roomId = room.id, .type = PropType::Barricade,
            .solid = true, .health = 120.0F, .maximumHealth = 120.0F,
            .visualSeed = preview.masterSeed + 8U}));
    }
    return result;
}

world::World makeIndustrialCorridorPreview(const wkc::core::Seed seed)
{
    world::World preview;
    preview.masterSeed = seed;
    preview.generationSeed = seed;

    // The world validator requires the main path to end in a Boss node.
    // This role only makes the two-room preview structurally valid; the
    // population below deliberately creates no enemy.
    preview.graph.nodes = {
        {1U, world::RoomRole::Start, world::RoomTheme::Industrial, 0U},
        {2U, world::RoomRole::Boss, world::RoomTheme::Industrial, 1U},
    };
    preview.graph.edges = {{1U, 1U, 2U, true}};
    preview.graph.mainPath = {1U, 2U};

    world::RoomModule module = world::makeFixedRoomModule();
    module.roleCapabilities |= world::roomRoleBit(world::RoomRole::Boss);
    for (auto& door : module.connectors) {
        if (door.id == 1U || door.id == 2U) {
            door.width = 2.4F;
            door.height = 2.8F;
        }
    }
    auto first = world::instantiateRoom(module, 1U, world::RoomRole::Start,
        seed, {}, wkc::core::QuarterTurn::Degrees0);
    auto second = world::instantiateRoom(module, 2U, world::RoomRole::Boss,
        seed, {0.0F, 0.0F, -20.0F}, wkc::core::QuarterTurn::Degrees0);
    if (!first || !second) {
        return preview;
    }

    // Both compact preview rooms leave the widened doorway clear. Campaign
    // architecture may reserve a narrower opening in this fixed module.
    first->moduleId = world::industrialCorridorStartPreviewModuleId;
    second->moduleId = world::industrialCorridorDestinationPreviewModuleId;

    preview.rooms.push_back(std::move(*first));
    preview.rooms.push_back(std::move(*second));
    auto& firstConnectors = preview.rooms.front().connectors;
    auto& secondConnectors = preview.rooms.back().connectors;
    const auto firstDoor = std::find_if(firstConnectors.begin(),
        firstConnectors.end(), [](const world::ConnectorInstance& door) {
            return door.definitionId == 2U;
        });
    const auto secondDoor = std::find_if(secondConnectors.begin(),
        secondConnectors.end(), [](const world::ConnectorInstance& door) {
            return door.definitionId == 1U;
        });
    if (firstDoor == firstConnectors.end()
        || secondDoor == secondConnectors.end()
        || !world::assignEdge(*firstDoor, 1U)
        || !world::assignEdge(*secondDoor, 1U)) {
        return preview;
    }

    // The wide passage needs stubs longer than half its width so the
    // center segment clears both room bounds during corridor validation.
    const auto route = world::routeCorridor(1U, 1U, 2U,
        *firstDoor, *secondDoor, preview.rooms, 1.5F);
    if (route) {
        preview.corridors.push_back(*route);
    }
    return preview;
}

world::InitialEntitySet populateIndustrialCorridorPreview(
    world::World& preview,
    entities::EntityManager& manager,
    entities::PlayerCreateInfo player)
{
    using namespace entities;
    if (preview.rooms.size() != 2U || preview.corridors.size() != 1U
        || manager.activeCount() != 0U) {
        return {};
    }
    auto& start = preview.rooms.front();
    player.roomId = start.id;
    world::InitialEntitySet result;
    result.player = manager.createPlayer(player);
    result.terminal = manager.createProp({
        .transform = {.position = {2.4F, 0.0F, 2.0F},
            .scale = {1.15F, 1.80F, 0.60F}},
        .roomId = start.id, .type = PropType::Terminal, .solid = true});
    start.entities = {result.player, result.terminal};
    return result;
}

world::InitialEntitySet populateTestArena(world::World& arena, entities::EntityManager& manager, entities::PlayerCreateInfo player)
{
    using namespace entities;
    if (arena.rooms.size() != 1U || manager.activeCount() != 0U) return {};
    auto& room = arena.rooms.front();
    player.roomId = room.id;
    world::InitialEntitySet result;
    result.player = manager.createPlayer(player);
    result.terminal = manager.createProp({
        .transform = {.position = {0.0F, 0.0F, 8.0F},
            .scale = {1.15F, 1.80F, 0.60F}},
        .roomId = 1U, .type = PropType::Terminal, .solid = true});
    room.entities = {result.player, result.terminal};
    // Two tall bulkheads shelter the weapon pickups and target lane from the
    // robot's long-range cannon. The four-metre center gap stays walkable.
    for (const float x : {-9.0F, 9.0F}) {
        room.entities.push_back(manager.createProp({
            .transform = {.position = {x, 0.0F, -18.0F},
                .scale = {14.0F, 3.5F, 1.5F}},
            .roomId = room.id, .type = PropType::Barricade, .solid = true,
            .health = 1'000'000.0F, .maximumHealth = 1'000'000.0F,
            .visualSeed = arena.masterSeed + (x < 0.0F ? 50U : 51U)}));
    }
    for (unsigned i = 0U; i < 5U; ++i) {
        room.entities.push_back(manager.createEnemy({
            .transform = {.position = {-10.0F + static_cast<float>(i) * 5.0F, 0.0F, -28.0F}},
            .roomId = 1U, .archetype = static_cast<EnemyArchetype>(i),
            .health = i == 4U ? 260.0F : 80.0F, .visualSeed = arena.masterSeed + i}));
    }
    for (unsigned i = 0U; i < 4U; ++i) {
        room.entities.push_back(manager.createProp({
            .transform = {.position = {-9.0F + static_cast<float>(i) * 6.0F, 0.0F, -6.0F},
                .scale = {2.0F, i % 2U == 0U ? 2.5F : 0.6F, 2.0F}},
            .roomId = 1U, .type = PropType::Barricade, .solid = true,
            .health = 2000.0F, .maximumHealth = 2000.0F, .visualSeed = i + 10U}));
        room.entities.push_back(manager.createItem({.transform = {.position = {-6.0F + static_cast<float>(i) * 4.0F, 0.0F, 7.0F}},
            .roomId = 1U, .type = i == 0U ? ItemType::Health : ItemType::Ammo, .quantity = 100U}));
    }
    for (unsigned i = 0U; i < 3U; ++i) {
        const auto id = manager.createItem({
            .transform = {.position = {-4.0F + static_cast<float>(i) * 4.0F, 0.0F, 0.0F}},
            .roomId = room.id, .type = ItemType::Weapon, .quantity = 1U});
        manager.findData<ItemData>(id)->spawnedWeaponArchetype
            = static_cast<std::uint32_t>(gameplay::WeaponArchetype::Sniper) + i;
        room.entities.push_back(id);
    }
    // Durable plates remain on the preparation side of the divider.
    // The existing canister role gives them a red metallic hit material.
    constexpr std::array<float, 3> targetDepths{1.0F, -7.0F, -14.0F};
    for (unsigned i = 0U; i < targetDepths.size(); ++i) {
        room.entities.push_back(manager.createProp({
            .transform = {.position = {14.5F, 0.0F, targetDepths[i]},
                .scale = {0.30F, 1.8F, 1.6F}},
            .roomId = room.id, .type = PropType::Canister, .solid = true,
            .health = 1'000'000.0F, .maximumHealth = 1'000'000.0F,
            .visualSeed = arena.masterSeed + 40U + i}));
    }
    return result;
}
}
