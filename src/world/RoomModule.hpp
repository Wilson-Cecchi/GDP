#pragma once

#include "core/Math.hpp"
#include "core/Seed.hpp"
#include "entities/EntityManager.hpp"
#include "world/Connector.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace gdp::world {

using RoomModuleId = std::uint32_t;
using RoomId = std::uint32_t;

enum class RoomRole : std::uint8_t {
    Start,
    Normal,
    Hub,
    Arena,
    DeadEnd,
    Secret,
    Boss,
    Count,
};

enum class RoomTheme : std::uint8_t {
    Industrial,
    Storage,
    Office,
    Laboratory,
    Maintenance,
    Security,
    Cafeteria,
};

using RoomRoleMask = std::uint32_t;

[[nodiscard]] constexpr RoomRoleMask roomRoleBit(const RoomRole role) noexcept
{
    return RoomRoleMask{1} << static_cast<std::uint8_t>(role);
}

struct GridSize {
    std::int32_t x{1};
    std::int32_t y{1};
    std::int32_t z{1};
};

struct SpawnZone {
    gdp::core::Aabb bounds{};
};

struct RoomModule {
    RoomModuleId id{0};
    std::string name;
    RoomTheme theme{RoomTheme::Industrial};
    GridSize footprint{};
    gdp::core::Aabb localBounds{};
    RoomRoleMask roleCapabilities{0};
    std::vector<ConnectorDefinition> connectors;
    std::vector<SpawnZone> enemyZones;
    std::vector<SpawnZone> lootZones;
    std::vector<SpawnZone> propZones;

    [[nodiscard]] bool supportsRole(RoomRole role) const noexcept;
};

enum class ModuleValidationError : std::uint8_t {
    InvalidId,
    EmptyName,
    InvalidFootprint,
    InvalidBounds,
    NoRoleCapability,
    NoConnectors,
    DuplicateConnectorId,
    InvalidConnector,
    ConnectorOffPerimeter,
    ZoneOutsideBounds,
    ZoneOverlapsConnectorClearance,
};

struct ModuleValidationResult {
    std::vector<ModuleValidationError> errors;

    [[nodiscard]] bool isValid() const noexcept;
};

struct RoomInstance {
    RoomId id{0};
    RoomRole role{RoomRole::Normal};
    RoomTheme theme{RoomTheme::Industrial};
    gdp::core::Seed seed{0};
    RoomModuleId moduleId{0};
    gdp::core::Transform transform{};
    gdp::core::QuarterTurn rotation{gdp::core::QuarterTurn::Degrees0};
    gdp::core::Aabb worldBounds{};
    std::vector<ConnectorInstance> connectors;
    std::vector<SpawnZone> enemyZones;
    std::vector<SpawnZone> lootZones;
    std::vector<SpawnZone> propZones;
    std::vector<gdp::entities::EntityId> entities;
};

[[nodiscard]] ModuleValidationResult validateRoomModule(const RoomModule& module);
[[nodiscard]] std::optional<RoomInstance> instantiateRoom(
    const RoomModule& module,
    RoomId roomId,
    RoomRole role,
    gdp::core::Seed seed,
    gdp::core::Vec3 worldOrigin,
    gdp::core::QuarterTurn rotation);
[[nodiscard]] bool hasUnresolvedRequiredConnector(const RoomInstance& room) noexcept;
[[nodiscard]] RoomModule makeFixedRoomModule();
[[nodiscard]] std::vector<RoomModule> makeDefaultRoomModuleLibrary();

} // namespace gdp::world
