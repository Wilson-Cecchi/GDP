#include "core/Math.hpp"
#include "world/Connector.hpp"
#include "world/Corridor.hpp"
#include "world/LightingGenerator.hpp"
#include "world/MapGraph.hpp"
#include "world/LootGenerator.hpp"
#include "world/PropGenerator.hpp"
#include "world/RoomArchitecture.hpp"
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
#include <tuple>
#include <utility>
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
                || module.supportsRole(RoomRole::Terminal)
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
                     RoomRole::Boss, RoomRole::Terminal}) {
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
    std::size_t bossModules = 0U;
    std::size_t wideNormalModules = 0U;
    for (const RoomModule& module : library) {
        if (module.supportsRole(RoomRole::Boss)) {
            ++bossModules;
            expect(module.footprint.x == 28 && module.footprint.y == 7
                    && module.footprint.z == 32,
                "each Boss module is 28 by 7 by 32 metres");
        }
        if (module.supportsRole(RoomRole::Normal)
            && module.footprint.x >= 16 && module.footprint.z >= 18) {
            ++wideNormalModules;
            expect(module.footprint.y == 6 && !module.supportsRole(RoomRole::DeadEnd),
                "wide Normal variants add height without widening DeadEnds");
        }
    }
    expect(bossModules == 3U && wideNormalModules == 10U,
        "library contains three expanded Boss modules and ten wide Normal variants");
    expect(library.size() >= 30U && sawCompact && sawMedium && sawLarge,
        "default library contains compact, medium and large structural families");
    expect(roleSizesValid,
        "Hub, Arena and Boss use large modules while Secret uses compact modules");
    expect(rotationsValid,
        "every default module keeps bounds and connectors valid in all four rotations");
}

void testCampaignRoomVariants()
{
    using namespace gdp::world;
    using wkc::core::QuarterTurn;

    const auto library = makeDefaultRoomModuleLibrary();
    for (const auto& [id, role] : std::array{
             std::pair{combatGalleryArenaModuleId, RoomRole::Arena},
             std::pair{securityHallModuleId, RoomRole::Terminal},
             std::pair{securityTModuleId, RoomRole::Terminal},
             std::pair{cafeteriaLongModuleId, RoomRole::Normal},
             std::pair{laboratoryHallModuleId, RoomRole::Normal},
         }) {
        const auto module = std::find_if(library.begin(), library.end(),
            [id](const RoomModule& candidate) { return candidate.id == id; });
        expect(module != library.end(), "approved room variant exists in campaign library");
        if (module == library.end()) continue;
        expect(validateRoomModule(*module).isValid() && module->supportsRole(role),
            "campaign room variant supports its intended encounter role");
        for (int rotation = 0; rotation < 4; ++rotation) {
            const auto room = instantiateRoom(*module, id, role, 0xA55AU,
                {24.0F, 0.0F, -18.0F}, static_cast<QuarterTurn>(rotation));
            expect(room.has_value(), "campaign room variant instantiates in every rotation");
            if (!room) continue;
            const auto parts = makeRoomArchitecture(*room);
            expect(!parts.empty() && validateRoomArchitecture(*room, parts),
                "campaign room architecture clears encounters and doorways");
            const auto fractionalRoom = instantiateRoom(*module, id, role,
                0xA55AU, {-15.85F, 0.0F, 10.3F},
                static_cast<QuarterTurn>(rotation));
            expect(fractionalRoom.has_value(),
                "campaign room variant instantiates at fractional world coordinates");
            if (fractionalRoom) {
                const auto fractionalParts = makeRoomArchitecture(*fractionalRoom);
                expect(!fractionalParts.empty()
                        && validateRoomArchitecture(*fractionalRoom, fractionalParts),
                    "rotated room architecture tolerates sub-millimetre rounding");
            }
        }
        for (std::uint32_t seed = 0; seed < 32U; ++seed) {
            const auto room = instantiateRoom(*module, id, role, seed,
                {0.0F, 0.0F, 0.0F}, QuarterTurn::Degrees0);
            if (!room) continue;
            const auto loot = generateLootLayout(room->seed, role, room->theme,
                3U, room->lootZones, 1.0F, true);
            const auto props = generatePropLayout(room->seed, role, room->theme,
                3U, room->propZones, 1.0F);
            expect(validateLootLayout(loot, room->lootZones).isValid()
                    && validatePropLayout(props, room->propZones).isValid(),
                "campaign room drops and props validate across room seeds");
        }
    }
}

void testRoomWallColumns()
{
    using namespace wkc::core;
    using namespace gdp::world;

    constexpr std::array rotations{
        QuarterTurn::Degrees0,
        QuarterTurn::Degrees90,
        QuarterTurn::Degrees180,
        QuarterTurn::Degrees270,
    };
    const auto overlapsOpen = [](const Aabb a, const Aabb b) {
        return a.minimum.x < b.maximum.x && a.maximum.x > b.minimum.x
            && a.minimum.y < b.maximum.y && a.maximum.y > b.minimum.y
            && a.minimum.z < b.maximum.z && a.maximum.z > b.minimum.z;
    };
    const auto library = makeDefaultRoomModuleLibrary();
    for (const RoomModuleId moduleId : {101U, 204U, 303U,
             505U, 605U, 703U}) {
        const auto found = std::find_if(library.begin(), library.end(),
            [=](const RoomModule& module) { return module.id == moduleId; });
        expect(found != library.end(), "wall column fixture module exists");
        if (found == library.end()) {
            continue;
        }
        std::vector<RoomArchitecturePart> unrotated;
        for (const QuarterTurn rotation : rotations) {
            const auto room = instantiateRoom(*found, moduleId,
                RoomRole::Normal, 0xA55AU, {24.0F, 2.0F, -18.0F}, rotation);
            expect(room.has_value(), "wall column fixture instantiates");
            if (!room) {
                continue;
            }
            const auto columns = makeRoomWallColumns(*room);
            const auto authored = makeRoomArchitecture(*room);
            const bool industrialHasIntegratedColumns = moduleId == 101U
                && std::count_if(authored.begin(), authored.end(),
                    [](const RoomArchitecturePart& part) {
                        const Vec3 extent = part.bounds.maximum
                            - part.bounds.minimum;
                        return part.structuralColumn && extent.y > 3.0F
                            && extent.x < 0.5F && extent.z < 0.5F;
                    }) >= 4;
            expect((!columns.empty() || industrialHasIntegratedColumns)
                    && columns == makeRoomWallColumns(*room),
                "wall columns exist and repeat exactly for the same seed");
            bool safe = true;
            for (const RoomArchitecturePart& column : columns) {
                const Vec3 extent = column.bounds.maximum
                    - column.bounds.minimum;
                const bool xWall = nearlyEqual(
                        column.bounds.minimum.x, room->worldBounds.minimum.x)
                    || nearlyEqual(column.bounds.maximum.x,
                        room->worldBounds.maximum.x);
                const bool zWall = nearlyEqual(
                        column.bounds.minimum.z, room->worldBounds.minimum.z)
                    || nearlyEqual(column.bounds.maximum.z,
                        room->worldBounds.maximum.z);
                safe &= column.solid && !column.breakable
                    && room->worldBounds.contains(column.bounds.minimum)
                    && room->worldBounds.contains(column.bounds.maximum)
                    && nearlyEqual(column.bounds.minimum.y,
                        room->worldBounds.minimum.y)
                    && nearlyEqual(column.bounds.maximum.y,
                        room->worldBounds.maximum.y)
                    && ((xWall && nearlyEqual(extent.x, 0.26F)
                            && nearlyEqual(extent.z, 0.38F))
                        || (zWall && nearlyEqual(extent.z, 0.26F)
                            && nearlyEqual(extent.x, 0.38F)));
                for (const RoomArchitecturePart& part : authored) {
                    safe &= !overlapsOpen(column.bounds, part.bounds);
                }
                for (const SpawnZone& zone : room->enemyZones) {
                    safe &= !overlapsOpen(column.bounds, zone.bounds);
                }
                for (const SpawnZone& zone : room->propZones) {
                    safe &= !overlapsOpen(column.bounds, zone.bounds);
                }
                for (const SpawnZone& zone : room->lootZones) {
                    safe &= !overlapsOpen(column.bounds, zone.bounds);
                }
                for (const ConnectorInstance& connector : room->connectors) {
                    const Vec3 midpoint = (column.bounds.minimum
                        + column.bounds.maximum) * 0.5F;
                    const bool sameWall = std::abs(connector.worldNormal.x) > 0.5F
                        ? std::abs(midpoint.x - connector.worldPosition.x) < 0.5F
                        : std::abs(midpoint.z - connector.worldPosition.z) < 0.5F;
                    const float along = std::abs(connector.worldNormal.x) > 0.5F
                        ? std::abs(midpoint.z - connector.worldPosition.z)
                        : std::abs(midpoint.x - connector.worldPosition.x);
                    safe &= !sameWall || along >= connector.width * 0.5F
                        + 0.30F + 0.19F - 1.0e-4F;
                }
            }
            expect(safe,
                "wall columns touch walls and clear doors, zones and authored geometry");
            if (rotation == QuarterTurn::Degrees0) {
                unrotated = columns;
            } else {
                bool rotated = columns.size() == unrotated.size();
                if (rotated) {
                    for (std::size_t index = 0; index < columns.size(); ++index) {
                        const Vec3 original = (unrotated[index].bounds.minimum
                            + unrotated[index].bounds.maximum) * 0.5F;
                        const Vec3 actual = (columns[index].bounds.minimum
                            + columns[index].bounds.maximum) * 0.5F;
                        rotated &= nearlyEqual(actual,
                            room->transform.position
                                + rotateQuarterTurns(
                                    original - room->transform.position,
                                    rotation), 1.0e-3F);
                    }
                }
                expect(rotated,
                    "wall column positions follow all room quarter turns");
            }
        }
    }

    RoomInstance bare;
    bare.theme = RoomTheme::Industrial;
    bare.seed = 19U;
    bare.worldBounds = {{-8.0F, 0.0F, -10.0F},
        {8.0F, 6.0F, 10.0F}};
    const auto originals = makeRoomWallColumns(bare);
    expect(originals.size() >= 4U,
        "large room has enough columns to test all reserved areas");
    if (originals.size() >= 4U) {
        bare.enemyZones.push_back({originals[0].bounds});
        bare.propZones.push_back({originals[1].bounds});
        bare.lootZones.push_back({originals[2].bounds});
        ConnectorInstance connector;
        const Vec3 midpoint = (originals[3].bounds.minimum
            + originals[3].bounds.maximum) * 0.5F;
        connector.worldPosition = {bare.worldBounds.minimum.x, 0.0F,
            midpoint.z};
        connector.worldNormal = {-1.0F, 0.0F, 0.0F};
        connector.width = 0.5F;
        bare.connectors.push_back(connector);
        const auto filtered = makeRoomWallColumns(bare);
        expect(filtered.size() == originals.size() - 4U,
            "enemy, prop, loot and connector reservations remove four columns");
    }
    bare.seed = 20U;
    bare.enemyZones.clear();
    bare.propZones.clear();
    bare.lootZones.clear();
    bare.connectors.clear();
    expect(makeRoomWallColumns(bare) != originals,
        "different room seeds vary column positions");
    bare.moduleId = centralHubPreviewModuleId;
    expect(makeRoomWallColumns(bare).empty(),
        "Central Hub keeps its existing authored wall pilasters only");
}

void testCentralHubPreviewArchitecture()
{
    using namespace wkc::core;
    using namespace gdp::world;

    constexpr Vec3 center{29.0F, 2.0F, -21.0F};
    constexpr std::array rotations{
        QuarterTurn::Degrees0,
        QuarterTurn::Degrees90,
        QuarterTurn::Degrees180,
        QuarterTurn::Degrees270,
    };
    const auto overlapsOpen = [](const Aabb a, const Aabb b) {
        return a.minimum.x < b.maximum.x && a.maximum.x > b.minimum.x
            && a.minimum.y < b.maximum.y && a.maximum.y > b.minimum.y
            && a.minimum.z < b.maximum.z && a.maximum.z > b.minimum.z;
    };
    std::vector<RoomArchitecturePart> unrotated;
    for (const QuarterTurn rotation : rotations) {
        const auto toWorld = [&](const Aabb local) {
            Vec3 minimum{1000.0F, center.y + local.minimum.y, 1000.0F};
            Vec3 maximum{-1000.0F, center.y + local.maximum.y, -1000.0F};
            for (const float x : {local.minimum.x, local.maximum.x}) {
                for (const float z : {local.minimum.z, local.maximum.z}) {
                    const Vec3 point = center
                        + rotateQuarterTurns(Vec3{x, 0.0F, z}, rotation);
                    minimum.x = std::min(minimum.x, point.x);
                    minimum.z = std::min(minimum.z, point.z);
                    maximum.x = std::max(maximum.x, point.x);
                    maximum.z = std::max(maximum.z, point.z);
                }
            }
            return Aabb{minimum, maximum};
        };
        RoomInstance room;
        room.role = RoomRole::Hub;
        room.theme = RoomTheme::Industrial;
        room.moduleId = centralHubPreviewModuleId;
        room.sizeClass = RoomSizeClass::Large;
        room.rotation = rotation;
        room.seed = 29U;
        room.worldBounds = toWorld({{-10.0F, 0.0F, -10.0F},
            {10.0F, 6.0F, 10.0F}});
        for (const auto& [position, normal] : std::array{
                 std::pair{Vec3{0.0F, 0.0F, -10.0F},
                     Vec3{0.0F, 0.0F, -1.0F}},
                 std::pair{Vec3{10.0F, 0.0F, 0.0F},
                     Vec3{1.0F, 0.0F, 0.0F}},
                 std::pair{Vec3{0.0F, 0.0F, 10.0F},
                     Vec3{0.0F, 0.0F, 1.0F}},
                 std::pair{Vec3{-10.0F, 0.0F, 0.0F},
                     Vec3{-1.0F, 0.0F, 0.0F}},
             }) {
            ConnectorInstance connector;
            connector.worldPosition = center
                + rotateQuarterTurns(position, rotation);
            connector.worldNormal = rotateQuarterTurns(normal, rotation);
            connector.width = 1.8F;
            connector.height = 2.4F;
            room.connectors.push_back(connector);
        }
        room.enemyZones.push_back({toWorld({{-6.0F, 0.0F, -6.0F},
            {-5.0F, 0.0F, -5.0F}})});
        room.enemyZones.push_back({toWorld({{5.0F, 0.0F, 5.0F},
            {6.0F, 0.0F, 6.0F}})});
        room.propZones.push_back({toWorld({{5.0F, 0.0F, -1.0F},
            {6.0F, 0.0F, 1.0F}})});
        room.lootZones.push_back({toWorld({{-6.0F, 0.0F, -1.0F},
            {-5.0F, 0.0F, 1.0F}})});

        const auto parts = makeRoomArchitecture(room);
        RoomInstance differentSeed = room;
        differentSeed.seed = 1001U;
        expect(parts.size() == 11U
                && parts == makeRoomArchitecture(room)
                && parts == makeRoomArchitecture(differentSeed),
            "Central Hub preview is bounded and seed independent");
        const std::array routes{
            toWorld({{-3.5F, 0.0F, -10.0F},
                {3.5F, 2.4F, -1.55F}}),
            toWorld({{-3.5F, 0.0F, 1.55F},
                {3.5F, 2.4F, 10.0F}}),
            toWorld({{-10.0F, 0.0F, -1.2F},
                {-1.55F, 2.4F, 1.2F}}),
            toWorld({{1.55F, 0.0F, -1.2F},
                {10.0F, 2.4F, 1.2F}}),
            toWorld({{-4.0F, 0.0F, -4.0F},
                {-1.55F, 2.4F, 4.0F}}),
            toWorld({{1.55F, 0.0F, -4.0F},
                {4.0F, 2.4F, 4.0F}}),
            toWorld({{-4.0F, 0.0F, -4.0F},
                {4.0F, 2.4F, -1.55F}}),
            toWorld({{-4.0F, 0.0F, 1.55F},
                {4.0F, 2.4F, 4.0F}}),
            toWorld({{-0.5F, 0.0F, 3.5F},
                {0.5F, 2.2F, 4.5F}}),
            toWorld({{-0.5F, 0.0F, 8.0F},
                {0.5F, 2.2F, 9.0F}}),
        };
        bool contained = true;
        bool routesClear = true;
        bool noIntersections = true;
        for (std::size_t first = 0U; first < parts.size(); ++first) {
            const RoomArchitecturePart& part = parts[first];
            contained &= part.solid && !part.breakable
                && part.bounds.isValid()
                && room.worldBounds.contains(part.bounds.minimum)
                && room.worldBounds.contains(part.bounds.maximum);
            for (const Aabb route : routes) {
                routesClear &= !overlapsOpen(part.bounds, route);
            }
            for (std::size_t second = first + 1U;
                 second < parts.size(); ++second) {
                noIntersections &= !overlapsOpen(part.bounds,
                    parts[second].bounds);
            }
        }
        const bool landmarkSupported = parts.size() >= 7U
            && nearlyEqual(parts[0].bounds.minimum.y, center.y, 1.0e-4F)
            && nearlyEqual(parts[0].bounds.maximum.y,
                parts[1].bounds.minimum.y, 1.0e-4F)
            && nearlyEqual(parts[1].bounds.maximum.y,
                parts[2].bounds.minimum.y, 1.0e-4F)
            && parts[1].decoration == RoomDecorationPart::CentralHubColumnShaft
            && std::all_of(parts.begin() + 3, parts.begin() + 7,
                [](const RoomArchitecturePart& part) {
                    return part.decoration
                        == RoomDecorationPart::CentralHubRouteDisplay;
                });
        bool directionPlatesTouch = parts.size() >= 7U;
        if (directionPlatesTouch) {
            const Aabb shaft = parts[1].bounds;
            for (std::size_t index = 3U; index < 7U; ++index) {
                const Aabb plate = parts[index].bounds;
                const bool touchesX = nearlyEqual(plate.maximum.x,
                        shaft.minimum.x, 1.0e-4F)
                    || nearlyEqual(plate.minimum.x,
                        shaft.maximum.x, 1.0e-4F);
                const bool touchesZ = nearlyEqual(plate.maximum.z,
                        shaft.minimum.z, 1.0e-4F)
                    || nearlyEqual(plate.minimum.z,
                        shaft.maximum.z, 1.0e-4F);
                const bool xOverlaps = plate.minimum.x < shaft.maximum.x
                    && plate.maximum.x > shaft.minimum.x;
                const bool zOverlaps = plate.minimum.z < shaft.maximum.z
                    && plate.maximum.z > shaft.minimum.z;
                directionPlatesTouch &= plate.minimum.y < shaft.maximum.y
                    && plate.maximum.y > shaft.minimum.y
                    && ((touchesX && zOverlaps)
                        || (touchesZ && xOverlaps));
            }
        }
        const bool bypassesConnect = overlapsOpen(routes[0], routes[4])
            && overlapsOpen(routes[0], routes[5])
            && overlapsOpen(routes[1], routes[4])
            && overlapsOpen(routes[1], routes[5])
            && overlapsOpen(routes[2], routes[4])
            && overlapsOpen(routes[3], routes[5])
            && overlapsOpen(routes[4], routes[6])
            && overlapsOpen(routes[4], routes[7])
            && overlapsOpen(routes[5], routes[6])
            && overlapsOpen(routes[5], routes[7]);
        expect(contained && routesClear && noIntersections
                && landmarkSupported && directionPlatesTouch
                && bypassesConnect
                && validateRoomArchitecture(room, parts),
            "Central Hub four doors connect around a supported landmark without blocking zones or terminal");
        if (rotation == QuarterTurn::Degrees0) {
            unrotated = parts;
        } else {
            bool followsRotation = parts.size() == unrotated.size();
            if (followsRotation) {
                for (std::size_t index = 0U; index < parts.size(); ++index) {
                    const Vec3 original = (unrotated[index].bounds.minimum
                        + unrotated[index].bounds.maximum) * 0.5F;
                    const Vec3 actual = (parts[index].bounds.minimum
                        + parts[index].bounds.maximum) * 0.5F;
                    followsRotation &= nearlyEqual(actual, center
                        + rotateQuarterTurns(original - center, rotation),
                        1.0e-4F);
                }
            }
            expect(followsRotation,
                "Central Hub preview follows all quarter-turn rotations");
        }
        auto blocked = parts;
        blocked.front().bounds = toWorld({{-5.8F, 0.0F, -5.8F},
            {-5.2F, 1.5F, -5.2F}});
        expect(!validateRoomArchitecture(room, blocked),
            "Central Hub rejects occupied enemy zone");
        blocked.front().bounds = toWorld({{5.2F, 0.0F, -0.4F},
            {5.8F, 1.5F, 0.4F}});
        expect(!validateRoomArchitecture(room, blocked),
            "Central Hub rejects occupied prop zone");
        blocked.front().bounds = toWorld({{-5.8F, 0.0F, -0.4F},
            {-5.2F, 1.5F, 0.4F}});
        expect(!validateRoomArchitecture(room, blocked),
            "Central Hub rejects occupied loot zone");
        blocked.front().bounds = toWorld({{-0.3F, 0.0F, -9.8F},
            {0.3F, 1.5F, -9.3F}});
        expect(!validateRoomArchitecture(room, blocked),
            "Central Hub rejects blocked connector");
    }

    RoomInstance ordinary;
    ordinary.role = RoomRole::Hub;
    ordinary.worldBounds = {{-10.0F, 0.0F, -10.0F},
        {10.0F, 6.0F, 10.0F}};
    expect(makeRoomArchitecture(ordinary).empty(),
        "Campaign Hub rooms retain generic architecture");
    ordinary.moduleId = centralHubPreviewModuleId;
    ordinary.worldBounds = {{-9.9F, 0.0F, -10.0F},
        {9.9F, 6.0F, 10.0F}};
    expect(makeRoomArchitecture(ordinary).empty(),
        "Central Hub preview rejects an undersized room");
}

void testSecurityPreviewArchitecture()
{
    using namespace wkc::core;
    using namespace gdp::world;

    constexpr Vec3 center{17.0F, 2.0F, -13.0F};
    constexpr std::array rotations{
        QuarterTurn::Degrees0,
        QuarterTurn::Degrees90,
        QuarterTurn::Degrees180,
        QuarterTurn::Degrees270,
    };
    const auto overlapsOpen = [](const Aabb a, const Aabb b) {
        return a.minimum.x < b.maximum.x && a.maximum.x > b.minimum.x
            && a.minimum.y < b.maximum.y && a.maximum.y > b.minimum.y
            && a.minimum.z < b.maximum.z && a.maximum.z > b.minimum.z;
    };
    std::vector<RoomArchitecturePart> unrotated;
    for (const QuarterTurn rotation : rotations) {
        const auto toWorld = [&](const Aabb local) {
            Vec3 minimum{1000.0F, center.y + local.minimum.y, 1000.0F};
            Vec3 maximum{-1000.0F, center.y + local.maximum.y, -1000.0F};
            for (const float x : {local.minimum.x, local.maximum.x}) {
                for (const float z : {local.minimum.z, local.maximum.z}) {
                    const Vec3 point = center
                        + rotateQuarterTurns(Vec3{x, 0.0F, z}, rotation);
                    minimum.x = std::min(minimum.x, point.x);
                    minimum.z = std::min(minimum.z, point.z);
                    maximum.x = std::max(maximum.x, point.x);
                    maximum.z = std::max(maximum.z, point.z);
                }
            }
            return Aabb{minimum, maximum};
        };
        RoomInstance room;
        room.theme = RoomTheme::Security;
        room.moduleId = securityPreviewModuleId;
        room.sizeClass = RoomSizeClass::Large;
        room.rotation = rotation;
        room.seed = 31U;
        room.worldBounds = toWorld({{-9.0F, 0.0F, -9.0F},
            {9.0F, 6.0F, 9.0F}});
        for (const auto& [position, normal] : std::array{
                 std::pair{Vec3{0.0F, 0.0F, -9.0F},
                     Vec3{0.0F, 0.0F, -1.0F}},
                 std::pair{Vec3{9.0F, 0.0F, 0.0F},
                     Vec3{1.0F, 0.0F, 0.0F}},
                 std::pair{Vec3{0.0F, 0.0F, 9.0F},
                     Vec3{0.0F, 0.0F, 1.0F}},
                 std::pair{Vec3{-9.0F, 0.0F, 0.0F},
                     Vec3{-1.0F, 0.0F, 0.0F}},
             }) {
            ConnectorInstance connector;
            connector.worldPosition = center
                + rotateQuarterTurns(position, rotation);
            connector.worldNormal = rotateQuarterTurns(normal, rotation);
            connector.width = 1.8F;
            connector.height = 2.4F;
            room.connectors.push_back(connector);
        }
        room.enemyZones.push_back({toWorld({{-0.8F, 0.0F, -6.4F},
            {0.8F, 0.0F, -5.4F}})});
        room.enemyZones.push_back({toWorld({{-0.8F, 0.0F, 5.4F},
            {0.8F, 0.0F, 6.4F}})});
        room.propZones.push_back({toWorld({{3.0F, 0.0F, -0.8F},
            {4.0F, 0.0F, 0.8F}})});
        room.lootZones.push_back({toWorld({{-4.0F, 0.0F, -0.8F},
            {-3.0F, 0.0F, 0.8F}})});

        const auto parts = makeRoomArchitecture(room);
        RoomInstance differentSeed = room;
        differentSeed.seed = 1001U;
        expect(parts.size() == 18U
                && parts == makeRoomArchitecture(room)
                && parts == makeRoomArchitecture(differentSeed),
            "Security preview geometry is bounded and seed independent");
        const Aabb mainLane = toWorld({{-1.5F, 0.0F, -9.0F},
            {1.5F, 2.4F, 9.0F}});
        const Aabb transverseLane = toWorld({{-9.0F, 0.0F, -1.8F},
            {9.0F, 2.4F, 1.8F}});
        const Aabb leftFlank = toWorld({{-7.2F, 0.0F, -7.2F},
            {-5.2F, 2.4F, 7.2F}});
        const Aabb rightFlank = toWorld({{5.2F, 0.0F, -7.2F},
            {7.8F, 2.4F, 7.2F}});
        const Aabb returnTerminal = toWorld({{-0.5F, 0.0F, 7.0F},
            {0.5F, 2.2F, 8.0F}});
        const QuarterTurn inverseRotation = rotation == QuarterTurn::Degrees90
            ? QuarterTurn::Degrees270
            : rotation == QuarterTurn::Degrees270
                ? QuarterTurn::Degrees90 : rotation;
        bool contained = true;
        bool routesClear = true;
        bool noIntersections = true;
        std::size_t supportedScreens = 0U;
        std::size_t barricades = 0U;
        std::size_t nichePieces = 0U;
        for (std::size_t first = 0U; first < parts.size(); ++first) {
            const RoomArchitecturePart& part = parts[first];
            contained &= part.solid
                && (!part.breakable || part.securityFurniturePart
                    == SecurityFurniturePart::Monitor)
                && part.bounds.isValid()
                && room.worldBounds.contains(part.bounds.minimum)
                && room.worldBounds.contains(part.bounds.maximum);
            routesClear &= !overlapsOpen(part.bounds, mainLane)
                && !overlapsOpen(part.bounds, transverseLane)
                && !overlapsOpen(part.bounds, leftFlank)
                && !overlapsOpen(part.bounds, rightFlank)
                && !overlapsOpen(part.bounds, returnTerminal);
            const Vec3 midpoint = (part.bounds.minimum
                + part.bounds.maximum) * 0.5F;
            const Vec3 local = rotateQuarterTurns(midpoint - center,
                inverseRotation);
            if (part.bounds.minimum.y == center.y
                && nearlyEqual(part.bounds.maximum.y,
                    center.y + 1.05F, 1.0e-4F)) {
                ++barricades;
                routesClear &= nearlyEqual(local.x,
                        local.x < 0.0F ? -4.20F : 4.20F, 1.0e-4F)
                    && nearlyEqual(local.z,
                        local.x < 0.0F ? 3.80F : -3.80F, 1.0e-4F);
            }
            if (part.securityFurniturePart == SecurityFurniturePart::Monitor) {
                bool supported = false;
                for (const RoomArchitecturePart& candidate : parts) {
                    supported |= nearlyEqual(
                            part.bounds.minimum.y,
                            candidate.bounds.maximum.y, 1.0e-4F)
                        && part.bounds.minimum.x < candidate.bounds.maximum.x
                        && part.bounds.maximum.x > candidate.bounds.minimum.x
                        && part.bounds.minimum.z < candidate.bounds.maximum.z
                        && part.bounds.maximum.z > candidate.bounds.minimum.z;
                }
                supportedScreens += static_cast<std::size_t>(supported);
            }
            if (local.x > 8.40F && local.z > 2.30F && local.z < 5.90F) {
                ++nichePieces;
            }
            for (std::size_t second = first + 1U;
                 second < parts.size(); ++second) {
                noIntersections &= !overlapsOpen(part.bounds,
                    parts[second].bounds);
            }
        }
        expect(contained && routesClear && noIntersections
                && barricades == 2U && supportedScreens == 2U
                && nichePieces == 5U
                && validateRoomArchitecture(room, parts),
            "Security preview keeps doors, reserved zones, terminal and circulation clear");
        if (rotation == QuarterTurn::Degrees0) {
            unrotated = parts;
        } else {
            bool followsRotation = parts.size() == unrotated.size();
            if (followsRotation) {
                for (std::size_t index = 0U; index < parts.size(); ++index) {
                    const Vec3 original = (unrotated[index].bounds.minimum
                        + unrotated[index].bounds.maximum) * 0.5F;
                    const Vec3 actual = (parts[index].bounds.minimum
                        + parts[index].bounds.maximum) * 0.5F;
                    followsRotation &= nearlyEqual(actual, center
                        + rotateQuarterTurns(original - center, rotation),
                        1.0e-4F);
                }
            }
            expect(followsRotation,
                "Security preview follows all quarter-turn rotations");
        }
        auto blocked = parts;
        blocked.front().bounds = toWorld({{-0.3F, 0.0F, 5.6F},
            {0.3F, 1.5F, 6.0F}});
        expect(!validateRoomArchitecture(room, blocked),
            "Security preview rejects occupied enemy zone");
        blocked.front().bounds = toWorld({{3.2F, 0.0F, -0.3F},
            {3.8F, 1.5F, 0.3F}});
        expect(!validateRoomArchitecture(room, blocked),
            "Security preview rejects occupied prop zone");
        blocked.front().bounds = toWorld({{-3.8F, 0.0F, -0.3F},
            {-3.2F, 1.5F, 0.3F}});
        expect(!validateRoomArchitecture(room, blocked),
            "Security preview rejects occupied loot zone");
        blocked.front().bounds = toWorld({{-0.3F, 0.0F, -8.8F},
            {0.3F, 1.5F, -8.3F}});
        expect(!validateRoomArchitecture(room, blocked),
            "Security preview rejects blocked rear connector");
    }

    RoomInstance ordinary;
    ordinary.theme = RoomTheme::Security;
    ordinary.worldBounds = {{-9.0F, 0.0F, -9.0F},
        {9.0F, 6.0F, 9.0F}};
    expect(!makeRoomArchitecture(ordinary).empty(),
        "Security campaign variants receive a compact authored station");
    ordinary.moduleId = securityPreviewModuleId;
    ordinary.worldBounds = {{-8.9F, 0.0F, -9.0F},
        {8.9F, 6.0F, 9.0F}};
    expect(makeRoomArchitecture(ordinary).empty(),
        "Security preview rejects an undersized room");
}

void testTechnicalPreviewArchitecture()
{
    using namespace wkc::core;
    using namespace gdp::world;

    constexpr Vec3 center{19.0F, 3.0F, -17.0F};
    constexpr std::array rotations{
        QuarterTurn::Degrees0,
        QuarterTurn::Degrees90,
        QuarterTurn::Degrees180,
        QuarterTurn::Degrees270,
    };
    const auto overlapsOpen = [](const Aabb a, const Aabb b) {
        return a.minimum.x < b.maximum.x && a.maximum.x > b.minimum.x
            && a.minimum.y < b.maximum.y && a.maximum.y > b.minimum.y
            && a.minimum.z < b.maximum.z && a.maximum.z > b.minimum.z;
    };
    std::vector<RoomArchitecturePart> unrotated;
    for (const QuarterTurn rotation : rotations) {
        const auto toWorld = [&](const Aabb local) {
            Vec3 minimum{1000.0F, center.y + local.minimum.y, 1000.0F};
            Vec3 maximum{-1000.0F, center.y + local.maximum.y, -1000.0F};
            for (const float x : {local.minimum.x, local.maximum.x}) {
                for (const float z : {local.minimum.z, local.maximum.z}) {
                    const Vec3 point = center
                        + rotateQuarterTurns(Vec3{x, 0.0F, z}, rotation);
                    minimum.x = std::min(minimum.x, point.x);
                    minimum.z = std::min(minimum.z, point.z);
                    maximum.x = std::max(maximum.x, point.x);
                    maximum.z = std::max(maximum.z, point.z);
                }
            }
            return Aabb{minimum, maximum};
        };
        RoomInstance room;
        room.theme = RoomTheme::Maintenance;
        room.moduleId = technicalPreviewModuleId;
        room.sizeClass = RoomSizeClass::Large;
        room.rotation = rotation;
        room.seed = 5U;
        room.worldBounds = toWorld({{-8.0F, 0.0F, -10.0F},
            {8.0F, 5.5F, 10.0F}});
        for (const auto& [position, normal] : std::array{
                 std::pair{Vec3{0.0F, 0.0F, -10.0F},
                     Vec3{0.0F, 0.0F, -1.0F}},
                 std::pair{Vec3{8.0F, 0.0F, 0.0F},
                     Vec3{1.0F, 0.0F, 0.0F}},
                 std::pair{Vec3{0.0F, 0.0F, 10.0F},
                     Vec3{0.0F, 0.0F, 1.0F}},
                 std::pair{Vec3{-8.0F, 0.0F, 0.0F},
                     Vec3{-1.0F, 0.0F, 0.0F}},
             }) {
            ConnectorInstance connector;
            connector.worldPosition = center
                + rotateQuarterTurns(position, rotation);
            connector.worldNormal = rotateQuarterTurns(normal, rotation);
            connector.width = 1.8F;
            connector.height = 2.4F;
            room.connectors.push_back(connector);
        }
        for (const float z : {-6.0F, 6.0F}) {
            room.enemyZones.push_back({toWorld({{-0.8F, 0.0F, z - 0.6F},
                {0.8F, 0.0F, z + 0.6F}})});
        }
        room.propZones.push_back({toWorld({{3.0F, 0.0F, -0.8F},
            {4.0F, 0.0F, 0.8F}})});
        room.lootZones.push_back({toWorld({{-4.0F, 0.0F, -0.8F},
            {-3.0F, 0.0F, 0.8F}})});

        const auto parts = makeRoomArchitecture(room);
        const std::array expectedDucts{
            toWorld({{-6.11F, 5.05F, -10.0F},
                {-5.59F, 5.33F, 10.0F}}),
            toWorld({{5.59F, 5.05F, -10.0F},
                {6.11F, 5.33F, 10.0F}}),
        };
        expect(std::count_if(parts.begin(), parts.end(),
                   [](const RoomArchitecturePart& part) {
                       return part.decoration == RoomDecorationPart::CeilingDuct;
                   }) == 2
                && std::all_of(expectedDucts.begin(), expectedDucts.end(),
                    [&](const Aabb expected) {
                        return std::any_of(parts.begin(), parts.end(),
                            [&](const RoomArchitecturePart& part) {
                                return part.decoration
                                        == RoomDecorationPart::CeilingDuct
                                    && nearlyEqual(part.bounds.minimum,
                                        expected.minimum, 1.0e-4F)
                                    && nearlyEqual(part.bounds.maximum,
                                        expected.maximum, 1.0e-4F);
                            });
                    }),
            "Technical ceiling ducts meet both walls in every rotation");
        RoomInstance differentSeed = room;
        differentSeed.seed = 555U;
        expect(parts.size() == 38U
                && parts == makeRoomArchitecture(room)
                && parts == makeRoomArchitecture(differentSeed),
            "Technical preview has bounded seed-independent geometry");
        const Aabb centralLane = toWorld({{-2.0F, 0.0F, -10.0F},
            {2.0F, 2.4F, 10.0F}});
        const Aabb sideDoorLane = toWorld({{-8.0F, 0.0F, -1.8F},
            {8.0F, 2.4F, 1.8F}});
        const Aabb leftFlank = toWorld({{-6.30F, 0.0F, -8.2F},
            {-4.50F, 2.4F, 8.2F}});
        const Aabb rightFlank = toWorld({{4.50F, 0.0F, -8.2F},
            {6.30F, 2.4F, 8.2F}});
        const Aabb returnTerminal = toWorld({{-0.5F, 0.0F, 8.0F},
            {0.5F, 2.2F, 9.0F}});
        const QuarterTurn inverseRotation = rotation == QuarterTurn::Degrees90
            ? QuarterTurn::Degrees270
            : rotation == QuarterTurn::Degrees270
                ? QuarterTurn::Degrees90 : rotation;
        bool contained = true;
        bool routesClear = true;
        bool noIntersections = true;
        bool stationsSupported = true;
        bool onlyModulesBreakable = true;
        std::size_t leftStationParts = 0U;
        std::size_t rightStationParts = 0U;
        std::size_t technicalModules = 0U;
        std::size_t breakableModules = 0U;
        for (std::size_t first = 0U; first < parts.size(); ++first) {
            const RoomArchitecturePart& part = parts[first];
            contained &= part.solid && part.bounds.isValid()
                && room.worldBounds.contains(part.bounds.minimum)
                && room.worldBounds.contains(part.bounds.maximum);
            routesClear &= !overlapsOpen(part.bounds, centralLane)
                && !overlapsOpen(part.bounds, sideDoorLane)
                && !overlapsOpen(part.bounds, leftFlank)
                && !overlapsOpen(part.bounds, rightFlank)
                && !overlapsOpen(part.bounds, returnTerminal);
            const Vec3 midpoint = (part.bounds.minimum
                + part.bounds.maximum) * 0.5F;
            const Vec3 local = rotateQuarterTurns(midpoint - center,
                inverseRotation);
            bool isTechnicalModule = false;
            if (part.bounds.maximum.y <= center.y + 1.33F
                && std::abs(local.x) > 2.4F && std::abs(local.x) < 3.6F
                && std::abs(local.z) > 2.5F && std::abs(local.z) < 5.0F) {
                (local.x < 0.0F ? leftStationParts : rightStationParts)++;
                isTechnicalModule = part.bounds.minimum.y
                    >= center.y + 0.92F - 1.0e-4F;
                technicalModules += static_cast<std::size_t>(
                    isTechnicalModule);
                routesClear &= nearlyEqual(local.x,
                        local.x < 0.0F
                            ? (isTechnicalModule ? -3.25F : -3.0F)
                            : (isTechnicalModule ? 3.25F : 3.0F),
                        1.0e-4F)
                    && nearlyEqual(local.z,
                        local.x < 0.0F ? -3.80F : 3.80F, 1.0e-4F);
                if (part.bounds.minimum.y > center.y + 1.0e-4F) {
                    bool touchesSupport = false;
                    for (const RoomArchitecturePart& candidate : parts) {
                        touchesSupport |= nearlyEqual(
                                part.bounds.minimum.y,
                                candidate.bounds.maximum.y, 1.0e-4F)
                            && part.bounds.minimum.x < candidate.bounds.maximum.x
                            && part.bounds.maximum.x > candidate.bounds.minimum.x
                            && part.bounds.minimum.z < candidate.bounds.maximum.z
                            && part.bounds.maximum.z > candidate.bounds.minimum.z;
                    }
                    stationsSupported &= touchesSupport;
                }
            }
            onlyModulesBreakable &= part.breakable == isTechnicalModule;
            if (part.breakable) {
                ++breakableModules;
                const Vec3 expectedTint = local.x < 0.0F
                    ? Vec3{0.44F, 0.43F, 0.30F}
                    : Vec3{0.34F, 0.35F, 0.34F};
                onlyModulesBreakable &= nearlyEqual(part.tint,
                    expectedTint, 1.0e-4F);
            }
            for (std::size_t second = first + 1U;
                 second < parts.size(); ++second) {
                noIntersections &= !overlapsOpen(part.bounds,
                    parts[second].bounds);
            }
        }
        expect(contained && routesClear && noIntersections
                && stationsSupported && onlyModulesBreakable
                && leftStationParts == 3U && rightStationParts == 3U
                && technicalModules == 2U && breakableModules == 2U
                && validateRoomArchitecture(room, parts),
            "Technical preview keeps doors, zones, terminal and three long lanes clear");
        if (rotation == QuarterTurn::Degrees0) {
            unrotated = parts;
        } else {
            bool followsRotation = parts.size() == unrotated.size();
            if (followsRotation) {
                for (std::size_t index = 0U; index < parts.size(); ++index) {
                    const Vec3 original = (unrotated[index].bounds.minimum
                        + unrotated[index].bounds.maximum) * 0.5F;
                    const Vec3 actual = (parts[index].bounds.minimum
                        + parts[index].bounds.maximum) * 0.5F;
                    followsRotation &= nearlyEqual(actual, center
                        + rotateQuarterTurns(original - center, rotation),
                        1.0e-4F);
                }
            }
            expect(followsRotation,
                "Technical preview follows all quarter-turn rotations");
        }
        auto blocked = parts;
        blocked.front().bounds = toWorld({{-0.3F, 0.0F, 5.8F},
            {0.3F, 1.5F, 6.2F}});
        expect(!validateRoomArchitecture(room, blocked),
            "Technical preview rejects enemy zone obstruction");
        blocked.front().bounds = toWorld({{3.2F, 0.0F, -0.3F},
            {3.8F, 1.5F, 0.3F}});
        expect(!validateRoomArchitecture(room, blocked),
            "Technical preview rejects prop zone obstruction");
        blocked.front().bounds = toWorld({{-3.8F, 0.0F, -0.3F},
            {-3.2F, 1.5F, 0.3F}});
        expect(!validateRoomArchitecture(room, blocked),
            "Technical preview rejects loot zone obstruction");
        blocked.front().bounds = toWorld({{-0.3F, 0.0F, 9.3F},
            {0.3F, 1.5F, 9.8F}});
        expect(!validateRoomArchitecture(room, blocked),
            "Technical preview rejects a blocked connector");
    }

    RoomInstance ordinary;
    ordinary.theme = RoomTheme::Maintenance;
    ordinary.worldBounds = {{-8.0F, 0.0F, -10.0F},
        {8.0F, 5.5F, 10.0F}};
    expect(!makeRoomArchitecture(ordinary).empty(),
        "Maintenance campaign variants receive a compact authored station");
    ordinary.moduleId = technicalPreviewModuleId;
    ordinary.worldBounds = {{-7.9F, 0.0F, -10.0F},
        {7.9F, 5.5F, 10.0F}};
    expect(makeRoomArchitecture(ordinary).empty(),
        "Technical preview rejects a room below its reserved footprint");
}

void testMaintenanceLAtFractionalCoordinates()
{
    using namespace gdp::world;
    using wkc::core::QuarterTurn;

    const auto library = makeDefaultRoomModuleLibrary();
    const auto module = std::find_if(library.begin(), library.end(),
        [](const RoomModule& candidate) {
            return candidate.id == maintenanceLModuleId;
        });
    expect(module != library.end(), "Maintenance L module is in the campaign library");
    if (module == library.end()) return;

    for (const QuarterTurn rotation : {QuarterTurn::Degrees0,
             QuarterTurn::Degrees90, QuarterTurn::Degrees180,
             QuarterTurn::Degrees270}) {
        const auto room = instantiateRoom(*module, 6U, RoomRole::Normal,
            722U, {-15.85F, 0.0F, 10.3F}, rotation);
        expect(room.has_value(), "Maintenance L instantiates at fractional coordinates");
        if (!room) continue;
        const auto parts = makeRoomArchitecture(*room);
        expect(!parts.empty() && validateRoomArchitecture(*room, parts),
            "Maintenance L keeps furniture and ducts at fractional coordinates");
    }
}

void testCampaignArchitectureAtFractionalCoordinates()
{
    using namespace gdp::world;
    using wkc::core::QuarterTurn;

    const auto library = makeDefaultRoomModuleLibrary();
    bool allValid = !library.empty();
    for (const RoomModule& module : library) {
        RoomRole role = RoomRole::Normal;
        for (const RoomRole candidate : {RoomRole::Start, RoomRole::Normal,
                 RoomRole::Hub, RoomRole::Arena, RoomRole::DeadEnd,
                 RoomRole::Secret, RoomRole::Boss, RoomRole::Terminal}) {
            if (module.supportsRole(candidate)) {
                role = candidate;
                break;
            }
        }
        for (const QuarterTurn rotation : {QuarterTurn::Degrees0,
                 QuarterTurn::Degrees90, QuarterTurn::Degrees180,
                 QuarterTurn::Degrees270}) {
            const auto room = instantiateRoom(module, module.id, role,
                722U, {-15.85F, 0.0F, 10.3F}, rotation);
            if (!room) {
                allValid = false;
                continue;
            }
            const auto parts = makeRoomArchitecture(*room);
            allValid &= validateRoomArchitecture(*room, parts);
        }
    }
    expect(allValid,
        "every campaign room variant keeps valid architecture across rotations");
}

void testLaboratoryPreviewArchitecture()
{
    using namespace wkc::core;
    using namespace gdp::world;

    constexpr Vec3 center{23.0F, 2.0F, -19.0F};
    constexpr std::array rotations{
        QuarterTurn::Degrees0,
        QuarterTurn::Degrees90,
        QuarterTurn::Degrees180,
        QuarterTurn::Degrees270,
    };
    const auto overlapsOpen = [](const Aabb first, const Aabb second) {
        return first.minimum.x < second.maximum.x
            && first.maximum.x > second.minimum.x
            && first.minimum.y < second.maximum.y
            && first.maximum.y > second.minimum.y
            && first.minimum.z < second.maximum.z
            && first.maximum.z > second.minimum.z;
    };
    std::vector<RoomArchitecturePart> unrotated;
    for (const QuarterTurn rotation : rotations) {
        const auto toWorld = [&](const Aabb local) {
            Vec3 minimum{1000.0F, center.y + local.minimum.y, 1000.0F};
            Vec3 maximum{-1000.0F, center.y + local.maximum.y, -1000.0F};
            for (const float x : {local.minimum.x, local.maximum.x}) {
                for (const float z : {local.minimum.z, local.maximum.z}) {
                    const Vec3 point = center
                        + rotateQuarterTurns(Vec3{x, 0.0F, z}, rotation);
                    minimum.x = std::min(minimum.x, point.x);
                    minimum.z = std::min(minimum.z, point.z);
                    maximum.x = std::max(maximum.x, point.x);
                    maximum.z = std::max(maximum.z, point.z);
                }
            }
            return Aabb{minimum, maximum};
        };
        RoomInstance room;
        room.theme = RoomTheme::Laboratory;
        room.moduleId = laboratoryPreviewModuleId;
        room.sizeClass = RoomSizeClass::Large;
        room.rotation = rotation;
        room.seed = 7U;
        room.worldBounds = toWorld({{-9.0F, 0.0F, -10.0F},
            {9.0F, 6.0F, 10.0F}});
        for (const auto& [position, normal] : std::array{
                 std::pair{Vec3{0.0F, 0.0F, -10.0F},
                     Vec3{0.0F, 0.0F, -1.0F}},
                 std::pair{Vec3{9.0F, 0.0F, 0.0F},
                     Vec3{1.0F, 0.0F, 0.0F}},
                 std::pair{Vec3{0.0F, 0.0F, 10.0F},
                     Vec3{0.0F, 0.0F, 1.0F}},
                 std::pair{Vec3{-9.0F, 0.0F, 0.0F},
                     Vec3{-1.0F, 0.0F, 0.0F}},
             }) {
            ConnectorInstance connector;
            connector.worldPosition = center
                + rotateQuarterTurns(position, rotation);
            connector.worldNormal = rotateQuarterTurns(normal, rotation);
            connector.width = 1.8F;
            connector.height = 2.4F;
            room.connectors.push_back(connector);
        }
        room.enemyZones.push_back({toWorld({{-0.8F, 0.0F, -6.5F},
            {0.8F, 0.0F, -5.3F}})});
        room.enemyZones.push_back({toWorld({{-0.8F, 0.0F, 5.3F},
            {0.8F, 0.0F, 6.5F}})});
        room.propZones.push_back({toWorld({{3.0F, 0.0F, -0.8F},
            {4.0F, 0.0F, 0.8F}})});
        room.lootZones.push_back({toWorld({{-4.0F, 0.0F, 3.0F},
            {-3.0F, 0.0F, 4.0F}})});

        const auto parts = makeRoomArchitecture(room);
        const auto columns = makeRoomWallColumns(room);
        const auto shafts = std::count_if(columns.begin(), columns.end(),
            [](const RoomArchitecturePart& part) {
                return part.structuralColumn && part.solid;
            });
        const auto indicators = std::count_if(columns.begin(), columns.end(),
            [](const RoomArchitecturePart& part) {
                return part.decoration
                    == RoomDecorationPart::LaboratoryColumnIndicator;
            });
        bool columnsSafe = shafts >= 4 && indicators == shafts * 2;
        for (const RoomArchitecturePart& column : columns) {
            columnsSafe &= room.worldBounds.contains(column.bounds.minimum)
                && room.worldBounds.contains(column.bounds.maximum);
            if (!column.solid) continue;
            for (const RoomArchitecturePart& furniture : parts) {
                columnsSafe &= !overlapsOpen(column.bounds, furniture.bounds);
            }
            for (const ConnectorInstance& door : room.connectors) {
                const Vec3 midpoint = (column.bounds.minimum
                    + column.bounds.maximum) * 0.5F;
                const float along = std::abs(door.worldNormal.x) > 0.5F
                    ? std::abs(midpoint.z - door.worldPosition.z)
                    : std::abs(midpoint.x - door.worldPosition.x);
                const float normal = std::abs(door.worldNormal.x) > 0.5F
                    ? std::abs(midpoint.x - door.worldPosition.x)
                    : std::abs(midpoint.z - door.worldPosition.z);
                columnsSafe &= normal > 0.5F
                    || along > door.width * 0.5F + 0.31F;
            }
        }
        expect(columnsSafe,
            "Laboratory 3D wall columns and lights clear furniture and doors in every rotation");
        RoomInstance differentSeed = room;
        differentSeed.seed = 999U;
        expect(parts.size() == 41U
                && parts == makeRoomArchitecture(room)
                && parts == makeRoomArchitecture(differentSeed),
            "Laboratory preview parts are bounded and seed independent");
        expect(std::count_if(parts.begin(), parts.end(),
                   [](const RoomArchitecturePart& part) {
                       return part.decoration == RoomDecorationPart::CeilingDuct;
                   }) == 2U
                && std::count_if(parts.begin(), parts.end(),
                       [](const RoomArchitecturePart& part) {
                           return part.decoration
                               == RoomDecorationPart::GlassTestPlanter;
                       }) == 1U
                && std::count_if(parts.begin(), parts.end(),
                       [&](const RoomArchitecturePart& part) {
                           return part.decoration
                                   == RoomDecorationPart::SmallGlassTestPlanter
                               && nearlyEqual(part.bounds.minimum.y,
                                   center.y + 0.92F, 1.0e-4F);
                       }) == 4U,
            "Laboratory has extraction ducts, a corner specimen and four tabletop specimens");
        const Aabb mainLane = toWorld({{-2.8F, 0.0F, -10.0F},
            {2.8F, 2.4F, 10.0F}});
        const Aabb sideDoorLane = toWorld({{-9.0F, 0.0F, -1.8F},
            {9.0F, 2.4F, 1.8F}});
        const Aabb leftFlank = toWorld({{-7.0F, 0.0F, -8.3F},
            {-5.25F, 2.4F, 8.3F}});
        const Aabb rightFlank = toWorld({{5.25F, 0.0F, -8.3F},
            {7.0F, 2.4F, 8.3F}});
        const Aabb returnTerminal = toWorld({{-0.5F, 0.0F, 8.0F},
            {0.5F, 2.2F, 9.0F}});
        const QuarterTurn inverseRotation = rotation == QuarterTurn::Degrees90
            ? QuarterTurn::Degrees270
            : rotation == QuarterTurn::Degrees270
                ? QuarterTurn::Degrees90 : rotation;
        bool partsContained = true;
        bool routesClear = true;
        bool solidsSeparated = true;
        std::size_t leftIslandParts = 0U;
        std::size_t rightIslandParts = 0U;
        for (std::size_t first = 0U; first < parts.size(); ++first) {
            const RoomArchitecturePart& part = parts[first];
            partsContained &= part.solid && part.bounds.isValid()
                && room.worldBounds.contains(part.bounds.minimum)
                && room.worldBounds.contains(part.bounds.maximum);
            routesClear &= !overlapsOpen(part.bounds, mainLane)
                && !overlapsOpen(part.bounds, sideDoorLane)
                && !overlapsOpen(part.bounds, leftFlank)
                && !overlapsOpen(part.bounds, rightFlank)
                && !overlapsOpen(part.bounds, returnTerminal);
            const Vec3 midpoint = (part.bounds.minimum
                + part.bounds.maximum) * 0.5F;
            const Vec3 localMidpoint = rotateQuarterTurns(midpoint - center,
                inverseRotation);
            if (part.bounds.maximum.y <= center.y + 1.0F
                && std::abs(localMidpoint.x) > 3.0F
                && std::abs(localMidpoint.x) < 6.0F
                && std::abs(localMidpoint.z) > 2.0F
                && std::abs(localMidpoint.z) < 6.0F) {
                const bool left = localMidpoint.x < 0.0F;
                (left ? leftIslandParts : rightIslandParts)++;
                routesClear &= nearlyEqual(localMidpoint.x,
                        left ? -4.40F : 4.40F, 1.0e-4F)
                    && nearlyEqual(localMidpoint.z,
                        left ? -3.90F : 3.90F, 1.0e-4F);
            }
            for (std::size_t second = first + 1U;
                 second < parts.size(); ++second) {
                solidsSeparated &= !overlapsOpen(part.bounds,
                    parts[second].bounds);
            }
        }
        expect(partsContained && routesClear && solidsSeparated
                && leftIslandParts == 2U && rightIslandParts == 2U
                && validateRoomArchitecture(room, parts),
            "Laboratory islands keep four doors, reserved zones, central corridor, terminal and flanks clear");
        if (rotation == QuarterTurn::Degrees0) {
            unrotated = parts;
        } else {
            bool followsRotation = parts.size() == unrotated.size();
            if (followsRotation) {
                for (std::size_t index = 0U; index < parts.size(); ++index) {
                    const Vec3 originalMidpoint = (unrotated[index].bounds.minimum
                        + unrotated[index].bounds.maximum) * 0.5F;
                    const Vec3 actualMidpoint = (parts[index].bounds.minimum
                        + parts[index].bounds.maximum) * 0.5F;
                    const Vec3 expectedMidpoint = center
                        + rotateQuarterTurns(originalMidpoint - center, rotation);
                    followsRotation &= nearlyEqual(expectedMidpoint,
                        actualMidpoint, 1.0e-4F);
                }
            }
            expect(followsRotation,
                "Laboratory preview geometry follows all room rotations");
        }
        auto obstructed = parts;
        obstructed.front().bounds = toWorld({{-0.3F, 0.0F, 5.6F},
            {0.3F, 1.8F, 6.0F}});
        expect(!validateRoomArchitecture(room, obstructed),
            "Laboratory preview rejects an occupied enemy zone");
        obstructed.front().bounds = toWorld({{3.2F, 0.0F, -0.3F},
            {3.8F, 1.8F, 0.3F}});
        expect(!validateRoomArchitecture(room, obstructed),
            "Laboratory preview rejects an occupied prop zone");
        obstructed.front().bounds = toWorld({{-3.8F, 0.0F, 3.2F},
            {-3.2F, 1.8F, 3.8F}});
        expect(!validateRoomArchitecture(room, obstructed),
            "Laboratory preview rejects an occupied loot zone");
        obstructed.front().bounds = toWorld({{-0.3F, 0.0F, 9.3F},
            {0.3F, 1.8F, 9.8F}});
        expect(!validateRoomArchitecture(room, obstructed),
            "Laboratory preview rejects a blocked connector");
    }

    RoomInstance ordinary;
    ordinary.theme = RoomTheme::Laboratory;
    ordinary.worldBounds = {{-9.0F, 0.0F, -10.0F},
        {9.0F, 6.0F, 10.0F}};
    expect(!makeRoomArchitecture(ordinary).empty(),
        "Laboratory campaign variants receive a compact authored station");
    ordinary.moduleId = laboratoryPreviewModuleId;
    ordinary.worldBounds = {{-8.9F, 0.0F, -10.0F},
        {8.9F, 6.0F, 10.0F}};
    expect(makeRoomArchitecture(ordinary).empty(),
        "Laboratory preview rejects an undersized room");
}

void testStorageArchitecture()
{
    using namespace wkc::core;
    using namespace gdp::world;

    constexpr Vec3 origin{23.0F, 2.0F, -19.0F};
    constexpr std::array rotations{
        QuarterTurn::Degrees0,
        QuarterTurn::Degrees90,
        QuarterTurn::Degrees180,
        QuarterTurn::Degrees270,
    };
    const auto overlapsOpen = [](const Aabb first, const Aabb second) {
        return first.minimum.x < second.maximum.x
            && first.maximum.x > second.minimum.x
            && first.minimum.y < second.maximum.y
            && first.maximum.y > second.minimum.y
            && first.minimum.z < second.maximum.z
            && first.maximum.z > second.minimum.z;
    };
    std::size_t storageVariants = 0U;
    for (const RoomModule& module : makeDefaultRoomModuleLibrary()) {
        if (module.theme != RoomTheme::Storage) {
            continue;
        }
        ++storageVariants;
        const RoomRole role = module.supportsRole(RoomRole::Normal)
            ? RoomRole::Normal : RoomRole::Secret;
        std::vector<RoomArchitecturePart> unrotated;
        for (const QuarterTurn rotation : rotations) {
            const auto room = instantiateRoom(module, module.id, role,
                7U, origin, rotation);
            expect(room.has_value(), "Storage room instantiates for architecture test");
            if (!room) {
                continue;
            }
            const auto parts = makeRoomArchitecture(*room);
            RoomInstance differentSeed = *room;
            differentSeed.seed = 999U;
            expect(!parts.empty() && parts.size() <= 256U
                    && parts == makeRoomArchitecture(*room)
                    && parts == makeRoomArchitecture(differentSeed),
                "Storage architecture is bounded and independent of random seed");

            bool contained = true;
            bool centralLaneClear = true;
            bool doorsClear = true;
            bool spawnZonesClear = true;
            bool generatedPropsClear = true;
            bool allPartsSolid = true;
            bool rackPartsSeparated = true;
            bool shelvesTouchUprights = true;
            bool boxesRestOnShelves = true;
            std::size_t shelfBoxes = 0U;
            std::size_t texturedRackParts = 0U;
            std::size_t rackUprights = 0U;
            std::size_t rackShelves = 0U;
            std::size_t rackLips = 0U;
            std::size_t rackBrackets = 0U;
            std::size_t leftIslandPosts = 0U;
            std::size_t rightIslandPosts = 0U;
            bool islandSideAislesClear = true;
            bool islandCentralAislesClear = true;
            bool hallPropAislesClear = true;
            const bool swapped = rotation == QuarterTurn::Degrees90
                || rotation == QuarterTurn::Degrees270;
            const QuarterTurn inverseRotation = rotation == QuarterTurn::Degrees90
                ? QuarterTurn::Degrees270
                : rotation == QuarterTurn::Degrees270
                    ? QuarterTurn::Degrees90 : rotation;
            const Vec3 center{
                (room->worldBounds.minimum.x + room->worldBounds.maximum.x)
                    * 0.5F,
                room->worldBounds.minimum.y,
                (room->worldBounds.minimum.z + room->worldBounds.maximum.z)
                    * 0.5F,
            };
            if (module.id == 204U) {
                hallPropAislesClear = room->propZones.size() == 1U;
                if (hallPropAislesClear) {
                    const Aabb zone = room->propZones.front().bounds;
                    float minimumLocalX = 1000.0F;
                    float maximumLocalX = -1000.0F;
                    for (const float x : {zone.minimum.x, zone.maximum.x}) {
                        for (const float z : {zone.minimum.z, zone.maximum.z}) {
                            const Vec3 local = rotateQuarterTurns(
                                Vec3{x, center.y, z} - center,
                                inverseRotation);
                            minimumLocalX = std::min(minimumLocalX, local.x);
                            maximumLocalX = std::max(maximumLocalX, local.x);
                        }
                    }
                    // Between the island edge at X=4.00 and wall rack at
                    // X=8.07, the entire prop zone preserves one-metre aisles.
                    hallPropAislesClear = minimumLocalX >= 5.00F
                        && maximumLocalX <= 7.07F;
                }
            }
            const Aabb lane = swapped
                ? Aabb{{room->worldBounds.minimum.x,
                            center.y, center.z - 1.5F},
                      {room->worldBounds.maximum.x,
                            center.y + 2.2F, center.z + 1.5F}}
                : Aabb{{center.x - 1.5F, center.y,
                            room->worldBounds.minimum.z},
                      {center.x + 1.5F, center.y + 2.2F,
                            room->worldBounds.maximum.z}};
            for (const RoomArchitecturePart& part : parts) {
                contained &= part.bounds.isValid()
                    && room->worldBounds.contains(part.bounds.minimum)
                    && room->worldBounds.contains(part.bounds.maximum);
                allPartsSolid &= part.solid;
                if (part.storageRack) {
                    ++texturedRackParts;
                    allPartsSolid &= !part.breakable;
                    switch (part.storageRackPart) {
                    case StorageRackPart::Upright: ++rackUprights; break;
                    case StorageRackPart::Shelf: ++rackShelves; break;
                    case StorageRackPart::Lip: ++rackLips; break;
                    case StorageRackPart::Bracket: ++rackBrackets; break;
                    case StorageRackPart::None: allPartsSolid = false; break;
                    }
                }
                if (part.breakable) {
                    ++shelfBoxes;
                    allPartsSolid &= !part.storageRack
                        && part.bounds.minimum.y > center.y
                        && part.bounds.maximum.y - part.bounds.minimum.y <= 0.45F;
                    bool supported = false;
                    for (const RoomArchitecturePart& shelf : parts) {
                        if (shelf.storageRackPart != StorageRackPart::Shelf) {
                            continue;
                        }
                        supported |= nearlyEqual(part.bounds.minimum.y,
                                shelf.bounds.maximum.y, 1.0e-4F)
                            && part.bounds.minimum.x >= shelf.bounds.minimum.x
                            && part.bounds.maximum.x <= shelf.bounds.maximum.x
                            && part.bounds.minimum.z >= shelf.bounds.minimum.z
                            && part.bounds.maximum.z <= shelf.bounds.maximum.z;
                    }
                    boxesRestOnShelves &= supported;
                }
                const Vec3 midpoint = (part.bounds.minimum + part.bounds.maximum)
                    * 0.5F;
                const Vec3 local = rotateQuarterTurns(midpoint - center,
                    inverseRotation);
                const bool islandPost = nearlyEqual(
                        part.bounds.minimum.y, center.y, 1.0e-4F)
                    && part.bounds.maximum.y >= center.y + 2.5F
                    && std::abs(local.x) > 2.5F
                    && std::abs(local.x) < 4.5F
                    && std::abs(local.z) > 4.0F
                    && std::abs(local.z) < 7.2F;
                if (islandPost) {
                    (local.x < 0.0F ? leftIslandPosts : rightIslandPosts)++;
                    const float localHalfWidth = swapped
                        ? (part.bounds.maximum.z - part.bounds.minimum.z) * 0.5F
                        : (part.bounds.maximum.x - part.bounds.minimum.x) * 0.5F;
                    // The nearest grounded wall rack starts at local |X|=8.07.
                    islandSideAislesClear &= std::abs(local.x)
                        + localHalfWidth <= 7.07F;
                    islandCentralAislesClear &= std::abs(local.x)
                        - localHalfWidth >= 2.50F;
                }
                if (!part.solid) {
                    continue;
                }
                centralLaneClear &= !overlapsOpen(part.bounds, lane);
                const auto clearOfZones = [&](const std::vector<SpawnZone>& zones,
                                              const float margin) {
                    for (const SpawnZone& zone : zones) {
                        const Aabb occupied{
                            {zone.bounds.minimum.x - margin, center.y,
                                zone.bounds.minimum.z - margin},
                            {zone.bounds.maximum.x + margin, center.y + 2.2F,
                                zone.bounds.maximum.z + margin},
                        };
                        if (overlapsOpen(part.bounds, occupied)) {
                            return false;
                        }
                    }
                    return true;
                };
                spawnZonesClear &= clearOfZones(room->enemyZones, 0.35F)
                    && clearOfZones(room->propZones, 0.15F)
                    && clearOfZones(room->lootZones, 0.10F);
                for (const ConnectorInstance& connector : room->connectors) {
                    const Vec3 p = connector.worldPosition;
                    const Vec3 n = connector.worldNormal;
                    const Aabb opening = std::abs(n.x) > 0.5F
                        ? Aabb{{p.x - (n.x > 0.0F ? 0.95F : 0.0F),
                                    p.y, p.z - 0.9F},
                              {p.x + (n.x < 0.0F ? 0.95F : 0.0F),
                                    p.y + 2.2F, p.z + 0.9F}}
                        : Aabb{{p.x - 0.9F, p.y,
                                    p.z - (n.z > 0.0F ? 0.95F : 0.0F)},
                              {p.x + 0.9F, p.y + 2.2F,
                                    p.z + (n.z < 0.0F ? 0.95F : 0.0F)}};
                    doorsClear &= !overlapsOpen(part.bounds, opening);
                }
            }
            for (std::size_t first = 0U; first < parts.size(); ++first) {
                if (!parts[first].storageRack) continue;
                for (std::size_t second = first + 1U;
                     second < parts.size(); ++second) {
                    if (parts[second].storageRack) {
                        rackPartsSeparated &= !overlapsOpen(
                            parts[first].bounds, parts[second].bounds);
                    }
                }
                if (parts[first].storageRackPart != StorageRackPart::Shelf) {
                    continue;
                }
                const Aabb shelf = parts[first].bounds;
                std::size_t uprightContacts = 0U;
                for (const RoomArchitecturePart& candidate : parts) {
                    if (candidate.storageRackPart != StorageRackPart::Upright
                        || candidate.bounds.minimum.y != center.y
                        || candidate.bounds.maximum.y - center.y < 2.5F) {
                        continue;
                    }
                    const Aabb upright = candidate.bounds;
                    const bool xOverlaps = shelf.minimum.x < upright.maximum.x
                        && shelf.maximum.x > upright.minimum.x;
                    const bool zOverlaps = shelf.minimum.z < upright.maximum.z
                        && shelf.maximum.z > upright.minimum.z;
                    const bool touchesZ = nearlyEqual(shelf.minimum.z,
                            upright.maximum.z, 1.0e-4F)
                        || nearlyEqual(shelf.maximum.z,
                            upright.minimum.z, 1.0e-4F);
                    const bool touchesX = nearlyEqual(shelf.minimum.x,
                            upright.maximum.x, 1.0e-4F)
                        || nearlyEqual(shelf.maximum.x,
                            upright.minimum.x, 1.0e-4F);
                    uprightContacts += static_cast<std::size_t>(
                        (xOverlaps && touchesZ) || (zOverlaps && touchesX));
                }
                shelvesTouchUprights &= uprightContacts == 4U;
            }
            for (wkc::core::Seed seed = 1U; seed <= 16U; ++seed) {
                const PropLayout layout = generatePropLayout(
                    seed, role, room->theme, 3U, room->propZones);
                generatedPropsClear &= validatePropLayout(
                    layout, room->propZones).isValid();
                for (const PropSpawn& prop : layout.props) {
                    const Aabb occupied{
                        {prop.position.x - prop.scale.x * 0.5F, center.y,
                            prop.position.z - prop.scale.z * 0.5F},
                        {prop.position.x + prop.scale.x * 0.5F,
                            center.y + prop.scale.y,
                            prop.position.z + prop.scale.z * 0.5F},
                    };
                    for (const RoomArchitecturePart& part : parts) {
                        generatedPropsClear &= !overlapsOpen(part.bounds, occupied);
                    }
                }
            }
            expect(contained && centralLaneClear && doorsClear
                    && spawnZonesClear && generatedPropsClear && allPartsSolid
                    && rackPartsSeparated && shelvesTouchUprights
                    && boxesRestOnShelves
                    && rackUprights * 3U == rackShelves * 4U
                    && rackShelves == shelfBoxes
                    && rackLips >= rackShelves
                    && rackBrackets * 3U == rackShelves * 2U
                    && shelfBoxes >= 12U
                    && texturedRackParts >= 20U
                    && validateRoomArchitecture(*room, parts),
                "Storage solids leave doors, enemy and loot zones, and generated props clear");
            if (module.id == 204U) {
                expect(leftIslandPosts == 4U && rightIslandPosts == 4U
                        && islandSideAislesClear && islandCentralAislesClear
                        && hallPropAislesClear,
                    "Storage Hall has two interior islands and metre-wide aisles around props in every rotation");
            } else {
                expect(leftIslandPosts == 0U && rightIslandPosts == 0U,
                    "compact Storage variants keep the interior free of shelf islands");
            }

            if (rotation == QuarterTurn::Degrees0) {
                auto blocked = parts;
                blocked.front().bounds = {
                    {center.x - 0.2F, center.y, center.z - 0.2F},
                    {center.x + 0.2F, center.y + 1.8F, center.z + 0.2F}};
                expect(!validateRoomArchitecture(*room, blocked),
                    "Storage placement validation rejects a solid inside an enemy zone");
                if (module.id == 204U) {
                    blocked.front().bounds = {
                        {center.x + 5.5F, center.y, center.z - 6.5F},
                        {center.x + 6.0F, center.y + 1.8F,
                            center.z - 6.0F}};
                    expect(!validateRoomArchitecture(*room, blocked),
                        "Storage placement validation rejects a solid in the Hall prop zone");
                }
                if (!room->connectors.empty()) {
                    blocked.front().bounds = {
                        {room->connectors.front().worldPosition.x - 0.2F,
                            center.y,
                            room->connectors.front().worldPosition.z - 0.2F},
                        {room->connectors.front().worldPosition.x + 0.2F,
                            center.y + 1.8F,
                            room->connectors.front().worldPosition.z + 0.2F}};
                    expect(!validateRoomArchitecture(*room, blocked),
                        "Storage placement validation rejects a solid in a doorway");
                }
            }

            if (rotation == QuarterTurn::Degrees0) {
                unrotated = parts;
            } else if (parts.size() == unrotated.size()) {
                bool rotatedCorrectly = true;
                for (std::size_t index = 0U; index < parts.size(); ++index) {
                    const auto midpoint = [](const Aabb bounds) {
                        return (bounds.minimum + bounds.maximum) * 0.5F;
                    };
                    const Vec3 expected = origin
                        + rotateQuarterTurns(midpoint(unrotated[index].bounds)
                                - origin, rotation);
                    const Vec3 actual = midpoint(parts[index].bounds);
                    rotatedCorrectly &= nearlyEqual(expected, actual, 1.0e-4F)
                        && parts[index].solid == unrotated[index].solid;
                }
                expect(rotatedCorrectly,
                    "Storage architecture follows room rotation in world space");
            } else {
                expect(false, "Storage architecture keeps part count under rotation");
            }
        }
    }
    expect(storageVariants == 4U,
        "all compact, wide, secret and hall Storage modules receive architecture");

    RoomInstance preview;
    preview.theme = RoomTheme::Storage;
    preview.sizeClass = RoomSizeClass::Large;
    preview.worldBounds = {{-9.0F, 0.0F, -10.0F}, {9.0F, 6.0F, 10.0F}};
    const auto previewParts = makeRoomArchitecture(preview);
    const auto previewIslandPosts = std::count_if(
        previewParts.begin(), previewParts.end(),
        [](const RoomArchitecturePart& part) {
            const Vec3 midpoint = (part.bounds.minimum + part.bounds.maximum)
                * 0.5F;
            return part.solid && part.bounds.minimum.y == 0.0F
                && part.bounds.maximum.y >= 2.5F
                && std::abs(midpoint.x) > 2.5F
                && std::abs(midpoint.x) < 4.5F
                && std::abs(midpoint.z) > 4.0F
                && std::abs(midpoint.z) < 7.2F;
        });
    expect(previewIslandPosts == 8 && validateRoomArchitecture(preview, previewParts),
        "18-by-20 Storage preview footprint receives the same two islands");

    RoomInstance industrial;
    industrial.theme = RoomTheme::Industrial;
    industrial.worldBounds = {{-4.0F, 0.0F, -5.0F}, {4.0F, 4.0F, 5.0F}};
    const auto industrialParts = makeRoomArchitecture(industrial);
    expect(!industrialParts.empty()
            && validateRoomArchitecture(industrial, industrialParts)
            && std::any_of(industrialParts.begin(), industrialParts.end(),
                [](const RoomArchitecturePart& part) {
                    return part.decoration == RoomDecorationPart::CeilingDuct;
                }),
        "Industrial rooms use authored service details instead of generic blocks");

    const auto library = makeDefaultRoomModuleLibrary();
    bool industrialConstructionValid = true;
    for (const RoomModuleId id : {101U, 102U, 103U, 105U}) {
        const auto module = std::find_if(library.begin(), library.end(),
            [id](const RoomModule& candidate) { return candidate.id == id; });
        industrialConstructionValid &= module != library.end();
        if (module == library.end()) continue;
        for (const QuarterTurn rotation : rotations) {
            const RoomRole role = id == 103U
                ? RoomRole::Start : RoomRole::Normal;
            const auto room = instantiateRoom(*module, id, role, 1793U,
                origin, rotation);
            if (!room) {
                industrialConstructionValid = false;
                continue;
            }
            const auto parts = makeRoomArchitecture(*room);
            industrialConstructionValid &= validateRoomArchitecture(*room, parts)
                && parts.size() >= (id == 103U ? 60U : 35U);
        }
    }
    expect(industrialConstructionValid,
        "Industrial construction and safe arrival cabinets clear every rotation");
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
    const bool hubSecret = graph.nodes[graph.mainPath.size()].role == RoomRole::Secret;
    expect(graph.edges.size() == (hubSecret ? 7U : 8U),
        "graph omits the loop that would wrap around the large Hub");
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
    expect(hubSecret || (firstIsBranch != secondIsBranch && branchParent != 0U
            && (mainEndpoint + 1U == branchParent
                || branchParent + 1U == mainEndpoint)),
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
    // The wider Hub/Arena transfer and connector/corridor dimensions now
    // belong to the deterministic world identity.
    expect(fingerprint == 0x8B4BEE9554BBA15EULL,
        "golden world fingerprint remains stable across builds");
    expect(fingerprint == worldFingerprint(*repeat.world),
        "same seed produces the same complete world fingerprint");
    expect(fingerprint != worldFingerprint(*different.world),
        "different seed changes the world fingerprint");
    expect(validateWorld(*first.world).isValid(), "committed world passes every structural gate");

    World changedGauge = *first.world;
    changedGauge.corridors.front().width += 0.1F;
    expect(worldFingerprint(changedGauge) != fingerprint
            && !validateWorld(changedGauge).isValid(),
        "corridor width changes fingerprint and must match both door connectors");
    World changedConnector = *first.world;
    const EdgeId changedEdge = changedConnector.corridors.front().edge;
    for (ConnectorInstance& connector : changedConnector.rooms.front().connectors) {
        if (connector.edge == changedEdge) {
            connector.height += 0.1F;
            break;
        }
    }
    expect(worldFingerprint(changedConnector) != fingerprint
            && !validateWorld(changedConnector).isValid(),
        "door height changes fingerprint and fails corridor dimension validation");

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

void testCampaignCorridorGauges()
{
    using namespace gdp::world;
    std::size_t mixedSeeds = 0U;
    std::size_t fallbackSeeds = 0U;
    bool validMix = true;
    bool dimensionsMatch = true;
    bool wideArchitectureClear = true;
    for (wkc::core::Seed seed = 0U; seed < 128U; ++seed) {
        const auto result = generateWorld(seed);
        if (!result.world) {
            validMix = false;
            break;
        }
        std::size_t wideCount = 0U;
        std::size_t narrowCount = 0U;
        for (const CorridorPath& corridor : result.world->corridors) {
            const bool wide = std::abs(corridor.width - 2.4F) < 0.01F
                && std::abs(corridor.height - 2.8F) < 0.01F;
            const bool narrow = std::abs(corridor.width - 1.2F) < 0.01F
                && std::abs(corridor.height - 2.2F) < 0.01F;
            wideCount += wide ? 1U : 0U;
            narrowCount += narrow ? 1U : 0U;
            if (!wide && !narrow) dimensionsMatch = false;
            for (const RoomInstance& room : result.world->rooms) {
                for (const ConnectorInstance& connector : room.connectors) {
                    if (connector.edge != corridor.edge) continue;
                    dimensionsMatch &= std::abs(connector.width - corridor.width) < 0.01F
                        && std::abs(connector.height - corridor.height) < 0.01F;
                    if (wide && usesAuthoredRoomArchitecture(room)) {
                        const auto parts = makeRoomArchitecture(room);
                        wideArchitectureClear &= validateRoomArchitecture(room, parts);
                    }
                }
            }
        }
        mixedSeeds += wideCount == 1U && narrowCount >= 1U ? 1U : 0U;
        fallbackSeeds += wideCount == 0U && narrowCount >= 1U ? 1U : 0U;
        validMix &= wideCount <= 1U && narrowCount >= 1U;
    }
    expect(validMix && mixedSeeds >= 120U
            && mixedSeeds + fallbackSeeds == 128U,
        "campaign seeds normally mix one wide transfer and narrow service corridors, with safe fallback");
    expect(dimensionsMatch,
        "generated corridor gauges match both linked door openings");
    expect(wideArchitectureClear,
        "wide door clearance remains free of authored room solids");
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

void testCorridorWallColumns()
{
    using namespace wkc::core;
    using namespace gdp::world;

    constexpr std::array straight{
        Vec3{0.0F, 0.0F, 0.0F}, Vec3{0.0F, 0.0F, 10.3F}};
    const auto wide = makeCorridorWallColumns(straight, 2.4F, 2.8F);
    expect(wide.size() == 6U && wide == makeCorridorWallColumns(straight,
            2.4F, 2.8F),
        "long preview corridor receives three deterministic pairs of columns");
    bool clear = true;
    for (const Aabb& column : wide) {
        const float closestEnd = std::min(column.minimum.z,
            straight.back().z - column.maximum.z);
        const bool leftWall = nearlyEqual(column.minimum.x, -1.2F)
            && nearlyEqual(column.maximum.x, -1.02F);
        const bool rightWall = nearlyEqual(column.minimum.x, 1.02F)
            && nearlyEqual(column.maximum.x, 1.2F);
        clear &= closestEnd >= 0.6F
            && nearlyEqual(column.minimum.y, 0.0F)
            && nearlyEqual(column.maximum.y, 2.8F)
            && nearlyEqual(column.maximum.z - column.minimum.z, 0.38F)
            && (leftWall || rightWall);
    }
    expect(clear,
        "wide corridor columns touch its walls and clear both door thresholds");

    const auto narrow = makeCorridorWallColumns(straight, 1.2F, 2.2F);
    bool robotClearance = narrow.size() == wide.size();
    for (const Aabb& column : narrow) {
        robotClearance &= column.maximum.x <= -0.54F
                || column.minimum.x >= 0.54F;
    }
    expect(robotClearance,
        "standard 1.2 m corridors retain clearance for the 0.52 m robot radius");

    constexpr std::array reversedStraight{
        Vec3{0.0F, 0.0F, 10.3F}, Vec3{0.0F, 0.0F, 0.0F}};
    expect(wide == makeCorridorWallColumns(reversedStraight, 2.4F, 2.8F),
        "reversing a straight route preserves all column bounds and order");

    constexpr std::array bend{
        Vec3{0.0F, 0.0F, 0.0F}, Vec3{0.0F, 0.0F, 7.0F},
        Vec3{7.0F, 0.0F, 7.0F}};
    constexpr std::array reversedBend{
        Vec3{7.0F, 0.0F, 7.0F}, Vec3{0.0F, 0.0F, 7.0F},
        Vec3{0.0F, 0.0F, 0.0F}};
    const auto bendColumns = makeCorridorWallColumns(bend, 2.4F, 2.8F);
    expect(bendColumns.size() == 8U
            && bendColumns == makeCorridorWallColumns(
                reversedBend, 2.4F, 2.8F),
        "L route has symmetric wall pairs on long arms, independent of direction");
    bool cornerClear = true;
    for (const Aabb& column : bendColumns) {
        const float distanceX = std::max({column.minimum.x,
            0.0F, -column.maximum.x});
        const float distanceZ = std::max({column.minimum.z - 7.0F,
            0.0F, 7.0F - column.maximum.z});
        cornerClear &= std::max(distanceX, distanceZ) >= 0.6F;
    }
    expect(cornerClear, "L route bend keeps an unobstructed turning area");

    constexpr std::array zigzag{
        Vec3{0.0F, 0.0F, 0.0F}, Vec3{0.0F, 0.0F, 6.0F},
        Vec3{7.0F, 0.0F, 6.0F}, Vec3{7.0F, 0.0F, 12.0F}};
    const auto zigzagColumns = makeCorridorWallColumns(zigzag, 1.2F, 2.2F);
    expect(zigzagColumns.size() == 12U,
        "Z route places wall pairs only on sufficiently long straight arms");
    constexpr std::array shortPath{
        Vec3{0.0F, 0.0F, 0.0F}, Vec3{0.0F, 0.0F, 3.0F}};
    expect(makeCorridorWallColumns(shortPath, 2.4F, 2.8F).empty()
            && makeCorridorWallColumns(straight, 1.0F, 2.2F).empty(),
        "short segments and corridors too narrow for the robot stay clear");
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
    const Entity* terminalEntity = manager.find(initial.terminal);
    expect(terminal != nullptr && terminalEntity != nullptr
            && terminal->type == PropType::Terminal && terminal->solid
            && wkc::core::nearlyEqual(terminalEntity->transform.scale,
                wkc::core::Vec3{1.15F, 1.80F, 0.60F}),
        "dedicated Terminal room receives the portal terminal");
    expect(!initial.exitTerminal.isValid(), "non-Campaign population has no exit terminal");

    bool arenaCompositionValid = true;
    std::size_t arenaEnemyCount = 0U;
    bool bossRobotValid = false;
    bool startEnemyFree = true;
    bool startTerminalFree = true;
    std::size_t terminalEnemyCount = 0U;
    bool terminalOwnsConsole = false;
    for (const RoomInstance& room : generated.world->rooms) {
        for (const EntityId id : room.entities) {
            const EnemyData* enemy = manager.findData<EnemyData>(id);
            const Entity* entity = manager.find(id);
            const PropData* prop = manager.findData<PropData>(id);
            if (room.role == RoomRole::Start) {
                startEnemyFree &= enemy == nullptr;
                startTerminalFree &= id != initial.terminal;
            }
            if (room.role == RoomRole::Terminal) {
                terminalEnemyCount += enemy != nullptr ? 1U : 0U;
                terminalOwnsConsole |= id == initial.terminal
                    && entity != nullptr && prop != nullptr
                    && prop->type == PropType::Terminal;
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
                && enemy->health >= 360.0F * 1.90F
                && enemy->health <= 420.0F * 1.90F
                && enemy->health == enemy->maximumHealth
                && enemy->visualSeed != 0U);
        }
    }
    expect(arenaCompositionValid && arenaEnemyCount >= 2U && arenaEnemyCount <= 4U,
        "Arena population uses two to four healthy seeded enemies");
    expect(bossRobotValid,
        "Boss population includes one healthy IndustrialRobot with an isolated visual seed");
    expect(startEnemyFree && startTerminalFree,
        "START has no initial enemies or mission terminal");
    expect(terminalOwnsConsole && terminalEnemyCount == 3U,
        "dedicated Terminal room owns its console and three initial enemies");

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
        const std::size_t enemyCount = static_cast<std::size_t>(std::count_if(
                room.entities.begin(), room.entities.end(),
                [&manager](const EntityId id) {
                    const Entity* entity = manager.find(id);
                    return entity != nullptr && entity->kind == EntityKind::Enemy;
                }));
        const std::size_t baseCount = room.role == RoomRole::Start ? 1U
            : room.role == RoomRole::Terminal ? enemyCount + 1U
            : room.role == RoomRole::Boss ? enemyCount + 3U : enemyCount;
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
            if (room.role == RoomRole::Boss
                && prop->type == PropType::Barricade
                && (prop->maximumHealth == 120.0F
                    || prop->maximumHealth == 180.0F)) {
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
            1.1F);
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
                    - (baselineEnemy->maximumHealth * 1.1F)) < 1.0e-4F;
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

    World missingTerminal = *generated.world;
    for (RoomInstance& room : missingTerminal.rooms) {
        if (room.role == RoomRole::Terminal) {
            room.role = RoomRole::Normal;
        }
    }
    EntityManager missingTerminalManager;
    expect(!populateInitialEntities(missingTerminal, missingTerminalManager, {}).isValid()
            && missingTerminalManager.activeCount() == 0,
        "population rejects a world without Terminal before creating partial content");

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
    bool sawWideNormal = false;
    bool sawCompactNormal = false;
    bool everyWorldHasWideNormal = true;
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
    bool sawArmorLoot = false;
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
        bool worldHasWideNormal = false;
        for (const RoomInstance& room : generated.world->rooms) {
            sawCompact = sawCompact || room.sizeClass == RoomSizeClass::Compact;
            sawMedium = sawMedium || room.sizeClass == RoomSizeClass::Medium;
            sawLarge = sawLarge || room.sizeClass == RoomSizeClass::Large;
            sawWideNormal = sawWideNormal || (room.role == RoomRole::Normal
                && room.sizeClass == RoomSizeClass::Large);
            worldHasWideNormal = worldHasWideNormal || (room.role == RoomRole::Normal
                && room.sizeClass == RoomSizeClass::Large
                && (room.worldBounds.maximum.y - room.worldBounds.minimum.y == 6.0F
                    || (room.moduleId == maintenanceLModuleId
                        && room.worldBounds.maximum.y - room.worldBounds.minimum.y == 5.5F)));
            sawCompactNormal = sawCompactNormal || (room.role == RoomRole::Normal
                && room.sizeClass == RoomSizeClass::Compact);
            if ((room.role == RoomRole::Hub || room.role == RoomRole::Arena
                    || room.role == RoomRole::Boss
                    || room.role == RoomRole::Terminal)
                && room.sizeClass != RoomSizeClass::Large) {
                roleSizesValid = false;
            }
            if (room.role == RoomRole::Secret
                && room.sizeClass != RoomSizeClass::Compact) {
                roleSizesValid = false;
            }
        }
        everyWorldHasWideNormal &= worldHasWideNormal;
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
                    sawArmorLoot |= item != nullptr
                        && item->type == gdp::entities::ItemType::Armor;
                    secretWeapons += room.role == RoomRole::Secret && item != nullptr
                        && item->type == gdp::entities::ItemType::Weapon ? 1U : 0U;
                    if (room.role == RoomRole::Boss && enemy != nullptr
                        && enemy->archetype == gdp::entities::EnemyArchetype::IndustrialRobot) {
                        const float stepped = (enemy->maximumHealth / 1.90F - 360.0F) / 30.0F;
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
                } else if (room.role == RoomRole::Terminal) {
                    encountersValid = encountersValid && enemies == 3U;
                } else if (room.role == RoomRole::Boss) {
                    encountersValid = encountersValid && enemies == 3U;
                }
            }
            encountersValid = encountersValid && secretWeapons == 1U;
        }
    }
    expect(allValid, "1,000 deterministic seeds produce valid committed worlds");
    expect(maximumAcceptedAttempt < 32U, "batch stays inside configured attempt budget");
    expect(sawCompact && sawMedium && sawLarge && sawWideNormal
            && sawCompactNormal && roleSizesValid,
        "seed batch exercises all size families and preserves role-specific sizing");
    expect(everyWorldHasWideNormal,
        "each of 1,000 generated worlds contains a wide Normal room");
    expect(maximumCorridorLength <= 17.0F,
        "local loop policy prevents map-spanning corridors");
    expect(secondBranchAwayFromHub && secretAtHub > 0U && secretFarther > 0U,
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
    expect(sawArmorLoot,
        "seed batch still offers procedural Armor pickups");
}

void testCampaignExitTerminalPopulation()
{
    using namespace gdp::world;
    using namespace gdp::entities;
    using namespace wkc::core;
    std::array<bool, 4> rotations{};
    for (Seed seed = 0U; seed < 64U; ++seed) {
        auto generated = generateWorld(seed);
        expect(generated.succeeded(), "Campaign exit seed generates");
        if (!generated.world) continue;
        EntityManager manager;
        const auto initial = populateInitialEntities(*generated.world, manager,
            {.transform = {.position = {0.0F, 0.0F, 4.0F}}}, 1.0F, true);
        const auto* terminal = manager.find(initial.exitTerminal);
        const auto* prop = manager.findData<PropData>(initial.exitTerminal);
        expect(initial.isValid() && terminal && prop,
            "every Campaign seed creates an exit interaction entity");
        if (!terminal || !prop) continue;
        const auto boss = std::find_if(generated.world->rooms.begin(), generated.world->rooms.end(),
            [](const auto& room) { return room.role == RoomRole::Boss; });
        expect(boss != generated.world->rooms.end(), "Campaign exit has a Boss room owner");
        if (boss == generated.world->rooms.end()) continue;
        rotations[static_cast<std::size_t>(boss->rotation)] = true;
        const Vec3 position = boss->transform.position
            + rotateQuarterTurns({0.0F, 0.0F, 2.2F}, boss->rotation);
        const bool sideways = boss->rotation == QuarterTurn::Degrees90
            || boss->rotation == QuarterTurn::Degrees270;
        expect(terminal->roomId == boss->id && prop->type == PropType::Terminal
            && prop->solid && prop->maximumHealth == 0.0F
            && nearlyEqual(terminal->transform.position, position)
            && nearlyEqual(terminal->transform.scale,
                sideways ? Vec3{0.60F, 1.80F, 1.15F}
                    : Vec3{1.15F, 1.80F, 0.60F})
            && nearlyEqual(terminal->transform.rotationDegrees.y,
                90.0F * static_cast<float>(boss->rotation))
            && std::count(boss->entities.begin(), boss->entities.end(), initial.exitTerminal) == 1,
            "protected exit portal follows the owning rotated Boss room");
    }
    expect(std::all_of(rotations.begin(), rotations.end(), [](bool seen) { return seen; }),
        "Campaign exit seed batch exercises all four console rotations");
}

void testCampaignEncounterProgression()
{
    using namespace gdp::entities;
    using namespace gdp::world;
    using wkc::core::Vec3;
    using EnemySnapshot = std::tuple<RoomRole, EnemyArchetype, Vec3, float>;

    bool sawFloorVariation = false;
    bool validPositions = true;
    bool validCounts = true;
    bool scaledBoss = true;
    bool deterministic = true;
    for (wkc::core::Seed seed = 1U; seed <= 128U; ++seed) {
        std::vector<EnemySnapshot> firstFloor;
        std::vector<EnemySnapshot> lastFloor;
        float firstBossHealth = 0.0F;
        for (const std::uint32_t floor : {1U, 5U, 9U, 9U}) {
            auto generated = generateWorld(seed);
            if (!generated.world) {
                validCounts = false;
                continue;
            }
            EntityManager manager;
            const auto initial = populateInitialEntities(*generated.world, manager,
                {.transform = {.position = {0.0F, 0.0F, 4.0F}}, .health = 100.0F},
                1.0F + 0.08F * static_cast<float>(floor - 1U), true, floor);
            validCounts &= initial.isValid();
            std::vector<EnemySnapshot> snapshot;
            for (const RoomInstance& room : generated.world->rooms) {
                std::vector<Vec3> enemyPositions;
                std::size_t count = 0U;
                std::size_t workers = 0U;
                for (const EntityId id : room.entities) {
                    const auto* enemy = manager.findData<EnemyData>(id);
                    const auto* entity = manager.find(id);
                    if (!enemy || !entity) continue;
                    ++count;
                    workers += enemy->archetype == EnemyArchetype::Worker ? 1U : 0U;
                    const Vec3 position = entity->transform.position;
                    snapshot.emplace_back(room.role, enemy->archetype, position, enemy->health);
                    if (floor > 1U && (room.role == RoomRole::Normal
                            || room.role == RoomRole::Arena
                            || room.role == RoomRole::Terminal
                            || room.role == RoomRole::Boss)) {
                        constexpr float radius = 0.52F;
                        validPositions &= position.x - radius >= room.worldBounds.minimum.x
                            && position.x + radius <= room.worldBounds.maximum.x
                            && position.z - radius >= room.worldBounds.minimum.z
                            && position.z + radius <= room.worldBounds.maximum.z;
                        for (const Vec3 other : enemyPositions) {
                            const Vec3 delta = position - other;
                            validPositions &= delta.x * delta.x + delta.z * delta.z
                                >= 1.04F * 1.04F;
                        }
                        for (const EntityId propId : room.entities) {
                            const auto* prop = manager.findData<PropData>(propId);
                            const auto* obstacle = manager.find(propId);
                            if (!prop || !prop->solid || !obstacle) continue;
                            const float halfX = obstacle->transform.scale.x * 0.5F;
                            const float halfZ = obstacle->transform.scale.z * 0.5F;
                            const float closestX = std::clamp(position.x,
                                obstacle->transform.position.x - halfX,
                                obstacle->transform.position.x + halfX);
                            const float closestZ = std::clamp(position.z,
                                obstacle->transform.position.z - halfZ,
                                obstacle->transform.position.z + halfZ);
                            const float dx = position.x - closestX;
                            const float dz = position.z - closestZ;
                            validPositions &= dx * dx + dz * dz >= radius * radius;
                        }
                    }
                    enemyPositions.push_back(position);
                    if (room.role == RoomRole::Boss
                        && enemy->archetype == EnemyArchetype::IndustrialRobot) {
                        if (floor == 1U) firstBossHealth = enemy->health;
                        else {
                            const float scale = floor >= 8U ? 1.90F
                                : floor >= 5U ? 1.82F : 1.72F;
                            const float expected = firstBossHealth * (scale / 1.65F)
                                * (1.0F + 0.08F * static_cast<float>(floor - 1U));
                            scaledBoss &= std::abs(enemy->health - expected) < 1.0e-3F;
                        }
                    }
                }
                if (room.role == RoomRole::Normal) {
                    validCounts &= count >= 1U && count <= 2U
                        && (floor < 8U || count == 2U);
                } else if (room.role == RoomRole::Arena) {
                    validCounts &= count >= (floor >= 8U ? 4U : floor >= 5U ? 3U : 2U)
                        && count <= 4U && (floor < 8U || workers == 0U);
                } else if (room.role == RoomRole::Boss) {
                    validCounts &= count == (floor >= 8U ? 4U : 3U);
                }
            }
            if (floor == 1U) firstFloor = snapshot;
            if (floor == 9U && lastFloor.empty()) lastFloor = snapshot;
            else if (floor == 9U) deterministic &= lastFloor == snapshot;
        }
        sawFloorVariation |= firstFloor != lastFloor;
    }
    expect(sawFloorVariation && validCounts,
        "Campaign floors vary bounded enemy formations and late-floor pressure");
    expect(validPositions, "Campaign encounter spawns clear walls, props and other enemies");
    expect(scaledBoss, "Campaign boss HP follows the floor bracket and difficulty scale");
    expect(deterministic, "Campaign enemy layout repeats for the same seed and floor");
}

void testBossTerminalLightingAcrossRotations()
{
    using namespace gdp::world;
    using namespace wkc::core;

    const auto library = makeDefaultRoomModuleLibrary();
    const auto bossModule = std::find_if(library.begin(), library.end(),
        [](const RoomModule& module) {
            return module.supportsRole(RoomRole::Boss);
        });
    expect(bossModule != library.end(), "Boss lighting has an arena module");
    if (bossModule == library.end()) return;

    bool valid = true;
    for (const QuarterTurn rotation : {QuarterTurn::Degrees0,
             QuarterTurn::Degrees90, QuarterTurn::Degrees180,
             QuarterTurn::Degrees270}) {
        const auto room = instantiateRoom(*bossModule, 1U, RoomRole::Boss,
            20261120U, {37.0F, 0.0F, -19.0F}, rotation);
        if (!room) {
            valid = false;
            continue;
        }
        const RoomLightingPlan plan = generateRoomLightingPlan(
            room->seed, room->role, room->theme, room->worldBounds);
        if (plan.lights.size() != maxRoomLights
            || !validateRoomLightingPlan(plan, room->worldBounds).isValid()) {
            valid = false;
            continue;
        }
        const Vec3 center = (room->worldBounds.minimum
            + room->worldBounds.maximum) * 0.5F;
        const RoomPointLight& focal = plan.lights.back();
        const Vec3 terminal = room->transform.position
            + rotateQuarterTurns({0.0F, 0.0F, 2.2F}, rotation);
        const Vec3 offset = focal.position - terminal;
        valid &= nearlyEqual(focal.position.x, center.x)
            && nearlyEqual(focal.position.z, center.z)
            && offset.x * offset.x + offset.z * offset.z < 3.0F * 3.0F;
        for (const RoomArchitecturePart& part : makeRoomArchitecture(*room)) {
            if (part.combatPart != CombatArchitecturePart::CeilingBeam
                && part.combatPart != CombatArchitecturePart::CeilingGrille) {
                continue;
            }
            const bool overlapsFixture = part.bounds.minimum.x < center.x + 1.30F
                && part.bounds.maximum.x > center.x - 1.30F
                && part.bounds.minimum.z < center.z + 0.20F
                && part.bounds.maximum.z > center.z - 0.20F;
            valid &= !overlapsFixture;
        }
    }
    expect(valid,
        "Boss focal fixture reaches the terminal and clears ceiling structures in all rotations");
}

void testCompactFurnitureClearanceAcrossRotations()
{
    using namespace gdp::world;
    using namespace wkc::core;
    const auto library = makeDefaultRoomModuleLibrary();
    bool allClear = true;
    for (const RoomModuleId moduleId : {301U, 302U, 303U,
             401U, 402U, 501U, 502U, 505U,
             601U, 602U, 603U, 701U, 702U, 703U}) {
        const auto module = std::find_if(library.begin(), library.end(),
            [moduleId](const RoomModule& candidate) {
                return candidate.id == moduleId;
            });
        if (module == library.end()) {
            allClear = false;
            continue;
        }
        const RoomRole role = module->supportsRole(RoomRole::Normal)
            ? RoomRole::Normal : RoomRole::Arena;
        for (const QuarterTurn rotation : {QuarterTurn::Degrees0,
                 QuarterTurn::Degrees90, QuarterTurn::Degrees180,
                 QuarterTurn::Degrees270}) {
            const auto room = instantiateRoom(*module, 1U, role,
                20261120U, {37.0F, 0.0F, -19.0F}, rotation);
            if (!room) {
                allClear = false;
                continue;
            }
            const auto parts = makeRoomArchitecture(*room);
            const bool clear = !parts.empty()
                && validateRoomArchitecture(*room, parts);
            if (!clear) {
                std::cerr << "Furniture module " << moduleId
                    << " rotation " << static_cast<int>(rotation)
                    << " parts " << parts.size() << " failed clearance\n";
            }
            allClear &= clear;
        }
    }
    expect(allClear,
        "campaign furniture variants clear connectors, loot and enemy zones in four rotations");
}

void testOfficeHallArchiveDesks()
{
    using namespace gdp::world;
    using namespace wkc::core;
    const auto library = makeDefaultRoomModuleLibrary();
    bool hallValid = true;
    for (const RoomModuleId id : {301U, 302U, 303U}) {
        const auto module = std::find_if(library.begin(), library.end(),
            [id](const RoomModule& candidate) { return candidate.id == id; });
        if (module == library.end()) {
            hallValid = false;
            continue;
        }
        for (const QuarterTurn rotation : {QuarterTurn::Degrees0,
                 QuarterTurn::Degrees90, QuarterTurn::Degrees180,
                 QuarterTurn::Degrees270}) {
            const auto room = instantiateRoom(*module, id, RoomRole::Normal,
                1116U, {-15.85F, 0.0F, 10.30F}, rotation);
            const auto otherSeed = instantiateRoom(*module, id,
                RoomRole::Normal, 1793U, {-15.85F, 0.0F, 10.30F}, rotation);
            if (!room || !otherSeed) {
                hallValid = false;
                continue;
            }
            const auto parts = makeRoomArchitecture(*room);
            const auto otherParts = makeRoomArchitecture(*otherSeed);
            hallValid &= validateRoomArchitecture(*room, parts)
                && parts == otherParts;
            if (id == 303U) {
                // The broad room has two 2.1 m archive desktops. Both are
                // outside the central spawn and the reserved prop pocket.
                const auto desktopCount = std::count_if(parts.begin(),
                    parts.end(), [](const RoomArchitecturePart& part) {
                        const auto extent = part.bounds.maximum
                            - part.bounds.minimum;
                        return (nearlyEqual(extent.x, 2.10F, 0.001F)
                                   || nearlyEqual(extent.z, 2.10F, 0.001F))
                            && nearlyEqual(extent.y, 0.12F, 0.001F);
                    });
                hallValid &= parts.size() >= 30U && desktopCount == 2;
                if (!validateRoomArchitecture(*room, parts)
                    || desktopCount != 2 || parts.size() < 30U) {
                    std::cerr << "Office hall rotation "
                        << static_cast<int>(rotation) << " parts "
                        << parts.size() << " desktops " << desktopCount
                        << " clearance " << validateRoomArchitecture(*room, parts)
                        << '\n';
                }
            } else {
                // Compact offices may carry additional mechanical trim and
                // overhead service geometry around the original workstation.
                hallValid &= parts.size() >= 6U;
            }
        }
    }
    expect(hallValid,
        "Office hall archive desks are deterministic and clear all reserved zones in four rotations");
}

void testBreakableCafeteriaPlantsAndSecurityMonitors()
{
    using namespace gdp::world;
    using namespace wkc::core;
    const auto library = makeDefaultRoomModuleLibrary();
    bool valid = true;
    for (const RoomModuleId id : {601U, 605U, 606U,
             701U, 702U, 703U, 704U}) {
        const auto module = std::find_if(library.begin(), library.end(),
            [id](const RoomModule& candidate) { return candidate.id == id; });
        if (module == library.end()) {
            valid = false;
            continue;
        }
        for (const QuarterTurn rotation : {QuarterTurn::Degrees0,
                 QuarterTurn::Degrees90, QuarterTurn::Degrees180,
                 QuarterTurn::Degrees270}) {
            const auto room = instantiateRoom(*module, id,
                RoomRole::Normal, 1116U, {-15.85F, 0.0F, 10.30F}, rotation);
            const auto otherSeed = instantiateRoom(*module, id,
                RoomRole::Normal, 1793U, {-15.85F, 0.0F, 10.30F}, rotation);
            if (!room || !otherSeed) {
                valid = false;
                continue;
            }
            const auto parts = makeRoomArchitecture(*room);
            const auto otherParts = makeRoomArchitecture(*otherSeed);
            const bool clear = !parts.empty()
                && validateRoomArchitecture(*room, parts)
                && parts == otherParts;
            valid &= clear;
            std::size_t breakablePlants = 0U;
            std::size_t breakableMonitors = 0U;
            std::size_t monitorBodies = 0U;
            std::size_t monitorStrips = 0U;
            std::size_t observationPanels = 0U;
            for (const RoomArchitecturePart& part : parts) {
                if (part.decoration == RoomDecorationPart::Planter
                    || part.decoration == RoomDecorationPart::BroadleafPlanter) {
                    breakablePlants += static_cast<std::size_t>(part.breakable);
                }
                if (part.securityFurniturePart
                    == SecurityFurniturePart::Monitor) {
                    breakableMonitors += static_cast<std::size_t>(part.breakable);
                    const float height = part.bounds.maximum.y
                        - part.bounds.minimum.y;
                    monitorBodies += static_cast<std::size_t>(height > 0.3F);
                    monitorStrips += static_cast<std::size_t>(height < 0.2F);
                }
                if (part.securityFurniturePart
                    == SecurityFurniturePart::ObservationPanel) {
                    ++observationPanels;
                    valid &= !part.breakable;
                }
            }
            if (id >= 701U) {
                valid &= breakablePlants == 2U;
            } else {
                valid &= breakableMonitors == 2U
                    && monitorBodies == (id == 601U ? 1U : 2U)
                    && monitorStrips == (id == 601U ? 1U : 0U)
                    && observationPanels == (id == 605U ? 1U : 0U);
            }
            if (!clear) {
                std::cerr << "Breakable room module " << id << " rotation "
                    << static_cast<int>(rotation) << " failed clearance\n";
            }
        }
    }
    expect(valid,
        "Cafeteria plants and Security monitor bodies remain breakable without blocking four rotations");
}

void testCompactSecurityConsoleTrim()
{
    using namespace gdp::world;
    using namespace wkc::core;
    const auto library = makeDefaultRoomModuleLibrary();
    bool valid = true;
    for (const RoomModuleId id : {601U, 602U, 603U}) {
        const auto module = std::find_if(library.begin(), library.end(),
            [id](const RoomModule& candidate) { return candidate.id == id; });
        if (module == library.end()) {
            valid = false;
            continue;
        }
        const RoomRole role = id == 603U ? RoomRole::Arena
            : RoomRole::Normal;
        for (const QuarterTurn rotation : {QuarterTurn::Degrees0,
                 QuarterTurn::Degrees90, QuarterTurn::Degrees180,
                 QuarterTurn::Degrees270}) {
            const auto room = instantiateRoom(*module, id, role, 1116U,
                {-15.85F, 0.0F, 10.30F}, rotation);
            if (!room) {
                valid = false;
                continue;
            }
            const auto parts = makeRoomArchitecture(*room);
            std::size_t tops = 0U;
            std::size_t trims = 0U;
            bool topSizesValid = true;
            for (const RoomArchitecturePart& part : parts) {
                const Vec3 size = part.bounds.maximum - part.bounds.minimum;
                if (part.securityFurniturePart
                    == SecurityFurniturePart::ConsoleTop) {
                    ++tops;
                    topSizesValid &= std::min(size.x, size.z) > 0.5F;
                } else if (part.securityFurniturePart
                    == SecurityFurniturePart::ConsoleTrim) {
                    ++trims;
                    topSizesValid &= std::min(size.x, size.z) < 0.1F;
                }
            }
            const bool roomValid = validateRoomArchitecture(*room, parts);
            if (!roomValid || tops != 1U || trims != 3U || !topSizesValid) {
                std::cerr << "Compact Security console " << id
                    << " rotation " << static_cast<int>(rotation)
                    << " parts " << parts.size() << " tops " << tops
                    << " trims " << trims << " sizes " << topSizesValid
                    << " clearance " << roomValid << '\n';
            }
            valid &= roomValid && tops == 1U
                && trims == (id == 601U ? 3U : 4U) && topSizesValid;
        }
    }
    expect(valid,
        "Compact Security drawer faces never generate full-size console tops in four rotations");
}

void testSecretLaboratorySampleVaults()
{
    using namespace gdp::world;
    using namespace wkc::core;
    const auto library = makeDefaultRoomModuleLibrary();
    bool valid = true;
    for (const RoomModuleId id : {401U, 402U, 403U}) {
        const auto module = std::find_if(library.begin(), library.end(),
            [id](const RoomModule& candidate) { return candidate.id == id; });
        if (module == library.end()) {
            valid = false;
            continue;
        }
        const RoomRole role = id == 403U ? RoomRole::Secret : RoomRole::Normal;
        for (const QuarterTurn rotation : {QuarterTurn::Degrees0,
                 QuarterTurn::Degrees90, QuarterTurn::Degrees180,
                 QuarterTurn::Degrees270}) {
            const auto room = instantiateRoom(*module, id, role, 1793U,
                {-15.85F, 0.0F, 10.30F}, rotation);
            const auto otherSeed = instantiateRoom(*module, id, role, 1116U,
                {-15.85F, 0.0F, 10.30F}, rotation);
            if (!room || !otherSeed) {
                valid = false;
                continue;
            }
            const auto parts = makeRoomArchitecture(*room);
            const bool clear = validateRoomArchitecture(*room, parts)
                && parts == makeRoomArchitecture(*otherSeed);
            const auto statusStrips = std::count_if(parts.begin(), parts.end(),
                [](const RoomArchitecturePart& part) {
                    return part.laboratoryFurniturePart
                        == LaboratoryFurniturePart::StatusStrip;
                });
            valid &= clear && (id == 403U
                ? parts.size() >= 30U && statusStrips == 2
                : parts.size() >= 15U && statusStrips == 0);
            if (!clear || (id == 403U
                    ? parts.size() < 30U || statusStrips != 2
                    : parts.size() < 15U || statusStrips != 0)) {
                std::cerr << "Lab module " << id << " rotation "
                    << static_cast<int>(rotation) << " parts "
                    << parts.size() << " strips " << statusStrips
                    << " clearance " << clear << '\n';
            }
            if (!clear) {
                std::cerr << "Secret laboratory module " << id
                    << " rotation " << static_cast<int>(rotation)
                    << " failed clearance\n";
            }
        }
    }
    expect(valid,
        "Laboratory furniture and the two secret sample vaults clear all rotations");
}

void testBossDifficultyProfiles()
{
    using namespace gdp::entities;
    using namespace gdp::world;
    using namespace wkc::core;

    struct Scenario {
        float difficulty;
        bool campaign;
        std::uint32_t floor;
        float bossScale;
        std::array<EnemyArchetype, 4> aides;
        std::size_t aideCount;
    };
    const std::array scenarios{
        Scenario{0.75F, false, 1U, 1.90F, {EnemyArchetype::Guard}, 1U},
        Scenario{1.0F, false, 1U, 1.90F,
            {EnemyArchetype::Officer, EnemyArchetype::Guard}, 2U},
        Scenario{1.25F, false, 1U, 1.90F,
            {EnemyArchetype::Officer, EnemyArchetype::Guard,
                EnemyArchetype::Guard}, 3U},
        Scenario{0.75F, true, 1U, 1.65F, {EnemyArchetype::Guard}, 1U},
        Scenario{1.0F, true, 1U, 1.65F,
            {EnemyArchetype::Officer, EnemyArchetype::Guard}, 2U},
        Scenario{1.25F, true, 1U, 1.65F,
            {EnemyArchetype::Officer, EnemyArchetype::Guard,
                EnemyArchetype::Guard}, 3U},
        Scenario{0.83F, true, 2U, 1.72F, {EnemyArchetype::Officer}, 1U},
        Scenario{1.08F, true, 2U, 1.72F,
            {EnemyArchetype::Officer, EnemyArchetype::Guard}, 2U},
        Scenario{1.33F, true, 2U, 1.72F,
            {EnemyArchetype::Officer, EnemyArchetype::Guard,
                EnemyArchetype::Guard}, 3U},
        Scenario{1.07F, true, 5U, 1.82F, {EnemyArchetype::Officer}, 1U},
        Scenario{1.32F, true, 5U, 1.82F,
            {EnemyArchetype::Officer, EnemyArchetype::Guard}, 2U},
        Scenario{1.57F, true, 5U, 1.82F,
            {EnemyArchetype::Officer, EnemyArchetype::Guard,
                EnemyArchetype::Guard}, 3U},
        Scenario{1.47F, true, 10U, 1.90F,
            {EnemyArchetype::Officer, EnemyArchetype::Guard}, 2U},
        Scenario{1.72F, true, 10U, 1.90F,
            {EnemyArchetype::Officer, EnemyArchetype::Guard,
                EnemyArchetype::Guard}, 3U},
        Scenario{1.97F, true, 10U, 1.90F,
            {EnemyArchetype::Officer, EnemyArchetype::Guard,
                EnemyArchetype::Guard, EnemyArchetype::Officer}, 4U},
    };
    bool validProfiles = true;
    bool clearSpawns = true;
    bool coverValid = true;
    bool arenaColumnsValid = true;
    std::array<bool, 4> rotations{};
    for (Seed seed = 0U; seed < 64U; ++seed) {
        const auto generated = generateWorld(seed);
        if (!generated.world) {
            validProfiles = false;
            continue;
        }
        const auto boss = std::find_if(generated.world->rooms.begin(),
            generated.world->rooms.end(),
            [](const RoomInstance& room) { return room.role == RoomRole::Boss; });
        if (boss == generated.world->rooms.end()) {
            validProfiles = false;
            continue;
        }
        rotations[static_cast<std::size_t>(boss->rotation)] = true;
        float seedBaseHealth = 0.0F;
        for (const Scenario& scenario : scenarios) {
            World populated = *generated.world;
            EntityManager manager;
            const InitialEntitySet initial = populateInitialEntities(populated, manager,
                {.transform = {.position = {0.0F, 0.0F, 4.0F}}},
                scenario.difficulty, scenario.campaign, scenario.floor);
            validProfiles &= initial.isValid();
            const auto room = std::find_if(populated.rooms.begin(), populated.rooms.end(),
                [](const RoomInstance& candidate) { return candidate.role == RoomRole::Boss; });
            if (room == populated.rooms.end()) {
                validProfiles = false;
                continue;
            }
            const auto arenaColumns = makeRoomArchitecture(*room);
            arenaColumnsValid &= arenaColumns.size() == 115U
                && validateRoomArchitecture(*room, arenaColumns);
            arenaColumnsValid &= std::count_if(arenaColumns.begin(),
                arenaColumns.end(), [](const RoomArchitecturePart& part) {
                    return part.combatPart
                        == CombatArchitecturePart::ColumnInset;
                }) == 8;
            arenaColumnsValid &= std::count_if(arenaColumns.begin(),
                arenaColumns.end(), [](const RoomArchitecturePart& part) {
                    return part.combatPart
                        == CombatArchitecturePart::CeilingBeam;
                }) == 6;
            arenaColumnsValid &= std::count_if(arenaColumns.begin(),
                arenaColumns.end(), [](const RoomArchitecturePart& part) {
                    return part.combatPart
                        == CombatArchitecturePart::GallerySupport;
                }) == 4;
            arenaColumnsValid &= std::count_if(arenaColumns.begin(),
                arenaColumns.end(), [](const RoomArchitecturePart& part) {
                    return part.combatPart
                        == CombatArchitecturePart::CeilingGrille;
                }) == 24;
            for (const float z : {-8.0F, 8.0F}) {
                const Vec3 center = room->transform.position
                    + rotateQuarterTurns({7.0F, 0.0F, z}, room->rotation);
                arenaColumnsValid &= std::any_of(arenaColumns.begin(), arenaColumns.end(),
                    [center, room](const RoomArchitecturePart& part) {
                        const Vec3 midpoint = (part.bounds.minimum + part.bounds.maximum)
                            * 0.5F;
                        return part.structuralColumn
                            && std::abs(midpoint.x - center.x) < 0.01F
                            && std::abs(midpoint.z - center.z) < 0.01F
                            && part.bounds.maximum.y - part.bounds.minimum.y
                                >= room->worldBounds.maximum.y - 0.2F;
                    });
            }
            std::size_t fixedCoverCount = 0U;
            for (const EntityId id : room->entities) {
                const auto* prop = manager.findData<PropData>(id);
                const auto* entity = manager.find(id);
                if (!prop || !entity || prop->type != PropType::Barricade
                    || (prop->maximumHealth != 120.0F
                        && prop->maximumHealth != 180.0F)) continue;
                ++fixedCoverCount;
                const bool low = prop->maximumHealth == 120.0F;
                const Vec3 offset = low ? Vec3{-8.5F, 0.0F, -8.5F}
                    : Vec3{-8.5F, 0.0F, 8.5F};
                const Vec3 localScale = low ? Vec3{2.4F, 1.35F, 0.6F}
                    : Vec3{0.6F, 2.25F, 2.4F};
                const Vec3 rotatedScale = rotateQuarterTurns(localScale, room->rotation);
                coverValid &= prop->solid && prop->health == prop->maximumHealth
                    && prop->visualSeed != 0U
                    && nearlyEqual(entity->transform.position,
                        room->transform.position + rotateQuarterTurns(offset, room->rotation))
                    && nearlyEqual(entity->transform.scale,
                        Vec3{std::abs(rotatedScale.x), localScale.y,
                            std::abs(rotatedScale.z)});
            }
            coverValid &= fixedCoverCount == 2U;
            std::vector<const EnemyData*> enemies;
            std::vector<const Entity*> enemyEntities;
            for (const EntityId id : room->entities) {
                const auto* enemy = manager.findData<EnemyData>(id);
                if (enemy) {
                    enemies.push_back(enemy);
                    enemyEntities.push_back(manager.find(id));
                }
            }
            validProfiles &= enemies.size() == scenario.aideCount + 1U;
            if (enemies.size() != scenario.aideCount + 1U) continue;
            const float baseHealth = enemies[0]->health
                / (scenario.difficulty * scenario.bossScale);
            if (seedBaseHealth == 0.0F) seedBaseHealth = baseHealth;
            validProfiles &= enemies[0]->archetype == EnemyArchetype::IndustrialRobot
                && enemies[0]->active && std::abs(baseHealth - seedBaseHealth) < 0.001F
                && (std::abs(baseHealth - 360.0F) < 0.001F
                    || std::abs(baseHealth - 390.0F) < 0.001F
                    || std::abs(baseHealth - 420.0F) < 0.001F);
            for (std::size_t index = 0U; index < scenario.aideCount; ++index) {
                constexpr std::array aideOffsets{
                    Vec3{-5.0F, 0.0F, -4.0F}, Vec3{-5.0F, 0.0F, 4.0F},
                    Vec3{-9.0F, 0.0F, -3.0F}, Vec3{-9.0F, 0.0F, 3.0F},
                };
                validProfiles &= enemies[index + 1U]->archetype == scenario.aides[index]
                    && enemies[index + 1U]->active
                    && nearlyEqual(enemyEntities[index + 1U]->transform.position,
                        room->transform.position + rotateQuarterTurns(
                            aideOffsets[index], room->rotation));
            }
            for (std::size_t index = 0U; index < enemyEntities.size(); ++index) {
                const Entity* entity = enemyEntities[index];
                if (!entity) { clearSpawns = false; continue; }
                const Vec3 position = entity->transform.position;
                constexpr float radius = 0.9F;
                clearSpawns &= position.x - radius >= room->worldBounds.minimum.x
                    && position.x + radius <= room->worldBounds.maximum.x
                    && position.z - radius >= room->worldBounds.minimum.z
                    && position.z + radius <= room->worldBounds.maximum.z;
                for (std::size_t other = 0U; other < index; ++other) {
                    const Vec3 delta = position - enemyEntities[other]->transform.position;
                    clearSpawns &= delta.x * delta.x + delta.z * delta.z >= 4.0F;
                }
                for (const ConnectorInstance& connector : room->connectors) {
                    const Vec3 delta = position - connector.worldPosition;
                    clearSpawns &= delta.x * delta.x + delta.z * delta.z >= 4.0F;
                }
                for (const EntityId id : room->entities) {
                    const auto* prop = manager.findData<PropData>(id);
                    const auto* obstacle = manager.find(id);
                    if (!prop || !prop->solid || !obstacle) continue;
                    const float halfX = obstacle->transform.scale.x * 0.5F;
                    const float halfZ = obstacle->transform.scale.z * 0.5F;
                    const float closestX = std::clamp(position.x,
                        obstacle->transform.position.x - halfX,
                        obstacle->transform.position.x + halfX);
                    const float closestZ = std::clamp(position.z,
                        obstacle->transform.position.z - halfZ,
                        obstacle->transform.position.z + halfZ);
                    const float dx = position.x - closestX;
                    const float dz = position.z - closestZ;
                    clearSpawns &= dx * dx + dz * dz >= radius * radius;
                }
                for (const RoomArchitecturePart& part : arenaColumns) {
                    if (!part.solid) continue;
                    const float closestX = std::clamp(position.x,
                        part.bounds.minimum.x, part.bounds.maximum.x);
                    const float closestZ = std::clamp(position.z,
                        part.bounds.minimum.z, part.bounds.maximum.z);
                    const float dx = position.x - closestX;
                    const float dz = position.z - closestZ;
                    clearSpawns &= dx * dx + dz * dz >= radius * radius;
                }
            }
        }
    }
    expect(validProfiles, "Boss health and active aide composition match mode, difficulty and floor");
    expect(clearSpawns, "Boss and aide spawns clear walls, doors, terminal, props and each other");
    expect(coverValid, "Boss has two rotated low/tall barricades on the left flank");
    expect(arenaColumnsValid,
        "Boss has two full-height right columns clear of doors and reserved zones");
    expect(std::all_of(rotations.begin(), rotations.end(), [](bool seen) { return seen; }),
        "Boss spawn batch covers all four room rotations");
}

void testSeparatedTerminalAcrossMapSizes()
{
    using namespace gdp::world;
    bool mapsValid = true;
    bool terminalSeparated = true;
    bool branchPlacement = true;
    for (std::uint32_t roomCount = 4U; roomCount <= 10U; ++roomCount) {
        for (wkc::core::Seed seed = 0U; seed < 1'000U; ++seed) {
            const auto generated = generateWorld(seed,
                {.roomCount = roomCount, .loopBudget = 1U,
                    .maximumAttempts = 128U});
            if (!generated.world || !validateWorld(*generated.world).isValid()) {
                mapsValid = false;
                std::cerr << "Terminal layout failed rooms=" << roomCount
                    << " seed=" << seed << '\n';
                continue;
            }
            const auto& graph = generated.world->graph;
            const auto terminal = std::find_if(graph.nodes.begin(), graph.nodes.end(),
                [](const RoomNode& node) { return node.role == RoomRole::Terminal; });
            const auto start = std::find_if(graph.nodes.begin(), graph.nodes.end(),
                [](const RoomNode& node) { return node.role == RoomRole::Start; });
            terminalSeparated &= terminal != graph.nodes.end()
                && start != graph.nodes.end() && terminal->id != start->id;
            if (terminal == graph.nodes.end() || start == graph.nodes.end()) continue;
            for (const MapEdge& edge : graph.edges) {
                terminalSeparated &= !((edge.first == start->id
                    && edge.second == terminal->id)
                    || (edge.second == start->id
                        && edge.first == terminal->id));
            }
            if (roomCount >= 6U) {
                branchPlacement &= std::find(graph.mainPath.begin(),
                    graph.mainPath.end(), terminal->id) == graph.mainPath.end();
            }
        }
    }
    expect(mapsValid, "Terminal layouts generate valid maps across 4-10 rooms");
    expect(terminalSeparated,
        "Terminal is separate from START and never directly connected to it");
    expect(branchPlacement,
        "Terminal occupies a procedural branch when the map has branches");
}

} // namespace

int main()
{
    testModuleValidation();
    testDefaultModuleStructuralVariety();
    testCampaignRoomVariants();
    testRoomWallColumns();
    testCentralHubPreviewArchitecture();
    testSecurityPreviewArchitecture();
    testTechnicalPreviewArchitecture();
    testMaintenanceLAtFractionalCoordinates();
    testCampaignArchitectureAtFractionalCoordinates();
    testLaboratoryPreviewArchitecture();
    testStorageArchitecture();
    testInstantiationAndRotation();
    testConnectorContracts();
    testRequiredConnectorResolution();
    testMapGraph();
    testWorldGeneration();
    testCampaignCorridorGauges();
    testCorridorRouting();
    testCorridorWallColumns();
    testInitialEntityPopulation();
    testCampaignExitTerminalPopulation();
    testCampaignEncounterProgression();
    testBossTerminalLightingAcrossRotations();
    testCompactFurnitureClearanceAcrossRotations();
    testOfficeHallArchiveDesks();
    testBreakableCafeteriaPlantsAndSecurityMonitors();
    testCompactSecurityConsoleTrim();
    testSecretLaboratorySampleVaults();
    testBossDifficultyProfiles();
    testWorldSeedBatch();
    testSeparatedTerminalAcrossMapSizes();

    if (failures != 0) {
        std::cerr << failures << " world module test(s) failed\n";
        return 1;
    }

    std::cout << "All GDP world module tests passed\n";
    return 0;
}
