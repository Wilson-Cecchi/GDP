#include "world/RoomModule.hpp"

#include <algorithm>
#include <array>
#include <unordered_set>
#include <utility>

namespace gdp::world {
namespace {

bool isZoneInside(const SpawnZone& zone, const gdp::core::Aabb bounds) noexcept
{
    return zone.bounds.isValid() && bounds.contains(zone.bounds.minimum)
        && bounds.contains(zone.bounds.maximum);
}

bool isConnectorOnPerimeter(
    const ConnectorDefinition& connector,
    const gdp::core::Aabb bounds) noexcept
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
        return gdp::core::nearlyEqual(connector.position.z, bounds.maximum.z);
    case Direction::East:
        return gdp::core::nearlyEqual(connector.position.x, bounds.maximum.x);
    case Direction::South:
        return gdp::core::nearlyEqual(connector.position.z, bounds.minimum.z);
    case Direction::West:
        return gdp::core::nearlyEqual(connector.position.x, bounds.minimum.x);
    }
    return false;
}

gdp::core::Aabb connectorClearance(const ConnectorDefinition& connector) noexcept
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

bool overlaps(const gdp::core::Aabb first, const gdp::core::Aabb second) noexcept
{
    return first.minimum.x <= second.maximum.x && first.maximum.x >= second.minimum.x
        && first.minimum.y <= second.maximum.y && first.maximum.y >= second.minimum.y
        && first.minimum.z <= second.maximum.z && first.maximum.z >= second.minimum.z;
}

gdp::core::Aabb transformBounds(
    const gdp::core::Aabb localBounds,
    const gdp::core::Vec3 origin,
    const gdp::core::QuarterTurn rotation) noexcept
{
    const std::array corners{
        gdp::core::Vec3{localBounds.minimum.x, localBounds.minimum.y, localBounds.minimum.z},
        gdp::core::Vec3{localBounds.maximum.x, localBounds.minimum.y, localBounds.minimum.z},
        gdp::core::Vec3{localBounds.minimum.x, localBounds.maximum.y, localBounds.minimum.z},
        gdp::core::Vec3{localBounds.maximum.x, localBounds.maximum.y, localBounds.minimum.z},
        gdp::core::Vec3{localBounds.minimum.x, localBounds.minimum.y, localBounds.maximum.z},
        gdp::core::Vec3{localBounds.maximum.x, localBounds.minimum.y, localBounds.maximum.z},
        gdp::core::Vec3{localBounds.minimum.x, localBounds.maximum.y, localBounds.maximum.z},
        gdp::core::Vec3{localBounds.maximum.x, localBounds.maximum.y, localBounds.maximum.z},
    };

    gdp::core::Vec3 minimum = origin + gdp::core::rotateQuarterTurns(corners[0], rotation);
    gdp::core::Vec3 maximum = minimum;
    for (const gdp::core::Vec3 corner : corners) {
        const gdp::core::Vec3 transformed = origin
            + gdp::core::rotateQuarterTurns(corner, rotation);
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
    }

    const auto validateZones = [&result, &module](const std::vector<SpawnZone>& zones) {
        for (const SpawnZone& zone : zones) {
            if (!isZoneInside(zone, module.localBounds)) {
                result.errors.push_back(ModuleValidationError::ZoneOutsideBounds);
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
    return result;
}

std::optional<RoomInstance> instantiateRoom(
    const RoomModule& module,
    const RoomId roomId,
    const RoomRole role,
    const gdp::core::Seed seed,
    const gdp::core::Vec3 worldOrigin,
    const gdp::core::QuarterTurn rotation)
{
    if (roomId == 0 || !validateRoomModule(module).isValid() || !module.supportsRole(role)) {
        return std::nullopt;
    }

    RoomInstance room;
    room.id = roomId;
    room.role = role;
    room.theme = module.theme;
    room.seed = seed;
    room.moduleId = module.id;
    room.transform.position = worldOrigin;
    room.transform.rotationDegrees.y = static_cast<float>(static_cast<std::uint8_t>(rotation))
        * 90.0F;
    room.rotation = rotation;
    room.worldBounds = transformBounds(module.localBounds, worldOrigin, rotation);
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
    RoomModule module;
    module.id = 1;
    module.name = "IndustrialFixedRoom_01";
    module.theme = RoomTheme::Industrial;
    module.footprint = {8, 4, 10};
    module.localBounds = {{-3.85F, 0.0F, -4.85F}, {3.85F, 4.0F, 4.85F}};
    module.roleCapabilities = roomRoleBit(RoomRole::Start)
        | roomRoleBit(RoomRole::Normal)
        | roomRoleBit(RoomRole::Hub)
        | roomRoleBit(RoomRole::Arena)
        | roomRoleBit(RoomRole::DeadEnd);
    module.connectors = {
        {1, ConnectorType::Door, {0.0F, 0.0F, 4.85F}, {0.0F, 0.0F, 1.0F}, 1.2F, 2.2F, false},
        {2, ConnectorType::Door, {0.0F, 0.0F, -4.85F}, {0.0F, 0.0F, -1.0F}, 1.2F, 2.2F, false},
        {3, ConnectorType::Door, {3.85F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, 1.2F, 2.2F, false},
        {4, ConnectorType::Door, {-3.85F, 0.0F, 0.0F}, {-1.0F, 0.0F, 0.0F}, 1.2F, 2.2F, false},
    };
    module.enemyZones = {{{{-2.0F, 0.0F, -1.0F}, {2.0F, 0.0F, 1.0F}}}};
    module.lootZones = {{{{-3.0F, 0.0F, -3.0F}, {-2.0F, 0.0F, -2.0F}}}};
    module.propZones = {{{{1.5F, 0.0F, -3.0F}, {3.0F, 0.0F, 1.5F}}}};
    return module;
}

std::vector<RoomModule> makeDefaultRoomModuleLibrary()
{
    const RoomModule base = makeFixedRoomModule();
    const auto makeVariant = [&base](
                                 const RoomModuleId id,
                                 std::string name,
                                 const RoomTheme theme,
                                 const RoomRoleMask roles) {
        RoomModule module = base;
        module.id = id;
        module.name = std::move(name);
        module.theme = theme;
        module.roleCapabilities = roles;
        return module;
    };

    const RoomRoleMask common = roomRoleBit(RoomRole::Normal)
        | roomRoleBit(RoomRole::DeadEnd);
    return {
        makeVariant(1, "Industrial_01", RoomTheme::Industrial,
            common | roomRoleBit(RoomRole::Start) | roomRoleBit(RoomRole::Hub)
                | roomRoleBit(RoomRole::Arena)),
        makeVariant(2, "Storage_01", RoomTheme::Storage,
            common | roomRoleBit(RoomRole::Secret)),
        makeVariant(3, "Office_01", RoomTheme::Office,
            common | roomRoleBit(RoomRole::Secret)),
        makeVariant(4, "Laboratory_01", RoomTheme::Laboratory,
            common | roomRoleBit(RoomRole::Secret) | roomRoleBit(RoomRole::Boss)),
        makeVariant(5, "Maintenance_01", RoomTheme::Maintenance,
            common | roomRoleBit(RoomRole::Hub) | roomRoleBit(RoomRole::Arena)
                | roomRoleBit(RoomRole::Boss)),
        makeVariant(6, "Security_01", RoomTheme::Security,
            common | roomRoleBit(RoomRole::Arena) | roomRoleBit(RoomRole::Boss)),
        makeVariant(7, "Cafeteria_01", RoomTheme::Cafeteria, common),
    };
}

} // namespace gdp::world
