#include "world/World.hpp"

#include "core/Hash.hpp"

#include <bit>

namespace gdp::world {
namespace {

std::uint64_t addFloat(const std::uint64_t hash, const float value) noexcept
{
    return wkc::core::hashCombine(hash, std::bit_cast<std::uint32_t>(value));
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
    std::uint64_t hash = wkc::core::hashCombine(world.masterSeed, world.generationSeed);
    hash = wkc::core::hashCombine(hash, world.generationVersion.major);
    hash = wkc::core::hashCombine(hash, world.generationVersion.minor);
    hash = wkc::core::hashCombine(hash, world.acceptedAttempt);
    for (const RoomNode& node : world.graph.nodes) {
        hash = wkc::core::hashCombine(hash, node.id);
        hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(node.role));
        hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(node.theme));
        hash = wkc::core::hashCombine(hash, node.depth);
    }
    for (const MapEdge& edge : world.graph.edges) {
        hash = wkc::core::hashCombine(hash, edge.id);
        hash = wkc::core::hashCombine(hash, edge.first);
        hash = wkc::core::hashCombine(hash, edge.second);
        hash = wkc::core::hashCombine(hash, edge.mainPath ? 1U : 0U);
    }
    for (const RoomInstance& room : world.rooms) {
        hash = wkc::core::hashCombine(hash, room.id);
        hash = wkc::core::hashCombine(hash, room.moduleId);
        hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(room.sizeClass));
        hash = wkc::core::hashCombine(hash, room.seed);
        hash = addVec3(hash, room.transform.position);
        hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(room.rotation));
        hash = addVec3(hash, room.worldBounds.minimum);
        hash = addVec3(hash, room.worldBounds.maximum);
        for (const ConnectorInstance& connector : room.connectors) {
            hash = wkc::core::hashCombine(hash, connector.definitionId);
            hash = addVec3(hash, connector.worldPosition);
            hash = addVec3(hash, connector.worldNormal);
            hash = wkc::core::hashCombine(hash, connector.edge.value_or(0));
            hash = wkc::core::hashCombine(hash, connector.sealed ? 1U : 0U);
        }
    }
    for (const CorridorPath& corridor : world.corridors) {
        hash = wkc::core::hashCombine(hash, corridor.edge);
        hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(corridor.shape));
        for (const gdp::core::Vec3 point : corridor.points) {
            hash = addVec3(hash, point);
        }
    }
    return hash;
}

} // namespace gdp::world
