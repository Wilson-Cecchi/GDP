#include "world/RoomModule.hpp"
#include "world/RoomFootprint.hpp"

#include <algorithm>
#include <array>
#include <unordered_set>
#include <utility>

namespace gdp::world {
namespace {

bool isZoneInside(const SpawnZone& zone, const wkc::core::Aabb bounds) noexcept
{
    return zone.bounds.isValid() && bounds.contains(zone.bounds.minimum)
        && bounds.contains(zone.bounds.maximum);
}

bool isConnectorOnPerimeter(
    const ConnectorDefinition& connector,
    const wkc::core::Aabb bounds) noexcept
{
    if (connector.position.y < bounds.minimum.y
        || connector.position.y + connector.height > bounds.maximum.y) {
        return false;
    }

    const auto direction = directionFromNormal(connector.normal);
    if (!direction) {
        return false;
    }
    switch (*direction) {
    case Direction::North:
        return wkc::core::nearlyEqual(connector.position.z, bounds.maximum.z);
    case Direction::East:
        return wkc::core::nearlyEqual(connector.position.x, bounds.maximum.x);
    case Direction::South:
        return wkc::core::nearlyEqual(connector.position.z, bounds.minimum.z);
    case Direction::West:
        return wkc::core::nearlyEqual(connector.position.x, bounds.minimum.x);
    }
    return false;
}

wkc::core::Aabb connectorClearance(const ConnectorDefinition& connector) noexcept
{
    constexpr float sideMargin = 0.25F;
    constexpr float inwardDepth = 0.75F;
    const float halfWidth = (connector.width * 0.5F) + sideMargin;
    const auto direction = directionFromNormal(connector.normal);
    if (!direction) {
        return {};
    }

    switch (*direction) {
    case Direction::North:
        return {
            {connector.position.x - halfWidth, connector.position.y,
                connector.position.z - inwardDepth},
            {connector.position.x + halfWidth, connector.position.y + connector.height,
                connector.position.z},
        };
    case Direction::East:
        return {
            {connector.position.x - inwardDepth, connector.position.y,
                connector.position.z - halfWidth},
            {connector.position.x, connector.position.y + connector.height,
                connector.position.z + halfWidth},
        };
    case Direction::South:
        return {
            {connector.position.x - halfWidth, connector.position.y,
                connector.position.z},
            {connector.position.x + halfWidth, connector.position.y + connector.height,
                connector.position.z + inwardDepth},
        };
    case Direction::West:
        return {
            {connector.position.x, connector.position.y,
                connector.position.z - halfWidth},
            {connector.position.x + inwardDepth, connector.position.y + connector.height,
                connector.position.z + halfWidth},
        };
    }
    return {};
}

bool overlaps(const wkc::core::Aabb first, const wkc::core::Aabb second) noexcept
{
    return first.minimum.x <= second.maximum.x && first.maximum.x >= second.minimum.x
        && first.minimum.y <= second.maximum.y && first.maximum.y >= second.minimum.y
        && first.minimum.z <= second.maximum.z && first.maximum.z >= second.minimum.z;
}

RoomModule makeRectangularModule(
    const RoomModuleId id,
    std::string name,
    const RoomTheme theme,
    const RoomSizeClass sizeClass,
    const GridSize footprint,
    const RoomRoleMask roles)
{
    constexpr float wallInset = 0.15F;
    const float halfX = (static_cast<float>(footprint.x) * 0.5F) - wallInset;
    const float halfZ = (static_cast<float>(footprint.z) * 0.5F) - wallInset;

    RoomModule module;
    module.id = id;
    module.name = std::move(name);
    module.theme = theme;
    module.sizeClass = sizeClass;
    module.footprint = footprint;
    module.localBounds = {
        {-halfX, 0.0F, -halfZ},
        {halfX, static_cast<float>(footprint.y), halfZ},
    };
    module.roleCapabilities = roles;
    module.connectors = {
        {1, ConnectorType::Door, {0.0F, 0.0F, halfZ}, {0.0F, 0.0F, 1.0F},
            1.2F, 2.2F, false},
        {2, ConnectorType::Door, {0.0F, 0.0F, -halfZ}, {0.0F, 0.0F, -1.0F},
            1.2F, 2.2F, false},
        {3, ConnectorType::Door, {halfX, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F},
            1.2F, 2.2F, false},
        {4, ConnectorType::Door, {-halfX, 0.0F, 0.0F}, {-1.0F, 0.0F, 0.0F},
            1.2F, 2.2F, false},
    };

    const float enemyHalfX = std::min(halfX - 1.15F, halfX * 0.48F);
    const float enemyHalfZ = std::min(halfZ - 1.15F, halfZ * 0.34F);
    module.enemyZones = {{{{-enemyHalfX, 0.0F, -enemyHalfZ},
        {enemyHalfX, 0.0F, enemyHalfZ}}}};
    module.lootZones = {{{{-halfX + 0.70F, 0.0F, -halfZ + 0.80F},
        {-halfX + 1.90F, 0.0F, -halfZ + 2.00F}}}};
    module.propZones = {{{{1.15F, 0.0F, -halfZ + 0.80F},
        {halfX - 1.00F, 0.0F, halfZ - 0.80F}}}};
    if (theme == RoomTheme::Storage) {
        // Keep loose loot clear of the shelving along the left wall.
        module.lootZones = {{{{-halfX + 1.05F, 0.0F, -halfZ + 0.80F},
            {-halfX + 1.90F, 0.0F, -halfZ + 2.00F}}}};
    }
    return module;
}

RoomModule makeBossArenaModule(
    const RoomModuleId id,
    std::string name,
    const RoomTheme theme)
{
    RoomModule module = makeRectangularModule(id, std::move(name), theme,
        RoomSizeClass::Large, {28, 7, 32}, roomRoleBit(RoomRole::Boss));
    // Keep procedural breakables on the inside of the two right-side columns.
    // The central firing lane, fixed left cover, aides and all four doors stay clear.
    module.propZones = {
        {{{2.0F, 0.0F, -12.0F}, {4.8F, 0.0F, -7.0F}}},
        {{{2.0F, 0.0F, 7.0F}, {4.8F, 0.0F, 12.0F}}},
    };
    return module;
}

RoomModule makeStorageHallModule()
{
    RoomModule module = makeRectangularModule(
        204, "StorageHall_01", RoomTheme::Storage,
        RoomSizeClass::Large, {18, 6, 20}, roomRoleBit(RoomRole::Normal));
    // The hall's loose props occupy a separate pocket behind the enemy zone.
    // Keep the side shelves and the two free-standing racks available as
    // cover without letting generated props intersect their solid geometry.
    module.propZones = {{{{5.15F, 0.0F, -8.50F},
        {6.95F, 0.0F, -4.00F}}}};
    return module;
}

RoomModule makeLaboratoryHallModule()
{
    RoomModule module = makeRectangularModule(
        laboratoryHallModuleId, "LaboratoryHall_01", RoomTheme::Laboratory,
        RoomSizeClass::Large, {18, 6, 20},
        roomRoleBit(RoomRole::Normal) | roomRoleBit(RoomRole::Terminal));
    module.localBounds = {{-9.0F, 0.0F, -10.0F}, {9.0F, 6.0F, 10.0F}};
    module.connectors[0].position.z = 10.0F;
    module.connectors[1].position.z = -10.0F;
    module.connectors[2].position.x = 9.0F;
    module.connectors[3].position.x = -9.0F;
    // The authored side installations and island benches leave this broad
    // center lane free for encounter spawns and terminal reinforcements.
    module.enemyZones = {{{{-2.5F, 0.0F, -2.0F},
        {2.5F, 0.0F, 2.0F}}}};
    module.lootZones = {{{{-6.5F, 0.0F, 5.6F},
        {-2.9F, 0.0F, 8.5F}}}};
    module.propZones = {{{{3.0F, 0.0F, -8.5F},
        {6.8F, 0.0F, -1.8F}}}};
    return module;
}

RoomModule makeCentralHubModule()
{
    RoomModule module = makeRectangularModule(centralHubModuleId, "IndustrialHub_01",
        RoomTheme::Industrial, RoomSizeClass::Large, {20, 6, 20},
        roomRoleBit(RoomRole::Hub));
    module.localBounds = {{-10.0F, 0.0F, -10.0F}, {10.0F, 6.0F, 10.0F}};
    module.localOutline = centralHubPreviewFootprint();
    module.connectors[0].position.z = 10.0F;
    module.connectors[1].position.z = -10.0F;
    module.connectors[2].position.x = 10.0F;
    module.connectors[3].position.x = -10.0F;
    module.enemyZones = {
        {{{-6.0F, 0.0F, -6.0F}, {-5.0F, 0.0F, -5.0F}}},
        {{{5.0F, 0.0F, 5.0F}, {6.0F, 0.0F, 6.0F}}},
    };
    module.propZones = {{{{3.0F, 0.0F, -1.5F}, {7.0F, 0.0F, 1.5F}}}};
    module.lootZones = {{{{-6.0F, 0.0F, -1.0F}, {-5.0F, 0.0F, 1.0F}}}};
    return module;
}

RoomModule makeMaintenanceLModule()
{
    RoomModule module = makeRectangularModule(maintenanceLModuleId, "MaintenanceL_01",
        RoomTheme::Maintenance, RoomSizeClass::Large, {16, 6, 20},
        roomRoleBit(RoomRole::Normal));
    module.localBounds = {{-8.0F, 0.0F, -10.0F}, {8.0F, 5.5F, 10.0F}};
    module.localOutline = technicalLPreviewFootprint();
    module.connectors[0].position.z = 10.0F;
    module.connectors[1].position.z = -10.0F;
    module.connectors[2].position.x = 8.0F;
    module.connectors[3].position.x = -8.0F;
    module.enemyZones = {
        {{{-0.8F, 0.0F, -6.5F}, {0.8F, 0.0F, -5.3F}}},
        {{{-0.8F, 0.0F, 5.3F}, {0.8F, 0.0F, 6.5F}}},
    };
    module.propZones = {{{{1.6F, 0.0F, -1.4F}, {6.1F, 0.0F, 1.0F}}}};
    module.lootZones = {{{{-4.0F, 0.0F, -0.8F}, {-3.0F, 0.0F, 0.8F}}}};
    return module;
}

RoomModule makeCombatGalleryArenaModule()
{
    RoomModule module = makeRectangularModule(
        combatGalleryArenaModuleId, "MaintenanceGalleryArena_01",
        RoomTheme::Maintenance, RoomSizeClass::Large, {20, 6, 22},
        roomRoleBit(RoomRole::Arena));
    // Keep the preview's wall-mounted stair dimensions and the four real
    // doorway positions. Reserved placement areas stay off the stairs.
    module.localBounds = {{-10.0F, 0.0F, -11.0F}, {10.0F, 6.0F, 11.0F}};
    module.connectors[0].position.z = 11.0F;
    module.connectors[1].position.z = -11.0F;
    module.connectors[2].position.x = 10.0F;
    module.connectors[3].position.x = -10.0F;
    module.enemyZones = {{{{-2.0F, 0.0F, -2.0F}, {2.0F, 0.0F, 2.0F}}}};
    module.lootZones = {{{{-6.0F, 0.0F, -7.8F},
        {-4.7F, 0.0F, -6.6F}}}};
    module.propZones = {{{{4.8F, 0.0F, -8.2F},
        {7.0F, 0.0F, -2.8F}}}};
    return module;
}

RoomModule makeSecurityTModule()
{
    RoomModule module = makeRectangularModule(
        securityTModuleId, "SecurityT_01", RoomTheme::Security,
        RoomSizeClass::Large, {20, 6, 20},
        roomRoleBit(RoomRole::Normal) | roomRoleBit(RoomRole::Terminal));
    module.localBounds = {{-10.0F, 0.0F, -10.0F}, {10.0F, 6.0F, 10.0F}};
    module.localOutline = securityTPreviewFootprint();
    module.connectors[0].position.z = 10.0F;
    module.connectors[1].position.z = -10.0F;
    module.connectors[2].position.x = 10.0F;
    module.connectors[3].position.x = -10.0F;
    // The Terminal encounter stages two more enemies beside the three fixed
    // defenders, so the event zone needs room for both without crowding them.
    module.enemyZones = {{{{-1.4F, 0.0F, -1.5F},
        {1.4F, 0.0F, 1.5F}}}};
    module.propZones = {{{{1.8F, 0.0F, -0.6F},
        {7.6F, 0.0F, 0.6F}}}};
    module.lootZones = {{{{-4.0F, 0.0F, -0.8F},
        {-3.0F, 0.0F, 0.8F}}}};
    return module;
}

RoomModule makeCafeteriaLongModule()
{
    RoomModule module = makeRectangularModule(
        cafeteriaLongModuleId, "CafeteriaLong_01", RoomTheme::Cafeteria,
        RoomSizeClass::Large, {18, 6, 24}, roomRoleBit(RoomRole::Normal));
    module.localBounds = {{-9.0F, 0.0F, -12.0F}, {9.0F, 6.0F, 12.0F}};
    module.connectors[0].position.z = 12.0F;
    module.connectors[1].position.z = -12.0F;
    module.connectors[2].position.x = 9.0F;
    module.connectors[3].position.x = -9.0F;
    module.enemyZones = {
        {{{-0.8F, 0.0F, -9.7F}, {0.8F, 0.0F, -8.8F}}},
        {{{-0.8F, 0.0F, 8.8F}, {0.8F, 0.0F, 9.7F}}},
    };
    module.lootZones = {{{{-2.3F, 0.0F, -5.2F},
        {-1.3F, 0.0F, -4.2F}}}};
    module.propZones = {{{{-2.9F, 0.0F, 3.0F},
        {-1.7F, 0.0F, 9.0F}}}};
    return module;
}

wkc::core::Aabb transformBounds(
    const wkc::core::Aabb localBounds,
    const wkc::core::Vec3 origin,
    const wkc::core::QuarterTurn rotation) noexcept
{
    const std::array corners{
        wkc::core::Vec3{localBounds.minimum.x, localBounds.minimum.y, localBounds.minimum.z},
        wkc::core::Vec3{localBounds.maximum.x, localBounds.minimum.y, localBounds.minimum.z},
        wkc::core::Vec3{localBounds.minimum.x, localBounds.maximum.y, localBounds.minimum.z},
        wkc::core::Vec3{localBounds.maximum.x, localBounds.maximum.y, localBounds.minimum.z},
        wkc::core::Vec3{localBounds.minimum.x, localBounds.minimum.y, localBounds.maximum.z},
        wkc::core::Vec3{localBounds.maximum.x, localBounds.minimum.y, localBounds.maximum.z},
        wkc::core::Vec3{localBounds.minimum.x, localBounds.maximum.y, localBounds.maximum.z},
        wkc::core::Vec3{localBounds.maximum.x, localBounds.maximum.y, localBounds.maximum.z},
    };

    wkc::core::Vec3 minimum = origin + wkc::core::rotateQuarterTurns(corners[0], rotation);
    wkc::core::Vec3 maximum = minimum;
    for (const wkc::core::Vec3 corner : corners) {
        const wkc::core::Vec3 transformed = origin
            + wkc::core::rotateQuarterTurns(corner, rotation);
        minimum.x = std::min(minimum.x, transformed.x);
        minimum.y = std::min(minimum.y, transformed.y);
        minimum.z = std::min(minimum.z, transformed.z);
        maximum.x = std::max(maximum.x, transformed.x);
        maximum.y = std::max(maximum.y, transformed.y);
        maximum.z = std::max(maximum.z, transformed.z);
    }
    return {minimum, maximum};
}

} // namespace

bool RoomModule::supportsRole(const RoomRole role) const noexcept
{
    if (role >= RoomRole::Count) {
        return false;
    }
    return (roleCapabilities & roomRoleBit(role)) != 0U;
}

bool ModuleValidationResult::isValid() const noexcept
{
    return errors.empty();
}

ModuleValidationResult validateRoomModule(const RoomModule& module)
{
    ModuleValidationResult result;
    if (module.id == 0) {
        result.errors.push_back(ModuleValidationError::InvalidId);
    }
    if (module.name.empty()) {
        result.errors.push_back(ModuleValidationError::EmptyName);
    }
    if (module.footprint.x <= 0 || module.footprint.y <= 0 || module.footprint.z <= 0) {
        result.errors.push_back(ModuleValidationError::InvalidFootprint);
    }
    if (!module.localBounds.isValid()) {
        result.errors.push_back(ModuleValidationError::InvalidBounds);
    } else {
        const wkc::core::Vec3 span = module.localBounds.maximum
            - module.localBounds.minimum;
        constexpr float maximumInset = 1.0F;
        constexpr float tolerance = 1.0e-4F;
        if (span.x > static_cast<float>(module.footprint.x) + tolerance
            || span.y > static_cast<float>(module.footprint.y) + tolerance
            || span.z > static_cast<float>(module.footprint.z) + tolerance
            || span.x < static_cast<float>(module.footprint.x) - maximumInset
            || span.y < static_cast<float>(module.footprint.y) - maximumInset
            || span.z < static_cast<float>(module.footprint.z) - maximumInset) {
            result.errors.push_back(ModuleValidationError::BoundsFootprintMismatch);
        }
    }
    if (module.roleCapabilities == 0U) {
        result.errors.push_back(ModuleValidationError::NoRoleCapability);
    }
    if (module.connectors.empty()) {
        result.errors.push_back(ModuleValidationError::NoConnectors);
    }

    std::unordered_set<ConnectorId> connectorIds;
    for (const ConnectorDefinition& connector : module.connectors) {
        if (!connectorIds.insert(connector.id).second) {
            result.errors.push_back(ModuleValidationError::DuplicateConnectorId);
        }
        if (validateConnector(connector) != ConnectorValidationError::None) {
            result.errors.push_back(ModuleValidationError::InvalidConnector);
        } else if (!isConnectorOnPerimeter(connector, module.localBounds)) {
            result.errors.push_back(ModuleValidationError::ConnectorOffPerimeter);
        }
        if (!module.localOutline.empty()) {
            const auto direction = directionFromNormal(connector.normal);
            bool onFace = false;
            constexpr float clearance = 0.30F;
            for (std::size_t i = 0U; i < module.localOutline.size(); ++i) {
                const auto a = module.localOutline[i];
                const auto b = module.localOutline[(i + 1U) % module.localOutline.size()];
                const float halfWidth = connector.width * 0.5F + clearance;
                if (direction && (*direction == Direction::North
                        || *direction == Direction::South)) {
                    onFace |= wkc::core::nearlyEqual(a.y, connector.position.z)
                        && wkc::core::nearlyEqual(b.y, connector.position.z)
                        && connector.position.x - halfWidth >= std::min(a.x, b.x)
                        && connector.position.x + halfWidth <= std::max(a.x, b.x);
                } else if (direction) {
                    onFace |= wkc::core::nearlyEqual(a.x, connector.position.x)
                        && wkc::core::nearlyEqual(b.x, connector.position.x)
                        && connector.position.z - halfWidth >= std::min(a.y, b.y)
                        && connector.position.z + halfWidth <= std::max(a.y, b.y);
                }
            }
            if (!onFace) result.errors.push_back(ModuleValidationError::ConnectorOffPerimeter);
        }
    }

    const auto validateZones = [&result, &module](const std::vector<SpawnZone>& zones) {
        for (const SpawnZone& zone : zones) {
            if (!isZoneInside(zone, module.localBounds)) {
                result.errors.push_back(ModuleValidationError::ZoneOutsideBounds);
            }
            if (!module.localOutline.empty()) {
                for (const float x : {zone.bounds.minimum.x, zone.bounds.maximum.x}) {
                    for (const float z : {zone.bounds.minimum.z, zone.bounds.maximum.z}) {
                        if (!footprintContainsDisk(module.localOutline, {x, 0.0F, z})) {
                            result.errors.push_back(ModuleValidationError::ZoneOutsideBounds);
                        }
                    }
                }
            }
            for (const ConnectorDefinition& connector : module.connectors) {
                if (validateConnector(connector) == ConnectorValidationError::None
                    && overlaps(zone.bounds, connectorClearance(connector))) {
                    result.errors.push_back(ModuleValidationError::ZoneOverlapsConnectorClearance);
                    break;
                }
            }
        }
    };
    validateZones(module.enemyZones);
    validateZones(module.lootZones);
    validateZones(module.propZones);
    if (!module.localOutline.empty()) {
        if (module.localOutline.size() < 3U) {
            result.errors.push_back(ModuleValidationError::InvalidFootprint);
        }
        for (const auto point : module.localOutline) {
            if (point.x < module.localBounds.minimum.x - 1.0e-4F
                || point.x > module.localBounds.maximum.x + 1.0e-4F
                || point.y < module.localBounds.minimum.z - 1.0e-4F
                || point.y > module.localBounds.maximum.z + 1.0e-4F) {
                result.errors.push_back(ModuleValidationError::InvalidFootprint);
                break;
            }
        }
    }
    return result;
}

std::optional<RoomInstance> instantiateRoom(
    const RoomModule& module,
    const RoomId roomId,
    const RoomRole role,
    const wkc::core::Seed seed,
    const wkc::core::Vec3 worldOrigin,
    const wkc::core::QuarterTurn rotation)
{
    if (roomId == 0 || !validateRoomModule(module).isValid() || !module.supportsRole(role)) {
        return std::nullopt;
    }

    RoomInstance room;
    room.id = roomId;
    room.role = role;
    room.theme = module.theme;
    room.sizeClass = module.sizeClass;
    room.seed = seed;
    room.moduleId = module.id;
    room.transform.position = worldOrigin;
    room.transform.rotationDegrees.y = static_cast<float>(static_cast<std::uint8_t>(rotation))
        * 90.0F;
    room.rotation = rotation;
    room.worldBounds = transformBounds(module.localBounds, worldOrigin, rotation);
    room.walkableOutline.reserve(module.localOutline.size());
    for (const auto point : module.localOutline) {
        const auto rotated = wkc::core::rotateQuarterTurns(
            wkc::core::Vec3{point.x, 0.0F, point.y}, rotation);
        room.walkableOutline.push_back({
            worldOrigin.x + rotated.x, worldOrigin.z + rotated.z});
    }
    room.connectors.reserve(module.connectors.size());
    for (const ConnectorDefinition& definition : module.connectors) {
        room.connectors.push_back(instantiateConnector(definition, worldOrigin, rotation));
    }
    room.enemyZones.reserve(module.enemyZones.size());
    for (const SpawnZone& zone : module.enemyZones) {
        room.enemyZones.push_back({transformBounds(zone.bounds, worldOrigin, rotation)});
    }
    room.lootZones.reserve(module.lootZones.size());
    for (const SpawnZone& zone : module.lootZones) {
        room.lootZones.push_back({transformBounds(zone.bounds, worldOrigin, rotation)});
    }
    room.propZones.reserve(module.propZones.size());
    for (const SpawnZone& zone : module.propZones) {
        room.propZones.push_back({transformBounds(zone.bounds, worldOrigin, rotation)});
    }
    return room;
}

bool hasUnresolvedRequiredConnector(const RoomInstance& room) noexcept
{
    return std::any_of(
        room.connectors.begin(),
        room.connectors.end(),
        [](const ConnectorInstance& connector) {
            return connector.required && !connector.edge.has_value();
        });
}

RoomModule makeFixedRoomModule()
{
    RoomModule module = makeRectangularModule(
        1,
        "IndustrialFixedRoom_01",
        RoomTheme::Industrial,
        RoomSizeClass::Medium,
        {8, 4, 10},
        roomRoleBit(RoomRole::Start)
        | roomRoleBit(RoomRole::Normal)
        | roomRoleBit(RoomRole::Hub)
        | roomRoleBit(RoomRole::Arena)
        | roomRoleBit(RoomRole::DeadEnd));
    module.enemyZones = {{{{-2.0F, 0.0F, -1.0F}, {2.0F, 0.0F, 1.0F}}}};
    module.lootZones = {{{{-3.0F, 0.0F, -3.0F}, {-2.0F, 0.0F, -2.0F}}}};
    module.propZones = {{{{1.5F, 0.0F, -3.0F}, {3.0F, 0.0F, 1.5F}}}};
    return module;
}

std::vector<RoomModule> makeDefaultRoomModuleLibrary()
{
    const RoomRoleMask common = roomRoleBit(RoomRole::Normal)
        | roomRoleBit(RoomRole::DeadEnd);
    return {
        makeRectangularModule(101, "IndustrialCompact_01", RoomTheme::Industrial,
            RoomSizeClass::Compact, {7, 4, 9}, common),
        makeRectangularModule(102, "IndustrialMedium_01", RoomTheme::Industrial,
            RoomSizeClass::Medium, {8, 4, 10}, common),
        makeRectangularModule(103, "IndustrialStart_01", RoomTheme::Industrial,
            RoomSizeClass::Medium, {9, 4, 10}, roomRoleBit(RoomRole::Start)),
        makeCentralHubModule(),
        makeRectangularModule(105, "IndustrialHall_01", RoomTheme::Industrial,
            RoomSizeClass::Large, {16, 6, 20}, roomRoleBit(RoomRole::Normal)),

        makeRectangularModule(201, "StorageCompact_01", RoomTheme::Storage,
            RoomSizeClass::Compact, {7, 4, 9}, common),
        makeRectangularModule(202, "StorageWide_01", RoomTheme::Storage,
            RoomSizeClass::Medium, {10, 4, 9}, common),
        makeRectangularModule(203, "StorageSecret_01", RoomTheme::Storage,
            RoomSizeClass::Compact, {7, 4, 9}, roomRoleBit(RoomRole::Secret)),
        makeStorageHallModule(),

        makeRectangularModule(301, "OfficeCompact_01", RoomTheme::Office,
            RoomSizeClass::Compact, {7, 4, 9}, common),
        makeRectangularModule(302, "OfficeWide_01", RoomTheme::Office,
            RoomSizeClass::Medium, {10, 4, 9}, common),
        makeRectangularModule(303, "OfficeHall_01", RoomTheme::Office,
            RoomSizeClass::Large, {16, 6, 18}, roomRoleBit(RoomRole::Normal)),

        makeRectangularModule(401, "LaboratoryCompact_01", RoomTheme::Laboratory,
            RoomSizeClass::Compact, {7, 4, 9}, common),
        makeRectangularModule(402, "LaboratoryMedium_01", RoomTheme::Laboratory,
            RoomSizeClass::Medium, {9, 5, 10}, common),
        makeRectangularModule(403, "LaboratorySecret_01", RoomTheme::Laboratory,
            RoomSizeClass::Compact, {7, 4, 9}, roomRoleBit(RoomRole::Secret)),
        makeBossArenaModule(404, "LaboratoryBoss_01", RoomTheme::Laboratory),
        makeLaboratoryHallModule(),

        makeRectangularModule(501, "MaintenanceCompact_01", RoomTheme::Maintenance,
            RoomSizeClass::Compact, {7, 4, 9}, common),
        makeRectangularModule(502, "MaintenanceMedium_01", RoomTheme::Maintenance,
            RoomSizeClass::Medium, {8, 5, 11}, common),
        makeRectangularModule(503, "MaintenanceArena_01", RoomTheme::Maintenance,
            RoomSizeClass::Large, {12, 5, 11}, roomRoleBit(RoomRole::Arena)),
        makeCombatGalleryArenaModule(),
        makeBossArenaModule(504, "MaintenanceBoss_01", RoomTheme::Maintenance),
        makeRectangularModule(505, "MaintenanceHall_01", RoomTheme::Maintenance,
            RoomSizeClass::Large, {16, 6, 20}, roomRoleBit(RoomRole::Normal)),
        makeMaintenanceLModule(),

        makeRectangularModule(601, "SecurityCompact_01", RoomTheme::Security,
            RoomSizeClass::Compact, {7, 4, 9}, common),
        makeRectangularModule(602, "SecurityMedium_01", RoomTheme::Security,
            RoomSizeClass::Medium, {9, 4, 10}, common),
        makeRectangularModule(603, "SecurityArena_01", RoomTheme::Security,
            RoomSizeClass::Large, {13, 5, 12}, roomRoleBit(RoomRole::Arena)),
        makeBossArenaModule(604, "SecurityBoss_01", RoomTheme::Security),
        makeRectangularModule(605, "SecurityHall_01", RoomTheme::Security,
            RoomSizeClass::Large, {18, 6, 18},
            roomRoleBit(RoomRole::Normal) | roomRoleBit(RoomRole::Terminal)),
        makeSecurityTModule(),

        makeRectangularModule(701, "CafeteriaCompact_01", RoomTheme::Cafeteria,
            RoomSizeClass::Compact, {7, 4, 9}, common),
        makeRectangularModule(702, "CafeteriaWide_01", RoomTheme::Cafeteria,
            RoomSizeClass::Medium, {10, 4, 9}, common),
        makeRectangularModule(703, "CafeteriaHall_01", RoomTheme::Cafeteria,
            RoomSizeClass::Large, {18, 6, 18}, roomRoleBit(RoomRole::Normal)),
        makeCafeteriaLongModule(),
    };
}

} // namespace gdp::world
