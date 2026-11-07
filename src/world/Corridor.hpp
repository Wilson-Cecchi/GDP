#pragma once

#include "world/Connector.hpp"
#include "world/RoomModule.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace gdp::world {

enum class CorridorShape : std::uint8_t {
    Straight,
    L,
    Z,
};

struct CorridorPath {
    EdgeId edge{0};
    RoomId firstRoom{0};
    RoomId secondRoom{0};
    CorridorShape shape{CorridorShape::Straight};
    float width{1.2F};
    float height{2.2F};
    std::vector<wkc::core::Vec3> points;
};

[[nodiscard]] std::optional<CorridorPath> routeCorridor(
    EdgeId edge,
    RoomId firstRoom,
    RoomId secondRoom,
    const ConnectorInstance& first,
    const ConnectorInstance& second,
    std::span<const RoomInstance> rooms,
    float stubLength = 0.75F);
[[nodiscard]] bool validateCorridorPath(
    const CorridorPath& corridor,
    std::span<const RoomInstance> rooms) noexcept;
// Shared world-space solids for corridor rendering and navigation collision.
[[nodiscard]] std::vector<wkc::core::Aabb> makeCorridorWallColumns(
    std::span<const wkc::core::Vec3> points,
    float width,
    float height);

} // namespace gdp::world
