#include "world/WorldValidation.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace gdp::world {
namespace {

bool overlapsWithGap(
    const gdp::core::Aabb first,
    const gdp::core::Aabb second,
    const float gap) noexcept
{
    return first.minimum.x - gap < second.maximum.x
        && first.maximum.x + gap > second.minimum.x
        && first.minimum.z - gap < second.maximum.z
        && first.maximum.z + gap > second.minimum.z;
}

bool rangesOverlap(
    const float firstMinimum,
    const float firstMaximum,
    const float secondMinimum,
    const float secondMaximum) noexcept
{
    return firstMinimum <= secondMaximum && firstMaximum >= secondMinimum;
}

bool segmentsIntersect(
    const gdp::core::Vec3 firstStart,
    const gdp::core::Vec3 firstEnd,
    const gdp::core::Vec3 secondStart,
    const gdp::core::Vec3 secondEnd) noexcept
{
    const bool firstVertical = gdp::core::nearlyEqual(firstStart.x, firstEnd.x);
    const bool secondVertical = gdp::core::nearlyEqual(secondStart.x, secondEnd.x);
    const float firstMinX = std::min(firstStart.x, firstEnd.x);
    const float firstMaxX = std::max(firstStart.x, firstEnd.x);
    const float firstMinZ = std::min(firstStart.z, firstEnd.z);
    const float firstMaxZ = std::max(firstStart.z, firstEnd.z);
    const float secondMinX = std::min(secondStart.x, secondEnd.x);
    const float secondMaxX = std::max(secondStart.x, secondEnd.x);
    const float secondMinZ = std::min(secondStart.z, secondEnd.z);
    const float secondMaxZ = std::max(secondStart.z, secondEnd.z);

    if (firstVertical == secondVertical) {
        if (firstVertical && !gdp::core::nearlyEqual(firstStart.x, secondStart.x)) {
            return false;
        }
        if (!firstVertical && !gdp::core::nearlyEqual(firstStart.z, secondStart.z)) {
            return false;
        }
        return rangesOverlap(firstMinX, firstMaxX, secondMinX, secondMaxX)
            && rangesOverlap(firstMinZ, firstMaxZ, secondMinZ, secondMaxZ);
    }

    const auto verticalStart = firstVertical ? firstStart : secondStart;
    const float verticalMinZ = firstVertical ? firstMinZ : secondMinZ;
    const float verticalMaxZ = firstVertical ? firstMaxZ : secondMaxZ;
    const auto horizontalStart = firstVertical ? secondStart : firstStart;
    const float horizontalMinX = firstVertical ? secondMinX : firstMinX;
    const float horizontalMaxX = firstVertical ? secondMaxX : firstMaxX;
    return verticalStart.x >= horizontalMinX && verticalStart.x <= horizontalMaxX
        && horizontalStart.z >= verticalMinZ && horizontalStart.z <= verticalMaxZ;
}

} // namespace

bool WorldValidationResult::isValid() const noexcept
{
    return failures.empty();
}

GenerationFailure WorldValidationResult::primaryFailure() const noexcept
{
    return failures.empty() ? GenerationFailure::None : failures.front();
}

WorldValidationResult validateWorld(const World& world)
{
    WorldValidationResult result;
    if (!validateMapGraph(world.graph).isValid()) {
        result.failures.push_back(GenerationFailure::GraphInvalid);
    }
    if (world.rooms.size() != world.graph.nodes.size()) {
        result.failures.push_back(GenerationFailure::ModuleUnavailable);
    }

    for (std::size_t first = 0; first < world.rooms.size(); ++first) {
        if (world.rooms[first].id == 0 || world.rooms[first].moduleId == 0
            || !world.rooms[first].worldBounds.isValid()) {
            result.failures.push_back(GenerationFailure::ModuleUnavailable);
        }
        if (hasUnresolvedRequiredConnector(world.rooms[first])) {
            result.failures.push_back(GenerationFailure::ConnectorUnresolved);
        }
        for (std::size_t second = first + 1U; second < world.rooms.size(); ++second) {
            if (overlapsWithGap(
                    world.rooms[first].worldBounds,
                    world.rooms[second].worldBounds,
                    0.5F)) {
                result.failures.push_back(GenerationFailure::RoomOverlap);
            }
        }
    }

    std::unordered_map<EdgeId, std::size_t> connectorUses;
    for (const RoomInstance& room : world.rooms) {
        for (const ConnectorInstance& connector : room.connectors) {
            if (connector.edge) {
                ++connectorUses[*connector.edge];
            }
        }
    }
    std::unordered_set<EdgeId> corridorEdges;
    for (const CorridorPath& corridor : world.corridors) {
        if (!corridorEdges.insert(corridor.edge).second
            || !validateCorridorPath(corridor, world.rooms)) {
            result.failures.push_back(GenerationFailure::CorridorInvalid);
        }
    }
    for (std::size_t first = 0; first < world.corridors.size(); ++first) {
        for (std::size_t second = first + 1U; second < world.corridors.size(); ++second) {
            bool intersects = false;
            for (std::size_t firstSegment = 0;
                 firstSegment + 1U < world.corridors[first].points.size() && !intersects;
                 ++firstSegment) {
                for (std::size_t secondSegment = 0;
                     secondSegment + 1U < world.corridors[second].points.size();
                     ++secondSegment) {
                    if (segmentsIntersect(
                            world.corridors[first].points[firstSegment],
                            world.corridors[first].points[firstSegment + 1U],
                            world.corridors[second].points[secondSegment],
                            world.corridors[second].points[secondSegment + 1U])) {
                        intersects = true;
                        break;
                    }
                }
            }
            if (intersects) {
                result.failures.push_back(GenerationFailure::CorridorInvalid);
            }
        }
    }
    for (const MapEdge& edge : world.graph.edges) {
        if (connectorUses[edge.id] != 2U) {
            result.failures.push_back(GenerationFailure::ConnectorUnresolved);
        }
        if (!corridorEdges.contains(edge.id)) {
            result.failures.push_back(GenerationFailure::CorridorInvalid);
        }
    }

    std::sort(result.failures.begin(), result.failures.end());
    result.failures.erase(
        std::unique(result.failures.begin(), result.failures.end()),
        result.failures.end());
    return result;
}

} // namespace gdp::world
