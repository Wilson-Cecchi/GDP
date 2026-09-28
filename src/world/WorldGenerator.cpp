#include "world/WorldGenerator.hpp"

#include "core/Random.hpp"
#include "game/SeedDomains.hpp"
#include "world/Corridor.hpp"
#include "world/MapGraph.hpp"

#include <algorithm>
#include <cmath>
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
    wkc::core::RandomStream& random)
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

wkc::core::Aabb translatedBounds(
    const wkc::core::Aabb bounds,
    const wkc::core::Vec3 translation) noexcept
{
    return {bounds.minimum + translation, bounds.maximum + translation};
}

bool overlapsWithGap(
    const wkc::core::Aabb first,
    const wkc::core::Aabb second,
    const float gap) noexcept
{
    return first.minimum.x - gap < second.maximum.x
        && first.maximum.x + gap > second.minimum.x
        && first.minimum.z - gap < second.maximum.z
        && first.maximum.z + gap > second.minimum.z;
}

std::optional<wkc::core::Vec3> placementForNode(
    const MapGraph& graph,
    const RoomNodeId node,
    const float spacing,
    const RoomInstance& prototype,
    const std::span<const RoomInstance> placedRooms,
    const wkc::core::Seed placementSeed)
{
    // Preserve the former default feel as a 2.5-unit doorway-to-doorway gap,
    // while allowing callers to request roomier maps through roomSpacing.
    const float corridorClearance = std::max(2.5F, spacing - 9.5F);
    const auto positionBeside = [&](const RoomInstance& anchor, const int direction) {
        switch (direction) {
        case 0: return wkc::core::Vec3{anchor.transform.position.x, 0.0F,
            anchor.worldBounds.minimum.z - corridorClearance
                - prototype.worldBounds.maximum.z};
        case 1: return wkc::core::Vec3{
            anchor.worldBounds.maximum.x + corridorClearance
                - prototype.worldBounds.minimum.x, 0.0F,
            anchor.transform.position.z};
        case 2: return wkc::core::Vec3{anchor.transform.position.x, 0.0F,
            anchor.worldBounds.maximum.z + corridorClearance
                - prototype.worldBounds.minimum.z};
        default: return wkc::core::Vec3{
            anchor.worldBounds.minimum.x - corridorClearance
                - prototype.worldBounds.maximum.x, 0.0F,
            anchor.transform.position.z};
        }
    };
    const auto clear = [&](const wkc::core::Vec3 position) {
        const wkc::core::Aabb bounds = translatedBounds(prototype.worldBounds, position);
        return std::none_of(placedRooms.begin(), placedRooms.end(),
            [bounds](const RoomInstance& room) {
                return overlapsWithGap(bounds, room.worldBounds, 0.5F);
            });
    };
    wkc::core::RandomStream random{placementSeed};
    const auto main = std::find(graph.mainPath.begin(), graph.mainPath.end(), node);
    if (main != graph.mainPath.end()) {
        if (main == graph.mainPath.begin()) {
            return wkc::core::Vec3{};
        }
        const RoomNodeId previousId = *(main - 1);
        const RoomInstance* previous = findPlacedRoom(placedRooms, previousId);
        if (previous == nullptr) {
            return std::nullopt;
        }
        // Bounded one- or two-turn routes change the silhouette without an
        // unconstrained random walk that can self-cross or trap corridors.
        wkc::core::RandomStream layout{placementSeed};
        const int firstDirection = layout.rangeInt(0, 3);
        const int turn = layout.chance(0.5F) ? 1 : 3;
        const std::size_t turnIndex = static_cast<std::size_t>(layout.rangeInt(
            2, static_cast<int>(graph.mainPath.size() - 2U)));
        const bool secondTurn = turnIndex + 2U < graph.mainPath.size()
            && layout.chance(0.5F);
        const int finalDirection = (firstDirection + turn
            + (layout.chance(0.5F) ? 1 : 3)) % 4;
        const std::size_t index = static_cast<std::size_t>(main - graph.mainPath.begin());
        const int direction = index < turnIndex ? firstDirection
            : secondTurn && index >= turnIndex + 2U
                ? finalDirection : (firstDirection + turn) % 4;
        const wkc::core::Vec3 position = positionBeside(*previous, direction);
        return clear(position) ? std::optional{position} : std::nullopt;
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
        return std::nullopt;
    }
    const RoomInstance& parent = *neighbors.front();
    const int offset = random.rangeInt(0, 3);
    std::optional<wkc::core::Vec3> best;
    float bestScore = 0.0F;
    for (int index = 0; index < 4; ++index) {
        const wkc::core::Vec3 position = positionBeside(parent, (offset + index) % 4);
        if (!clear(position)) {
            continue;
        }
        float score = 0.0F;
        for (const RoomInstance* neighbor : neighbors) {
            score += std::abs(position.x - neighbor->transform.position.x)
                + std::abs(position.z - neighbor->transform.position.z);
        }
        if (!best || score < bestScore) {
            best = position;
            bestScore = score;
        }
    }
    return best;
}

ConnectorInstance* chooseConnector(RoomInstance& room, const wkc::core::Vec3 target)
{
    const wkc::core::Vec3 direction = wkc::core::normalized(target - room.transform.position);
    ConnectorInstance* best = nullptr;
    float bestAlignment = -2.0F;
    for (ConnectorInstance& connector : room.connectors) {
        if (!connector.isAvailable()) {
            continue;
        }
        const float alignment = wkc::core::dot(connector.worldNormal, direction);
        if (alignment > bestAlignment) {
            bestAlignment = alignment;
            best = &connector;
        }
    }
    return best;
}

CandidateOutcome generateCandidate(
    const wkc::core::Seed masterSeed,
    const wkc::core::Seed attemptSeed,
    const std::uint32_t attempt,
    const WorldGenerationConfig config,
    const std::span<const RoomModule> library)
{
    World candidate;
    candidate.masterSeed = masterSeed;
    candidate.generationSeed = attemptSeed;
    candidate.generationVersion = wkc::core::currentGenerationVersion;
    candidate.acceptedAttempt = attempt;
    candidate.graph = generateMapGraph(
        wkc::core::deriveSeed(attemptSeed, gdp::game::SeedDomain::Graph, 0),
        {config.roomCount, config.loopBudget});
    if (!validateMapGraph(candidate.graph).isValid()) {
        return {std::nullopt, GenerationFailure::GraphInvalid};
    }

    candidate.rooms.reserve(candidate.graph.nodes.size());
    for (const RoomNode& node : candidate.graph.nodes) {
        const wkc::core::Seed roomSeed = wkc::core::deriveSeed(
            attemptSeed, gdp::game::SeedDomain::Room, node.id);
        wkc::core::RandomStream random{wkc::core::deriveSeed(
            roomSeed, gdp::game::SeedDomain::RoomModule, node.id)};
        const RoomModule* module = selectModule(library, node, random);
        if (module == nullptr) {
            return {std::nullopt, GenerationFailure::ModuleUnavailable};
        }
        const auto rotation = static_cast<wkc::core::QuarterTurn>(random.rangeInt(0, 3));
        const auto prototype = instantiateRoom(
            *module, node.id, node.role, roomSeed, {}, rotation);
        if (!prototype) {
            return {std::nullopt, GenerationFailure::ModuleUnavailable};
        }
        const auto position = placementForNode(
            candidate.graph, node.id, config.roomSpacing, *prototype, candidate.rooms,
            wkc::core::deriveSeed(attemptSeed, gdp::game::SeedDomain::MapPlacement,
                node.id <= candidate.graph.mainPath.size() ? 0U : node.id));
        if (!position) {
            return {std::nullopt, GenerationFailure::RoomOverlap};
        }
        auto room = instantiateRoom(*module, node.id, node.role, roomSeed, *position, rotation);
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
        float routeLength = 0.0F;
        for (std::size_t point = 1U; point < corridor->points.size(); ++point) {
            routeLength += std::abs(corridor->points[point].x - corridor->points[point - 1U].x)
                + std::abs(corridor->points[point].z - corridor->points[point - 1U].z);
        }
        if (routeLength > 17.0F) {
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
    const wkc::core::Seed masterSeed,
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
        const wkc::core::Seed attemptSeed = wkc::core::deriveSeed(
            masterSeed, gdp::game::SeedDomain::World, attempt);
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
