#include "world/Corridor.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace gdp::world {
namespace {

bool segmentHitsRoom(
    const wkc::core::Vec3 first,
    const wkc::core::Vec3 second,
    const float halfWidth,
    const wkc::core::Aabb room) noexcept
{
    const float minimumX = std::min(first.x, second.x) - halfWidth;
    const float maximumX = std::max(first.x, second.x) + halfWidth;
    const float minimumZ = std::min(first.z, second.z) - halfWidth;
    const float maximumZ = std::max(first.z, second.z) + halfWidth;
    return minimumX < room.maximum.x && maximumX > room.minimum.x
        && minimumZ < room.maximum.z && maximumZ > room.minimum.z;
}

bool pathIsClear(
    const std::vector<wkc::core::Vec3>& points,
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

bool axisAligned(const wkc::core::Vec3 first, const wkc::core::Vec3 second) noexcept
{
    const bool sameX = wkc::core::nearlyEqual(first.x, second.x);
    const bool sameZ = wkc::core::nearlyEqual(first.z, second.z);
    return sameX != sameZ;
}

void removeRepeatedPoints(std::vector<wkc::core::Vec3>& points)
{
    points.erase(
        std::unique(points.begin(), points.end(), [](const auto first, const auto second) {
            return wkc::core::nearlyEqual(first, second);
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
    const wkc::core::Vec3 firstStub = first.worldPosition + (first.worldNormal * stub);
    const wkc::core::Vec3 secondStub = second.worldPosition + (second.worldNormal * stub);
    const float width = std::min(first.width, second.width);
    const float height = std::min(first.height, second.height);

    std::vector<std::pair<CorridorShape, std::vector<wkc::core::Vec3>>> candidates;
    if (wkc::core::nearlyEqual(firstStub.x, secondStub.x)
        || wkc::core::nearlyEqual(firstStub.z, secondStub.z)) {
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

std::vector<wkc::core::Aabb> makeCorridorWallColumns(
    const std::span<const wkc::core::Vec3> points,
    const float width,
    const float height)
{
    std::vector<wkc::core::Aabb> columns;
    // The largest corridor occupant has radius 0.52 m. Keep at least
    // 1.08 m between facing columns so it can pass down the center.
    constexpr float minimumClearWidth = 1.08F;
    constexpr float minimumSegmentLength = 5.0F;
    constexpr float endpointClearance = 1.2F;
    constexpr float stationSpacing = 2.8F;
    constexpr float halfColumnLength = 0.19F;
    if (points.size() < 2U || !std::isfinite(width)
        || !std::isfinite(height) || width <= minimumClearWidth
        || height <= 0.0F) {
        return columns;
    }
    const float depth = std::min(0.18F,
        (width - minimumClearWidth) * 0.5F);
    if (depth < 0.025F) {
        return columns;
    }

    for (std::size_t segment = 0U; segment + 1U < points.size(); ++segment) {
        const auto first = points[segment];
        const auto second = points[segment + 1U];
        if (!std::isfinite(first.x) || !std::isfinite(first.y)
            || !std::isfinite(first.z) || !std::isfinite(second.x)
            || !std::isfinite(second.y) || !std::isfinite(second.z)
            || !wkc::core::nearlyEqual(first.y, second.y)) {
            continue;
        }
        const float dx = second.x - first.x;
        const float dz = second.z - first.z;
        const bool alongX = std::abs(dx) >= minimumSegmentLength
            && wkc::core::nearlyEqual(first.z, second.z);
        const bool alongZ = std::abs(dz) >= minimumSegmentLength
            && wkc::core::nearlyEqual(first.x, second.x);
        if (alongX == alongZ) {
            continue;
        }
        const float length = alongX ? std::abs(dx) : std::abs(dz);
        const auto count = std::min<std::size_t>(16U,
            1U + static_cast<std::size_t>(
                std::floor((length - 2.0F * endpointClearance)
                    / stationSpacing)));
        const float floor = (first.y + second.y) * 0.5F;
        const float ceiling = floor + height;
        for (std::size_t station = 0U; station < count; ++station) {
            // A centered fraction gives identical placements when a route is
            // visited in reverse, including segments in an L or Z path.
            const float numerator = static_cast<float>(
                static_cast<int>(2U * station) - static_cast<int>(count - 1U));
            const float offset = numerator
                / static_cast<float>(2U * (count + 1U));
            if (alongX) {
                const float x = (first.x + second.x) * 0.5F + dx * offset;
                const float z = (first.z + second.z) * 0.5F;
                columns.push_back({
                    {x - halfColumnLength, floor, z - width * 0.5F},
                    {x + halfColumnLength, ceiling,
                        z - width * 0.5F + depth}});
                columns.push_back({
                    {x - halfColumnLength, floor,
                        z + width * 0.5F - depth},
                    {x + halfColumnLength, ceiling, z + width * 0.5F}});
            } else {
                const float x = (first.x + second.x) * 0.5F;
                const float z = (first.z + second.z) * 0.5F + dz * offset;
                columns.push_back({
                    {x - width * 0.5F, floor, z - halfColumnLength},
                    {x - width * 0.5F + depth, ceiling,
                        z + halfColumnLength}});
                columns.push_back({
                    {x + width * 0.5F - depth, floor,
                        z - halfColumnLength},
                    {x + width * 0.5F, ceiling,
                        z + halfColumnLength}});
            }
        }
    }
    std::sort(columns.begin(), columns.end(),
        [](const wkc::core::Aabb& first, const wkc::core::Aabb& second) {
            if (first.minimum.x != second.minimum.x) {
                return first.minimum.x < second.minimum.x;
            }
            if (first.minimum.z != second.minimum.z) {
                return first.minimum.z < second.minimum.z;
            }
            return first.minimum.y < second.minimum.y;
        });
    return columns;
}

} // namespace gdp::world
