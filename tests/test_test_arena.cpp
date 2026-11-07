#include "game/TestArena.hpp"

#include "gameplay/Combat.hpp"
#include "gameplay/Enemy.hpp"
#include "gameplay/Props.hpp"
#include "gameplay/RoomTraversal.hpp"
#include "world/RoomArchitecture.hpp"
#include "world/RoomFootprint.hpp"
#include "world/WorldValidation.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <vector>

namespace {

int failures = 0;

void expect(const bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void testPracticeAreaAndBossApproach()
{
    using namespace gdp;
    using namespace gdp::entities;
    using namespace gdp::gameplay;
    for (const wkc::core::Vec3 playerPosition : std::array{
             wkc::core::Vec3{0.0F, 0.0F, 0.0F},
             wkc::core::Vec3{4.0F, 0.0F, 0.0F},
             wkc::core::Vec3{12.0F, 0.0F, -12.0F}}) {
        auto arena = game::makeTestArena(12345U);
        EntityManager manager;
        const auto initial = game::populateTestArena(arena, manager, {
            .transform = {.position = playerPosition}, .health = 100.0F});
        expect(initial.isValid(), "test arena populates");
        const CollisionWorld collision = collisionWorldWithProps(
            CollisionWorld{arena.rooms.front().worldBounds}, manager, 1U);
        expect(collision.canOccupy(playerPosition),
            "preparation and target areas remain walkable");
        Player player;
        player.spawn(playerPosition);
        std::vector<EnemyProjectile> shots;
        const EnemyPerception perception{.projectiles = &shots};
        for (int tick = 0; tick < 600; ++tick) {
            (void)updateIndustrialRobots(manager, player, 1U, collision,
                1.0F / 60.0F, {}, perception);
        }
        expect(shots.empty(),
            "boss cannot cannon the preparation and target areas from afar");
    }

    auto arena = game::makeTestArena(54321U);
    EntityManager manager;
    const auto initial = game::populateTestArena(arena, manager, {
        .transform = {.position = {0.0F, 0.0F, 0.0F}}, .health = 100.0F});
    expect(initial.isValid(), "boss approach arena populates");
    const CollisionWorld collision = collisionWorldWithProps(
        CollisionWorld{arena.rooms.front().worldBounds}, manager, 1U);
    expect(collision.canOccupy({0.0F, 0.0F, -18.0F})
            && collision.canOccupy({0.0F, 0.0F, -23.0F}),
        "central passage connects preparation and combat zones");
    Player practicePlayer;
    practicePlayer.spawn({0.0F, 0.0F, 0.0F});
    std::size_t targetCount = 0U;
    for (const EntityId id : manager.query(EntityKind::Prop)) {
        const Entity* entity = manager.find(id);
        const PropData* prop = manager.findData<PropData>(id);
        if (entity == nullptr || prop == nullptr
            || prop->type != PropType::Canister
            || prop->maximumHealth != 1'000'000.0F) {
            continue;
        }
        ++targetCount;
        Weapon gun(baseWeaponDefinition(WeaponArchetype::Carbine));
        const auto origin = practicePlayer.cameraPosition();
        const auto direction = wkc::core::normalized(
            entity->transform.position + wkc::core::Vec3{0.0F, 0.9F, 0.0F}
            - origin);
        const ShotResult shot = fireHitscan(gun, origin, direction,
            1U, collision, manager);
        expect(shot.hit && shot.hit->target == id,
            "each practice target remains reachable from the safe area");
    }
    expect(targetCount == 3U, "test arena retains three practice targets");
    Player player;
    player.spawn({0.0F, 0.0F, -23.0F});
    std::vector<EnemyProjectile> shots;
    const EnemyPerception perception{.projectiles = &shots};
    for (int tick = 0; tick < 120 && shots.empty(); ++tick) {
        (void)updateIndustrialRobots(manager, player, 1U, collision,
            1.0F / 60.0F, {}, perception);
    }
    expect(!shots.empty(),
        "boss can still attack after the player enters the combat zone");
}

void testBossRoomPreview()
{
    using namespace gdp;
    using namespace gdp::entities;
    auto preview = game::makeBossRoomPreview(20261101U);
    expect(preview.rooms.size() == 1U,
        "Boss preview uses one directly accessible arena");
    if (preview.rooms.size() != 1U) return;
    const auto& room = preview.rooms.front();
    const auto span = room.worldBounds.maximum - room.worldBounds.minimum;
    const auto architecture = world::makeRoomArchitecture(room);
    expect(room.moduleId == world::bossRoomPreviewModuleId
            && span.x > 27.0F && span.z > 31.0F && span.y == 7.0F
            && architecture.size() == 8U
            && world::validateRoomArchitecture(room, architecture),
        "Boss preview matches the campaign arena and its two columns");

    EntityManager manager;
    const auto initial = game::populateBossRoomPreview(preview, manager,
        {.transform = {.position = {0.0F, 0.0F, 13.0F}}, .health = 100.0F});
    std::size_t bossCount = 0U;
    std::size_t aideCount = 0U;
    for (const EntityId id : manager.query(EntityKind::Enemy)) {
        const auto* enemy = manager.findData<EnemyData>(id);
        bossCount += enemy && enemy->archetype == EnemyArchetype::IndustrialRobot;
        aideCount += enemy && enemy->archetype != EnemyArchetype::IndustrialRobot;
    }
    const auto collision = gameplay::collisionWorldWithProps(
        gameplay::makeRoomNavigationCollision(preview, room.id), manager, room.id);
    expect(initial.isValid() && bossCount == 1U && aideCount == 2U
            && manager.query(EntityKind::Prop).size() >= 5U
            && collision.canOccupy({0.0F, 0.0F, 13.0F})
            && !collision.canOccupy({7.0F, 0.0F, -8.0F})
            && !collision.canOccupy({7.0F, 0.0F, 8.0F}),
        "Boss preview starts clear with its combatants and solid columns");
}

void testStorageRoomPreview()
{
    using namespace gdp;
    using namespace gdp::entities;
    using namespace gdp::gameplay;
    auto preview = game::makeStorageRoomPreview(20261027U);
    expect(preview.rooms.size() == 1U
            && preview.rooms.front().theme == world::RoomTheme::Storage
            && preview.rooms.front().role == world::RoomRole::Start
            && preview.rooms.front().worldBounds.maximum.y == 6.0F,
        "Storage preview opens the requested tall warehouse room directly");
    EntityManager manager;
    const auto initial = game::populateStorageRoomPreview(preview, manager, {
        .transform = {.position = {0.0F, 0.0F, 4.0F}},
        .health = 100.0F,
    });
    expect(initial.isValid()
            && manager.query(EntityKind::Enemy).empty(),
        "Storage preview has a valid player and no combat distractions");
    const CollisionWorld navigation = makeRoomNavigationCollision(
        preview, preview.rooms.front().id, false);
    expect(navigation.canOccupy({0.0F, 0.0F, 4.0F})
            && navigation.canOccupy({0.0F, 0.0F, -7.0F}),
        "the preview player can traverse the open warehouse center");
    expect(!navigation.canOccupy({-3.5F, 0.0F, 5.55F})
            && !navigation.canOccupy({3.5F, 0.0F, -5.55F})
            && navigation.canOccupy({-1.5F, 0.0F, 5.55F})
            && navigation.canOccupy({5.0F, 0.0F, -5.55F}),
        "both interior shelves block movement while adjacent aisles remain open");
    const auto parts = world::makeRoomArchitecture(preview.rooms.front());
    expect(world::validateRoomArchitecture(preview.rooms.front(), parts)
            && std::count_if(parts.begin(), parts.end(),
                [](const world::RoomArchitecturePart& part) {
                    return part.decoration
                        == world::RoomDecorationPart::CeilingDuct;
                }) == 1,
        "Storage Hall overhead duct preserves reserved gameplay space");
    const auto rack = std::find_if(parts.begin(), parts.end(),
        [](const world::RoomArchitecturePart& part) {
            return part.solid && part.bounds.minimum.y == 0.0F;
        });
    if (rack != parts.end()) {
        const wkc::core::Vec3 center
            = (rack->bounds.minimum + rack->bounds.maximum) * 0.5F;
        expect(!navigation.canOccupy({center.x, 0.0F, center.z}),
            "visible Storage rack structure also blocks movement");
    } else {
        expect(false, "Storage preview contains solid rack structure");
    }
}

void testCombatRoomPreview()
{
    using namespace gdp;
    using namespace gdp::entities;
    using namespace gdp::gameplay;
    auto preview = game::makeCombatRoomPreview(20261028U);
    const auto& room = preview.rooms.front();
    expect(preview.rooms.size() == 1U
            && room.moduleId == world::combatRoomPreviewModuleId
            && room.worldBounds.maximum.x == 10.0F
            && room.worldBounds.maximum.y == 6.0F
            && room.worldBounds.maximum.z == 11.0F,
        "Combat preview opens one 20 by 22 by 6 metre room");
    const auto parts = world::makeRoomArchitecture(room);
    expect(!parts.empty() && world::validateRoomArchitecture(room, parts),
        "Combat architecture clears reserved enemy pockets");
    EntityManager manager;
    const auto initial = game::populateCombatRoomPreview(preview, manager, {
        .transform = {.position = {0.0F, 0.0F, 4.0F}},
        .health = 100.0F,
    });
    expect(initial.isValid() && manager.query(EntityKind::Enemy).empty(),
        "Combat preview can be inspected without encounter spawns");
    const CollisionWorld navigation = makeRoomNavigationCollision(
        preview, room.id, false);
    expect(navigation.canOccupy({0.0F, 0.0F, 4.0F})
            && navigation.canOccupy({0.0F, 0.0F, -8.0F})
            && navigation.canOccupy({5.0F, 0.0F, 4.0F})
            && navigation.canOccupy({-5.0F, 0.0F, -4.0F}),
        "center and flanks remain traversable");
    expect(!navigation.canOccupy({3.2F, 0.0F, -3.6F})
            && !navigation.canOccupy({-3.2F, 0.0F, 3.6F})
            && !navigation.canOccupy({6.9F, 0.0F, 0.0F}),
        "visible waist and tall cover have navigation collision");
    for (const float side : {-1.0F, 1.0F}) {
        expect(navigation.canOccupy({side * 9.1F, 0.0F, 4.0F}),
            "upper wall-mounted treads leave walkable space beneath them");
        wkc::core::Vec3 feet{side * 9.1F, 0.0F, 9.35F};
        for (int update = 0; update < 90; ++update) {
            const auto motion = navigation.moveCharacter(feet,
                {0.0F, -0.35F, -4.5F}, 1.0F / 60.0F, {});
            feet = motion.position;
            if (feet.z <= 3.55F) break;
        }
        expect(feet.z <= 3.55F && feet.y >= 3.31F,
            "both shorter Combat Room stairs reach the gallery height");
        for (int update = 0; update < 12; ++update) {
            feet = navigation.moveCharacter(feet,
                {0.0F, -0.35F, -4.5F}, 1.0F / 60.0F, {}).position;
        }
        expect(feet.z < 3.35F && feet.y >= 3.31F,
            "wall-mounted stairs connect to the end of each gallery");
    }

    auto doorway = room;
    doorway.connectors.push_back({
        .definitionId = 1U,
        .worldPosition = {0.0F, 0.0F, 11.0F},
        .worldNormal = {0.0F, 0.0F, 1.0F},
        .width = 1.2F,
        .height = 2.2F,
        .edge = std::nullopt,
    });
    expect(world::validateRoomArchitecture(doorway, parts),
        "authored cover leaves a future doorway open");
    auto blocked = parts;
    blocked.push_back({{{-0.4F, 0.0F, 10.0F},
        {0.4F, 2.0F, 10.8F}}, {0.5F, 0.5F, 0.5F}, true, false});
    expect(!world::validateRoomArchitecture(doorway, blocked),
        "doorway obstruction is rejected before rendering");
}

void testCafeteriaPreview()
{
    using namespace gdp;
    using namespace gdp::entities;
    using namespace gdp::gameplay;
    auto preview = game::makeCafeteriaPreview(20261029U);
    auto& room = preview.rooms.front();
    expect(preview.rooms.size() == 1U
            && room.theme == world::RoomTheme::Cafeteria
            && room.moduleId == world::cafeteriaPreviewModuleId
            && room.worldBounds.maximum.y == 6.0F,
        "Cafeteria preview opens the requested room directly");
    const auto parts = world::makeRoomArchitecture(room);
    expect(!parts.empty() && world::validateRoomArchitecture(room, parts),
        "Cafeteria furniture clears reserved enemy positions");
    EntityManager manager;
    const auto initial = game::populateCafeteriaPreview(preview, manager, {
        .transform = {.position = {0.0F, 0.0F, 4.0F}},
        .health = 100.0F,
    });
    expect(initial.isValid() && manager.query(EntityKind::Enemy).empty(),
        "Cafeteria preview starts clear for layout inspection");
    const CollisionWorld navigation = makeRoomNavigationCollision(
        preview, room.id, false);
    expect(navigation.canOccupy({0.0F, 0.0F, -8.0F})
            && navigation.canOccupy({0.0F, 0.0F, 4.0F})
            && navigation.canOccupy({-2.0F, 0.0F, 0.0F})
            && navigation.canOccupy({4.8F, 0.0F, 0.0F}),
        "Cafeteria center and side aisles remain walkable");
    expect(!navigation.canOccupy({-4.6F, 0.0F, -4.3F})
            && !navigation.canOccupy({3.0F, 0.0F, 4.3F})
            && !navigation.canOccupy({6.5F, 0.0F, 4.3F})
            && !navigation.canOccupy({8.4F, 0.0F, 4.3F}),
        "tables and serving counter have matching collision");
    expect(navigation.canOccupy({5.4F, 0.0F, 0.0F})
            && navigation.canOccupy({6.4F, 0.0F, 0.0F})
            && navigation.canOccupy({7.4F, 0.0F, 0.0F})
            && navigation.canOccupy({8.4F, 0.0F, 0.0F}),
        "a full-width passage connects the room to the side doorway");
    wkc::core::Vec3 doorwayPosition{8.4F, 0.0F, 0.0F};
    bool exitCollided = false;
    for (int tick = 0; tick < 60; ++tick) {
        const auto movement = navigation.moveCharacter(
            doorwayPosition, {-3.0F, 0.0F, 0.0F}, 1.0F / 60.0F, {});
        doorwayPosition = movement.position;
        exitCollided = exitCollided || movement.collided;
    }
    expect(!exitCollided && doorwayPosition.x < 5.5F,
        "the player can actually move out from behind the service line");

    room.connectors = {
        {.definitionId = 1U, .worldPosition = {0.0F, 0.0F, 10.0F},
            .worldNormal = {0.0F, 0.0F, 1.0F}, .edge = std::nullopt},
        {.definitionId = 2U, .worldPosition = {0.0F, 0.0F, -10.0F},
            .worldNormal = {0.0F, 0.0F, -1.0F}, .edge = std::nullopt},
        {.definitionId = 3U, .worldPosition = {9.0F, 0.0F, 0.0F},
            .worldNormal = {1.0F, 0.0F, 0.0F}, .edge = std::nullopt},
        {.definitionId = 4U, .worldPosition = {-9.0F, 0.0F, 0.0F},
            .worldNormal = {-1.0F, 0.0F, 0.0F}, .edge = std::nullopt},
    };
    expect(world::validateRoomArchitecture(room, parts),
        "four future door positions remain unobstructed");
    auto blocked = parts;
    blocked.push_back({{{8.1F, 0.0F, -0.4F},
        {8.8F, 1.8F, 0.4F}}, {0.5F, 0.5F, 0.5F}, true, false});
    expect(!world::validateRoomArchitecture(room, blocked),
        "a cabinet in the side doorway is rejected");
}

void testCafeteriaLongPreview()
{
    using namespace gdp;
    auto preview = game::makeCafeteriaLongPreview(20261029U);
    const auto& room = preview.rooms.front();
    const auto parts = world::makeRoomArchitecture(room);
    expect(room.moduleId == world::cafeteriaLongPreviewModuleId
            && room.worldBounds.minimum.z == -12.0F
            && room.worldBounds.maximum.z == 12.0F
            && parts.size() > world::makeRoomArchitecture(
                game::makeCafeteriaPreview(20261029U).rooms.front()).size()
            && world::validateRoomArchitecture(room, parts),
        "long Cafeteria adds a third row of tables without obstructing reserved space");
    expect(std::count_if(parts.begin(), parts.end(),
                [](const world::RoomArchitecturePart& part) {
                    return part.decoration
                        == world::RoomDecorationPart::BroadleafPlanter;
                }) == 2
            && std::count_if(parts.begin(), parts.end(),
                [](const world::RoomArchitecturePart& part) {
                    return part.decoration
                        == world::RoomDecorationPart::CeilingDuct;
                }) == 1,
        "long Cafeteria has two distinct corner plants and a service duct");
    bool upperServicePartsOverlap = false;
    for (std::size_t first = 0; first < parts.size(); ++first) {
        const auto& a = parts[first].bounds;
        if (a.minimum.x < 6.2F || a.minimum.y < 1.15F) continue;
        for (std::size_t second = first + 1; second < parts.size(); ++second) {
            const auto& b = parts[second].bounds;
            if (b.minimum.x < 6.2F || b.minimum.y < 1.15F) continue;
            if (a.minimum.x < b.maximum.x && b.minimum.x < a.maximum.x
                && a.minimum.y < b.maximum.y && b.minimum.y < a.maximum.y
                && a.minimum.z < b.maximum.z && b.minimum.z < a.maximum.z) {
                upperServicePartsOverlap = true;
            }
        }
    }
    expect(!upperServicePartsOverlap,
        "upper Cafeteria counter and tray surfaces do not intersect");
    const auto navigation = gameplay::makeRoomNavigationCollision(
        preview, room.id, false);
    expect(navigation.canOccupy({0.0F, 0.0F, -10.5F})
            && navigation.canOccupy({0.0F, 0.0F, 10.5F})
            && navigation.canOccupy({-2.0F, 0.0F, 0.0F})
            && navigation.canOccupy({5.3F, 0.0F, 0.0F})
            && !navigation.canOccupy({-4.6F, 0.0F, -7.0F})
            && !navigation.canOccupy({3.0F, 0.0F, 7.0F})
            && !navigation.canOccupy({6.5F, 0.0F, 7.0F}),
        "long Cafeteria keeps end and side approaches while tables and counter collide");
    expect(!navigation.canOccupy({-7.8F, 0.0F, -10.7F})
            && !navigation.canOccupy({-7.8F, 0.0F, 10.7F})
            && navigation.canOccupy({-5.8F, 0.0F, -10.7F})
            && navigation.canOccupy({-5.8F, 0.0F, 10.7F}),
        "Cafeteria planters collide while the end aisles stay traversable");
    entities::EntityManager manager;
    const auto initial = game::populateCafeteriaPreview(preview, manager, {
        .transform = {.position = {0.0F, 0.0F, 4.0F}}, .health = 100.0F});
    expect(initial.isValid() && manager.query(entities::EntityKind::Enemy).empty(),
        "long Cafeteria preview starts clear for playtest");
}

void testLaboratoryPreview()
{
    using namespace gdp;
    using namespace gdp::entities;
    using namespace gdp::gameplay;
    auto preview = game::makeLaboratoryPreview(20261029U);
    const auto& room = preview.rooms.front();
    expect(preview.rooms.size() == 1U
            && room.theme == world::RoomTheme::Laboratory
            && room.moduleId == world::laboratoryPreviewModuleId
            && room.worldBounds.maximum.y == 6.0F,
        "Laboratory preview opens one tall room directly");
    const auto parts = world::makeRoomArchitecture(room);
    expect(!parts.empty() && world::validateRoomArchitecture(room, parts),
        "Laboratory architecture clears its reserved encounter positions");
    EntityManager manager;
    const auto initial = game::populateLaboratoryPreview(preview, manager, {
        .transform = {.position = {0.0F, 0.0F, 4.0F}},
        .health = 100.0F,
    });
    expect(initial.isValid() && manager.query(EntityKind::Enemy).empty(),
        "Laboratory preview starts without enemies for layout inspection");
    const CollisionWorld navigation = makeRoomNavigationCollision(
        preview, room.id, false);
    expect(navigation.canOccupy({0.0F, 0.0F, 4.0F})
            && navigation.canOccupy({0.0F, 0.0F, -8.0F})
            && navigation.canOccupy({-5.0F, 0.0F, 0.0F})
            && navigation.canOccupy({5.0F, 0.0F, 0.0F}),
        "Laboratory center and side approaches remain walkable");
    expect(!navigation.canOccupy({8.0F, 0.0F, 4.0F})
            && !navigation.canOccupy({-8.0F, 0.0F, -7.0F})
            && navigation.canOccupy({8.0F, 0.0F, 0.0F})
            && navigation.canOccupy({-8.0F, 0.0F, 0.0F}),
        "side equipment blocks movement without closing side doors");
    expect(!navigation.canOccupy({-4.4F, 0.0F, -3.9F})
            && !navigation.canOccupy({4.4F, 0.0F, 3.9F})
            && navigation.canOccupy({0.0F, 0.0F, -3.9F})
            && navigation.canOccupy({0.0F, 0.0F, 3.9F})
            && navigation.canOccupy({-6.2F, 0.0F, -3.9F})
            && navigation.canOccupy({6.2F, 0.0F, 3.9F}),
        "central benches block movement while their center and side routes remain open");
}

void testTechnicalPreview()
{
    using namespace gdp;
    using namespace gdp::entities;
    using namespace gdp::gameplay;
    auto preview = game::makeTechnicalPreview(20261029U);
    const auto& room = preview.rooms.front();
    expect(preview.rooms.size() == 1U
            && room.theme == world::RoomTheme::Maintenance
            && room.moduleId == world::technicalPreviewModuleId
            && room.worldBounds.minimum.x == -8.0F
            && room.worldBounds.maximum.x == 8.0F
            && room.worldBounds.maximum.y == 5.5F,
        "Technical preview opens one tall room directly");
    const auto parts = world::makeRoomArchitecture(room);
    expect(!parts.empty() && world::validateRoomArchitecture(room, parts),
        "Technical architecture clears its reserved encounter positions");
    EntityManager manager;
    const auto initial = game::populateTechnicalPreview(preview, manager, {
        .transform = {.position = {0.0F, 0.0F, 4.0F}},
        .health = 100.0F,
    });
    expect(initial.isValid() && manager.query(EntityKind::Enemy).empty(),
        "Technical preview starts without enemies for layout inspection");
    const CollisionWorld navigation = makeRoomNavigationCollision(
        preview, room.id, false);
    expect(navigation.canOccupy({0.0F, 0.0F, 4.0F})
            && navigation.canOccupy({0.0F, 0.0F, -8.0F})
            && navigation.canOccupy({0.0F, 0.0F, 0.0F})
            && navigation.canOccupy({7.0F, 0.0F, 0.0F})
            && navigation.canOccupy({-7.0F, 0.0F, 0.0F}),
        "Technical center and side doors remain walkable");
    expect(!navigation.canOccupy({-3.0F, 0.0F, -3.8F})
            && !navigation.canOccupy({3.0F, 0.0F, 3.8F})
            && !navigation.canOccupy({-7.0F, 0.0F, -3.8F})
            && !navigation.canOccupy({7.0F, 0.0F, 3.8F})
            && navigation.canOccupy({-5.6F, 0.0F, -3.8F})
            && navigation.canOccupy({5.6F, 0.0F, 3.8F})
            && navigation.canOccupy({0.0F, 0.0F, -3.8F})
            && navigation.canOccupy({0.0F, 0.0F, 3.8F}),
        "Technical maintenance stations collide while axial and flank routes remain open");

    const auto box = std::find_if(parts.begin(), parts.end(),
        [](const world::RoomArchitecturePart& part) {
            return part.breakable && part.bounds.minimum.x < 0.0F;
        });
    expect(box != parts.end(),
        "Technical station provides a breakable box on its tabletop");
    if (box != parts.end()) {
        const wkc::core::Vec3 scale = box->bounds.maximum - box->bounds.minimum;
        const wkc::core::Vec3 position{
            (box->bounds.minimum.x + box->bounds.maximum.x) * 0.5F,
            box->bounds.minimum.y,
            (box->bounds.minimum.z + box->bounds.maximum.z) * 0.5F,
        };
        const EntityId target = manager.createProp({
            .transform = {.position = position, .scale = scale},
            .roomId = room.id, .type = PropType::StorageBox,
            .solid = true, .health = 35.0F, .maximumHealth = 35.0F,
        });
        const CollisionWorld withBox = collisionWorldWithProps(
            navigation, manager, room.id);
        Weapon gun({.damage = 70.0F, .fireInterval = 0.0F, .range = 10.0F});
        const wkc::core::Vec3 origin{0.0F, 1.58F, -3.8F};
        const wkc::core::Vec3 aim = wkc::core::normalized(
            position + wkc::core::Vec3{0.0F, scale.y * 0.5F, 0.0F}
            - origin);
        const ShotResult shot = fireHitscan(
            gun, origin, aim, room.id, withBox, manager);
        expect(shot.outcome == ShotOutcome::Destroyed && shot.hit
                && shot.hit->target == target
                && manager.findData<PropData>(target)->state
                    == PropState::Destroyed,
            "a shot destroys the tabletop box from the open center lane");
        expect(collisionWorldWithProps(navigation, manager, room.id)
                .solids().size() + 1U == withBox.solids().size(),
            "destroyed tabletop box no longer contributes collision");
    }
}

void testTechnicalLPreview()
{
    using namespace gdp;
    using namespace gdp::gameplay;
    auto preview = game::makeTechnicalLPreview(20261029U);
    const auto& room = preview.rooms.front();
    expect(room.moduleId == world::technicalLPreviewModuleId
            && room.walkableOutline.size() == 6U,
        "Maintenance L preview has a separate six-edge footprint");
    const auto parts = world::makeRoomArchitecture(room);
    expect(!parts.empty() && world::validateRoomArchitecture(room, parts),
        "Maintenance L furniture clears reserved positions");
    const auto navigation = makeRoomNavigationCollision(preview, room.id, false);
    expect(navigation.canOccupy({0.0F, 0.0F, 4.0F})
            && navigation.canOccupy({0.0F, 0.0F, -8.0F})
            && navigation.canOccupy({6.0F, 0.0F, 0.0F})
            && !navigation.canOccupy({6.0F, 0.0F, -6.0F})
            && !roomContainingPosition(preview, {6.0F, 0.0F, -6.0F})
            && !roomContainingCharacter(preview, {1.10F, 0.0F, -3.0F}, {})
            && !navigation.canOccupy({1.10F, 0.0F, -3.0F}),
        "Maintenance L blocks the cut-out and its inside corner");
    entities::EntityManager manager;
    const auto populated = game::populateTechnicalPreview(preview, manager, {
        .transform = {.position = {0.0F, 0.0F, 4.0F}}, .health = 100.0F});
    expect(populated.isValid()
            && collisionWorldWithProps(navigation, manager, room.id)
                .canOccupy({0.0F, 0.0F, -8.0F}),
        "Maintenance L keeps its footprint after prop collision is added");
}

void testSecurityPreview()
{
    using namespace gdp;
    using namespace gdp::entities;
    using namespace gdp::gameplay;
    auto preview = game::makeSecurityPreview(20261029U);
    const auto& room = preview.rooms.front();
    expect(preview.rooms.size() == 1U
            && room.theme == world::RoomTheme::Security
            && room.moduleId == world::securityPreviewModuleId
            && room.worldBounds.minimum.x == -9.0F
            && room.worldBounds.maximum.x == 9.0F
            && room.worldBounds.maximum.y == 6.0F,
        "Security preview opens one large room directly");
    const auto parts = world::makeRoomArchitecture(room);
    expect(!parts.empty() && world::validateRoomArchitecture(room, parts),
        "Security furniture clears reserved encounter positions");
    EntityManager manager;
    const auto initial = game::populateSecurityPreview(preview, manager, {
        .transform = {.position = {0.0F, 0.0F, 4.0F}},
        .health = 100.0F,
    });
    expect(initial.isValid() && manager.query(EntityKind::Enemy).empty(),
        "Security preview starts without enemies for layout inspection");
    const CollisionWorld navigation = makeRoomNavigationCollision(
        preview, room.id, false);
    expect(navigation.canOccupy({0.0F, 0.0F, 4.0F})
            && navigation.canOccupy({0.0F, 0.0F, -7.0F})
            && navigation.canOccupy({0.0F, 0.0F, 0.0F})
            && navigation.canOccupy({8.0F, 0.0F, 0.0F})
            && navigation.canOccupy({-8.0F, 0.0F, 0.0F}),
        "Security center and side doors remain walkable");
    expect(!navigation.canOccupy({-6.0F, 0.0F, -8.0F})
            && !navigation.canOccupy({6.0F, 0.0F, -8.0F})
            && !navigation.canOccupy({-4.2F, 0.0F, 3.8F})
            && !navigation.canOccupy({4.2F, 0.0F, -3.8F})
            && navigation.canOccupy({0.0F, 0.0F, 3.8F})
            && navigation.canOccupy({0.0F, 0.0F, -3.8F})
            && navigation.canOccupy({-6.0F, 0.0F, 3.8F})
            && navigation.canOccupy({6.0F, 0.0F, -3.8F}),
        "Security consoles and barricades collide while center and flanks remain open");
}

void testSecurityTPreview()
{
    using namespace gdp;
    using namespace gdp::gameplay;
    auto preview = game::makeSecurityTPreview(20261102U);
    const auto& room = preview.rooms.front();
    expect(room.moduleId == world::securityTPreviewModuleId
            && room.walkableOutline.size() == 8U
            && room.worldBounds.maximum.y == 6.0F,
        "Security T preview has a separate eight-edge footprint");
    const auto parts = world::makeRoomArchitecture(room);
    bool partsInside = !parts.empty();
    for (const auto& part : parts) {
        for (const float x : {part.bounds.minimum.x, part.bounds.maximum.x}) {
            for (const float z : {part.bounds.minimum.z, part.bounds.maximum.z}) {
                partsInside &= world::footprintContainsDisk(
                    room.walkableOutline, {x, 0.0F, z});
            }
        }
    }
    expect(partsInside && world::validateRoomArchitecture(room, parts),
        "Security T consoles and cover stay inside the footprint and reserved zones");
    const auto planterCount = std::count_if(parts.begin(), parts.end(),
        [](const world::RoomArchitecturePart& part) {
            return part.decoration == world::RoomDecorationPart::Planter;
        });
    const auto ductCount = std::count_if(parts.begin(), parts.end(),
        [](const world::RoomArchitecturePart& part) {
            return part.decoration == world::RoomDecorationPart::CeilingDuct
                && part.bounds.minimum.y >= 5.0F;
        });
    expect(planterCount == 2 && ductCount == 2,
        "Security T has two corner planters and two overhead duct sections");
    const auto furnitureCount = [&](const world::SecurityFurniturePart role) {
        return std::count_if(parts.begin(), parts.end(),
            [role](const world::RoomArchitecturePart& part) {
                return part.securityFurniturePart == role;
            });
    };
    expect(furnitureCount(world::SecurityFurniturePart::ConsoleBase) == 2
            && furnitureCount(world::SecurityFurniturePart::ConsoleTop) == 2
            && furnitureCount(world::SecurityFurniturePart::Monitor) == 2
            && furnitureCount(world::SecurityFurniturePart::Cabinet) == 2
            && furnitureCount(world::SecurityFurniturePart::Barricade) == 2,
        "Security T assigns the approved furniture treatment to both stations, cabinets and covers");
    const auto navigation = makeRoomNavigationCollision(preview, room.id, false);
    expect(navigation.canOccupy({0.0F, 0.0F, -8.0F})
            && navigation.canOccupy({0.0F, 0.0F, 0.0F})
            && navigation.canOccupy({-8.0F, 0.0F, 0.0F})
            && navigation.canOccupy({8.0F, 0.0F, 0.0F})
            && !navigation.canOccupy({-8.0F, 0.0F, -8.0F})
            && !navigation.canOccupy({8.0F, 0.0F, -8.0F})
            && !navigation.canOccupy({8.9F, 0.0F, -1.5F})
            && !navigation.canOccupy({-8.9F, 0.0F, -1.5F})
            && !navigation.canOccupy({-6.3F, 0.0F, 2.5F}),
        "Security T approach and gallery stay open around cover and planters");
}

void testCentralHubPreview()
{
    using namespace gdp;
    using namespace gdp::entities;
    using namespace gdp::gameplay;
    auto preview = game::makeCentralHubPreview(20261030U);
    const auto& room = preview.rooms.front();
    expect(preview.rooms.size() == 1U
            && room.role == world::RoomRole::Start
            && room.moduleId == world::centralHubPreviewModuleId
            && room.worldBounds.minimum.x == -10.0F
            && room.worldBounds.maximum.x == 10.0F
            && room.worldBounds.maximum.y == 6.0F
            && room.walkableOutline.size() == 8U,
        "Central Hub preview opens one large room directly");
    const auto parts = world::makeRoomArchitecture(room);
    expect(!parts.empty() && world::validateRoomArchitecture(room, parts),
        "Central Hub landmark clears its reserved encounter positions");
    EntityManager manager;
    const auto initial = game::populateCentralHubPreview(preview, manager, {
        .transform = {.position = {0.0F, 0.0F, 4.0F}},
        .health = 100.0F,
    });
    expect(initial.isValid() && manager.query(EntityKind::Enemy).empty(),
        "Central Hub preview starts without enemies for layout inspection");
    const CollisionWorld navigation = makeRoomNavigationCollision(
        preview, room.id, false);
    expect(!navigation.canOccupy({0.0F, 0.0F, 0.0F})
            && navigation.canOccupy({0.0F, 0.0F, 4.0F})
            && navigation.canOccupy({3.0F, 0.0F, 0.0F})
            && navigation.canOccupy({-3.0F, 0.0F, 0.0F})
            && navigation.canOccupy({0.0F, 0.0F, 3.0F})
            && navigation.canOccupy({0.0F, 0.0F, -3.0F}),
        "Central Hub landmark blocks its footprint with routes on four sides");
    expect(navigation.canOccupy({9.0F, 0.0F, 0.0F})
            && navigation.canOccupy({-9.0F, 0.0F, 0.0F})
            && navigation.canOccupy({0.0F, 0.0F, 8.5F})
            && navigation.canOccupy({0.0F, 0.0F, -8.5F})
            && !navigation.canOccupy({8.0F, 0.0F, 8.0F})
            && !roomContainingPosition(preview, {8.0F, 0.0F, 8.0F})
            && !roomContainingCharacter(preview, {8.0F, 0.0F, 8.0F}, {}),
        "Central Hub keeps all four door approaches open");
}

void testIndustrialCorridorPreview()
{
    using namespace gdp;
    using namespace gdp::entities;
    using namespace gdp::gameplay;
    auto preview = game::makeIndustrialCorridorPreview(20261031U);
    expect(preview.rooms.size() == 2U
            && preview.corridors.size() == 1U
            && preview.graph.nodes.size() == 2U
            && preview.graph.edges.size() == 1U
            && world::validateWorld(preview).isValid(),
        "Industrial Corridor preview has two valid rooms joined by one edge");
    if (preview.rooms.size() != 2U || preview.corridors.size() != 1U) {
        return;
    }

    const auto& first = preview.rooms.front();
    const auto& second = preview.rooms.back();
    const auto& corridor = preview.corridors.front();
    expect(second.moduleId
                == world::industrialCorridorDestinationPreviewModuleId
            && world::makeRoomArchitecture(second).empty()
            && world::validateRoomArchitecture(second, {}),
        "corridor destination does not inherit oversized boss arena architecture");
    const auto doorForEdge = [edge = corridor.edge](const world::RoomInstance& room) {
        return std::find_if(room.connectors.begin(), room.connectors.end(),
            [edge](const world::ConnectorInstance& door) {
                return door.edge == edge;
            });
    };
    const auto firstDoor = doorForEdge(first);
    const auto secondDoor = doorForEdge(second);
    expect(first.theme == world::RoomTheme::Industrial
            && second.theme == world::RoomTheme::Industrial
            && firstDoor != first.connectors.end()
            && secondDoor != second.connectors.end()
            && corridor.shape == world::CorridorShape::Straight
            && corridor.firstRoom == first.id
            && corridor.secondRoom == second.id
            && corridor.width == 2.4F && corridor.height == 2.8F
            && corridor.points.front().z > corridor.points.back().z
            && corridor.points.front().z - corridor.points.back().z > 9.0F,
        "Industrial passage is a long straight CorridorPath between paired doors");
    if (firstDoor == first.connectors.end()
        || secondDoor == second.connectors.end()) {
        return;
    }
    expect(firstDoor->width == 2.4F && firstDoor->height == 2.8F
            && secondDoor->width == 2.4F && secondDoor->height == 2.8F
            && firstDoor->worldNormal.z < 0.0F
            && secondDoor->worldNormal.z > 0.0F,
        "both widened doors face the corridor");

    EntityManager manager;
    const auto initial = game::populateIndustrialCorridorPreview(preview,
        manager, {.transform = {.position = {0.0F, 0.0F, 4.0F}},
            .health = 100.0F});
    expect(initial.isValid() && manager.query(EntityKind::Enemy).empty()
            && manager.find(initial.player)->roomId == first.id
            && manager.find(initial.terminal)->roomId == first.id,
        "player and return terminal start in the first room without enemies");

    const CollisionWorld openNavigation = makeRoomNavigationCollision(
        preview, first.id, false);
    const CollisionWorld populatedNavigation = collisionWorldWithProps(
        openNavigation, manager, first.id);
    expect(populatedNavigation.canOccupy({0.0F, 0.0F, 4.0F}),
        "preview spawn stays clear of the return terminal");
    const CollisionWorld closedNavigation = makeRoomNavigationCollision(
        preview, first.id, true);
    expect(openNavigation.canOccupy({0.0F, 0.0F, -10.0F})
            && openNavigation.canOccupy({0.0F, 0.0F, -16.0F})
            && !closedNavigation.canOccupy({0.0F, 0.0F, -10.0F}),
        "open doors expose the walkable corridor and destination room");
    const auto forward = findRoomTransition(preview, first.id,
        {0.0F, 0.0F, -3.9F}, {0.0F, 0.0F, -1.0F});
    const auto backward = findRoomTransition(preview, second.id,
        {0.0F, 0.0F, -16.1F}, {0.0F, 0.0F, 1.0F});
    expect(forward && forward->destinationRoom == second.id
            && backward && backward->destinationRoom == first.id,
        "both door interactions permit a return trip");

    wkc::core::Vec3 position{0.0F, 0.0F, -4.0F};
    bool collided = false;
    for (int tick = 0; tick < 240; ++tick) {
        const auto motion = openNavigation.moveCharacter(position,
            {0.0F, 0.0F, -3.0F}, 1.0F / 60.0F, {});
        position = motion.position;
        collided = collided || motion.collided;
    }
    expect(!collided && position.z < -15.5F
            && roomContainingCharacter(preview, position) == second.id,
        "player can walk through the corridor into the second room");
}

} // namespace

int main()
{
    testPracticeAreaAndBossApproach();
    testBossRoomPreview();
    testStorageRoomPreview();
    testCombatRoomPreview();
    testCafeteriaPreview();
    testCafeteriaLongPreview();
    testLaboratoryPreview();
    testTechnicalPreview();
    testTechnicalLPreview();
    testSecurityPreview();
    testSecurityTPreview();
    testCentralHubPreview();
    testIndustrialCorridorPreview();
    if (failures != 0) {
        std::cerr << failures << " test arena test(s) failed\n";
        return 1;
    }
    std::cout << "All test arena tests passed\n";
    return 0;
}
