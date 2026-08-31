#pragma once

#include "core/Math.hpp"

#include <cstdint>
#include <optional>

namespace gdp::world {

using ConnectorId = std::uint32_t;
using EdgeId = std::uint32_t;

enum class ConnectorType : std::uint8_t {
    Door,
    Large,
    Secret,
    Service,
    Elevator,
};

enum class Direction : std::uint8_t {
    North,
    East,
    South,
    West,
};

enum class DoorState : std::uint8_t {
    Closed,
    Opening,
    Open,
    Closing,
    Locked,
    Disabled,
};

struct ConnectorDefinition {
    ConnectorId id{0};
    ConnectorType type{ConnectorType::Door};
    wkc::core::Vec3 position{};
    wkc::core::Vec3 normal{0.0F, 0.0F, 1.0F};
    float width{1.2F};
    float height{2.2F};
    bool required{false};
};

struct ConnectorInstance {
    ConnectorId definitionId{0};
    ConnectorType type{ConnectorType::Door};
    wkc::core::Vec3 worldPosition{};
    wkc::core::Vec3 worldNormal{0.0F, 0.0F, 1.0F};
    float width{1.2F};
    float height{2.2F};
    bool required{false};
    bool sealed{false};
    std::optional<EdgeId> edge;

    [[nodiscard]] bool isAvailable() const noexcept;
};

enum class ConnectorValidationError : std::uint8_t {
    None,
    InvalidId,
    InvalidDimensions,
    InvalidNormal,
};

[[nodiscard]] ConnectorValidationError validateConnector(
    const ConnectorDefinition& connector) noexcept;
[[nodiscard]] ConnectorInstance instantiateConnector(
    const ConnectorDefinition& definition,
    wkc::core::Vec3 roomOrigin,
    wkc::core::QuarterTurn rotation) noexcept;
[[nodiscard]] bool areConnectorDefinitionsCompatible(
    const ConnectorDefinition& first,
    const ConnectorDefinition& second,
    float dimensionTolerance = 0.05F) noexcept;
[[nodiscard]] bool canMate(
    const ConnectorInstance& first,
    const ConnectorInstance& second,
    float maximumGap = 0.05F) noexcept;
[[nodiscard]] bool assignEdge(ConnectorInstance& connector, EdgeId edge) noexcept;
[[nodiscard]] bool sealConnector(ConnectorInstance& connector) noexcept;
[[nodiscard]] std::optional<Direction> directionFromNormal(
    wkc::core::Vec3 normal,
    float epsilon = 1.0e-4F) noexcept;

} // namespace gdp::world
