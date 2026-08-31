#include "world/MapGraph.hpp"

#include "core/Random.hpp"

#include <algorithm>
#include <queue>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace gdp::world {
namespace {

std::uint64_t connectionKey(RoomNodeId first, RoomNodeId second) noexcept
{
    if (first > second) {
        std::swap(first, second);
    }
    return (static_cast<std::uint64_t>(first) << 32U) | second;
}

RoomTheme chooseTheme(const RoomRole role, wkc::core::RandomStream& random)
{
    switch (role) {
    case RoomRole::Start:
    case RoomRole::Hub:
        return RoomTheme::Industrial;
    case RoomRole::Arena:
        return random.chance(0.5F) ? RoomTheme::Security : RoomTheme::Maintenance;
    case RoomRole::Secret:
        return random.chance(0.5F) ? RoomTheme::Laboratory : RoomTheme::Storage;
    case RoomRole::Boss: {
        constexpr RoomTheme choices[]{
            RoomTheme::Laboratory,
            RoomTheme::Maintenance,
            RoomTheme::Security,
        };
        return choices[random.rangeInt(0, 2)];
    }
    case RoomRole::Normal:
    case RoomRole::DeadEnd: {
        constexpr RoomTheme choices[]{
            RoomTheme::Industrial,
            RoomTheme::Storage,
            RoomTheme::Office,
            RoomTheme::Laboratory,
            RoomTheme::Maintenance,
            RoomTheme::Security,
            RoomTheme::Cafeteria,
        };
        return choices[random.rangeInt(0, 6)];
    }
    case RoomRole::Count:
        break;
    }
    return RoomTheme::Industrial;
}

std::unordered_map<RoomNodeId, std::vector<RoomNodeId>> adjacency(const MapGraph& graph)
{
    std::unordered_map<RoomNodeId, std::vector<RoomNodeId>> result;
    for (const RoomNode& node : graph.nodes) {
        result[node.id];
    }
    for (const MapEdge& edge : graph.edges) {
        result[edge.first].push_back(edge.second);
        result[edge.second].push_back(edge.first);
    }
    return result;
}

void calculateDepths(MapGraph& graph)
{
    const auto links = adjacency(graph);
    std::unordered_map<RoomNodeId, std::uint32_t> depths;
    std::queue<RoomNodeId> pending;
    depths[1] = 0;
    pending.push(1);
    while (!pending.empty()) {
        const RoomNodeId current = pending.front();
        pending.pop();
        for (const RoomNodeId neighbor : links.at(current)) {
            if (!depths.contains(neighbor)) {
                depths[neighbor] = depths[current] + 1U;
                pending.push(neighbor);
            }
        }
    }
    for (RoomNode& node : graph.nodes) {
        node.depth = depths.contains(node.id) ? depths[node.id] : 0U;
    }
}

} // namespace

bool GraphValidationResult::isValid() const noexcept
{
    return errors.empty();
}

MapGraph generateMapGraph(const wkc::core::Seed seed, MapGraphConfig config)
{
    config.roomCount = std::clamp(config.roomCount, 4U, 32U);
    wkc::core::RandomStream random{seed};
    MapGraph graph;
    graph.nodes.reserve(config.roomCount);

    const std::uint32_t branchCount = config.roomCount >= 6U ? 2U : 0U;
    const std::uint32_t mainCount = config.roomCount - branchCount;
    for (std::uint32_t index = 0; index < config.roomCount; ++index) {
        RoomRole role = RoomRole::Normal;
        if (index == 0U) {
            role = RoomRole::Start;
        } else if (index == mainCount - 1U) {
            role = RoomRole::Boss;
        } else if (branchCount > 0U && index == 1U) {
            role = RoomRole::Hub;
        } else if (index == mainCount - 2U) {
            role = RoomRole::Arena;
        } else if (index >= mainCount) {
            role = ((index - mainCount) % 2U == 0U) ? RoomRole::DeadEnd : RoomRole::Secret;
        }
        graph.nodes.push_back({index + 1U, role, chooseTheme(role, random), 0});
    }

    EdgeId nextEdge = 1;
    std::unordered_set<std::uint64_t> connections;
    std::vector<std::uint32_t> degrees(config.roomCount, 0U);
    for (std::uint32_t index = 0; index + 1U < mainCount; ++index) {
        graph.edges.push_back({nextEdge++, index + 1U, index + 2U, true});
        graph.mainPath.push_back(index + 1U);
        connections.insert(connectionKey(index + 1U, index + 2U));
        ++degrees[index];
        ++degrees[index + 1U];
    }
    graph.mainPath.push_back(mainCount);

    for (std::uint32_t index = mainCount; index < config.roomCount; ++index) {
        const RoomNodeId parent = 2;
        graph.edges.push_back({nextEdge++, parent, index + 1U, false});
        connections.insert(connectionKey(parent, index + 1U));
        ++degrees[parent - 1U];
        ++degrees[index];
    }

    // A loop must close a nearby branch. Arbitrary pairs can be valid as a
    // graph while producing a corridor that runs beside several rooms before
    // reaching its destination.
    const auto treeLinks = adjacency(graph);
    const auto isMainNode = [&graph](const RoomNodeId id) {
        return std::find(graph.mainPath.begin(), graph.mainPath.end(), id)
            != graph.mainPath.end();
    };
    std::vector<std::pair<std::uint32_t, std::uint32_t>> loopCandidates;
    for (std::uint32_t firstIndex = 0; firstIndex < config.roomCount; ++firstIndex) {
        for (std::uint32_t secondIndex = firstIndex + 1U;
             secondIndex < config.roomCount; ++secondIndex) {
            const RoomNodeId first = firstIndex + 1U;
            const RoomNodeId second = secondIndex + 1U;
            if (isMainNode(first) == isMainNode(second)
                || connections.contains(connectionKey(first, second))) {
                continue;
            }
            const bool shareNeighbor = std::any_of(
                treeLinks.at(first).begin(), treeLinks.at(first).end(),
                [&treeLinks, second](const RoomNodeId neighbor) {
                    const auto& secondLinks = treeLinks.at(second);
                    return std::find(secondLinks.begin(), secondLinks.end(), neighbor)
                        != secondLinks.end();
                });
            if (shareNeighbor) {
                loopCandidates.push_back({firstIndex, secondIndex});
            }
        }
    }

    std::uint32_t loopsAdded = 0;
    while (!loopCandidates.empty() && loopsAdded < config.loopBudget) {
        const auto selected = static_cast<std::size_t>(random.rangeInt(
            0, static_cast<int>(loopCandidates.size() - 1U)));
        const auto [firstIndex, secondIndex] = loopCandidates[selected];
        loopCandidates.erase(loopCandidates.begin()
            + static_cast<std::ptrdiff_t>(selected));
        if (degrees[firstIndex] >= 4U || degrees[secondIndex] >= 4U) {
            continue;
        }
        const RoomNodeId first = firstIndex + 1U;
        const RoomNodeId second = secondIndex + 1U;
        graph.edges.push_back({nextEdge++, first, second, false});
        connections.insert(connectionKey(first, second));
        ++degrees[firstIndex];
        ++degrees[secondIndex];
        ++loopsAdded;
    }

    calculateDepths(graph);
    return graph;
}

GraphValidationResult validateMapGraph(const MapGraph& graph)
{
    GraphValidationResult result;
    if (graph.nodes.empty()) {
        result.errors.push_back(GraphValidationError::Empty);
        return result;
    }

    std::unordered_set<RoomNodeId> nodeIds;
    std::size_t startCount = 0;
    std::size_t bossCount = 0;
    for (const RoomNode& node : graph.nodes) {
        if (node.id == 0) {
            result.errors.push_back(GraphValidationError::InvalidNodeId);
        }
        if (!nodeIds.insert(node.id).second) {
            result.errors.push_back(GraphValidationError::DuplicateNodeId);
        }
        startCount += node.role == RoomRole::Start ? 1U : 0U;
        bossCount += node.role == RoomRole::Boss ? 1U : 0U;
    }
    if (startCount != 1U) {
        result.errors.push_back(GraphValidationError::InvalidStartCount);
    }
    if (bossCount != 1U) {
        result.errors.push_back(GraphValidationError::InvalidBossCount);
    }

    std::unordered_set<EdgeId> edgeIds;
    std::unordered_set<std::uint64_t> connections;
    for (const MapEdge& edge : graph.edges) {
        if (edge.id == 0) {
            result.errors.push_back(GraphValidationError::InvalidEdgeId);
        }
        if (!edgeIds.insert(edge.id).second) {
            result.errors.push_back(GraphValidationError::DuplicateEdgeId);
        }
        if (edge.first == edge.second) {
            result.errors.push_back(GraphValidationError::SelfEdge);
        }
        if (!connections.insert(connectionKey(edge.first, edge.second)).second) {
            result.errors.push_back(GraphValidationError::DuplicateConnection);
        }
        if (!nodeIds.contains(edge.first) || !nodeIds.contains(edge.second)) {
            result.errors.push_back(GraphValidationError::MissingEndpoint);
        }
    }

    const auto links = adjacency(graph);
    std::unordered_set<RoomNodeId> visited;
    std::queue<RoomNodeId> pending;
    pending.push(graph.nodes.front().id);
    visited.insert(graph.nodes.front().id);
    while (!pending.empty()) {
        const RoomNodeId current = pending.front();
        pending.pop();
        for (const RoomNodeId neighbor : links.at(current)) {
            if (visited.insert(neighbor).second) {
                pending.push(neighbor);
            }
        }
    }
    if (visited.size() != graph.nodes.size()) {
        result.errors.push_back(GraphValidationError::Disconnected);
    }

    for (const RoomNode& node : graph.nodes) {
        if (node.role == RoomRole::Start && node.depth != 0U) {
            result.errors.push_back(GraphValidationError::InvalidDepth);
        }
        if (node.role != RoomRole::Start && node.depth == 0U) {
            result.errors.push_back(GraphValidationError::InvalidDepth);
        }
        if (node.role == RoomRole::Hub && links.at(node.id).size() < 3U) {
            result.errors.push_back(GraphValidationError::InvalidHub);
        }
    }
    if (graph.mainPath.size() < 2U || graph.mainPath.front() != 1U
        || !nodeIds.contains(graph.mainPath.back())) {
        result.errors.push_back(GraphValidationError::InvalidMainPath);
    } else {
        const auto lastNode = std::find_if(
            graph.nodes.begin(), graph.nodes.end(), [&graph](const RoomNode& node) {
                return node.id == graph.mainPath.back();
            });
        if (lastNode == graph.nodes.end() || lastNode->role != RoomRole::Boss) {
            result.errors.push_back(GraphValidationError::InvalidMainPath);
        }
        for (std::size_t index = 0; index + 1U < graph.mainPath.size(); ++index) {
            if (!connections.contains(connectionKey(graph.mainPath[index], graph.mainPath[index + 1U]))) {
                result.errors.push_back(GraphValidationError::InvalidMainPath);
                break;
            }
        }
    }
    return result;
}

std::string graphToDot(const MapGraph& graph)
{
    std::ostringstream output;
    output << "graph GDP {\n";
    for (const RoomNode& node : graph.nodes) {
        output << "  " << node.id << " [label=\"room " << node.id << " depth "
               << node.depth << "\"];\n";
    }
    for (const MapEdge& edge : graph.edges) {
        output << "  " << edge.first << " -- " << edge.second
               << " [label=\"edge " << edge.id << "\"];\n";
    }
    output << "}\n";
    return output.str();
}

} // namespace gdp::world
