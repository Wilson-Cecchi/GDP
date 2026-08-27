#include "world/World.hpp"

#include "core/Hash.hpp"

#include <bit>

namespace gdp::world {
namespace {

std::uint64_t addFloat(const std::uint64_t hash, const float value) noexcept
{
    return gdp::core::hashCombine(hash, std::bit_cast<std::uint32_t>(value));
}

std::uint64_t addVec3(std::uint64_t hash, const gdp::core::Vec3 value) noexcept
{
    hash = addFloat(hash, value.x);
    hash = addFloat(hash, value.y);
    return addFloat(hash, value.z);
}

} // namespace

std::uint64_t worldFingerprint(const World& world) noexcept
{
    std::uint64_t hash = gdp::core::hashCombine(world.masterSeed, world.generationSeed);
    hash = gdp::core::hashCombine(hash, world.generationVersion.major);
    hash = gdp::core::hashCombine(hash, world.generationVersion.minor);
    hash = gdp::core::hashCombine(hash, world.acceptedAttempt);
    for (const RoomNode& node : world.graph.nodes) {
        hash = gdp::core::hashCombine(hash, node.id);
        hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(node.role));
        hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(node.theme));
        hash = gdp::core::hashCombine(hash, node.depth);
    }
    for (const MapEdge& edge : world.graph.edges) {
        hash = gdp::core::hashCombine(hash, edge.id);
        hash = gdp::core::hashCombine(hash, edge.first);
        hash = gdp::core::hashCombine(hash, edge.second);
        hash = gdp::core::hashCombine(hash, edge.mainPath ? 1U : 0U);
    }
    for (const RoomInstance& room : world.rooms) {
        hash = gdp::core::hashCombine(hash, room.id);
        hash = gdp::core::hashCombine(hash, room.moduleId);
        hash = gdp::core::hashCombine(hash, room.seed);
        hash = addVec3(hash, room.transform.position);
        hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(room.rotation));
        for (const ConnectorInstance& connector : room.connectors) {
            hash = gdp::core::hashCombine(hash, connector.definitionId);
            hash = addVec3(hash, connector.worldPosition);
            hash = addVec3(hash, connector.worldNormal);
            hash = gdp::core::hashCombine(hash, connector.edge.value_or(0));
            hash = gdp::core::hashCombine(hash, connector.sealed ? 1U : 0U);
        }
    }
    for (const CorridorPath& corridor : world.corridors) {
        hash = gdp::core::hashCombine(hash, corridor.edge);
        hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(corridor.shape));
        for (const gdp::core::Vec3 point : corridor.points) {
            hash = addVec3(hash, point);
        }
    }
    return hash;
}

} // namespace gdp::world
