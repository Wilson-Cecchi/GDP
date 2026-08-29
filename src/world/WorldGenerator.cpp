#include "world/WorldGenerator.hpp"

#include "core/Random.hpp"
#include "world/Corridor.hpp"
#include "world/MapGraph.hpp"

#include <algorithm>
#include <limits>
#include <vector>

namespace gdp::world {
namespace {

struct CandidateOutcome {
    std::optional<World> world;
    GenerationFailure failure{GenerationFailure::None};
};

const RoomModule* selectModule(
    const std::span<const RoomModule> library,
    const RoomNode& node,
    gdp::core::RandomStream& random)
{
    std::vector<const RoomModule*> candidates;
    for (const RoomModule& module : library) {
        if (module.theme == node.theme && module.supportsRole(node.role)
            && validateRoomModule(module).isValid()) {
            candidates.push_back(&module);
        }
    }
    if (candidates.empty()) {
        return nullptr;
    }
    return candidates[static_cast<std::size_t>(
        random.rangeInt(0, static_cast<int>(candidates.size() - 1U)))];
}

const RoomInstance* findPlacedRoom(
    const std::span<const RoomInstance> rooms,
    const RoomNodeId id) noexcept
{
    const auto room = std::find_if(
        rooms.begin(), rooms.end(),
        [id](const RoomInstance& candidate) { return candidate.id == id; });
    return room == rooms.end() ? nullptr : &*room;
}

gdp::core::Aabb translatedBounds(
    const gdp::core::Aabb bounds,
    const gdp::core::Vec3 translation) noexcept
{
    return {bounds.minimum + translation, bounds.maximum + translation};
}

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

gdp::core::Vec3 placementForNode(
    const MapGraph& graph,
    const RoomNodeId node,
    const float spacing,
    const RoomInstance& prototype,
    const std::span<const RoomInstance> placedRooms)
{
    // Preserve the former default feel as a 2.5-unit doorway-to-doorway gap,
    // while allowing callers to request roomier maps through roomSpacing.
    const float corridorClearance = std::max(2.5F, spacing - 9.5F);
    const auto main = std::find(graph.mainPath.begin(), graph.mainPath.end(), node);
    if (main != graph.mainPath.end()) {
        if (main == graph.mainPath.begin()) {
            return {};
        }
        const RoomNodeId previousId = *(main - 1);
        const RoomInstance* previous = findPlacedRoom(placedRooms, previousId);
        if (previous == nullptr) {
            return {};
        }
        return {
            previous->transform.position.x,
            0.0F,
            previous->worldBounds.minimum.z - corridorClearance
                - prototype.worldBounds.maximum.z,
        };
    }

    std::vector<const RoomInstance*> neighbors;
    for (const MapEdge& edge : graph.edges) {
        const RoomNodeId other = edge.first == node ? edge.second
            : edge.second == node ? edge.first : 0U;
        if (other != 0U) {
            if (const RoomInstance* neighbor = findPlacedRoom(placedRooms, other)) {
                neighbors.push_back(neighbor);
            }
        }
    }
    if (neighbors.empty()) {
        return {};
    }

    std::size_t branchOrdinal = 0;
    for (const RoomNode& candidate : graph.nodes) {
        if (candidate.id == node) {
            break;
        }
        if (std::find(graph.mainPath.begin(), graph.mainPath.end(), candidate.id)
            == graph.mainPath.end()) {
            ++branchOrdinal;
        }
    }
    const float side = (branchOrdinal % 2U == 0U) ? -1.0F : 1.0F;
    float averageZ = 0.0F;
    float sideBoundary = side < 0.0F
        ? std::numeric_limits<float>::max()
        : std::numeric_limits<float>::lowest();
    for (const RoomInstance* neighbor : neighbors) {
        averageZ += neighbor->transform.position.z;
        sideBoundary = side < 0.0F
            ? std::min(sideBoundary, neighbor->worldBounds.minimum.x)
            : std::max(sideBoundary, neighbor->worldBounds.maximum.x);
    }
    averageZ /= static_cast<float>(neighbors.size());

    gdp::core::Vec3 position{
        side < 0.0F
            ? sideBoundary - corridorClearance - prototype.worldBounds.maximum.x
            : sideBoundary + corridorClearance - prototype.worldBounds.minimum.x,
        0.0F,
        averageZ,
    };
    for (int adjustment = 0; adjustment < 64; ++adjustment) {
        const gdp::core::Aabb candidateBounds
            = translatedBounds(prototype.worldBounds, position);
        const bool overlaps = std::any_of(
            placedRooms.begin(), placedRooms.end(),
            [candidateBounds](const RoomInstance& room) {
                return overlapsWithGap(candidateBounds, room.worldBounds, 0.5F);
            });
        if (!overlaps) {
            break;
        }
        position.x += side;
    }
    return position;
}

ConnectorInstance* chooseConnector(RoomInstance& room, const gdp::core::Vec3 target)
{
    const gdp::core::Vec3 direction = gdp::core::normalized(target - room.transform.position);
    ConnectorInstance* best = nullptr;
    float bestAlignment = -2.0F;
    for (ConnectorInstance& connector : room.connectors) {
        if (!connector.isAvailable()) {
            continue;
        }
        const float alignment = gdp::core::dot(connector.worldNormal, direction);
        if (alignment > bestAlignment) {
            bestAlignment = alignment;
            best = &connector;
        }
    }
    return best;
}

CandidateOutcome generateCandidate(
    const gdp::core::Seed masterSeed,
    const gdp::core::Seed attemptSeed,
    const std::uint32_t attempt,
    const WorldGenerationConfig config,
    const std::span<const RoomModule> library)
{
    World candidate;
    candidate.masterSeed = masterSeed;
    candidate.generationSeed = attemptSeed;
    candidate.generationVersion = gdp::core::currentGenerationVersion;
    candidate.acceptedAttempt = attempt;
    candidate.graph = generateMapGraph(
        gdp::core::deriveSeed(attemptSeed, gdp::core::SeedDomain::Graph, 0),
        {config.roomCount, config.loopBudget});
    if (!validateMapGraph(candidate.graph).isValid()) {
        return {std::nullopt, GenerationFailure::GraphInvalid};
    }

    candidate.rooms.reserve(candidate.graph.nodes.size());
    for (const RoomNode& node : candidate.graph.nodes) {
        const gdp::core::Seed roomSeed = gdp::core::deriveSeed(
            attemptSeed, gdp::core::SeedDomain::Room, node.id);
        gdp::core::RandomStream random{gdp::core::deriveSeed(
            roomSeed, gdp::core::SeedDomain::RoomModule, node.id)};
        const RoomModule* module = selectModule(library, node, random);
        if (module == nullptr) {
            return {std::nullopt, GenerationFailure::ModuleUnavailable};
        }
        const auto rotation = static_cast<gdp::core::QuarterTurn>(random.rangeInt(0, 3));
        const auto prototype = instantiateRoom(
            *module, node.id, node.role, roomSeed, {}, rotation);
        if (!prototype) {
            return {std::nullopt, GenerationFailure::ModuleUnavailable};
        }
        const gdp::core::Vec3 position = placementForNode(
            candidate.graph, node.id, config.roomSpacing, *prototype, candidate.rooms);
        auto room = instantiateRoom(*module, node.id, node.role, roomSeed, position, rotation);
        if (!room) {
            return {std::nullopt, GenerationFailure::ModuleUnavailable};
        }
        candidate.rooms.push_back(std::move(*room));
    }

    const auto findRoom = [&candidate](const RoomNodeId id) -> RoomInstance* {
        const auto room = std::find_if(
            candidate.rooms.begin(), candidate.rooms.end(),
            [id](const RoomInstance& value) { return value.id == id; });
        return room == candidate.rooms.end() ? nullptr : &*room;
    };
    for (const MapEdge& edge : candidate.graph.edges) {
        RoomInstance* firstRoom = findRoom(edge.first);
        RoomInstance* secondRoom = findRoom(edge.second);
        if (firstRoom == nullptr || secondRoom == nullptr) {
            return {std::nullopt, GenerationFailure::ConnectorUnresolved};
        }
        ConnectorInstance* first = chooseConnector(*firstRoom, secondRoom->transform.position);
        ConnectorInstance* second = chooseConnector(*secondRoom, firstRoom->transform.position);
        if (first == nullptr || second == nullptr || first->type != second->type
            || !assignEdge(*first, edge.id) || !assignEdge(*second, edge.id)) {
            return {std::nullopt, GenerationFailure::ConnectorUnresolved};
        }
        auto corridor = routeCorridor(
            edge.id, firstRoom->id, secondRoom->id, *first, *second, candidate.rooms);
        if (!corridor) {
            return {std::nullopt, GenerationFailure::CorridorInvalid};
        }
        candidate.corridors.push_back(std::move(*corridor));
    }
    return {std::move(candidate), GenerationFailure::None};
}

} // namespace

bool WorldGenerationResult::succeeded() const noexcept
{
    return world.has_value();
}

WorldGenerationResult generateWorld(
    const gdp::core::Seed masterSeed,
    WorldGenerationConfig config,
    const std::span<const RoomModule> moduleLibrary)
{
    config.maximumAttempts = std::max(config.maximumAttempts, 1U);
    config.roomSpacing = std::max(config.roomSpacing, 12.0F);
    const std::vector<RoomModule> defaultLibrary = moduleLibrary.empty()
        ? makeDefaultRoomModuleLibrary()
        : std::vector<RoomModule>{};
    const std::span<const RoomModule> library = moduleLibrary.empty()
        ? std::span<const RoomModule>{defaultLibrary}
        : moduleLibrary;

    WorldGenerationResult result;
    for (std::uint32_t attempt = 0; attempt < config.maximumAttempts; ++attempt) {
        ++result.attempts;
        const gdp::core::Seed attemptSeed = gdp::core::deriveSeed(
            masterSeed, gdp::core::SeedDomain::World, attempt);
        CandidateOutcome candidate = generateCandidate(
            masterSeed, attemptSeed, attempt, config, library);
        if (!candidate.world) {
            result.lastFailure = candidate.failure;
            continue;
        }
        const WorldValidationResult validation = validateWorld(*candidate.world);
        if (validation.isValid()) {
            result.world = std::move(candidate.world);
            result.lastFailure = GenerationFailure::None;
            return result;
        }
        result.lastFailure = validation.primaryFailure();
    }
    return result;
}

} // namespace gdp::world
