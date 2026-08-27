#pragma once

#include "core/Seed.hpp"
#include "world/Connector.hpp"
#include "world/RoomModule.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace gdp::world {

using RoomNodeId = std::uint32_t;

struct RoomNode {
    RoomNodeId id{0};
    RoomRole role{RoomRole::Normal};
    RoomTheme theme{RoomTheme::Industrial};
    std::uint32_t depth{0};
};

struct MapEdge {
    EdgeId id{0};
    RoomNodeId first{0};
    RoomNodeId second{0};
    bool mainPath{false};
};

struct MapGraph {
    std::vector<RoomNode> nodes;
    std::vector<MapEdge> edges;
    std::vector<RoomNodeId> mainPath;
};

struct MapGraphConfig {
    std::uint32_t roomCount{8};
    std::uint32_t loopBudget{1};
};

enum class GraphValidationError : std::uint8_t {
    Empty,
    InvalidNodeId,
    DuplicateNodeId,
    InvalidEdgeId,
    DuplicateEdgeId,
    SelfEdge,
    DuplicateConnection,
    MissingEndpoint,
    InvalidStartCount,
    InvalidBossCount,
    Disconnected,
    InvalidDepth,
    InvalidMainPath,
    InvalidHub,
};

struct GraphValidationResult {
    std::vector<GraphValidationError> errors;

    [[nodiscard]] bool isValid() const noexcept;
};

[[nodiscard]] MapGraph generateMapGraph(gdp::core::Seed seed, MapGraphConfig config = {});
[[nodiscard]] GraphValidationResult validateMapGraph(const MapGraph& graph);
[[nodiscard]] std::string graphToDot(const MapGraph& graph);

} // namespace gdp::world
