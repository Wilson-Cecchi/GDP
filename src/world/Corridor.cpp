#include "world/Corridor.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace gdp::world {
namespace {

bool segmentHitsRoom(
    const gdp::core::Vec3 first,
    const gdp::core::Vec3 second,
    const float halfWidth,
    const gdp::core::Aabb room) noexcept
{
    const float minimumX = std::min(first.x, second.x) - halfWidth;
    const float maximumX = std::max(first.x, second.x) + halfWidth;
    const float minimumZ = std::min(first.z, second.z) - halfWidth;
    const float maximumZ = std::max(first.z, second.z) + halfWidth;
    return minimumX < room.maximum.x && maximumX > room.minimum.x
        && minimumZ < room.maximum.z && maximumZ > room.minimum.z;
}

bool pathIsClear(
    const std::vector<gdp::core::Vec3>& points,
    const float width,
    const std::span<const RoomInstance> rooms,
    const RoomId firstRoom,
    const RoomId secondRoom) noexcept
{
    for (std::size_t index = 0; index + 1U < points.size(); ++index) {
        for (const RoomInstance& room : rooms) {
            const bool exitsFirstRoom = room.id == firstRoom && index == 0U;
            const bool entersSecondRoom = room.id == secondRoom
                && index + 2U == points.size();
            if (exitsFirstRoom || entersSecondRoom) {
                continue;
            }
            if (segmentHitsRoom(points[index], points[index + 1U], width * 0.5F,
                    room.worldBounds)) {
                return false;
            }
        }
    }
    return true;
}

bool axisAligned(const gdp::core::Vec3 first, const gdp::core::Vec3 second) noexcept
{
    const bool sameX = gdp::core::nearlyEqual(first.x, second.x);
    const bool sameZ = gdp::core::nearlyEqual(first.z, second.z);
    return sameX != sameZ;
}

void removeRepeatedPoints(std::vector<gdp::core::Vec3>& points)
{
    points.erase(
        std::unique(points.begin(), points.end(), [](const auto first, const auto second) {
            return gdp::core::nearlyEqual(first, second);
        }),
        points.end());
}

} // namespace

std::optional<CorridorPath> routeCorridor(
    const EdgeId edge,
    const RoomId firstRoom,
    const RoomId secondRoom,
    const ConnectorInstance& first,
    const ConnectorInstance& second,
    const std::span<const RoomInstance> rooms,
    const float stubLength)
{
    if (edge == 0 || !first.edge || !second.edge || *first.edge != edge || *second.edge != edge) {
        return std::nullopt;
    }

    const float stub = std::max(stubLength, 0.0F);
    const gdp::core::Vec3 firstStub = first.worldPosition + (first.worldNormal * stub);
    const gdp::core::Vec3 secondStub = second.worldPosition + (second.worldNormal * stub);
    const float width = std::min(first.width, second.width);
    const float height = std::min(first.height, second.height);

    std::vector<std::pair<CorridorShape, std::vector<gdp::core::Vec3>>> candidates;
    if (gdp::core::nearlyEqual(firstStub.x, secondStub.x)
        || gdp::core::nearlyEqual(firstStub.z, secondStub.z)) {
        candidates.push_back({CorridorShape::Straight,
            {first.worldPosition, firstStub, secondStub, second.worldPosition}});
    } else {
        candidates.push_back({CorridorShape::L,
            {first.worldPosition, firstStub, {secondStub.x, firstStub.y, firstStub.z},
                secondStub, second.worldPosition}});
        candidates.push_back({CorridorShape::L,
            {first.worldPosition, firstStub, {firstStub.x, firstStub.y, secondStub.z},
                secondStub, second.worldPosition}});
        const float middleX = (firstStub.x + secondStub.x) * 0.5F;
        candidates.push_back({CorridorShape::Z,
            {first.worldPosition, firstStub, {middleX, firstStub.y, firstStub.z},
                {middleX, firstStub.y, secondStub.z}, secondStub, second.worldPosition}});
        const float middleZ = (firstStub.z + secondStub.z) * 0.5F;
        candidates.push_back({CorridorShape::Z,
            {first.worldPosition, firstStub, {firstStub.x, firstStub.y, middleZ},
                {secondStub.x, firstStub.y, middleZ}, secondStub, second.worldPosition}});
    }

    for (auto& [shape, points] : candidates) {
        removeRepeatedPoints(points);
        if (points.size() >= 2U && pathIsClear(points, width, rooms, firstRoom, secondRoom)) {
            CorridorPath corridor{edge, firstRoom, secondRoom, shape, width, height, std::move(points)};
            if (validateCorridorPath(corridor, rooms)) {
                return corridor;
            }
        }
    }
    return std::nullopt;
}

bool validateCorridorPath(
    const CorridorPath& corridor,
    const std::span<const RoomInstance> rooms) noexcept
{
    if (corridor.edge == 0 || corridor.firstRoom == corridor.secondRoom
        || corridor.width <= 0.0F || corridor.height <= 0.0F
        || corridor.points.size() < 2U) {
        return false;
    }
    for (std::size_t index = 0; index + 1U < corridor.points.size(); ++index) {
        if (!axisAligned(corridor.points[index], corridor.points[index + 1U])) {
            return false;
        }
    }
    return pathIsClear(
        corridor.points,
        corridor.width,
        rooms,
        corridor.firstRoom,
        corridor.secondRoom);
}

} // namespace gdp::world
