#include "world/Connector.hpp"

#include <algorithm>
#include <cmath>

namespace gdp::world {

bool ConnectorInstance::isAvailable() const noexcept
{
    return !sealed && !edge.has_value();
}

ConnectorValidationError validateConnector(const ConnectorDefinition& connector) noexcept
{
    if (connector.id == 0) {
        return ConnectorValidationError::InvalidId;
    }
    if (connector.width <= 0.0F || connector.height <= 0.0F
        || !std::isfinite(connector.width) || !std::isfinite(connector.height)) {
        return ConnectorValidationError::InvalidDimensions;
    }
    if (!directionFromNormal(connector.normal)) {
        return ConnectorValidationError::InvalidNormal;
    }
    return ConnectorValidationError::None;
}

ConnectorInstance instantiateConnector(
    const ConnectorDefinition& definition,
    const gdp::core::Vec3 roomOrigin,
    const gdp::core::QuarterTurn rotation) noexcept
{
    return {
        definition.id,
        definition.type,
        roomOrigin + gdp::core::rotateQuarterTurns(definition.position, rotation),
        gdp::core::rotateQuarterTurns(definition.normal, rotation),
        definition.width,
        definition.height,
        definition.required,
        false,
        std::nullopt,
    };
}

bool areConnectorDefinitionsCompatible(
    const ConnectorDefinition& first,
    const ConnectorDefinition& second,
    const float dimensionTolerance) noexcept
{
    if (validateConnector(first) != ConnectorValidationError::None
        || validateConnector(second) != ConnectorValidationError::None) {
        return false;
    }
    const float tolerance = std::max(dimensionTolerance, 0.0F);
    return first.type == second.type
        && std::abs(first.width - second.width) <= tolerance
        && std::abs(first.height - second.height) <= tolerance;
}

bool canMate(
    const ConnectorInstance& first,
    const ConnectorInstance& second,
    const float maximumGap) noexcept
{
    if (!first.isAvailable() || !second.isAvailable() || first.type != second.type
        || std::abs(first.width - second.width) > 0.05F
        || std::abs(first.height - second.height) > 0.05F) {
        return false;
    }

    const gdp::core::Vec3 offset = first.worldPosition - second.worldPosition;
    const float gap = gdp::core::length(offset);
    return gap <= std::max(maximumGap, 0.0F)
        && gdp::core::dot(first.worldNormal, second.worldNormal) <= -0.999F;
}

bool assignEdge(ConnectorInstance& connector, const EdgeId edge) noexcept
{
    if (!connector.isAvailable() || edge == 0) {
        return false;
    }
    connector.edge = edge;
    return true;
}

bool sealConnector(ConnectorInstance& connector) noexcept
{
    if (!connector.isAvailable() || connector.required) {
        return false;
    }
    connector.sealed = true;
    return true;
}

std::optional<Direction> directionFromNormal(
    const gdp::core::Vec3 normal,
    const float epsilon) noexcept
{
    const float tolerance = std::max(epsilon, 0.0F);
    if (gdp::core::nearlyEqual(normal, {0.0F, 0.0F, 1.0F}, tolerance)) {
        return Direction::North;
    }
    if (gdp::core::nearlyEqual(normal, {1.0F, 0.0F, 0.0F}, tolerance)) {
        return Direction::East;
    }
    if (gdp::core::nearlyEqual(normal, {0.0F, 0.0F, -1.0F}, tolerance)) {
        return Direction::South;
    }
    if (gdp::core::nearlyEqual(normal, {-1.0F, 0.0F, 0.0F}, tolerance)) {
        return Direction::West;
    }
    return std::nullopt;
}

} // namespace gdp::world
