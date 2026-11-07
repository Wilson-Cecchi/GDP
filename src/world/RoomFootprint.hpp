#pragma once

#include "core/Math.hpp"

#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

namespace gdp::world {

[[nodiscard]] inline std::vector<wkc::core::Vec2> centralHubPreviewFootprint()
{
    // Regular octagon inside the 20 x 20 m bounds. Every face has the same
    // length; the four cardinal faces can host the future doors.
    constexpr float halfFlat = 4.14213562F;
    return {
        {-halfFlat, -10.0F}, {halfFlat, -10.0F},
        {10.0F, -halfFlat}, {10.0F, halfFlat},
        {halfFlat, 10.0F}, {-halfFlat, 10.0F},
        {-10.0F, halfFlat}, {-10.0F, -halfFlat},
    };
}

[[nodiscard]] inline std::vector<wkc::core::Vec2> technicalLPreviewFootprint()
{
    return {
        {-8.0F, -10.0F}, {1.0F, -10.0F}, {1.0F, -2.0F},
        {8.0F, -2.0F}, {8.0F, 10.0F}, {-8.0F, 10.0F},
    };
}

[[nodiscard]] inline std::vector<wkc::core::Vec2> securityTPreviewFootprint()
{
    // A broad observation gallery and a narrower southern approach.
    return {
        {-4.0F, -10.0F}, {4.0F, -10.0F}, {4.0F, -2.0F},
        {10.0F, -2.0F}, {10.0F, 10.0F}, {-10.0F, 10.0F},
        {-10.0F, -2.0F}, {-4.0F, -2.0F},
    };
}

// World-space X/Z outline. The disk test keeps the character's whole body
// inside diagonal and concave walls, rather than just its centre point.
[[nodiscard]] inline bool footprintContainsDisk(
    const std::span<const wkc::core::Vec2> outline,
    const wkc::core::Vec3 position,
    const float radius = 0.0F) noexcept
{
    if (outline.size() < 3U || radius < 0.0F) return false;
    bool inside = false;
    float closestSquared = 1.0e30F;
    for (std::size_t index = 0U; index < outline.size(); ++index) {
        const auto a = outline[index];
        const auto b = outline[(index + 1U) % outline.size()];
        const float dx = b.x - a.x;
        const float dz = b.y - a.y;
        const float lengthSquared = dx * dx + dz * dz;
        if (lengthSquared < 1.0e-8F) return false;
        const float t = std::clamp(((position.x - a.x) * dx
            + (position.z - a.y) * dz) / lengthSquared, 0.0F, 1.0F);
        const float offsetX = position.x - (a.x + t * dx);
        const float offsetZ = position.z - (a.y + t * dz);
        closestSquared = std::min(closestSquared,
            offsetX * offsetX + offsetZ * offsetZ);
        if ((a.y > position.z) != (b.y > position.z)) {
            const float crossing = a.x + dx * (position.z - a.y) / dz;
            if (position.x < crossing) inside = !inside;
        }
    }
    return inside && closestSquared + 1.0e-6F >= radius * radius;
}

} // namespace gdp::world
