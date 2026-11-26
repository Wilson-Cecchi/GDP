#include "game/visual/GdpRenderer.hpp"

#include "core/Hash.hpp"
#include "core/Log.hpp"
#include "core/Math.hpp"
#include "game/visual/EnemyGenerator.hpp"
#include "game/visual/RoomMaterialGenerator.hpp"
#include "game/visual/VisualAssets.hpp"
#include "game/visual/UiFontAtlas.hpp"
#include "game/Localization.hpp"
#include "gameplay/PlayerGrenade.hpp"
#include "procedural/MeshGenerator.hpp"
#include "procedural/TextureGenerator.hpp"
#include "world/Corridor.hpp"
#include "world/RoomFootprint.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <numbers>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace gdp::game::visual {
namespace {

constexpr wkc::core::Vec3 hubRouteLedOrange{0.90F, 0.36F, 0.08F};

wkc::render::MeshData makePlanterLeafMesh()
{
    struct LeafRow { float height; float halfWidth; float bend; };
    constexpr std::array rows{
        LeafRow{0.0F, 0.018F, 0.0F},
        LeafRow{0.18F, 0.10F, 0.06F},
        LeafRow{0.48F, 0.17F, 0.18F},
        LeafRow{0.76F, 0.13F, 0.36F},
        LeafRow{1.0F, 0.008F, 0.56F},
    };
    wkc::render::MeshData mesh;
    const auto addFacet = [&](const wkc::core::Vec3 a,
        const wkc::core::Vec3 b, const wkc::core::Vec3 c) {
        const wkc::core::Vec3 normal = wkc::core::normalized(
            wkc::core::cross(b - a, c - a));
        const std::uint32_t first = static_cast<std::uint32_t>(
            mesh.vertices.size());
        mesh.vertices.push_back({a, normal, {0.0F, 0.0F}, 0U});
        mesh.vertices.push_back({b, normal, {0.5F, 1.0F}, 0U});
        mesh.vertices.push_back({c, normal, {1.0F, 0.0F}, 0U});
        mesh.indices.insert(mesh.indices.end(),
            {first, first + 1U, first + 2U});
    };
    for (std::size_t index = 0; index + 1U < rows.size(); ++index) {
        const auto& low = rows[index];
        const auto& high = rows[index + 1U];
        const wkc::core::Vec3 lowLeft{
            -low.halfWidth, low.height, low.bend};
        const wkc::core::Vec3 lowMiddle{
            0.0F, low.height, low.bend + 0.025F};
        const wkc::core::Vec3 lowRight{
            low.halfWidth, low.height, low.bend};
        const wkc::core::Vec3 highLeft{
            -high.halfWidth, high.height, high.bend};
        const wkc::core::Vec3 highMiddle{
            0.0F, high.height, high.bend + 0.025F};
        const wkc::core::Vec3 highRight{
            high.halfWidth, high.height, high.bend};
        for (const std::array triangle : {
                 std::array{lowLeft, highLeft, highMiddle},
                 std::array{lowLeft, highMiddle, lowMiddle},
                 std::array{lowMiddle, highMiddle, highRight},
                 std::array{lowMiddle, highRight, lowRight}}) {
            addFacet(triangle[0], triangle[1], triangle[2]);
            addFacet(triangle[2], triangle[1], triangle[0]);
        }
    }
    mesh.bounds = {{-0.17F, 0.0F, 0.0F},
        {0.17F, 1.0F, 0.585F}};
    mesh.backfaceCullingSafe = true;
    return mesh;
}

wkc::render::MeshData makeGlassTestDomeMesh()
{
    constexpr std::uint32_t slices = 24U;
    constexpr std::uint32_t stacks = 8U;
    constexpr float pi = std::numbers::pi_v<float>;
    wkc::render::MeshData mesh;
    mesh.vertices.reserve((slices + 1U) * (stacks + 1U));
    mesh.indices.reserve(slices * stacks * 6U);
    for (std::uint32_t row = 0U; row <= stacks; ++row) {
        const float latitude = 0.5F * pi * static_cast<float>(row) / stacks;
        const float radius = std::cos(latitude);
        const float height = std::sin(latitude);
        for (std::uint32_t column = 0U; column <= slices; ++column) {
            const float longitude = 2.0F * pi * static_cast<float>(column)
                / slices;
            const wkc::core::Vec3 normal{
                radius * std::cos(longitude), height,
                radius * std::sin(longitude)};
            mesh.vertices.push_back({normal, normal,
                {static_cast<float>(column) / slices,
                    static_cast<float>(row) / stacks}, 0U});
        }
    }
    for (std::uint32_t row = 0U; row < stacks; ++row) {
        for (std::uint32_t column = 0U; column < slices; ++column) {
            const std::uint32_t first = row * (slices + 1U) + column;
            const std::uint32_t next = first + slices + 1U;
            mesh.indices.insert(mesh.indices.end(),
                {first, first + 1U, next,
                    first + 1U, next + 1U, next});
        }
    }
    mesh.bounds = {{-1.0F, 0.0F, -1.0F}, {1.0F, 1.0F, 1.0F}};
    mesh.backfaceCullingSafe = false;
    return mesh;
}

enum class FootprintSurface : std::uint8_t { Floor, Ceiling, Wall };

std::uint64_t footprintMeshKey(const std::span<const wkc::core::Vec2> outline,
    const wkc::core::Aabb bounds, const FootprintSurface surface,
    const std::span<const DoorRenderData> doors) noexcept
{
    std::uint64_t hash = wkc::core::hashCombine(
        0x524F4F4D5348454CULL, static_cast<std::uint64_t>(surface));
    const auto add = [&hash](const float value) {
        hash = wkc::core::hashCombine(hash, std::bit_cast<std::uint32_t>(value));
    };
    add(bounds.minimum.y);
    add(bounds.maximum.y);
    for (const auto point : outline) { add(point.x); add(point.y); }
    if (surface == FootprintSurface::Wall) {
        for (const auto& door : doors) {
            add(door.position.x); add(door.position.z);
            add(door.outwardNormal.x); add(door.outwardNormal.z);
            add(door.width); add(door.height);
        }
    }
    return hash;
}

float footprintCross(const wkc::core::Vec2 a, const wkc::core::Vec2 b,
    const wkc::core::Vec2 c) noexcept
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

std::optional<wkc::render::MeshData> makeFootprintMesh(
    const std::span<const wkc::core::Vec2> outline,
    const wkc::core::Aabb bounds, const FootprintSurface surface,
    const std::span<const DoorRenderData> doors)
{
    if (outline.size() < 3U) return std::nullopt;
    wkc::render::MeshData mesh;
    mesh.backfaceCullingSafe = false;
    const float floor = bounds.minimum.y - 0.02F;
    const float ceiling = bounds.maximum.y + 0.02F;
    mesh.bounds = {{bounds.minimum.x, floor, bounds.minimum.z},
        {bounds.maximum.x, ceiling, bounds.maximum.z}};
    float signedArea = 0.0F;
    for (std::size_t i = 0U; i < outline.size(); ++i) {
        const auto a = outline[i];
        const auto b = outline[(i + 1U) % outline.size()];
        signedArea += a.x * b.y - b.x * a.y;
    }
    const float orientation = signedArea >= 0.0F ? 1.0F : -1.0F;
    if (std::abs(signedArea) < 1.0e-4F) return std::nullopt;
    if (surface == FootprintSurface::Wall) {
        float distance = 0.0F;
        for (std::size_t i = 0U; i < outline.size(); ++i) {
            const auto a = outline[i];
            const auto b = outline[(i + 1U) % outline.size()];
            const float dx = b.x - a.x;
            const float dz = b.y - a.y;
            const float length = std::sqrt(dx * dx + dz * dz);
            if (length < 1.0e-4F) return std::nullopt;
            const wkc::core::Vec3 inward{
                -orientation * dz / length, 0.0F,
                orientation * dx / length};
            const auto addSpan = [&](const float start, const float end,
                                     const float bottom, const float top) {
                if (end - start < 1.0e-4F || top - bottom < 1.0e-4F) return;
                const auto point = [&](const float along, const float y) {
                    return wkc::core::Vec3{a.x + dx * along / length,
                        y, a.y + dz * along / length};
                };
                const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
                mesh.vertices.insert(mesh.vertices.end(), {
                    {point(start, bottom), inward,
                        {(distance + start) / 4.0F, (bottom - floor) / 4.0F}, 0U},
                    {point(end, bottom), inward,
                        {(distance + end) / 4.0F, (bottom - floor) / 4.0F}, 0U},
                    {point(end, top), inward,
                        {(distance + end) / 4.0F, (top - floor) / 4.0F}, 0U},
                    {point(start, top), inward,
                        {(distance + start) / 4.0F, (top - floor) / 4.0F}, 0U},
                });
                mesh.indices.insert(mesh.indices.end(), {
                    base, base + 1U, base + 2U,
                    base, base + 2U, base + 3U,
                });
            };
            const auto door = std::find_if(doors.begin(), doors.end(),
                [&](const DoorRenderData& candidate) {
                    const float along = ((candidate.position.x - a.x) * dx
                        + (candidate.position.z - a.y) * dz) / length;
                    const float offsetX = candidate.position.x
                        - (a.x + dx * along / length);
                    const float offsetZ = candidate.position.z
                        - (a.y + dz * along / length);
                    return wkc::core::dot(candidate.outwardNormal, inward) < -0.9F
                        && offsetX * offsetX + offsetZ * offsetZ < 0.04F
                        && along - candidate.width * 0.5F >= -1.0e-3F
                        && along + candidate.width * 0.5F <= length + 1.0e-3F;
                });
            if (door == doors.end()) {
                addSpan(0.0F, length, floor, ceiling);
            } else {
                const float along = ((door->position.x - a.x) * dx
                    + (door->position.z - a.y) * dz) / length;
                const float left = along - door->width * 0.5F;
                const float right = along + door->width * 0.5F;
                addSpan(0.0F, left, floor, ceiling);
                addSpan(left, right,
                    std::min(floor + door->height, ceiling), ceiling);
                addSpan(right, length, floor, ceiling);
            }
            distance += length;
        }
        return mesh;
    }
    const float y = surface == FootprintSurface::Floor ? floor : ceiling;
    const wkc::core::Vec3 normal = surface == FootprintSurface::Floor
        ? wkc::core::Vec3{0.0F, 1.0F, 0.0F}
        : wkc::core::Vec3{0.0F, -1.0F, 0.0F};
    for (const auto point : outline) {
        mesh.vertices.push_back({{point.x, y, point.y}, normal,
            {point.x / 4.0F, point.y / 4.0F}, 0U});
    }
    std::vector<std::uint32_t> remaining(outline.size());
    for (std::uint32_t i = 0U; i < remaining.size(); ++i) remaining[i] = i;
    while (remaining.size() > 2U) {
        bool clipped = false;
        for (std::size_t i = 0U; i < remaining.size(); ++i) {
            const auto prev = remaining[(i + remaining.size() - 1U) % remaining.size()];
            const auto curr = remaining[i];
            const auto next = remaining[(i + 1U) % remaining.size()];
            if (footprintCross(outline[prev], outline[curr], outline[next])
                    * orientation <= 1.0e-5F) continue;
            bool containsOther = false;
            for (const auto candidate : remaining) {
                if (candidate == prev || candidate == curr || candidate == next) continue;
                const auto p = outline[candidate];
                if (footprintCross(outline[prev], outline[curr], p) * orientation
                        >= -1.0e-5F
                    && footprintCross(outline[curr], outline[next], p) * orientation
                        >= -1.0e-5F
                    && footprintCross(outline[next], outline[prev], p) * orientation
                        >= -1.0e-5F) {
                    containsOther = true;
                    break;
                }
            }
            if (containsOther) continue;
            mesh.indices.insert(mesh.indices.end(), {prev, curr, next});
            remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(i));
            clipped = true;
            break;
        }
        if (!clipped) return std::nullopt;
    }
    return mesh;
}

bool referenceEnemyModelsEnabled() noexcept
{
    const char* const value = std::getenv("GDP_ENEMY_REFERENCE_MODELS");
    return value == nullptr || std::string_view{value} != "0";
}

std::optional<wkc::render::ImageData> loadVisualImage(
    const std::string_view fileName)
{
    const char* const basePath = SDL_GetBasePath();
    const std::filesystem::path base = basePath != nullptr
        ? std::filesystem::path{basePath} : std::filesystem::path{};
    const std::array candidates{
        base / "visual" / fileName,
        base / "../Resources/visual" / fileName,
        std::filesystem::path{"assets/visual"} / fileName,
    };
    for (const auto& path : candidates) {
        SDL_Surface* const source = SDL_LoadPNG(path.string().c_str());
        if (source == nullptr) continue;
        SDL_Surface* const rgba = SDL_ConvertSurface(source, SDL_PIXELFORMAT_RGBA32);
        SDL_DestroySurface(source);
        if (rgba == nullptr || rgba->w <= 0 || rgba->h <= 0) {
            if (rgba != nullptr) SDL_DestroySurface(rgba);
            return std::nullopt;
        }
        wkc::render::ImageData image;
        image.width = rgba->w;
        image.height = rgba->h;
        const std::size_t rowBytes = static_cast<std::size_t>(rgba->w) * 4U;
        image.rgba.resize(rowBytes * static_cast<std::size_t>(rgba->h));
        const auto* const pixels = static_cast<const std::uint8_t*>(rgba->pixels);
        for (int y = 0; y < rgba->h; ++y) {
            std::memcpy(image.rgba.data() + static_cast<std::size_t>(y) * rowBytes,
                pixels + static_cast<std::size_t>(y) * static_cast<std::size_t>(rgba->pitch),
                rowBytes);
        }
        SDL_DestroySurface(rgba);
        return image;
    }
    return std::nullopt;
}

bool referencePainted(const std::uint8_t material)
{
    return material == 0U;
}

wkc::core::Vec3 referenceMaterialColor(
    const EnemyVisualRole role, const std::uint8_t material)
{
    constexpr std::array<std::array<wkc::core::Vec3, 6U>, 5U> colors{{
        {{{0.85F, 0.39F, 0.10F}, {0.20F, 0.22F, 0.24F},
            {0.48F, 0.50F, 0.52F}, {1.0F, 0.32F, 0.03F},
            {0.09F, 0.10F, 0.11F}, {0.62F, 0.39F, 0.08F}}},
        {{{0.72F, 0.69F, 0.56F}, {0.19F, 0.20F, 0.19F},
            {0.48F, 0.48F, 0.44F}, {1.0F, 0.32F, 0.03F},
            {0.10F, 0.10F, 0.10F}, {0.34F, 0.37F, 0.28F}}},
        {{{0.38F, 0.13F, 0.19F}, {0.18F, 0.20F, 0.28F},
            {0.45F, 0.47F, 0.50F}, {1.0F, 0.32F, 0.03F},
            {0.09F, 0.10F, 0.12F}, {0.24F, 0.26F, 0.31F}}},
        {{{0.19F, 0.51F, 0.51F}, {0.83F, 0.36F, 0.10F},
            {0.48F, 0.50F, 0.51F}, {1.0F, 0.32F, 0.03F},
            {0.09F, 0.11F, 0.12F}, {0.25F, 0.32F, 0.33F}}},
        {{{0.85F, 0.39F, 0.10F}, {0.18F, 0.20F, 0.23F},
            {0.48F, 0.50F, 0.52F}, {1.0F, 0.32F, 0.03F},
            {0.09F, 0.10F, 0.12F}, {0.55F, 0.23F, 0.08F}}},
    }};
    const auto roleIndex = static_cast<std::size_t>(role);
    if (roleIndex >= colors.size() || material >= colors[0].size()) {
        return {1.0F, 1.0F, 1.0F};
    }
    return colors[roleIndex][material];
}

wkc::render::MeshData makeRoundedMesh(
    const std::uint32_t slices, const std::uint32_t stacks)
{
    constexpr float pi = std::numbers::pi_v<float>;
    wkc::render::MeshData mesh;
    mesh.vertices.reserve((slices + 1U) * (stacks + 1U));
    mesh.indices.reserve(slices * stacks * 6U);
    for (std::uint32_t row = 0U; row <= stacks; ++row) {
        const float latitude = pi * static_cast<float>(row)
            / static_cast<float>(stacks);
        const float height = std::cos(latitude);
        const float radius = std::sin(latitude);
        for (std::uint32_t column = 0U; column <= slices; ++column) {
            const float longitude = 2.0F * pi * static_cast<float>(column)
                / static_cast<float>(slices);
            const wkc::core::Vec3 normal{
                radius * std::cos(longitude), height,
                radius * std::sin(longitude)};
            mesh.vertices.push_back({normal * 0.5F, normal,
                {static_cast<float>(column) / static_cast<float>(slices),
                    static_cast<float>(row) / static_cast<float>(stacks)}, 0U});
        }
    }
    for (std::uint32_t row = 0U; row < stacks; ++row) {
        for (std::uint32_t column = 0U; column < slices; ++column) {
            const std::uint32_t top = row * (slices + 1U) + column;
            const std::uint32_t bottom = top + slices + 1U;
            mesh.indices.insert(mesh.indices.end(), {
                top, top + 1U, bottom,
                top + 1U, bottom + 1U, bottom});
        }
    }
    mesh.bounds = {{-0.5F, -0.5F, -0.5F}, {0.5F, 0.5F, 0.5F}};
    mesh.backfaceCullingSafe = true;
    return mesh;
}

wkc::render::MeshData makeCentralHubColumnShaftMesh()
{
    // A regular octagon inside the existing square collision shaft. Its
    // square plinth remains wider than every bevel at player height.
    constexpr float pi = std::numbers::pi_v<float>;
    const float radius = 0.5F / std::cos(pi / 8.0F);
    wkc::render::MeshData mesh;
    mesh.vertices.reserve(32U);
    mesh.indices.reserve(48U);
    for (std::uint32_t side = 0U; side < 8U; ++side) {
        const float angleA = pi / 8.0F + static_cast<float>(side) * pi / 4.0F;
        const float angleB = angleA + pi / 4.0F;
        const float faceAngle = (angleA + angleB) * 0.5F;
        const wkc::core::Vec3 normal{
            std::cos(faceAngle), 0.0F, std::sin(faceAngle)};
        const wkc::core::Vec3 a{radius * std::cos(angleA), 0.0F,
            radius * std::sin(angleA)};
        const wkc::core::Vec3 b{radius * std::cos(angleB), 0.0F,
            radius * std::sin(angleB)};
        const std::uint32_t first = static_cast<std::uint32_t>(mesh.vertices.size());
        const float tileU = std::hypot(b.x - a.x, b.z - a.z) * 2.4F;
        mesh.vertices.push_back({{a.x, -0.5F, a.z}, normal, {0.0F, 0.0F}, 0U});
        mesh.vertices.push_back({{a.x, 0.5F, a.z}, normal, {0.0F, 5.2F}, 0U});
        mesh.vertices.push_back({{b.x, -0.5F, b.z}, normal, {tileU, 0.0F}, 0U});
        mesh.vertices.push_back({{b.x, 0.5F, b.z}, normal, {tileU, 5.2F}, 0U});
        mesh.indices.insert(mesh.indices.end(), {
            first, first + 1U, first + 2U,
            first + 1U, first + 3U, first + 2U});
    }
    mesh.bounds = {{-0.55F, -0.5F, -0.55F}, {0.55F, 0.5F, 0.55F}};
    mesh.backfaceCullingSafe = true;
    return mesh;
}

wkc::render::MeshData makeCanisterCylinderMesh()
{
    // Faceted steel vessel, normalized to the same unit AABB as the prop's
    // existing collision volume. Flat normals keep its industrial facets.
    constexpr std::uint32_t sides = 12U;
    constexpr float pi = std::numbers::pi_v<float>;
    wkc::render::MeshData mesh;
    mesh.vertices.reserve(sides * 8U);
    mesh.indices.reserve(sides * 12U);
    for (std::uint32_t side = 0U; side < sides; ++side) {
        const float angleA = 2.0F * pi * static_cast<float>(side)
            / static_cast<float>(sides);
        const float angleB = 2.0F * pi * static_cast<float>(side + 1U)
            / static_cast<float>(sides);
        const float mid = (angleA + angleB) * 0.5F;
        const wkc::core::Vec3 normal{std::cos(mid), 0.0F, std::sin(mid)};
        const wkc::core::Vec3 a{0.5F * std::cos(angleA), 0.0F,
            0.5F * std::sin(angleA)};
        const wkc::core::Vec3 b{0.5F * std::cos(angleB), 0.0F,
            0.5F * std::sin(angleB)};
        const auto first = static_cast<std::uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back({{a.x, -0.5F, a.z}, normal, {0.0F, 0.0F}, 0U});
        mesh.vertices.push_back({{a.x, 0.5F, a.z}, normal, {0.0F, 1.0F}, 0U});
        mesh.vertices.push_back({{b.x, -0.5F, b.z}, normal, {1.0F, 0.0F}, 0U});
        mesh.vertices.push_back({{b.x, 0.5F, b.z}, normal, {1.0F, 1.0F}, 0U});
        mesh.indices.insert(mesh.indices.end(), {
            first, first + 1U, first + 2U,
            first + 1U, first + 3U, first + 2U});
        const auto cap = static_cast<std::uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back({{0.0F, 0.5F, 0.0F}, {0.0F, 1.0F, 0.0F},
            {0.5F, 0.5F}, 0U});
        mesh.vertices.push_back({{a.x, 0.5F, a.z}, {0.0F, 1.0F, 0.0F},
            {a.x + 0.5F, a.z + 0.5F}, 0U});
        mesh.vertices.push_back({{b.x, 0.5F, b.z}, {0.0F, 1.0F, 0.0F},
            {b.x + 0.5F, b.z + 0.5F}, 0U});
        mesh.vertices.push_back({{0.0F, -0.5F, 0.0F}, {0.0F, -1.0F, 0.0F},
            {0.5F, 0.5F}, 0U});
        mesh.vertices.push_back({{a.x, -0.5F, a.z}, {0.0F, -1.0F, 0.0F},
            {a.x + 0.5F, a.z + 0.5F}, 0U});
        mesh.vertices.push_back({{b.x, -0.5F, b.z}, {0.0F, -1.0F, 0.0F},
            {b.x + 0.5F, b.z + 0.5F}, 0U});
        mesh.indices.insert(mesh.indices.end(), {
            cap, cap + 2U, cap + 1U,
            cap + 3U, cap + 4U, cap + 5U});
    }
    mesh.bounds = {{-0.5F, -0.5F, -0.5F}, {0.5F, 0.5F, 0.5F}};
    mesh.backfaceCullingSafe = true;
    return mesh;
}

wkc::render::MeshData makeGrenadeArmorMesh()
{
    // Sixteen separate curved plates leave the dark housing visible at their
    // seams. One uploaded mesh keeps the in-flight grenade inexpensive.
    constexpr float pi = std::numbers::pi_v<float>;
    constexpr std::uint32_t platesAround = 8U;
    constexpr std::uint32_t subdivisions = 4U;
    wkc::render::MeshData mesh;
    mesh.vertices.reserve(2U * platesAround
        * (subdivisions + 1U) * (subdivisions + 1U));
    mesh.indices.reserve(2U * platesAround
        * subdivisions * subdivisions * 6U);
    for (std::uint32_t band = 0U; band < 2U; ++band) {
        const float top = band == 0U ? 0.34F : 1.60F;
        const float bottom = band == 0U ? 1.54F : 2.80F;
        for (std::uint32_t plate = 0U; plate < platesAround; ++plate) {
            const std::uint32_t first = static_cast<std::uint32_t>(mesh.vertices.size());
            for (std::uint32_t row = 0U; row <= subdivisions; ++row) {
                const float v = static_cast<float>(row) / subdivisions;
                const float latitude = top + (bottom - top) * v;
                const float edgeInset = (row == 0U || row == subdivisions)
                    ? 0.065F : 0.0F;
                for (std::uint32_t column = 0U; column <= subdivisions; ++column) {
                    const float u = static_cast<float>(column) / subdivisions;
                    const float longitude = (2.0F * pi / platesAround)
                        * (static_cast<float>(plate) + 0.04F + edgeInset
                            + u * (0.92F - 2.0F * edgeInset));
                    const wkc::core::Vec3 normal{
                        std::sin(latitude) * std::cos(longitude),
                        std::cos(latitude),
                        std::sin(latitude) * std::sin(longitude)};
                    mesh.vertices.push_back({normal * 0.525F, normal,
                        {u, v}, 0U});
                }
            }
            for (std::uint32_t row = 0U; row < subdivisions; ++row) {
                for (std::uint32_t column = 0U; column < subdivisions; ++column) {
                    const std::uint32_t a = first
                        + row * (subdivisions + 1U) + column;
                    const std::uint32_t b = a + subdivisions + 1U;
                    mesh.indices.insert(mesh.indices.end(), {
                        a, b, a + 1U, a + 1U, b, b + 1U});
                }
            }
        }
    }
    mesh.bounds = {{-0.525F, -0.525F, -0.525F},
        {0.525F, 0.525F, 0.525F}};
    // The first-person basis mirrors one axis relative to the world pass.
    // Two-sided panels keep the same mesh visible in both presentations.
    mesh.backfaceCullingSafe = false;
    return mesh;
}

wkc::render::MeshData makeGrenadeLightSlotsMesh()
{
    constexpr float pi = std::numbers::pi_v<float>;
    wkc::render::MeshData mesh;
    mesh.vertices.reserve(8U * 4U);
    mesh.indices.reserve(8U * 6U);
    for (std::uint32_t slot = 0U; slot < 8U; ++slot) {
        const std::uint32_t first = static_cast<std::uint32_t>(mesh.vertices.size());
        const float center = 2.0F * pi * static_cast<float>(slot) / 8.0F;
        for (std::uint32_t row = 0U; row < 2U; ++row) {
            const float latitude = row == 0U ? 1.50F : 1.64F;
            for (std::uint32_t column = 0U; column < 2U; ++column) {
                const float longitude = center + (column == 0U ? -0.034F : 0.034F);
                const wkc::core::Vec3 normal{
                    std::sin(latitude) * std::cos(longitude),
                    std::cos(latitude),
                    std::sin(latitude) * std::sin(longitude)};
                mesh.vertices.push_back({normal * 0.528F, normal,
                    {static_cast<float>(column), static_cast<float>(row)}, 0U});
            }
        }
        mesh.indices.insert(mesh.indices.end(), {
            first, first + 2U, first + 1U,
            first + 1U, first + 2U, first + 3U});
    }
    mesh.bounds = {{-0.53F, -0.53F, -0.53F},
        {0.53F, 0.53F, 0.53F}};
    mesh.backfaceCullingSafe = false;
    return mesh;
}

wkc::render::MeshData makeGrenadeRingMesh()
{
    constexpr float pi = std::numbers::pi_v<float>;
    constexpr std::uint32_t segments = 24U;
    constexpr std::uint32_t tubeSides = 6U;
    wkc::render::MeshData mesh;
    mesh.vertices.reserve((segments + 1U) * (tubeSides + 1U));
    mesh.indices.reserve(segments * tubeSides * 6U);
    for (std::uint32_t segment = 0U; segment <= segments; ++segment) {
        const float around = 2.0F * pi * static_cast<float>(segment) / segments;
        const float radialX = std::cos(around);
        const float radialZ = std::sin(around);
        for (std::uint32_t side = 0U; side <= tubeSides; ++side) {
            const float cross = 2.0F * pi * static_cast<float>(side) / tubeSides;
            const float tubeRadius = 0.038F;
            const float ringRadius = 0.52F + tubeRadius * std::cos(cross);
            const wkc::core::Vec3 normal{
                radialX * std::cos(cross), std::sin(cross),
                radialZ * std::cos(cross)};
            mesh.vertices.push_back({
                {radialX * ringRadius, tubeRadius * std::sin(cross),
                    radialZ * ringRadius},
                normal,
                {static_cast<float>(segment) / segments,
                    static_cast<float>(side) / tubeSides}, 0U});
        }
    }
    for (std::uint32_t segment = 0U; segment < segments; ++segment) {
        for (std::uint32_t side = 0U; side < tubeSides; ++side) {
            const std::uint32_t a = segment * (tubeSides + 1U) + side;
            const std::uint32_t b = a + tubeSides + 1U;
            mesh.indices.insert(mesh.indices.end(), {
                a, b, a + 1U, a + 1U, b, b + 1U});
        }
    }
    mesh.bounds = {{-0.56F, -0.04F, -0.56F},
        {0.56F, 0.04F, 0.56F}};
    mesh.backfaceCullingSafe = true;
    return mesh;
}

constexpr std::string_view uiVertexShader = R"glsl(#version 330 core
layout(location = 0) in vec3 aPosition;
uniform vec2 uViewport;
uniform vec2 uOrigin;
uniform vec2 uSize;
out vec2 vLocal;
void main() {
    vLocal = aPosition.xy;
    gl_Position = vec4((uOrigin + vLocal * uSize) / uViewport * 2.0 - 1.0, 0.0, 1.0);
}
)glsl";
constexpr std::string_view uiFragmentShader = R"glsl(#version 330 core
in vec2 vLocal;
uniform sampler2D uAtlas;
uniform vec2 uGlyphOrigin;
uniform vec2 uGlyphSize;
uniform int uUseFont;
uniform vec2 uViewport;
uniform vec3 uColor;
uniform float uAlpha;
uniform float uScopeRadius;
out vec4 fragmentColor;
void main() {
    if (uUseFont == 2) {
        // Pixel-space circle, independent of world scale/FOV and aspect ratio.
        vec2 p = gl_FragCoord.xy - uViewport * 0.5;
        float radius = length(p);
        float outside = smoothstep(uScopeRadius - 1.0, uScopeRadius + 1.0, radius);
        float rim = smoothstep(uScopeRadius - 6.0, uScopeRadius - 1.0, radius);
        float crosshair = 1.0 - smoothstep(0.45, 1.05, min(abs(p.x), abs(p.y)));
        float center = 1.0 - smoothstep(1.0, 1.8, radius);
        float coverage = max(max(outside, rim * 0.85), crosshair);
        fragmentColor = vec4(mix(uColor, vec3(0.83, 0.86, 0.85), center),
            uAlpha * max(coverage, center));
        return;
    }
    float coverage = uUseFont == 0 ? 1.0 : textureLod(uAtlas,
        uGlyphOrigin + vec2(vLocal.x, 1.0 - vLocal.y) * uGlyphSize, 0.0).r;
    fragmentColor = vec4(uColor, uAlpha * coverage);
}
)glsl";

constexpr std::string_view vertexShaderSource = R"glsl(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;
layout(location = 4) in mat4 aInstanceModel;

uniform mat4 uModel;
uniform int uInstanced;
uniform int uPlayerSkinning;
uniform mat4 uPlayerBones[36];
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vWorldPosition;
out vec3 vNormal;
out vec2 vUv;
out vec3 vSurfaceSize;

void main()
{
    mat4 model = uInstanced != 0 ? aInstanceModel : uModel;
    vec4 position = vec4(aPosition, 1.0);
    vec3 normal = aNormal;
    vUv = aUv;
    if (uPlayerSkinning != 0) {
        int joint = int(floor(aUv.x * 0.5));
        float weight = aUv.x - float(joint) * 2.0;
        mat4 skin = uPlayerBones[joint] * weight
            + uPlayerBones[int(aUv.y)] * (1.0 - weight);
        position = skin * position;
        normal = mat3(skin) * normal;
        if (uPlayerSkinning == 2) {
            int localJoint = joint % 18;
            if (localJoint < 2) {
                // Sleeves: circumference plus length, not a tiny repeated
                // planar checker that mipmaps into a smooth plastic cylinder.
                vUv = vec2(atan(aPosition.z, aPosition.x + 0.000001) / 6.283185 + 0.5,
                    -aPosition.y / 1.02);
            } else if (localJoint >= 3) {
                const float rootsX[5] = float[5](0.0385, 0.013, -0.013, -0.0365, 0.039);
                const float rootsY[5] = float[5](0.073, 0.078, 0.075, 0.065, 0.021);
                const float lengths[5] = float[5](0.064, 0.071, 0.066, 0.054, 0.056);
                int digit = (localJoint - 3) / 3;
                float mirror = joint < 18 ? 1.0 : -1.0;
                vec3 bind = aPosition - vec3(rootsX[digit] * mirror, rootsY[digit], 0.0) * 2.4;
                vUv = vec2(atan(bind.z, bind.x + 0.000001) / 6.283185 + 0.5,
                    bind.y / (lengths[digit] * 2.4));
            } else {
                vUv = vec2(aPosition.x / 0.22 + 0.5, aPosition.y / 0.44);
            }
        } else {
            vUv = vec2(aPosition.x * 2.0 + aPosition.z * 3.0, aPosition.y * 2.0);
        }
    }
    vec4 worldPosition = model * position;
    vWorldPosition = worldPosition.xyz;
    vNormal = normalize(mat3(model) * normal);
    vSurfaceSize = vec3(length(model[0].xyz), length(model[1].xyz),
        length(model[2].xyz));
    gl_Position = uProjection * uView * worldPosition;
}
)glsl";

constexpr std::string_view shadowDepthVertexShader = R"glsl(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 4) in mat4 aInstanceModel;
uniform mat4 uModel;
uniform int uInstanced;
uniform mat4 uView;
uniform mat4 uProjection;
out vec3 vWorldPosition;
void main() {
    mat4 model = uInstanced != 0 ? aInstanceModel : uModel;
    vec4 world = model * vec4(aPosition, 1.0);
    vWorldPosition = world.xyz;
    gl_Position = uProjection * uView * world;
}
)glsl";
constexpr std::string_view shadowDepthFragmentShader = R"glsl(#version 330 core
in vec3 vWorldPosition;
uniform vec3 uLightPosition;
uniform float uFarPlane;
void main() {
    gl_FragDepth = length(vWorldPosition - uLightPosition) / uFarPlane;
}
)glsl";

const std::string fragmentShaderSource = [] {
    std::string source{R"glsl(#version 330 core
#ifndef GDP_LOW_SPEC
#define GDP_LOW_SPEC 0
#endif

in vec3 vWorldPosition;
in vec3 vNormal;
in vec2 vUv;
in vec3 vSurfaceSize;

uniform sampler2D uAlbedo;
uniform sampler2D uNormalMap;
uniform sampler2D uMaterialMap;
uniform sampler2D uEmissionMap;
uniform sampler2D uTechnicalDetailAlbedo;
uniform vec3 uTint;
uniform float uOpacity;
uniform int uPlayerBodyFade;
uniform float uBodyViewportHeight;
uniform int uSoftEdges;
uniform float uUnlitMix;
uniform float uMaterialUnlitMix;
uniform vec3 uCameraPosition;
#if GDP_LOW_SPEC
const int MAX_LIGHTS = 4;
#else
const int MAX_LIGHTS = 16;
#endif
#ifndef GDP_MAX_SHADOW_OCCLUDERS
#define GDP_MAX_SHADOW_OCCLUDERS 8
#endif
uniform int uLightCount;
uniform vec3 uLightPositions[MAX_LIGHTS];
uniform vec3 uLightColors[MAX_LIGHTS];
uniform float uLightIntensities[MAX_LIGHTS];
uniform float uLightRanges[MAX_LIGHTS];
uniform vec3 uLightRoomMinima[MAX_LIGHTS];
uniform vec3 uLightRoomMaxima[MAX_LIGHTS];
uniform int uLightBoundedToRoom[MAX_LIGHTS];
uniform int uFlashlightEnabled;
uniform vec3 uFlashlightPosition;
uniform vec3 uFlashlightDirection;
const int MAX_SHADOW_OCCLUDERS = GDP_MAX_SHADOW_OCCLUDERS;
#if !GDP_LOW_SPEC
uniform int uShadowLightCount;
uniform samplerCube uShadowCubes[8];
uniform int uShadowCubeCount;
uniform vec3 uShadowCubeLightPositions[8];
uniform float uShadowCubeFarPlane;
uniform int uShadowOccluderCount;
uniform vec3 uShadowOccluderMinima[MAX_SHADOW_OCCLUDERS];
uniform vec3 uShadowOccluderMaxima[MAX_SHADOW_OCCLUDERS];
uniform int uShadowOccluderOctagonal[MAX_SHADOW_OCCLUDERS];
#endif
uniform vec3 uAmbientColor;
uniform float uAmbientIntensity;
const int MAX_AMBIENT_REGIONS = 16;
uniform int uAmbientRegionCount;
uniform vec3 uAmbientRegionMinima[MAX_AMBIENT_REGIONS];
uniform vec3 uAmbientRegionMaxima[MAX_AMBIENT_REGIONS];
uniform vec3 uAmbientRegionColors[MAX_AMBIENT_REGIONS];
uniform float uAmbientRegionIntensities[MAX_AMBIENT_REGIONS];
uniform vec2 uUvScale;
uniform vec2 uUvOffset;
uniform float uSurfaceTileMetres;
uniform int uMirrorSurfaceTile;
uniform int uRackPart;
uniform int uFurniturePart;
uniform int uStructuralSteelMaterial;
uniform float uPropIndirectFill;
uniform int uHubWallMaterial;
uniform int uCombatColumnMaterial;
uniform int uCombatRailMaterial;
uniform int uHubFloorMaterial;
uniform int uBossFloorMaterial;
uniform int uTechnicalSteelMaterial;
uniform int uHubPlinthContactShadow;
uniform vec3 uHubPlinthFootprint;
uniform int uSecurityFloorVariation;
uniform float uSecurityFloorSeed;
uniform int uCombatWallVariation;
uniform float uCombatWallSeed;
uniform float uEmissiveStrength;
uniform float uWorkerPaintWear;
uniform int uEyeHalo;
uniform int uDetailedMaterial;
uniform int uDetailedMaterialsEnabled;
uniform int uUseEmissionMap;
uniform int uViewmodel;
uniform vec3 uViewmodelLightDirection;

out vec4 fragmentColor;

mat3 cotangentFrame(vec3 normal, vec3 position, vec2 uv)
{
    vec3 positionX = dFdx(position);
    vec3 positionY = dFdy(position);
    vec2 uvX = dFdx(uv);
    vec2 uvY = dFdy(uv);
    vec3 perpendicularX = cross(positionY, normal);
    vec3 perpendicularY = cross(normal, positionX);
    vec3 tangent = perpendicularX * uvX.x + perpendicularY * uvY.x;
    vec3 bitangent = perpendicularX * uvX.y + perpendicularY * uvY.y;
    float scale = inversesqrt(max(max(dot(tangent, tangent), dot(bitangent, bitangent)), 1.0e-8));
    return mat3(tangent * scale, bitangent * scale, normal);
}

#if !GDP_LOW_SPEC
bool clipShadowDiagonal(
    float origin,
    float direction,
    float limit,
    inout float nearDistance,
    inout float farDistance)
{
    if (abs(direction) < 0.0001) {
        return abs(origin) <= limit;
    }
    float first = (-limit - origin) / direction;
    float second = (limit - origin) / direction;
    nearDistance = max(nearDistance, min(first, second));
    farDistance = min(farDistance, max(first, second));
    return farDistance >= nearDistance;
}

bool segmentIntersectsShadowBox(
    vec3 origin,
    vec3 direction,
    float maximumDistance,
    vec3 boundsMinimum,
    vec3 boundsMaximum,
    bool octagonal)
{
    vec3 safeDirection = vec3(
        abs(direction.x) < 0.0001 ? (direction.x < 0.0 ? -0.0001 : 0.0001)
                                  : direction.x,
        abs(direction.y) < 0.0001 ? (direction.y < 0.0 ? -0.0001 : 0.0001)
                                  : direction.y,
        abs(direction.z) < 0.0001 ? (direction.z < 0.0 ? -0.0001 : 0.0001)
                                  : direction.z);
    vec3 first = (boundsMinimum - origin) / safeDirection;
    vec3 second = (boundsMaximum - origin) / safeDirection;
    vec3 nearAxes = min(first, second);
    vec3 farAxes = max(first, second);
    float nearDistance = max(max(nearAxes.x, nearAxes.y), nearAxes.z);
    float farDistance = min(min(farAxes.x, farAxes.y), farAxes.z);
    if (farDistance < max(nearDistance, 0.0)
        || nearDistance >= maximumDistance - 0.025) {
        return false;
    }
    if (octagonal) {
        // The Hub shaft has four bevels. Its collision box must not cast a
        // square shadow across the open corners of the octagonal mesh.
        vec2 center = (boundsMinimum.xz + boundsMaximum.xz) * 0.5;
        vec2 halfSize = (boundsMaximum.xz - boundsMinimum.xz) * 0.5;
        vec2 localOrigin = origin.xz - center;
        float diagonalLimit = (halfSize.x + halfSize.y) * 0.70710678;
        if (!clipShadowDiagonal(localOrigin.x + localOrigin.y,
                direction.x + direction.z, diagonalLimit,
                nearDistance, farDistance)
            || !clipShadowDiagonal(localOrigin.x - localOrigin.y,
                direction.x - direction.z, diagonalLimit,
                nearDistance, farDistance)) {
            return false;
        }
    }
    return farDistance >= max(nearDistance, 0.0)
        && nearDistance > 0.025
        && nearDistance < maximumDistance - 0.025;
}

float directLightVisibility(
    vec3 position,
    vec3 normal,
    vec3 lightPosition,
    int lightIndex)
{
    for (int mapIndex = 0; mapIndex < uShadowCubeCount; ++mapIndex) {
        if (distance(lightPosition, uShadowCubeLightPositions[mapIndex]) >= 0.08) {
            continue;
        }
        vec3 fromLight = position - uShadowCubeLightPositions[mapIndex];
        float distanceToLight = length(fromLight);
        if (distanceToLight < 0.08 || distanceToLight >= uShadowCubeFarPlane) {
            return 1.0;
        }
        // The slope component reduces acne on the bevelled shaft and posed limbs.
        float bias = max(0.045, 0.12 * (1.0 - dot(normalize(normal),
            normalize(-fromLight))));
        // GLSL 3.30 requires a constant sampler-array index. The branches
        // select the map belonging to this light without a dynamic index.
        float nearest = 1.0;
        if (mapIndex == 0) nearest = texture(uShadowCubes[0], fromLight).r;
        else if (mapIndex == 1) nearest = texture(uShadowCubes[1], fromLight).r;
        else if (mapIndex == 2) nearest = texture(uShadowCubes[2], fromLight).r;
)glsl"};
    source += R"glsl(        else if (mapIndex == 3) nearest = texture(uShadowCubes[3], fromLight).r;
        else if (mapIndex == 4) nearest = texture(uShadowCubes[4], fromLight).r;
        else if (mapIndex == 5) nearest = texture(uShadowCubes[5], fromLight).r;
        else if (mapIndex == 6) nearest = texture(uShadowCubes[6], fromLight).r;
        else if (mapIndex == 7) nearest = texture(uShadowCubes[7], fromLight).r;
        nearest *= uShadowCubeFarPlane;
        return distanceToLight - bias > nearest ? 0.16 : 1.0;
    }
    if (lightIndex >= uShadowLightCount || uShadowOccluderCount <= 0) {
        return 1.0;
    }
    vec3 offset = lightPosition - position;
    float maximumDistance = length(offset);
    if (maximumDistance <= 0.05) {
        return 1.0;
    }
    vec3 origin = position + (normal * 0.035);
    vec3 direction = offset / maximumDistance;
    for (int index = 0; index < MAX_SHADOW_OCCLUDERS; ++index) {
        if (index >= uShadowOccluderCount) {
            break;
        }
        if (segmentIntersectsShadowBox(origin, direction, maximumDistance,
                uShadowOccluderMinima[index], uShadowOccluderMaxima[index],
                uShadowOccluderOctagonal[index] != 0)) {
            return 0.16;
        }
    }
    return 1.0;
}
#endif

void main()
{
    vec2 materialUv = (vUv * uUvScale) + uUvOffset;
    vec3 normal = normalize(vNormal);
    if (uSurfaceTileMetres > 0.0) {
        // Static architecture uses a physical tile size. This preserves
        // texel density on long counters and narrow posts alike.
        vec2 physicalUv = abs(normal.y) > 0.5
            ? vWorldPosition.xz / uSurfaceTileMetres
            : abs(normal.z) > 0.5
                ? vWorldPosition.xy / uSurfaceTileMetres
                : vWorldPosition.zy / uSurfaceTileMetres;
        // Mirroring is reserved for albedos whose opposite edges do not
        // meet exactly. Seamless tiles use GL_REPEAT directly: keeping the
        // unwrapped coordinates preserves derivatives and mipmap selection
        // across tile boundaries.
        materialUv = uMirrorSurfaceTile != 0
            ? clamp(1.0 - abs(mod(physicalUv, 2.0) - 1.0),
                vec2(0.002), vec2(0.998))
            : physicalUv;
    } else if (uMirrorSurfaceTile != 0) {
        // Shaped-room shell UVs already follow true edge length, including
        // diagonal faces. Mirror the panel at its borders without changing
        // that physical scale or restarting around a doorway.
        materialUv = clamp(1.0 - abs(mod(materialUv, 2.0) - 1.0),
            vec2(0.002), vec2(0.998));
    }
    vec3 albedo;
    if (uSecurityFloorVariation != 0) {
        // The 2 m base panel keeps its physical size. Mirroring scatters its
        // wear marks while preserving the direction of its panel seams.
        vec2 cell = floor(vWorldPosition.xz / 2.0);
        float choice = fract(sin(dot(cell, vec2(127.1, 311.7))
            + uSecurityFloorSeed) * 43758.5453);
        int orientation = min(int(choice * 4.0), 3);
        vec2 localUv = fract(materialUv);
        vec2 gradientX = dFdx(materialUv);
        vec2 gradientY = dFdy(materialUv);
        if ((orientation & 1) != 0) {
            localUv.x = 1.0 - localUv.x;
            gradientX.x = -gradientX.x;
            gradientY.x = -gradientY.x;
        }
        if ((orientation & 2) != 0) {
            localUv.y = 1.0 - localUv.y;
            gradientX.y = -gradientX.y;
            gradientY.y = -gradientY.y;
        }
        albedo = textureGrad(uAlbedo, localUv, gradientX, gradientY).rgb
            * uTint * (0.975 + choice * 0.05);
        // Localized wear is placed in larger world-space cells, independent
        // of the repeating panel bitmap. Sparse cells stay clear.
        vec2 wearGrid = vWorldPosition.xz / 3.7;
        vec2 wearCell = floor(wearGrid);
        float wearChoice = fract(sin(dot(wearCell, vec2(46.37, 92.11))
            + uSecurityFloorSeed * 0.031) * 17329.43);
        if (wearChoice > 0.57) {
            vec2 wearCenter = vec2(0.34 + fract(wearChoice * 7.3) * 0.32,
                0.34 + fract(wearChoice * 11.7) * 0.32);
            vec2 wearOffset = fract(wearGrid) - wearCenter;
            if (wearChoice > 0.79) wearOffset = wearOffset.yx;
            float wearRadius = length(wearOffset / vec2(0.17, 0.085));
            float wearFade = 1.0 - smoothstep(0.18, 1.0, wearRadius);
            float scratches = 0.56 + 0.44 * sin(dot(vWorldPosition.xz,
                vec2(54.0, 17.0)) + wearChoice * 25.0);
            albedo += vec3(0.055) * wearFade * scratches;
        }
    } else if (uCombatWallVariation != 0 && abs(normal.y) < 0.5) {
        // Each 6 m sheet samples the same plate atlas with an independent
        // orientation. Its 3 m panels also vary slightly in paint value, so
        // a long wall does not reveal a repeated block of four plates.
        vec2 wallPosition = abs(normal.z) > 0.5
            ? vWorldPosition.xy : vWorldPosition.zy;
        vec2 atlasPosition = wallPosition / 6.0;
        vec2 sheet = floor(atlasPosition);
        float choice = fract(sin(dot(sheet, vec2(127.1, 311.7))
            + uCombatWallSeed) * 43758.5453);
        int orientation = min(int(choice * 4.0), 3);
        vec2 localUv = fract(atlasPosition);
        vec2 gradientX = dFdx(atlasPosition);
        vec2 gradientY = dFdy(atlasPosition);
        if ((orientation & 1) != 0) {
            localUv.x = 1.0 - localUv.x;
            gradientX.x = -gradientX.x;
            gradientY.x = -gradientY.x;
        }
        if ((orientation & 2) != 0) {
            localUv.y = 1.0 - localUv.y;
            gradientX.y = -gradientX.y;
        }
        vec2 plate = floor(wallPosition / 3.0);
        float paint = fract(sin(dot(plate, vec2(46.37, 92.11))
            + uCombatWallSeed * 0.031) * 17329.43);
        albedo = textureGrad(uAlbedo, localUv, gradientX, gradientY).rgb
            * uTint * (0.88 + paint * 0.24);
    } else {
        albedo = texture(uAlbedo, materialUv).rgb * uTint;
    }
    if (uTechnicalSteelMaterial != 0 && uDetailedMaterialsEnabled != 0) {
        // The large plate atlas supplies joints and room-scale variation;
        // a separately mipmapped source retains fine brushed-metal detail
        // without a 12k-wide atlas or nearest-neighbour aliasing.
        vec2 detailUv = uTechnicalSteelMaterial == 2
            ? vWorldPosition.xz / 4.0
            : (abs(normal.x) > abs(normal.z)
                ? vec2(vWorldPosition.z, vWorldPosition.y) / 4.0
                : vec2(vWorldPosition.x, vWorldPosition.y) / 4.0);
        vec3 detail = texture(uTechnicalDetailAlbedo, detailUv).rgb;
        vec3 neutral = uTechnicalSteelMaterial == 2
            ? vec3(0.19, 0.19, 0.19)
            : vec3(0.463, 0.404, 0.345);
        albedo += (detail - neutral) * (uTechnicalSteelMaterial == 2 ? 0.9 : 0.75)
            * smoothstep(0.07, 0.20, max(max(albedo.r, albedo.g), albedo.b));
    }
    if (uFurniturePart == 4 && abs(normal.y) < 0.5) {
        // Cabinet joints and inset pulls are sized in metres. They remain
        // aligned on a long counter while its albedo repeats independently.
        float along = abs(normal.x) > 0.5
            ? vWorldPosition.z : vWorldPosition.x;
        float panel = fract(along / 0.86);
        float seamDistance = min(panel, 1.0 - panel);
        float seam = 1.0 - smoothstep(0.0, 0.012, seamDistance);
        albedo *= 1.0 - seam * 0.25;
        float handleHeight = min(abs(vWorldPosition.y - 0.86),
            abs(vWorldPosition.y - 1.94));
        float handle = (1.0 - smoothstep(0.012, 0.045,
                abs(panel - 0.76)))
            * (1.0 - smoothstep(0.012, 0.040, handleHeight));
        albedo = mix(albedo, vec3(0.12, 0.12, 0.11), handle * 0.68);
    }
    if (uRackPart == 1) {
        // Match Storage uprights to the graphite-green baseboard while
        // retaining scratches, recesses and wear from the approved texture.
        float paintValue = dot(albedo, vec3(0.30, 0.59, 0.11));
        albedo = vec3(0.35, 0.40, 0.35)
            * clamp(paintValue / 0.46, 0.08, 1.45);
    }
    if (uRackPart == 1 && abs(normal.y) < 0.5) {
        // Upright corners lose paint first. Keep chips in physical-space
        // bands so the damage reads on a narrow post without stretching.
        float corner = 1.0 - smoothstep(0.02, 0.25,
            min(vUv.x, 1.0 - vUv.x));
        float chipCell = floor(vWorldPosition.y * 12.0);
        float postCell = floor((vWorldPosition.x + vWorldPosition.z) * 9.0);
        float chipNoise = fract(sin(dot(vec2(chipCell, postCell),
            vec2(127.1, 311.7))) * 43758.5453);
        float exposedEdge = corner * smoothstep(0.45, 0.82, chipNoise);
        albedo *= 1.0 - corner * 0.055;
        albedo = mix(albedo, vec3(0.29, 0.28, 0.25),
            exposedEdge * 0.45);
        // These are shaded adjustment recesses, not marks baked into a
        // stretched bitmap. Whole rows fit between the post's end caps.
        float rows = max(1.0, floor(vSurfaceSize.y / 0.18 + 0.05));
        vec2 cell = abs(fract(vec2(vUv.x, vUv.y * rows)) - 0.5);
        float edge = max(cell.x / 0.18, cell.y / 0.17);
        float aa = max(fwidth(edge), 0.015);
        float recess = 1.0 - smoothstep(0.96 - aa, 1.04 + aa, edge);
        float rim = (1.0 - smoothstep(1.10 - aa, 1.24 + aa, edge))
            * (1.0 - recess);
        albedo = mix(albedo, vec3(0.035, 0.039, 0.038), recess * 0.92);
        albedo += rim * vec3(0.075, 0.047, 0.027);
    } else if (uRackPart == 2 && normal.y > 0.5) {
        float ribs = max(1.0, floor(vSurfaceSize.z / 0.18 + 0.05));
        float ribUv = vUv.y * ribs;
        float phase = fract(ribUv);
        float ridge = smoothstep(0.08, 0.23, phase)
            * (1.0 - smoothstep(0.77, 0.92, phase));
        float visibility = 1.0 - smoothstep(0.22, 0.7, fwidth(ribUv));
        albedo *= 1.0 - (1.0 - ridge) * visibility * 0.20;
        normal = normalize(normal + vec3(0.0, 0.0,
            cos(phase * 6.2831853) * visibility * 0.16));
    }
    if (uWorkerPaintWear > 0.0) {
        // Exposed steel is concentrated along painted armor edges. The chip
        // pattern is stable in UV space and applies only to Worker paint.
        vec2 edgeDistance = min(vUv, vec2(1.0) - vUv);
        float edge = 1.0 - smoothstep(0.012, 0.13,
            min(edgeDistance.x, edgeDistance.y));
        float chipNoise = fract(sin(dot(floor(vUv * 64.0),
            vec2(127.1, 311.7))) * 43758.5453);
        float chips = smoothstep(0.37, 0.76, chipNoise);
        albedo *= 1.0 - edge * 0.32;
        float exposed = edge * chips * uWorkerPaintWear;
        albedo = mix(albedo, vec3(0.38, 0.41, 0.43), exposed);
    }
    float roughness = 0.72;
    float metalness = 0.0;
    float ambientOcclusion = 1.0;
)glsl";
    source += R"glsl(    if (uRackPart != 0) {
        roughness = 0.58;
        metalness = 0.12;
    } else if (uFurniturePart != 0) {
        // Bare brushed steel reflects the room lamps most strongly. Painted
        // metal retains a wider, weaker sheen instead of looking like plastic.
        roughness = uFurniturePart == 1 ? 0.38
            : uFurniturePart == 2 ? 0.59 : 0.65;
        metalness = uFurniturePart == 1 ? 0.52
            : uFurniturePart == 2 ? 0.12 : 0.18;
    } else if (uStructuralSteelMaterial != 0) {
        // Painted structural steel is less glossy than exposed fittings, but
        // should still catch the room lamps instead of reading as flat stone.
        roughness = uDetailedMaterialsEnabled != 0 ? 0.46 : 0.61;
        metalness = uDetailedMaterialsEnabled != 0 ? 0.26 : 0.14;
    } else if (uCombatColumnMaterial != 0) {
        // Dark brushed Arena steel has narrower lamp glints than the painted
        // walls, while its diffuse face stays close to the other supports.
        roughness = uDetailedMaterialsEnabled != 0 ? 0.30 : 0.46;
        metalness = uDetailedMaterialsEnabled != 0 ? 0.56 : 0.35;
    } else if (uCombatRailMaterial != 0) {
        roughness = uDetailedMaterialsEnabled != 0 ? 0.35 : 0.50;
        metalness = uDetailedMaterialsEnabled != 0 ? 0.46 : 0.28;
    } else if (uHubWallMaterial != 0) {
        // Dark painted steel keeps a broad lamp reflection on every preset.
        roughness = uDetailedMaterialsEnabled != 0 ? 0.43 : 0.60;
        metalness = uDetailedMaterialsEnabled != 0 ? 0.34 : 0.20;
    } else if (uHubFloorMaterial != 0) {
        // Warm brushed steel has a softer lamp reflection than the earlier
        // cool graphite floor, while remaining legible on lower presets.
        roughness = uBossFloorMaterial != 0
            ? (uDetailedMaterialsEnabled != 0 ? 0.28 : 0.48)
            : (uDetailedMaterialsEnabled != 0 ? 0.39 : 0.56);
        metalness = uBossFloorMaterial != 0
            ? (uDetailedMaterialsEnabled != 0 ? 0.52 : 0.27)
            : (uDetailedMaterialsEnabled != 0 ? 0.30 : 0.18);
    } else if (uTechnicalSteelMaterial != 0) {
        // Broad painted wall sheets and bare floor plates share the room
        // lamps, with a soft sheen that survives lower presets.
        roughness = uTechnicalSteelMaterial == 2
            ? (uDetailedMaterialsEnabled != 0 ? 0.38 : 0.55)
            : 0.62;
        metalness = uTechnicalSteelMaterial == 2 ? 0.34 : 0.10;
    }
#if !GDP_LOW_SPEC
    if (uDetailedMaterial != 0 && uDetailedMaterialsEnabled != 0) {
        vec3 tangentNormal = texture(uNormalMap, materialUv).xyz * 2.0 - 1.0;
        normal = normalize(cotangentFrame(normal, vWorldPosition, materialUv) * tangentNormal);
        vec3 material = texture(uMaterialMap, materialUv).rgb;
        roughness = clamp(material.r, 0.08, 1.0);
        metalness = clamp(material.g, 0.0, 1.0);
        ambientOcclusion = clamp(material.b, 0.0, 1.0);
    }
#endif
    vec3 roomAmbientColor = uAmbientColor;
    float roomAmbientIntensity = uAmbientIntensity;
    if (uViewmodel == 0) {
        for (int index = 0; index < MAX_AMBIENT_REGIONS; ++index) {
            if (index >= uAmbientRegionCount) {
                break;
            }
            if (vWorldPosition.x >= uAmbientRegionMinima[index].x
                && vWorldPosition.x <= uAmbientRegionMaxima[index].x
                && vWorldPosition.z >= uAmbientRegionMinima[index].z
                && vWorldPosition.z <= uAmbientRegionMaxima[index].z) {
                roomAmbientColor = uAmbientRegionColors[index];
                roomAmbientIntensity = uAmbientRegionIntensities[index];
                break;
            }
        }
    }
    vec3 result = albedo * roomAmbientColor * roomAmbientIntensity
        * ambientOcclusion;
    vec3 viewDirection = normalize(uCameraPosition - vWorldPosition);
    if (uFurniturePart == 1) {
        // The room has no reflection probe. A broad ceiling-coloured response
        // keeps brushed steel readable between point lights without making it
        // emissive; the sharper reflections below still track live lights.
        float grazing = 1.0 - max(dot(normal, viewDirection), 0.0);
        result += albedo * roomAmbientColor * roomAmbientIntensity
            * (0.15 + 0.65 * grazing * grazing);
    }
    for (int index = 0; index < MAX_LIGHTS; ++index) {
        if (index >= uLightCount) {
            break;
        }
        if (uLightIntensities[index] <= 0.0001) {
            continue;
        }
        float roomReach = 1.0;
        if (uViewmodel == 0 && uLightBoundedToRoom[index] != 0) {
            // Neighbouring fixtures light their own room and the first part
            // of a doorway, not surfaces deep behind another room's wall.
            vec2 outside = max(max(uLightRoomMinima[index].xz - vWorldPosition.xz,
                vWorldPosition.xz - uLightRoomMaxima[index].xz), vec2(0.0));
            roomReach = 1.0 - smoothstep(0.0, 1.2, length(outside));
            if (roomReach <= 0.0001) {
                continue;
            }
        }
        vec3 toLight = uLightPositions[index] - vWorldPosition;
        float distanceToLight = length(toLight);
        vec3 lightDirection = normalize(toLight);
        float diffuse = max(dot(normal, lightDirection), 0.0);
        float normalizedDistance = distanceToLight / max(uLightRanges[index], 0.001);
        float attenuation = 1.0 / (1.0 + 2.2 * normalizedDistance * normalizedDistance);
        vec3 radiance = uLightColors[index] * attenuation
            * uLightIntensities[index] * roomReach * 1.9;
        float visibility = 1.0;
#if !GDP_LOW_SPEC
        visibility = diffuse > 0.0
            ? directLightVisibility(
                vWorldPosition, normal, uLightPositions[index], index)
            : 1.0;
#endif
        result += albedo * (1.0 - metalness) * diffuse * radiance * visibility;
        if ((uTechnicalSteelMaterial == 2 || uBossFloorMaterial != 0)
            && uDetailedMaterialsEnabled != 0
            && diffuse > 0.0) {
            // A narrow lamp footprint gives the broad graphite plates a
            // readable metallic response without a costly reflection probe.
            // It follows each live fixture and its shadow visibility.
            vec2 viewAxis = normalize(viewDirection.xz + vec2(0.0001));
            vec2 lampOffset = uLightPositions[index].xz - vWorldPosition.xz;
            float across = dot(lampOffset, vec2(-viewAxis.y, viewAxis.x));
            float along = dot(lampOffset, viewAxis);
            float footprint = exp(-0.5 * (across * across / 1.2
                + along * along / 64.0));
            result += radiance * footprint * visibility * diffuse
                * (uBossFloorMaterial != 0 ? 0.19 : 0.30);
        }
        if (uPropIndirectFill > 0.0 && uViewmodel == 0) {
            // Approximate nearby floor/wall bounce on vertical prop faces.
            // Every surviving fixture contributes independently.
            float sideFace = 1.0 - abs(normal.y);
            float bounce = uPropIndirectFill * sideFace * (1.0 - diffuse);
            result += albedo * (1.0 - metalness) * radiance * bounce;
        }
#if GDP_LOW_SPEC
        if ((uFurniturePart != 0 || uStructuralSteelMaterial != 0
                || uHubFloorMaterial != 0
                || uTechnicalSteelMaterial != 0
                || uHubWallMaterial != 0)
            && diffuse > 0.0) {
#else
        if (((uDetailedMaterial != 0 && uDetailedMaterialsEnabled != 0)
                || uFurniturePart != 0 || uStructuralSteelMaterial != 0
                || uHubFloorMaterial != 0
                || uTechnicalSteelMaterial != 0
                || uHubWallMaterial != 0)
            && diffuse > 0.0) {
#endif
            vec3 halfway = normalize(lightDirection + viewDirection);
            float exponent = (uHubFloorMaterial != 0
                    || uTechnicalSteelMaterial == 2)
                ? mix(11.0, 3.0, roughness)
                : (uFurniturePart != 0 || uStructuralSteelMaterial != 0
                    || uTechnicalSteelMaterial != 0
                    || uHubWallMaterial != 0)
                    ? mix(44.0, 7.0, roughness)
                    : mix(96.0, 8.0, roughness);
            // POTATO keeps one broad reflection per room lamp. Use the same
            // lobe when a shadow occluder selects the full shader path.
            if (uDetailedMaterialsEnabled == 0) exponent *= 0.65;
            float highlightStrength = uHubFloorMaterial != 0
                ? (uBossFloorMaterial != 0
                    ? mix(4.0, 0.65, roughness)
                    : mix(2.8, 0.50, roughness))
                : uCombatColumnMaterial != 0
                    ? mix(2.7, 0.40, roughness)
                : uTechnicalSteelMaterial == 2
                    ? mix(3.4, 0.55, roughness)
                : (uFurniturePart != 0 || uStructuralSteelMaterial != 0
                    || uTechnicalSteelMaterial != 0
                    || uHubWallMaterial != 0)
                    ? mix(2.0, 0.20, roughness)
                    : mix(0.90, 0.12, roughness);
            float highlight = pow(max(dot(normal, halfway), 0.0), exponent)
                * highlightStrength;
            vec3 reflectance = mix(vec3(0.035), albedo, metalness);
            result += reflectance * highlight * diffuse * radiance * visibility;
        }
    }
    if (uFlashlightEnabled != 0 && uViewmodel == 0) {
        vec3 beam = vWorldPosition - uFlashlightPosition;
        float beamDistance = length(beam);
        if (beamDistance > 0.001 && beamDistance < 14.0) {
            vec3 beamDirection = beam / beamDistance;
            float cone = smoothstep(0.82, 0.95,
                dot(beamDirection, normalize(uFlashlightDirection)));
            float incidence = max(dot(normal, -beamDirection), 0.0);
            float falloff = 1.0 / (1.0 + 0.05 * beamDistance * beamDistance);
            float rangeFade = 1.0 - smoothstep(11.0, 14.0, beamDistance);
            result += albedo * vec3(0.90, 0.95, 1.0)
                * cone * incidence * falloff * rangeFade * 5.5;
            if ((uFurniturePart != 0 || uStructuralSteelMaterial != 0
                    || uHubFloorMaterial != 0
                    || uTechnicalSteelMaterial != 0
                    || uHubWallMaterial != 0)
                && incidence > 0.0) {
                vec3 halfway = normalize(-beamDirection + viewDirection);
                float highlight = pow(max(dot(normal, halfway), 0.0),
                    mix(38.0, 7.0, roughness));
                result += mix(vec3(0.035), albedo, metalness)
                    * highlight * cone * incidence * falloff * rangeFade * 2.0;
            }
        }
    }
    if (uHubFloorMaterial != 0 && uHubPlinthContactShadow != 0
        && normal.y > 0.7) {
        vec2 outside = max(abs(vWorldPosition.xz - uHubPlinthFootprint.xz)
            - vec2(uHubPlinthFootprint.y), vec2(0.0));
        float contact = 1.0 - smoothstep(0.0, 0.55, length(outside));
        result *= 1.0 - (0.38 * contact);
    }
    if (uViewmodel != 0) {
        // A restrained camera-space reflection keeps steel readable when the
        // room lights are behind the player. It affects no world surfaces.
        vec3 fillDirection = normalize(uViewmodelLightDirection);
        float fill = max(dot(normal, fillDirection), 0.0);
        result += albedo * (0.14 + 0.29 * fill);
#if !GDP_LOW_SPEC
        if (uDetailedMaterial != 0 && uDetailedMaterialsEnabled != 0) {
            vec3 halfway = normalize(fillDirection + viewDirection);
            float reflection = pow(max(dot(normal, halfway), 0.0),
                mix(30.0, 6.0, roughness));
            result += mix(vec3(0.035), albedo, metalness)
                * reflection * (1.0 - roughness) * 1.15;
        }
#endif
    }
    float emissiveMask = uUseEmissionMap != 0
        ? texture(uEmissionMap, materialUv).r
        : smoothstep(0.62, 0.82, max(albedo.r, max(albedo.g, albedo.b)));
    float edgeOpacity = uSoftEdges != 0
        ? smoothstep(0.05, 0.75, max(dot(normal, viewDirection), 0.0))
        : 1.0;
    vec3 litColor = result + (albedo * emissiveMask * uEmissiveStrength);
    float haloOpacity = uEyeHalo != 0
        ? 1.0 - smoothstep(0.05, 0.5, length(vUv - vec2(0.5)))
        : 1.0;
    float bodyOpacity = uPlayerBodyFade != 0
        ? 1.0 - smoothstep(0.23, 0.54, gl_FragCoord.y / uBodyViewportHeight)
        : 1.0;
    fragmentColor = vec4(mix(litColor,
        mix(uTint, albedo, uMaterialUnlitMix), uUnlitMix),
        uOpacity * edgeOpacity * haloOpacity * bodyOpacity);
}
)glsl";
    return source;
}();

std::string makeLowSpecFragmentShaderSource()
{
    std::string source{fragmentShaderSource};
    constexpr std::string_view versionLine{"#version 330 core\n"};
    source.insert(versionLine.size(), "#define GDP_LOW_SPEC 1\n");
    return source;
}

std::string makeShadowBudgetFragmentShaderSource(const std::uint32_t capacity)
{
    std::string source{fragmentShaderSource};
    constexpr std::string_view versionLine{"#version 330 core\n"};
    source.insert(versionLine.size(),
        "#define GDP_MAX_SHADOW_OCCLUDERS " + std::to_string(capacity) + "\n");
    return source;
}

enum class RoomMaterialChannel : std::uint8_t {
    Wall,
    Floor,
    HubFloorDetail,
    Ceiling,
    CombatCover,
    CombatColumn,
    CombatRail,
    StorageRack,
    CafeteriaFurniture,
    LaboratoryFurniture,
    SecurityFurniture,
    TechnicalFurniture,
    StructuralColumn,
    SolidColor,
    Generic,
};

using Glyph = std::array<std::uint8_t, 7U>;

constexpr Glyph glyphFor(const std::uint32_t character) noexcept
{
    switch (character) {
    case '0': return {0x0EU, 0x11U, 0x13U, 0x15U, 0x19U, 0x11U, 0x0EU};
    case '1': return {0x04U, 0x0CU, 0x04U, 0x04U, 0x04U, 0x04U, 0x0EU};
    case '2': return {0x0EU, 0x11U, 0x01U, 0x02U, 0x04U, 0x08U, 0x1FU};
    case '3': return {0x1EU, 0x01U, 0x01U, 0x0EU, 0x01U, 0x01U, 0x1EU};
    case '4': return {0x02U, 0x06U, 0x0AU, 0x12U, 0x1FU, 0x02U, 0x02U};
    case '5': return {0x1FU, 0x10U, 0x10U, 0x1EU, 0x01U, 0x01U, 0x1EU};
    case '6': return {0x0EU, 0x10U, 0x10U, 0x1EU, 0x11U, 0x11U, 0x0EU};
    case '7': return {0x1FU, 0x01U, 0x02U, 0x04U, 0x08U, 0x08U, 0x08U};
    case '8': return {0x0EU, 0x11U, 0x11U, 0x0EU, 0x11U, 0x11U, 0x0EU};
    case '9': return {0x0EU, 0x11U, 0x11U, 0x0FU, 0x01U, 0x01U, 0x0EU};
    case 'A': return {0x0EU, 0x11U, 0x11U, 0x1FU, 0x11U, 0x11U, 0x11U};
    case 'B': return {0x1EU, 0x11U, 0x11U, 0x1EU, 0x11U, 0x11U, 0x1EU};
    case 'C': return {0x0FU, 0x10U, 0x10U, 0x10U, 0x10U, 0x10U, 0x0FU};
    case 'D': return {0x1EU, 0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x1EU};
    case 'E': return {0x1FU, 0x10U, 0x10U, 0x1EU, 0x10U, 0x10U, 0x1FU};
    case 'F': return {0x1FU, 0x10U, 0x10U, 0x1EU, 0x10U, 0x10U, 0x10U};
    case 'G': return {0x0FU, 0x10U, 0x10U, 0x17U, 0x11U, 0x11U, 0x0FU};
    case 'H': return {0x11U, 0x11U, 0x11U, 0x1FU, 0x11U, 0x11U, 0x11U};
    case 'I': return {0x1FU, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U, 0x1FU};
    case 'J': return {0x07U, 0x02U, 0x02U, 0x02U, 0x12U, 0x12U, 0x0CU};
    case 'K': return {0x11U, 0x12U, 0x14U, 0x18U, 0x14U, 0x12U, 0x11U};
    case 'L': return {0x10U, 0x10U, 0x10U, 0x10U, 0x10U, 0x10U, 0x1FU};
    case 'M': return {0x11U, 0x1BU, 0x15U, 0x15U, 0x11U, 0x11U, 0x11U};
    case 'N': return {0x11U, 0x19U, 0x19U, 0x15U, 0x13U, 0x13U, 0x11U};
    case 'O': return {0x0EU, 0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x0EU};
    case 'P': return {0x1EU, 0x11U, 0x11U, 0x1EU, 0x10U, 0x10U, 0x10U};
    case 'Q': return {0x0EU, 0x11U, 0x11U, 0x11U, 0x15U, 0x12U, 0x0DU};
    case 'R': return {0x1EU, 0x11U, 0x11U, 0x1EU, 0x14U, 0x12U, 0x11U};
    case 'S': return {0x0FU, 0x10U, 0x10U, 0x0EU, 0x01U, 0x01U, 0x1EU};
    case 'T': return {0x1FU, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U};
    case 'U': return {0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x0EU};
    case 'V': return {0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x0AU, 0x04U};
    case 'W': return {0x11U, 0x11U, 0x11U, 0x15U, 0x15U, 0x15U, 0x0AU};
    case 'X': return {0x11U, 0x11U, 0x0AU, 0x04U, 0x0AU, 0x11U, 0x11U};
    case 'Y': return {0x11U, 0x11U, 0x0AU, 0x04U, 0x04U, 0x04U, 0x04U};
    case 'Z': return {0x1FU, 0x01U, 0x02U, 0x04U, 0x08U, 0x10U, 0x1FU};
    case '-': return {0x00U, 0x00U, 0x00U, 0x0EU, 0x00U, 0x00U, 0x00U};
    case '/': return {0x01U, 0x02U, 0x02U, 0x04U, 0x08U, 0x08U, 0x10U};
    case ':': return {0x00U, 0x04U, 0x04U, 0x00U, 0x04U, 0x04U, 0x00U};
    case '.': return {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x06U, 0x06U};
    case '%': return {0x11U, 0x02U, 0x04U, 0x08U, 0x11U, 0x00U, 0x00U};
    case '<': return {0x01U, 0x02U, 0x04U, 0x08U, 0x04U, 0x02U, 0x01U};
    case '>': return {0x10U, 0x08U, 0x04U, 0x02U, 0x04U, 0x08U, 0x10U};
    case '[': return {0x0EU, 0x08U, 0x08U, 0x08U, 0x08U, 0x08U, 0x0EU};
    case ']': return {0x0EU, 0x02U, 0x02U, 0x02U, 0x02U, 0x02U, 0x0EU};
    case '+': return {0x00U, 0x04U, 0x04U, 0x1FU, 0x04U, 0x04U, 0x00U};
    case '=': return {0x00U, 0x00U, 0x1FU, 0x00U, 0x1FU, 0x00U, 0x00U};
    case '_': return {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x1FU};
    case '?': return {0x0EU, 0x11U, 0x01U, 0x02U, 0x04U, 0x00U, 0x04U};
    case U'Á': case U'á': return {0x04U, 0x08U, 0x0EU, 0x11U, 0x1FU, 0x11U, 0x11U};
    case U'À': case U'à': return {0x08U, 0x04U, 0x0EU, 0x11U, 0x1FU, 0x11U, 0x11U};
    case U'Â': case U'â': return {0x04U, 0x0AU, 0x0EU, 0x11U, 0x1FU, 0x11U, 0x11U};
    case U'Ã': case U'ã': return {0x0AU, 0x14U, 0x0EU, 0x11U, 0x1FU, 0x11U, 0x11U};
    case U'É': case U'é':
        return {0x04U, 0x08U, 0x1FU, 0x10U, 0x1EU, 0x10U, 0x1FU};
    case U'Ê': case U'ê': return {0x04U, 0x0AU, 0x1FU, 0x10U, 0x1EU, 0x10U, 0x1FU};
    case U'Í': case U'í': return {0x02U, 0x04U, 0x0EU, 0x04U, 0x04U, 0x04U, 0x0EU};
    case U'Ó': case U'ó': return {0x04U, 0x08U, 0x0EU, 0x11U, 0x11U, 0x11U, 0x0EU};
    case U'Ô': case U'ô': return {0x04U, 0x0AU, 0x0EU, 0x11U, 0x11U, 0x11U, 0x0EU};
    case U'Õ': case U'õ': return {0x0AU, 0x14U, 0x0EU, 0x11U, 0x11U, 0x11U, 0x0EU};
    case U'Ú': case U'ú': return {0x04U, 0x08U, 0x11U, 0x11U, 0x11U, 0x11U, 0x0EU};
    case U'Ç': case U'ç': return {0x0FU, 0x10U, 0x10U, 0x10U, 0x0FU, 0x04U, 0x08U};
    default: return {};
    }
}

[[maybe_unused]] int bitmapTextWidth(const std::string_view text, const int scale) noexcept
{
    return static_cast<int>(uiTextLength(text)) * 6 * scale;
}

void drawSolidRect(
    const int x,
    const int y,
    const int width,
    const int height,
    const wkc::core::Vec3 color)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    glClearColor(color.x, color.y, color.z, 1.0F);
    glScissor(x, y, width, height);
    glClear(GL_COLOR_BUFFER_BIT);
}

void drawBitmapText(
    const int x,
    const int y,
    const int scale,
    const std::string_view text,
    const wkc::core::Vec3 color)
{
    glClearColor(color.x, color.y, color.z, 1.0F);
    int cursor = x;
    for (std::size_t offset = 0U; offset < text.size();) {
        const auto character = nextUiCodepoint(text, offset);
        const Glyph glyph = glyphFor(character);
        for (std::size_t row = 0; row < glyph.size(); ++row) {
            for (int column = 0; column < 5; ++column) {
                if ((glyph[row] & (1U << (4 - column))) == 0U) {
                    continue;
                }
                glScissor(cursor + (column * scale),
                    y + (static_cast<int>(glyph.size() - 1U - row) * scale),
                    scale, scale);
                glClear(GL_COLOR_BUFFER_BIT);
            }
        }
        cursor += 6 * scale;
    }
}

wkc::core::Mat4 basisMatrix(
    const wkc::core::Vec3 right,
    const wkc::core::Vec3 up,
    const wkc::core::Vec3 forward) noexcept
{
    wkc::core::Mat4 result = wkc::core::Mat4::identity();
    result.elements[0] = right.x;
    result.elements[1] = right.y;
    result.elements[2] = right.z;
    result.elements[4] = up.x;
    result.elements[5] = up.y;
    result.elements[6] = up.z;
    result.elements[8] = forward.x;
    result.elements[9] = forward.y;
    result.elements[10] = forward.z;
    return result;
}

class ScopedGpuTimer {
public:
    explicit ScopedGpuTimer(wkc::render::GpuTimer& timer) noexcept
        : timer_(timer)
        , active_(timer_.beginFrame())
    {
    }

    ~ScopedGpuTimer()
    {
        if (active_) {
            timer_.endFrame();
        }
    }

    ScopedGpuTimer(const ScopedGpuTimer&) = delete;
    ScopedGpuTimer& operator=(const ScopedGpuTimer&) = delete;

private:
    wkc::render::GpuTimer& timer_;
    bool active_{false};
};

std::string openGlString(const GLenum name)
{
    const GLubyte* const value = glGetString(name);
    return value == nullptr
        ? std::string{"unknown"}
        : std::string{reinterpret_cast<const char*>(value)};
}

} // namespace

bool GdpRenderer::initialize(const int width, const int height)
{
    if (!loadGlApi(gl_)) {
        return false;
    }
    gpuVendor_ = openGlString(GL_VENDOR);
    gpuRenderer_ = openGlString(GL_RENDERER);
    gpuVersion_ = openGlString(GL_VERSION);
    wkc::render::MeshData uiQuad;
    uiQuad.vertices = {{{0.0F, 0.0F, 0.0F}}, {{1.0F, 0.0F, 0.0F}},
        {{1.0F, 1.0F, 0.0F}}, {{0.0F, 1.0F, 0.0F}}};
    uiQuad.indices = {0U, 1U, 2U, 0U, 2U, 3U};
    uiQuad.bounds = {{0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 0.0F}};
    wkc::render::ImageData atlas{uiFontAtlasWidth, uiFontAtlasHeight, {}};
    atlas.rgba.reserve(static_cast<std::size_t>(uiFontAtlasWidth * uiFontAtlasHeight * 4));
    for (std::size_t i = 0; i < uiFontAtlasRle.size(); i += 2U) {
        for (unsigned int n = 0; n < uiFontAtlasRle[i]; ++n) {
            const auto coverage = uiFontAtlasRle[i + 1U];
            atlas.rgba.insert(atlas.rgba.end(), {coverage, coverage, coverage, 255U});
        }
    }
    if (!uiShader_.compile(gl_, uiVertexShader, uiFragmentShader)
        || !uiQuad_.upload(gl_, uiQuad)
        || !uiFont_.upload(gl_, atlas, wkc::render::TextureSampling::Trilinear)) {
        return false;
    }
    if (!shader_.compile(gl_, vertexShaderSource, fragmentShaderSource)
        || !shadow3Shader_.compile(gl_, vertexShaderSource,
            makeShadowBudgetFragmentShaderSource(3U))
        || !shadow6Shader_.compile(gl_, vertexShaderSource,
            makeShadowBudgetFragmentShaderSource(6U))
        || !lowSpecShader_.compile(
            gl_, vertexShaderSource, makeLowSpecFragmentShaderSource())
        || !shadowDepthShader_.compile(gl_, shadowDepthVertexShader,
            shadowDepthFragmentShader)) {
        return false;
    }
    if (!box_.upload(gl_, wkc::render::makeBoxMesh())
        || !canisterCylinder_.upload(gl_, makeCanisterCylinderMesh())
        || !centralHubColumnShaft_.upload(gl_, makeCentralHubColumnShaftMesh())
        || !planterLeafMesh_.upload(gl_, makePlanterLeafMesh())
        || !glassTestDome_.upload(gl_, makeGlassTestDomeMesh())
        || !grenadeBody_.upload(gl_, makeRoundedMesh(24U, 16U))
        || !grenadeArmor_.upload(gl_, makeGrenadeArmorMesh())
        || !grenadeLights_.upload(gl_, makeGrenadeLightSlotsMesh())
        || !grenadeRing_.upload(gl_, makeGrenadeRingMesh())
        || !smokePuff_.upload(gl_, makeRoundedMesh(12U, 8U))) {
        return false;
    }
    for (std::size_t role = 0; role < weaponRoleCount; ++role) {
        const auto meshes = makeWeaponViewmodelMeshes(
            static_cast<HudWeaponRole>(role));
        for (std::size_t material = 0; material < weaponMaterialCount; ++material) {
            if (!meshes[material].vertices.empty()
                && !weaponMeshes_[role][material].upload(gl_, meshes[material])) {
                return false;
            }
        }
    }
    for (std::size_t role = 0; role < weaponRoleCount; ++role) {
        const auto parts = makeWeaponViewmodelParts(static_cast<HudWeaponRole>(role));
        for (std::size_t part = 0; part < weaponPartCount; ++part)
            for (std::size_t material = 0; material < weaponMaterialCount; ++material)
                if (!parts[part][material].vertices.empty()
                    && !weaponPartMeshes_[role][part][material].upload(gl_, parts[part][material]))
                    return false;
    }
    for (std::size_t material = 0; material < weaponMaterialCount; ++material) {
        const WeaponMaterialImages images = makeUsedWeaponMaterialImages(
            static_cast<WeaponMaterial>(material));
        WeaponGpuMaterial& gpuMaterial = weaponMaterials_[material];
        const auto sharedPaint = material == static_cast<std::size_t>(WeaponMaterial::AccentPaint)
            ? loadVisualImage("models/worker_reference/worker_paint_albedo.png")
            : std::nullopt;
        if (!gpuMaterial.albedo.upload(gl_, sharedPaint ? *sharedPaint : images.albedo,
                wkc::render::TextureSampling::Trilinear)
            || !gpuMaterial.normal.upload(
                gl_, images.normal, wkc::render::TextureSampling::Trilinear)
            || !gpuMaterial.properties.upload(
                gl_, images.properties, wkc::render::TextureSampling::Trilinear)) {
            return false;
        }
    }
    {
        constexpr std::string_view relative =
            "models/player_weapons/grenade/player_grenade.isemesh";
        const char* const basePath = SDL_GetBasePath();
        const std::filesystem::path base = basePath != nullptr
            ? std::filesystem::path{basePath} : std::filesystem::path{};
        const std::array candidates{
            base / "visual" / relative,
            base / "../Resources/visual" / relative,
            std::filesystem::path{"assets/visual"} / relative,
        };
        for (const auto& path : candidates) {
            std::error_code error;
            if (!std::filesystem::exists(path, error)) continue;
            std::string reason;
            const auto asset = loadEnemyModelAsset(path, &reason);
            if (!asset) {
                wkc::core::log(wkc::core::LogLevel::Error,
                    "Invalid player grenade model " + path.string() + ": " + reason);
                break;
            }
            bool valid = true;
            for (const EnemyModelSubmesh& part : asset->submeshes) {
                if (part.partId > 1U || part.materialId >= weaponMaterialCount) {
                    valid = false;
                    break;
                }
            }
            if (!valid) {
                wkc::core::log(wkc::core::LogLevel::Error,
                    "Invalid player grenade part or material ID: " + path.string());
                break;
            }
            for (const EnemyModelSubmesh& part : asset->submeshes) {
                GrenadeGpuPart gpu;
                gpu.partId = part.partId;
                gpu.materialId = part.materialId;
                if (!gpu.mesh.upload(gl_, part.mesh)) return false;
                grenadeModelParts_.push_back(std::move(gpu));
            }
            wkc::core::log(wkc::core::LogLevel::Info,
                "Loaded player Blender grenade " + path.string());
            break;
        }
    }
    const auto playerMeshes = makePlayerBodyMeshes();
    for (std::size_t material = 0; material < playerBodyMaterialCount; ++material) {
        const auto images = makeUsedPlayerBodyMaterialImages(static_cast<PlayerBodyMaterial>(material));
        auto& gpu = playerBodyMaterials_[material];
        if (!playerBodyMeshes_[material].upload(gl_, playerMeshes[material])
            || !gpu.albedo.upload(gl_, images.albedo, wkc::render::TextureSampling::Trilinear)
            || !gpu.normal.upload(gl_, images.normal, wkc::render::TextureSampling::Trilinear)
            || !gpu.properties.upload(gl_, images.properties, wkc::render::TextureSampling::Trilinear)) return false;
    }
    const char* const exosuitPreview = std::getenv("GDP_PLAYER_EXOSUIT_PREVIEW");
    const char* const legacyRigPreview = std::getenv("GDP_PLAYER_RIG_PREVIEW");
    const bool useExosuit = exosuitPreview != nullptr
        && std::string_view{exosuitPreview} == "1";
    playerRigPreview_ = useExosuit || (legacyRigPreview != nullptr
        && std::string_view{legacyRigPreview} == "1");
    if (playerRigPreview_) {
        const char* const basePath = SDL_GetBasePath();
        const std::filesystem::path base = basePath != nullptr
            ? std::filesystem::path{basePath} : std::filesystem::path{};
        const std::filesystem::path relative = useExosuit
            ? "models/player_exosuit/player_exosuit.isplayer"
            : "models/player_rig/player_rig.isplayer";
        const std::array candidates{
            base / "visual" / relative,
            base / "../Resources/visual" / relative,
            std::filesystem::path{"assets/visual"} / relative,
        };
        std::string reason = "file not found";
        for (const auto& path : candidates) {
            std::error_code error;
            if (!std::filesystem::exists(path,error)) continue;
            playerRigAsset_ = loadPlayerRigAsset(path,&reason);
            if (playerRigAsset_) {
                wkc::core::log(wkc::core::LogLevel::Info,
                    "Loaded Player Blender preview " + path.string());
            }
            break;
        }
        if (!playerRigAsset_) {
            wkc::core::log(wkc::core::LogLevel::Error,
                "Cannot load Player Blender preview: " + reason);
            return false;
        }
        constexpr std::array<std::array<std::uint8_t,3U>,playerExosuitMaterialCount> colors{{
            {100U,105U,110U}, {119U,121U,124U}, {78U,82U,86U},
            {63U,67U,72U}, {130U,135U,140U}, {43U,47U,52U},
            {214U,87U,16U}, {14U,174U,238U},
        }};
        for (std::size_t material = 0; material < playerRigAsset_->materialCount; ++material) {
            wkc::render::ImageData albedo;
            if (useExosuit && material == 6U) {
                const auto paint = loadVisualImage(
                    "models/worker_reference/worker_paint_albedo.png");
                if (!paint) return false;
                albedo = *paint;
            } else {
                albedo.width = albedo.height = 1;
                albedo.rgba = {colors[material][0],colors[material][1],
                    colors[material][2],255U};
            }
            if (!playerRigAlbedo_[material].upload(gl_,albedo)) return false;
        }
        for (std::size_t part = 0; part < playerRigAsset_->partCount; ++part) {
            for (std::size_t material = 0; material < playerRigAsset_->materialCount; ++material) {
                const auto meshIndex = part*playerRigAsset_->materialCount+material;
                if (playerRigAsset_->meshes[meshIndex].indices.empty()) continue;
                if (!playerRigMeshes_[meshIndex].upload(gl_,
                        playerRigAsset_->meshes[meshIndex])) return false;
            }
        }
    }
    const auto handsMeshes = makePlayerHandsMeshes();
    const auto singleHandMeshes = makePlayerHandsMeshes(false);
    for (std::size_t i = 0; i < playerHandsMaterialCount; ++i) {
        const auto images = makeUsedPlayerHandsMaterialImages(static_cast<PlayerHandsMaterial>(i));
        auto& gpu = playerHandsMaterials_[i];
        if (!playerHandsMeshes_[i].upload(gl_, handsMeshes[i])
            || !playerSingleHandMeshes_[i].upload(gl_, singleHandMeshes[i])
            || !gpu.albedo.upload(gl_, images.albedo, wkc::render::TextureSampling::Trilinear)
            || !gpu.normal.upload(gl_, images.normal, wkc::render::TextureSampling::Trilinear)
            || !gpu.properties.upload(gl_, images.properties, wkc::render::TextureSampling::Trilinear)) return false;
    }
    if (!fallbackTexture_.upload(gl_, gdp::game::makeIndustrialCheckerImage())) {
        return false;
    }
    wkc::render::ImageData solidWhite;
    solidWhite.width = 1;
    solidWhite.height = 1;
    solidWhite.rgba = {255U, 255U, 255U, 255U};
    if (!solidColorTexture_.upload(gl_, solidWhite)) return false;
    constexpr std::array<std::string_view, 2> storageBoxFiles{
        "rooms/storage/box_olive_v1.png",
        "rooms/storage/box_charcoal_v1.png",
    };
    for (std::size_t index = 0; index < storageBoxFiles.size(); ++index) {
        const auto image = loadVisualImage(storageBoxFiles[index]);
        if (!image || !storageBoxTextures_[index].upload(
                gl_, *image, wkc::render::TextureSampling::Trilinear)) {
            wkc::core::log(wkc::core::LogLevel::Error,
                "Storage shelf box texture is missing or invalid");
            return false;
        }
    }
    const auto workerPaint = loadVisualImage("worker_painted_steel_v2.png");
    if (!workerPaint || !workerPaintTexture_.upload(
            gl_, *workerPaint, wkc::render::TextureSampling::Trilinear)) {
        wkc::core::log(wkc::core::LogLevel::Error,
            "Missing or invalid Worker painted-steel texture");
        return false;
    }
    const auto workerPanel = loadVisualImage("worker_front_panel_source_v2.png");
    if (!workerPanel || !workerFrontPanelTexture_.upload(
            gl_, *workerPanel, wkc::render::TextureSampling::Trilinear)) {
        wkc::core::log(wkc::core::LogLevel::Error,
            "Missing or invalid Worker front-panel source image");
        return false;
    }
    if (referenceEnemyModelsEnabled()) {
        wkc::render::ImageData white;
        white.width = 1;
        white.height = 1;
        white.rgba = {255U, 255U, 255U, 255U};
        if (!referenceWhiteTexture_.upload(gl_, white)) return false;
        wkc::render::ImageData neutralPaint = *workerPaint;
        for (std::size_t pixel = 0U; pixel + 3U < neutralPaint.rgba.size(); pixel += 4U) {
            const float luminance = 0.2126F * neutralPaint.rgba[pixel]
                + 0.7152F * neutralPaint.rgba[pixel + 1U]
                + 0.0722F * neutralPaint.rgba[pixel + 2U];
            const auto value = static_cast<std::uint8_t>(
                std::clamp(170.0F + (luminance - 115.0F) * 1.15F, 36.0F, 245.0F));
            neutralPaint.rgba[pixel] = value;
            neutralPaint.rgba[pixel + 1U] = value;
            neutralPaint.rgba[pixel + 2U] = value;
        }
        if (!referenceNeutralPaintTexture_.upload(
                gl_, neutralPaint, wkc::render::TextureSampling::Trilinear)) {
            return false;
        }
        constexpr std::array roleNames{"worker", "guard", "officer", "engineer", "robot"};
        for (std::size_t index = 0U; index < roleNames.size(); ++index) {
            const std::string fileName = std::string{"models/"} + roleNames[index]
                + "_reference/" + roleNames[index] + "_paint_albedo.png";
            const auto paint = loadVisualImage(fileName);
            if (paint) {
                referencePaintReady_[index] = referencePaintTextures_[index].upload(
                    gl_, *paint, wkc::render::TextureSampling::Trilinear);
                if (!referencePaintReady_[index]) return false;
            }
        }
    }
    for (std::size_t index = 0; index < structuralColumnTextures_.size(); ++index) {
        const auto image = gdp::game::makeStructuralSteelImage(
            static_cast<std::uint8_t>(index));
        if (!structuralColumnTextures_[index].upload(
                gl_, image, wkc::render::TextureSampling::Trilinear)) {
            return false;
        }
    }
    const auto hubColumn = loadVisualImage(
        "rooms/central_hub/column_plate_v1.png");
    if (!hubColumn || !centralHubColumnTexture_.upload(
            gl_, *hubColumn, wkc::render::TextureSampling::Trilinear)) {
        return false;
    }
    const auto hubFloorDetail = loadVisualImage(
        "rooms/central_hub/floor_detail_metal_v1.png");
    if (!hubFloorDetail || !centralHubFloorDetailTexture_.upload(
            gl_, *hubFloorDetail, wkc::render::TextureSampling::Trilinear)) {
        return false;
    }
    const gdp::game::CeilingFixtureMaterialImages fixtureImages
        = gdp::game::makeCeilingFixtureMaterialImages();
    if (!ceilingFixtureMaterial_.albedo.upload(
            gl_, fixtureImages.albedo, wkc::render::TextureSampling::Trilinear)
        || !ceilingFixtureMaterial_.normal.upload(
            gl_, fixtureImages.normal, wkc::render::TextureSampling::Trilinear)
        || !ceilingFixtureMaterial_.properties.upload(
            gl_, fixtureImages.properties, wkc::render::TextureSampling::Trilinear)
        || !ceilingFixtureMaterial_.emission.upload(
            gl_, fixtureImages.emission, wkc::render::TextureSampling::Trilinear)) {
        return false;
    }
    constexpr std::array fixtureFiles{
        "industrial.isemesh", "storage.isemesh", "maintenance.isemesh",
        "laboratory.isemesh", "cafeteria.isemesh", "security.isemesh",
        "office.isemesh", "boss_arena.isemesh", "central_hub.isemesh",
    };
    const char* const basePath = SDL_GetBasePath();
    const std::filesystem::path base = basePath != nullptr
        ? std::filesystem::path{basePath} : std::filesystem::path{};
    for (std::size_t variant = 0U; variant < fixtureFiles.size(); ++variant) {
        const std::filesystem::path relative
            = std::filesystem::path{"models/ceiling_fixture_family_v2"}
                / fixtureFiles[variant];
        const std::array candidates{
            base / "visual" / relative,
            base / "../Resources/visual" / relative,
            std::filesystem::path{"assets/visual"} / relative,
        };
        for (const auto& path : candidates) {
            std::error_code error;
            if (!std::filesystem::exists(path, error)) continue;
            std::string reason;
            const auto asset = loadEnemyModelAsset(path, &reason);
            if (!asset) {
                wkc::core::log(wkc::core::LogLevel::Error,
                    "Invalid ceiling fixture " + path.string() + ": " + reason);
                return false;
            }
            for (const EnemyModelSubmesh& submesh : asset->submeshes) {
                if (submesh.partId != 0U || submesh.materialId > 4U) {
                    wkc::core::log(wkc::core::LogLevel::Error,
                        "Invalid ceiling fixture material: " + path.string());
                    return false;
                }
                CeilingFixtureGpuPart part;
                part.materialId = submesh.materialId;
                if (!part.mesh.upload(gl_, submesh.mesh)) return false;
                ceilingFixtureModels_[variant].push_back(std::move(part));
            }
            wkc::core::log(wkc::core::LogLevel::Info,
                "Loaded ceiling fixture " + path.string());
            break;
        }
    }
    const gdp::game::CeilingFixtureMaterialImages terminalImages
        = gdp::game::makeMissionTerminalMaterialImages();
    if (!terminalMaterial_.albedo.upload(
            gl_, terminalImages.albedo, wkc::render::TextureSampling::Trilinear)
        || !terminalMaterial_.normal.upload(
            gl_, terminalImages.normal, wkc::render::TextureSampling::Trilinear)
        || !terminalMaterial_.properties.upload(
            gl_, terminalImages.properties, wkc::render::TextureSampling::Trilinear)
        || !terminalMaterial_.emission.upload(
            gl_, terminalImages.emission, wkc::render::TextureSampling::Trilinear)) {
        return false;
    }
    const std::filesystem::path terminalRelative
        = "models/mission_terminal/terminal_a.isemesh";
    const std::array terminalCandidates{
        base / "visual" / terminalRelative,
        base / "../Resources/visual" / terminalRelative,
        std::filesystem::path{"assets/visual"} / terminalRelative,
    };
    bool terminalLoaded = false;
    for (const auto& path : terminalCandidates) {
        std::error_code error;
        if (!std::filesystem::exists(path, error)) continue;
        std::string reason;
        const auto asset = loadEnemyModelAsset(path, &reason);
        if (!asset) {
            wkc::core::log(wkc::core::LogLevel::Error,
                "Invalid mission terminal " + path.string() + ": " + reason);
            return false;
        }
        for (const EnemyModelSubmesh& submesh : asset->submeshes) {
            if (submesh.partId > 2U || submesh.materialId > 5U) return false;
            TerminalGpuPart part;
            part.partId = submesh.partId;
            part.materialId = submesh.materialId;
            if (!part.mesh.upload(gl_, submesh.mesh)) return false;
            terminalModelParts_.push_back(std::move(part));
        }
        terminalLoaded = !terminalModelParts_.empty();
        break;
    }
    if (!terminalLoaded) {
        wkc::core::log(wkc::core::LogLevel::Error,
            "Mission terminal model is missing from visual assets");
        return false;
    }
    if (!framebuffer_.resize(gl_, width, height)) {
        return false;
    }
    if (!gpuTimer_.initialize(gl_)) {
        wkc::core::log(wkc::core::LogLevel::Warning,
            "OpenGL GPU timer queries unavailable; diagnostics will use CPU timings");
    }

    camera_.setPerspective(68.0F, 0.05F, 100.0F);
    camera_.lookAt({0.0F, 1.7F, 6.5F}, {0.0F, 1.5F, -1.5F});
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
    initialized_ = true;
    if (!prepareIndustrialRoomMaterials(0) || !prepareWorkerVisual(0)
        || !prepareGuardVisual(0) || !prepareOfficerVisual(0)
        || !prepareEngineerVisual(0) || !prepareIndustrialRobotVisual(0)) {
        initialized_ = false;
        return false;
    }
    wkc::core::log(wkc::core::LogLevel::Info,
        "OpenGL device: vendor=" + gpuVendor_
            + ", renderer=" + gpuRenderer_ + ", version=" + gpuVersion_);
    wkc::core::log(wkc::core::LogLevel::Info,
        "Renderer initialized with fixed-room scene");
    return true;
}

bool GdpRenderer::prepareIndustrialRoomMaterials(const wkc::core::Seed roomSeed)
{
    if (!initialized_) {
        return false;
    }
    const IndustrialRoomMaterialSet materials
        = generateIndustrialRoomMaterials(roomSeed);
    if (!validateIndustrialRoomMaterials(materials).isValid()) {
        wkc::core::log(
            wkc::core::LogLevel::Error, "Industrial room material generation failed validation");
        return false;
    }
    if (roomMaterials_.contains(materials.materialHash)) {
        activeRoomMaterialKey_ = materials.materialHash;
        return true;
    }

    const std::size_t conditionIndex = static_cast<std::size_t>(materials.condition);
    auto& sharedWall = industrialWallMaterials_[conditionIndex];
    if (!sharedWall) {
        const IndustrialSurfaceRecipe archetype
            = makeIndustrialWallArchetypeRecipe(materials.condition);
        const auto images = generateIndustrialWallMaterialImages(archetype);
        if (!images) {
            return false;
        }
        IndustrialWallGpuMaterial wallMaterial;
        if (!wallMaterial.albedo.upload(
                gl_, images->albedo, wkc::render::TextureSampling::Trilinear)
            || !wallMaterial.normal.upload(
                gl_, images->normal, wkc::render::TextureSampling::Trilinear)
            || !wallMaterial.properties.upload(
                gl_, images->properties, wkc::render::TextureSampling::Trilinear)) {
            return false;
        }
        sharedWall.emplace(std::move(wallMaterial));
    }

    auto& sharedCeiling = industrialCeilingMaterials_[conditionIndex];
    if (!sharedCeiling) {
        const IndustrialSurfaceRecipe archetype
            = makeIndustrialCeilingArchetypeRecipe(materials.condition);
        const auto images = generateIndustrialCeilingMaterialImages(
            archetype);
        if (!images) {
            return false;
        }
        IndustrialWallGpuMaterial ceilingMaterial;
        if (!ceilingMaterial.albedo.upload(
                gl_, images->albedo, wkc::render::TextureSampling::Trilinear)
            || !ceilingMaterial.normal.upload(
                gl_, images->normal, wkc::render::TextureSampling::Trilinear)
            || !ceilingMaterial.properties.upload(
                gl_, images->properties, wkc::render::TextureSampling::Trilinear)) {
            return false;
        }
        sharedCeiling.emplace(std::move(ceilingMaterial));
    }

    RoomGpuMaterials gpuMaterials;
    gpuMaterials.columnCondition = static_cast<std::uint8_t>(materials.condition);
    gpuMaterials.industrialWallCondition = materials.condition;
    gpuMaterials.industrialCeilingCondition = materials.condition;
    gpuMaterials.wallUvOffset = {
        static_cast<float>((roomSeed >> 8U) & 1U) * 0.5F,
        0.0F,
    };
    gpuMaterials.ceilingUvOffset = {
        static_cast<float>((roomSeed >> 10U) & 1U) * 0.5F,
        static_cast<float>((roomSeed >> 11U) & 1U) * 0.5F,
    };
    // The former low-resolution floor was the last visibly old surface in
    // Industrial rooms. Reuse the broad metal-plate recipe already used by
    // Office, with a warm charcoal palette that matches the Industrial wall
    // without losing the floor-to-wall contrast, and an independent seed.
    auto floorRecipe = generateCorridorRoomMaterials(
        wkc::core::hashCombine(roomSeed, 0x494E445553464C32ULL))
        .surfaces[static_cast<std::size_t>(RoomSurface::Floor)];
    floorRecipe.pattern = CorridorSurfacePattern::Office;
    floorRecipe.baseColor = {0.21F, 0.20F, 0.185F};
    floorRecipe.secondaryColor = {0.061F, 0.058F, 0.052F};
    floorRecipe.accentColor = {0.45F, 0.31F, 0.11F};
    const auto floorImages = generateCorridorSurfaceMaterialImages(floorRecipe);
    if (!floorImages) {
        return false;
    }
    auto& floorMaterial = gpuMaterials.specialSurfaceMaterials[
        static_cast<std::size_t>(RoomSurface::Floor)].emplace();
    if (!floorMaterial.albedo.upload(
            gl_, floorImages->albedo, wkc::render::TextureSampling::Trilinear)
        || !floorMaterial.normal.upload(
            gl_, floorImages->normal, wkc::render::TextureSampling::Trilinear)
        || !floorMaterial.properties.upload(
            gl_, floorImages->properties, wkc::render::TextureSampling::Trilinear)) {
        return false;
    }
    gpuMaterials.industrialFloorV2 = true;
    const auto& wallRecipe = materials.surfaces[static_cast<std::size_t>(
        IndustrialSurface::Wall)];
    gpuMaterials.wallTone = std::clamp(wallRecipe.baseColor.x / 0.33F, 0.94F, 1.06F);
    const auto& ceilingRecipe = materials.surfaces[static_cast<std::size_t>(
        IndustrialSurface::Ceiling)];
    gpuMaterials.ceilingTone = std::clamp(
        ceilingRecipe.baseColor.x / 0.38F, 0.94F, 1.06F);
    roomMaterials_.emplace(materials.materialHash, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materials.materialHash;
    if (roomSeed == 0U) {
        fallbackRoomMaterialKey_ = materials.materialHash;
    }
    return true;
}

bool GdpRenderer::prepareCorridorRoomMaterials(
    const wkc::core::Seed corridorSeed, const bool structuralTransit)
{
    if (!initialized_) return false;
    const CorridorRoomMaterialSet materials = generateCorridorRoomMaterials(
        corridorSeed, structuralTransit
            ? CorridorSurfacePattern::StructuralTransit
            : CorridorSurfacePattern::ServiceCorridor);
    if (!validateCorridorRoomMaterials(materials).isValid()) return false;
    if (roomMaterials_.contains(materials.materialHash)) {
        activeRoomMaterialKey_ = materials.materialHash;
        return true;
    }
    RoomGpuMaterials gpu;
    gpu.corridorV2 = true;
    for (std::size_t i = 0; i < materials.surfaces.size(); ++i) {
        const auto images = generateCorridorSurfaceMaterialImages(materials.surfaces[i]);
        if (!images) return false;
        auto& surface = gpu.specialSurfaceMaterials[i].emplace();
        if (!surface.albedo.upload(gl_, images->albedo,
                wkc::render::TextureSampling::Trilinear)
            || !surface.normal.upload(gl_, images->normal,
                wkc::render::TextureSampling::Trilinear)
            || !surface.properties.upload(gl_, images->properties,
                wkc::render::TextureSampling::Trilinear)) return false;
    }
    roomMaterials_.emplace(materials.materialHash, std::move(gpu));
    activeRoomMaterialKey_ = materials.materialHash;
    return true;
}

bool GdpRenderer::prepareOfficeRoomMaterials(const wkc::core::Seed roomSeed)
{
    if (!initialized_) return false;
    // Office has broad recessed cassettes, separate from the corridor's
    // narrow service sheets and floor grip strips.
    CorridorRoomMaterialSet materials = generateCorridorRoomMaterials(
        wkc::core::hashCombine(roomSeed, 0x4F46464943455F31ULL));
    auto& wall = materials.surfaces[static_cast<std::size_t>(RoomSurface::Wall)];
    wall.pattern = CorridorSurfacePattern::Office;
    wall.baseColor = {0.33F, 0.36F, 0.35F};
    wall.secondaryColor = {0.115F, 0.14F, 0.14F};
    wall.accentColor = {0.24F, 0.56F, 0.60F};
    auto& floor = materials.surfaces[static_cast<std::size_t>(RoomSurface::Floor)];
    floor.pattern = CorridorSurfacePattern::Office;
    floor.baseColor = {0.17F, 0.19F, 0.19F};
    floor.secondaryColor = {0.065F, 0.075F, 0.075F};
    floor.accentColor = {0.26F, 0.45F, 0.46F};
    auto& ceiling = materials.surfaces[static_cast<std::size_t>(RoomSurface::Ceiling)];
    ceiling.pattern = CorridorSurfacePattern::Office;
    ceiling.baseColor = {0.25F, 0.28F, 0.28F};
    ceiling.secondaryColor = {0.09F, 0.11F, 0.12F};
    ceiling.accentColor = {0.24F, 0.50F, 0.52F};
    materials.materialHash = hashCorridorRoomMaterials(materials);
    if (!validateCorridorRoomMaterials(materials).isValid()) return false;
    if (roomMaterials_.contains(materials.materialHash)) {
        activeRoomMaterialKey_ = materials.materialHash;
        return true;
    }
    RoomGpuMaterials gpu;
    gpu.officeV1 = true;
    for (std::size_t i = 0; i < materials.surfaces.size(); ++i) {
        const auto images = generateCorridorSurfaceMaterialImages(materials.surfaces[i]);
        if (!images) return false;
        auto& surface = gpu.specialSurfaceMaterials[i].emplace();
        if (!surface.albedo.upload(gl_, images->albedo,
                wkc::render::TextureSampling::Trilinear)
            || !surface.normal.upload(gl_, images->normal,
                wkc::render::TextureSampling::Trilinear)
            || !surface.properties.upload(gl_, images->properties,
                wkc::render::TextureSampling::Trilinear)) return false;
    }
    roomMaterials_.emplace(materials.materialHash, std::move(gpu));
    activeRoomMaterialKey_ = materials.materialHash;
    return true;
}

bool GdpRenderer::prepareStorageRoomMaterials(
    const wkc::core::Seed roomSeed,
    const bool approvedHallMaterials)
{
    if (!initialized_) {
        return false;
    }
    const StorageRoomMaterialSet materials
        = generateStorageRoomMaterials(roomSeed);
    if (!validateStorageRoomMaterials(materials).isValid()) {
        wkc::core::log(
            wkc::core::LogLevel::Error, "Storage room material generation failed validation");
        return false;
    }
    const std::uint64_t materialKey = approvedHallMaterials
        ? wkc::core::hashCombine(materials.materialHash, 0x53544F5241575633ULL)
        : materials.materialHash;
    if (roomMaterials_.contains(materialKey)) {
        activeRoomMaterialKey_ = materialKey;
        return true;
    }

    RoomGpuMaterials gpuMaterials;
    gpuMaterials.columnCondition = static_cast<std::uint8_t>(materials.condition);
    gpuMaterials.storageHallV1 = approvedHallMaterials;
    gpuMaterials.storageHallSeed = roomSeed;
    gpuMaterials.storageRackCondition = materials.condition;
    constexpr std::array<std::string_view, 3> rackUprightFiles{
        "rooms/storage/rack_v3_upright_orderly.png",
        "rooms/storage/rack_v3_upright_dusty.png",
        "rooms/storage/rack_v3_upright_neglected.png",
    };
    constexpr std::array<std::string_view, 3> rackShelfFiles{
        "rooms/storage/rack_v3_shelf_orderly.png",
        "rooms/storage/rack_v3_shelf_dusty.png",
        "rooms/storage/rack_v3_shelf_neglected.png",
    };
    for (std::size_t index = 0; index < rackUprightFiles.size(); ++index) {
        if (!storageRackUprightTextures_[index].isValid()) {
            const auto image = loadVisualImage(rackUprightFiles[index]);
            if (!image || !storageRackUprightTextures_[index].upload(
                gl_, *image, wkc::render::TextureSampling::Trilinear)) {
                wkc::core::log(wkc::core::LogLevel::Error,
                    "Storage rack upright texture is missing or invalid");
                return false;
            }
        }
        if (!storageRackShelfTextures_[index].isValid()) {
            const auto image = loadVisualImage(rackShelfFiles[index]);
            if (!image || !storageRackShelfTextures_[index].upload(
                gl_, *image, wkc::render::TextureSampling::Trilinear)) {
                wkc::core::log(wkc::core::LogLevel::Error,
                    "Storage rack shelf texture is missing or invalid");
                return false;
            }
        }
    }
    if (approvedHallMaterials) {
        constexpr std::array<std::string_view, 6> wallFiles{
            "rooms/storage/wall_orderly_plain_v1.png",
            "rooms/storage/wall_orderly_marked_v1.png",
            "rooms/storage/wall_dusty_plain_v1.png",
            "rooms/storage/wall_dusty_v1.png",
            "rooms/storage/wall_neglected_plain_v1.png",
            "rooms/storage/wall_neglected_marked_v1.png",
        };
        constexpr std::array<std::string_view, 3> floorFiles{
            "rooms/storage/floor_orderly_v1.png",
            "rooms/storage/floor_dusty_v1.png",
            "rooms/storage/floor_neglected_v1.png",
        };
        constexpr std::array<std::string_view, 3> ceilingFiles{
            "rooms/storage/ceiling_orderly_v1.png",
            "rooms/storage/ceiling_dusty_v1.png",
            "rooms/storage/ceiling_neglected_v1.png",
        };
        const auto loadVariants = [this](auto& textures, const auto& files,
                                      const wkc::render::TextureSampling sampling,
                                      const std::string_view errorMessage) {
            for (std::size_t index = 0; index < files.size(); ++index) {
                if (textures[index].isValid()) {
                    continue;
                }
                const auto image = loadVisualImage(files[index]);
                if (!image || !textures[index].upload(gl_, *image, sampling)) {
                    wkc::core::log(wkc::core::LogLevel::Error, errorMessage);
                    return false;
                }
            }
            return true;
        };
        if (!loadVariants(storageWallTextures_, wallFiles,
                wkc::render::TextureSampling::Pixelated,
                "Storage wall variant is missing or invalid")
            || !loadVariants(storageFloorTextures_, floorFiles,
                wkc::render::TextureSampling::Trilinear,
                "Storage floor variant is missing or invalid")
            || !loadVariants(storageCeilingTextures_, ceilingFiles,
                wkc::render::TextureSampling::Trilinear,
                "Storage ceiling variant is missing or invalid")) {
            return false;
        }
    }
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        if (approvedHallMaterials) {
            continue;
        }
        const StorageSurfaceRecipe& recipe = materials.surfaces[index];
        const auto textureData = textureCache_.acquire(
            storageSurfaceRecipeHash(recipe),
            [&recipe] {
                return std::move(*generateStorageSurfaceAlbedo(recipe));
            });
        if (!gpuMaterials.textures[index].upload(gl_, *textureData)) {
            return false;
        }
    }
    roomMaterials_.emplace(materialKey, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materialKey;
    return true;
}

bool GdpRenderer::prepareCombatRoomMaterials(
    const wkc::core::Seed roomSeed, const bool galleryWallV1,
    const bool bossArenaV1)
{
    if (!initialized_) {
        return false;
    }
    const CombatRoomMaterialSet materials
        = generateCombatRoomMaterials(roomSeed, galleryWallV1, bossArenaV1);
    if (!validateCombatRoomMaterials(materials).isValid()) {
        wkc::core::log(
            wkc::core::LogLevel::Error, "Combat room material generation failed validation");
        return false;
    }
    if (roomMaterials_.contains(materials.materialHash)) {
        activeRoomMaterialKey_ = materials.materialHash;
        return true;
    }

    RoomGpuMaterials gpuMaterials;
    gpuMaterials.combatGalleryWallV1 = galleryWallV1 || bossArenaV1;
    gpuMaterials.combatBossV1 = bossArenaV1;
    gpuMaterials.combatWallSeed = static_cast<float>(roomSeed % 100000U);
    gpuMaterials.columnCondition = static_cast<std::uint8_t>(materials.condition);
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        const CombatSurfaceRecipe& recipe = materials.surfaces[index];
        if (bossArenaV1) {
            auto images = generateBossArenaSurfaceMaterialImages(recipe);
            if (!images) return false;
            auto& gpu = gpuMaterials.bossSurfaceMaterials[index].emplace();
            if (!gpu.albedo.upload(gl_, images->albedo,
                    wkc::render::TextureSampling::Trilinear)
                || !gpu.normal.upload(gl_, images->normal,
                    wkc::render::TextureSampling::Trilinear)
                || !gpu.properties.upload(gl_, images->properties,
                    wkc::render::TextureSampling::Trilinear)) {
                return false;
            }
            continue;
        }
        const auto textureData = textureCache_.acquire(
            combatSurfaceRecipeHash(recipe),
            [&recipe] {
                return std::move(*generateCombatSurfaceAlbedo(recipe));
            });
        if (!gpuMaterials.textures[index].upload(gl_, *textureData)) {
            return false;
        }
    }
    if (galleryWallV1 || bossArenaV1) {
        const auto coverAlbedo = textureCache_.acquire(
            combatCoverMaterialHash(roomSeed), [roomSeed] {
                return std::move(*generateCombatCoverAlbedo(roomSeed));
            });
        if (!gpuMaterials.combatCoverTexture.upload(gl_, *coverAlbedo)) {
            return false;
        }
        const auto columnAlbedo = textureCache_.acquire(
            combatColumnMaterialHash(roomSeed), [roomSeed] {
                return std::move(*generateCombatColumnAlbedo(roomSeed));
            });
        if (!gpuMaterials.combatColumnTexture.upload(gl_, *columnAlbedo)) {
            return false;
        }
        const auto railAlbedo = textureCache_.acquire(
            combatRailMaterialHash(roomSeed), [roomSeed] {
                return std::move(*generateCombatRailAlbedo(roomSeed));
            });
        if (!gpuMaterials.combatRailTexture.upload(gl_, *railAlbedo)) {
            return false;
        }
    }
    roomMaterials_.emplace(materials.materialHash, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materials.materialHash;
    return true;
}

bool GdpRenderer::prepareCafeteriaRoomMaterials(const wkc::core::Seed roomSeed)
{
    if (!initialized_) {
        return false;
    }
    const CafeteriaRoomMaterialSet materials
        = generateCafeteriaRoomMaterials(roomSeed);
    if (!validateCafeteriaRoomMaterials(materials).isValid()) {
        wkc::core::log(
            wkc::core::LogLevel::Error, "Cafeteria room material generation failed validation");
        return false;
    }
    const std::uint64_t materialKey = wkc::core::hashCombine(
        materials.materialHash, 0x434146455F563100ULL);
    if (roomMaterials_.contains(materialKey)) {
        activeRoomMaterialKey_ = materialKey;
        return true;
    }

    RoomGpuMaterials gpuMaterials;
    gpuMaterials.columnCondition = static_cast<std::uint8_t>(materials.condition);
    gpuMaterials.cafeteriaV1 = true;
    gpuMaterials.cafeteriaCondition
        = static_cast<std::uint8_t>(materials.condition);
    constexpr std::array<std::string_view, 3> wallFiles{
        "rooms/cafeteria/wall_clean.png",
        "rooms/cafeteria/wall_inuse.png",
        "rooms/cafeteria/wall_faded.png",
    };
    constexpr std::array<std::string_view, 3> floorFiles{
        "rooms/cafeteria/floor_clean.png",
        "rooms/cafeteria/floor_inuse.png",
        "rooms/cafeteria/floor_faded.png",
    };
    constexpr std::array<std::string_view, 3> ceilingFiles{
        "rooms/cafeteria/ceiling_clean.png",
        "rooms/cafeteria/ceiling_inuse.png",
        "rooms/cafeteria/ceiling_faded.png",
    };
    const auto loadVariants = [this](auto& textures, const auto& files,
                                  const std::string_view errorMessage) {
        for (std::size_t index = 0; index < files.size(); ++index) {
            if (textures[index].isValid()) {
                continue;
            }
            const auto image = loadVisualImage(files[index]);
            if (!image || !textures[index].upload(
                    gl_, *image, wkc::render::TextureSampling::Trilinear)) {
                wkc::core::log(wkc::core::LogLevel::Error, errorMessage);
                return false;
            }
        }
        return true;
    };
    if (!loadVariants(cafeteriaWallTextures_, wallFiles,
            "Cafeteria wall variant is missing or invalid")
        || !loadVariants(cafeteriaFloorTextures_, floorFiles,
            "Cafeteria floor variant is missing or invalid")
        || !loadVariants(cafeteriaCeilingTextures_, ceilingFiles,
            "Cafeteria ceiling variant is missing or invalid")) {
        return false;
    }
    constexpr std::array<std::array<std::string_view, 3>, 4> furnitureFiles{{
        {"rooms/cafeteria/furniture_steel_clean.png",
            "rooms/cafeteria/furniture_steel_inuse.png",
            "rooms/cafeteria/furniture_steel_faded.png"},
        {"rooms/cafeteria/furniture_seat_clean.png",
            "rooms/cafeteria/furniture_seat_inuse.png",
            "rooms/cafeteria/furniture_seat_faded.png"},
        {"rooms/cafeteria/furniture_support_clean.png",
            "rooms/cafeteria/furniture_support_inuse.png",
            "rooms/cafeteria/furniture_support_faded.png"},
        {"rooms/cafeteria/furniture_cabinet_clean.png",
            "rooms/cafeteria/furniture_cabinet_inuse.png",
            "rooms/cafeteria/furniture_cabinet_faded.png"},
    }};
    for (std::size_t part = 0; part < furnitureFiles.size(); ++part) {
        if (!loadVariants(cafeteriaFurnitureTextures_[part], furnitureFiles[part],
                "Cafeteria furniture variant is missing or invalid")) {
            return false;
        }
    }
    roomMaterials_.emplace(materialKey, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materialKey;
    return true;
}

bool GdpRenderer::prepareLaboratoryRoomMaterials(
    const wkc::core::Seed roomSeed, const bool approvedPreviewMaterials)
{
    if (!initialized_) {
        return false;
    }
    const LaboratoryRoomMaterialSet materials
        = generateLaboratoryRoomMaterials(roomSeed);
    if (!validateLaboratoryRoomMaterials(materials).isValid()) {
        wkc::core::log(
            wkc::core::LogLevel::Error, "Laboratory room material generation failed validation");
        return false;
    }
    const std::uint64_t materialKey = approvedPreviewMaterials
        ? wkc::core::hashCombine(materials.materialHash, 0x4C41425F50525631ULL)
        : materials.materialHash;
    if (roomMaterials_.contains(materialKey)) {
        activeRoomMaterialKey_ = materialKey;
        return true;
    }

    RoomGpuMaterials gpuMaterials;
    gpuMaterials.columnCondition = static_cast<std::uint8_t>(materials.condition);
    gpuMaterials.laboratoryPreviewV1 = approvedPreviewMaterials;
    if (approvedPreviewMaterials && !laboratoryFurnitureTexture_.isValid()) {
        const auto image = loadVisualImage(
            "rooms/laboratory/furniture_enamel_v1.png");
        if (!image || !laboratoryFurnitureTexture_.upload(
                gl_, *image, wkc::render::TextureSampling::Trilinear)) {
            wkc::core::log(wkc::core::LogLevel::Error,
                "Laboratory furniture texture is missing or invalid");
            return false;
        }
    }
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        if (approvedPreviewMaterials) {
            constexpr std::array<std::string_view, 3> files{
                "rooms/laboratory/wall_operational_v2.png",
                "rooms/laboratory/floor_operational_v1.png",
                "rooms/laboratory/ceiling_operational_v1.png",
            };
            const auto image = loadVisualImage(files[index]);
            if (!image || !gpuMaterials.textures[index].upload(
                    gl_, *image, wkc::render::TextureSampling::Trilinear)) {
                wkc::core::log(wkc::core::LogLevel::Error,
                    "Laboratory preview texture is missing or invalid");
                return false;
            }
        } else {
            const LaboratorySurfaceRecipe& recipe = materials.surfaces[index];
            const auto textureData = textureCache_.acquire(
                laboratorySurfaceRecipeHash(recipe),
                [&recipe] {
                    return std::move(*generateLaboratorySurfaceAlbedo(recipe));
                });
            if (!gpuMaterials.textures[index].upload(gl_, *textureData)) {
                return false;
            }
        }
    }
    roomMaterials_.emplace(materialKey, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materialKey;
    return true;
}

bool GdpRenderer::prepareTechnicalRoomMaterials(const wkc::core::Seed roomSeed)
{
    if (!initialized_) {
        return false;
    }
    const TechnicalRoomMaterialSet materials
        = generateTechnicalRoomMaterials(roomSeed);
    if (!validateTechnicalRoomMaterials(materials).isValid()) {
        wkc::core::log(
            wkc::core::LogLevel::Error, "Technical room material generation failed validation");
        return false;
    }
    if (roomMaterials_.contains(materials.materialHash)) {
        activeRoomMaterialKey_ = materials.materialHash;
        return true;
    }

    RoomGpuMaterials gpuMaterials;
    gpuMaterials.columnCondition = static_cast<std::uint8_t>(materials.condition);
    gpuMaterials.technicalV1 = true;
    constexpr std::array<std::string_view, 3> furnitureFiles{
        "rooms/technical/furniture_serviced.png",
        "rooms/technical/furniture_working.png",
        "rooms/technical/furniture_overloaded.png",
    };
    for (std::size_t index = 0; index < furnitureFiles.size(); ++index) {
        if (technicalFurnitureTextures_[index].isValid()) continue;
        const auto image = loadVisualImage(furnitureFiles[index]);
        if (!image || !technicalFurnitureTextures_[index].upload(
                gl_, *image, wkc::render::TextureSampling::Trilinear)) {
            wkc::core::log(wkc::core::LogLevel::Error,
                "Technical furniture texture is missing or invalid");
            return false;
        }
    }
    for (const auto& [texture, fileName] : {
             std::pair{&technicalWallDetailTexture_,
                 "rooms/technical/wall_steel_albedo_v2.png"},
             std::pair{&technicalFloorDetailTexture_,
                 "rooms/technical/floor_graphite_albedo_v2.png"}}) {
        if (texture->isValid()) continue;
        const auto image = loadVisualImage(fileName);
        if (!image || !texture->upload(gl_, *image,
                wkc::render::TextureSampling::Trilinear)) {
            wkc::core::log(wkc::core::LogLevel::Error,
                "Technical wall or floor albedo source is missing");
            return false;
        }
    }
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        const TechnicalSurfaceRecipe& recipe = materials.surfaces[index];
        const auto textureData = textureCache_.acquire(
            technicalSurfaceRecipeHash(recipe),
            [&recipe] {
                return std::move(*generateTechnicalSurfaceAlbedo(recipe));
            });
        if (!gpuMaterials.textures[index].upload(gl_, *textureData)) {
            return false;
        }
    }
    roomMaterials_.emplace(materials.materialHash, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materials.materialHash;
    return true;
}

bool GdpRenderer::prepareSecurityRoomMaterials(const wkc::core::Seed roomSeed)
{
    if (!initialized_) {
        return false;
    }
    const SecurityRoomMaterialSet materials
        = generateSecurityRoomMaterials(roomSeed);
    if (!validateSecurityRoomMaterials(materials).isValid()) {
        wkc::core::log(
            wkc::core::LogLevel::Error, "Security room material generation failed validation");
        return false;
    }
    const std::uint64_t materialKey = wkc::core::hashCombine(
        materials.materialHash, 0x5345435552495459ULL);
    if (roomMaterials_.contains(materialKey)) {
        activeRoomMaterialKey_ = materialKey;
        return true;
    }

    RoomGpuMaterials gpuMaterials;
    gpuMaterials.columnCondition = static_cast<std::uint8_t>(materials.condition);
    gpuMaterials.securityV1 = true;
    gpuMaterials.securityFloorSeed = static_cast<float>(roomSeed % 65521U);
    gpuMaterials.securityFloorTone = materials.condition == SecurityCondition::Secured
        ? 1.02F : materials.condition == SecurityCondition::Alert ? 0.96F : 1.0F;
    if (!securityFurnitureTexture_.isValid()) {
        const auto furniture = loadVisualImage(
            "rooms/security/furniture_graphite_v1.png");
        if (!furniture || !securityFurnitureTexture_.upload(
                gl_, *furniture, wkc::render::TextureSampling::Trilinear)) {
            wkc::core::log(wkc::core::LogLevel::Error,
                "Security furniture texture is missing or invalid");
            return false;
        }
    }
    constexpr std::array<std::array<std::string_view, 3>, 3> files{{
        {"rooms/security/wall_secured.png", "rooms/security/wall_monitored.png",
            "rooms/security/wall_alert.png"},
        {"rooms/security/floor_neutral_v2.png",
            "rooms/security/floor_neutral_v2.png",
            "rooms/security/floor_neutral_v2.png"},
        {"rooms/security/ceiling_secured.png", "rooms/security/ceiling_monitored.png",
            "rooms/security/ceiling_alert.png"},
    }};
    const std::size_t condition = static_cast<std::size_t>(materials.condition);
    for (std::size_t index = 0; index < files.size(); ++index) {
        const auto image = loadVisualImage(files[index][condition]);
        if (!image || !gpuMaterials.textures[index].upload(
                gl_, *image, wkc::render::TextureSampling::Trilinear)) {
            wkc::core::log(wkc::core::LogLevel::Error,
                "Security room texture is missing or invalid");
            return false;
        }
    }
    roomMaterials_.emplace(materialKey, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materialKey;
    return true;
}

bool GdpRenderer::prepareCentralHubRoomMaterials(const wkc::core::Seed roomSeed)
{
    if (!initialized_) {
        return false;
    }
    const CentralHubRoomMaterialSet materials
        = generateCentralHubRoomMaterials(roomSeed);
    if (!validateCentralHubRoomMaterials(materials).isValid()) {
        wkc::core::log(
            wkc::core::LogLevel::Error, "Central Hub material generation failed validation");
        return false;
    }
    if (roomMaterials_.contains(materials.materialHash)) {
        activeRoomMaterialKey_ = materials.materialHash;
        return true;
    }

    RoomGpuMaterials gpuMaterials;
    gpuMaterials.centralHubWallCondition = materials.condition;
    gpuMaterials.columnCondition = static_cast<std::uint8_t>(materials.condition);
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        if (index == static_cast<std::size_t>(RoomSurface::Wall)
            || index == static_cast<std::size_t>(RoomSurface::Floor)
            || index == static_cast<std::size_t>(RoomSurface::Ceiling)) {
            const bool wall = index == static_cast<std::size_t>(RoomSurface::Wall);
            const bool floor = index == static_cast<std::size_t>(RoomSurface::Floor);
            const auto image = loadVisualImage(
                wall ? "rooms/central_hub/wall_plate_v1.png"
                     : floor ? "rooms/central_hub/floor_plate_v1.png"
                             : "rooms/central_hub/ceiling_plate_v1.png");
            if (!image || !gpuMaterials.textures[index].upload(
                    gl_, *image, wkc::render::TextureSampling::Trilinear)) {
                wkc::core::log(wkc::core::LogLevel::Error,
                    wall ? "Central Hub wall texture is missing or invalid"
                         : floor ? "Central Hub floor texture is missing or invalid"
                                 : "Central Hub ceiling texture is missing or invalid");
                return false;
            }
            continue;
        }
        const CentralHubSurfaceRecipe& recipe = materials.surfaces[index];
        const auto textureData = textureCache_.acquire(
            centralHubSurfaceRecipeHash(recipe),
            [&recipe] {
                return std::move(*generateCentralHubSurfaceAlbedo(recipe));
            });
        if (!gpuMaterials.textures[index].upload(gl_, *textureData)) {
            return false;
        }
    }
    roomMaterials_.emplace(materials.materialHash, std::move(gpuMaterials));
    activeRoomMaterialKey_ = materials.materialHash;
    return true;
}

bool GdpRenderer::prepareWorkerVisual(const wkc::core::Seed visualSeed)
{
    if (!initialized_) {
        return false;
    }
    if (tryPrepareEnemyReferenceVisual("worker", 0U, workerVisuals_)) {
        return true;
    }
    if (workerVisuals_.contains(visualSeed)) {
        return true;
    }

    const WorkerVisual visual
        = generateWorkerVisual(visualSeed);
    if (!validateWorkerVisual(visual).isValid()) {
        wkc::core::log(wkc::core::LogLevel::Error, "Worker visual generation failed validation");
        return false;
    }
    EnemyGpuVisual gpuVisual;
    if (!uploadEnemyGpuVisual(
            visual.meshRecipe, visual.textureRecipe, gpuVisual)) {
        return false;
    }
    for (std::size_t index = 0; index < visual.partTints.size(); ++index) {
        gpuVisual.parts[index].tint = visual.partTints[index];
        const auto tint = visual.partTints[index];
        gpuVisual.parts[index].workerPaint = tint.x > tint.y * 1.45F
            && tint.x > tint.z * 1.65F;
    }
    gpuVisual.parts.reserve(gpuVisual.parts.size() + visual.details.size());
    for (std::size_t detailIndex = 0; detailIndex < visual.details.size(); ++detailIndex) {
        const WorkerDetailRecipe& detail = visual.details[detailIndex];
        const auto meshData = meshCache_.acquire(
            wkc::procedural::meshRecipeHash(detail.meshRecipe),
            [&detail] {
                return std::move(
                    wkc::procedural::generateMesh(detail.meshRecipe)->mesh);
            });
        EnemyGpuPart gpuPart;
        if (!gpuPart.mesh.upload(gl_, *meshData)) {
            return false;
        }
        gpuPart.index = detail.parentPartIndex;
        gpuPart.pivot = gpuVisual.parts[detail.parentPartIndex].pivot;
        gpuPart.tint = detail.tint;
        gpuPart.workerPaint
            = detail.meshRecipe.primitives.front().semantic
                    != wkc::procedural::SemanticRegion::Lens
                && detail.tint.x > detail.tint.y * 1.45F
                && detail.tint.x > detail.tint.z * 1.65F;
        // The second detail is the flat front panel, whose exact painted
        // surface is sampled from the approved concept image.
        gpuPart.workerFrontPanel = detailIndex == 1U;
        gpuPart.workerEyeCore = detailIndex == 3U;
        // The final Worker detail is a circular blade mounted on the tool.
        // Its first primitive locates the axle in assembled Worker space.
        gpuPart.workerSawBlade = detailIndex == 13U;
        if (gpuPart.workerSawBlade) {
            gpuPart.workerSawCenter
                = detail.meshRecipe.primitives.front().localTransform.position;
        }
        gpuVisual.parts.push_back(std::move(gpuPart));
    }
    // A soft billboard-like plane sits just ahead of the visor core. It uses
    // the same head pivot, so it follows every Worker pose without gameplay
    // or hitbox changes.
    const auto& eye = visual.details[3].meshRecipe.primitives.front();
    wkc::procedural::MeshRecipe haloRecipe;
    haloRecipe.version = 1U;
    haloRecipe.seed = visualSeed;
    haloRecipe.primitives.push_back({wkc::procedural::PrimitiveType::Box,
        {.position = eye.localTransform.position
            + wkc::core::Vec3{0.0F, 0.0F, 0.025F}},
        {eye.dimensions.x * 2.35F, eye.dimensions.y * 4.0F, 0.003F},
        wkc::procedural::MaterialSlot::Emissive,
        wkc::procedural::SemanticRegion::Lens});
    const auto haloMesh = wkc::procedural::generateMesh(haloRecipe);
    EnemyGpuPart haloPart;
    if (!haloMesh || !haloPart.mesh.upload(gl_, haloMesh->mesh)) return false;
    haloPart.index = 1U;
    haloPart.pivot = gpuVisual.parts[1].pivot;
    haloPart.tint = {1.0F, 0.58F, 0.10F};
    haloPart.workerEyeHalo = true;
    gpuVisual.parts.push_back(std::move(haloPart));
    workerVisuals_.emplace(visualSeed, std::move(gpuVisual));
    return true;
}

bool GdpRenderer::prepareGuardVisual(const wkc::core::Seed visualSeed)
{
    if (!initialized_) {
        return false;
    }
    if (tryPrepareEnemyReferenceVisual("guard", 1U, guardVisuals_)) {
        return true;
    }
    if (guardVisuals_.contains(visualSeed)) {
        return true;
    }

    const GuardVisual visual
        = generateGuardVisual(visualSeed);
    if (!validateGuardVisual(visual).isValid()) {
        wkc::core::log(wkc::core::LogLevel::Error, "Guard visual generation failed validation");
        return false;
    }
    EnemyGpuVisual gpuVisual;
    if (!uploadEnemyGpuVisual(
            visual.meshRecipe, visual.textureRecipe, gpuVisual)) {
        return false;
    }
    guardVisuals_.emplace(visualSeed, std::move(gpuVisual));
    return true;
}

bool GdpRenderer::prepareOfficerVisual(const wkc::core::Seed visualSeed)
{
    if (!initialized_) {
        return false;
    }
    if (tryPrepareEnemyReferenceVisual("officer", 2U, officerVisuals_)) {
        return true;
    }
    if (officerVisuals_.contains(visualSeed)) {
        return true;
    }

    const OfficerVisual visual
        = generateOfficerVisual(visualSeed);
    if (!validateOfficerVisual(visual).isValid()) {
        wkc::core::log(wkc::core::LogLevel::Error, "Officer visual generation failed validation");
        return false;
    }
    EnemyGpuVisual gpuVisual;
    if (!uploadEnemyGpuVisual(
            visual.meshRecipe, visual.textureRecipe, gpuVisual)) {
        return false;
    }
    officerVisuals_.emplace(visualSeed, std::move(gpuVisual));
    return true;
}

bool GdpRenderer::prepareEngineerVisual(const wkc::core::Seed visualSeed)
{
    if (!initialized_) {
        return false;
    }
    if (tryPrepareEnemyReferenceVisual("engineer", 3U, engineerVisuals_)) {
        return true;
    }
    if (engineerVisuals_.contains(visualSeed)) {
        return true;
    }

    const EngineerVisual visual
        = generateEngineerVisual(visualSeed);
    if (!validateEngineerVisual(visual).isValid()) {
        wkc::core::log(wkc::core::LogLevel::Error,
            "Engineer visual generation failed validation");
        return false;
    }
    EnemyGpuVisual gpuVisual;
    if (!uploadEnemyGpuVisual(
            visual.meshRecipe, visual.textureRecipe, gpuVisual)) {
        return false;
    }
    engineerVisuals_.emplace(visualSeed, std::move(gpuVisual));
    return true;
}

bool GdpRenderer::prepareIndustrialRobotVisual(const wkc::core::Seed visualSeed)
{
    if (!initialized_) {
        return false;
    }
    if (tryPrepareEnemyReferenceVisual("robot", 4U, industrialRobotVisuals_)) {
        return true;
    }
    if (industrialRobotVisuals_.contains(visualSeed)) {
        return true;
    }
    const IndustrialRobotVisual visual
        = generateIndustrialRobotVisual(visualSeed);
    if (!validateIndustrialRobotVisual(visual).isValid()) {
        wkc::core::log(wkc::core::LogLevel::Error,
            "IndustrialRobot visual generation failed validation");
        return false;
    }
    EnemyGpuVisual gpuVisual;
    if (!uploadEnemyGpuVisual(
            visual.meshRecipe, visual.textureRecipe, gpuVisual)) {
        return false;
    }
    industrialRobotVisuals_.emplace(visualSeed, std::move(gpuVisual));
    return true;
}

bool GdpRenderer::uploadEnemyGpuVisual(
    const wkc::procedural::MeshRecipe& meshRecipe,
    const wkc::procedural::TextureRecipe& textureRecipe,
    EnemyGpuVisual& gpuVisual)
{
    const std::vector<EnemyRigidPartRecipe> rigidParts
        = splitEnemyRigidParts(meshRecipe);
    if (rigidParts.empty()) {
        return false;
    }
    gpuVisual.parts.reserve(rigidParts.size());
    for (const EnemyRigidPartRecipe& rigidPart : rigidParts) {
        const auto meshData = meshCache_.acquire(
            wkc::procedural::meshRecipeHash(rigidPart.meshRecipe),
            [&rigidPart] {
                return std::move(
                    wkc::procedural::generateMesh(rigidPart.meshRecipe)->mesh);
            });
        EnemyGpuPart gpuPart;
        if (!gpuPart.mesh.upload(gl_, *meshData)) {
            return false;
        }
        gpuPart.pivot = rigidPart.pivot;
        gpuPart.index = rigidPart.index;
        gpuVisual.parts.push_back(std::move(gpuPart));
    }
    const auto textureData = textureCache_.acquire(
        wkc::procedural::textureRecipeHash(textureRecipe),
        [&textureRecipe] {
            return std::move(*wkc::procedural::generateAlbedo(textureRecipe));
        });
    if (!gpuVisual.texture.upload(gl_, *textureData)) {
        return false;
    }
    gpuVisual.emissiveStrength = textureRecipe.emissiveAmount;
    gpuVisual.wearAmount = textureRecipe.wearAmount;
    gpuVisual.basePartCount = static_cast<std::uint32_t>(rigidParts.size());
    return true;
}

bool GdpRenderer::uploadEnemyReferenceVisual(
    const EnemyModelAsset& asset, const std::string_view roleName,
    EnemyGpuVisual& gpuVisual)
{
    wkc::core::Vec3 toolPivot{};
    for (const EnemyModelSubmesh& submesh : asset.submeshes) {
        if (submesh.partId == 7U) {
            toolPivot = submesh.pivot;
            break;
        }
    }
    gpuVisual.parts.reserve(asset.submeshes.size());
    for (const EnemyModelSubmesh& submesh : asset.submeshes) {
        EnemyGpuPart part;
        if (!part.mesh.upload(gl_, submesh.mesh)) return false;
        part.workerSawBlade = roleName == "worker" && submesh.partId == 8U;
        part.index = part.workerSawBlade ? 7U : submesh.partId;
        part.pivot = part.workerSawBlade ? toolPivot : submesh.pivot;
        if (part.workerSawBlade) part.workerSawCenter = submesh.pivot;
        part.referenceMaterialId = static_cast<std::uint8_t>(submesh.materialId);
        gpuVisual.parts.push_back(std::move(part));
    }
    wkc::render::ImageData white;
    white.width = 1;
    white.height = 1;
    white.rgba = {255U, 255U, 255U, 255U};
    if (!gpuVisual.texture.upload(gl_, white)) return false;
    gpuVisual.basePartCount = roleName == "guard" ? 10U : 8U;
    return true;
}

bool GdpRenderer::tryPrepareEnemyReferenceVisual(
    const std::string_view roleName, const std::size_t roleIndex,
    std::unordered_map<wkc::core::Seed, EnemyGpuVisual>& visuals)
{
    if (!referenceEnemyModelsEnabled()) return false;
    if (referenceModelReady_[roleIndex]) return true;
    const std::string fileName = std::string{"models/"} + std::string{roleName}
        + "_reference/" + std::string{roleName} + "_reference.isemesh";
    const char* const basePath = SDL_GetBasePath();
    const std::filesystem::path base = basePath != nullptr
        ? std::filesystem::path{basePath} : std::filesystem::path{};
    const std::array candidates{
        base / "visual" / fileName,
        base / "../Resources/visual" / fileName,
        std::filesystem::path{"assets/visual"} / fileName,
    };
    for (const auto& path : candidates) {
        std::error_code error;
        if (!std::filesystem::exists(path, error)) continue;
        std::string reason;
        const auto asset = loadEnemyModelAsset(path, &reason);
        if (!asset) {
            wkc::core::log(wkc::core::LogLevel::Error,
                "Invalid enemy reference model " + path.string() + ": " + reason);
            return false;
        }
        EnemyGpuVisual gpuVisual;
        if (!uploadEnemyReferenceVisual(*asset, roleName, gpuVisual)) return false;
        visuals.emplace(0U, std::move(gpuVisual));
        referenceModelReady_[roleIndex] = true;
        wkc::core::log(wkc::core::LogLevel::Info,
            "Loaded enemy reference model " + path.string());
        return true;
    }
    return false;
}

void GdpRenderer::drawMesh(const wkc::render::Mesh& mesh) noexcept
{
    if (!mesh.isValid()) {
        return;
    }
    const bool cullingEnabled = mesh.backfaceCullingSafe();
    if (backfaceCulling_.needsUpdate(cullingEnabled)) {
        if (cullingEnabled) {
            glEnable(GL_CULL_FACE);
        } else {
            glDisable(GL_CULL_FACE);
        }
    }
    if (cullingEnabled) {
        ++lastFrameStats_.backfaceCulledDraws;
    }
    mesh.draw(gl_, meshBindings_);
    ++lastFrameStats_.drawCalls;
    lastFrameStats_.triangles += mesh.indexCount() / 3U;
}

void GdpRenderer::drawMeshInstanced(const wkc::render::Mesh& mesh) noexcept
{
    if (!mesh.isValid() || mesh.instanceCount() == 0U) {
        return;
    }
    const bool cullingEnabled = mesh.backfaceCullingSafe();
    if (backfaceCulling_.needsUpdate(cullingEnabled)) {
        if (cullingEnabled) {
            glEnable(GL_CULL_FACE);
        } else {
            glDisable(GL_CULL_FACE);
        }
    }
    if (cullingEnabled) {
        ++lastFrameStats_.backfaceCulledDraws;
    }
    mesh.drawInstanced(gl_, meshBindings_);
    ++lastFrameStats_.drawCalls;
    ++lastFrameStats_.instancedDrawCalls;
    lastFrameStats_.renderedInstances += mesh.instanceCount();
    lastFrameStats_.avoidedDrawCalls += mesh.instanceCount() - 1U;
    lastFrameStats_.triangles += (mesh.indexCount() / 3U) * mesh.instanceCount();
}

bool GdpRenderer::render(
    const int width,
    const int height,
    const CameraData camera,
    const std::span<const EnemyRenderData> enemies,
    const std::span<const PickupRenderData> pickups,
    const std::span<const PropRenderData> props,
    const RoomLightingRenderData lighting,
    const RoomRenderData room,
    const std::span<const DoorRenderData> doors,
    const std::span<const CorridorRenderData> corridors,
    const std::span<const RoomRenderData> connectedRooms,
    const HudRenderData hud,
    const std::span<const ImpactRenderData> impacts,
    const WeaponViewmodelRenderData weaponViewmodel,
    const std::span<const EnemyProjectileRenderData> enemyProjectiles,
    const PlayerBodyRenderData playerBody,
    const std::span<const GrenadeRenderData> grenades,
    const std::span<const GrenadeBlastRenderData> grenadeBlasts)
{
    lastFrameStats_ = {};
    if (!initialized_ || width <= 0 || height <= 0) {
        return false;
    }
    const bool scopeAllowed = !hud.paused && !hud.mainMenu && !hud.settingsMenu
        && !hud.controlsMenu && !hud.listMenu.active && !hud.exitConfirmation
        && hud.objective != ObjectiveRenderState::Victory
        && hud.objective != ObjectiveRenderState::Defeat;
    const float scopeStrength = scopeAllowed ? sniperScopeStrength(weaponViewmodel) : 0.0F;
    const int renderScalePercent = std::clamp(
        static_cast<int>(hud.worldRenderScalePercent), 50, 150);
    const int renderWidth = std::max(1, (width * renderScalePercent) / 100);
    const int renderHeight = std::max(1, (height * renderScalePercent) / 100);
    const std::uint32_t lightCount = std::min<std::uint32_t>(
        lighting.lightCount, static_cast<std::uint32_t>(maxRenderedRoomLights));
    const std::uint32_t shadowLightCount = std::min({
        lighting.shadowLightCount,
        lightCount,
        static_cast<std::uint32_t>(maxShadowCastingLights)});
    const std::uint32_t shadowOccluderCount = std::min({
        lighting.shadowOccluderCount,
        static_cast<std::uint32_t>(maxActiveShadowOccluders),
        static_cast<std::uint32_t>(maxShadowOccluders)});
    const bool geometryShadowsActive = geometryShadowsEnabled_
        && !hud.mainMenu
        && hud.geometryShadowLightBudget > 0U
        && hud.graphicsQuality >= HudGraphicsQuality::Medium;
    activeShaderPath_ = selectRendererShaderPath(
        hud.detailedMaterials || geometryShadowsActive, lightCount,
        shadowLightCount, shadowOccluderCount);
    if (!shadowShaderSpecializationEnabled_
        && activeShaderPath_ != RendererShaderPath::LowSpec) {
        activeShaderPath_ = RendererShaderPath::FullShadow8;
    }
    lowSpecShaderActive_ = activeShaderPath_ == RendererShaderPath::LowSpec;
    lastFrameStats_.lowSpecShader = lowSpecShaderActive_;
    lastFrameStats_.shadowShaderOccluderCapacity
        = shadowShaderOccluderCapacity(activeShaderPath_);
    wkc::render::Shader& shader = activeShader();
    shader.beginUniformFrame();
    lastFrameStats_.meshCacheEntries = meshCache_.stats().entries;
    lastFrameStats_.textureCacheEntries = textureCache_.stats().entries;
    const auto reportAllocationFailure = [](const std::string_view resource) {
        const GLenum error = glGetError();
        if (error == GL_NO_ERROR) return;
        std::ostringstream message;
        message << resource << " allocation failed with OpenGL error 0x"
            << std::hex << error;
        wkc::core::log(wkc::core::LogLevel::Error, message.str());
        // A failed allocation can leave more than one pending error. Clear
        // those errors before the next frame retries at a smaller scale.
        for (int index = 0; index < 16 && glGetError() != GL_NO_ERROR; ++index) {}
    };
    if (!framebuffer_.resize(gl_, renderWidth, renderHeight)) {
        reportAllocationFailure("World framebuffer");
        return false;
    }
    if (geometryShadowsActive) {
        const int shadowSize = hud.graphicsQuality == HudGraphicsQuality::Medium
            ? 256 : 512;
        const std::uint32_t requestedMaps = std::min<std::uint32_t>(
            hud.geometryShadowLightBudget,
            static_cast<std::uint32_t>(maxRenderedRoomLights));
        for (std::uint32_t index = 0U; index < requestedMaps; ++index) {
            if (!shadowCubes_[index].resize(gl_, shadowSize)) {
                reportAllocationFailure("Shadow map");
                return false;
            }
        }
    } else {
        shadowCubeCount_ = 0U;
    }
    textureBindings_.beginFrame();
    meshBindings_.beginFrame();
    backfaceCulling_.beginFrame();
    const ScopedGpuTimer gpuFrameTimer{gpuTimer_};
    if (const std::optional<double> gpuMilliseconds
        = gpuTimer_.latestMilliseconds()) {
        lastFrameStats_.gpuMilliseconds = *gpuMilliseconds;
        lastFrameStats_.gpuTimingAvailable = true;
    }

    framebuffer_.bind(gl_);
    glViewport(0, 0, renderWidth, renderHeight);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.018F, 0.025F, 0.03F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shader.use(gl_);
    camera_.setPerspective(camera.verticalFieldOfViewDegrees, 0.05F, 100.0F);
    camera_.lookAt(camera.position, camera.position + camera.forward);
    shader.setInt(gl_, "uAlbedo", 0);
    shader.setInt(gl_, "uNormalMap", 1);
    shader.setInt(gl_, "uMaterialMap", 2);
    shader.setInt(gl_, "uEmissionMap", 3);
    shader.setInt(gl_, "uTechnicalDetailAlbedo", 12);
    shader.setInt(gl_, "uUseEmissionMap", 0);
    shader.setInt(gl_, "uViewmodel", 0);
    shader.setFloat(gl_, "uPropIndirectFill", 0.0F);
    shader.setFloat(gl_, "uOpacity", 1.0F);
    shader.setInt(gl_, "uPlayerBodyFade", 0);
    shader.setFloat(gl_, "uBodyViewportHeight", static_cast<float>(renderHeight));
    shader.setInt(gl_, "uSoftEdges", 0);
    shader.setFloat(gl_, "uUnlitMix", 0.0F);
    shader.setFloat(gl_, "uMaterialUnlitMix", 0.0F);
    shader.setInt(gl_, "uInstanced", 0);
    shader.setInt(gl_, "uPlayerSkinning", 0);
    shader.setInt(gl_, "uDetailedMaterialsEnabled",
        hud.detailedMaterials ? 1 : 0);
    shader.setVec3(gl_, "uCameraPosition", camera.position);
    shader.setInt(gl_, "uHubPlinthContactShadow", 0);
    const auto setHubPlinthShadow
        = [&shader, this](const RoomRenderData& renderedRoom) {
              for (const world::RoomArchitecturePart& part
                  : renderedRoom.architectureParts) {
                  if (part.decoration
                      != world::RoomDecorationPart::CentralHubColumnPlinth) {
                      continue;
                  }
                  const wkc::core::Vec3 center
                      = (part.bounds.minimum + part.bounds.maximum) * 0.5F;
                  const float halfExtent
                      = (part.bounds.maximum.x - part.bounds.minimum.x) * 0.5F;
                  shader.setInt(gl_, "uHubPlinthContactShadow", 1);
                  shader.setVec3(gl_, "uHubPlinthFootprint",
                      {center.x, halfExtent, center.z});
                  return true;
              }
              return false;
          };
    if (!setHubPlinthShadow(room)) {
        for (const RoomRenderData& connectedRoom : connectedRooms) {
            if (setHubPlinthShadow(connectedRoom)) break;
        }
    }
    shader.setMat4(gl_, "uView", camera_.viewMatrix());
    shader.setMat4(
        gl_,
        "uProjection",
        camera_.projectionMatrix(
            static_cast<float>(renderWidth) / static_cast<float>(renderHeight)));
    const wkc::core::Vec3 alertColor{1.0F, 0.12F, 0.06F};
    const wkc::core::Vec3 ambientColor = lighting.alert
        ? (lighting.ambientColor * 0.28F) + (alertColor * 0.72F)
        : lighting.ambientColor;
    shader.setInt(gl_, "uLightCount", static_cast<int>(lightCount));
    shader.setInt(gl_, "uFlashlightEnabled",
        lighting.flashlightEnabled ? 1 : 0);
    shader.setVec3(gl_, "uFlashlightPosition", lighting.flashlightPosition);
    shader.setVec3(gl_, "uFlashlightDirection", lighting.flashlightDirection);
    shader.setVec3(gl_, "uAmbientColor", ambientColor);
    shader.setFloat(gl_, "uAmbientIntensity", lighting.ambientIntensity);
    static_assert(maxRenderedAmbientRegions == 16U);
    const std::uint32_t ambientRegionCount = std::min<std::uint32_t>(
        lighting.ambientRegionCount,
        static_cast<std::uint32_t>(maxRenderedAmbientRegions));
    shader.setInt(gl_, "uAmbientRegionCount",
        static_cast<int>(ambientRegionCount));
    static const auto ambientRegionUniforms = [] {
        std::array<std::array<std::string, maxRenderedAmbientRegions>, 4> names;
        constexpr std::array prefixes{
            "uAmbientRegionMinima[", "uAmbientRegionMaxima[",
            "uAmbientRegionColors[", "uAmbientRegionIntensities[",
        };
        for (std::size_t field = 0U; field < names.size(); ++field) {
            for (std::size_t index = 0U; index < maxRenderedAmbientRegions;
                 ++index) {
                names[field][index] = std::string{prefixes[field]}
                    + std::to_string(index) + ']';
            }
        }
        return names;
    }();
    for (std::size_t index = 0U; index < ambientRegionCount; ++index) {
        const AmbientRegionRenderData& region = lighting.ambientRegions[index];
        const wkc::core::Vec3 color = lighting.alert && index == 0U
            ? (region.color * 0.28F) + (alertColor * 0.72F)
            : region.color;
        shader.setVec3(gl_, ambientRegionUniforms[0][index].c_str(),
            region.bounds.minimum);
        shader.setVec3(gl_, ambientRegionUniforms[1][index].c_str(),
            region.bounds.maximum);
        shader.setVec3(gl_, ambientRegionUniforms[2][index].c_str(), color);
        shader.setFloat(gl_, ambientRegionUniforms[3][index].c_str(),
            region.intensity);
    }
    if (!lowSpecShaderActive_) {
        shader.setInt(gl_, "uShadowLightCount",
            geometryShadowsActive ? 0 : static_cast<int>(shadowLightCount));
        shader.setInt(gl_, "uShadowOccluderCount",
            geometryShadowsActive ? 0 : static_cast<int>(shadowOccluderCount));
        const std::uint32_t readyMapCount = geometryShadowsActive
            ? shadowCubeCount_ : 0U;
        shader.setInt(gl_, "uShadowCubeCount",
            static_cast<int>(readyMapCount));
        shader.setFloat(gl_, "uShadowCubeFarPlane", 22.0F);
        // Even an inactive sampler must not alias the 2D material samplers
        // on unit 0. Mesa validates sampler types before the draw call.
        for (std::uint32_t index = 0U;
             index < static_cast<std::uint32_t>(maxRenderedRoomLights);
             ++index) {
            const std::string mapUniform = "uShadowCubes["
                + std::to_string(index) + ']';
            shader.setInt(gl_, mapUniform.c_str(), static_cast<int>(4U + index));
            if (index >= readyMapCount) continue;
            const std::string positionUniform = "uShadowCubeLightPositions["
                + std::to_string(index) + ']';
            shader.setVec3(gl_, positionUniform.c_str(),
                shadowCubeLightPositions_[index]);
            const auto binding = textureBindings_.prepare(
                4U + index, shadowCubes_[index].texture());
            if (binding.activateUnit) gl_.activeTexture(GL_TEXTURE0 + 4U + index);
            if (binding.bindTexture) {
                glBindTexture(GL_TEXTURE_CUBE_MAP,
                    shadowCubes_[index].texture());
            }
        }
    }
    constexpr std::array positionUniforms{
        "uLightPositions[0]", "uLightPositions[1]",
        "uLightPositions[2]", "uLightPositions[3]",
        "uLightPositions[4]", "uLightPositions[5]",
        "uLightPositions[6]", "uLightPositions[7]",
    };
    constexpr std::array colorUniforms{
        "uLightColors[0]", "uLightColors[1]",
        "uLightColors[2]", "uLightColors[3]",
        "uLightColors[4]", "uLightColors[5]",
        "uLightColors[6]", "uLightColors[7]",
    };
    constexpr std::array intensityUniforms{
        "uLightIntensities[0]", "uLightIntensities[1]",
        "uLightIntensities[2]", "uLightIntensities[3]",
        "uLightIntensities[4]", "uLightIntensities[5]",
        "uLightIntensities[6]", "uLightIntensities[7]",
    };
    constexpr std::array rangeUniforms{
        "uLightRanges[0]", "uLightRanges[1]",
        "uLightRanges[2]", "uLightRanges[3]",
        "uLightRanges[4]", "uLightRanges[5]",
        "uLightRanges[6]", "uLightRanges[7]",
    };
    constexpr std::array roomMinimumUniforms{
        "uLightRoomMinima[0]", "uLightRoomMinima[1]",
        "uLightRoomMinima[2]", "uLightRoomMinima[3]",
        "uLightRoomMinima[4]", "uLightRoomMinima[5]",
        "uLightRoomMinima[6]", "uLightRoomMinima[7]",
    };
    constexpr std::array roomMaximumUniforms{
        "uLightRoomMaxima[0]", "uLightRoomMaxima[1]",
        "uLightRoomMaxima[2]", "uLightRoomMaxima[3]",
        "uLightRoomMaxima[4]", "uLightRoomMaxima[5]",
        "uLightRoomMaxima[6]", "uLightRoomMaxima[7]",
    };
    constexpr std::array boundedUniforms{
        "uLightBoundedToRoom[0]", "uLightBoundedToRoom[1]",
        "uLightBoundedToRoom[2]", "uLightBoundedToRoom[3]",
        "uLightBoundedToRoom[4]", "uLightBoundedToRoom[5]",
        "uLightBoundedToRoom[6]", "uLightBoundedToRoom[7]",
    };
    constexpr std::array shadowMinimumUniforms{
        "uShadowOccluderMinima[0]", "uShadowOccluderMinima[1]",
        "uShadowOccluderMinima[2]", "uShadowOccluderMinima[3]",
        "uShadowOccluderMinima[4]", "uShadowOccluderMinima[5]",
        "uShadowOccluderMinima[6]", "uShadowOccluderMinima[7]",
        "uShadowOccluderMinima[8]", "uShadowOccluderMinima[9]",
        "uShadowOccluderMinima[10]", "uShadowOccluderMinima[11]",
        "uShadowOccluderMinima[12]", "uShadowOccluderMinima[13]",
        "uShadowOccluderMinima[14]", "uShadowOccluderMinima[15]",
    };
    constexpr std::array shadowMaximumUniforms{
        "uShadowOccluderMaxima[0]", "uShadowOccluderMaxima[1]",
        "uShadowOccluderMaxima[2]", "uShadowOccluderMaxima[3]",
        "uShadowOccluderMaxima[4]", "uShadowOccluderMaxima[5]",
        "uShadowOccluderMaxima[6]", "uShadowOccluderMaxima[7]",
        "uShadowOccluderMaxima[8]", "uShadowOccluderMaxima[9]",
        "uShadowOccluderMaxima[10]", "uShadowOccluderMaxima[11]",
        "uShadowOccluderMaxima[12]", "uShadowOccluderMaxima[13]",
        "uShadowOccluderMaxima[14]", "uShadowOccluderMaxima[15]",
    };
    constexpr std::array shadowOctagonalUniforms{
        "uShadowOccluderOctagonal[0]", "uShadowOccluderOctagonal[1]",
        "uShadowOccluderOctagonal[2]", "uShadowOccluderOctagonal[3]",
        "uShadowOccluderOctagonal[4]", "uShadowOccluderOctagonal[5]",
        "uShadowOccluderOctagonal[6]", "uShadowOccluderOctagonal[7]",
        "uShadowOccluderOctagonal[8]", "uShadowOccluderOctagonal[9]",
        "uShadowOccluderOctagonal[10]", "uShadowOccluderOctagonal[11]",
        "uShadowOccluderOctagonal[12]", "uShadowOccluderOctagonal[13]",
        "uShadowOccluderOctagonal[14]", "uShadowOccluderOctagonal[15]",
    };
    for (std::size_t index = 0; index < lightCount; ++index) {
        const PointLightRenderData& light = lighting.lights[index];
        const wkc::core::Vec3 lightColor = lighting.alert
            ? (light.color * 0.24F) + (alertColor * 0.76F)
            : light.color;
        shader.setVec3(gl_, positionUniforms[index], light.position);
        shader.setVec3(gl_, colorUniforms[index], lightColor);
        shader.setFloat(gl_, intensityUniforms[index],
            lighting.alert ? light.intensity * 1.08F : light.intensity);
        shader.setFloat(gl_, rangeUniforms[index], light.range);
        shader.setInt(gl_, boundedUniforms[index], light.boundedToRoom ? 1 : 0);
        if (light.boundedToRoom) {
            shader.setVec3(gl_, roomMinimumUniforms[index],
                light.sourceRoomBounds.minimum);
            shader.setVec3(gl_, roomMaximumUniforms[index],
                light.sourceRoomBounds.maximum);
        }
    }
    for (std::size_t index = 0; index < shadowOccluderCount; ++index) {
        shader.setVec3(gl_, shadowMinimumUniforms[index],
            lighting.shadowOccluders[index].bounds.minimum);
        shader.setVec3(gl_, shadowMaximumUniforms[index],
            lighting.shadowOccluders[index].bounds.maximum);
        shader.setInt(gl_, shadowOctagonalUniforms[index],
            lighting.shadowOccluders[index].octagonalPrism ? 1 : 0);
    }

    const auto fallbackMaterials = roomMaterials_.find(fallbackRoomMaterialKey_);
    if (fallbackMaterials == roomMaterials_.end()) {
        return false;
    }
    const auto resolveMaterials = [this, &fallbackMaterials](const std::uint64_t key)
        -> const RoomGpuMaterials& {
        const std::uint64_t requestedKey = key == 0U ? activeRoomMaterialKey_ : key;
        const auto materials = roomMaterials_.find(requestedKey);
        return materials == roomMaterials_.end()
            ? fallbackMaterials->second : materials->second;
    };

    struct SurfaceStyle {
        const RoomGpuMaterials* roomMaterials{nullptr};
        RoomMaterialChannel material{RoomMaterialChannel::Generic};
        wkc::core::Vec3 tint{1.0F, 1.0F, 1.0F};
        wkc::core::Vec2 uvScale{1.0F, 1.0F};
        wkc::core::Vec2 uvOffset{};
        float surfaceTileMetres{0.0F};
        float emissiveStrength{0.0F};
        float unlitMix{0.0F};
        bool hubColumnPlate{false};
        bool mirrorSurfaceTile{false};
        bool detailedMaterial{false};
        std::uint8_t surfaceVariant{0};
        world::StorageRackPart rackPart{world::StorageRackPart::None};
        world::CafeteriaFurniturePart furniturePart{
            world::CafeteriaFurniturePart::None};

        constexpr bool operator==(const SurfaceStyle&) const noexcept = default;
    };
    struct SurfaceCommand {
        SurfaceStyle style{};
        wkc::core::Mat4 model{wkc::core::Mat4::identity()};
        const wkc::render::Mesh* mesh{nullptr};
        bool castsShadow{false};
    };
    std::vector<SurfaceCommand> surfaceCommands;
    bool architectureCastsShadow = false;
    struct ShadowCaster {
        const wkc::render::Mesh* mesh;
        wkc::core::Mat4 model;
    };
    std::vector<ShadowCaster> shadowCasters;
    bool footprintShellReady = true;
    const auto queueSurface = [this, &surfaceCommands,
                                  &architectureCastsShadow](
        const RoomGpuMaterials& roomMaterials,
        const wkc::core::Vec3 position,
        const wkc::core::Vec3 scale,
        const RoomMaterialChannel material,
        const float uvScale = 1.0F,
        const wkc::core::Vec3 tint = {1.0F, 1.0F, 1.0F},
        const wkc::core::Vec2 detailedUvScale = {},
        const wkc::core::Vec2 detailedUvOffset = {},
        const std::uint8_t surfaceVariant = 0,
        const world::StorageRackPart rackPart
            = world::StorageRackPart::None,
        const world::CafeteriaFurniturePart furniturePart
            = world::CafeteriaFurniturePart::None) {
        SurfaceStyle style;
        style.roomMaterials = &roomMaterials;
        style.material = material;
        bool detailedMaterial = false;
        wkc::core::Vec3 resolvedTint = tint;
        wkc::core::Vec2 materialUvOffset{};
        switch (material) {
        case RoomMaterialChannel::Wall:
            if (roomMaterials.combatGalleryWallV1) {
                // Two panels per axis: 6 m tiles make each wall plate 3 m
                // wide, matching the scale of the approved arena board.
                style.surfaceTileMetres = 6.0F;
                style.mirrorSurfaceTile = true;
            }
            if (roomMaterials.combatBossV1) {
                resolvedTint = {tint.x * 0.78F, tint.y * 0.76F,
                    tint.z * 0.73F};
            }
            if (roomMaterials.technicalV1) {
                style.surfaceTileMetres = 2.0F;
                style.mirrorSurfaceTile = true;
            }
            if (roomMaterials.laboratoryPreviewV1) {
                style.surfaceTileMetres = 2.0F;
                style.mirrorSurfaceTile = true;
            }
            if (roomMaterials.centralHubWallCondition) {
                style.mirrorSurfaceTile = true;
                // Match the warm graphite of the central column plate while
                // retaining this wall tile's own seams and edge wear.
                resolvedTint = {tint.x * 0.63F, tint.y * 0.59F,
                    tint.z * 0.53F};
            }
            if (roomMaterials.industrialWallCondition) {
                const std::size_t conditionIndex = static_cast<std::size_t>(
                    *roomMaterials.industrialWallCondition);
                const auto& wallMaterial = industrialWallMaterials_[conditionIndex];
                if (wallMaterial) {
                    detailedMaterial = true;
                    resolvedTint = tint * roomMaterials.wallTone;
                    materialUvOffset = roomMaterials.wallUvOffset;
                }
            }
            break;
        case RoomMaterialChannel::Floor:
            if (roomMaterials.industrialFloorV2) {
                style.surfaceTileMetres = 2.0F;
                style.mirrorSurfaceTile = true;
            }
            if (roomMaterials.combatGalleryWallV1) {
                style.surfaceTileMetres = 2.0F;
                style.mirrorSurfaceTile = true;
            }
            if (roomMaterials.technicalV1) {
                style.surfaceTileMetres = 2.0F;
                style.mirrorSurfaceTile = true;
            }
            if (roomMaterials.centralHubWallCondition) {
                style.mirrorSurfaceTile = true;
                // Lift the approved plate's midtones toward the column's
                // warm graphite without stretching or replacing its wear.
                resolvedTint = {tint.x * 1.16F, tint.y * 1.08F,
                    tint.z * 0.96F};
            }
            if (roomMaterials.laboratoryPreviewV1) {
                style.surfaceTileMetres = 2.0F;
                style.mirrorSurfaceTile = true;
            }
            if (roomMaterials.securityV1) {
                resolvedTint = tint * roomMaterials.securityFloorTone;
            }
            break;
        case RoomMaterialChannel::HubFloorDetail:
            // The fine brushed metal repeats by distance on hatches, frames
            // and slats instead of stretching once across each box.
            style.surfaceTileMetres = 0.75F;
            break;
        case RoomMaterialChannel::Ceiling:
            if (roomMaterials.combatGalleryWallV1) {
                style.surfaceTileMetres = 2.0F;
                style.mirrorSurfaceTile = true;
            }
            if (roomMaterials.technicalV1) {
                style.surfaceTileMetres = 2.0F;
                style.mirrorSurfaceTile = true;
            }
            if (roomMaterials.centralHubWallCondition) {
                style.mirrorSurfaceTile = true;
            }
            if (roomMaterials.laboratoryPreviewV1) {
                style.surfaceTileMetres = 2.0F;
                style.mirrorSurfaceTile = true;
            }
            if (roomMaterials.industrialCeilingCondition) {
                const std::size_t conditionIndex = static_cast<std::size_t>(
                    *roomMaterials.industrialCeilingCondition);
                const auto& ceilingMaterial = industrialCeilingMaterials_[conditionIndex];
                if (ceilingMaterial) {
                    detailedMaterial = true;
                    resolvedTint = tint * roomMaterials.ceilingTone;
                    materialUvOffset = roomMaterials.ceilingUvOffset;
                }
            }
            break;
        case RoomMaterialChannel::CombatCover:
            // Covers are a few metres across. Their own painted-steel
            // material keeps ribs and wear in object scale.
            style.surfaceTileMetres = 1.2F;
            style.mirrorSurfaceTile = true;
            break;
        case RoomMaterialChannel::CombatColumn:
            // One broad finish across a column face. The physical braces
            // supply the panel divisions without repeating joints sideways.
            style.surfaceTileMetres = 1.8F;
            style.mirrorSurfaceTile = true;
            break;
        case RoomMaterialChannel::CombatRail:
            // No panel seams: the posts and joints are already real geometry.
            style.surfaceTileMetres = 0.9F;
            break;
        case RoomMaterialChannel::StorageRack:
            style.surfaceTileMetres = 0.8F;
            style.mirrorSurfaceTile = true;
            break;
        case RoomMaterialChannel::CafeteriaFurniture:
            style.surfaceTileMetres = 0.75F;
            break;
        case RoomMaterialChannel::LaboratoryFurniture:
            style.surfaceTileMetres = 0.75F;
            style.mirrorSurfaceTile = true;
            break;
        case RoomMaterialChannel::SecurityFurniture:
            style.surfaceTileMetres = 0.75F;
            style.mirrorSurfaceTile = true;
            break;
        case RoomMaterialChannel::TechnicalFurniture:
            style.surfaceTileMetres = 1.0F;
            break;
        case RoomMaterialChannel::StructuralColumn:
            style.surfaceTileMetres = 0.8F;
            break;
        case RoomMaterialChannel::SolidColor:
            break;
        case RoomMaterialChannel::Generic:
            break;
        }
        if ((roomMaterials.corridorV2 || roomMaterials.officeV1
                || (roomMaterials.industrialFloorV2
                    && material == RoomMaterialChannel::Floor))
            && (material == RoomMaterialChannel::Wall
                || material == RoomMaterialChannel::Floor
                || material == RoomMaterialChannel::Ceiling)) {
            detailedMaterial = true;
            style.surfaceTileMetres = material == RoomMaterialChannel::Wall
                ? 3.0F : 2.0F;
        }
        const bool customUv = (detailedMaterial || roomMaterials.storageHallV1
            || roomMaterials.cafeteriaV1 || roomMaterials.securityV1)
            && detailedUvScale.x > 0.0F
            && std::abs(detailedUvScale.y) > 1.0e-6F;
        style.tint = resolvedTint;
        style.detailedMaterial = detailedMaterial;
        style.surfaceVariant = material == RoomMaterialChannel::StructuralColumn
            ? roomMaterials.columnCondition : surfaceVariant;
        style.rackPart = rackPart;
        style.furniturePart = material == RoomMaterialChannel::SecurityFurniture
            ? world::CafeteriaFurniturePart::Steel : furniturePart;
        style.uvScale = customUv
            ? detailedUvScale : wkc::core::Vec2{uvScale, uvScale};
        style.uvOffset = customUv
            ? detailedUvOffset + materialUvOffset : wkc::core::Vec2{};
        surfaceCommands.push_back({
            style,
            wkc::core::translationMatrix(position)
                * wkc::core::scaleMatrix(scale),
        });
        // Walls close the light volume at room boundaries. The footprint
        // wall mesh already contains the door opening. Floor and ceiling
        // tiles stay receivers: casting from their coplanar seams creates
        // dark stripes around ceiling fixtures.
        surfaceCommands.back().castsShadow = architectureCastsShadow
            || material == RoomMaterialChannel::Wall;
    };

    constexpr float fallbackRoomShellHalfThickness = 0.10F;
    const auto drawRoomShell = [&](const RoomRenderData renderedRoom) {
        const RoomGpuMaterials& roomMaterials = resolveMaterials(renderedRoom.materialKey);
        const wkc::core::Aabb bounds = renderedRoom.worldBounds;
        // Technical atlases follow continuous room UVs. Give rectangular
        // rooms the same shell UVs used by the shaped Technical room.
        std::vector<wkc::core::Vec2> rectangularOutline;
        if (roomMaterials.technicalV1 && renderedRoom.walkableOutline.empty()) {
            rectangularOutline = {
                {bounds.minimum.x, bounds.minimum.z},
                {bounds.maximum.x, bounds.minimum.z},
                {bounds.maximum.x, bounds.maximum.z},
                {bounds.minimum.x, bounds.maximum.z},
            };
        }
        const std::span<const wkc::core::Vec2> shellOutline =
            renderedRoom.walkableOutline.empty()
                ? std::span<const wkc::core::Vec2>{rectangularOutline}
                : std::span<const wkc::core::Vec2>{renderedRoom.walkableOutline};
        if (!shellOutline.empty()) {
            constexpr std::array surfaces{FootprintSurface::Floor,
                FootprintSurface::Ceiling, FootprintSurface::Wall};
            for (const FootprintSurface surface : surfaces) {
                const auto key = footprintMeshKey(
                    shellOutline, bounds, surface, doors);
                auto mesh = footprintMeshes_.find(key);
                if (mesh == footprintMeshes_.end()) {
                    const auto data = makeFootprintMesh(
                        shellOutline, bounds, surface, doors);
                    if (!data) { footprintShellReady = false; return; }
                    auto inserted = footprintMeshes_.try_emplace(key).first;
                    if (!inserted->second.upload(gl_, *data)) {
                        footprintMeshes_.erase(inserted);
                        footprintShellReady = false;
                        return;
                    }
                    mesh = inserted;
                }
                const auto material = surface == FootprintSurface::Floor
                    ? RoomMaterialChannel::Floor
                    : surface == FootprintSurface::Ceiling
                        ? RoomMaterialChannel::Ceiling
                        : RoomMaterialChannel::Wall;
                // Footprint mesh UVs are measured in 4 m; the Security
                // albedo uses a 2 m physical tile on every shell surface.
                queueSurface(roomMaterials, {}, {1.0F, 1.0F, 1.0F},
                    material, roomMaterials.securityV1
                        || ((material == RoomMaterialChannel::Wall
                                || material == RoomMaterialChannel::Floor
                                || material == RoomMaterialChannel::Ceiling)
                            && roomMaterials.centralHubWallCondition)
                        ? 2.0F : 1.0F);
                surfaceCommands.back().model = wkc::core::Mat4::identity();
                surfaceCommands.back().mesh = &mesh->second;
                if (roomMaterials.technicalV1 && surface != FootprintSurface::Ceiling) {
                    auto& style = surfaceCommands.back().style;
                    style.surfaceTileMetres = 0.0F;
                    style.mirrorSurfaceTile = false;
                    if (surface == FootprintSurface::Wall) {
                        // 48 two-metre cells around the perimeter, four high.
                        style.uvScale = {4.0F / 96.0F, 4.0F / 8.0F};
                        style.uvOffset = {};
                    } else {
                        // 12 two-metre cells per axis, anchored at room bounds.
                        style.uvScale = {4.0F / 24.0F, 4.0F / 24.0F};
                        style.uvOffset = {-bounds.minimum.x / 24.0F,
                            -bounds.minimum.z / 24.0F};
                    }
                }
            }
            return;
        }
        const float roomWidth = bounds.maximum.x - bounds.minimum.x;
        const float roomDepth = bounds.maximum.z - bounds.minimum.z;
        const float roomHeight = bounds.maximum.y - bounds.minimum.y;
        const bool approvedTiledRoom = roomMaterials.storageHallV1
            || roomMaterials.cafeteriaV1 || roomMaterials.securityV1
            || roomMaterials.technicalV1;
        const wkc::core::Vec3 floorTint = roomMaterials.cafeteriaV1
            ? wkc::core::Vec3{0.57F, 0.57F, 0.57F}
            : wkc::core::Vec3{1.0F, 1.0F, 1.0F};
        const wkc::core::Vec3 wallTint = roomMaterials.cafeteriaV1
            ? wkc::core::Vec3{0.54F, 0.54F, 0.54F}
            : wkc::core::Vec3{1.0F, 1.0F, 1.0F};
        const wkc::core::Vec3 roomCenter{
            (bounds.minimum.x + bounds.maximum.x) * 0.5F,
            bounds.minimum.y,
            (bounds.minimum.z + bounds.maximum.z) * 0.5F,
        };
        constexpr float shellThickness = fallbackRoomShellHalfThickness * 2.0F;
        if (approvedTiledRoom) {
            const float materialTileMetres = (roomMaterials.securityV1
                    || roomMaterials.technicalV1)
                ? 2.0F : 6.0F;
            const std::size_t tileCountX = static_cast<std::size_t>(
                std::ceil(roomWidth / materialTileMetres));
            const std::size_t tileCountZ = static_cast<std::size_t>(
                std::ceil(roomDepth / materialTileMetres));
            for (std::size_t tileX = 0; tileX < tileCountX; ++tileX) {
                const float minimumX = bounds.minimum.x
                    + static_cast<float>(tileX) * materialTileMetres;
                const float maximumX = std::min(
                    minimumX + materialTileMetres, bounds.maximum.x);
                for (std::size_t tileZ = 0; tileZ < tileCountZ; ++tileZ) {
                    const float minimumZ = bounds.minimum.z
                        + static_cast<float>(tileZ) * materialTileMetres;
                    const float maximumZ = std::min(
                        minimumZ + materialTileMetres, bounds.maximum.z);
                    const float tileWidth = maximumX - minimumX;
                    const float tileDepth = maximumZ - minimumZ;
                    const wkc::core::Vec3 tileCenter{
                        (minimumX + maximumX) * 0.5F,
                        bounds.minimum.y,
                        (minimumZ + maximumZ) * 0.5F,
                    };
                    const wkc::core::Vec2 tileUvScale{
                        tileWidth / materialTileMetres,
                        tileDepth / materialTileMetres,
                    };
                    const std::uint8_t floorVariant = roomMaterials.cafeteriaV1
                        ? roomMaterials.cafeteriaCondition
                        : static_cast<std::uint8_t>(
                            (roomMaterials.storageHallSeed + tileX + 2U * tileZ) % 3U);
                    const std::uint8_t ceilingVariant = roomMaterials.cafeteriaV1
                        ? roomMaterials.cafeteriaCondition
                        : static_cast<std::uint8_t>(
                            (roomMaterials.storageHallSeed + 2U * tileX + tileZ + 1U) % 3U);
                    queueSurface(roomMaterials,
                        tileCenter + wkc::core::Vec3{
                            0.0F, -shellThickness * 0.5F, 0.0F},
                        {tileWidth, shellThickness, tileDepth},
                        RoomMaterialChannel::Floor, 1.0F,
                        floorTint, tileUvScale, {}, floorVariant);
                    queueSurface(roomMaterials,
                        tileCenter + wkc::core::Vec3{
                            0.0F, roomHeight + shellThickness * 0.5F, 0.0F},
                        {tileWidth, shellThickness, tileDepth},
                        RoomMaterialChannel::Ceiling, 1.0F,
                        {1.0F, 1.0F, 1.0F}, tileUvScale, {}, ceilingVariant);
                }
            }
        } else {
            queueSurface(roomMaterials,
                roomCenter + wkc::core::Vec3{0.0F, -shellThickness * 0.5F, 0.0F},
                {roomWidth + shellThickness, shellThickness,
                    roomDepth + shellThickness},
                RoomMaterialChannel::Floor, 4.0F);
            queueSurface(roomMaterials,
                roomCenter
                    + wkc::core::Vec3{0.0F, roomHeight + shellThickness * 0.5F, 0.0F},
                {roomWidth + shellThickness, shellThickness,
                    roomDepth + shellThickness},
                RoomMaterialChannel::Ceiling, 1.0F, {1.0F, 1.0F, 1.0F},
                {roomWidth / 4.0F, roomDepth / 4.0F},
                {bounds.minimum.x / 4.0F, bounds.minimum.z / 4.0F});
        }

        const auto drawWall = [&](const wkc::core::Vec3 normal) {
            const bool facesX = std::abs(normal.x) > 0.5F;
            const std::size_t wallIndex = facesX
                ? (normal.x > 0.0F ? 0U : 1U)
                : (normal.z > 0.0F ? 2U : 3U);
            const float wallCenter = facesX
                ? (normal.x > 0.0F ? bounds.maximum.x : bounds.minimum.x)
                : (normal.z > 0.0F ? bounds.maximum.z : bounds.minimum.z);
            const float transverseMinimum = facesX
                ? bounds.minimum.z
                : bounds.minimum.x;
            const float transverseMaximum = facesX
                ? bounds.maximum.z
                : bounds.maximum.x;
            const auto door = std::find_if(
                doors.begin(), doors.end(), [&](const DoorRenderData& candidate) {
                    const float candidatePlane
                        = facesX ? candidate.position.x : candidate.position.z;
                    const float candidateTransverse
                        = facesX ? candidate.position.z : candidate.position.x;
                    return wkc::core::dot(normal, candidate.outwardNormal) > 0.90F
                        && std::abs(candidatePlane - wallCenter) < 0.25F
                        && candidateTransverse >= transverseMinimum - 0.01F
                        && candidateTransverse <= transverseMaximum + 0.01F;
                });
            const float tileMetres = roomMaterials.securityV1
                ? 2.0F : approvedTiledRoom ? roomHeight : 4.0F;
            const std::size_t tileCount = approvedTiledRoom
                ? std::max<std::size_t>(1U, static_cast<std::size_t>(
                    std::ceil((transverseMaximum - transverseMinimum) / tileMetres)))
                : 1U;
            std::size_t markedTile = static_cast<std::size_t>(
                (roomMaterials.storageHallSeed + wallIndex) % tileCount);
            if (roomMaterials.storageHallV1 && door != doors.end()) {
                const float doorCenter = facesX ? door->position.z : door->position.x;
                const float doorMinimum = doorCenter - (door->width * 0.5F);
                const float doorMaximum = doorCenter + (door->width * 0.5F);
                for (std::size_t step = 0; step < tileCount; ++step) {
                    const std::size_t candidate = (markedTile + step) % tileCount;
                    const float tileMinimum = transverseMinimum
                        + static_cast<float>(candidate) * tileMetres;
                    const float tileMaximum = std::min(
                        tileMinimum + tileMetres, transverseMaximum);
                    if (tileMaximum <= doorMinimum || tileMinimum >= doorMaximum) {
                        markedTile = candidate;
                        break;
                    }
                }
            }
            const auto drawWallSegment = [&](const float minimum, const float maximum,
                                             const float bottom, const float top,
                                             const std::size_t tileIndex,
                                             const bool upperIndustrialSegment) {
                if (maximum - minimum <= 1.0e-4F || top - bottom <= 1.0e-4F) {
                    return;
                }
                const float transverseCenter = (minimum + maximum) * 0.5F;
                const wkc::core::Vec3 position = facesX
                    ? wkc::core::Vec3{wallCenter,
                        bounds.minimum.y + ((bottom + top) * 0.5F),
                        transverseCenter}
                    : wkc::core::Vec3{transverseCenter,
                        bounds.minimum.y + ((bottom + top) * 0.5F),
                        wallCenter};
                const wkc::core::Vec3 scale = facesX
                    ? wkc::core::Vec3{
                        shellThickness, top - bottom, maximum - minimum}
                    : wkc::core::Vec3{
                        maximum - minimum, top - bottom, shellThickness};
                const std::uint8_t wallVariant = roomMaterials.storageHallV1
                    ? static_cast<std::uint8_t>(
                        2U * ((roomMaterials.storageHallSeed + wallIndex + tileIndex) % 3U)
                        + (tileIndex == markedTile ? 1U : 0U))
                    : roomMaterials.cafeteriaV1
                        ? roomMaterials.cafeteriaCondition : 0U;
                queueSurface(roomMaterials, position, scale, RoomMaterialChannel::Wall,
                    1.0F, wallTint,
                    {(maximum - minimum) / tileMetres,
                        approvedTiledRoom
                            ? -(top - bottom) / tileMetres
                            : (top - bottom) / tileMetres},
                    {approvedTiledRoom
                            ? ((wallIndex == 0U || wallIndex == 3U)
                                ? (transverseMinimum
                                    + static_cast<float>(tileIndex + 1U) * tileMetres
                                    - maximum) / tileMetres
                                : (minimum - transverseMinimum
                                    - static_cast<float>(tileIndex) * tileMetres)
                                    / tileMetres)
                            : minimum / tileMetres,
                        approvedTiledRoom
                            ? 1.0F - (bottom / tileMetres)
                            : upperIndustrialSegment
                                ? 0.5F + (bottom - 4.0F) / tileMetres
                                : bottom / tileMetres},
                    wallVariant);
            };
            const auto drawWallSpan = [&](const float minimum, const float maximum,
                                          const float bottom, const float top,
                                          const std::size_t tileIndex) {
                // The industrial wall atlas has a safety stripe only at its
                // base. On a wall taller than its 4 m tile, sample the upper
                // half of that atlas for the additional height at the same
                // texel density instead of wrapping the stripe mid-wall.
                if (roomMaterials.industrialWallCondition
                    && !approvedTiledRoom && bottom < 4.0F && top > 4.0F) {
                    drawWallSegment(minimum, maximum, bottom, 4.0F,
                        tileIndex, false);
                    drawWallSegment(minimum, maximum, 4.0F, top,
                        tileIndex, true);
                } else {
                    drawWallSegment(minimum, maximum, bottom, top, tileIndex,
                        roomMaterials.industrialWallCondition
                            && !approvedTiledRoom && bottom >= 4.0F);
                }
            };
            for (std::size_t tileIndex = 0; tileIndex < tileCount; ++tileIndex) {
                const float tileMinimum = approvedTiledRoom
                    ? transverseMinimum + static_cast<float>(tileIndex) * tileMetres
                    : transverseMinimum;
                const float tileMaximum = approvedTiledRoom
                    ? std::min(tileMinimum + tileMetres, transverseMaximum)
                    : transverseMaximum;
                if (door == doors.end()) {
                    drawWallSpan(tileMinimum, tileMaximum, 0.0F, roomHeight, tileIndex);
                    continue;
                }
                const float doorCenter = facesX ? door->position.z : door->position.x;
                const float openingMinimum = std::clamp(
                    doorCenter - (door->width * 0.5F), tileMinimum, tileMaximum);
                const float openingMaximum = std::clamp(
                    doorCenter + (door->width * 0.5F), tileMinimum, tileMaximum);
                drawWallSpan(tileMinimum, openingMinimum,
                    0.0F, roomHeight, tileIndex);
                drawWallSpan(openingMaximum, tileMaximum,
                    0.0F, roomHeight, tileIndex);
                drawWallSpan(openingMinimum, openingMaximum,
                    std::min(door->height, roomHeight), roomHeight, tileIndex);
            }
        };
        drawWall({1.0F, 0.0F, 0.0F});
        drawWall({-1.0F, 0.0F, 0.0F});
        drawWall({0.0F, 0.0F, 1.0F});
        drawWall({0.0F, 0.0F, -1.0F});
    };
    drawRoomShell(room);
    for (const RoomRenderData& connectedRoom : connectedRooms) {
        drawRoomShell(connectedRoom);
    }
    if (!footprintShellReady) return false;

    const auto drawSecurityFloorDetails = [&](const RoomRenderData& renderedRoom) {
        const RoomGpuMaterials& materials = resolveMaterials(renderedRoom.materialKey);
        if (!materials.securityV1) return;
        const wkc::core::Aabb bounds = renderedRoom.worldBounds;
        const float floorY = bounds.minimum.y;
        std::uint64_t roomKey = wkc::core::hashCombine(renderedRoom.materialKey,
            std::bit_cast<std::uint32_t>(bounds.minimum.x));
        roomKey = wkc::core::hashCombine(roomKey,
            std::bit_cast<std::uint32_t>(bounds.minimum.z));
        for (float z = bounds.minimum.z + 2.0F; z <= bounds.maximum.z - 2.0F;
             z += 4.0F) {
            for (float x = bounds.minimum.x + 2.0F; x <= bounds.maximum.x - 2.0F;
                 x += 4.0F) {
                std::uint64_t detailKey = wkc::core::hashCombine(roomKey,
                    std::bit_cast<std::uint32_t>(x));
                detailKey = wkc::core::hashCombine(detailKey,
                    std::bit_cast<std::uint32_t>(z));
                if (detailKey % 5U >= 2U) continue;
                const float offsetX = (static_cast<float>((detailKey >> 8U) & 255U)
                    / 255.0F - 0.5F) * 1.7F;
                const float offsetZ = (static_cast<float>((detailKey >> 16U) & 255U)
                    / 255.0F - 0.5F) * 1.7F;
                const wkc::core::Vec3 center{x + offsetX, floorY, z + offsetZ};
                if (!renderedRoom.walkableOutline.empty()
                    && !world::footprintContainsDisk(
                        renderedRoom.walkableOutline, center, 1.05F)) {
                    continue;
                }
                const bool nearDoor = std::any_of(doors.begin(), doors.end(),
                    [&](const DoorRenderData& door) {
                        return std::abs(door.position.x - center.x) < 1.75F
                            && std::abs(door.position.z - center.z) < 1.75F;
                    });
                if (nearDoor) continue;
                const bool nearFurniture = std::any_of(
                    renderedRoom.architectureParts.begin(),
                    renderedRoom.architectureParts.end(),
                    [&](const world::RoomArchitecturePart& part) {
                        return part.bounds.minimum.y < floorY + 1.2F
                            && part.bounds.maximum.y > floorY - 0.02F
                            && part.bounds.minimum.x < center.x + 1.0F
                            && part.bounds.maximum.x > center.x - 1.0F
                            && part.bounds.minimum.z < center.z + 1.0F
                            && part.bounds.maximum.z > center.z - 1.0F;
                    });
                if (nearFurniture) continue;

                const bool quarterTurn = ((detailKey >> 24U) & 1U) != 0U;
                const auto addPlate = [&](const float dx, const float dz,
                    const float plateWidth, const float plateDepth,
                    const float plateHeight,
                    const wkc::core::Vec3 tint,
                    const RoomMaterialChannel material
                        = RoomMaterialChannel::Floor) {
                    const float localX = quarterTurn ? dz : dx;
                    const float localZ = quarterTurn ? dx : dz;
                    const float spanX = quarterTurn ? plateDepth : plateWidth;
                    const float spanZ = quarterTurn ? plateWidth : plateDepth;
                    queueSurface(materials,
                        {center.x + localX, floorY + plateHeight * 0.5F,
                            center.z + localZ},
                        {spanX, plateHeight, spanZ}, material,
                        1.0F, tint, {spanX / 2.0F, spanZ / 2.0F});
                };
                const std::uint64_t detailType = (detailKey >> 28U) % 3U;
                if (detailType == 0U) {
                    // Service hatch: a muted inset, raised perimeter and
                    // four fasteners. It is decorative and has no collision.
                    addPlate(0.0F, 0.0F, 1.25F, 0.85F, 0.014F,
                        {0.55F, 0.60F, 0.61F});
                    for (const float side : {-1.0F, 1.0F}) {
                        addPlate(0.0F, side * 0.42F, 1.32F, 0.045F,
                            0.024F, {0.35F, 0.40F, 0.42F});
                        addPlate(side * 0.62F, 0.0F, 0.045F, 0.82F,
                            0.024F, {0.35F, 0.40F, 0.42F});
                        for (const float end : {-1.0F, 1.0F}) {
                            addPlate(side * 0.53F, end * 0.33F,
                                0.045F, 0.045F, 0.029F,
                                {0.76F, 0.69F, 0.52F});
                        }
                    }
                } else if (detailType == 1U) {
                    // Dark cavity, raised frame and slats read as a physical
                    // vent instead of a slightly tinted floor decal.
                    addPlate(0.0F, 0.0F, 1.35F, 0.55F, 0.009F,
                        {0.025F, 0.035F, 0.038F},
                        RoomMaterialChannel::SolidColor);
                    for (const float side : {-1.0F, 1.0F}) {
                        addPlate(0.0F, side * 0.30F, 1.46F, 0.055F,
                            0.052F, {0.43F, 0.49F, 0.50F},
                            RoomMaterialChannel::StructuralColumn);
                        addPlate(side * 0.70F, 0.0F, 0.055F, 0.56F,
                            0.052F, {0.43F, 0.49F, 0.50F},
                            RoomMaterialChannel::StructuralColumn);
                    }
                    for (int slat = -2; slat <= 2; ++slat) {
                        addPlate(static_cast<float>(slat) * 0.24F, 0.0F,
                            0.10F, 0.52F, 0.043F,
                            {0.51F, 0.57F, 0.58F},
                            RoomMaterialChannel::StructuralColumn);
                    }
                } else {
                    // A small repaired seam with restrained amber inspection
                    // paint breaks the large-area floor rhythm.
                    addPlate(0.0F, 0.0F, 1.05F, 0.66F, 0.012F,
                        {0.68F, 0.69F, 0.65F});
                    addPlate(0.0F, -0.31F, 1.04F, 0.035F, 0.019F,
                        {0.83F, 0.50F, 0.21F});
                    addPlate(0.0F, 0.31F, 1.04F, 0.035F, 0.019F,
                        {0.37F, 0.42F, 0.44F});
                }
            }
        }
    };
    drawSecurityFloorDetails(room);
    for (const RoomRenderData& connectedRoom : connectedRooms) {
        drawSecurityFloorDetails(connectedRoom);
    }

    const auto drawTechnicalFloorDetails = [&](const RoomRenderData& renderedRoom) {
        const RoomGpuMaterials& materials = resolveMaterials(renderedRoom.materialKey);
        if (!materials.technicalV1) return;
        const wkc::core::Vec3 center{
            (renderedRoom.worldBounds.minimum.x
                + renderedRoom.worldBounds.maximum.x) * 0.5F,
            renderedRoom.worldBounds.minimum.y,
            (renderedRoom.worldBounds.minimum.z
                + renderedRoom.worldBounds.maximum.z) * 0.5F};
        for (const float localZ : {-5.1F, 5.1F}) {
            const wkc::core::Vec3 offset = wkc::core::rotateQuarterTurns(
                {0.0F, 0.0F, localZ}, renderedRoom.rotation);
            const wkc::core::Vec3 at = center + offset;
            if (!renderedRoom.walkableOutline.empty()
                && !world::footprintContainsDisk(
                    renderedRoom.walkableOutline, at, 0.76F)) {
                continue;
            }
            const bool vent = localZ > 0.0F;
            const auto piece = [&](const float x, const float z,
                const wkc::core::Vec3 size, const wkc::core::Vec3 color) {
                const wkc::core::Vec3 rotated = wkc::core::rotateQuarterTurns(
                    {x, 0.0F, z}, renderedRoom.rotation);
                const bool quarterTurn
                    = renderedRoom.rotation == wkc::core::QuarterTurn::Degrees90
                        || renderedRoom.rotation
                            == wkc::core::QuarterTurn::Degrees270;
                queueSurface(materials,
                    {at.x + rotated.x, at.y + size.y * 0.5F,
                        at.z + rotated.z},
                    quarterTurn
                        ? wkc::core::Vec3{size.z, size.y, size.x} : size,
                    RoomMaterialChannel::SolidColor, 1.0F, color);
            };
            piece(0.0F, 0.0F, {1.35F, 0.012F, 0.72F},
                vent ? wkc::core::Vec3{0.035F, 0.045F, 0.048F}
                     : wkc::core::Vec3{0.22F, 0.25F, 0.25F});
            for (const float side : {-1.0F, 1.0F}) {
                piece(0.0F, side * 0.34F,
                    {1.40F, 0.034F, 0.045F}, {0.38F, 0.41F, 0.40F});
                piece(side * 0.67F, 0.0F,
                    {0.045F, 0.034F, 0.72F}, {0.38F, 0.41F, 0.40F});
                for (const float end : {-1.0F, 1.0F}) {
                    piece(side * 0.58F, end * 0.26F,
                        {0.045F, 0.038F, 0.045F}, {0.15F, 0.16F, 0.16F});
                }
            }
            if (vent) {
                for (int slat = -2; slat <= 2; ++slat) {
                    piece(static_cast<float>(slat) * 0.21F, 0.0F,
                        {0.075F, 0.025F, 0.57F}, {0.48F, 0.51F, 0.49F});
                }
            } else {
                piece(0.0F, 0.0F,
                    {1.02F, 0.017F, 0.48F}, {0.27F, 0.29F, 0.29F});
                piece(0.0F, -0.25F,
                    {0.62F, 0.020F, 0.026F}, {0.47F, 0.34F, 0.16F});
            }
        }
    };
    drawTechnicalFloorDetails(room);
    for (const RoomRenderData& connectedRoom : connectedRooms) {
        drawTechnicalFloorDetails(connectedRoom);
    }

    const auto drawTechnicalCeilingBraces = [&](const RoomRenderData& renderedRoom) {
        const RoomGpuMaterials& materials = resolveMaterials(renderedRoom.materialKey);
        if (!materials.technicalV1) return;
        const auto extent = renderedRoom.worldBounds.maximum
            - renderedRoom.worldBounds.minimum;
        // This layout belongs to the 16 x 20 m Maintenance module. Smaller
        // Technical rooms keep their existing ceiling and fixture spacing.
        if (std::abs(std::min(extent.x, extent.z) - 16.0F) > 0.1F
            || std::abs(std::max(extent.x, extent.z) - 20.0F) > 0.1F
            || extent.y < 5.4F) return;

        const wkc::core::Vec3 origin{
            (renderedRoom.worldBounds.minimum.x
                + renderedRoom.worldBounds.maximum.x) * 0.5F,
            renderedRoom.worldBounds.minimum.y,
            (renderedRoom.worldBounds.minimum.z
                + renderedRoom.worldBounds.maximum.z) * 0.5F};
        const bool quarterTurn = renderedRoom.rotation
                == wkc::core::QuarterTurn::Degrees90
            || renderedRoom.rotation == wkc::core::QuarterTurn::Degrees270;
        const bool lShape = renderedRoom.walkableOutline.size() == 6U;
        const auto piece = [&](const float x, const float z,
            const wkc::core::Vec3 size, const wkc::core::Vec3 color,
            const float drop = 0.075F) {
            const auto offset = wkc::core::rotateQuarterTurns(
                {x, 0.0F, z}, renderedRoom.rotation);
            queueSurface(materials,
                {origin.x + offset.x, renderedRoom.worldBounds.maximum.y - drop,
                    origin.z + offset.z},
                quarterTurn ? wkc::core::Vec3{size.z, size.y, size.x} : size,
                RoomMaterialChannel::SolidColor, 1.0F, color);
            surfaceCommands.back().castsShadow = true;
        };
        constexpr wkc::core::Vec3 brace{0.23F, 0.25F, 0.25F};
        constexpr wkc::core::Vec3 fastener{0.49F, 0.48F, 0.44F};
        // Long braces end at the return wall in the L variant. Cross braces
        // on its narrow side stop before the missing corner, leaving no
        // geometry floating outside the room footprint.
        for (const float x : {-6.0F, -2.0F, 2.0F, 6.0F}) {
            const bool shortened = lShape && x > 0.0F;
            piece(x, shortened ? 4.0F : 0.0F,
                {0.16F, 0.13F, shortened ? 11.8F : 19.8F}, brace);
        }
        for (const float z : {-8.0F, -4.0F, 0.0F, 4.0F, 8.0F}) {
            const bool shortened = lShape && z < -2.0F;
            piece(shortened ? -3.5F : 0.0F, z,
                {shortened ? 8.8F : 15.8F, 0.13F, 0.16F}, brace);
            for (const float x : {-6.0F, -2.0F, 2.0F, 6.0F}) {
                if (shortened && x > 0.0F) continue;
                piece(x, z, {0.075F, 0.015F, 0.075F}, fastener, 0.145F);
            }
        }
    };
    drawTechnicalCeilingBraces(room);
    for (const RoomRenderData& connectedRoom : connectedRooms) {
        drawTechnicalCeilingBraces(connectedRoom);
    }

    const auto drawRoomCeilingBraces = [&](const RoomRenderData& renderedRoom) {
        const RoomGpuMaterials& materials = resolveMaterials(renderedRoom.materialKey);
        const bool storage = materials.storageHallV1;
        const bool cafeteria = materials.cafeteriaV1;
        const bool security = materials.securityV1;
        const bool laboratory = materials.laboratoryPreviewV1;
        const bool hub = materials.centralHubWallCondition.has_value();
        const bool boss = materials.combatBossV1;
        if (!storage && !cafeteria && !security && !laboratory && !hub
            && !boss) return;
        const auto& bounds = renderedRoom.worldBounds;
        const auto extent = bounds.maximum - bounds.minimum;
        // Compact modules keep their simpler ceiling. These braces belong to
        // the spacious room variants, where their 4 m rhythm remains legible.
        if (std::min(extent.x, extent.z) < 14.0F || extent.y < 5.4F) return;

        const std::array<wkc::core::Vec2, 4> rectangle{{
            {bounds.minimum.x, bounds.minimum.z},
            {bounds.maximum.x, bounds.minimum.z},
            {bounds.maximum.x, bounds.maximum.z},
            {bounds.minimum.x, bounds.maximum.z},
        }};
        const std::span<const wkc::core::Vec2> outline
            = renderedRoom.walkableOutline.empty()
                ? std::span<const wkc::core::Vec2>{rectangle}
                : renderedRoom.walkableOutline;
        const wkc::core::Vec3 braceColor = storage
            ? wkc::core::Vec3{0.29F, 0.34F, 0.30F}
            : cafeteria ? wkc::core::Vec3{0.36F, 0.37F, 0.35F}
            : security ? wkc::core::Vec3{0.27F, 0.36F, 0.41F}
            : laboratory ? wkc::core::Vec3{0.40F, 0.51F, 0.55F}
            : boss ? wkc::core::Vec3{0.105F, 0.103F, 0.10F}
                         : wkc::core::Vec3{0.31F, 0.35F, 0.36F};
        const wkc::core::Vec3 fastenerColor = storage
            ? wkc::core::Vec3{0.57F, 0.56F, 0.48F}
            : cafeteria ? wkc::core::Vec3{0.62F, 0.58F, 0.53F}
            : security ? wkc::core::Vec3{0.54F, 0.64F, 0.68F}
            : laboratory ? wkc::core::Vec3{0.70F, 0.77F, 0.78F}
            : boss ? wkc::core::Vec3{0.35F, 0.32F, 0.27F}
                         : wkc::core::Vec3{0.55F, 0.60F, 0.60F};
        const float centerX = (bounds.minimum.x + bounds.maximum.x) * 0.5F;
        const float centerZ = (bounds.minimum.z + bounds.maximum.z) * 0.5F;
        const float ceiling = bounds.maximum.y;
        const auto piece = [&](const wkc::core::Vec3 center,
            const wkc::core::Vec3 size, const wkc::core::Vec3 color) {
            queueSurface(materials, center, size,
                RoomMaterialChannel::SolidColor, 1.0F, color);
            surfaceCommands.back().castsShadow = !boss || size.y >= 0.08F;
        };
        const std::size_t fixtureCount = lighting.visibleFixturesPopulated
            ? lighting.visibleFixtures.size()
            : std::min<std::size_t>(lighting.lightCount, lighting.lights.size());
        const auto fixtureAt = [&](const std::size_t index)
            -> const PointLightRenderData& {
            return lighting.visibleFixturesPopulated
                ? lighting.visibleFixtures[index] : lighting.lights[index];
        };
        const auto roomFixture = [&](const PointLightRenderData& fixture) {
            return fixture.position.x > bounds.minimum.x + 0.2F
                && fixture.position.x < bounds.maximum.x - 0.2F
                && fixture.position.z > bounds.minimum.z + 0.2F
                && fixture.position.z < bounds.maximum.z - 0.2F
                && std::abs(fixture.position.y - ceiling) < 1.6F;
        };
        const auto spans = [&](const bool alongX, const float fixed) {
            std::vector<float> crossings;
            crossings.reserve(outline.size());
            for (std::size_t index = 0U; index < outline.size(); ++index) {
                const auto& a = outline[index];
                const auto& b = outline[(index + 1U) % outline.size()];
                const float aFixed = alongX ? a.y : a.x;
                const float bFixed = alongX ? b.y : b.x;
                if ((aFixed > fixed) == (bFixed > fixed)) continue;
                const float aAlong = alongX ? a.x : a.y;
                const float bAlong = alongX ? b.x : b.y;
                crossings.push_back(aAlong + (fixed - aFixed)
                    * (bAlong - aAlong) / (bFixed - aFixed));
            }
            std::sort(crossings.begin(), crossings.end());
            for (std::size_t index = 0U; index + 1U < crossings.size(); index += 2U) {
                const float start = crossings[index] + 0.16F;
                const float end = crossings[index + 1U] - 0.16F;
                if (end - start < 1.0F) continue;
                std::vector<std::pair<float, float>> clearSpans{{start, end}};
                for (std::size_t fixtureIndex = 0U;
                     fixtureIndex < fixtureCount; ++fixtureIndex) {
                    const PointLightRenderData& fixture = fixtureAt(fixtureIndex);
                    if (!roomFixture(fixture)) continue;
                    const float across = alongX
                        ? fixture.position.z : fixture.position.x;
                    if (std::abs(across - fixed) > 1.2F) continue;
                    const float along = alongX
                        ? fixture.position.x : fixture.position.z;
                    std::vector<std::pair<float, float>> next;
                    for (const auto& [first, last] : clearSpans) {
                        if (along - 1.1F > first + 0.6F) {
                            next.emplace_back(first,
                                std::min(last, along - 1.1F));
                        }
                        if (along + 1.1F < last - 0.6F) {
                            next.emplace_back(std::max(first, along + 1.1F),
                                last);
                        }
                    }
                    clearSpans = std::move(next);
                }
                for (const auto& [first, last] : clearSpans) {
                    if (last - first < 0.6F) continue;
                    const float middle = (first + last) * 0.5F;
                    piece(alongX
                            ? wkc::core::Vec3{middle, ceiling - 0.14F, fixed}
                            : wkc::core::Vec3{fixed, ceiling - 0.075F, middle},
                        alongX ? wkc::core::Vec3{last - first, 0.12F, 0.16F}
                               : wkc::core::Vec3{0.16F, 0.13F, last - first},
                        braceColor);
                }
            }
        };
        const auto forGrid = [&](const float center, const float halfSpan,
            const auto& callback) {
            for (float offset = 2.0F; offset < halfSpan - 0.5F; offset += 4.0F) {
                callback(center - offset);
                callback(center + offset);
            }
        };
        forGrid(centerZ, extent.z * 0.5F,
            [&](const float z) { spans(true, z); });
        forGrid(centerX, extent.x * 0.5F,
            [&](const float x) { spans(false, x); });
        forGrid(centerX, extent.x * 0.5F, [&](const float x) {
            forGrid(centerZ, extent.z * 0.5F, [&](const float z) {
                if (!world::footprintContainsDisk(outline,
                        {x, ceiling, z}, 0.22F)) return;
                for (std::size_t index = 0U; index < fixtureCount; ++index) {
                    const PointLightRenderData& fixture = fixtureAt(index);
                    if (roomFixture(fixture)
                        && std::abs(fixture.position.x - x) < 1.2F
                        && std::abs(fixture.position.z - z) < 1.2F) return;
                }
                piece({x, ceiling - 0.212F, z},
                    {0.075F, 0.018F, 0.075F}, fastenerColor);
            });
        });
    };
    drawRoomCeilingBraces(room);
    for (const RoomRenderData& connectedRoom : connectedRooms) {
        drawRoomCeilingBraces(connectedRoom);
    }

    const auto drawTechnicalServiceWalls = [&](const RoomRenderData& renderedRoom) {
        const RoomGpuMaterials& materials = resolveMaterials(renderedRoom.materialKey);
        if (!materials.technicalV1) return;
        const auto extent = renderedRoom.worldBounds.maximum
            - renderedRoom.worldBounds.minimum;
        if (std::min(extent.x, extent.z) < 15.9F
            || std::max(extent.x, extent.z) < 19.9F
            || extent.y < 5.4F
            || !std::any_of(renderedRoom.architectureParts.begin(),
                renderedRoom.architectureParts.end(),
                [](const world::RoomArchitecturePart& part) {
                    return part.technicalFurniturePart
                        == world::TechnicalFurniturePart::Generator;
                })) {
            return;
        }
        const wkc::core::Vec3 origin{
            (renderedRoom.worldBounds.minimum.x
                + renderedRoom.worldBounds.maximum.x) * 0.5F,
            renderedRoom.worldBounds.minimum.y,
            (renderedRoom.worldBounds.minimum.z
                + renderedRoom.worldBounds.maximum.z) * 0.5F};
        const bool quarterTurn
            = renderedRoom.rotation == wkc::core::QuarterTurn::Degrees90
                || renderedRoom.rotation == wkc::core::QuarterTurn::Degrees270;
        const auto piece = [&](const float x, const float y, const float z,
            const wkc::core::Vec3 size, const wkc::core::Vec3 color) {
            const auto offset = wkc::core::rotateQuarterTurns(
                {x, 0.0F, z}, renderedRoom.rotation);
            queueSurface(materials,
                {origin.x + offset.x, origin.y + y, origin.z + offset.z},
                quarterTurn ? wkc::core::Vec3{size.z, size.y, size.x} : size,
                RoomMaterialChannel::SolidColor, 1.0F, color);
        };
        constexpr wkc::core::Vec3 casing{0.22F, 0.24F, 0.24F};
        constexpr wkc::core::Vec3 steel{0.47F, 0.48F, 0.45F};
        constexpr wkc::core::Vec3 dark{0.055F, 0.065F, 0.066F};
        constexpr wkc::core::Vec3 amber{0.66F, 0.34F, 0.075F};
        // These shallow assemblies are mounted above the player. The service
        // stations below them retain their existing collision and clearances.
        const auto station = [&](const float x, const float z,
            const float facing) {
            const float front = x + facing * 0.10F;
            piece(x, 3.16F, z, {0.11F, 1.34F, 1.72F}, casing);
            piece(front, 3.16F, z, {0.035F, 1.16F, 1.50F}, steel);
            piece(front + facing * 0.022F, 3.20F, z - 0.22F,
                {0.025F, 0.72F, 0.76F}, dark);
            for (int slat = 0; slat < 5; ++slat) {
                piece(front + facing * 0.041F,
                    2.92F + static_cast<float>(slat) * 0.13F,
                    z - 0.22F, {0.018F, 0.026F, 0.65F}, casing);
            }
            piece(front + facing * 0.024F, 3.47F, z + 0.45F,
                {0.03F, 0.19F, 0.30F}, dark);
            piece(front + facing * 0.046F, 3.47F, z + 0.45F,
                {0.016F, 0.075F, 0.20F}, amber);
            for (const float side : {-1.0F, 1.0F}) {
                piece(front + facing * 0.035F, 3.16F,
                    z + side * 0.80F, {0.04F, 1.36F, 0.05F}, dark);
                for (const float y : {2.59F, 3.73F}) {
                    piece(front + facing * 0.05F, y,
                        z + side * 0.72F, {0.035F, 0.07F, 0.07F}, dark);
                }
            }
            piece(front + facing * 0.031F, 2.63F, z,
                {0.025F, 0.08F, 1.45F}, amber);
        };
        station(-7.90F, -6.75F, 1.0F);
        station(-7.90F, 2.85F, 1.0F);
        station(7.90F, 2.85F, -1.0F);
        if (renderedRoom.walkableOutline.size() == 6U) {
            station(0.90F, -6.20F, -1.0F);
        } else {
            station(7.90F, -6.75F, -1.0F);
        }
        for (const float x : {-7.87F, 7.87F}) {
            for (const float z : {-8.35F, 8.35F}) {
                if (x > 0.0F && z < 0.0F
                    && renderedRoom.walkableOutline.size() == 6U) {
                    continue;
                }
                const float facing = x < 0.0F ? 1.0F : -1.0F;
                piece(x, 2.35F, z, {0.13F, 4.32F, 0.14F}, casing);
                piece(x + facing * 0.075F, 2.35F, z,
                    {0.045F, 3.65F, 0.05F}, amber);
                for (const float y : {0.52F, 2.10F, 3.75F}) {
                    piece(x + facing * 0.095F, y, z,
                        {0.08F, 0.12F, 0.24F}, dark);
                }
            }
        }
        if (renderedRoom.walkableOutline.size() == 6U) {
            // The north-facing inner return is the dominant wall from the
            // standard L-room camera. Give it its own large service assembly.
            piece(4.50F, 2.75F, -1.90F,
                {4.25F, 2.55F, 0.11F}, casing);
            piece(4.50F, 2.75F, -1.83F,
                {3.93F, 2.24F, 0.045F}, steel);
            piece(4.50F, 2.89F, -1.80F,
                {2.88F, 1.46F, 0.03F}, dark);
            for (int slat = 0; slat < 8; ++slat) {
                piece(4.50F, 2.37F + static_cast<float>(slat) * 0.13F,
                    -1.77F, {2.70F, 0.031F, 0.025F}, casing);
            }
            for (const float side : {-1.0F, 1.0F}) {
                piece(4.50F + side * 1.78F, 2.75F, -1.77F,
                    {0.085F, 2.30F, 0.055F}, dark);
                for (const float y : {1.67F, 3.82F}) {
                    piece(4.50F + side * 1.78F, y, -1.72F,
                        {0.075F, 0.075F, 0.055F}, amber);
                }
            }
            piece(4.50F, 1.57F, -1.75F,
                {3.90F, 0.10F, 0.06F}, amber);
            piece(4.50F, 4.15F, -1.90F,
                {5.60F, 0.12F, 0.13F}, casing);
        }
        // A continuous exposed cable run connects the panels visually.
        // Its wall-facing size stays small and its height clears every door.
        for (const float x : {-7.88F, 7.88F}) {
            if (x > 0.0F && renderedRoom.walkableOutline.size() == 6U) {
                piece(x, 4.18F, 6.05F, {0.08F, 0.09F, 6.6F}, casing);
                piece(x - 0.03F, 4.18F, 6.05F,
                    {0.04F, 0.025F, 6.6F}, amber);
            } else {
                piece(x, 4.18F, 0.0F, {0.08F, 0.09F, 17.0F}, casing);
                piece(x + (x < 0.0F ? 0.03F : -0.03F), 4.18F, 0.0F,
                    {0.04F, 0.025F, 17.0F}, amber);
            }
        }
        if (renderedRoom.walkableOutline.size() == 6U) {
            piece(0.90F, 4.18F, -6.0F,
                {0.08F, 0.09F, 7.2F}, casing);
            piece(0.86F, 4.18F, -6.0F,
                {0.04F, 0.025F, 7.2F}, amber);
        }
    };
    drawTechnicalServiceWalls(room);
    for (const RoomRenderData& connectedRoom : connectedRooms) {
        drawTechnicalServiceWalls(connectedRoom);
    }

    const auto drawRoomWallTrim = [&](const RoomRenderData& renderedRoom) {
        const RoomGpuMaterials& materials = resolveMaterials(renderedRoom.materialKey);
        if (materials.centralHubWallCondition
            && renderedRoom.walkableOutline.size() == 8U) {
            const auto outline = std::span{renderedRoom.walkableOutline};
            float signedArea = 0.0F;
            for (std::size_t index = 0; index < outline.size(); ++index) {
                const auto& first = outline[index];
                const auto& second = outline[(index + 1U) % outline.size()];
                signedArea += first.x * second.y - second.x * first.y;
            }
            const float interiorSign = signedArea >= 0.0F ? 1.0F : -1.0F;
            const float floorY = renderedRoom.worldBounds.minimum.y;
            const auto addOriented = [&](const wkc::core::Vec3 center,
                const wkc::core::Vec3 dimensions,
                const wkc::core::Vec3 tint, const float yaw,
                const RoomMaterialChannel channel
                    = RoomMaterialChannel::SolidColor) {
                queueSurface(materials, center, dimensions,
                    channel, 1.0F, tint);
                surfaceCommands.back().model = wkc::core::translationMatrix(center)
                    * wkc::core::rotationYMatrix(yaw)
                    * wkc::core::scaleMatrix(dimensions);
            };
            for (std::size_t index = 0; index < outline.size(); ++index) {
                const auto& first = outline[index];
                const auto& second = outline[(index + 1U) % outline.size()];
                const float deltaX = second.x - first.x;
                const float deltaZ = second.y - first.y;
                const float length = std::hypot(deltaX, deltaZ);
                if (length < 0.3F) continue;
                const float directionX = deltaX / length;
                const float directionZ = deltaZ / length;
                const float inwardX = -directionZ * interiorSign;
                const float inwardZ = directionX * interiorSign;
                const float yaw = -std::atan2(directionZ, directionX);
                queueSurface(materials,
                    {first.x + inwardX * 0.10F, floorY + 2.82F,
                        first.y + inwardZ * 0.10F},
                    {0.26F, 5.64F, 0.26F},
                    RoomMaterialChannel::SolidColor, 1.0F,
                    {0.23F, 0.21F, 0.18F});
                addOriented({(first.x + second.x) * 0.5F + inwardX * 0.05F,
                        floorY + 5.63F,
                        (first.y + second.y) * 0.5F + inwardZ * 0.05F},
                    {length, 0.18F, 0.10F},
                    {0.24F, 0.22F, 0.19F}, yaw);
                std::vector<std::pair<float, float>> spans{{0.0F, length}};
                for (const DoorRenderData& door : doors) {
                    const float relativeX = door.position.x - first.x;
                    const float relativeZ = door.position.z - first.y;
                    const float distance = std::abs(relativeX * inwardX
                        + relativeZ * inwardZ);
                    const float along = relativeX * directionX
                        + relativeZ * directionZ;
                    if (distance > 0.25F || along < -door.width
                        || along > length + door.width) continue;
                    const float openingStart = along - door.width * 0.5F - 0.15F;
                    const float openingEnd = along + door.width * 0.5F + 0.15F;
                    std::vector<std::pair<float, float>> remaining;
                    for (const auto& [start, end] : spans) {
                        if (openingEnd <= start || openingStart >= end) {
                            remaining.emplace_back(start, end);
                        } else {
                            if (openingStart > start + 0.02F)
                                remaining.emplace_back(start,
                                    std::min(end, openingStart));
                            if (openingEnd < end - 0.02F)
                                remaining.emplace_back(
                                    std::max(start, openingEnd), end);
                        }
                    }
                    spans = std::move(remaining);
                }
                for (const auto& [start, end] : spans) {
                    const float segmentLength = end - start;
                    if (segmentLength < 0.10F) continue;
                    const float middle = (start + end) * 0.5F;
                    const float x = first.x + directionX * middle;
                    const float z = first.y + directionZ * middle;
                    const auto at = [&](const float y, const float inset) {
                        return wkc::core::Vec3{x + inwardX * inset,
                            floorY + y, z + inwardZ * inset};
                    };
                    addOriented(at(0.49F, 0.030F),
                        {segmentLength, 0.98F, 0.060F},
                        {0.22F, 0.20F, 0.17F}, yaw);
                    addOriented(at(0.17F, 0.070F),
                        {segmentLength, 0.25F, 0.080F},
                        {0.12F, 0.11F, 0.10F}, yaw);
                    addOriented(at(0.99F, 0.071F),
                        {segmentLength, 0.055F, 0.052F},
                        hubRouteLedOrange, yaw);
                    addOriented(at(1.04F, 0.062F),
                        {segmentLength, 0.023F, 0.045F},
                        {0.34F, 0.31F, 0.27F}, yaw);
                    for (float along = std::ceil(start / 2.0F) * 2.0F;
                         along < end - 0.12F; along += 2.0F) {
                        if (along < start + 0.12F) continue;
                        const float boltX = first.x + directionX * along;
                        const float boltZ = first.y + directionZ * along;
                        addOriented({boltX + inwardX * 0.107F,
                                floorY + 0.49F,
                                boltZ + inwardZ * 0.107F},
                            {0.024F, 0.77F, 0.028F},
                            {0.11F, 0.10F, 0.09F}, yaw);
                        for (const float y : {0.19F, 0.83F}) {
                            addOriented({boltX + inwardX * 0.126F,
                                    floorY + y,
                                    boltZ + inwardZ * 0.126F},
                                {0.070F, 0.070F, 0.025F},
                                {0.55F, 0.51F, 0.45F}, yaw);
                        }
                    }
                }
                // Conduits stay on diagonal faces and above the player.
                // Their condition is room-wide; no cable is baked into the tile.
                const bool rerouted = *materials.centralHubWallCondition
                    == CentralHubCondition::Rerouted;
                const bool active = *materials.centralHubWallCondition
                    == CentralHubCondition::Active;
                if ((index & 1U) != 0U
                    && ((active && (index == 1U || index == 5U))
                        || (rerouted && index != 7U))) {
                    const float cx = (first.x + second.x) * 0.5F;
                    const float cz = (first.y + second.y) * 0.5F;
                    const wkc::core::Vec3 center{
                        cx + inwardX * 0.12F, floorY + 3.65F,
                        cz + inwardZ * 0.12F};
                    addOriented(center, {2.20F, 0.065F, 0.075F},
                        {0.27F, 0.25F, 0.22F}, yaw);
                    addOriented(center + wkc::core::Vec3{
                            directionX * 0.62F, -0.50F,
                            directionZ * 0.62F},
                        {0.065F, 1.00F, 0.075F},
                        {0.27F, 0.25F, 0.22F}, yaw);
                    addOriented(center + wkc::core::Vec3{
                            directionX * 0.62F, -0.83F,
                            directionZ * 0.62F},
                        {0.15F, 0.12F, 0.10F},
                        {0.16F, 0.15F, 0.13F}, yaw);
                    addOriented(center + wkc::core::Vec3{
                            -directionX * 0.80F, -0.15F,
                            -directionZ * 0.80F},
                        {0.42F, 0.38F, 0.12F},
                        {0.34F, 0.31F, 0.27F}, yaw);
                }
                if ((index & 1U) != 0U) {
                    // Service ducts follow the diagonal ceiling perimeter.
                    // They are well above the player and clear of cardinal
                    // doors and the four central light fixtures.
                    const wkc::core::Vec3 ductCenter{
                        (first.x + second.x) * 0.5F + inwardX * 0.48F,
                        floorY + 5.75F,
                        (first.y + second.y) * 0.5F + inwardZ * 0.48F};
                    addOriented(ductCenter,
                        {length - 0.72F, 0.20F, 0.28F},
                        {0.34F, 0.31F, 0.27F}, yaw,
                        RoomMaterialChannel::StructuralColumn);
                    addOriented(ductCenter + wkc::core::Vec3{0.0F, -0.113F, 0.0F},
                        {length - 0.82F, 0.026F, 0.30F},
                        {0.13F, 0.12F, 0.11F}, yaw);
                    for (const float side : {-1.0F, 1.0F}) {
                        const wkc::core::Vec3 collarCenter = ductCenter
                            + wkc::core::Vec3{
                                directionX * side * (length * 0.30F), 0.0F,
                                directionZ * side * (length * 0.30F)};
                        addOriented(collarCenter, {0.10F, 0.25F, 0.34F},
                            {0.17F, 0.16F, 0.14F}, yaw);
                        addOriented(collarCenter
                                + wkc::core::Vec3{0.0F, 0.175F, 0.0F},
                            {0.055F, 0.16F, 0.075F},
                            {0.25F, 0.23F, 0.20F}, yaw);
                    }
                    // Shallow wall-mounted service pieces occupy only the
                    // diagonal faces. Cardinal doorways stay unobstructed.
                    const float detailX = (first.x + second.x) * 0.5F;
                    const float detailZ = (first.y + second.y) * 0.5F;
                    const auto wallDetail = [&](const float along,
                        const float wallY, const float inset) {
                        return wkc::core::Vec3{
                            detailX + directionX * along + inwardX * inset,
                            floorY + wallY,
                            detailZ + directionZ * along + inwardZ * inset};
                    };
                    const bool servicePanel = index == 1U || index == 5U;
                    addOriented(wallDetail(0.0F, 2.18F, 0.055F),
                        {1.46F, 1.30F, 0.065F},
                        {0.11F, 0.10F, 0.09F}, yaw);
                    addOriented(wallDetail(0.0F, 2.18F, 0.098F),
                        {1.31F, 1.14F, 0.035F},
                        servicePanel ? wkc::core::Vec3{0.33F, 0.30F, 0.26F}
                                     : wkc::core::Vec3{0.14F, 0.13F, 0.11F},
                        yaw, servicePanel
                            ? RoomMaterialChannel::StructuralColumn
                            : RoomMaterialChannel::SolidColor);
                    for (const float side : {-1.0F, 1.0F}) {
                        for (const float boltY : {1.67F, 2.69F}) {
                            addOriented(wallDetail(side * 0.63F,
                                    boltY, 0.124F),
                                {0.055F, 0.055F, 0.025F},
                                {0.57F, 0.53F, 0.47F}, yaw);
                        }
                    }
                    if (servicePanel) {
                        addOriented(wallDetail(0.0F, 2.69F, 0.124F),
                            {1.18F, 0.035F, 0.025F},
                            {0.13F, 0.12F, 0.10F}, yaw);
                        addOriented(wallDetail(0.47F, 2.15F, 0.126F),
                            {0.11F, 0.39F, 0.032F},
                            {0.10F, 0.09F, 0.08F}, yaw);
                        addOriented(wallDetail(0.47F, 2.15F, 0.148F),
                            {0.045F, 0.17F, 0.020F},
                            {0.53F, 0.49F, 0.43F}, yaw);
                        addOriented(wallDetail(-0.43F, 1.80F, 0.126F),
                            {0.21F, 0.045F, 0.022F},
                            hubRouteLedOrange, yaw);
                    } else {
                        for (int slat = 0; slat < 8; ++slat) {
                            addOriented(wallDetail(0.0F,
                                    1.74F + static_cast<float>(slat) * 0.12F,
                                    0.133F),
                                {1.17F, 0.052F, 0.030F},
                                {0.34F, 0.31F, 0.27F}, yaw);
                        }
                        addOriented(wallDetail(0.0F, 1.64F, 0.125F),
                            {1.16F, 0.033F, 0.025F},
                            hubRouteLedOrange * 0.80F, yaw);
                    }
                    // Paired recessed service bays give the otherwise empty
                    // diagonal walls a readable rhythm. Everything is fixed
                    // to the wall, above floor traffic and away from doors.
                    for (const float side : {-1.0F, 1.0F}) {
                        const float along = side * 1.68F;
                        addOriented(wallDetail(along, 2.18F, 0.070F),
                            {0.55F, 1.28F, 0.080F},
                            {0.085F, 0.078F, 0.068F}, yaw);
                        addOriented(wallDetail(along, 2.18F, 0.116F),
                            {0.43F, 1.12F, 0.024F},
                            {0.32F, 0.29F, 0.25F}, yaw,
                            RoomMaterialChannel::StructuralColumn);
                        addOriented(wallDetail(along, 2.61F, 0.137F),
                            {0.32F, 0.055F, 0.025F},
                            hubRouteLedOrange * 0.75F, yaw);
                        for (const float boltY : {1.66F, 2.70F}) {
                            for (const float boltSide : {-1.0F, 1.0F}) {
                                addOriented(wallDetail(along + boltSide * 0.22F,
                                        boltY, 0.135F),
                                    {0.045F, 0.045F, 0.025F},
                                    {0.56F, 0.51F, 0.44F}, yaw);
                            }
                        }
                    }
                    // A short overhead status rail visually connects the
                    // three bays without turning into a repeated wall tile.
                    addOriented(wallDetail(0.0F, 3.34F, 0.080F),
                        {4.25F, 0.14F, 0.085F},
                        {0.18F, 0.16F, 0.14F}, yaw);
                    for (const float side : {-1.0F, 1.0F}) {
                        addOriented(wallDetail(side * 1.66F, 3.34F, 0.132F),
                            {0.44F, 0.055F, 0.020F},
                            hubRouteLedOrange * 0.65F, yaw);
                    }
                }
            }
            // One maintenance hatch and one vent occupy opposite diagonal
            // quadrants. Their highest parts sit under 4 cm above the floor; both
            // remain walkable and avoid the four cardinal navigation routes.
            const wkc::core::Vec3 hubCenter{
                (renderedRoom.worldBounds.minimum.x
                    + renderedRoom.worldBounds.maximum.x) * 0.5F,
                floorY,
                (renderedRoom.worldBounds.minimum.z
                    + renderedRoom.worldBounds.maximum.z) * 0.5F};
            // Navigation traces are localized geometry. The repeatable metal
            // plate contains no orange stripe that would recur on every tile.
            for (const float direction : {-1.0F, 1.0F}) {
                for (const bool alongX : {false, true}) {
                    const wkc::core::Vec3 traceCenter = hubCenter
                        + wkc::core::Vec3{
                            alongX ? direction * 5.30F : 0.0F,
                            0.012F,
                            alongX ? 0.0F : direction * 5.30F};
                    addOriented(traceCenter,
                        alongX ? wkc::core::Vec3{7.40F, 0.016F, 0.075F}
                               : wkc::core::Vec3{0.075F, 0.016F, 7.40F},
                        {0.055F, 0.052F, 0.045F}, 0.0F);
                    addOriented(traceCenter + wkc::core::Vec3{0.0F, 0.012F, 0.0F},
                        alongX ? wkc::core::Vec3{7.40F, 0.008F, 0.024F}
                               : wkc::core::Vec3{0.024F, 0.008F, 7.40F},
                        hubRouteLedOrange, 0.0F);
                }
            }
            const auto floorPosition = [&](const float x, const float z) {
                return hubCenter + wkc::core::rotateQuarterTurns(
                    {x, 0.0F, z}, renderedRoom.rotation);
            };
            const auto floorDetailOffset = [&](const float x, const float z) {
                constexpr float diagonal = 0.70710678F;
                return wkc::core::rotateQuarterTurns(
                    {(x + z) * diagonal, 0.0F, (z - x) * diagonal},
                    renderedRoom.rotation);
            };
            const float floorYaw = std::numbers::pi_v<float> * 0.25F
                + static_cast<float>(renderedRoom.rotation)
                    * std::numbers::pi_v<float> * 0.5F;
            const wkc::core::Vec3 hatch = floorPosition(-4.4F, 4.0F);
            addOriented(hatch + wkc::core::Vec3{0.0F, 0.008F, 0.0F},
                {1.72F, 0.016F, 1.10F},
                {0.13F, 0.12F, 0.10F}, floorYaw);
            addOriented(hatch + wkc::core::Vec3{0.0F, 0.019F, 0.0F},
                {1.56F, 0.012F, 0.94F},
                {0.74F, 0.70F, 0.63F}, floorYaw,
                RoomMaterialChannel::HubFloorDetail);
            for (const float side : {-1.0F, 1.0F}) {
                addOriented(hatch + wkc::core::Vec3{0.0F, 0.029F, 0.0F}
                        + floorDetailOffset(side * 0.73F, 0.0F),
                    {0.055F, 0.010F, 0.94F},
                    {0.61F, 0.57F, 0.51F}, floorYaw,
                    RoomMaterialChannel::HubFloorDetail);
            }
            for (const float x : {-0.66F, 0.66F}) {
                for (const float z : {-0.39F, 0.39F}) {
                    addOriented(hatch
                            + wkc::core::Vec3{0.0F, 0.030F, 0.0F}
                            + floorDetailOffset(x, z),
                        {0.045F, 0.010F, 0.045F},
                        {0.57F, 0.53F, 0.47F}, floorYaw);
                }
            }
            addOriented(hatch + wkc::core::Vec3{0.0F, 0.029F, 0.0F}
                    + floorDetailOffset(0.42F, 0.0F),
                {0.25F, 0.012F, 0.055F},
                {0.10F, 0.09F, 0.08F}, floorYaw);
            const wkc::core::Vec3 vent = floorPosition(4.2F, -4.1F);
            addOriented(vent + wkc::core::Vec3{0.0F, 0.008F, 0.0F},
                {1.60F, 0.016F, 0.88F},
                {0.045F, 0.042F, 0.038F}, floorYaw);
            for (const float side : {-1.0F, 1.0F}) {
                addOriented(vent + wkc::core::Vec3{0.0F, 0.026F, 0.0F}
                        + floorDetailOffset(0.0F, side * 0.40F),
                    {1.60F, 0.020F, 0.07F},
                    {0.74F, 0.70F, 0.63F}, floorYaw,
                    RoomMaterialChannel::HubFloorDetail);
            }
            for (int slat = -3; slat <= 3; ++slat) {
                addOriented(vent + wkc::core::Vec3{0.0F, 0.024F, 0.0F}
                        + floorDetailOffset(static_cast<float>(slat) * 0.19F, 0.0F),
                    {0.075F, 0.018F, 0.76F},
                    {0.76F, 0.72F, 0.65F}, floorYaw,
                    RoomMaterialChannel::HubFloorDetail);
            }
            for (const wkc::core::Vec3 local
                 : {wkc::core::Vec3{-3.0F, 0.0F, -3.8F},
                    wkc::core::Vec3{3.0F, 0.0F, 3.8F}}) {
                const wkc::core::Vec3 grille = floorPosition(local.x, local.z);
                addOriented(grille + wkc::core::Vec3{0.0F, 5.978F, 0.0F},
                    {1.46F, 0.044F, 0.88F},
                    {0.53F, 0.49F, 0.43F}, floorYaw,
                    RoomMaterialChannel::HubFloorDetail);
                addOriented(grille + wkc::core::Vec3{0.0F, 5.946F, 0.0F},
                    {1.34F, 0.014F, 0.76F},
                    {0.030F, 0.029F, 0.026F}, floorYaw);
                for (const float side : {-1.0F, 1.0F}) {
                    addOriented(grille
                            + wkc::core::Vec3{0.0F, 5.928F, 0.0F}
                            + floorDetailOffset(0.0F, side * 0.40F),
                        {1.46F, 0.035F, 0.060F},
                        {0.70F, 0.66F, 0.59F}, floorYaw,
                        RoomMaterialChannel::HubFloorDetail);
                }
                for (int slat = -3; slat <= 3; ++slat) {
                    addOriented(grille
                            + wkc::core::Vec3{0.0F, 5.930F, 0.0F}
                            + floorDetailOffset(
                                static_cast<float>(slat) * 0.19F, 0.0F),
                        {0.050F, 0.036F, 0.70F},
                        {0.72F, 0.68F, 0.61F}, floorYaw,
                        RoomMaterialChannel::HubFloorDetail);
                }
            }
            return;
        }
        const wkc::core::Vec3 span = renderedRoom.worldBounds.maximum
            - renderedRoom.worldBounds.minimum;
        const bool storageHall = materials.storageHallV1
            && std::min(span.x, span.z) >= 17.5F
            && std::max(span.x, span.z) >= 19.5F && span.y >= 5.8F;
        const bool longCafeteria = materials.cafeteriaV1
            && std::min(span.x, span.z) >= 17.5F
            && std::max(span.x, span.z) >= 23.5F && span.y >= 5.8F;
        if (!materials.securityV1 && !storageHall && !longCafeteria
            && !materials.laboratoryPreviewV1) return;
        const auto& bounds = renderedRoom.worldBounds;
        const std::array<wkc::core::Vec2, 4> rectangularOutline{{
            {bounds.minimum.x, bounds.minimum.z},
            {bounds.maximum.x, bounds.minimum.z},
            {bounds.maximum.x, bounds.maximum.z},
            {bounds.minimum.x, bounds.maximum.z},
        }};
        const std::span<const wkc::core::Vec2> outline
            = renderedRoom.walkableOutline.empty()
                ? std::span<const wkc::core::Vec2>{rectangularOutline}
                : renderedRoom.walkableOutline;
        if (outline.size() < 3U) return;
        const wkc::core::Vec3 trimTint = storageHall
            ? wkc::core::Vec3{0.30F, 0.35F, 0.30F}
            : longCafeteria ? wkc::core::Vec3{0.46F, 0.43F, 0.39F}
            : materials.laboratoryPreviewV1
                ? wkc::core::Vec3{0.52F, 0.65F, 0.68F}
                             : wkc::core::Vec3{0.40F, 0.46F, 0.48F};
        const wkc::core::Vec3 capTint = storageHall
            ? wkc::core::Vec3{0.43F, 0.40F, 0.29F}
            : longCafeteria ? wkc::core::Vec3{0.53F, 0.43F, 0.39F}
            : materials.laboratoryPreviewV1
                ? wkc::core::Vec3{0.71F, 0.78F, 0.77F}
                             : wkc::core::Vec3{0.47F, 0.46F, 0.40F};

        float signedArea = 0.0F;
        for (std::size_t index = 0; index < outline.size(); ++index) {
            const auto& first = outline[index];
            const auto& second = outline[(index + 1U) % outline.size()];
            signedArea += first.x * second.y - second.x * first.y;
        }
        const float interiorSign = signedArea >= 0.0F ? 1.0F : -1.0F;
        const float floorY = renderedRoom.worldBounds.minimum.y;
        for (std::size_t index = 0; index < outline.size(); ++index) {
            const auto& first = outline[index];
            const auto& second = outline[(index + 1U) % outline.size()];
            const float deltaX = second.x - first.x;
            const float deltaZ = second.y - first.y;
            const float length = std::sqrt(deltaX * deltaX + deltaZ * deltaZ);
            if (length < 0.3F) continue;
            const float directionX = deltaX / length;
            const float directionZ = deltaZ / length;
            const float inwardX = -directionZ * interiorSign;
            const float inwardZ = directionX * interiorSign;
            const bool alongX = std::abs(deltaX) >= std::abs(deltaZ);

            const auto& previous = outline[
                (index + outline.size() - 1U) % outline.size()];
            const float previousX = first.x - previous.x;
            const float previousZ = first.y - previous.y;
            const float previousLength = std::hypot(previousX, previousZ);
            if (previousLength > 0.3F) {
                const float cornerX = first.x + 0.055F
                    * (inwardX - previousZ / previousLength * interiorSign);
                const float cornerZ = first.y + 0.055F
                    * (inwardZ + previousX / previousLength * interiorSign);
                // Adjacent trims sit on different inset axes. A small joint
                // block closes their visible gap at convex and concave turns.
                queueSurface(materials, {cornerX, floorY + 0.105F, cornerZ},
                    {0.14F, 0.21F, 0.14F},
                    RoomMaterialChannel::StructuralColumn, 1.0F,
                    trimTint);
                queueSurface(materials, {cornerX, floorY + 0.222F, cornerZ},
                    {0.15F, 0.024F, 0.15F},
                    RoomMaterialChannel::StructuralColumn, 1.0F,
                    capTint);
            }

            std::vector<std::pair<float, float>> spans{{0.0F, length}};
            for (const DoorRenderData& door : doors) {
                const float relativeX = door.position.x - first.x;
                const float relativeZ = door.position.z - first.y;
                const float distanceFromWall = std::abs(
                    relativeX * inwardX + relativeZ * inwardZ);
                const float along = relativeX * directionX + relativeZ * directionZ;
                if (distanceFromWall > 0.25F || along < -door.width
                    || along > length + door.width) {
                    continue;
                }
                const float openingStart = along - door.width * 0.5F - 0.12F;
                const float openingEnd = along + door.width * 0.5F + 0.12F;
                std::vector<std::pair<float, float>> remaining;
                for (const auto& [start, end] : spans) {
                    if (openingEnd <= start || openingStart >= end) {
                        remaining.emplace_back(start, end);
                        continue;
                    }
                    if (openingStart > start + 0.02F) {
                        remaining.emplace_back(start, std::min(end, openingStart));
                    }
                    if (openingEnd < end - 0.02F) {
                        remaining.emplace_back(std::max(start, openingEnd), end);
                    }
                }
                spans = std::move(remaining);
            }

            for (const auto& [start, end] : spans) {
                const float spanLength = end - start;
                if (spanLength < 0.08F) continue;
                const float middle = (start + end) * 0.5F;
                const float x = first.x + directionX * middle + inwardX * 0.055F;
                const float z = first.y + directionZ * middle + inwardZ * 0.055F;
                const auto dimensions = [&](const float thickness,
                    const float trimHeight) {
                    return alongX
                        ? wkc::core::Vec3{spanLength, trimHeight, thickness}
                        : wkc::core::Vec3{thickness, trimHeight, spanLength};
                };
                // Textured physical trim and a narrow worn cap sit against the
                // wall. Their shallow projection leaves the walkable area open.
                queueSurface(materials, {x, floorY + 0.105F, z},
                    dimensions(0.11F, 0.21F),
                    RoomMaterialChannel::StructuralColumn, 1.0F,
                    trimTint);
                queueSurface(materials, {x + inwardX * 0.015F,
                        floorY + 0.222F, z + inwardZ * 0.015F},
                    dimensions(0.13F, 0.024F),
                    RoomMaterialChannel::StructuralColumn, 1.0F,
                    capTint);

                const float firstFastener = std::ceil(start / 2.0F) * 2.0F;
                for (float along = firstFastener; along < end - 0.12F;
                     along += 2.0F) {
                    if (along < start + 0.12F) continue;
                    const float boltX = first.x + directionX * along
                        + inwardX * 0.125F;
                    const float boltZ = first.y + directionZ * along
                        + inwardZ * 0.125F;
                    queueSurface(materials,
                        {boltX, floorY + 0.115F, boltZ},
                        alongX ? wkc::core::Vec3{0.045F, 0.045F, 0.027F}
                               : wkc::core::Vec3{0.027F, 0.045F, 0.045F},
                        RoomMaterialChannel::StructuralColumn, 1.0F,
                        {0.63F, 0.66F, 0.61F});
                }

                // A few shallow service panels interrupt the wall field.
                // Skip occupied stretches so consoles remain unobstructed.
                if (materials.securityV1 && spanLength >= 4.0F) {
                    const float along = start + spanLength
                        * (index % 2U == 0U ? 0.38F : 0.62F);
                    const float panelX = first.x + directionX * along;
                    const float panelZ = first.y + directionZ * along;
                    const bool blocked = std::any_of(
                        renderedRoom.architectureParts.begin(),
                        renderedRoom.architectureParts.end(),
                        [&](const world::RoomArchitecturePart& part) {
                            return part.bounds.minimum.y < floorY + 2.1F
                                && part.bounds.maximum.y > floorY + 1.3F
                                && part.bounds.minimum.x < panelX + 0.5F
                                && part.bounds.maximum.x > panelX - 0.5F
                                && part.bounds.minimum.z < panelZ + 0.5F
                                && part.bounds.maximum.z > panelZ - 0.5F;
                        });
                    if (blocked) continue;
                    const auto panelSize = [&](const float alongSpan,
                        const float depth, const float panelHeight) {
                        return alongX
                            ? wkc::core::Vec3{alongSpan, panelHeight, depth}
                            : wkc::core::Vec3{depth, panelHeight, alongSpan};
                    };
                    const wkc::core::Vec3 panelCenter{
                        panelX + inwardX * 0.055F,
                        floorY + 1.78F,
                        panelZ + inwardZ * 0.055F};
                    queueSurface(materials, panelCenter,
                        panelSize(0.82F, 0.095F, 0.56F),
                        RoomMaterialChannel::StructuralColumn, 1.0F,
                        {0.27F, 0.34F, 0.36F});
                    queueSurface(materials,
                        panelCenter + wkc::core::Vec3{
                            inwardX * 0.055F, 0.0F, inwardZ * 0.055F},
                        panelSize(0.66F, 0.012F, 0.40F),
                        RoomMaterialChannel::StructuralColumn, 1.0F,
                        {0.37F, 0.43F, 0.42F});
                    queueSurface(materials,
                        panelCenter + wkc::core::Vec3{
                            inwardX * 0.067F, -0.14F, inwardZ * 0.067F},
                        panelSize(0.50F, 0.009F, 0.025F),
                        RoomMaterialChannel::StructuralColumn, 1.0F,
                        {0.54F, 0.43F, 0.29F});
                    for (const float side : {-1.0F, 1.0F}) {
                        for (const float row : {-1.0F, 1.0F}) {
                            queueSurface(materials,
                                panelCenter + wkc::core::Vec3{
                                    directionX * side * 0.34F
                                        + inwardX * 0.07F,
                                    row * 0.21F,
                                    directionZ * side * 0.34F
                                        + inwardZ * 0.07F},
                                panelSize(0.035F, 0.018F, 0.035F),
                                RoomMaterialChannel::StructuralColumn, 1.0F,
                                {0.71F, 0.73F, 0.66F});
                        }
                    }
                }
            }
        }
    };
    drawRoomWallTrim(room);
    for (const RoomRenderData& connectedRoom : connectedRooms) {
        drawRoomWallTrim(connectedRoom);
    }

    constexpr float corridorShellThickness = 0.12F;
    for (const CorridorRenderData& corridor : corridors) {
        const RoomGpuMaterials& roomMaterials = resolveMaterials(corridor.materialKey);
        const float corridorHalfWidth = corridor.width * 0.5F;
        const bool transit = corridor.width >= 2.4F;
        const wkc::core::Vec3 routeTint = transit
            ? wkc::core::Vec3{0.12F, 0.51F, 0.56F}
            : wkc::core::Vec3{0.69F, 0.32F, 0.055F};
        const auto corridorDetail = [&](const wkc::core::Vec3 position,
                                         const wkc::core::Vec3 scale,
                                         const wkc::core::Vec3 tint,
                                         const bool shadow = false,
                                         const bool luminous = false) {
            queueSurface(roomMaterials, position, scale,
                luminous ? RoomMaterialChannel::SolidColor
                         : RoomMaterialChannel::StructuralColumn,
                1.0F, tint);
            surfaceCommands.back().castsShadow = shadow;
            if (luminous)
                surfaceCommands.back().style.emissiveStrength = 0.95F;
        };
        for (std::size_t index = 1U; index + 1U < corridor.points.size(); ++index) {
            const wkc::core::Vec3 incoming = wkc::core::normalized(
                corridor.points[index] - corridor.points[index - 1U]);
            const wkc::core::Vec3 outgoing = wkc::core::normalized(
                corridor.points[index + 1U] - corridor.points[index]);
            if (std::abs(wkc::core::dot(incoming, outgoing)) > 0.5F) {
                continue;
            }
            const wkc::core::Vec3 corner = corridor.points[index];
            const wkc::core::Vec3 cornerScale{
                corridor.width, corridorShellThickness, corridor.width};
            queueSurface(roomMaterials,
                corner + wkc::core::Vec3{
                    0.0F, -corridorShellThickness * 0.5F, 0.0F},
                cornerScale, RoomMaterialChannel::Floor, 2.0F,
                {1.0F, 1.0F, 1.0F},
                {corridor.width / 4.0F, corridor.width / 4.0F},
                {corner.x / 4.0F, corner.z / 4.0F});
            queueSurface(roomMaterials,
                corner + wkc::core::Vec3{
                    0.0F, corridor.height + corridorShellThickness * 0.5F, 0.0F},
                cornerScale, RoomMaterialChannel::Ceiling, 1.0F,
                {1.0F, 1.0F, 1.0F},
                {corridor.width / 4.0F, corridor.width / 4.0F},
                {corner.x / 4.0F, corner.z / 4.0F});
            const float ductWidth = transit ? 0.32F : 0.20F;
            corridorDetail(corner + wkc::core::Vec3{
                    0.0F, corridor.height - 0.035F, 0.0F},
                {ductWidth + 0.07F, 0.10F, ductWidth + 0.07F},
                {0.18F, 0.18F, 0.19F}, true);
        }
        for (std::size_t index = 0; index + 1U < corridor.points.size(); ++index) {
            const wkc::core::Vec3 first = corridor.points[index];
            const wkc::core::Vec3 second = corridor.points[index + 1U];
            const bool alongX = wkc::core::nearlyEqual(first.z, second.z);
            const float length = alongX
                ? std::abs(second.x - first.x) : std::abs(second.z - first.z);
            if (length <= 1.0e-4F) {
                continue;
            }
            const wkc::core::Vec3 center = (first + second) * 0.5F;
            const wkc::core::Vec3 floorScale = alongX
                ? wkc::core::Vec3{length, corridorShellThickness, corridor.width}
                : wkc::core::Vec3{corridor.width, corridorShellThickness, length};
            queueSurface(roomMaterials,
                center + wkc::core::Vec3{0.0F, -corridorShellThickness * 0.5F, 0.0F},
                floorScale, RoomMaterialChannel::Floor, 2.0F,
                {1.0F, 1.0F, 1.0F},
                {floorScale.x / 4.0F, floorScale.z / 4.0F},
                {(alongX ? std::min(first.x, second.x) : center.x) / 4.0F,
                    (alongX ? center.z : std::min(first.z, second.z)) / 4.0F});
            queueSurface(roomMaterials, center + wkc::core::Vec3{
                    0.0F, corridor.height + corridorShellThickness * 0.5F, 0.0F},
                floorScale, RoomMaterialChannel::Ceiling, 1.0F,
                {1.0F, 1.0F, 1.0F},
                {floorScale.x / 4.0F, floorScale.z / 4.0F},
                {(alongX ? std::min(first.x, second.x) : center.x) / 4.0F,
                    (alongX ? center.z : std::min(first.z, second.z)) / 4.0F});
            const wkc::core::Vec3 direction = wkc::core::normalized(second - first);
            const wkc::core::Vec3 left{-direction.z, 0.0F, direction.x};
            for (const float sideSign : {-1.0F, 1.0F}) {
                const wkc::core::Vec3 side = left * sideSign;
                wkc::core::Vec3 wallStart = first;
                wkc::core::Vec3 wallEnd = second;
                if (index > 0U) {
                    const wkc::core::Vec3 previousDirection = wkc::core::normalized(
                        first - corridor.points[index - 1U]);
                    if (std::abs(wkc::core::dot(previousDirection, direction)) < 0.5F) {
                        const bool inner = wkc::core::dot(
                            side, previousDirection * -1.0F) > 0.5F;
                        wallStart = wallStart
                            + direction * (inner ? corridorHalfWidth : -corridorHalfWidth);
                    }
                }
                if (index + 2U < corridor.points.size()) {
                    const wkc::core::Vec3 nextDirection = wkc::core::normalized(
                        corridor.points[index + 2U] - second);
                    if (std::abs(wkc::core::dot(direction, nextDirection)) < 0.5F) {
                        const bool inner = wkc::core::dot(side, nextDirection) > 0.5F;
                        wallEnd = wallEnd
                            + direction * (inner ? -corridorHalfWidth : corridorHalfWidth);
                    }
                }
                const float wallLength = alongX
                    ? std::abs(wallEnd.x - wallStart.x)
                    : std::abs(wallEnd.z - wallStart.z);
                if (wallLength <= 1.0e-4F) {
                    continue;
                }
                const wkc::core::Vec3 wallScale = alongX
                    ? wkc::core::Vec3{
                        wallLength, corridor.height, corridorShellThickness}
                    : wkc::core::Vec3{
                        corridorShellThickness, corridor.height, wallLength};
                const wkc::core::Vec3 wallCenter = (wallStart + wallEnd) * 0.5F
                    + side * corridorHalfWidth
                    + wkc::core::Vec3{0.0F, corridor.height * 0.5F, 0.0F};
                queueSurface(roomMaterials, wallCenter,
                    wallScale, RoomMaterialChannel::Wall,
                    1.0F, {1.0F, 1.0F, 1.0F},
                    {wallLength / 4.0F, 1.0F},
                    {(alongX ? std::min(wallStart.x, wallEnd.x)
                             : std::min(wallStart.z, wallEnd.z)) / 4.0F, 0.0F});
                const wkc::core::Vec3 trimCenter = wallCenter
                    - side * 0.095F
                    + wkc::core::Vec3{0.0F,
                        0.15F - corridor.height * 0.5F, 0.0F};
                const wkc::core::Vec3 trimScale = alongX
                    ? wkc::core::Vec3{wallLength, 0.27F, 0.055F}
                    : wkc::core::Vec3{0.055F, 0.27F, wallLength};
                corridorDetail(trimCenter, trimScale,
                    transit ? wkc::core::Vec3{0.17F, 0.20F, 0.22F}
                            : wkc::core::Vec3{0.20F, 0.19F, 0.17F});
                const wkc::core::Vec3 lineScale = alongX
                    ? wkc::core::Vec3{wallLength, 0.022F, 0.021F}
                    : wkc::core::Vec3{0.021F, 0.022F, wallLength};
                corridorDetail(wallCenter - side * 0.082F
                        + wkc::core::Vec3{0.0F,
                            corridor.height * 0.47F
                                - corridor.height * 0.5F, 0.0F},
                    lineScale, routeTint, false, true);
                if (transit) {
                    const wkc::core::Vec3 railScale = alongX
                        ? wkc::core::Vec3{wallLength, 0.062F, 0.085F}
                        : wkc::core::Vec3{0.085F, 0.062F, wallLength};
                    corridorDetail(wallCenter - side * 0.116F
                            + wkc::core::Vec3{0.0F,
                                0.79F - corridor.height * 0.5F, 0.0F},
                        railScale, {0.32F, 0.35F, 0.36F}, true);
                    const int bracketCount = std::clamp(
                        static_cast<int>(std::floor(wallLength / 1.9F)), 1, 6);
                    for (int bracket = 0; bracket < bracketCount; ++bracket) {
                        const float fraction = (static_cast<float>(bracket) + 0.5F)
                            / static_cast<float>(bracketCount);
                        const wkc::core::Vec3 station
                            = wallStart + (wallEnd - wallStart) * fraction
                                + side * (corridorHalfWidth - 0.12F);
                        const wkc::core::Vec3 bracketScale = alongX
                            ? wkc::core::Vec3{0.10F, 0.21F, 0.14F}
                            : wkc::core::Vec3{0.14F, 0.21F, 0.10F};
                        corridorDetail(station + wkc::core::Vec3{0.0F, 0.79F, 0.0F},
                            bracketScale, {0.19F, 0.21F, 0.22F});
                        corridorDetail(station - side * 0.08F
                                + wkc::core::Vec3{0.0F, 0.79F, 0.0F},
                            {0.037F, 0.037F, 0.037F},
                            {0.56F, 0.58F, 0.56F});
                    }
                }
                if (wallLength >= 3.4F && sideSign > 0.0F) {
                    const wkc::core::Vec3 hatchScale = alongX
                        ? wkc::core::Vec3{0.57F, 0.54F, 0.029F}
                        : wkc::core::Vec3{0.029F, 0.54F, 0.57F};
                    const wkc::core::Vec3 hatchCenter = wallCenter
                        - side * 0.095F
                        + wkc::core::Vec3{0.0F,
                            0.58F - corridor.height * 0.5F, 0.0F};
                    corridorDetail(hatchCenter, hatchScale,
                        {0.26F, 0.27F, 0.27F});
                    const wkc::core::Vec3 gripScale = alongX
                        ? wkc::core::Vec3{0.13F, 0.017F, 0.038F}
                        : wkc::core::Vec3{0.038F, 0.017F, 0.13F};
                    corridorDetail(hatchCenter - side * 0.020F,
                        gripScale, {0.10F, 0.11F, 0.11F});
                }
            }
            const float ductWidth = transit ? 0.32F : 0.20F;
            const wkc::core::Vec3 ductScale = alongX
                ? wkc::core::Vec3{length, 0.105F, ductWidth}
                : wkc::core::Vec3{ductWidth, 0.105F, length};
            corridorDetail(center + wkc::core::Vec3{
                    0.0F, corridor.height - 0.035F, 0.0F},
                ductScale, {0.19F, 0.19F, 0.20F}, true);
            const int ductJointCount = std::clamp(
                static_cast<int>(std::floor(length / 1.8F)), 1, 8);
            for (int joint = 0; joint < ductJointCount; ++joint) {
                const float fraction = (static_cast<float>(joint) + 0.5F)
                    / static_cast<float>(ductJointCount);
                const wkc::core::Vec3 station
                    = first + (second - first) * fraction;
                corridorDetail(station + wkc::core::Vec3{
                        0.0F, corridor.height - 0.095F, 0.0F},
                    alongX
                        ? wkc::core::Vec3{0.045F, 0.025F, ductWidth + 0.06F}
                        : wkc::core::Vec3{ductWidth + 0.06F, 0.025F, 0.045F},
                    {0.35F, 0.36F, 0.35F});
            }
            const wkc::core::Vec3 trayScale = alongX
                ? wkc::core::Vec3{length, 0.055F, 0.10F}
                : wkc::core::Vec3{0.10F, 0.055F, length};
            corridorDetail(center + left * (transit ? 0.68F : 0.37F)
                    + wkc::core::Vec3{0.0F, corridor.height - 0.015F, 0.0F},
                trayScale, {0.12F, 0.13F, 0.14F});
            if (length >= 4.5F) {
                const wkc::core::Vec3 hatchCenter
                    = center + wkc::core::Vec3{0.0F, 0.009F, 0.0F};
                const wkc::core::Vec3 hatchFrame = alongX
                    ? wkc::core::Vec3{1.04F, 0.014F, 0.74F}
                    : wkc::core::Vec3{0.74F, 0.014F, 1.04F};
                const wkc::core::Vec3 hatchPlate = alongX
                    ? wkc::core::Vec3{0.91F, 0.016F, 0.61F}
                    : wkc::core::Vec3{0.61F, 0.016F, 0.91F};
                corridorDetail(hatchCenter, hatchFrame,
                    {0.47F, 0.48F, 0.45F});
                corridorDetail(hatchCenter + wkc::core::Vec3{0.0F, 0.011F, 0.0F},
                    hatchPlate, {0.16F, 0.18F, 0.19F});
                for (const float along : {-0.42F, 0.42F}) {
                    for (const float across : {-0.29F, 0.29F}) {
                        corridorDetail(hatchCenter
                                + direction * along + left * across
                                + wkc::core::Vec3{0.0F, 0.026F, 0.0F},
                            {0.035F, 0.015F, 0.035F},
                            {0.63F, 0.62F, 0.56F});
                    }
                }
            }
            if (length >= 2.2F) {
                const int fixtureCount = std::clamp(
                    static_cast<int>(std::floor(length / 3.5F)), 1, 3);
                for (int fixture = 0; fixture < fixtureCount; ++fixture) {
                    const float fraction = (static_cast<float>(fixture) + 0.5F)
                        / static_cast<float>(fixtureCount);
                    const wkc::core::Vec3 mountScale = alongX
                        ? wkc::core::Vec3{0.69F, 0.075F, 0.13F}
                        : wkc::core::Vec3{0.13F, 0.075F, 0.69F};
                    const wkc::core::Vec3 lensScale = alongX
                        ? wkc::core::Vec3{0.61F, 0.019F, 0.075F}
                        : wkc::core::Vec3{0.075F, 0.019F, 0.61F};
                    for (int sideIndex = 0; sideIndex < (transit ? 2 : 1);
                         ++sideIndex) {
                        const float side = sideIndex == 0 ? -1.0F : 1.0F;
                        const wkc::core::Vec3 fixtureCenter = first
                            + (second - first) * fraction
                            + left * side * (transit ? 0.62F : 0.37F)
                            + wkc::core::Vec3{0.0F,
                                corridor.height - 0.075F, 0.0F};
                        corridorDetail(fixtureCenter, mountScale,
                            {0.15F, 0.15F, 0.16F});
                        corridorDetail(fixtureCenter
                                + wkc::core::Vec3{0.0F, -0.048F, 0.0F},
                            lensScale, {0.88F, 0.78F, 0.62F}, false, true);
                    }
                }
            }
        }
        const auto columns = world::makeCorridorWallColumns(
            corridor.points, corridor.width, corridor.height);
        for (const wkc::core::Aabb& column : columns) {
            queueSurface(roomMaterials,
                (column.minimum + column.maximum) * 0.5F,
                column.maximum - column.minimum,
                RoomMaterialChannel::StructuralColumn, 2.0F,
                transit ? wkc::core::Vec3{0.21F, 0.23F, 0.24F}
                        : wkc::core::Vec3{0.24F, 0.23F, 0.21F});
            surfaceCommands.back().castsShadow = true;
        }
        std::vector<bool> pairedColumns(columns.size(), false);
        for (std::size_t index = 0U; index < columns.size(); ++index) {
            if (pairedColumns[index]) continue;
            const wkc::core::Aabb& first = columns[index];
            const bool alongX = first.maximum.x - first.minimum.x
                > first.maximum.z - first.minimum.z;
            const wkc::core::Vec3 firstCenter
                = (first.minimum + first.maximum) * 0.5F;
            std::size_t matchingIndex = columns.size();
            for (std::size_t other = index + 1U; other < columns.size(); ++other) {
                if (pairedColumns[other]) continue;
                const wkc::core::Aabb& candidate = columns[other];
                const bool candidateAlongX
                    = candidate.maximum.x - candidate.minimum.x
                        > candidate.maximum.z - candidate.minimum.z;
                if (candidateAlongX != alongX) continue;
                const wkc::core::Vec3 candidateCenter
                    = (candidate.minimum + candidate.maximum) * 0.5F;
                const float longitudinalDistance = alongX
                    ? std::abs(firstCenter.x - candidateCenter.x)
                    : std::abs(firstCenter.z - candidateCenter.z);
                const float lateralDistance = alongX
                    ? std::abs(firstCenter.z - candidateCenter.z)
                    : std::abs(firstCenter.x - candidateCenter.x);
                const float columnDepth = alongX
                    ? first.maximum.z - first.minimum.z
                    : first.maximum.x - first.minimum.x;
                if (longitudinalDistance < 0.02F
                    && std::abs(lateralDistance
                        - (corridor.width - columnDepth)) < 0.04F) {
                    matchingIndex = other;
                    break;
                }
            }
            if (matchingIndex == columns.size()) continue;
            pairedColumns[index] = pairedColumns[matchingIndex] = true;
            const wkc::core::Aabb& second = columns[matchingIndex];
            const wkc::core::Vec3 transverse = alongX
                ? wkc::core::Vec3{0.0F, 0.0F, 1.0F}
                : wkc::core::Vec3{1.0F, 0.0F, 0.0F};
            const wkc::core::Vec3 portalCenter
                = (first.minimum + first.maximum
                    + second.minimum + second.maximum) * 0.25F;
            const wkc::core::Vec3 minimum{
                std::min(first.minimum.x, second.minimum.x),
                corridor.height - 0.11F,
                std::min(first.minimum.z, second.minimum.z),
            };
            const wkc::core::Vec3 maximum{
                std::max(first.maximum.x, second.maximum.x),
                corridor.height,
                std::max(first.maximum.z, second.maximum.z),
            };
            corridorDetail((minimum + maximum) * 0.5F,
                maximum - minimum, {0.22F, 0.22F, 0.22F}, true);
            if (transit) {
                for (const float side : {-1.0F, 1.0F}) {
                    const wkc::core::Vec3 braceCenter = {
                        portalCenter.x + transverse.x * side * 0.91F,
                        first.minimum.y + corridor.height - 0.34F,
                        portalCenter.z + transverse.z * side * 0.91F,
                    };
                    const wkc::core::Vec3 braceScale = alongX
                        ? wkc::core::Vec3{0.20F, 0.68F, 0.12F}
                        : wkc::core::Vec3{0.12F, 0.68F, 0.20F};
                    corridorDetail(braceCenter, braceScale,
                        {0.29F, 0.31F, 0.32F}, true);
                    surfaceCommands.back().model
                        = wkc::core::translationMatrix(braceCenter)
                            * (alongX
                                ? wkc::core::rotationXMatrix(side * 0.55F)
                                : wkc::core::rotationZMatrix(-side * 0.55F))
                            * wkc::core::scaleMatrix(braceScale);
                    const wkc::core::Vec3 plateCenter = portalCenter
                        + transverse * side * (corridorHalfWidth - 0.035F);
                    const wkc::core::Vec3 plateScale = alongX
                        ? wkc::core::Vec3{0.28F, 0.56F, 0.045F}
                        : wkc::core::Vec3{0.045F, 0.56F, 0.28F};
                    corridorDetail(plateCenter + wkc::core::Vec3{
                            0.0F, -0.45F, 0.0F},
                        plateScale, {0.24F, 0.26F, 0.27F});
                    for (const float boltY : {-0.61F, -0.29F}) {
                        corridorDetail(plateCenter - transverse * side * 0.035F
                                + wkc::core::Vec3{0.0F, boltY, 0.0F},
                            {0.042F, 0.042F, 0.042F},
                            {0.61F, 0.61F, 0.58F});
                    }
                }
            }
        }
    }

    struct GlassTestDome {
        wkc::core::Vec3 center{};
        float radius{0.0F};
        float height{0.0F};
    };
    std::vector<GlassTestDome> glassTestDomes;
    const auto drawRoomParts = [&](const RoomRenderData& renderedRoom) {
        const RoomGpuMaterials& roomMaterials = resolveMaterials(renderedRoom.materialKey);
        for (const world::RoomArchitecturePart& part
            : renderedRoom.architectureParts) {
            architectureCastsShadow = part.solid || part.structuralColumn;
            if (roomMaterials.combatBossV1) {
                // The cabinet doors, bolt heads and grille slats rest on
                // larger shadow casters. Drawing each one into every light's
                // shadow map adds cost without a visible silhouette change.
                architectureCastsShadow = architectureCastsShadow
                    && part.combatPart
                        != world::CombatArchitecturePart::CeilingPanel
                    && part.combatPart
                        != world::CombatArchitecturePart::CeilingGrille
                    && part.combatPart
                        != world::CombatArchitecturePart::GalleryFastener
                    && part.combatPart
                        != world::CombatArchitecturePart::ColumnInset
                    && part.combatPart
                        != world::CombatArchitecturePart::ColumnBrace;
            }
            if (part.breakable) continue;
            const wkc::core::Vec3 position
                = (part.bounds.minimum + part.bounds.maximum) * 0.5F;
            const wkc::core::Vec3 scale
                = part.bounds.maximum - part.bounds.minimum;
            if (part.decoration
                == world::RoomDecorationPart::CentralHubRouteDisplay) {
                // These four solid collision pads remain authored in the
                // world; the screen geometry is drawn with the shaft below.
                continue;
            }
            if (part.decoration
                == world::RoomDecorationPart::CentralHubColumnShaft) {
                const wkc::core::Vec3 base{
                    position.x, renderedRoom.worldBounds.minimum.y, position.z};
                queueSurface(roomMaterials, position, scale,
                    RoomMaterialChannel::StructuralColumn, 1.0F,
                    {0.96F, 0.93F, 0.88F});
                surfaceCommands.back().mesh = &centralHubColumnShaft_;
                surfaceCommands.back().style.hubColumnPlate = true;
                const auto piece = [&](const wkc::core::Vec3 offset,
                    const wkc::core::Vec3 size, const float angle,
                    const wkc::core::Vec3 tint, const float emission = 0.0F,
                    const RoomMaterialChannel channel
                        = RoomMaterialChannel::SolidColor) {
                    queueSurface(roomMaterials, base, size, channel, 1.0F, tint);
                    surfaceCommands.back().model
                        = wkc::core::translationMatrix(base)
                            * wkc::core::rotationYMatrix(angle)
                            * wkc::core::translationMatrix(offset)
                            * wkc::core::scaleMatrix(size);
                    surfaceCommands.back().style.emissiveStrength = emission;
                };
                const auto angledPiece = [&](const wkc::core::Vec3 offset,
                    const wkc::core::Vec3 size, const float angle,
                    const float roll, const wkc::core::Vec3 tint,
                    const float emission) {
                    piece(offset, size, angle, tint, emission);
                    surfaceCommands.back().model
                        = wkc::core::translationMatrix(base)
                            * wkc::core::rotationYMatrix(angle)
                            * wkc::core::translationMatrix(offset)
                            * wkc::core::rotationZMatrix(roll)
                            * wkc::core::scaleMatrix(size);
                };
                constexpr float pi = std::numbers::pi_v<float>;
                const wkc::core::Vec3 seam{0.13F, 0.15F, 0.16F};
                const wkc::core::Vec3 bolt{0.65F, 0.63F, 0.56F};
                for (int face = 0; face < 8; ++face) {
                    const float angle = static_cast<float>(face) * pi / 4.0F;
                    for (const float level : {0.35F, 1.35F, 3.35F, 5.35F}) {
                        piece({0.0F, level, 1.212F},
                            {0.98F, 0.025F, 0.035F}, angle, seam);
                    }
                    for (const float x : {-0.43F, 0.43F}) {
                        for (const float level : {0.70F, 1.55F, 3.75F, 5.04F}) {
                            piece({x, level, 1.226F},
                                {0.055F, 0.055F, 0.028F}, angle, bolt);
                        }
                    }
                    if ((face & 1) != 0) {
                        // Four diagonal bevels carry integrated light slots.
                        piece({0.0F, 2.70F, 1.222F},
                            {0.115F, 3.42F, 0.055F}, angle, seam);
                        piece({0.0F, 2.70F, 1.256F},
                            {0.070F, 3.22F, 0.025F}, angle,
                            hubRouteLedOrange, 1.15F);
                    }
                }
                for (int face = 0; face < 4; ++face) {
                    const float angle = static_cast<float>(face) * pi / 2.0F;
                    piece({0.0F, 2.48F, 1.235F},
                        {0.87F, 1.25F, 0.075F}, angle, seam);
                    piece({0.0F, 2.48F, 1.280F},
                        {0.73F, 1.10F, 0.020F}, angle,
                        {0.045F, 0.035F, 0.025F});
                    piece({0.0F, 2.48F, 1.298F},
                        {0.63F, 0.98F, 0.008F}, angle,
                        {0.025F, 0.020F, 0.015F});
                    for (const float x : {-0.39F, 0.39F}) {
                        for (const float y : {1.92F, 3.04F}) {
                            piece({x, y, 1.288F},
                                {0.043F, 0.043F, 0.026F}, angle, bolt);
                        }
                    }
                    const bool horizontal = face == 1 || face == 3;
                    piece({0.0F, 2.37F, 1.306F},
                        horizontal
                            ? wkc::core::Vec3{0.43F, 0.018F, 0.012F}
                            : wkc::core::Vec3{0.018F, 0.43F, 0.012F},
                        angle, hubRouteLedOrange, 0.75F);
                    for (int node = -1; node <= 1; ++node) {
                        piece({horizontal ? static_cast<float>(node) * 0.20F : 0.0F,
                                horizontal ? 2.37F
                                    : 2.37F + static_cast<float>(node) * 0.20F,
                                1.317F},
                            {0.055F, 0.055F, 0.014F}, angle,
                            hubRouteLedOrange, 0.90F);
                    }
                    if (horizontal) {
                        const float direction = face == 1 ? 1.0F : -1.0F;
                        piece({direction * 0.04F, 2.78F, 1.308F},
                            {0.28F, 0.044F, 0.016F}, angle,
                            hubRouteLedOrange, 0.95F);
                        for (const float side : {-1.0F, 1.0F}) {
                            angledPiece({direction * 0.23F,
                                2.78F + side * 0.075F, 1.308F},
                                {0.16F, 0.035F, 0.016F}, angle,
                                direction * side * pi / 4.0F,
                                hubRouteLedOrange, 0.95F);
                        }
                    } else {
                        piece({0.0F, 2.78F, 1.308F},
                            {0.044F, 0.22F, 0.016F}, angle,
                            hubRouteLedOrange, 0.95F);
                        for (const float side : {-1.0F, 1.0F}) {
                            angledPiece({side * 0.066F, 2.89F, 1.308F},
                                {0.16F, 0.035F, 0.016F}, angle,
                                -side * pi / 4.0F, hubRouteLedOrange, 0.95F);
                        }
                    }
                }
                // Two power leads terminate inside the square plinth, so
                // the visible equipment never widens its collision footprint.
                piece({0.0F, 0.78F, 1.264F},
                    {0.55F, 0.29F, 0.15F}, 0.0F, seam);
                for (const float x : {-0.16F, 0.16F}) {
                    piece({x, 0.47F, 1.37F},
                        {0.075F, 0.45F, 0.075F}, 0.0F,
                        {0.14F, 0.16F, 0.17F});
                    piece({x, 0.80F, 1.351F},
                        {0.12F, 0.12F, 0.08F}, 0.0F, bolt);
                }
                continue;
            }
            if (part.decoration
                    == world::RoomDecorationPart::LaboratoryColumnIndicator
                || part.decoration
                    == world::RoomDecorationPart::IndustrialStatusIndicator) {
                queueSurface(roomMaterials, position, scale,
                    RoomMaterialChannel::SolidColor, 1.0F, part.tint);
                surfaceCommands.back().style.emissiveStrength
                    = part.decoration
                            == world::RoomDecorationPart::IndustrialStatusIndicator
                        ? 0.65F : 1.6F;
                continue;
            }
            if (part.decoration == world::RoomDecorationPart::GlassTestPlanter
                || part.decoration
                    == world::RoomDecorationPart::SmallGlassTestPlanter) {
                const bool small = part.decoration
                    == world::RoomDecorationPart::SmallGlassTestPlanter;
                const float top = part.bounds.maximum.y;
                queueSurface(roomMaterials, position, scale,
                    RoomMaterialChannel::LaboratoryFurniture, 1.0F,
                    {0.55F, 0.69F, 0.70F});
                queueSurface(roomMaterials,
                    {position.x, top + 0.007F, position.z},
                    {scale.x - (small ? 0.08F : 0.12F), 0.014F,
                        scale.z - (small ? 0.08F : 0.12F)},
                    RoomMaterialChannel::SolidColor, 1.0F,
                    {0.045F, 0.075F, 0.070F});
                for (int leaf = 0; leaf < 7; ++leaf) {
                    const float angle = static_cast<float>(leaf)
                        * (2.0F * std::numbers::pi_v<float> / 7.0F);
                    const wkc::core::Vec3 leafBase{
                        position.x + std::cos(angle) * (small ? 0.01F : 0.025F),
                        top + 0.01F,
                        position.z + std::sin(angle) * (small ? 0.01F : 0.025F)};
                    const wkc::core::Vec3 leafScale = small
                        ? wkc::core::Vec3{0.20F,
                            0.17F + 0.015F * static_cast<float>(leaf % 3), 0.20F}
                        : wkc::core::Vec3{0.56F,
                            0.44F + 0.035F * static_cast<float>(leaf % 3), 0.56F};
                    queueSurface(roomMaterials, leafBase, leafScale,
                        RoomMaterialChannel::SolidColor, 1.0F,
                        leaf % 2 == 0
                            ? wkc::core::Vec3{0.24F, 0.52F, 0.38F}
                            : wkc::core::Vec3{0.16F, 0.39F, 0.31F});
                    surfaceCommands.back().mesh = &planterLeafMesh_;
                    surfaceCommands.back().model
                        = wkc::core::translationMatrix(leafBase)
                        * wkc::core::rotationYMatrix(angle)
                        * wkc::core::scaleMatrix(leafScale);
                }
                // The rim and crown make the transparent shell readable even
                // with low graphics settings and without a bright room light.
                queueSurface(roomMaterials,
                    {position.x, top + (small ? 0.012F : 0.025F), position.z},
                    small ? wkc::core::Vec3{0.38F, 0.025F, 0.38F}
                          : wkc::core::Vec3{1.04F, 0.045F, 1.04F},
                    RoomMaterialChannel::StructuralColumn, 1.0F,
                    {0.57F, 0.72F, 0.75F});
                surfaceCommands.back().mesh = &grenadeRing_;
                queueSurface(roomMaterials,
                    {position.x, top + (small ? 0.255F : 0.60F), position.z},
                    small ? wkc::core::Vec3{0.07F, 0.035F, 0.07F}
                          : wkc::core::Vec3{0.14F, 0.055F, 0.14F},
                    RoomMaterialChannel::StructuralColumn, 1.0F,
                    {0.69F, 0.79F, 0.79F});
                glassTestDomes.push_back({
                    {position.x, top + (small ? 0.012F : 0.025F), position.z},
                    small ? 0.20F : 0.54F,
                    small ? 0.24F : 0.57F});
                continue;
            }
            if (part.decoration == world::RoomDecorationPart::Planter
                || part.decoration == world::RoomDecorationPart::BroadleafPlanter) {
                const bool broadleaf = part.decoration
                    == world::RoomDecorationPart::BroadleafPlanter;
                const float top = part.bounds.maximum.y;
                queueSurface(roomMaterials, position, scale,
                    RoomMaterialChannel::StructuralColumn, 1.0F,
                    broadleaf ? wkc::core::Vec3{0.50F, 0.37F, 0.33F}
                              : wkc::core::Vec3{0.38F, 0.43F, 0.43F});
                queueSurface(roomMaterials,
                    {position.x, top + 0.012F, position.z},
                    {scale.x - 0.11F, 0.025F, scale.z - 0.11F},
                    RoomMaterialChannel::SolidColor, 1.0F,
                    {0.065F, 0.085F, 0.065F});
                for (const float side : {-1.0F, 1.0F}) {
                    queueSurface(roomMaterials,
                        {position.x + side * (scale.x * 0.5F - 0.035F),
                            top, position.z},
                        {0.07F, 0.075F, scale.z},
                        RoomMaterialChannel::StructuralColumn, 1.0F,
                        {0.52F, 0.56F, 0.53F});
                    queueSurface(roomMaterials,
                        {position.x, top,
                            position.z + side * (scale.z * 0.5F - 0.035F)},
                        {scale.x, 0.075F, 0.07F},
                        RoomMaterialChannel::StructuralColumn, 1.0F,
                        {0.52F, 0.56F, 0.53F});
                }
                const int leafCount = broadleaf ? 7 : 9;
                for (int leaf = 0; leaf < leafCount; ++leaf) {
                    const float angle = static_cast<float>(leaf)
                        * (2.0F * std::numbers::pi_v<float>
                            / static_cast<float>(leafCount));
                    const float leafHeight = broadleaf
                        ? 0.48F + 0.07F * static_cast<float>(leaf % 3)
                        : leaf == 8 ? 0.92F
                                    : 0.76F + 0.10F * static_cast<float>(leaf % 3);
                    const float leafSpread = broadleaf
                        ? 1.00F + 0.12F * static_cast<float>(leaf % 2)
                        : leaf == 8 ? 0.55F
                                    : 0.78F + 0.08F * static_cast<float>(leaf % 2);
                    const wkc::core::Vec3 leafBase{
                        position.x + std::cos(angle) * 0.055F,
                        top + 0.005F,
                        position.z + std::sin(angle) * 0.055F};
                    const wkc::core::Vec3 leafScale{
                        leafSpread, leafHeight, leafSpread};
                    const wkc::core::Vec3 foliageTint = broadleaf
                        ? (leaf % 2 == 0
                            ? wkc::core::Vec3{0.30F, 0.46F, 0.27F}
                            : wkc::core::Vec3{0.22F, 0.36F, 0.21F})
                        : (leaf % 2 == 0
                            ? wkc::core::Vec3{0.26F, 0.47F, 0.25F}
                            : wkc::core::Vec3{0.18F, 0.35F, 0.23F});
                    queueSurface(roomMaterials, leafBase, leafScale,
                        RoomMaterialChannel::SolidColor, 1.0F, foliageTint);
                    surfaceCommands.back().mesh = &planterLeafMesh_;
                    surfaceCommands.back().model
                        = wkc::core::translationMatrix(leafBase)
                        * wkc::core::rotationYMatrix(angle)
                        * wkc::core::scaleMatrix(leafScale);
                }
                continue;
            }
            if (part.decoration == world::RoomDecorationPart::CeilingDuct
                || part.decoration
                    == world::RoomDecorationPart::CeilingDuctMuted) {
                const bool alongX = scale.x >= scale.z;
                const float length = alongX ? scale.x : scale.z;
                const float ductWidth = alongX ? scale.z : scale.x;
                const float ductTone = roomMaterials.combatGalleryWallV1
                    ? 0.22F
                    : part.decoration
                            == world::RoomDecorationPart::CeilingDuctMuted
                        ? 0.52F : 1.0F;
                queueSurface(roomMaterials, position, scale,
                    RoomMaterialChannel::StructuralColumn, 1.0F,
                    part.decoration
                            == world::RoomDecorationPart::CeilingDuctMuted
                        ? part.tint * 0.70F
                        : roomMaterials.industrialWallCondition
                            ? part.tint
                            : wkc::core::Vec3{0.39F, 0.45F, 0.47F} * ductTone);
                if (roomMaterials.technicalV1) {
                    // A shallow mounting collar makes the duct-to-wall joint
                    // visible without extending into the room's walkable space.
                    for (const float end : {-1.0F, 1.0F}) {
                        const float along = end * (length * 0.5F - 0.055F);
                        queueSurface(roomMaterials,
                            alongX
                                ? wkc::core::Vec3{position.x + along,
                                    position.y, position.z}
                                : wkc::core::Vec3{position.x, position.y,
                                    position.z + along},
                            alongX
                                ? wkc::core::Vec3{0.11F,
                                    scale.y + 0.12F, ductWidth + 0.12F}
                                : wkc::core::Vec3{ductWidth + 0.12F,
                                    scale.y + 0.12F, 0.11F},
                            RoomMaterialChannel::StructuralColumn, 1.0F,
                            part.decoration
                                    == world::RoomDecorationPart::CeilingDuctMuted
                                ? part.tint * 0.90F
                                : wkc::core::Vec3{0.55F, 0.58F, 0.55F});
                    }
                }
                const float hangerGap = renderedRoom.worldBounds.maximum.y
                    - part.bounds.maximum.y;
                if (hangerGap > 0.08F) {
                    for (float offset = 2.0F; offset < length - 1.0F;
                         offset += 4.0F) {
                        const float along = -length * 0.5F + offset;
                        for (const float side : {-1.0F, 1.0F}) {
                            const wkc::core::Vec3 hangerCenter = alongX
                                ? wkc::core::Vec3{
                                    position.x + along,
                                    part.bounds.maximum.y + hangerGap * 0.5F,
                                    position.z + side * ductWidth * 0.29F}
                                : wkc::core::Vec3{
                                    position.x + side * ductWidth * 0.29F,
                                    part.bounds.maximum.y + hangerGap * 0.5F,
                                    position.z + along};
                            queueSurface(roomMaterials, hangerCenter,
                                {0.035F, hangerGap, 0.035F},
                                RoomMaterialChannel::SolidColor, 1.0F,
                                roomMaterials.industrialWallCondition
                                    ? wkc::core::Vec3{0.26F, 0.25F, 0.23F}
                                    : wkc::core::Vec3{0.28F, 0.31F, 0.31F});
                            queueSurface(roomMaterials,
                                {hangerCenter.x,
                                    renderedRoom.worldBounds.maximum.y - 0.016F,
                                    hangerCenter.z},
                                {0.15F, 0.032F, 0.15F},
                                RoomMaterialChannel::StructuralColumn, 1.0F,
                                (roomMaterials.industrialWallCondition
                                    ? wkc::core::Vec3{0.45F, 0.43F, 0.39F}
                                    : wkc::core::Vec3{0.48F, 0.52F, 0.50F})
                                    * ductTone);
                        }
                    }
                }
                for (float offset = 1.5F; offset < length - 0.6F;
                     offset += 2.4F) {
                    const float along = -length * 0.5F + offset;
                    const wkc::core::Vec3 bandCenter = alongX
                        ? wkc::core::Vec3{position.x + along,
                            position.y, position.z}
                        : wkc::core::Vec3{position.x,
                            position.y, position.z + along};
                    queueSurface(roomMaterials, bandCenter,
                        alongX
                            ? wkc::core::Vec3{0.095F, scale.y + 0.04F,
                                ductWidth + 0.05F}
                            : wkc::core::Vec3{ductWidth + 0.05F,
                                scale.y + 0.04F, 0.095F},
                        RoomMaterialChannel::StructuralColumn, 1.0F,
                        (roomMaterials.industrialWallCondition
                            ? wkc::core::Vec3{0.50F, 0.48F, 0.44F}
                            : wkc::core::Vec3{0.56F, 0.59F, 0.56F}) * ductTone);
                }
                for (float offset = 2.8F; offset < length - 1.0F;
                     offset += 4.3F) {
                    const float along = -length * 0.5F + offset;
                    const wkc::core::Vec3 ventCenter = alongX
                        ? wkc::core::Vec3{position.x + along,
                            part.bounds.minimum.y - 0.007F, position.z}
                        : wkc::core::Vec3{position.x,
                            part.bounds.minimum.y - 0.007F,
                            position.z + along};
                    queueSurface(roomMaterials, ventCenter,
                        alongX
                            ? wkc::core::Vec3{1.10F, 0.016F, ductWidth * 0.64F}
                            : wkc::core::Vec3{ductWidth * 0.64F, 0.016F, 1.10F},
                        RoomMaterialChannel::SolidColor, 1.0F,
                        {0.035F, 0.045F, 0.050F});
                    for (int slat = -2; slat <= 2; ++slat) {
                        const float slatOffset = static_cast<float>(slat) * 0.17F;
                        queueSurface(roomMaterials,
                            ventCenter + (alongX
                                ? wkc::core::Vec3{slatOffset,
                                    -0.012F, 0.0F}
                                : wkc::core::Vec3{0.0F,
                                    -0.012F, slatOffset}),
                            alongX
                                ? wkc::core::Vec3{0.06F, 0.022F, ductWidth * 0.64F}
                                : wkc::core::Vec3{ductWidth * 0.64F,
                                    0.022F, 0.06F},
                            RoomMaterialChannel::StructuralColumn, 1.0F,
                            (roomMaterials.industrialWallCondition
                                ? wkc::core::Vec3{0.52F, 0.50F, 0.47F}
                                : wkc::core::Vec3{0.59F, 0.62F, 0.61F})
                                * ductTone);
                    }
                }
                continue;
            }
            const bool texturedRack = part.storageRack
                && roomMaterials.storageRackCondition.has_value();
            const bool texturedFurniture = roomMaterials.cafeteriaV1
                && part.cafeteriaFurniturePart
                    != world::CafeteriaFurniturePart::None;
            const bool securityFurniture = roomMaterials.securityV1
                && part.securityFurniturePart
                    != world::SecurityFurniturePart::None;
            const bool securityWallColumn = roomMaterials.securityV1
                && part.structuralColumn;
            const bool laboratoryFurniture = part.laboratoryFurniturePart
                != world::LaboratoryFurniturePart::None;
            const bool technicalFurniture = roomMaterials.technicalV1
                && part.technicalFurniturePart
                    != world::TechnicalFurniturePart::None
                && part.technicalFurniturePart
                    != world::TechnicalFurniturePart::ServicePipe;
            const bool combatCover = roomMaterials.combatGalleryWallV1
                && (part.combatPart == world::CombatArchitecturePart::Cover
                    || part.combatPart == world::CombatArchitecturePart::CoverFace);
            const bool combatCeilingPanel = roomMaterials.combatGalleryWallV1
                && !roomMaterials.combatBossV1
                && part.combatPart == world::CombatArchitecturePart::CeilingPanel;
            const bool bossCabinetDoor = roomMaterials.combatBossV1
                && part.combatPart == world::CombatArchitecturePart::CeilingPanel;
            const bool combatColumn = roomMaterials.combatGalleryWallV1
                && (bossCabinetDoor
                    || part.combatPart == world::CombatArchitecturePart::ColumnShaft
                    || part.combatPart == world::CombatArchitecturePart::ColumnTrim
                    || part.combatPart == world::CombatArchitecturePart::ColumnInset
                    || part.combatPart == world::CombatArchitecturePart::ColumnBrace
                    || part.combatPart == world::CombatArchitecturePart::GalleryPillar
                    || part.combatPart
                        == world::CombatArchitecturePart::GalleryPillarTrim);
            const bool combatMetal = roomMaterials.combatGalleryWallV1
                && part.combatPart != world::CombatArchitecturePart::None
                && !combatCover && !combatCeilingPanel;
            const bool combatRail = combatMetal
                && (part.combatPart == world::CombatArchitecturePart::GalleryRail
                    || part.combatPart == world::CombatArchitecturePart::StairRail
                    || part.combatPart == world::CombatArchitecturePart::StairRailSlope);
            const bool combatFastener = combatMetal
                && (part.combatPart == world::CombatArchitecturePart::CoverBolt
                    || part.combatPart
                        == world::CombatArchitecturePart::GalleryFastener);
            const bool combatEdge = combatMetal
                && (part.combatPart == world::CombatArchitecturePart::CoverRib
                    || part.combatPart
                        == world::CombatArchitecturePart::GalleryBracket
                    || part.combatPart
                        == world::CombatArchitecturePart::ColumnBrace
                    || part.combatPart
                        == world::CombatArchitecturePart::CeilingBrace);
            const float securityTone = roomMaterials.columnCondition == 0U
                ? 1.18F : roomMaterials.columnCondition == 2U ? 0.94F : 1.05F;
            const float securityPartTone = part.securityFurniturePart
                    == world::SecurityFurniturePart::ObservationSurround
                ? securityTone * 0.78F : securityTone;
            queueSurface(roomMaterials, position, scale,
                combatCover ? RoomMaterialChannel::CombatCover
                    : combatColumn ? RoomMaterialChannel::CombatColumn
                    : combatRail ? RoomMaterialChannel::CombatRail
                    : combatCeilingPanel ? RoomMaterialChannel::Ceiling
                    : combatMetal ? (combatRail || combatFastener
                        ? RoomMaterialChannel::SolidColor
                        : RoomMaterialChannel::StructuralColumn)
                    : securityWallColumn ? RoomMaterialChannel::Wall
                    : part.structuralColumn ? RoomMaterialChannel::StructuralColumn
                    : texturedRack ? RoomMaterialChannel::StorageRack
                    : texturedFurniture ? RoomMaterialChannel::CafeteriaFurniture
                    : laboratoryFurniture
                        ? (part.laboratoryFurniturePart
                                == world::LaboratoryFurniturePart::StatusStrip
                            ? RoomMaterialChannel::SolidColor
                            : RoomMaterialChannel::LaboratoryFurniture)
                    : securityFurniture ? RoomMaterialChannel::SecurityFurniture
                    : technicalFurniture ? RoomMaterialChannel::TechnicalFurniture
                    : roomMaterials.officeV1 ? RoomMaterialChannel::StructuralColumn
                    : RoomMaterialChannel::Generic,
                texturedRack ? 3.0F : 2.0F,
                texturedRack
                    ? (part.storageRackPart
                            == world::StorageRackPart::Upright
                        ? wkc::core::Vec3{0.88F, 1.16F, 1.05F}
                        : wkc::core::Vec3{1.35F, 1.35F, 1.35F})
                    : combatCover ? wkc::core::Vec3{1.0F, 1.0F, 1.0F}
                    : combatCeilingPanel ? wkc::core::Vec3{0.85F, 0.85F, 0.83F}
                    : combatRail ? wkc::core::Vec3{0.34F, 0.20F, 0.060F}
                    : combatFastener ? wkc::core::Vec3{0.43F, 0.34F, 0.18F}
                    : roomMaterials.combatBossV1 && combatColumn
                        ? (bossCabinetDoor
                            ? wkc::core::Vec3{0.70F, 0.67F, 0.61F}
                            : part.combatPart
                                == world::CombatArchitecturePart::ColumnInset
                            ? wkc::core::Vec3{0.78F, 0.73F, 0.66F}
                            : part.combatPart
                                    == world::CombatArchitecturePart::ColumnBrace
                                ? wkc::core::Vec3{0.78F, 0.34F, 0.095F}
                            : part.combatPart
                                    == world::CombatArchitecturePart::ColumnTrim
                                ? wkc::core::Vec3{0.24F, 0.25F, 0.25F}
                                : wkc::core::Vec3{0.24F, 0.25F, 0.26F})
                    : combatColumn ? (part.combatPart
                            == world::CombatArchitecturePart::ColumnInset
                        ? wkc::core::Vec3{0.29F, 0.33F, 0.31F}
                        : (part.combatPart
                                == world::CombatArchitecturePart::GalleryPillar
                            || part.combatPart
                                == world::CombatArchitecturePart::GalleryPillarTrim)
                            ? wkc::core::Vec3{0.21F, 0.25F, 0.23F}
                        : combatEdge
                            ? wkc::core::Vec3{0.24F, 0.28F, 0.26F}
                            : wkc::core::Vec3{0.26F, 0.30F, 0.28F})
                    : combatMetal ? part.tint * (combatEdge ? 0.55F : 0.42F)
                    : texturedFurniture ? wkc::core::Vec3{1.0F, 1.0F, 1.0F}
                    : securityFurniture ? wkc::core::Vec3{
                        securityPartTone, securityPartTone, securityPartTone}
                    : technicalFurniture ? wkc::core::Vec3{1.0F, 1.0F, 1.0F}
                    : roomMaterials.combatBossV1 && part.structuralColumn
                        ? wkc::core::Vec3{0.30F, 0.29F, 0.27F}
                    : securityWallColumn
                        ? wkc::core::Vec3{0.95F, 0.95F, 0.95F}
                        : part.tint,
                {}, {}, texturedRack
                    ? static_cast<std::uint8_t>(
                        *roomMaterials.storageRackCondition)
                    : texturedFurniture ? roomMaterials.cafeteriaCondition
                    : technicalFurniture ? roomMaterials.columnCondition : 0U,
                texturedRack ? part.storageRackPart
                    : world::StorageRackPart::None,
                texturedFurniture ? part.cafeteriaFurniturePart
                    : world::CafeteriaFurniturePart::None);
            if (roomMaterials.combatGalleryWallV1
                && part.combatPart == world::CombatArchitecturePart::StairRailSlope) {
                const auto delta = part.railEnd - part.railStart;
                const float length = std::sqrt(delta.x * delta.x
                    + delta.y * delta.y + delta.z * delta.z);
                const float horizontal = std::sqrt(delta.x * delta.x
                    + delta.z * delta.z);
                if (length > 0.01F && horizontal > 0.01F) {
                    const auto center = (part.railStart + part.railEnd) * 0.5F;
                    const float yaw = std::atan2(delta.x, delta.z);
                    const float pitch = -std::atan2(delta.y, horizontal);
                    surfaceCommands.back().model
                        = wkc::core::translationMatrix(center)
                        * wkc::core::rotationYMatrix(yaw)
                        * wkc::core::rotationXMatrix(pitch)
                        * wkc::core::scaleMatrix({0.09F, 0.09F, length});
                }
            }
            if (roomMaterials.combatGalleryWallV1
                && (part.combatPart == world::CombatArchitecturePart::GalleryRail
                    || part.combatPart == world::CombatArchitecturePart::StairRail
                    || part.combatPart == world::CombatArchitecturePart::StairRailSlope
                    || part.combatPart == world::CombatArchitecturePart::ColumnInset
                    || part.combatPart == world::CombatArchitecturePart::CeilingGrille
                    || part.combatPart == world::CombatArchitecturePart::CoverFace
                    || part.combatPart == world::CombatArchitecturePart::CoverRib
                    || part.combatPart == world::CombatArchitecturePart::CoverBolt
                    || part.combatPart == world::CombatArchitecturePart::ColumnBrace
                    || part.combatPart == world::CombatArchitecturePart::GalleryBracket
                    || part.combatPart
                        == world::CombatArchitecturePart::GalleryFastener
                    || part.combatPart == world::CombatArchitecturePart::CeilingBrace)) {
                // These narrow inlays and rails do not change the broad
                // shadows, but each otherwise costs a cubemap draw per lamp.
                surfaceCommands.back().castsShadow = false;
            }
            if (roomMaterials.combatGalleryWallV1
                && part.combatPart == world::CombatArchitecturePart::Cover
                && std::none_of(renderedRoom.architectureParts.begin(),
                    renderedRoom.architectureParts.end(),
                    [](const world::RoomArchitecturePart& candidate) {
                        return candidate.combatPart
                            == world::CombatArchitecturePart::CoverFrame;
                    })) {
                // The cap and fasteners sit on the same solid footprint as
                // the cover, so bullets and navigation meet the visible form.
                const float top = part.bounds.maximum.y;
                const float bottom = part.bounds.minimum.y;
                const bool alongX = scale.x >= scale.z;
                queueSurface(roomMaterials,
                    {position.x, top - 0.014F, position.z},
                    {scale.x, 0.028F, scale.z},
                    RoomMaterialChannel::SolidColor, 1.0F,
                    {0.31F, 0.34F, 0.32F});
                for (const float faceSide : {-1.0F, 1.0F}) {
                    const float face = alongX
                        ? position.z + faceSide * (scale.z * 0.5F + 0.004F)
                        : position.x + faceSide * (scale.x * 0.5F + 0.004F);
                    const auto faceDetail = [&](const float along,
                        const float verticalCenter, const float length,
                        const float thickness, const wkc::core::Vec3 tint) {
                        queueSurface(roomMaterials,
                            alongX ? wkc::core::Vec3{position.x + along,
                                         verticalCenter, face}
                                   : wkc::core::Vec3{face, verticalCenter,
                                         position.z + along},
                            alongX ? wkc::core::Vec3{length, thickness, 0.014F}
                                   : wkc::core::Vec3{0.014F, thickness, length},
                            RoomMaterialChannel::SolidColor, 1.0F, tint);
                    };
                    const float length = alongX ? scale.x : scale.z;
                    for (const float end : {-1.0F, 1.0F}) {
                        faceDetail(end * (length * 0.5F - 0.07F),
                            (top + bottom) * 0.5F, 0.075F,
                            top - bottom - 0.16F, {0.17F, 0.19F, 0.18F});
                        faceDetail(end * (length * 0.5F - 0.07F),
                            top - 0.16F, 0.045F, 0.045F,
                            {0.49F, 0.50F, 0.45F});
                    }
                    faceDetail(0.0F, top - 0.07F, length - 0.14F, 0.055F,
                        {0.18F, 0.20F, 0.19F});
                    faceDetail(0.0F, bottom + 0.07F, length - 0.14F, 0.055F,
                        {0.15F, 0.17F, 0.16F});
                }
                for (const float side : {-1.0F, 1.0F}) {
                    const wkc::core::Vec3 boltPosition = scale.x >= scale.z
                        ? wkc::core::Vec3{
                            position.x + side * (scale.x * 0.5F - 0.12F),
                            top - 0.008F, position.z}
                        : wkc::core::Vec3{
                            position.x, top - 0.008F,
                            position.z + side * (scale.z * 0.5F - 0.12F)};
                    queueSurface(roomMaterials, boltPosition,
                        {0.075F, 0.016F, 0.075F},
                        RoomMaterialChannel::SolidColor, 1.0F,
                        {0.085F, 0.095F, 0.09F});
                }
            }
            if (roomMaterials.combatGalleryWallV1
                && part.combatPart
                    == world::CombatArchitecturePart::ColumnShaft
                && std::none_of(renderedRoom.architectureParts.begin(),
                    renderedRoom.architectureParts.end(),
                    [](const world::RoomArchitecturePart& candidate) {
                        return candidate.combatPart
                            == world::CombatArchitecturePart::ColumnInset;
                    })) {
                const float shaftHeight = scale.y - 0.60F;
                if (shaftHeight > 0.0F) {
                    for (const float side : {-1.0F, 1.0F}) {
                        queueSurface(roomMaterials,
                            {position.x,
                                (part.bounds.minimum.y
                                    + part.bounds.maximum.y) * 0.5F,
                                position.z + side * (scale.z * 0.5F + 0.004F)},
                            {0.12F, shaftHeight, 0.014F},
                            RoomMaterialChannel::SolidColor, 1.0F,
                            {0.16F, 0.18F, 0.17F});
                    }
                }
            }
            if (technicalFurniture) {
                const auto kind = part.technicalFurniturePart;
                surfaceCommands.back().style.furniturePart
                    = kind == world::TechnicalFurniturePart::BenchTop
                        ? world::CafeteriaFurniturePart::Steel
                        : world::CafeteriaFurniturePart::Support;
                if (kind == world::TechnicalFurniturePart::BenchTop) {
                    surfaceCommands.back().style.surfaceTileMetres = 0.5F;
                }
                const wkc::core::Vec3 min = part.bounds.minimum;
                const wkc::core::Vec3 max = part.bounds.maximum;
                const bool alongZ = scale.z >= scale.x;
                const float roomX = (renderedRoom.worldBounds.minimum.x
                    + renderedRoom.worldBounds.maximum.x) * 0.5F;
                const float roomZ = (renderedRoom.worldBounds.minimum.z
                    + renderedRoom.worldBounds.maximum.z) * 0.5F;
                const float face = alongZ
                    ? (position.x >= roomX ? min.x - 0.006F : max.x + 0.006F)
                    : (position.z >= roomZ ? min.z - 0.006F : max.z + 0.006F);
                const auto detail = [&](const wkc::core::Vec3 center,
                    const wkc::core::Vec3 size,
                    const wkc::core::Vec3 tint) {
                    queueSurface(roomMaterials, center, size,
                        RoomMaterialChannel::SolidColor, 1.0F, tint);
                };
                constexpr wkc::core::Vec3 seam{0.055F, 0.060F, 0.061F};
                constexpr wkc::core::Vec3 edge{0.53F, 0.55F, 0.54F};
                constexpr wkc::core::Vec3 bolt{0.18F, 0.19F, 0.19F};
                if (kind == world::TechnicalFurniturePart::BenchTop) {
                    // Raised metal lip and corner fasteners follow the actual
                    // bench bounds, while the albedo repeats every 0.5 m.
                    for (const float side : {-1.0F, 1.0F}) {
                        detail(alongZ
                                ? wkc::core::Vec3{position.x + side
                                    * (scale.x * 0.5F - 0.018F),
                                    max.y + 0.008F, position.z}
                                : wkc::core::Vec3{position.x,
                                    max.y + 0.008F,
                                    position.z + side
                                        * (scale.z * 0.5F - 0.018F)},
                            alongZ
                                ? wkc::core::Vec3{0.035F, 0.018F, scale.z}
                                : wkc::core::Vec3{scale.x, 0.018F, 0.035F},
                            edge);
                    }
                    for (const float x : {min.x + 0.10F, max.x - 0.10F}) {
                        for (const float z : {min.z + 0.10F, max.z - 0.10F}) {
                            detail({x, max.y + 0.015F, z},
                                {0.055F, 0.028F, 0.055F}, bolt);
                        }
                    }
                    // Parts trays and a tool rail stay within the solid
                    // bench footprint; they make each work station readable
                    // from the player's normal view height.
                    const float trayZ = position.z + scale.z * 0.28F;
                    detail({position.x, max.y + 0.024F, trayZ},
                        {scale.x * 0.56F, 0.045F, 0.28F}, seam);
                    detail({position.x, max.y + 0.051F, trayZ},
                        {scale.x * 0.46F, 0.012F, 0.19F}, edge);
                    for (const float side : {-1.0F, 1.0F}) {
                        detail({position.x + side * scale.x * 0.16F,
                                max.y + 0.063F, trayZ},
                            {0.11F, 0.025F, 0.08F}, bolt);
                    }
                } else if (kind == world::TechnicalFurniturePart::BenchBase) {
                    const float length = alongZ ? scale.z : scale.x;
                    const int drawers = std::max(1,
                        static_cast<int>(std::floor(length / 0.8F)));
                    const float drawerWidth = length / static_cast<float>(drawers);
                    for (int drawer = 0; drawer < drawers; ++drawer) {
                        const float along = -length * 0.5F
                            + drawerWidth * (static_cast<float>(drawer) + 0.5F);
                        const wkc::core::Vec3 panel = alongZ
                            ? wkc::core::Vec3{face, position.y, position.z + along}
                            : wkc::core::Vec3{position.x + along, position.y, face};
                        detail(panel, alongZ
                                ? wkc::core::Vec3{0.014F, scale.y * 0.55F,
                                    drawerWidth - 0.08F}
                                : wkc::core::Vec3{drawerWidth - 0.08F,
                                    scale.y * 0.55F, 0.014F}, seam);
                        detail({panel.x, position.y + 0.10F, panel.z},
                            alongZ
                                ? wkc::core::Vec3{0.028F, 0.045F, 0.22F}
                                : wkc::core::Vec3{0.22F, 0.045F, 0.028F}, edge);
                    }
                } else if (kind == world::TechnicalFurniturePart::Generator
                    && scale.y > 0.8F && scale.x > 0.5F && scale.z > 0.5F) {
                    // The service grille is geometry, so its slots and
                    // fasteners stay crisp without baking them into a tile.
                    const float length = alongZ ? scale.z : scale.x;
                    detail(alongZ
                            ? wkc::core::Vec3{face, position.y, position.z}
                            : wkc::core::Vec3{position.x, position.y, face},
                        alongZ
                            ? wkc::core::Vec3{0.018F, scale.y * 0.53F,
                                length - 0.26F}
                            : wkc::core::Vec3{length - 0.26F,
                                scale.y * 0.53F, 0.018F}, seam);
                    for (int slat = -3; slat <= 3; ++slat) {
                        const float y = position.y + static_cast<float>(slat) * 0.105F;
                        detail(alongZ
                                ? wkc::core::Vec3{face - 0.004F, y, position.z}
                                : wkc::core::Vec3{position.x, y, face - 0.004F},
                            alongZ
                                ? wkc::core::Vec3{0.022F, 0.022F,
                                    length - 0.37F}
                                : wkc::core::Vec3{length - 0.37F,
                                    0.022F, 0.022F}, edge);
                    }
                    for (const float along : {-length * 0.5F + 0.16F,
                             length * 0.5F - 0.16F}) {
                        for (const float y : {position.y - 0.42F,
                                 position.y + 0.42F}) {
                            detail(alongZ
                                    ? wkc::core::Vec3{face - 0.008F,
                                        y, position.z + along}
                                    : wkc::core::Vec3{position.x + along,
                                        y, face - 0.008F},
                                {0.05F, 0.05F, 0.05F}, bolt);
                        }
                    }
                    // A framed turbine housing and service lamps give the
                    // large generator a distinct silhouette in side view.
                    const float facing = position.x >= roomX ? -1.0F : 1.0F;
                    const float front = face + facing * 0.02F;
                    if (alongZ) {
                        detail({front, position.y + 0.18F, position.z},
                            {0.035F, 0.77F, 1.08F}, bolt);
                        detail({front + facing * 0.008F,
                                position.y + 0.18F, position.z},
                            {0.040F, 0.55F, 0.84F}, edge);
                        detail({front + facing * 0.017F,
                                position.y + 0.18F, position.z},
                            {0.043F, 0.33F, 0.61F}, seam);
                        for (const float side : {-1.0F, 1.0F}) {
                            detail({front + facing * 0.024F,
                                    position.y - 0.48F,
                                    position.z + side * 0.36F},
                                {0.045F, 0.065F, 0.28F},
                                {0.68F, 0.37F, 0.085F});
                        }
                    }
                    detail(alongZ
                            ? wkc::core::Vec3{face - 0.03F,
                                position.y + scale.y * 0.32F,
                                position.z + length * 0.33F}
                            : wkc::core::Vec3{position.x + length * 0.33F,
                                position.y + scale.y * 0.32F, face - 0.03F},
                        alongZ ? wkc::core::Vec3{0.045F, 0.12F, 0.22F}
                               : wkc::core::Vec3{0.22F, 0.12F, 0.045F},
                        {0.74F, 0.42F, 0.10F});
                } else if (kind == world::TechnicalFurniturePart::Instrument
                    && scale.y > 0.30F) {
                    // The instrument now has a distinct inset display and
                    // controls instead of reading as a plain cargo box.
                    const auto panel = alongZ
                        ? wkc::core::Vec3{face - 0.01F,
                            position.y + scale.y * 0.12F, position.z}
                        : wkc::core::Vec3{position.x,
                            position.y + scale.y * 0.12F, face - 0.01F};
                    detail(panel,
                        alongZ ? wkc::core::Vec3{0.025F, scale.y * 0.52F,
                            scale.z * 0.68F}
                               : wkc::core::Vec3{scale.x * 0.68F,
                                   scale.y * 0.52F, 0.025F}, seam);
                    detail({panel.x, panel.y + 0.035F, panel.z},
                        alongZ ? wkc::core::Vec3{0.030F, 0.12F, 0.31F}
                               : wkc::core::Vec3{0.31F, 0.12F, 0.030F},
                        {0.035F, 0.056F, 0.057F});
                    detail({panel.x, panel.y - 0.10F, panel.z},
                        alongZ ? wkc::core::Vec3{0.032F, 0.035F, 0.23F}
                               : wkc::core::Vec3{0.23F, 0.035F, 0.032F},
                        {0.68F, 0.38F, 0.10F});
                }
            }
            if (roomMaterials.technicalV1
                && part.technicalFurniturePart
                    == world::TechnicalFurniturePart::ServicePipe) {
                const auto servicePiece = [&](const wkc::core::Vec3 center,
                    const wkc::core::Vec3 size,
                    const wkc::core::Vec3 tint) {
                    queueSurface(roomMaterials, center, size,
                        RoomMaterialChannel::SolidColor, 1.0F, tint);
                };
                constexpr wkc::core::Vec3 tray{0.18F, 0.20F, 0.21F};
                constexpr wkc::core::Vec3 rim{0.43F, 0.44F, 0.40F};
                const bool alongX = scale.x > scale.z;
                const float length = std::max(scale.x, scale.z);
                if (length > 10.0F) {
                    // Shallow cable tray hangs below the existing overhead
                    // pipe. Both runs stay above player and door height.
                    servicePiece({position.x, position.y - 0.22F, position.z},
                        alongX
                            ? wkc::core::Vec3{length - 0.20F, 0.035F, 0.32F}
                            : wkc::core::Vec3{0.32F, 0.035F, length - 0.20F},
                        tray);
                    for (const float side : {-1.0F, 1.0F}) {
                        servicePiece(alongX
                                ? wkc::core::Vec3{position.x,
                                    position.y - 0.16F,
                                    position.z + side * 0.15F}
                                : wkc::core::Vec3{
                                    position.x + side * 0.15F,
                                    position.y - 0.16F, position.z},
                            alongX
                                ? wkc::core::Vec3{length - 0.20F,
                                    0.09F, 0.026F}
                                : wkc::core::Vec3{0.026F, 0.09F,
                                    length - 0.20F}, rim);
                    }
                    for (float along = -length * 0.5F + 1.25F;
                         along < length * 0.5F - 0.50F; along += 2.5F) {
                        servicePiece(alongX
                                ? wkc::core::Vec3{position.x + along,
                                    position.y - 0.22F, position.z}
                                : wkc::core::Vec3{position.x,
                                    position.y - 0.22F, position.z + along},
                            alongX
                                ? wkc::core::Vec3{0.045F, 0.06F, 0.38F}
                                : wkc::core::Vec3{0.38F, 0.06F, 0.045F}, rim);
                    }
                } else if (scale.y > 3.0F) {
                    const float valveY = part.bounds.minimum.y + 3.42F;
                    const float roomX = (renderedRoom.worldBounds.minimum.x
                        + renderedRoom.worldBounds.maximum.x) * 0.5F;
                    const float roomZ = (renderedRoom.worldBounds.minimum.z
                        + renderedRoom.worldBounds.maximum.z) * 0.5F;
                    const bool xWall = std::abs(position.x - roomX)
                        > std::abs(position.z - roomZ);
                    const wkc::core::Vec3 wheelCenter = xWall
                        ? wkc::core::Vec3{position.x
                                + (position.x > roomX ? -0.16F : 0.16F),
                            valveY, position.z}
                        : wkc::core::Vec3{position.x, valveY,
                            position.z
                                + (position.z > roomZ ? -0.16F : 0.16F)};
                    servicePiece(wheelCenter,
                        {0.08F, 0.08F, 0.08F}, tray);
                    for (const float side : {-1.0F, 1.0F}) {
                        servicePiece(wheelCenter
                                + wkc::core::Vec3{0.0F,
                                    side * 0.15F, 0.0F},
                            xWall
                                ? wkc::core::Vec3{0.045F, 0.04F, 0.34F}
                                : wkc::core::Vec3{0.34F, 0.04F, 0.045F}, rim);
                        servicePiece(wheelCenter
                                + (xWall
                                    ? wkc::core::Vec3{0.0F, 0.0F,
                                        side * 0.15F}
                                    : wkc::core::Vec3{side * 0.15F,
                                        0.0F, 0.0F}),
                            xWall
                                ? wkc::core::Vec3{0.045F, 0.34F, 0.04F}
                                : wkc::core::Vec3{0.04F, 0.34F, 0.045F}, rim);
                    }
                }
            }
            if (laboratoryFurniture) {
                const wkc::core::Vec3 min = part.bounds.minimum;
                const wkc::core::Vec3 max = part.bounds.maximum;
                const auto solid = [&](const wkc::core::Vec3 center,
                    const wkc::core::Vec3 size, const wkc::core::Vec3 tint) {
                    queueSurface(roomMaterials, center, size,
                        RoomMaterialChannel::SolidColor, 1.0F, tint);
                };
                const auto lit = [&](const wkc::core::Vec3 center,
                    const wkc::core::Vec3 size, const wkc::core::Vec3 tint) {
                    solid(center, size, tint);
                    surfaceCommands.back().style.emissiveStrength = 1.3F;
                };
                const wkc::core::Vec3 navy{0.12F, 0.20F, 0.26F};
                const wkc::core::Vec3 brushed{0.48F, 0.56F, 0.59F};
                const wkc::core::Vec3 pale{0.72F, 0.78F, 0.79F};
                const float facing = position.x > 0.0F ? -1.0F : 1.0F;
                const float faceX = facing < 0.0F ? min.x : max.x;
                switch (part.laboratoryFurniturePart) {
                case world::LaboratoryFurniturePart::BenchBase: {
                    for (int door = 0; door < 2; ++door) {
                        const float length = (scale.z - 0.18F) * 0.5F;
                        const float doorZ = min.z + 0.09F
                            + (static_cast<float>(door) + 0.5F) * length;
                        solid({faceX + facing * 0.014F, 0.43F, doorZ},
                            {0.024F, 0.68F, length - 0.035F}, navy);
                        solid({faceX + facing * 0.030F, 0.43F, doorZ},
                            {0.025F, 0.62F, length - 0.10F}, pale);
                        solid({faceX + facing * 0.054F, 0.44F,
                                doorZ + length * 0.25F},
                            {0.024F, 0.17F, 0.035F}, brushed);
                        for (const float y : {0.18F, 0.68F}) {
                            solid({faceX + facing * 0.053F, y,
                                    doorZ - length * 0.35F},
                                {0.018F, 0.026F, 0.026F}, brushed);
                        }
                    }
                    break;
                }
                case world::LaboratoryFurniturePart::BenchTop:
                    for (const float x : {min.x + 0.025F, max.x - 0.025F}) {
                        solid({x, max.y + 0.007F, position.z},
                            {0.035F, 0.025F, scale.z}, brushed);
                    }
                    for (const float z : {min.z + 0.025F, max.z - 0.025F}) {
                        solid({position.x, max.y + 0.007F, z},
                            {scale.x - 0.09F, 0.025F, 0.035F}, brushed);
                    }
                    for (const float x : {min.x + 0.10F, max.x - 0.10F}) {
                        for (const float z : {min.z + 0.11F, max.z - 0.11F}) {
                            solid({x, max.y + 0.025F, z},
                                {0.040F, 0.014F, 0.040F}, navy);
                            solid({x, max.y + 0.036F, z},
                                {0.020F, 0.008F, 0.020F}, brushed);
                        }
                    }
                    break;
                case world::LaboratoryFurniturePart::Instrument:
                    solid({faceX + facing * 0.015F, 1.33F, position.z},
                        {0.025F, 0.28F, scale.z - 0.22F}, navy);
                    solid({faceX + facing * 0.032F, 1.34F, position.z},
                        {0.020F, 0.21F, scale.z - 0.33F},
                        {0.08F, 0.21F, 0.26F});
                    for (int line = 0; line < 3; ++line) {
                        lit({faceX + facing * 0.046F,
                                1.28F + static_cast<float>(line) * 0.055F,
                                position.z - 0.35F},
                            {0.012F, 0.012F, 0.30F},
                            {0.19F, 0.72F, 0.75F});
                    }
                    for (int vent = 0; vent < 4; ++vent) {
                        solid({faceX + facing * 0.033F, 1.06F,
                                position.z + 0.20F
                                    + static_cast<float>(vent) * 0.10F},
                            {0.018F, 0.030F, 0.035F}, navy);
                    }
                    break;
                case world::LaboratoryFurniturePart::StatusStrip:
                    surfaceCommands.back().style.emissiveStrength = 1.1F;
                    break;
                case world::LaboratoryFurniturePart::ContainmentBase:
                    for (int vent = 0; vent < 6; ++vent) {
                        solid({faceX + facing * 0.022F, 0.13F,
                                min.z + 0.35F
                                    + static_cast<float>(vent) * 0.39F},
                            {0.020F, 0.045F, 0.15F}, navy);
                    }
                    break;
                case world::LaboratoryFurniturePart::ContainmentBody:
                    for (const float z : {min.z + 0.06F, max.z - 0.06F}) {
                        solid({faceX + facing * 0.030F, position.y, z},
                            {0.065F, scale.y, 0.10F}, brushed);
                    }
                    solid({faceX + facing * 0.030F, max.y - 0.06F, position.z},
                        {0.065F, 0.10F, scale.z - 0.22F}, navy);
                    break;
                case world::LaboratoryFurniturePart::ContainmentPanel:
                    solid({faceX + facing * 0.017F, position.y, position.z},
                        {0.028F, scale.y - 0.06F, scale.z - 0.06F}, navy);
                    solid({faceX + facing * 0.034F, position.y, position.z},
                        {0.025F, scale.y - 0.17F, scale.z - 0.18F},
                        {0.055F, 0.105F, 0.12F});
                    // The sealed viewport now shows two restrained sample
                    // silhouettes and their clamps instead of a flat blue
                    // rectangle. Details stay shallow against the existing
                    // physical cover and outside the firing lane.
                    for (const float offset : {-0.38F, 0.38F}) {
                        solid({faceX + facing * 0.052F,
                                position.y - 0.08F, position.z + offset},
                            {0.045F, 0.48F, 0.17F},
                            {0.17F, 0.50F, 0.48F});
                        for (const float y : {position.y - 0.30F,
                                 position.y + 0.14F}) {
                            solid({faceX + facing * 0.061F,
                                    y, position.z + offset},
                                {0.026F, 0.035F, 0.24F}, brushed);
                        }
                    }
                    solid({faceX + facing * 0.072F,
                            position.y + 0.12F, position.z + 0.12F},
                        {0.012F, scale.y * 0.55F, 0.025F},
                        {0.25F, 0.37F, 0.39F});
                    for (const float z : {min.z + 0.04F, max.z - 0.04F}) {
                        solid({faceX + facing * 0.052F, position.y, z},
                            {0.030F, scale.y, 0.055F}, brushed);
                        for (const float y : {min.y + 0.08F, max.y - 0.08F}) {
                            solid({faceX + facing * 0.073F, y, z},
                                {0.018F, 0.036F, 0.036F}, navy);
                            solid({faceX + facing * 0.085F, y, z},
                                {0.009F, 0.018F, 0.018F}, brushed);
                        }
                    }
                    lit({faceX + facing * 0.051F, max.y - 0.08F, position.z},
                        {0.025F, 0.025F, 0.30F},
                        {0.18F, 0.78F, 0.72F});
                    break;
                case world::LaboratoryFurniturePart::CeilingRib:
                case world::LaboratoryFurniturePart::None:
                    break;
                }
            }
            if (!securityFurniture) continue;

            // Architecture bounds are already in world space. Build furniture
            // details in the room's local frame, then rotate them with the
            // room so their faces remain attached to the collision boxes.
            const wkc::core::Vec3 roomOrigin{
                (renderedRoom.worldBounds.minimum.x
                    + renderedRoom.worldBounds.maximum.x) * 0.5F,
                0.0F,
                (renderedRoom.worldBounds.minimum.z
                    + renderedRoom.worldBounds.maximum.z) * 0.5F};
            const auto inverseRotation = static_cast<wkc::core::QuarterTurn>(
                (4 - static_cast<int>(renderedRoom.rotation)) % 4);
            const auto toWorld = [&](const wkc::core::Vec3 local) {
                return roomOrigin + wkc::core::rotateQuarterTurns(
                    local, renderedRoom.rotation);
            };
            const auto rotateSize = [&](wkc::core::Vec3 size) {
                if (renderedRoom.rotation == wkc::core::QuarterTurn::Degrees90
                    || renderedRoom.rotation == wkc::core::QuarterTurn::Degrees270) {
                    std::swap(size.x, size.z);
                }
                return size;
            };
            const wkc::core::Vec3 localPosition
                = wkc::core::rotateQuarterTurns(position - roomOrigin,
                    inverseRotation);
            const wkc::core::Vec3 localScale = rotateSize(scale);
            const wkc::core::Vec3 localMin = localPosition - localScale * 0.5F;
            const wkc::core::Vec3 localMax = localPosition + localScale * 0.5F;
            const bool mirroredConsole = localPosition.z < 0.0F
                && (part.securityFurniturePart
                        == world::SecurityFurniturePart::ConsoleBase
                    || part.securityFurniturePart
                        == world::SecurityFurniturePart::ConsoleTop
                    || part.securityFurniturePart
                        == world::SecurityFurniturePart::Monitor);
            const auto detailToWorld = [&](wkc::core::Vec3 local) {
                if (mirroredConsole) {
                    local.z = 2.0F * localPosition.z - local.z;
                }
                return toWorld(local);
            };
            const auto metal = [&](const wkc::core::Vec3 center,
                const wkc::core::Vec3 size, const wkc::core::Vec3 tint) {
                queueSurface(roomMaterials, detailToWorld(center), rotateSize(size),
                    RoomMaterialChannel::SecurityFurniture, 1.0F, tint);
            };
            const auto solid = [&](const wkc::core::Vec3 center,
                const wkc::core::Vec3 size, const wkc::core::Vec3 tint) {
                queueSurface(roomMaterials, detailToWorld(center), rotateSize(size),
                    RoomMaterialChannel::SolidColor, 1.0F, tint);
            };
            const SecurityIndicatorVisual indicator = sampleSecurityIndicator(
                renderedRoom.securityIndicatorPhase,
                renderedRoom.securityIndicatorSeconds);
            const auto light = [&](const wkc::core::Vec3 center,
                const wkc::core::Vec3 size, const wkc::core::Vec3 tint) {
                solid(center, size, tint);
                surfaceCommands.back().style.emissiveStrength
                    = indicator.emissiveStrength;
            };
            const wkc::core::Vec3 edge{0.57F, 0.60F, 0.59F};
            const wkc::core::Vec3 gasket{0.035F, 0.043F, 0.047F};
            const auto bolt = [&](const wkc::core::Vec3 center,
                const bool onXFace) {
                solid(center, onXFace
                    ? wkc::core::Vec3{0.025F, 0.055F, 0.055F}
                    : wkc::core::Vec3{0.055F, 0.055F, 0.025F}, edge);
            };
            const wkc::core::Vec3& min = localMin;
            const wkc::core::Vec3& max = localMax;
            switch (part.securityFurniturePart) {
            case world::SecurityFurniturePart::ConsoleTrim:
                // Compact drawer faces already have their final geometry.
                // Reusing ConsoleTop here would add full-size control banks.
                if (min.y > 1.0F) {
                    surfaceCommands.back().style.emissiveStrength
                        = indicator.emissiveStrength;
                }
                break;
            case world::SecurityFurniturePart::Cabinet: {
                const float faceX = localPosition.x < 0.0F
                    ? max.x + 0.022F : min.x - 0.022F;
                metal({localPosition.x, max.y - 0.045F, localPosition.z},
                    {localScale.x + 0.08F, 0.09F, localScale.z + 0.08F},
                    {1.14F, 1.12F, 1.08F});
                solid({faceX, 0.12F, localPosition.z},
                    {0.045F, 0.18F, localScale.z - 0.08F}, gasket);
                for (int bay = 0; bay < 3; ++bay) {
                    const float z = min.z + 0.46F
                        + static_cast<float>(bay) * 0.83F;
                    solid({faceX, 1.15F, z}, {0.05F, 1.78F, 0.75F}, gasket);
                    metal({faceX + (localPosition.x < 0.0F ? 0.027F : -0.027F),
                            1.15F, z},
                        {0.045F, 1.68F, 0.67F},
                        {securityTone * 1.08F, securityTone * 1.08F,
                            securityTone * 1.08F});
                    for (const float y : {0.28F, 1.94F}) {
                        for (const float offset : {-0.28F, 0.28F}) {
                            bolt({faceX + (localPosition.x < 0.0F ? 0.058F : -0.058F),
                                y, z + offset}, true);
                        }
                    }
                    for (int slat = 0; slat < 4; ++slat) {
                        solid({faceX + (localPosition.x < 0.0F ? 0.057F : -0.057F),
                                0.42F + static_cast<float>(slat) * 0.065F, z},
                            {0.015F, 0.023F, 0.39F}, gasket);
                    }
                    metal({faceX + (localPosition.x < 0.0F ? 0.085F : -0.085F),
                            1.10F, z + 0.21F},
                        {0.08F, 0.34F, 0.055F}, edge);
                }
                light({faceX + (localPosition.x < 0.0F ? 0.060F : -0.060F),
                        2.05F, localPosition.z},
                    {0.020F, 0.035F, 0.55F}, indicator.color);
                break;
            }
            case world::SecurityFurniturePart::ConsoleBase: {
                if (localScale.x < 1.0F) {
                    // The compact station faces the room along X. Its three
                    // drawer fronts are authored as separate trim parts.
                    break;
                }
                const float faceZ = min.z - 0.025F;
                metal({localPosition.x, 0.10F, localPosition.z},
                    {localScale.x + 0.08F, 0.20F, localScale.z + 0.08F},
                    {0.75F, 0.77F, 0.77F});
                for (int bay = 0; bay < 4; ++bay) {
                    const float x = min.x + 0.53F
                        + static_cast<float>(bay) * 1.02F;
                    solid({x, 0.51F, faceZ},
                        {0.94F, 0.63F, 0.035F}, gasket);
                    metal({x, 0.51F, faceZ - 0.027F},
                        {0.86F, 0.55F, 0.030F},
                        {securityTone, securityTone, securityTone});
                    solid({x + 0.30F, 0.51F, faceZ - 0.048F},
                        {0.035F, 0.16F, 0.018F}, edge);
                    for (const float dx : {-0.40F, 0.40F}) {
                        bolt({x + dx, 0.23F, faceZ - 0.046F}, false);
                        bolt({x + dx, 0.79F, faceZ - 0.046F}, false);
                    }
                }
                break;
            }
            case world::SecurityFurniturePart::ConsoleTop: {
                if (localScale.x < 1.0F) {
                    metal({localPosition.x, max.y + 0.014F,
                            localPosition.z},
                        {localScale.x + 0.06F, 0.028F,
                            localScale.z + 0.06F},
                        {1.12F, 1.10F, 1.05F});
                    for (int bank = 0; bank < 3; ++bank) {
                        const float z = min.z + 0.25F
                            + static_cast<float>(bank) * 0.37F;
                        solid({max.x - 0.17F, max.y + 0.033F, z},
                            {0.30F, 0.020F, 0.25F}, gasket);
                        light({max.x - 0.17F, max.y + 0.047F, z},
                            {0.13F, 0.010F, 0.045F}, indicator.color);
                    }
                    break;
                }
                metal({localPosition.x, max.y + 0.014F, localPosition.z},
                    {localScale.x + 0.12F, 0.028F, localScale.z + 0.06F},
                    {1.12F, 1.10F, 1.05F});
                for (int bank = 0; bank < 3; ++bank) {
                    const float x = min.x + 0.78F
                        + static_cast<float>(bank) * 1.29F;
                    solid({x, max.y + 0.032F, min.z + 0.34F},
                        {0.96F, 0.020F, 0.42F}, gasket);
                    for (int button = 0; button < 4; ++button) {
                        const float bx = x - 0.33F
                            + static_cast<float>(button) * 0.22F;
                        solid({bx, max.y + 0.050F, min.z + 0.23F},
                            {0.10F, 0.025F, 0.075F}, edge);
                        if (button % 2 == bank % 2) {
                            light({bx, max.y + 0.068F, min.z + 0.23F},
                                {0.045F, 0.008F, 0.035F}, indicator.color);
                        }
                    }
                }
                light({localPosition.x, max.y + 0.035F, max.z - 0.14F},
                    {2.9F, 0.014F, 0.04F}, indicator.color);
                break;
            }
            case world::SecurityFurniturePart::Monitor: {
                if (localScale.x < 1.0F) {
                    const float faceX = max.x + 0.025F;
                    solid({faceX, localPosition.y, localPosition.z},
                        {0.035F, localScale.y - 0.06F,
                            localScale.z - 0.06F}, edge);
                    solid({faceX + 0.023F, localPosition.y,
                            localPosition.z},
                        {0.020F, localScale.y - 0.11F,
                            localScale.z - 0.11F}, gasket);
                    for (int row = 0; row < 3; ++row) {
                        light({faceX + 0.038F,
                                min.y + 0.10F
                                    + static_cast<float>(row) * 0.08F,
                                localPosition.z},
                            {0.008F, 0.015F, localScale.z * 0.48F},
                            indicator.color);
                    }
                    break;
                }
                const float faceZ = min.z - 0.028F;
                metal({localPosition.x, max.y + 0.04F, localPosition.z},
                    {localScale.x + 0.12F, 0.08F, localScale.z + 0.04F}, edge);
                for (int screen = 0; screen < 3; ++screen) {
                    const float x = min.x + (screen == 0 ? 0.44F
                        : screen == 1 ? 1.25F : 2.06F);
                    const float screenWidth = screen == 1 ? 0.86F : 0.62F;
                    solid({x, 1.53F, faceZ},
                        {screenWidth + 0.08F, 0.66F, 0.035F}, edge);
                    solid({x, 1.53F, faceZ - 0.027F},
                        {screenWidth, 0.58F, 0.020F}, gasket);
                    for (int row = 0; row < 4; ++row) {
                        light({x - screenWidth * 0.12F,
                                1.36F + static_cast<float>(row) * 0.10F,
                                faceZ - 0.042F},
                            {screenWidth * (0.30F
                                    + 0.08F * static_cast<float>(row % 3)),
                                0.019F, 0.008F}, indicator.color);
                    }
                    bolt({x - screenWidth * 0.5F - 0.025F,
                            1.84F, faceZ - 0.030F}, false);
                    bolt({x + screenWidth * 0.5F + 0.025F,
                            1.84F, faceZ - 0.030F}, false);
                }
                light({localPosition.x, max.y + 0.045F, faceZ - 0.035F},
                    {localScale.x * 0.55F, 0.025F, 0.018F}, indicator.color);
                break;
            }
            case world::SecurityFurniturePart::ObservationSurround: {
                const float faceX = min.x - 0.035F;
                if (max.y < 1.0F) {
                    // The former wall-like sill reads as three equipment bays.
                    solid({faceX, 0.46F, localPosition.z},
                        {0.040F, 0.68F, localScale.z - 0.20F}, gasket);
                    for (const float y : {0.10F, 0.84F}) {
                        metal({faceX - 0.035F, y, localPosition.z},
                            {0.055F, 0.075F, localScale.z - 0.12F},
                            {1.22F, 1.27F, 1.28F});
                    }
                    for (int bay = 0; bay < 3; ++bay) {
                        const float z = min.z + 0.53F
                            + static_cast<float>(bay) * 0.97F;
                        solid({faceX - 0.028F, 0.46F, z},
                            {0.020F, 0.53F, 0.79F},
                            {0.11F, 0.15F, 0.17F});
                        for (int slat = 0; slat < 4; ++slat) {
                            metal({faceX - 0.042F,
                                    0.31F + static_cast<float>(slat) * 0.10F,
                                    z},
                                {0.012F, 0.020F, 0.55F},
                                {0.95F, 1.02F, 1.04F});
                        }
                        bolt({faceX - 0.044F, 0.74F, z - 0.32F}, true);
                        bolt({faceX - 0.044F, 0.74F, z + 0.32F}, true);
                    }
                } else if (min.y > 3.0F) {
                    metal({faceX - 0.030F, localPosition.y, localPosition.z},
                        {0.060F, 0.075F, localScale.z - 0.08F},
                        {1.22F, 1.27F, 1.28F});
                    for (const float z : {min.z + 0.16F, max.z - 0.16F}) {
                        bolt({faceX - 0.066F, localPosition.y, z}, true);
                    }
                } else {
                    // Narrow side uprights keep the display recessed but no
                    // longer look like full-height structural columns.
                    solid({faceX, 1.72F, localPosition.z},
                        {0.045F, 3.18F, localScale.z - 0.035F},
                        {0.095F, 0.13F, 0.14F});
                    metal({faceX - 0.030F, 1.72F, localPosition.z},
                        {0.025F, 3.14F, 0.030F},
                        {1.24F, 1.30F, 1.32F});
                    for (const float y : {0.30F, 1.33F, 2.36F, 3.22F}) {
                        bolt({faceX - 0.052F, y, localPosition.z}, true);
                    }
                }
                break;
            }
            case world::SecurityFurniturePart::ObservationPanel: {
                // The displays sit on the recessed backing, behind the
                // structural piers rather than flush with the room wall.
                const float faceX = min.x - 0.055F;
                solid({faceX, 2.20F, localPosition.z},
                    {0.065F, 1.72F, localScale.z - 0.24F}, gasket);
                // A slim four-piece steel frame replaces the broad, dark
                // border without changing the display faces or their state.
                for (const float y : {1.39F, 3.02F}) {
                    metal({faceX - 0.095F, y, localPosition.z},
                        {0.065F, 0.075F, localScale.z - 0.32F},
                        {1.42F, 1.46F, 1.46F});
                }
                for (const float z : {min.z + 0.18F, max.z - 0.18F}) {
                    metal({faceX - 0.095F, 2.205F, z},
                        {0.065F, 1.56F, 0.075F},
                        {1.42F, 1.46F, 1.46F});
                    for (const float y : {1.39F, 3.02F}) {
                        bolt({faceX - 0.133F, y, z}, true);
                    }
                }
                for (int screen = 0; screen < 3; ++screen) {
                    const float z = min.z + 0.55F
                        + static_cast<float>(screen) * 0.95F;
                    metal({faceX - 0.065F, 2.24F, z},
                        {0.06F, 1.47F, 0.83F}, edge);
                    solid({faceX - 0.102F, 2.24F, z},
                        {0.020F, 1.34F, 0.73F},
                        {0.012F, 0.027F, 0.034F});
                    for (int row = 0; row < 5; ++row) {
                        solid({faceX - 0.115F,
                                1.86F + static_cast<float>(row) * 0.15F,
                                z - 0.17F},
                            {0.008F, 0.025F,
                                0.22F + static_cast<float>((row + screen) % 3)
                                    * 0.11F},
                            {0.07F, 0.31F, 0.38F});
                        surfaceCommands.back().style.emissiveStrength = 0.38F;
                    }
                    light({faceX - 0.115F, 2.76F, z},
                        {0.010F, 0.037F, 0.44F}, indicator.color);
                    for (const float y : {1.43F, 3.00F}) {
                        for (const float dz : {-0.36F, 0.36F}) {
                            bolt({faceX - 0.101F, y, z + dz}, true);
                        }
                    }
                }
                break;
            }
            case world::SecurityFurniturePart::Barricade: {
                for (const float side : {-1.0F, 1.0F}) {
                    const float faceZ = side < 0.0F
                        ? min.z - 0.030F : max.z + 0.030F;
                    solid({localPosition.x, 0.62F, faceZ},
                        {localScale.x - 0.22F, 0.70F, 0.025F}, gasket);
                    metal({localPosition.x, 0.62F,
                            faceZ + side * 0.025F},
                        {localScale.x - 0.30F, 0.62F, 0.025F},
                        {securityTone * 1.06F, securityTone * 1.06F,
                            securityTone * 1.06F});
                    for (int slat = 0; slat < 5; ++slat) {
                        solid({localPosition.x,
                                0.39F + static_cast<float>(slat) * 0.065F,
                                faceZ + side * 0.043F},
                            {0.66F, 0.018F, 0.015F}, gasket);
                    }
                    for (const float x : {min.x + 0.17F, max.x - 0.17F}) {
                        for (const float y : {0.30F, 0.94F}) {
                            bolt({x, y, faceZ + side * 0.048F}, false);
                        }
                    }
                    for (int stripe = 0; stripe < 4; ++stripe) {
                        const float x = min.x + 0.36F
                            + static_cast<float>(stripe) * 0.29F;
                        solid({x, 0.17F, faceZ + side * 0.060F},
                            {0.12F, 0.055F, 0.018F},
                            {0.78F, 0.48F, 0.09F});
                        surfaceCommands.back().model
                            = wkc::core::translationMatrix(
                                toWorld({x, 0.17F, faceZ + side * 0.060F}))
                            * wkc::core::rotationYMatrix(
                                static_cast<float>(renderedRoom.rotation)
                                    * std::numbers::pi_v<float> * 0.5F)
                            * wkc::core::rotationZMatrix(-0.42F)
                            * wkc::core::scaleMatrix({0.12F, 0.055F, 0.018F});
                    }
                }
                // The brace projects beyond the collision box. Its side and
                // top must not share a plane with the box faces: that caused
                // the flickering stripes visible at close range.
                for (const float x : {min.x + 0.04F, max.x - 0.04F}) {
                    metal({x, 0.58F, localPosition.z},
                        {0.18F, 1.16F, localScale.z + 0.06F},
                        {0.82F, 0.84F, 0.83F});
                }
                for (const float xOffset : {-0.45F, 0.0F, 0.45F}) {
                    light({localPosition.x + xOffset, 1.112F, min.z + 0.16F},
                        {0.24F, 0.024F, 0.045F}, indicator.color);
                }
                break;
            }
            case world::SecurityFurniturePart::None:
                break;
            }
        }
        architectureCastsShadow = false;
    };
    drawRoomParts(room);
    for (const RoomRenderData& connectedRoom : connectedRooms) {
        drawRoomParts(connectedRoom);
    }

    const auto drawBossArenaDetails = [&](const RoomRenderData& renderedRoom) {
        const RoomGpuMaterials& materials = resolveMaterials(renderedRoom.materialKey);
        if (!materials.combatBossV1) return;
        const bool quarterTurn = renderedRoom.rotation
                == wkc::core::QuarterTurn::Degrees90
            || renderedRoom.rotation == wkc::core::QuarterTurn::Degrees270;
        const auto piece = [&](const wkc::core::Vec3 local,
            wkc::core::Vec3 size, const wkc::core::Vec3 tint,
            const bool shadow = false) {
            const wkc::core::Vec3 center = renderedRoom.origin
                + wkc::core::rotateQuarterTurns(local, renderedRoom.rotation);
            if (quarterTurn) std::swap(size.x, size.z);
            queueSurface(materials, center, size,
                RoomMaterialChannel::SolidColor, 1.0F, tint);
            surfaceCommands.back().castsShadow = shadow;
        };
        constexpr wkc::core::Vec3 graphite{0.105F, 0.101F, 0.096F};
        constexpr wkc::core::Vec3 rim{0.27F, 0.25F, 0.22F};
        constexpr wkc::core::Vec3 steel{0.47F, 0.44F, 0.39F};
        constexpr wkc::core::Vec3 amber{0.62F, 0.31F, 0.075F};
        constexpr wkc::core::Vec3 bolt{0.58F, 0.54F, 0.47F};
        const float floor = renderedRoom.worldBounds.minimum.y
            - renderedRoom.origin.y;
        const float ceiling = renderedRoom.worldBounds.maximum.y
            - renderedRoom.origin.y;

        // Two clear, walkable route outlines replace a repeated warning mark
        // in every floor tile. Their shallow inlays do not change collision.
        for (const float side : {-1.0F, 1.0F}) {
            for (const float x : {side * 4.25F, side * 11.20F}) {
                piece({x, floor + 0.010F, 0.0F},
                    {0.14F, 0.014F, 21.0F}, graphite);
                piece({x, floor + 0.019F, 0.0F},
                    {0.062F, 0.012F, 20.95F}, amber);
            }
            for (const float z : {-10.50F, 10.50F}) {
                piece({side * 7.725F, floor + 0.010F, z},
                    {7.05F, 0.014F, 0.14F}, graphite);
                piece({side * 7.725F, floor + 0.019F, z},
                    {7.02F, 0.012F, 0.062F}, amber);
            }
        }

        const auto wallPiece = [&](const bool xWall, const float side,
            const float along, const float y, const float length,
            const float panelHeight, const float depth,
            const wkc::core::Vec3 tint, const bool shadow = false) {
            piece(xWall
                    ? wkc::core::Vec3{side * 13.68F, floor + y, along}
                    : wkc::core::Vec3{along, floor + y, side * 15.68F},
                xWall ? wkc::core::Vec3{depth, panelHeight, length}
                      : wkc::core::Vec3{length, panelHeight, depth},
                tint, shadow);
        };
        for (const bool xWall : {false, true}) {
            for (const float side : {-1.0F, 1.0F}) {
                const float halfLength = xWall ? 15.5F : 13.5F;
                wallPiece(xWall, side, 0.0F, 5.25F,
                    halfLength * 2.0F, 0.20F, 0.20F, graphite, true);
                for (const float segment : {-1.0F, 1.0F}) {
                    const float middle = segment * (halfLength + 1.8F) * 0.5F;
                    const float length = halfLength - 2.0F;
                    wallPiece(xWall, side, middle, 0.42F,
                        length, 0.84F, 0.20F, graphite, true);
                    wallPiece(xWall, side, middle, 0.88F,
                        length, 0.075F, 0.23F, rim);
                    wallPiece(xWall, side, middle, 3.54F,
                        length * 0.38F, 0.18F, 0.12F, rim);
                    wallPiece(xWall, side, middle, 3.55F,
                        length * 0.36F, 0.070F, 0.145F, amber);
                }
                const float service = side * (xWall ? 9.2F : 8.0F);
                wallPiece(xWall, side, service, 2.52F,
                    2.15F, 1.58F, 0.20F, graphite, true);
                wallPiece(xWall, side, service, 2.52F,
                    1.91F, 1.35F, 0.225F, steel);
                wallPiece(xWall, side, service, 3.15F,
                    1.82F, 0.075F, 0.245F, amber);
                for (int slat = 0; slat < 6; ++slat) {
                    wallPiece(xWall, side, service,
                        2.08F + static_cast<float>(slat) * 0.17F,
                        1.58F, 0.055F, 0.25F, rim);
                }
                for (const float offset : {-0.90F, 0.90F}) {
                    for (const float y : {1.82F, 3.20F}) {
                        wallPiece(xWall, side, service + offset, y,
                            0.075F, 0.075F, 0.27F, bolt);
                    }
                }
            }
        }
        // A few large recessed ceiling grilles and paired conduit runs keep
        // the high roof readable without multiplying the ceiling texture.
        for (const float x : {-9.0F, 9.0F}) {
            piece({x, ceiling - 0.23F, 0.0F},
                {0.13F, 0.15F, 27.0F}, rim, true);
            for (const float z : {-10.5F, 10.5F}) {
                piece({x, ceiling - 0.19F, z},
                    {2.4F, 0.13F, 1.8F}, graphite, true);
                for (int slat = -4; slat <= 4; ++slat) {
                    piece({x + static_cast<float>(slat) * 0.23F,
                            ceiling - 0.28F, z},
                        {0.075F, 0.055F, 1.46F}, rim);
                }
            }
        }
    };
    drawBossArenaDetails(room);
    for (const RoomRenderData& connectedRoom : connectedRooms) {
        drawBossArenaDetails(connectedRoom);
    }

    // Sparse service-sector identifiers make the far Arena walls distinct
    // without adding cover or changing the approved sniper sightlines.
    const auto drawArenaSectorMarkers = [&](const RoomRenderData& renderedRoom) {
        const RoomGpuMaterials& materials = resolveMaterials(renderedRoom.materialKey);
        if (!materials.combatGalleryWallV1 || materials.combatBossV1) return;
        const wkc::core::Vec3 span = renderedRoom.worldBounds.maximum
            - renderedRoom.worldBounds.minimum;
        const bool quarterTurn = renderedRoom.rotation
                == wkc::core::QuarterTurn::Degrees90
            || renderedRoom.rotation == wkc::core::QuarterTurn::Degrees270;
        const float halfX = (quarterTurn ? span.z : span.x) * 0.5F;
        const float halfZ = (quarterTurn ? span.x : span.z) * 0.5F;
        if (halfX < 8.0F || halfZ < 8.0F) return;
        const auto marker = [&](const wkc::core::Vec3 local,
                                wkc::core::Vec3 size,
                                const wkc::core::Vec3 color) {
            if (quarterTurn) std::swap(size.x, size.z);
            queueSurface(materials,
                renderedRoom.origin + wkc::core::rotateQuarterTurns(
                    local, renderedRoom.rotation),
                size, RoomMaterialChannel::SolidColor, 1.0F, color);
            surfaceCommands.back().castsShadow = false;
            surfaceCommands.back().style.emissiveStrength = 0.16F;
        };
        for (const float end : {-1.0F, 1.0F}) {
            for (const float side : {-1.0F, 1.0F}) {
                const float x = side * 5.6F;
                const float z = end * (halfZ - 0.10F);
                marker({x, 3.95F, z}, {0.94F, 0.075F, 0.030F},
                    {0.70F, 0.36F, 0.10F});
                for (int bar = 0; bar < (side > 0.0F ? 2 : 1); ++bar) {
                    marker({x - 0.35F + static_cast<float>(bar) * 0.16F,
                            3.73F, z},
                        {0.065F, 0.27F, 0.032F},
                        {0.70F, 0.36F, 0.10F});
                }
            }
        }
    };
    drawArenaSectorMarkers(room);
    for (const RoomRenderData& connectedRoom : connectedRooms) {
        drawArenaSectorMarkers(connectedRoom);
    }

    const auto bindSurfaceStyle = [this, &shader](const SurfaceStyle& style) {
        if (style.roomMaterials == nullptr) {
            return false;
        }
        const RoomGpuMaterials& roomMaterials = *style.roomMaterials;
        switch (style.material) {
        case RoomMaterialChannel::Wall:
            if (roomMaterials.corridorV2 || roomMaterials.officeV1) {
                const auto& material = roomMaterials.specialSurfaceMaterials[
                    static_cast<std::size_t>(IndustrialSurface::Wall)];
                if (!material) return false;
                material->albedo.bind(gl_, textureBindings_, 0U);
                material->normal.bind(gl_, textureBindings_, 1U);
                material->properties.bind(gl_, textureBindings_, 2U);
            } else if (roomMaterials.combatBossV1) {
                const auto& material = roomMaterials.bossSurfaceMaterials[
                    static_cast<std::size_t>(IndustrialSurface::Wall)];
                if (!material) return false;
                material->albedo.bind(gl_, textureBindings_, 0U);
                material->normal.bind(gl_, textureBindings_, 1U);
                material->properties.bind(gl_, textureBindings_, 2U);
            } else if (style.detailedMaterial) {
                if (!roomMaterials.industrialWallCondition) {
                    return false;
                }
                const auto& material = industrialWallMaterials_[
                    static_cast<std::size_t>(
                        *roomMaterials.industrialWallCondition)];
                if (!material) {
                    return false;
                }
                material->albedo.bind(gl_, textureBindings_, 0U);
                material->normal.bind(gl_, textureBindings_, 1U);
                material->properties.bind(gl_, textureBindings_, 2U);
            } else {
                if (roomMaterials.storageHallV1) {
                    storageWallTextures_[style.surfaceVariant]
                        .bind(gl_, textureBindings_);
                } else if (roomMaterials.cafeteriaV1) {
                    cafeteriaWallTextures_[style.surfaceVariant]
                        .bind(gl_, textureBindings_);
                } else {
                    roomMaterials.textures[static_cast<std::size_t>(
                        IndustrialSurface::Wall)].bind(gl_, textureBindings_);
                }
            }
            break;
        case RoomMaterialChannel::Floor:
            if (roomMaterials.corridorV2 || roomMaterials.officeV1) {
                const auto& material = roomMaterials.specialSurfaceMaterials[
                    static_cast<std::size_t>(IndustrialSurface::Floor)];
                if (!material) return false;
                material->albedo.bind(gl_, textureBindings_, 0U);
                material->normal.bind(gl_, textureBindings_, 1U);
                material->properties.bind(gl_, textureBindings_, 2U);
            } else if (roomMaterials.industrialFloorV2) {
                const auto& material = roomMaterials.specialSurfaceMaterials[
                    static_cast<std::size_t>(RoomSurface::Floor)];
                if (!material) return false;
                material->albedo.bind(gl_, textureBindings_, 0U);
                material->normal.bind(gl_, textureBindings_, 1U);
                material->properties.bind(gl_, textureBindings_, 2U);
            } else if (roomMaterials.combatBossV1) {
                const auto& material = roomMaterials.bossSurfaceMaterials[
                    static_cast<std::size_t>(IndustrialSurface::Floor)];
                if (!material) return false;
                material->albedo.bind(gl_, textureBindings_, 0U);
                material->normal.bind(gl_, textureBindings_, 1U);
                material->properties.bind(gl_, textureBindings_, 2U);
            } else if (roomMaterials.storageHallV1) {
                storageFloorTextures_[style.surfaceVariant]
                    .bind(gl_, textureBindings_);
            } else if (roomMaterials.cafeteriaV1) {
                cafeteriaFloorTextures_[style.surfaceVariant]
                    .bind(gl_, textureBindings_);
            } else {
                roomMaterials.textures[static_cast<std::size_t>(
                    IndustrialSurface::Floor)].bind(gl_, textureBindings_);
            }
            break;
        case RoomMaterialChannel::HubFloorDetail:
            centralHubFloorDetailTexture_.bind(gl_, textureBindings_);
            break;
        case RoomMaterialChannel::Ceiling:
            if (roomMaterials.corridorV2 || roomMaterials.officeV1) {
                const auto& material = roomMaterials.specialSurfaceMaterials[
                    static_cast<std::size_t>(IndustrialSurface::Ceiling)];
                if (!material) return false;
                material->albedo.bind(gl_, textureBindings_, 0U);
                material->normal.bind(gl_, textureBindings_, 1U);
                material->properties.bind(gl_, textureBindings_, 2U);
            } else if (roomMaterials.combatBossV1) {
                const auto& material = roomMaterials.bossSurfaceMaterials[
                    static_cast<std::size_t>(IndustrialSurface::Ceiling)];
                if (!material) return false;
                material->albedo.bind(gl_, textureBindings_, 0U);
                material->normal.bind(gl_, textureBindings_, 1U);
                material->properties.bind(gl_, textureBindings_, 2U);
            } else if (style.detailedMaterial) {
                if (!roomMaterials.industrialCeilingCondition) {
                    return false;
                }
                const auto& material = industrialCeilingMaterials_[
                    static_cast<std::size_t>(
                        *roomMaterials.industrialCeilingCondition)];
                if (!material) {
                    return false;
                }
                material->albedo.bind(gl_, textureBindings_, 0U);
                material->normal.bind(gl_, textureBindings_, 1U);
                material->properties.bind(gl_, textureBindings_, 2U);
            } else {
                if (roomMaterials.storageHallV1) {
                    storageCeilingTextures_[style.surfaceVariant]
                        .bind(gl_, textureBindings_);
                } else if (roomMaterials.cafeteriaV1) {
                    cafeteriaCeilingTextures_[style.surfaceVariant]
                        .bind(gl_, textureBindings_);
                } else {
                    roomMaterials.textures[static_cast<std::size_t>(
                        IndustrialSurface::Ceiling)].bind(gl_, textureBindings_);
                }
            }
            break;
        case RoomMaterialChannel::CombatCover:
            roomMaterials.combatCoverTexture.bind(gl_, textureBindings_);
            break;
        case RoomMaterialChannel::CombatColumn:
            roomMaterials.combatColumnTexture.bind(gl_, textureBindings_);
            break;
        case RoomMaterialChannel::CombatRail:
            roomMaterials.combatRailTexture.bind(gl_, textureBindings_);
            break;
        case RoomMaterialChannel::StorageRack:
            if (style.rackPart == world::StorageRackPart::Upright) {
                storageRackUprightTextures_[style.surfaceVariant]
                    .bind(gl_, textureBindings_);
            } else {
                storageRackShelfTextures_[style.surfaceVariant]
                    .bind(gl_, textureBindings_);
            }
            break;
        case RoomMaterialChannel::CafeteriaFurniture:
            cafeteriaFurnitureTextures_[static_cast<std::size_t>(
                style.furniturePart) - 1U][style.surfaceVariant]
                .bind(gl_, textureBindings_);
            break;
        case RoomMaterialChannel::LaboratoryFurniture:
            laboratoryFurnitureTexture_.bind(gl_, textureBindings_);
            break;
        case RoomMaterialChannel::SecurityFurniture:
            securityFurnitureTexture_.bind(gl_, textureBindings_);
            break;
        case RoomMaterialChannel::TechnicalFurniture:
            technicalFurnitureTextures_[style.surfaceVariant]
                .bind(gl_, textureBindings_);
            break;
        case RoomMaterialChannel::StructuralColumn:
            if (style.hubColumnPlate) {
                centralHubColumnTexture_.bind(gl_, textureBindings_);
            } else {
                structuralColumnTextures_[style.surfaceVariant]
                    .bind(gl_, textureBindings_);
            }
            break;
        case RoomMaterialChannel::SolidColor:
            solidColorTexture_.bind(gl_, textureBindings_);
            break;
        case RoomMaterialChannel::Generic:
            fallbackTexture_.bind(gl_, textureBindings_);
            break;
        }
        if (roomMaterials.technicalV1) {
            if (style.material == RoomMaterialChannel::Wall) {
                technicalWallDetailTexture_.bind(gl_, textureBindings_, 12U);
            } else if (style.material == RoomMaterialChannel::Floor) {
                technicalFloorDetailTexture_.bind(gl_, textureBindings_, 12U);
            }
        }
        shader.setVec3(gl_, "uTint", style.tint);
        shader.setInt(gl_, "uDetailedMaterial",
            style.detailedMaterial || (roomMaterials.combatBossV1
                && (style.material == RoomMaterialChannel::Wall
                    || style.material == RoomMaterialChannel::Floor
                    || style.material == RoomMaterialChannel::Ceiling)) ? 1 : 0);
        shader.setVec2(gl_, "uUvScale", style.uvScale);
        shader.setVec2(gl_, "uUvOffset", style.uvOffset);
        shader.setFloat(gl_, "uSurfaceTileMetres", style.surfaceTileMetres);
        shader.setInt(gl_, "uMirrorSurfaceTile", style.mirrorSurfaceTile ? 1 : 0);
        shader.setInt(gl_, "uRackPart", static_cast<int>(style.rackPart));
        shader.setInt(gl_, "uFurniturePart", static_cast<int>(style.furniturePart));
        shader.setInt(gl_, "uStructuralSteelMaterial",
            style.material == RoomMaterialChannel::StructuralColumn
                && !style.hubColumnPlate ? 1 : 0);
        shader.setInt(gl_, "uHubWallMaterial",
            ((roomMaterials.centralHubWallCondition
                    || roomMaterials.combatGalleryWallV1)
                    && style.material == RoomMaterialChannel::Wall)
                || (roomMaterials.combatGalleryWallV1
                    && (style.material == RoomMaterialChannel::CombatCover
                        || style.material == RoomMaterialChannel::CombatColumn
                        || style.material
                            == RoomMaterialChannel::StructuralColumn)) ? 1 : 0);
        shader.setInt(gl_, "uCombatColumnMaterial",
            roomMaterials.combatGalleryWallV1
                && style.material == RoomMaterialChannel::CombatColumn ? 1 : 0);
        shader.setInt(gl_, "uCombatRailMaterial",
            roomMaterials.combatGalleryWallV1
                && style.material == RoomMaterialChannel::CombatRail ? 1 : 0);
        shader.setInt(gl_, "uHubFloorMaterial",
            (roomMaterials.centralHubWallCondition
                || roomMaterials.combatGalleryWallV1)
                && (style.material == RoomMaterialChannel::Floor
                    || style.material == RoomMaterialChannel::HubFloorDetail)
                ? 1 : 0);
        shader.setInt(gl_, "uBossFloorMaterial",
            roomMaterials.combatBossV1
                && style.material == RoomMaterialChannel::Floor ? 1 : 0);
        shader.setInt(gl_, "uTechnicalSteelMaterial",
            roomMaterials.technicalV1
                ? (style.material == RoomMaterialChannel::Floor ? 2
                    : style.material == RoomMaterialChannel::Wall ? 1 : 0)
                : 0);
        shader.setInt(gl_, "uSecurityFloorVariation",
            roomMaterials.securityV1
                && style.material == RoomMaterialChannel::Floor ? 1 : 0);
        shader.setFloat(gl_, "uSecurityFloorSeed", roomMaterials.securityFloorSeed);
        shader.setInt(gl_, "uCombatWallVariation",
            roomMaterials.combatGalleryWallV1
                && !roomMaterials.combatBossV1
                && style.material == RoomMaterialChannel::Wall ? 1 : 0);
        shader.setFloat(gl_, "uCombatWallSeed", roomMaterials.combatWallSeed);
        shader.setFloat(gl_, "uEmissiveStrength", style.emissiveStrength);
        shader.setFloat(gl_, "uUnlitMix", style.unlitMix);
        return true;
    };

    if (geometryShadowsActive) {
        for (const SurfaceCommand& command : surfaceCommands) {
            if (command.castsShadow) {
                shadowCasters.push_back({command.mesh ? command.mesh : &box_,
                    command.model});
            }
        }
    }
    if (!surfaceInstancingEnabled_) {
        shader.setInt(gl_, "uInstanced", 0);
        for (const SurfaceCommand& command : surfaceCommands) {
            if (!bindSurfaceStyle(command.style)) {
                return false;
            }
            shader.setMat4(gl_, "uModel", command.model);
            drawMesh(command.mesh ? *command.mesh : box_);
        }
    } else {
        struct SurfaceBatch {
            SurfaceStyle style{};
            std::vector<wkc::core::Mat4> models;
        };
        std::vector<SurfaceBatch> surfaceBatches;
        surfaceBatches.reserve(surfaceCommands.size());
        for (const SurfaceCommand& command : surfaceCommands) {
            if (command.mesh) {
                if (!bindSurfaceStyle(command.style)) return false;
                shader.setInt(gl_, "uInstanced", 0);
                shader.setMat4(gl_, "uModel", command.model);
                drawMesh(*command.mesh);
                continue;
            }
            const auto batch = std::find_if(surfaceBatches.begin(),
                surfaceBatches.end(), [&command](const SurfaceBatch& candidate) {
                    return candidate.style == command.style;
                });
            if (batch == surfaceBatches.end()) {
                surfaceBatches.push_back({command.style, {command.model}});
            } else {
                batch->models.push_back(command.model);
            }
        }
        for (const SurfaceBatch& batch : surfaceBatches) {
            if (!bindSurfaceStyle(batch.style)) {
                return false;
            }
            if (batch.models.size() > 1U) {
                shader.setInt(gl_, "uInstanced", 1);
                if (!box_.uploadInstanceTransforms(
                        gl_, batch.models, meshBindings_)) {
                    return false;
                }
                drawMeshInstanced(box_);
            } else {
                shader.setInt(gl_, "uInstanced", 0);
                shader.setMat4(gl_, "uModel", batch.models.front());
                drawMesh(box_);
            }
        }
        shader.setInt(gl_, "uInstanced", 0);
    }
    shader.setFloat(gl_, "uUnlitMix", 0.0F);

    shader.setInt(gl_, "uRackPart", 0);
    shader.setInt(gl_, "uFurniturePart", 0);
    shader.setInt(gl_, "uStructuralSteelMaterial", 0);
    shader.setInt(gl_, "uHubWallMaterial", 0);
    shader.setInt(gl_, "uCombatColumnMaterial", 0);
    shader.setInt(gl_, "uCombatRailMaterial", 0);
    shader.setInt(gl_, "uHubFloorMaterial", 0);
    shader.setInt(gl_, "uBossFloorMaterial", 0);
    shader.setInt(gl_, "uTechnicalSteelMaterial", 0);
    shader.setInt(gl_, "uSecurityFloorVariation", 0);
    shader.setInt(gl_, "uCombatWallVariation", 0);
    shader.setFloat(gl_, "uSurfaceTileMetres", 0.0F);
    shader.setInt(gl_, "uMirrorSurfaceTile", 0);
    fallbackTexture_.bind(gl_, textureBindings_);
    for (const DoorRenderData& door : doors) {
        const bool facesX = std::abs(door.outwardNormal.x) > 0.5F;
        const wkc::core::Vec3 insetPosition
            = door.position - (door.outwardNormal * 0.12F);
        const auto drawDoorPiece = [&](const wkc::core::Vec3 position,
                                       const wkc::core::Vec3 scale,
                                       const wkc::core::Vec3 tint,
                                       const float emissive,
                                       const bool steel = true,
                                       const bool castsShadow = false) {
            const wkc::core::Mat4 model
                = wkc::core::translationMatrix(position)
                    * wkc::core::scaleMatrix(scale);
            shader.setMat4(gl_, "uModel", model);
            if (geometryShadowsActive && castsShadow)
                shadowCasters.push_back({&box_, model});
            if (steel) structuralColumnTextures_[1U].bind(gl_, textureBindings_);
            else solidColorTexture_.bind(gl_, textureBindings_);
            shader.setVec3(gl_, "uTint", tint);
            shader.setInt(gl_, "uDetailedMaterial", 0);
            shader.setInt(gl_, "uStructuralSteelMaterial", steel ? 1 : 0);
            shader.setVec2(gl_, "uUvScale", {2.0F, 2.0F});
            shader.setVec2(gl_, "uUvOffset", {});
            shader.setFloat(gl_, "uEmissiveStrength", emissive);
            drawMesh(box_);
        };
        constexpr float frameThickness = 0.14F;
        const wkc::core::Vec3 frameColor{0.25F, 0.26F, 0.26F};
        const wkc::core::Vec3 darkSteel{0.17F, 0.18F, 0.19F};
        const wkc::core::Vec3 edgeSteel{0.27F, 0.28F, 0.28F};
        const wkc::core::Vec3 boltSteel{0.54F, 0.55F, 0.52F};
        const wkc::core::Vec3 transverse = facesX
            ? wkc::core::Vec3{0.0F, 0.0F, 1.0F}
            : wkc::core::Vec3{1.0F, 0.0F, 0.0F};
        const wkc::core::Vec3 postScale = facesX
            ? wkc::core::Vec3{0.12F, door.height, frameThickness}
            : wkc::core::Vec3{frameThickness, door.height, 0.12F};
        const float postOffset = (door.width + frameThickness) * 0.5F;
        const wkc::core::Vec3 postCenter
            = insetPosition + wkc::core::Vec3{0.0F, door.height * 0.5F, 0.0F};
        drawDoorPiece(postCenter + (transverse * postOffset),
            postScale, frameColor, 0.0F, true, true);
        drawDoorPiece(postCenter - (transverse * postOffset),
            postScale, frameColor, 0.0F, true, true);
        const wkc::core::Vec3 lintelScale = facesX
            ? wkc::core::Vec3{0.12F, frameThickness,
                door.width + (frameThickness * 2.0F)}
            : wkc::core::Vec3{door.width + (frameThickness * 2.0F),
                frameThickness, 0.12F};
        drawDoorPiece(insetPosition
                + wkc::core::Vec3{0.0F, door.height + (frameThickness * 0.5F), 0.0F},
            lintelScale, frameColor, 0.0F, true, true);

        // Both doors retain the existing two-leaf travel and collision. The
        // tracks, actuator and fasteners add depth without occupying the
        // opening; only the small status lens carries the lock colour.
        const wkc::core::Vec3 trackScale{
            0.065F, door.height - 0.10F, 0.065F};
        for (const float sign : {-1.0F, 1.0F}) {
            const wkc::core::Vec3 trackCenter = postCenter
                + transverse * (sign * postOffset)
                - door.outwardNormal * 0.082F;
            drawDoorPiece(trackCenter, trackScale, darkSteel, 0.0F);
            for (const float fastenerY : {0.20F, 0.82F}) {
                const wkc::core::Vec3 boltCenter = postCenter
                    + transverse * (sign * postOffset)
                    - door.outwardNormal * 0.112F
                    + wkc::core::Vec3{0.0F,
                        (fastenerY - 0.5F) * door.height, 0.0F};
                drawDoorPiece(boltCenter, {0.047F, 0.047F, 0.047F},
                    boltSteel, 0.0F);
            }
        }
        const wkc::core::Vec3 actuatorScale = facesX
            ? wkc::core::Vec3{0.17F, 0.20F, door.width * 0.72F}
            : wkc::core::Vec3{door.width * 0.72F, 0.20F, 0.17F};
        const wkc::core::Vec3 actuatorCenter = insetPosition
            - door.outwardNormal * 0.065F
            + wkc::core::Vec3{0.0F, door.height + 0.17F, 0.0F};
        drawDoorPiece(actuatorCenter, actuatorScale, darkSteel, 0.0F, true, true);
        const wkc::core::Vec3 indicatorColor = door.locked
            ? wkc::core::Vec3{0.90F, 0.10F, 0.05F}
            : door.openFraction > 0.01F && door.openFraction < 0.99F
                ? wkc::core::Vec3{0.95F, 0.53F, 0.08F}
                : wkc::core::Vec3{0.08F, 0.74F, 0.77F};
        const wkc::core::Vec3 indicatorScale = facesX
            ? wkc::core::Vec3{0.022F, 0.035F, std::min(0.30F, door.width * 0.24F)}
            : wkc::core::Vec3{std::min(0.30F, door.width * 0.24F), 0.035F, 0.022F};
        drawDoorPiece(actuatorCenter - door.outwardNormal * 0.100F,
            indicatorScale, indicatorColor, 1.8F, false);

        const float openFraction = std::isfinite(door.openFraction)
            ? std::clamp(door.openFraction, 0.0F, 1.0F) : 0.0F;
        const float panelWidth = std::max(0.05F, (door.width * 0.5F) - 0.02F);
        const float panelOffset = (panelWidth * 0.5F)
            + ((panelWidth + 0.04F) * openFraction);
        const wkc::core::Vec3 panelScale = facesX
            ? wkc::core::Vec3{0.10F, door.height - 0.10F, panelWidth}
            : wkc::core::Vec3{panelWidth, door.height - 0.10F, 0.10F};
        const wkc::core::Vec3 panelCenter
            = insetPosition + wkc::core::Vec3{0.0F, door.height * 0.5F, 0.0F};
        for (const float side : {-1.0F, 1.0F}) {
            const wkc::core::Vec3 leafCenter = panelCenter
                + transverse * (side * panelOffset);
            drawDoorPiece(leafCenter, panelScale, frameColor,
                0.0F, true, true);
            const wkc::core::Vec3 face = leafCenter - door.outwardNormal * 0.058F;
            const wkc::core::Vec3 insetScale = facesX
                ? wkc::core::Vec3{0.024F, door.height * 0.72F,
                    panelWidth * 0.77F}
                : wkc::core::Vec3{panelWidth * 0.77F,
                    door.height * 0.72F, 0.024F};
            drawDoorPiece(face, insetScale, darkSteel, 0.0F);
            const wkc::core::Vec3 panelLip = leafCenter
                - door.outwardNormal * 0.075F;
            const wkc::core::Vec3 horizontalLip = facesX
                ? wkc::core::Vec3{0.027F, 0.028F, panelWidth * 0.78F}
                : wkc::core::Vec3{panelWidth * 0.78F, 0.028F, 0.027F};
            for (const float level : {-0.36F, 0.36F}) {
                drawDoorPiece(panelLip
                        + wkc::core::Vec3{0.0F, door.height * level, 0.0F},
                    horizontalLip, edgeSteel, 0.0F);
            }
            const wkc::core::Vec3 seamScale = facesX
                ? wkc::core::Vec3{0.028F, door.height * 0.86F, 0.026F}
                : wkc::core::Vec3{0.026F, door.height * 0.86F, 0.028F};
            drawDoorPiece(leafCenter - door.outwardNormal * 0.069F
                    - transverse * (side * panelWidth * 0.46F),
                seamScale, edgeSteel, 0.0F);
            for (const float level : {0.16F, 0.83F}) {
                for (const float edge : {-0.33F, 0.33F}) {
                    drawDoorPiece(panelLip - door.outwardNormal * 0.014F
                            + transverse * (edge * panelWidth)
                            + wkc::core::Vec3{
                                0.0F, (level - 0.5F) * door.height, 0.0F},
                        {0.030F, 0.030F, 0.030F}, boltSteel, 0.0F);
                }
            }
            if (door.inspectionWindow) {
                const wkc::core::Vec3 glassScale = facesX
                    ? wkc::core::Vec3{0.034F, door.height * 0.32F,
                        std::min(0.12F, panelWidth * 0.25F)}
                    : wkc::core::Vec3{std::min(0.12F, panelWidth * 0.25F),
                        door.height * 0.32F, 0.034F};
                const wkc::core::Vec3 glassCenter = face
                    - door.outwardNormal * 0.025F
                    + wkc::core::Vec3{0.0F, door.height * 0.13F, 0.0F};
                drawDoorPiece(glassCenter, glassScale,
                    {0.025F, 0.075F, 0.095F}, 0.0F, false);
                const wkc::core::Vec3 rimScale = facesX
                    ? wkc::core::Vec3{0.040F, door.height * 0.34F,
                        std::min(0.16F, panelWidth * 0.32F)}
                    : wkc::core::Vec3{std::min(0.16F, panelWidth * 0.32F),
                        door.height * 0.34F, 0.040F};
                drawDoorPiece(glassCenter + door.outwardNormal * 0.012F,
                    rimScale, edgeSteel, 0.0F);
                const wkc::core::Vec3 lowerPlateScale = facesX
                    ? wkc::core::Vec3{0.037F, door.height * 0.23F,
                        panelWidth * 0.67F}
                    : wkc::core::Vec3{panelWidth * 0.67F,
                        door.height * 0.23F, 0.037F};
                drawDoorPiece(face - door.outwardNormal * 0.027F
                        + wkc::core::Vec3{
                            0.0F, -door.height * 0.27F, 0.0F},
                    lowerPlateScale, {0.33F, 0.34F, 0.34F}, 0.0F);
            } else {
                const wkc::core::Vec3 armorBandScale = facesX
                    ? wkc::core::Vec3{0.038F, door.height * 0.16F,
                        panelWidth * 0.79F}
                    : wkc::core::Vec3{panelWidth * 0.79F,
                        door.height * 0.16F, 0.038F};
                drawDoorPiece(face - door.outwardNormal * 0.035F
                        + wkc::core::Vec3{0.0F, door.height * 0.07F, 0.0F},
                    armorBandScale, {0.32F, 0.33F, 0.33F}, 0.0F);
            }
        }
    }
    shader.setInt(gl_, "uStructuralSteelMaterial", 0);

    fallbackTexture_.bind(gl_, textureBindings_);
    shader.setInt(gl_, "uDetailedMaterial", 0);
    shader.setVec2(gl_, "uUvScale", {1.0F, 1.0F});
    shader.setVec2(gl_, "uUvOffset", {});
    const auto bulletMarkPosition = [&](const ImpactRenderData& mark) {
        const auto onFallbackWall = [&](const RoomRenderData& renderedRoom) {
            if (!renderedRoom.walkableOutline.empty()
                || resolveMaterials(renderedRoom.materialKey).technicalV1) {
                return false;
            }
            const auto& bounds = renderedRoom.worldBounds;
            if (mark.position.y < bounds.minimum.y - 0.02F
                || mark.position.y > bounds.maximum.y + 0.02F) {
                return false;
            }
            constexpr float wallTolerance = 0.02F;
            return (mark.normal.x > 0.9F
                    && std::abs(mark.position.x - bounds.minimum.x) < wallTolerance)
                || (mark.normal.x < -0.9F
                    && std::abs(mark.position.x - bounds.maximum.x) < wallTolerance)
                || (mark.normal.z > 0.9F
                    && std::abs(mark.position.z - bounds.minimum.z) < wallTolerance)
                || (mark.normal.z < -0.9F
                    && std::abs(mark.position.z - bounds.maximum.z) < wallTolerance);
        };
        if (onFallbackWall(room)
            || std::any_of(connectedRooms.begin(), connectedRooms.end(),
                onFallbackWall)) {
            // Collision ends at the room boundary; box shells render 10 cm
            // inside it. Keep the mark on that visible face.
            return mark.position + wkc::core::normalized(mark.normal)
                * (fallbackRoomShellHalfThickness + 0.005F);
        }
        return mark.position;
    };
    for (const ImpactRenderData& impact : impacts) {
        if (impact.kind == ImpactVisualKind::BulletMark) {
            const wkc::core::Vec3 normal = wkc::core::normalized(impact.normal);
            const wkc::core::Vec3 markPosition = bulletMarkPosition(impact);
            const wkc::core::Vec3 reference = std::abs(normal.y) > 0.9F
                ? wkc::core::Vec3{0.0F, 0.0F, 1.0F}
                : wkc::core::Vec3{0.0F, 1.0F, 0.0F};
            const wkc::core::Vec3 right = wkc::core::normalized(
                wkc::core::cross(reference, normal));
            const wkc::core::Mat4 orientation = basisMatrix(
                right, normal, wkc::core::cross(right, normal));
            const float variation = 0.85F + 0.30F * static_cast<float>(
                wkc::core::mix64(impact.visualSeed) & 0xffU) / 255.0F;
            shader.setFloat(gl_, "uEmissiveStrength", 0.0F);
            shader.setVec3(gl_, "uTint", {0.025F, 0.027F, 0.029F});
            shader.setMat4(gl_, "uModel",
                wkc::core::translationMatrix(
                    markPosition + normal * 0.006F)
                    * orientation
                    * wkc::core::scaleMatrix(
                        {0.105F * variation, 0.006F, 0.105F * variation}));
            drawMesh(grenadeBody_);
            shader.setVec3(gl_, "uTint", {0.78F, 0.75F, 0.68F});
            shader.setMat4(gl_, "uModel",
                wkc::core::translationMatrix(
                    markPosition + normal * 0.014F)
                    * orientation
                    * wkc::core::scaleMatrix(
                        {0.077F * variation, 0.077F * variation,
                            0.077F * variation}));
            drawMesh(grenadeRing_);
            continue;
        }
        const float strength = std::clamp(
            std::isfinite(impact.strength) ? impact.strength : 0.0F,
            0.0F, 1.0F);
        if (strength <= 0.0F) {
            continue;
        }
        const wkc::core::Vec3 color
            = impactVisualColor(impact.material, strength);
        const float size = impactVisualSize(strength);
        const wkc::core::Vec3 normal = wkc::core::normalized(impact.normal);
        const wkc::core::Vec3 center
            = impact.position + (normal * 0.018F);
        shader.setMat4(gl_, "uModel", wkc::core::translationMatrix(center)
                * wkc::core::scaleMatrix({size, size, size}));
        shader.setVec3(gl_, "uTint", color);
        shader.setFloat(gl_, "uEmissiveStrength",
            impact.material == ImpactVisualMaterial::Metal ? 0.75F : 0.12F);
        drawMesh(box_);

        const std::size_t particleCount
            = impact.material == ImpactVisualMaterial::Metal ? 4U
            : impact.material == ImpactVisualMaterial::Composite ? 3U : 2U;
        constexpr float tau = 6.28318530717958647692F;
        for (std::size_t particle = 0U; particle < particleCount; ++particle) {
            const std::uint64_t phaseBits = wkc::core::mix64(
                impact.visualSeed + static_cast<std::uint64_t>(particle));
            const float phase = static_cast<float>(phaseBits & 0xffffU)
                / 65535.0F * tau;
            const float travel = (1.0F - strength)
                * (0.10F + (static_cast<float>(particle) * 0.035F));
            const wkc::core::Vec3 particleOffset{
                std::cos(phase) * travel,
                (0.04F + (0.02F * static_cast<float>(particle))) * strength,
                std::sin(phase) * travel,
            };
            const float particleSize = size * (0.34F + (strength * 0.18F));
            shader.setMat4(gl_, "uModel",
                wkc::core::translationMatrix(center + particleOffset)
                    * wkc::core::scaleMatrix(
                        {particleSize, particleSize, particleSize}));
            shader.setVec3(gl_, "uTint", color);
            drawMesh(box_);
        }
    }

    // The world projectile and first-person viewmodel use the same mesh,
    // proportions, tint and material. Only their transforms differ.
    const auto drawGrenadeModel = [&](const wkc::core::Mat4& root,
                                      const wkc::core::Vec3 center,
                                      const bool pinAttached) {
        const auto bindMaterial = [&](const WeaponMaterial material) {
            const auto& gpu = weaponMaterials_[static_cast<std::size_t>(material)];
            gpu.albedo.bind(gl_, textureBindings_, 0U);
            gpu.normal.bind(gl_, textureBindings_, 1U);
            gpu.properties.bind(gl_, textureBindings_, 2U);
            shader.setInt(gl_, "uDetailedMaterial", 1);
        };
        const auto roundedPiece = [&](const wkc::core::Vec3 offset,
                                      const wkc::core::Vec3 scale,
                                      const wkc::core::Vec3 tint) {
            shader.setMat4(gl_, "uModel", root
                * wkc::core::translationMatrix(offset)
                * wkc::core::scaleMatrix(scale));
            shader.setVec3(gl_, "uTint", tint);
            shader.setFloat(gl_, "uEmissiveStrength", 0.0F);
            drawMesh(grenadeBody_);
        };
        shader.setFloat(gl_, "uOpacity", 1.0F);
        shader.setInt(gl_, "uUseEmissionMap", 0);
        shader.setVec2(gl_, "uUvScale", {1.0F, 1.0F});
        shader.setVec2(gl_, "uUvOffset", {});
        if (!grenadeModelParts_.empty()) {
            shader.setMat4(gl_, "uModel", root
                * wkc::core::translationMatrix(center)
                * wkc::core::scaleMatrix({0.205F, 0.225F, 0.205F}));
            constexpr std::array materialTints{
                wkc::core::Vec3{1.0F, 1.0F, 1.0F},
                wkc::core::Vec3{0.20F, 0.22F, 0.24F},
                wkc::core::Vec3{0.49F, 0.51F, 0.52F},
                wkc::core::Vec3{0.13F, 0.69F, 0.80F},
                wkc::core::Vec3{0.32F, 0.35F, 0.37F},
                wkc::core::Vec3{0.075F, 0.085F, 0.09F},
            };
            constexpr std::array materialBindings{
                WeaponMaterial::AccentPaint, WeaponMaterial::Polymer,
                WeaponMaterial::BareSteel, WeaponMaterial::Sight,
                WeaponMaterial::CoatedSteel, WeaponMaterial::Recess,
            };
            for (const GrenadeGpuPart& part : grenadeModelParts_) {
                if (part.partId == 1U && !pinAttached) continue;
                bindMaterial(materialBindings[part.materialId]);
                shader.setVec3(gl_, "uTint", materialTints[part.materialId]);
                shader.setFloat(gl_, "uEmissiveStrength",
                    part.materialId == 3U ? 0.55F : 0.0F);
                drawMesh(part.mesh);
            }
            shader.setFloat(gl_, "uEmissiveStrength", 0.0F);
            return;
        }
        constexpr wkc::core::Vec3 bodyScale{0.205F, 0.225F, 0.205F};
        bindMaterial(WeaponMaterial::Polymer);
        roundedPiece(center, bodyScale, {0.20F, 0.22F, 0.24F});
        bindMaterial(WeaponMaterial::AccentPaint);
        shader.setMat4(gl_, "uModel", root
            * wkc::core::translationMatrix(center)
            * wkc::core::rotationYMatrix(std::numbers::pi_v<float> / 8.0F)
            * wkc::core::scaleMatrix(bodyScale));
        shader.setVec3(gl_, "uTint", {1.0F, 1.0F, 1.0F});
        drawMesh(grenadeArmor_);
        bindMaterial(WeaponMaterial::Sight);
        shader.setMat4(gl_, "uModel", root
            * wkc::core::translationMatrix(center)
            * wkc::core::rotationYMatrix(std::numbers::pi_v<float> / 8.0F)
            * wkc::core::scaleMatrix(bodyScale));
        shader.setVec3(gl_, "uTint", {0.13F, 0.69F, 0.80F});
        shader.setFloat(gl_, "uEmissiveStrength", 0.18F);
        drawMesh(grenadeLights_);
        shader.setFloat(gl_, "uEmissiveStrength", 0.0F);
        bindMaterial(WeaponMaterial::CoatedSteel);
        roundedPiece(center + wkc::core::Vec3{0.0F, 0.119F, 0.0F},
            {0.104F, 0.052F, 0.104F}, {0.32F, 0.35F, 0.37F});
        shader.setMat4(gl_, "uModel", root
            * wkc::core::translationMatrix(center
                + wkc::core::Vec3{-0.067F, 0.103F, 0.0F})
            * wkc::core::rotationZMatrix(-15.0F * std::numbers::pi_v<float> / 180.0F)
            * wkc::core::scaleMatrix({0.014F, 0.116F, 0.040F}));
        shader.setVec3(gl_, "uTint", {0.38F, 0.40F, 0.42F});
        drawMesh(box_);
        if (pinAttached) {
            shader.setMat4(gl_, "uModel", root
                * wkc::core::translationMatrix(center
                    + wkc::core::Vec3{-0.085F, 0.166F, 0.0F})
                * wkc::core::rotationXMatrix(std::numbers::pi_v<float> * 0.5F)
                * wkc::core::scaleMatrix({0.105F, 0.105F, 0.105F}));
            shader.setVec3(gl_, "uTint", {0.70F, 0.73F, 0.75F});
            drawMesh(grenadeRing_);
        }
    };

    for (const EnemyProjectileRenderData& projectile : enemyProjectiles) {
        const wkc::core::Vec3 direction
            = wkc::core::normalized(projectile.direction);
        const float yaw = std::atan2(direction.x, direction.z);
        const float pitch = -std::asin(std::clamp(direction.y, -1.0F, 1.0F));
        shader.setMat4(gl_, "uModel",
            wkc::core::translationMatrix(projectile.position)
                * wkc::core::rotationYMatrix(yaw)
                * wkc::core::rotationXMatrix(pitch)
                * wkc::core::scaleMatrix({0.11F, 0.11F, 0.30F}));
        shader.setVec3(gl_, "uTint", {1.0F, 0.32F, 0.08F});
        shader.setFloat(gl_, "uEmissiveStrength", 2.0F);
        drawMesh(box_);
    }
    // Preserve paint and exposed steel in the world instead of mixing toward
    // the neutral shader tint. This also keeps the projectile readable away
    // from the first-person viewmodel light.
    shader.setFloat(gl_, "uMaterialUnlitMix",
        grenadeModelParts_.empty() ? 0.0F : 1.0F);
    shader.setFloat(gl_, "uUnlitMix", 0.35F);
    for (const GrenadeRenderData& grenade : grenades) {
        drawGrenadeModel(wkc::core::translationMatrix(grenade.position), {}, false);
    }
    shader.setFloat(gl_, "uUnlitMix", 0.0F);
    shader.setFloat(gl_, "uMaterialUnlitMix", 0.0F);
    shader.setInt(gl_, "uDetailedMaterial", 0);
    fallbackTexture_.bind(gl_, textureBindings_);
    shader.setFloat(gl_, "uEmissiveStrength", 0.0F);

    ceilingFixtureMaterial_.albedo.bind(gl_, textureBindings_, 0U);
    ceilingFixtureMaterial_.normal.bind(gl_, textureBindings_, 1U);
    ceilingFixtureMaterial_.properties.bind(gl_, textureBindings_, 2U);
    ceilingFixtureMaterial_.emission.bind(gl_, textureBindings_, 3U);
    shader.setInt(gl_, "uDetailedMaterial", 1);
    shader.setInt(gl_, "uUseEmissionMap", 1);
    constexpr wkc::core::Vec3 fixtureAlertColor{0.84F, 0.29F, 0.23F};
    constexpr std::array fixturePaintColors{
        wkc::core::Vec3{0.105F, 0.119F, 0.125F}, // Industrial
        wkc::core::Vec3{0.18F, 0.20F, 0.19F},     // Storage
        wkc::core::Vec3{0.15F, 0.17F, 0.175F},   // Maintenance
        wkc::core::Vec3{0.49F, 0.54F, 0.55F},    // Laboratory
        wkc::core::Vec3{0.47F, 0.45F, 0.40F},    // Cafeteria
        wkc::core::Vec3{0.13F, 0.16F, 0.17F},    // Security
        wkc::core::Vec3{0.39F, 0.43F, 0.44F},    // Office
        wkc::core::Vec3{0.125F, 0.125F, 0.13F}, // Boss / Arena
        wkc::core::Vec3{0.21F, 0.19F, 0.16F},   // Central Hub
    };
    const std::span<const PointLightRenderData> visibleFixtures
        = lighting.visibleFixturesPopulated
        ? std::span<const PointLightRenderData>{lighting.visibleFixtures}
        : std::span<const PointLightRenderData>{lighting.lights.data(), lightCount};
    for (const PointLightRenderData& light : visibleFixtures) {
        const bool hubFixture
            = light.fixtureVariant == CeilingFixtureVariant::CentralHub;
        const bool combatFixture
            = light.fixtureVariant == CeilingFixtureVariant::BossArena
            && light.sourceRoomBounds.maximum.x
                - light.sourceRoomBounds.minimum.x < 24.0F;
        const bool bossFixture
            = light.fixtureVariant == CeilingFixtureVariant::BossArena
            && light.sourceRoomBounds.maximum.x
                - light.sourceRoomBounds.minimum.x >= 24.0F;
        const wkc::core::Vec3 roomCenter
            = (light.sourceRoomBounds.minimum
                + light.sourceRoomBounds.maximum) * 0.5F;
        const float fixtureYaw = hubFixture
            && (light.position.x - roomCenter.x)
                * (light.position.z - roomCenter.z) > 0.0F
            ? std::numbers::pi_v<float> * 0.5F : 0.0F;
        const wkc::core::Vec3 fixtureColor = lighting.alert
            ? fixtureAlertColor : light.color;
        const float baseLensEmission = lighting.alert
            ? lighting.fixtureEmissive * 1.35F : lighting.fixtureEmissive;
        const float lensEmission
            = baseLensEmission * light.fixtureEmissionScale;
        const wkc::core::Vec3 lensTint = lensEmission > 0.01F
            ? fixtureColor : wkc::core::Vec3{0.42F, 0.44F, 0.43F};
        const auto drawFixturePiece = [this, &light, &shader, fixtureYaw](
            const wkc::core::Vec3 offset,
            const wkc::core::Vec3 scale,
            const wkc::core::Vec2 uvScale,
            const wkc::core::Vec2 uvOffset,
            const wkc::core::Vec3 tint,
            const float emissiveStrength) {
            shader.setMat4(gl_, "uModel",
                wkc::core::translationMatrix(light.position)
                    * wkc::core::rotationYMatrix(fixtureYaw)
                    * wkc::core::translationMatrix(offset)
                    * wkc::core::scaleMatrix(scale));
            shader.setVec3(gl_, "uTint", tint);
            shader.setVec2(gl_, "uUvScale", uvScale);
            shader.setVec2(gl_, "uUvOffset", uvOffset);
            shader.setFloat(gl_, "uEmissiveStrength", emissiveStrength);
            drawMesh(box_);
        };
        const wkc::core::Vec3 housingTint
            = light.fixtureState == PropVisualState::Destroyed
            ? wkc::core::Vec3{0.30F, 0.32F, 0.31F}
            : light.fixtureState == PropVisualState::Damaged
                ? wkc::core::Vec3{0.70F, 0.56F, 0.40F}
                : wkc::core::Vec3{1.0F, 1.0F, 1.0F};
        const float suspensionLength = light.sourceRoomBounds.maximum.y
            - light.position.y - 0.26F;
        if ((hubFixture || combatFixture || bossFixture)
            && suspensionLength > 0.05F) {
            const wkc::core::Vec2 metalUvScale{0.12F, 0.88F};
            const wkc::core::Vec2 metalUvOffset{0.53F, 0.05F};
            for (const float side : {-1.0F, 1.0F}) {
                drawFixturePiece({side * (bossFixture ? 0.82F : 0.44F),
                        0.18F + suspensionLength * 0.5F, 0.0F},
                    {0.028F, suspensionLength, 0.028F},
                    metalUvScale, metalUvOffset,
                    housingTint * 0.72F, 0.0F);
                drawFixturePiece({side * (bossFixture ? 0.82F : 0.44F),
                        0.18F + suspensionLength + 0.025F, 0.0F},
                    {0.15F, 0.05F, 0.15F},
                    metalUvScale, metalUvOffset,
                    housingTint * 0.78F, 0.0F);
            }
        }
        const std::size_t variant = static_cast<std::size_t>(light.fixtureVariant);
        if (variant < ceilingFixtureModels_.size()
            && !ceilingFixtureModels_[variant].empty()) {
            wkc::core::Vec3 paint = fixturePaintColors[variant];
            if (light.fixtureCondition == CeilingFixtureCondition::Maintained) {
                paint = paint * 1.10F;
            } else if (light.fixtureCondition == CeilingFixtureCondition::Worn) {
                paint = paint * 0.58F
                    + wkc::core::Vec3{0.35F, 0.24F, 0.14F} * 0.42F;
            }
            shader.setMat4(gl_, "uModel",
                wkc::core::translationMatrix(light.position)
                    * wkc::core::rotationYMatrix(fixtureYaw)
                    * wkc::core::translationMatrix(
                        {0.0F, 0.095F, 0.0F})
                    * wkc::core::scaleMatrix(bossFixture
                        ? wkc::core::Vec3{1.9F, 1.0F, 1.0F}
                        : wkc::core::Vec3{1.0F, 1.0F, 1.0F}));
            shader.setVec2(gl_, "uUvScale", {1.0F, 1.0F});
            shader.setVec2(gl_, "uUvOffset", {0.0F, 0.0F});
            for (const CeilingFixtureGpuPart& part : ceilingFixtureModels_[variant]) {
                wkc::core::Vec3 tint{1.0F, 1.0F, 1.0F};
                if (part.materialId == 0U) {
                    // Atlas paint is deliberately neutral, allowing each room
                    // to use the Blender proposal's own housing palette.
                    tint = {paint.x / 0.19F * housingTint.x,
                        paint.y / 0.19F * housingTint.y,
                        paint.z / 0.19F * housingTint.z};
                } else if (part.materialId == 1U || part.materialId == 3U) {
                    tint = light.fixtureCondition == CeilingFixtureCondition::Worn
                        ? wkc::core::Vec3{0.70F, 0.53F, 0.38F}
                        : wkc::core::Vec3{0.72F, 0.76F, 0.76F};
                } else if (part.materialId == 4U) {
                    tint = lensTint;
                }
                shader.setVec3(gl_, "uTint", tint);
                shader.setFloat(gl_, "uEmissiveStrength",
                    part.materialId == 4U ? lensEmission : 0.0F);
                drawMesh(part.mesh);
            }
            if (bossFixture) {
                // Use an unmasked metal housing and diffuser: the fixture
                // atlas only lights a short central strip of this long bar.
                shader.setInt(gl_, "uUseEmissionMap", 0);
                shader.setInt(gl_, "uDetailedMaterial", 0);
                solidColorTexture_.bind(gl_, textureBindings_);
                drawFixturePiece({0.0F, -0.045F, 0.0F},
                    {2.48F, 0.15F, 0.31F}, {}, {},
                    {0.15F, 0.14F, 0.13F}, 0.0F);
                drawFixturePiece({0.0F, -0.129F, 0.0F},
                    {2.27F, 0.045F, 0.22F}, {}, {},
                    lensTint, lensEmission * 2.1F);
                ceilingFixtureMaterial_.albedo.bind(gl_, textureBindings_, 0U);
                ceilingFixtureMaterial_.normal.bind(gl_, textureBindings_, 1U);
                ceilingFixtureMaterial_.properties.bind(gl_, textureBindings_, 2U);
                ceilingFixtureMaterial_.emission.bind(gl_, textureBindings_, 3U);
                shader.setInt(gl_, "uDetailedMaterial", 1);
                shader.setInt(gl_, "uUseEmissionMap", 1);
            }
            continue;
        }
        drawFixturePiece({0.0F, 0.095F, 0.0F}, {1.20F, 0.12F, 0.28F},
            {0.46F, 0.96F}, {0.02F, 0.02F}, housingTint, 0.0F);
        drawFixturePiece({0.0F, 0.015F, 0.0F}, {1.17F, 0.055F, 0.255F},
            {0.14F, 0.96F}, {0.52F, 0.02F}, housingTint, 0.0F);
        drawFixturePiece({0.0F, -0.022F, 0.0F}, {1.10F, 0.040F, 0.215F},
            {0.05F, 0.96F}, {0.69F, 0.02F}, housingTint, 0.0F);
        drawFixturePiece({0.0F, -0.052F, 0.0F}, {1.03F, 0.030F, 0.170F},
            {0.21F, 0.96F}, {0.77F, 0.02F}, lensTint, lensEmission);

        constexpr std::array fastenerOffsets{
            wkc::core::Vec3{-0.525F, -0.041F, -0.105F},
            wkc::core::Vec3{0.525F, -0.041F, -0.105F},
            wkc::core::Vec3{-0.525F, -0.041F, 0.105F},
            wkc::core::Vec3{0.525F, -0.041F, 0.105F},
        };
        for (const wkc::core::Vec3 fastenerOffset : fastenerOffsets) {
            drawFixturePiece(fastenerOffset, {0.045F, 0.018F, 0.045F},
                {0.14F, 0.96F}, {0.52F, 0.02F}, housingTint, 0.0F);
        }
    }
    shader.setInt(gl_, "uUseEmissionMap", 0);
    shader.setInt(gl_, "uDetailedMaterial", 0);
    shader.setFloat(gl_, "uEmissiveStrength", 0.0F);

    for (const EnemyRenderData& enemy : enemies) {
        auto* visuals = &workerVisuals_;
        if (enemy.role == EnemyVisualRole::Guard) {
            visuals = &guardVisuals_;
        } else if (enemy.role == EnemyVisualRole::Officer) {
            visuals = &officerVisuals_;
        } else if (enemy.role == EnemyVisualRole::Engineer) {
            visuals = &engineerVisuals_;
        } else if (enemy.role == EnemyVisualRole::IndustrialRobot) {
            visuals = &industrialRobotVisuals_;
        }
        auto visual = visuals->find(enemy.visualSeed);
        if (visual == visuals->end()) {
            visual = visuals->find(0);
        }
        if (visual == visuals->end()) {
            return false;
        }
        visual->second.texture.bind(gl_, textureBindings_);
        const EnemyAnimationState currentAnimationState
            = enemy.alive ? enemy.animationState : EnemyAnimationState::Dead;
        const EnemyRootPose currentPose = sampleEnemyRootPose(
            currentAnimationState,
            enemy.animationTick, enemy.visualSeed, enemy.active, enemy.role);
        const EnemyRootPose previousPose = sampleEnemyRootPose(
            enemy.previousAnimationState,
            enemy.animationTick, enemy.visualSeed, enemy.active, enemy.role);
        const float transitionBlend = std::isfinite(enemy.animationTransitionBlend)
            ? std::clamp(enemy.animationTransitionBlend, 0.0F, 1.0F) : 1.0F;
        const EnemyRootPose pose = currentAnimationState == EnemyAnimationState::Dead
            ? sampleEnemyRagdollRootPose(
                previousPose, enemy.visualSeed, enemy.role, transitionBlend)
            : blendEnemyRootPoses(previousPose, currentPose, transitionBlend);
        const wkc::core::Vec3 position = enemy.position + pose.offset;
        constexpr float degreesToRadians = std::numbers::pi_v<float> / 180.0F;
        const wkc::core::Mat4 rootModel = wkc::core::translationMatrix(position)
            * wkc::core::rotationYMatrix(enemy.yawDegrees * degreesToRadians)
            * wkc::core::rotationZMatrix(
                pose.rotationDegrees.z * degreesToRadians)
            * wkc::core::rotationXMatrix(
                pose.rotationDegrees.x * degreesToRadians)
            * wkc::core::scaleMatrix(pose.scale);
        const bool hit = enemy.alive && enemy.active
            && enemy.animationState == EnemyAnimationState::Hit;
        const float hitStrength = std::isfinite(enemy.hitReactionStrength)
            ? std::clamp(enemy.hitReactionStrength, 0.0F, 1.0F) : 0.0F;
        const float visibleHitStrength
            = hit ? hitStrength * transitionBlend : hitStrength;
        const float deathBlend = !enemy.alive ? transitionBlend : 0.0F;
        const wkc::core::Vec3 enemyTint = !enemy.alive
                ? wkc::core::Vec3{
                    1.0F - (deathBlend * 0.70F),
                    1.0F - (deathBlend * 0.76F),
                    1.0F - (deathBlend * 0.80F)}
                : hit
                ? wkc::core::Vec3{
                    1.0F, 1.0F - (visibleHitStrength * 0.28F),
                    1.0F - (visibleHitStrength * 0.32F)}
                : enemy.active
                ? wkc::core::Vec3{1.0F, 1.0F, 1.0F}
                : wkc::core::Vec3{0.68F, 0.72F, 0.78F};
        shader.setInt(gl_, "uDetailedMaterial", 0);
        shader.setVec2(gl_, "uUvScale", {4.0F, 4.0F});
        shader.setVec2(gl_, "uUvOffset", {});
        const float enemyEmissiveStrength =
            enemy.alive
                ? visual->second.emissiveStrength
                    * (hit ? 1.0F + (visibleHitStrength * 0.15F)
                           : enemy.active ? 1.0F : 0.25F)
                : visual->second.emissiveStrength * (1.0F - deathBlend);
        for (const EnemyGpuPart& part : visual->second.parts) {
            const bool referencePart = part.referenceMaterialId <= 5U;
            const auto roleIndex = static_cast<std::size_t>(enemy.role);
            const bool paintedReference = referencePart
                && referencePainted(part.referenceMaterialId);
            if (referencePart) {
                if (paintedReference && referencePaintReady_[roleIndex]) {
                    referencePaintTextures_[roleIndex].bind(gl_, textureBindings_);
                } else if (paintedReference) {
                    referenceNeutralPaintTexture_.bind(gl_, textureBindings_);
                } else {
                    referenceWhiteTexture_.bind(gl_, textureBindings_);
                }
                shader.setVec2(gl_, "uUvScale", {1.0F, 1.0F});
                shader.setVec2(gl_, "uUvOffset", {});
            } else if (enemy.role == EnemyVisualRole::Worker && part.workerFrontPanel) {
                workerFrontPanelTexture_.bind(gl_, textureBindings_);
                shader.setVec2(gl_, "uUvScale", {0.095F, -0.126F});
                shader.setVec2(gl_, "uUvOffset", {0.453F, 0.698F});
            } else if (enemy.role == EnemyVisualRole::Worker && part.workerPaint) {
                workerPaintTexture_.bind(gl_, textureBindings_);
                shader.setVec2(gl_, "uUvScale", {1.0F, 1.0F});
                shader.setVec2(gl_, "uUvOffset", {});
            } else {
                visual->second.texture.bind(gl_, textureBindings_);
                shader.setVec2(gl_, "uUvScale", {4.0F, 4.0F});
                shader.setVec2(gl_, "uUvOffset", {});
            }
            shader.setFloat(gl_, "uWorkerPaintWear",
                !referencePart && enemy.role == EnemyVisualRole::Worker && part.workerPaint
                    ? std::clamp(0.32F + visual->second.wearAmount * 0.46F,
                        0.32F, 0.62F)
                    : 0.0F);
            shader.setFloat(gl_, "uEmissiveStrength",
                referencePart && part.referenceMaterialId == 3U
                    ? (enemy.alive ? (enemy.active ? 1.15F : 0.24F) : 0.0F)
                : part.workerEyeCore && enemy.alive
                    ? (enemy.active ? 1.45F : 0.35F) : enemyEmissiveStrength);
            shader.setInt(gl_, "uEyeHalo", part.workerEyeHalo ? 1 : 0);
            const wkc::core::Vec3 materialTint
                = referencePart
                ? paintedReference && referencePaintReady_[roleIndex]
                    ? wkc::core::Vec3{1.0F, 1.0F, 1.0F}
                    : referenceMaterialColor(enemy.role, part.referenceMaterialId)
                : enemy.role == EnemyVisualRole::Worker && part.workerPaint
                ? part.workerFrontPanel
                    ? wkc::core::Vec3{0.68F, 0.68F, 0.68F}
                    : wkc::core::Vec3{0.77F, 0.76F, 0.75F}
                : part.tint;
            shader.setVec3(gl_, "uTint", {
                enemyTint.x * materialTint.x,
                enemyTint.y * materialTint.y,
                enemyTint.z * materialTint.z,
            });
            const auto blendedPoseFor = [&](const std::uint32_t partIndex) {
                const EnemyPartPose currentLimbPose = sampleEnemyPartPose(
                    currentAnimationState,
                    enemy.animationTick, enemy.visualSeed, partIndex,
                    visual->second.basePartCount,
                    hitStrength, enemy.active, enemy.role,
                    enemy.attackVisualKind, enemy.attackPulse);
                const EnemyPartPose previousLimbPose = sampleEnemyPartPose(
                    enemy.previousAnimationState,
                    enemy.animationTick, enemy.visualSeed, partIndex,
                    visual->second.basePartCount,
                    hitStrength, enemy.active, enemy.role,
                    enemy.attackVisualKind, enemy.attackPulse);
                return currentAnimationState == EnemyAnimationState::Dead
                    ? sampleEnemyRagdollPartPose(
                        previousLimbPose, enemy.visualSeed, enemy.role, partIndex,
                        visual->second.basePartCount, transitionBlend)
                    : blendEnemyPartPoses(
                        previousLimbPose, currentLimbPose, transitionBlend);
            };
            const auto posedModel = [&](const wkc::core::Vec3 partPivot,
                                         const EnemyPartPose limbPose) {
                return wkc::core::translationMatrix(partPivot + limbPose.offset)
                    * wkc::core::rotationXMatrix(
                        limbPose.rotationDegrees.x * degreesToRadians)
                    * wkc::core::rotationYMatrix(
                        limbPose.rotationDegrees.y * degreesToRadians)
                    * wkc::core::rotationZMatrix(
                        limbPose.rotationDegrees.z * degreesToRadians)
                    * wkc::core::translationMatrix(partPivot * -1.0F);
            };
            const EnemyPartPose partPose = blendedPoseFor(part.index);
            wkc::core::Mat4 partModel = posedModel(part.pivot, partPose);
            if (referencePart && enemy.role == EnemyVisualRole::Guard
                && (part.index == 8U || part.index == 9U)) {
                // The separate forearm follows its shoulder before bending
                // around the elbow. This keeps both joints connected while
                // the hands move to different grips on the rifle.
                constexpr float modelScale = 1.8F / 5.35F;
                const float side = part.index == 8U ? 1.0F : -1.0F;
                const wkc::core::Vec3 shoulderPivot{
                    side * 0.95F * modelScale,
                    4.08F * modelScale, 0.01F * modelScale};
                const EnemyPartPose shoulderPose = blendedPoseFor(
                    part.index == 8U ? 2U : 3U);
                partModel = posedModel(shoulderPivot, shoulderPose) * partModel;
            }
            wkc::core::Mat4 detailModel = partModel;
            if (part.workerSawBlade && enemy.alive && enemy.active) {
                // Rotation about the blade axle is visual only. The tool's
                // existing pose still carries the whole saw with the hand.
                constexpr std::uint64_t spinPeriod = 360U;
                const float spinRadians
                    = static_cast<float>(enemy.animationTick % spinPeriod)
                        * 17.0F * degreesToRadians;
                detailModel = detailModel
                    * wkc::core::translationMatrix(part.workerSawCenter)
                    * wkc::core::rotationXMatrix(spinRadians)
                    * wkc::core::translationMatrix({
                        -part.workerSawCenter.x,
                        -part.workerSawCenter.y,
                        -part.workerSawCenter.z});
            }
            const wkc::core::Mat4 posedPartModel = rootModel * detailModel;
            shader.setMat4(gl_, "uModel", posedPartModel);
            if (geometryShadowsActive && !part.workerEyeHalo) {
                shadowCasters.push_back({&part.mesh, posedPartModel});
            }
            if (part.workerEyeHalo) {
                shader.setFloat(gl_, "uOpacity",
                    enemy.alive ? (enemy.active ? 0.28F : 0.08F) : 0.0F);
                shader.setFloat(gl_, "uUnlitMix", 1.0F);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                glDepthMask(GL_FALSE);
            }
            drawMesh(part.mesh);
            if (part.workerEyeHalo) {
                glDepthMask(GL_TRUE);
                glDisable(GL_BLEND);
                shader.setFloat(gl_, "uOpacity", 1.0F);
                shader.setFloat(gl_, "uUnlitMix", 0.0F);
            }
        }
    }
    shader.setFloat(gl_, "uWorkerPaintWear", 0.0F);
    shader.setFloat(gl_, "uEmissiveStrength", 0.0F);
    shader.setInt(gl_, "uEyeHalo", 0);
    shader.setVec2(gl_, "uUvScale", {4.0F, 4.0F});
    shader.setVec2(gl_, "uUvOffset", {});

    fallbackTexture_.bind(gl_, textureBindings_);
    for (const PickupRenderData& pickup : pickups) {
        if (pickup.role == PickupVisualRole::Weapon
            || pickup.role == PickupVisualRole::DroppedPistol
            || pickup.role == PickupVisualRole::DroppedSniper
            || pickup.role == PickupVisualRole::DroppedShotgun
            || pickup.role == PickupVisualRole::DroppedSubmachineGun) {
            constexpr float degreesToRadians = std::numbers::pi_v<float> / 180.0F;
            const HudWeaponRole weaponRole
                = pickup.role == PickupVisualRole::DroppedPistol
                ? HudWeaponRole::Pistol
                : pickup.role == PickupVisualRole::DroppedSniper ? HudWeaponRole::Sniper
                : pickup.role == PickupVisualRole::DroppedShotgun ? HudWeaponRole::Shotgun
                : pickup.role == PickupVisualRole::DroppedSubmachineGun
                    ? HudWeaponRole::SubmachineGun
                : HudWeaponRole::Carbine;
            const DroppedWeaponPose pose = sampleDroppedWeaponPose(
                pickup.visualSeed, pickup.role, pickup.fallProgress);
            const float yaw = (pickup.baseYawDegrees + pose.yawDegrees)
                * degreesToRadians;
            // A faint footprint anchors the visual mesh without a marker or
            // pedestal. It is not a gameplay collider and does not cast light.
            const wkc::core::Vec3 shadowScale
                = weaponRole == HudWeaponRole::Pistol
                ? wkc::core::Vec3{0.27F, 0.003F, 0.52F}
                : weaponRole == HudWeaponRole::SubmachineGun
                    ? wkc::core::Vec3{0.30F, 0.003F, 0.92F}
                : wkc::core::Vec3{0.32F, 0.003F, 1.25F};
            shader.setMat4(gl_, "uModel",
                wkc::core::translationMatrix(pickup.position
                    + wkc::core::Vec3{0.0F, 0.004F, 0.0F})
                    * wkc::core::rotationYMatrix(yaw)
                    * wkc::core::scaleMatrix(shadowScale));
            shader.setVec3(gl_, "uTint", {0.025F, 0.03F, 0.035F});
            shader.setInt(gl_, "uDetailedMaterial", 0);
            shader.setFloat(gl_, "uEmissiveStrength", 0.0F);
            shader.setFloat(gl_, "uOpacity", 0.36F
                * std::clamp(pickup.fallProgress, 0.0F, 1.0F));
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
            drawMesh(box_);
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
            shader.setFloat(gl_, "uOpacity", 1.0F);
            const wkc::core::Mat4 model
                = wkc::core::translationMatrix(pickup.position
                    + wkc::core::Vec3{0.0F, pose.height, 0.0F})
                * wkc::core::rotationYMatrix(yaw)
                * wkc::core::rotationZMatrix(pose.rollDegrees * degreesToRadians);
            shader.setMat4(gl_, "uModel", model);
            shader.setInt(gl_, "uDetailedMaterial", 1);
            shader.setInt(gl_, "uUseEmissionMap", 0);
            shader.setVec2(gl_, "uUvScale", {1.0F, 1.0F});
            shader.setVec2(gl_, "uUvOffset", {});
            const std::array materialColors{
                wkc::core::Vec3{0.45F, 0.48F, 0.50F},
                wkc::core::Vec3{0.64F, 0.67F, 0.68F},
                wkc::core::Vec3{0.23F, 0.24F, 0.24F},
                wkc::core::Vec3{0.075F, 0.085F, 0.09F},
                wkc::core::Vec3{0.12F, 0.72F, 0.80F},
                wkc::core::Vec3{1.0F, 1.0F, 1.0F},
            };
            const std::size_t weaponIndex = static_cast<std::size_t>(weaponRole);
            for (std::size_t material = 0; material < weaponMaterialCount; ++material) {
                const wkc::render::Mesh& mesh = weaponMeshes_[weaponIndex][material];
                if (!mesh.isValid()) {
                    continue;
                }
                const WeaponGpuMaterial& gpuMaterial = weaponMaterials_[material];
                gpuMaterial.albedo.bind(gl_, textureBindings_, 0U);
                gpuMaterial.normal.bind(gl_, textureBindings_, 1U);
                gpuMaterial.properties.bind(gl_, textureBindings_, 2U);
                shader.setVec3(gl_, "uTint", materialColors[material]);
                shader.setFloat(gl_, "uEmissiveStrength", 0.0F);
                drawMesh(mesh);
            }
            fallbackTexture_.bind(gl_, textureBindings_);
            continue;
        }
        // Supply pickups share the room's metal language, but each has a
        // distinct silhouette and readable mark instead of a plain color box.
        shader.setInt(gl_, "uDetailedMaterial", 0);
        shader.setVec2(gl_, "uUvScale", {1.0F, 1.0F});
        shader.setVec2(gl_, "uUvOffset", {});
        const float yaw = static_cast<float>(pickup.visualSeed & 3U)
            * std::numbers::pi_v<float> * 0.5F;
        const wkc::core::Mat4 pickupRoot
            = wkc::core::translationMatrix(pickup.position)
            * wkc::core::rotationYMatrix(yaw);
        const auto pickupPart = [&](const wkc::core::Vec3 center,
            const wkc::core::Vec3 dimensions,
            const wkc::core::Vec3 color, const float emission = 0.0F) {
            shader.setMat4(gl_, "uModel", pickupRoot
                * wkc::core::translationMatrix(center)
                * wkc::core::scaleMatrix(dimensions));
            shader.setVec3(gl_, "uTint", color);
            shader.setFloat(gl_, "uEmissiveStrength", emission);
            drawMesh(box_);
        };
        constexpr wkc::core::Vec3 darkHousing{0.20F, 0.23F, 0.24F};
        constexpr wkc::core::Vec3 edgeSteel{0.55F, 0.58F, 0.55F};
        switch (pickup.role) {
        case PickupVisualRole::Health:
            pickupPart({0.0F, 0.10F, 0.0F}, {0.42F, 0.19F, 0.32F},
                {0.29F, 0.48F, 0.34F});
            pickupPart({0.0F, 0.205F, 0.0F}, {0.36F, 0.020F, 0.26F},
                darkHousing);
            pickupPart({0.0F, 0.220F, 0.0F}, {0.22F, 0.012F, 0.055F},
                {0.77F, 0.89F, 0.72F}, 0.25F);
            pickupPart({0.0F, 0.220F, 0.0F}, {0.055F, 0.012F, 0.20F},
                {0.77F, 0.89F, 0.72F}, 0.25F);
            break;
        case PickupVisualRole::Ammo:
            pickupPart({0.0F, 0.065F, 0.0F}, {0.46F, 0.12F, 0.30F},
                darkHousing);
            pickupPart({0.0F, 0.133F, 0.0F}, {0.41F, 0.022F, 0.25F},
                edgeSteel);
            for (const float x : {-0.12F, 0.0F, 0.12F}) {
                pickupPart({x, 0.160F, 0.0F}, {0.075F, 0.036F, 0.19F},
                    {0.70F, 0.49F, 0.19F});
                pickupPart({x, 0.181F, -0.065F},
                    {0.075F, 0.010F, 0.045F},
                    {0.88F, 0.70F, 0.36F}, 0.08F);
            }
            break;
        case PickupVisualRole::Key:
            pickupPart({0.0F, 0.035F, 0.0F}, {0.46F, 0.065F, 0.24F},
                darkHousing);
            pickupPart({0.0F, 0.073F, 0.0F}, {0.40F, 0.012F, 0.18F},
                {0.17F, 0.52F, 0.60F}, 0.12F);
            pickupPart({-0.11F, 0.083F, 0.0F},
                {0.055F, 0.010F, 0.11F},
                {0.63F, 0.87F, 0.88F}, 0.25F);
            pickupPart({0.14F, 0.083F, 0.0F},
                {0.055F, 0.010F, 0.11F},
                {0.79F, 0.50F, 0.18F});
            break;
        case PickupVisualRole::Armor:
            pickupPart({0.0F, 0.090F, 0.0F}, {0.44F, 0.17F, 0.36F},
                darkHousing);
            pickupPart({0.0F, 0.183F, 0.0F}, {0.36F, 0.024F, 0.28F},
                {0.32F, 0.42F, 0.48F});
            pickupPart({0.0F, 0.199F, 0.0F}, {0.12F, 0.012F, 0.22F},
                {0.22F, 0.66F, 0.82F}, 0.22F);
            break;
        case PickupVisualRole::Special:
            pickupPart({0.0F, 0.035F, 0.0F}, {0.24F, 0.07F, 0.24F},
                darkHousing);
            pickupPart({0.0F, 0.17F, 0.0F}, {0.16F, 0.25F, 0.16F},
                {0.82F, 0.24F, 0.43F}, 0.30F);
            for (const float side : {-1.0F, 1.0F}) {
                pickupPart({side * 0.095F, 0.17F, 0.0F},
                    {0.028F, 0.27F, 0.20F}, darkHousing);
            }
            pickupPart({0.0F, 0.345F, 0.0F}, {0.24F, 0.035F, 0.24F},
                edgeSteel);
            pickupPart({0.0F, 0.225F, 0.086F},
                {0.11F, 0.055F, 0.010F},
                {0.91F, 0.42F, 0.56F}, 0.35F);
            break;
        default:
            break;
        }
        shader.setFloat(gl_, "uEmissiveStrength", 0.0F);
    }

    shader.setFloat(gl_, "uPropIndirectFill", 0.28F);
    for (const PropRenderData& prop : props) {
        if (prop.role == PropVisualRole::Terminal) {
            const wkc::core::Mat4 model
                = wkc::core::translationMatrix(prop.position)
                    * wkc::core::rotationYMatrix(prop.yawDegrees
                        * std::numbers::pi_v<float> / 180.0F);
            wkc::core::Vec3 statusColor{0.28F, 0.77F, 0.82F};
            if (prop.terminalState == TerminalVisualState::Locked) {
                statusColor = {0.91F, 0.48F, 0.10F};
            } else if (prop.terminalState == TerminalVisualState::Alert) {
                statusColor = (hud.elapsedRunTicks / 60U) % 2U == 0U
                    ? wkc::core::Vec3{0.98F, 0.14F, 0.08F}
                    : wkc::core::Vec3{0.10F, 0.025F, 0.025F};
            } else if (prop.terminalState == TerminalVisualState::Ready) {
                statusColor = {0.22F, 0.78F, 0.42F};
            }
            shader.setFloat(gl_, "uPropIndirectFill", 0.08F);
            shader.setMat4(gl_, "uModel", model);
            shader.setVec2(gl_, "uUvScale", {1.0F, 1.0F});
            shader.setVec2(gl_, "uUvOffset", {});
            for (const TerminalGpuPart& part : terminalModelParts_) {
                const bool metal = part.materialId <= 3U;
                if (metal) {
                    terminalMaterial_.albedo.bind(gl_, textureBindings_, 0U);
                    terminalMaterial_.normal.bind(gl_, textureBindings_, 1U);
                    terminalMaterial_.properties.bind(gl_, textureBindings_, 2U);
                    terminalMaterial_.emission.bind(gl_, textureBindings_, 3U);
                    shader.setInt(gl_, "uDetailedMaterial", 1);
                    shader.setInt(gl_, "uUseEmissionMap", 1);
                    shader.setFloat(gl_, "uEmissiveStrength", 0.0F);
                    shader.setVec3(gl_, "uTint", prop.bossArena
                        ? part.materialId == 0U
                            ? wkc::core::Vec3{0.78F, 0.69F, 0.57F}
                            : part.materialId == 1U
                                ? wkc::core::Vec3{0.33F, 0.31F, 0.28F}
                            : part.materialId == 2U
                                ? wkc::core::Vec3{0.87F, 0.73F, 0.58F}
                            : wkc::core::Vec3{0.82F, 0.74F, 0.65F}
                        : part.materialId == 1U
                            ? wkc::core::Vec3{0.36F, 0.40F, 0.43F}
                            : wkc::core::Vec3{1.0F, 1.0F, 1.0F});
                    if (geometryShadowsActive && part.partId == 0U) {
                        shadowCasters.push_back({&part.mesh, model});
                    }
                } else {
                    solidColorTexture_.bind(gl_, textureBindings_);
                    shader.setInt(gl_, "uDetailedMaterial", 0);
                    shader.setInt(gl_, "uUseEmissionMap", 0);
                    shader.setVec3(gl_, "uTint", part.materialId == 4U
                        ? wkc::core::Vec3{0.012F, 0.044F, 0.054F}
                        : part.partId == 1U
                            ? (prop.bossArena
                                ? wkc::core::Vec3{1.0F, 0.48F, 0.16F}
                                : wkc::core::Vec3{0.96F, 0.81F, 0.62F})
                            : statusColor);
                    shader.setFloat(gl_, "uEmissiveStrength",
                        part.materialId == 4U ? 0.0F
                            : part.partId == 1U ? 0.55F
                            : prop.terminalState == TerminalVisualState::Alert
                                && (hud.elapsedRunTicks / 60U) % 2U != 0U
                                ? 0.0F : 0.65F);
                }
                drawMesh(part.mesh);
            }
            shader.setFloat(gl_, "uPropIndirectFill", 0.28F);
            shader.setInt(gl_, "uDetailedMaterial", 0);
            shader.setInt(gl_, "uUseEmissionMap", 0);
            shader.setFloat(gl_, "uEmissiveStrength", 0.0F);
            fallbackTexture_.bind(gl_, textureBindings_);
            continue;
        }
        if (prop.role == PropVisualRole::SecurityMonitor
            || prop.role == PropVisualRole::CafeteriaPlanter) {
            solidColorTexture_.bind(gl_, textureBindings_);
            shader.setInt(gl_, "uDetailedMaterial", 0);
            shader.setInt(gl_, "uUseEmissionMap", 0);
            shader.setVec2(gl_, "uUvScale", {1.0F, 1.0F});
            shader.setVec2(gl_, "uUvOffset", {});
            if (geometryShadowsActive
                && prop.state != PropVisualState::Destroyed) {
                shadowCasters.push_back({&box_,
                    wkc::core::translationMatrix(prop.position)
                        * wkc::core::scaleMatrix(prop.scale)});
            }
            const auto piece = [&](const wkc::core::Vec3 center,
                                   const wkc::core::Vec3 size,
                                   const wkc::core::Vec3 color,
                                   const float glow = 0.0F,
                                   wkc::render::Mesh* mesh = nullptr,
                                   const float yaw = 0.0F) {
                shader.setMat4(gl_, "uModel",
                    wkc::core::translationMatrix(center)
                        * wkc::core::rotationYMatrix(yaw)
                        * wkc::core::scaleMatrix(size));
                shader.setVec3(gl_, "uTint", color);
                shader.setFloat(gl_, "uEmissiveStrength", glow);
                drawMesh(mesh ? *mesh : box_);
            };
            if (prop.role == PropVisualRole::SecurityMonitor) {
                const wkc::core::Vec3 casing{0.19F, 0.25F, 0.28F};
                const wkc::core::Vec3 rim{0.42F, 0.48F, 0.49F};
                const wkc::core::Vec3 activeColor
                    = prop.terminalState == TerminalVisualState::Alert
                        ? wkc::core::Vec3{0.95F, 0.16F, 0.10F}
                    : prop.terminalState == TerminalVisualState::Locked
                        ? wkc::core::Vec3{0.96F, 0.70F, 0.12F}
                    : prop.terminalState == TerminalVisualState::Ready
                        ? wkc::core::Vec3{0.27F, 0.82F, 0.53F}
                        : wkc::core::Vec3{0.23F, 0.64F, 0.76F};
                const bool lightOn = prop.terminalState
                        != TerminalVisualState::Alert
                    || (hud.elapsedRunTicks / 60U) % 2U == 0U;
                const bool facesX = prop.scale.x < prop.scale.z;
                const float length = facesX ? prop.scale.z : prop.scale.x;
                const float depth = facesX ? prop.scale.x : prop.scale.z;
                if (prop.state == PropVisualState::Destroyed) {
                    const float base = prop.position.y - prop.scale.y * 0.5F;
                    piece({prop.position.x, base + 0.025F, prop.position.z},
                        {prop.scale.x * 0.82F, 0.05F, prop.scale.z * 0.82F}, casing);
                    for (const float side : {-1.0F, 1.0F}) {
                        piece(prop.position + wkc::core::Vec3{
                                facesX ? side * depth * 0.36F : side * length * 0.29F,
                                -prop.scale.y * 0.5F + 0.075F,
                                facesX ? side * length * 0.29F : side * depth * 0.36F},
                            {0.13F, 0.10F, 0.14F}, rim);
                    }
                } else {
                    piece(prop.position, prop.scale, casing);
                    const float faceOffset = depth * 0.5F + 0.012F;
                    const float panelWidth = length * 0.25F;
                    for (const float side : {-1.0F, 1.0F}) {
                        for (int screen = 0; screen < 3; ++screen) {
                            const float along = (static_cast<float>(screen) - 1.0F)
                                * length * 0.29F;
                            const wkc::core::Vec3 face = prop.position
                                + wkc::core::Vec3{
                                    facesX ? side * faceOffset : along,
                                    0.0F,
                                    facesX ? along : side * faceOffset};
                            const wkc::core::Vec3 panelSize = facesX
                                ? wkc::core::Vec3{0.028F,
                                    prop.scale.y * 0.68F, panelWidth}
                                : wkc::core::Vec3{panelWidth,
                                    prop.scale.y * 0.68F, 0.028F};
                            piece(face, panelSize, rim);
                            piece(face + wkc::core::Vec3{
                                    facesX ? side * 0.018F : 0.0F, 0.0F,
                                    facesX ? 0.0F : side * 0.018F},
                                panelSize * 0.86F,
                                prop.state == PropVisualState::Damaged
                                    ? wkc::core::Vec3{0.025F, 0.027F, 0.026F}
                                    : wkc::core::Vec3{0.022F, 0.045F, 0.055F});
                            if (prop.state == PropVisualState::Intact && lightOn) {
                                piece(face + wkc::core::Vec3{
                                        facesX ? side * 0.035F : 0.0F,
                                        prop.scale.y * 0.18F,
                                        facesX ? 0.0F : side * 0.035F},
                                    facesX
                                        ? wkc::core::Vec3{0.009F, 0.024F,
                                            panelWidth * 0.72F}
                                        : wkc::core::Vec3{panelWidth * 0.72F,
                                            0.024F, 0.009F},
                                    activeColor, 0.58F);
                            }
                        }
                    }
                }
            } else {
                const float base = prop.position.y - prop.scale.y * 0.5F;
                const wkc::core::Vec3 pot{0.35F, 0.39F, 0.37F};
                if (prop.state == PropVisualState::Destroyed) {
                    for (const float side : {-1.0F, 1.0F}) {
                        piece({prop.position.x + side * prop.scale.x * 0.28F,
                                base + 0.065F,
                                prop.position.z - side * prop.scale.z * 0.17F},
                            {prop.scale.x * 0.42F, 0.13F,
                                prop.scale.z * 0.38F}, pot);
                        piece({prop.position.x + side * prop.scale.x * 0.28F,
                                base + 0.136F,
                                prop.position.z - side * prop.scale.z * 0.17F},
                            {prop.scale.x * 0.39F, 0.012F,
                                prop.scale.z * 0.35F},
                            {0.60F, 0.62F, 0.54F});
                    }
                    for (int leaf = 0; leaf < 5; ++leaf) {
                        const float angle = static_cast<float>(leaf)
                            * (2.0F * std::numbers::pi_v<float> / 5.0F);
                        piece({prop.position.x + std::cos(angle) * 0.18F,
                                base + 0.12F,
                                prop.position.z + std::sin(angle) * 0.18F},
                            {0.48F, 0.22F, 0.48F},
                            leaf % 2 == 0
                                ? wkc::core::Vec3{0.30F, 0.48F, 0.24F}
                                : wkc::core::Vec3{0.21F, 0.38F, 0.19F},
                            0.0F, &planterLeafMesh_, angle);
                    }
                } else {
                    piece({prop.position.x, base + prop.scale.y * 0.40F,
                            prop.position.z},
                        {prop.scale.x, prop.scale.y * 0.80F, prop.scale.z}, pot);
                    piece({prop.position.x, base + prop.scale.y * 0.78F,
                            prop.position.z},
                        {prop.scale.x * 0.90F, 0.06F,
                            prop.scale.z * 0.90F},
                        {0.56F, 0.58F, 0.51F});
                    piece({prop.position.x, base + prop.scale.y * 0.82F,
                            prop.position.z},
                        {prop.scale.x * 0.71F, 0.025F,
                            prop.scale.z * 0.71F},
                        {0.07F, 0.09F, 0.07F});
                    for (int leaf = 0; leaf < 7; ++leaf) {
                        const float angle = static_cast<float>(leaf)
                            * (2.0F * std::numbers::pi_v<float> / 7.0F);
                        piece({prop.position.x + std::cos(angle) * 0.06F,
                                base + prop.scale.y * 0.82F,
                                prop.position.z + std::sin(angle) * 0.06F},
                            {0.88F, 0.43F + 0.05F * static_cast<float>(leaf % 3),
                                0.88F},
                            leaf % 2 == 0
                                ? wkc::core::Vec3{0.27F, 0.45F, 0.25F}
                                : wkc::core::Vec3{0.20F, 0.36F, 0.19F},
                            0.0F, &planterLeafMesh_, angle);
                    }
                }
            }
            shader.setFloat(gl_, "uEmissiveStrength", 0.0F);
            fallbackTexture_.bind(gl_, textureBindings_);
            continue;
        }
        if (prop.role == PropVisualRole::LaboratoryPlanter) {
            solidColorTexture_.bind(gl_, textureBindings_);
            shader.setInt(gl_, "uDetailedMaterial", 0);
            shader.setVec2(gl_, "uUvScale", {1.0F, 1.0F});
            shader.setVec2(gl_, "uUvOffset", {});
            shader.setFloat(gl_, "uEmissiveStrength", 0.0F);
            const auto planterPart = [&](const wkc::core::Vec3 center,
                                         const wkc::core::Vec3 size,
                                         const wkc::core::Vec3 tint,
                                         wkc::render::Mesh* mesh = nullptr) {
                shader.setMat4(gl_, "uModel",
                    wkc::core::translationMatrix(prop.position + center)
                        * wkc::core::scaleMatrix(size));
                shader.setVec3(gl_, "uTint", tint);
                drawMesh(mesh != nullptr ? *mesh : box_);
            };
            if (prop.state == PropVisualState::Destroyed) {
                planterPart({-0.09F, 0.015F, 0.03F},
                    {0.21F, 0.03F, 0.25F}, {0.30F, 0.41F, 0.44F});
                planterPart({0.12F, 0.018F, -0.07F},
                    {0.17F, 0.035F, 0.18F}, {0.19F, 0.27F, 0.30F});
            } else {
                if (geometryShadowsActive) {
                    shadowCasters.push_back({&box_,
                        wkc::core::translationMatrix(prop.position
                            + wkc::core::Vec3{0.0F, 0.08F, 0.0F})
                            * wkc::core::scaleMatrix({0.44F, 0.16F, 0.44F})});
                }
                planterPart({0.0F, 0.08F, 0.0F},
                    {0.44F, 0.16F, 0.44F}, {0.55F, 0.69F, 0.70F});
                planterPart({0.0F, 0.167F, 0.0F},
                    {0.36F, 0.014F, 0.36F}, {0.045F, 0.075F, 0.070F});
                for (int leaf = 0; leaf < 7; ++leaf) {
                    const float angle = static_cast<float>(leaf)
                        * (2.0F * std::numbers::pi_v<float> / 7.0F);
                    const wkc::core::Vec3 leafBase{
                        std::cos(angle) * 0.01F,
                        0.17F,
                        std::sin(angle) * 0.01F};
                    const wkc::core::Vec3 leafScale{0.20F,
                        0.17F + 0.015F * static_cast<float>(leaf % 3), 0.20F};
                    shader.setMat4(gl_, "uModel",
                        wkc::core::translationMatrix(prop.position + leafBase)
                            * wkc::core::rotationYMatrix(angle)
                            * wkc::core::scaleMatrix(leafScale));
                    shader.setVec3(gl_, "uTint", leaf % 2 == 0
                        ? wkc::core::Vec3{0.24F, 0.52F, 0.38F}
                        : wkc::core::Vec3{0.16F, 0.39F, 0.31F});
                    drawMesh(planterLeafMesh_);
                }
                if (prop.state == PropVisualState::Intact) {
                    planterPart({0.0F, 0.172F, 0.0F},
                        {0.38F, 0.025F, 0.38F},
                        {0.57F, 0.72F, 0.75F}, &grenadeRing_);
                    planterPart({0.0F, 0.415F, 0.0F},
                        {0.07F, 0.035F, 0.07F}, {0.69F, 0.79F, 0.79F});
                    glassTestDomes.push_back({
                        prop.position + wkc::core::Vec3{0.0F, 0.172F, 0.0F},
                        0.20F, 0.24F});
                }
            }
            if (prop.state != PropVisualState::Intact) {
                // Glass remains visible after either the dome-only or direct pot shot.
                planterPart({-0.25F, 0.015F, 0.03F},
                    {0.08F, 0.022F, 0.15F}, {0.45F, 0.71F, 0.76F});
                planterPart({0.26F, 0.014F, -0.06F},
                    {0.09F, 0.020F, 0.11F}, {0.55F, 0.78F, 0.81F});
                planterPart({0.04F, 0.012F, 0.27F},
                    {0.13F, 0.018F, 0.08F}, {0.41F, 0.66F, 0.72F});
            }
            fallbackTexture_.bind(gl_, textureBindings_);
            continue;
        }
        const bool storageBox = prop.role == PropVisualRole::StorageBox;
        const bool tableBox = prop.role == PropVisualRole::TableBox;
        const bool canister = prop.role == PropVisualRole::Canister;
        const bool barricade = prop.role == PropVisualRole::Barricade;
        const bool supplyCrate = prop.role == PropVisualRole::Crate;
        struct CratePalette {
            wkc::core::Vec3 shell;
            wkc::core::Vec3 frame;
            wkc::core::Vec3 accent;
        };
        CratePalette cratePalette{{0.30F, 0.31F, 0.30F},
            {0.13F, 0.15F, 0.16F}, {0.83F, 0.40F, 0.11F}};
        switch (prop.theme) {
        case world::RoomTheme::Storage:
            cratePalette = {{0.35F, 0.39F, 0.30F},
                {0.19F, 0.24F, 0.20F}, {0.69F, 0.54F, 0.26F}};
            break;
        case world::RoomTheme::Cafeteria:
            cratePalette = {{0.55F, 0.57F, 0.52F},
                {0.25F, 0.30F, 0.29F}, {0.37F, 0.63F, 0.51F}};
            break;
        case world::RoomTheme::Security:
            cratePalette = {{0.31F, 0.39F, 0.43F},
                {0.13F, 0.19F, 0.23F}, {0.26F, 0.67F, 0.75F}};
            break;
        case world::RoomTheme::Laboratory:
            cratePalette = {{0.60F, 0.64F, 0.62F},
                {0.22F, 0.31F, 0.36F}, {0.26F, 0.69F, 0.65F}};
            break;
        case world::RoomTheme::Maintenance:
            cratePalette = {{0.34F, 0.37F, 0.32F},
                {0.15F, 0.18F, 0.16F}, {0.77F, 0.58F, 0.22F}};
            break;
        case world::RoomTheme::Office:
            cratePalette = {{0.48F, 0.46F, 0.40F},
                {0.25F, 0.25F, 0.23F}, {0.69F, 0.48F, 0.31F}};
            break;
        case world::RoomTheme::Industrial:
            cratePalette = {{0.36F, 0.34F, 0.30F},
                {0.15F, 0.15F, 0.14F}, {0.76F, 0.42F, 0.14F}};
            break;
        }
        if (prop.bossArena) {
            cratePalette = {{0.43F, 0.41F, 0.37F},
                {0.12F, 0.13F, 0.13F}, {0.88F, 0.45F, 0.10F}};
        }
        wkc::core::Vec3 tint{0.54F, 0.34F, 0.17F};
        if (prop.role == PropVisualRole::Canister) {
            tint = {0.68F, 0.18F, 0.10F};
        } else if (prop.role == PropVisualRole::Barricade) {
            tint = prop.theme == world::RoomTheme::Industrial
                ? wkc::core::Vec3{0.30F, 0.29F, 0.26F}
                : wkc::core::Vec3{0.24F, 0.34F, 0.40F};
        } else if (storageBox) {
            tint = {1.0F, 1.0F, 1.0F};
        } else if (supplyCrate || tableBox) {
            tint = cratePalette.shell;
        }
        const float seedVariation = storageBox ? 0.0F
            : static_cast<float>((prop.visualSeed >> 8U) & 0xFFU)
                / 255.0F * 0.12F - 0.06F;
        tint.x = std::clamp(tint.x + seedVariation, 0.08F, 1.0F);
        tint.y = std::clamp(tint.y + seedVariation, 0.08F, 1.0F);
        tint.z = std::clamp(tint.z + seedVariation, 0.08F, 1.0F);
        wkc::core::Vec3 scale = prop.scale;
        if (prop.state == PropVisualState::Damaged) {
            tint = tint * 0.62F;
        } else if (prop.state == PropVisualState::Destroyed) {
            tint = tint * 0.30F;
            scale.y = std::min(scale.y, 0.12F);
            scale.x *= 1.12F;
            scale.z *= 1.12F;
        }
        // The test arena also reuses Canister for wide target plates. Keep
        // their rectangular geometry; only ordinary square-footprint props
        // become the faceted vessel.
        const bool canisterVessel = canister
            && prop.state != PropVisualState::Destroyed
            && std::abs(prop.scale.x - prop.scale.z)
                <= std::min(prop.scale.x, prop.scale.z) * 0.35F;
        const float coverLongAxis = std::max(prop.scale.x, prop.scale.z);
        const float coverShortAxis = std::min(prop.scale.x, prop.scale.z);
        // Large test-arena walls and square targets reuse Barricade. Preserve
        // those shapes while styling the narrow loose cover and boss panels.
        const bool barricadePanel = barricade
            && coverLongAxis >= coverShortAxis * 1.8F
            && coverLongAxis < 4.1F;
        const wkc::core::Mat4 model = wkc::core::translationMatrix(
            prop.position + wkc::core::Vec3{0.0F, scale.y * 0.5F, 0.0F})
            * wkc::core::scaleMatrix(scale);
        shader.setMat4(gl_, "uModel", model);
        if (geometryShadowsActive) {
            shadowCasters.push_back({
                canisterVessel ? &canisterCylinder_ : &box_, model});
        }
        if (canisterVessel) {
            solidColorTexture_.bind(gl_, textureBindings_);
            shader.setInt(gl_, "uDetailedMaterial", 0);
            shader.setVec2(gl_, "uUvScale", {1.0F, 1.0F});
            shader.setVec2(gl_, "uUvOffset", {});
            const float wear = prop.state == PropVisualState::Damaged
                ? 0.72F : 1.0F;
            const float variant = 0.94F
                + static_cast<float>((prop.visualSeed >> 9U) & 7U) * 0.012F;
            const auto canisterPart = [&](const wkc::core::Vec3 center,
                const wkc::core::Vec3 dimensions,
                const wkc::core::Vec3 color,
                const bool round = false) {
                shader.setMat4(gl_, "uModel", model
                    * wkc::core::translationMatrix(center)
                    * wkc::core::scaleMatrix(dimensions));
                shader.setVec3(gl_, "uTint", color * (wear * variant));
                shader.setFloat(gl_, "uEmissiveStrength", 0.0F);
                drawMesh(round ? canisterCylinder_ : box_);
            };
            // The pressure vessel, raised bands and fittings remain inside
            // the original 0.46 x 1.02 x 0.46 collision AABB.
            canisterPart({0.0F, -0.02F, 0.0F},
                {0.84F, 0.76F, 0.84F}, {0.69F, 0.15F, 0.105F}, true);
            canisterPart({0.0F, 0.345F, 0.0F},
                {0.76F, 0.125F, 0.76F}, {0.57F, 0.12F, 0.09F}, true);
            for (const float y : {-0.445F, -0.25F, 0.24F, 0.405F}) {
                const bool endBand = std::abs(y) > 0.40F;
                canisterPart({0.0F, y, 0.0F},
                    {endBand ? 0.94F : 0.91F,
                        endBand ? 0.09F : 0.055F,
                        endBand ? 0.94F : 0.91F},
                    endBand ? wkc::core::Vec3{0.18F, 0.20F, 0.21F}
                        : wkc::core::Vec3{0.27F, 0.28F, 0.27F}, true);
            }
            canisterPart({0.0F, 0.463F, 0.0F},
                {0.72F, 0.046F, 0.72F}, {0.31F, 0.33F, 0.32F}, true);
            for (const float side : {-1.0F, 1.0F}) {
                canisterPart({side * 0.16F, -0.02F, 0.405F},
                    {0.040F, 0.58F, 0.074F},
                    {0.38F, 0.09F, 0.075F});
                canisterPart({side * 0.25F, 0.466F, 0.0F},
                    {0.045F, 0.052F, 0.12F},
                    {0.24F, 0.26F, 0.26F});
            }
            canisterPart({0.0F, 0.488F, 0.0F},
                {0.54F, 0.020F, 0.10F}, {0.20F, 0.22F, 0.23F});
            canisterPart({0.0F, -0.02F, 0.438F},
                {0.27F, 0.22F, 0.075F}, {0.13F, 0.16F, 0.17F});
            canisterPart({0.0F, -0.02F, 0.481F},
                {0.21F, 0.15F, 0.024F}, {0.66F, 0.67F, 0.61F});
            canisterPart({0.0F, -0.02F, 0.495F},
                {0.15F, 0.029F, 0.008F}, cratePalette.accent);
            if (prop.state == PropVisualState::Damaged) {
                canisterPart({0.07F, -0.11F, 0.476F},
                    {0.08F, 0.025F, 0.025F}, {0.07F, 0.065F, 0.06F});
            }
            fallbackTexture_.bind(gl_, textureBindings_);
            continue;
        }
        if (barricadePanel) {
            solidColorTexture_.bind(gl_, textureBindings_);
            shader.setInt(gl_, "uDetailedMaterial", 0);
            shader.setVec2(gl_, "uUvScale", {1.0F, 1.0F});
            shader.setVec2(gl_, "uUvOffset", {});
            const bool alongZ = prop.scale.z > prop.scale.x;
            const float wear = prop.state == PropVisualState::Damaged
                ? 0.70F : prop.state == PropVisualState::Destroyed
                    ? 0.36F : 1.0F;
            const float variant = 0.94F
                + static_cast<float>((prop.visualSeed >> 8U) & 7U) * 0.012F;
            // Coordinates are width, height, depth. Swap the axes when a room
            // rotates the cover 90 degrees so its detailing follows the wall.
            const auto coverPart = [&](const wkc::core::Vec3 center,
                const wkc::core::Vec3 dimensions,
                const wkc::core::Vec3 color) {
                const wkc::core::Vec3 localCenter = alongZ
                    ? wkc::core::Vec3{center.z, center.y, center.x} : center;
                const wkc::core::Vec3 localDimensions = alongZ
                    ? wkc::core::Vec3{dimensions.z, dimensions.y, dimensions.x}
                    : dimensions;
                shader.setMat4(gl_, "uModel", model
                    * wkc::core::translationMatrix(localCenter)
                    * wkc::core::scaleMatrix(localDimensions));
                shader.setVec3(gl_, "uTint", color * (wear * variant));
                shader.setFloat(gl_, "uEmissiveStrength", 0.0F);
                drawMesh(box_);
            };
            const auto bossHazardStripe = [&](const float horizontal,
                const float face, const wkc::core::Vec3 color) {
                const wkc::core::Vec3 center{horizontal, -0.14F,
                    face * 0.474F};
                const wkc::core::Vec3 localCenter = alongZ
                    ? wkc::core::Vec3{center.z, center.y, center.x} : center;
                const wkc::core::Vec3 dimensions = alongZ
                    ? wkc::core::Vec3{0.035F, 0.26F, 0.072F}
                    : wkc::core::Vec3{0.072F, 0.26F, 0.035F};
                shader.setMat4(gl_, "uModel", model
                    * wkc::core::translationMatrix(localCenter)
                    * (alongZ ? wkc::core::rotationXMatrix(-0.37F)
                        : wkc::core::rotationZMatrix(-0.37F))
                    * wkc::core::scaleMatrix(dimensions));
                shader.setVec3(gl_, "uTint", color * (wear * variant));
                shader.setFloat(gl_, "uEmissiveStrength", 0.0F);
                drawMesh(box_);
            };
            const wkc::core::Vec3 coverShell = prop.bossArena
                ? wkc::core::Vec3{0.48F, 0.46F, 0.40F}
                : cratePalette.shell * 0.88F;
            const wkc::core::Vec3 coverFrame = prop.bossArena
                ? wkc::core::Vec3{0.12F, 0.13F, 0.13F}
                : cratePalette.frame;
            const wkc::core::Vec3 coverAccent = prop.bossArena
                ? wkc::core::Vec3{0.88F, 0.45F, 0.10F}
                : cratePalette.accent;
            if (prop.state == PropVisualState::Destroyed) {
                coverPart({}, {0.92F, 0.68F, 0.80F}, coverShell);
                coverPart({0.0F, 0.24F, 0.0F},
                    {0.86F, 0.12F, 0.88F}, coverFrame);
            } else {
                // Solid panels match the existing full AABB cover collision.
                // Recesses, braces and bolt heads add depth without creating
                // visual holes that bullets cannot pass through.
                coverPart({}, {0.80F, 0.80F, 0.76F}, coverFrame * 0.72F);
                for (const float face : {-1.0F, 1.0F}) {
                    coverPart({0.0F, 0.0F, face * 0.410F},
                        {0.76F, 0.72F, 0.060F}, coverShell);
                    for (const float side : {-1.0F, 1.0F}) {
                        coverPart({side * 0.445F, 0.0F, face * 0.420F},
                            {0.09F, 0.86F, 0.12F}, coverFrame);
                        for (const float y : {-0.32F, 0.32F}) {
                            coverPart({side * 0.33F, y, face * 0.472F},
                                {0.045F, 0.045F, 0.025F},
                                {0.67F, 0.68F, 0.64F});
                        }
                    }
                    for (const float y : {-0.365F, 0.365F}) {
                        coverPart({0.0F, y, face * 0.445F},
                            {0.90F, 0.075F, 0.085F}, coverFrame);
                    }
                    coverPart({0.0F, 0.015F, face * 0.452F},
                        {0.62F, 0.055F, 0.078F},
                        coverAccent * 0.70F);
                }
                coverPart({0.0F, 0.445F, 0.0F},
                    {0.95F, 0.085F, 0.94F}, coverFrame);
                coverPart({0.0F, -0.445F, 0.0F},
                    {0.95F, 0.085F, 0.94F}, coverFrame);
                coverPart({0.0F, 0.493F, 0.0F},
                    {0.76F, 0.012F, 0.72F}, coverShell * 0.82F);
                switch (prop.theme) {
                case world::RoomTheme::Storage:
                    // Cargo-barrier slats echo the storage rack finish.
                    for (const float face : {-1.0F, 1.0F}) {
                        for (const float y : {-0.20F, 0.20F}) {
                            coverPart({0.0F, y, face * 0.466F},
                                {0.58F, 0.07F, 0.060F},
                                cratePalette.accent * 0.70F);
                        }
                    }
                    break;
                case world::RoomTheme::Cafeteria:
                    for (const float face : {-1.0F, 1.0F}) {
                        coverPart({0.0F, -0.20F, face * 0.467F},
                            {0.58F, 0.10F, 0.058F},
                            cratePalette.shell * 0.58F);
                    }
                    break;
                case world::RoomTheme::Security:
                    for (const float face : {-1.0F, 1.0F}) {
                        coverPart({0.0F, -0.18F, face * 0.470F},
                            {0.56F, 0.055F, 0.052F},
                            cratePalette.accent);
                    }
                    break;
                case world::RoomTheme::Laboratory:
                    for (const float face : {-1.0F, 1.0F}) {
                        for (const float side : {-1.0F, 1.0F}) {
                            coverPart({side * 0.25F, -0.15F, face * 0.466F},
                                {0.025F, 0.30F, 0.058F},
                                cratePalette.accent * 0.80F);
                        }
                    }
                    break;
                case world::RoomTheme::Maintenance:
                    for (const float face : {-1.0F, 1.0F}) {
                        if (prop.bossArena) {
                            for (const float horizontal :
                                    {-0.24F, -0.08F, 0.08F, 0.24F}) {
                                bossHazardStripe(horizontal, face,
                                    coverAccent);
                            }
                            coverPart({0.0F, 0.23F, face * 0.469F},
                                {0.55F, 0.035F, 0.046F}, coverFrame);
                        } else {
                            coverPart({0.0F, -0.25F, face * 0.469F},
                                {0.58F, 0.07F, 0.055F}, coverAccent);
                        }
                    }
                    break;
                case world::RoomTheme::Office:
                    for (const float face : {-1.0F, 1.0F}) {
                        for (const float y : {-0.21F, -0.28F}) {
                            coverPart({0.0F, y, face * 0.468F},
                                {0.54F, 0.020F, 0.055F}, coverFrame);
                        }
                    }
                    break;
                case world::RoomTheme::Industrial:
                    for (const float face : {-1.0F, 1.0F}) {
                        for (const float side : {-1.0F, 1.0F}) {
                            coverPart({side * 0.26F, -0.17F,
                                    face * 0.469F},
                                {0.042F, 0.28F, 0.055F},
                                cratePalette.accent);
                        }
                    }
                    break;
                }
            }
            fallbackTexture_.bind(gl_, textureBindings_);
            continue;
        }
        if (tableBox && prop.state != PropVisualState::Destroyed) {
            solidColorTexture_.bind(gl_, textureBindings_);
            shader.setInt(gl_, "uDetailedMaterial", 0);
            shader.setVec2(gl_, "uUvScale", {1.0F, 1.0F});
            shader.setVec2(gl_, "uUvOffset", {});
            const float wear = prop.state == PropVisualState::Damaged
                ? 0.68F : 1.0F;
            const float variant = 0.94F
                + static_cast<float>((prop.visualSeed >> 9U) & 7U) * 0.012F;
            const auto tablePart = [&](const wkc::core::Vec3 center,
                const wkc::core::Vec3 dimensions,
                const wkc::core::Vec3 color) {
                shader.setMat4(gl_, "uModel", model
                    * wkc::core::translationMatrix(center)
                    * wkc::core::scaleMatrix(dimensions));
                shader.setVec3(gl_, "uTint", color * (wear * variant));
                shader.setFloat(gl_, "uEmissiveStrength", 0.0F);
                drawMesh(box_);
            };
            // Compact bench case, built from the same room palette as the
            // supply crates. Every raised piece fits its original hitbox.
            tablePart({}, {0.84F, 0.82F, 0.84F},
                cratePalette.frame * 0.76F);
            for (const float face : {-1.0F, 1.0F}) {
                tablePart({0.0F, 0.0F, face * 0.431F},
                    {0.76F, 0.70F, 0.045F}, cratePalette.shell);
                tablePart({face * 0.431F, 0.0F, 0.0F},
                    {0.045F, 0.70F, 0.76F}, cratePalette.shell * 0.85F);
                for (const float side : {-1.0F, 1.0F}) {
                    tablePart({side * 0.43F, 0.0F, face * 0.43F},
                        {0.10F, 0.89F, 0.10F}, cratePalette.frame);
                }
                for (const float y : {-0.34F, 0.34F}) {
                    tablePart({0.0F, y, face * 0.469F},
                        {0.84F, 0.07F, 0.05F}, cratePalette.frame);
                    tablePart({face * 0.469F, y, 0.0F},
                        {0.05F, 0.07F, 0.84F}, cratePalette.frame);
                }
            }
            tablePart({0.0F, 0.445F, 0.0F},
                {0.94F, 0.08F, 0.94F}, cratePalette.frame);
            tablePart({0.0F, 0.490F, 0.0F},
                {0.76F, 0.014F, 0.76F}, cratePalette.shell * 0.82F);
            tablePart({0.0F, -0.445F, 0.0F},
                {0.92F, 0.07F, 0.92F}, cratePalette.frame);
            for (const float side : {-1.0F, 1.0F}) {
                tablePart({side * 0.23F, 0.0F, 0.472F},
                    {0.045F, 0.58F, 0.035F}, cratePalette.frame);
                tablePart({side * 0.27F, 0.26F, 0.490F},
                    {0.055F, 0.055F, 0.018F},
                    {0.69F, 0.69F, 0.64F});
            }
            tablePart({0.0F, 0.12F, 0.466F},
                {0.34F, 0.13F, 0.025F}, cratePalette.frame * 0.62F);
            tablePart({0.0F, 0.12F, 0.487F},
                {0.22F, 0.035F, 0.018F}, cratePalette.accent);
            // Alternating top ribs read clearly at bench height without
            // repeating the large crate's full-size cargo markings.
            for (const float x : {-0.21F, 0.21F}) {
                tablePart({x, 0.492F, 0.0F},
                    {0.030F, 0.008F, 0.51F}, cratePalette.frame * 0.70F);
            }
            fallbackTexture_.bind(gl_, textureBindings_);
            continue;
        }
        if (supplyCrate && prop.state != PropVisualState::Destroyed) {
            solidColorTexture_.bind(gl_, textureBindings_);
            shader.setInt(gl_, "uDetailedMaterial", 0);
            shader.setVec2(gl_, "uUvScale", {1.0F, 1.0F});
            shader.setVec2(gl_, "uUvOffset", {});
            const float wear = prop.state == PropVisualState::Damaged
                ? 0.72F : 1.0F;
            const float variant = 0.93F
                + static_cast<float>((prop.visualSeed >> 9U) & 7U) * 0.014F;
            const auto cratePart = [&](const wkc::core::Vec3 center,
                const wkc::core::Vec3 dimensions,
                const wkc::core::Vec3 color,
                const float emission = 0.0F) {
                shader.setMat4(gl_, "uModel", model
                    * wkc::core::translationMatrix(center)
                    * wkc::core::scaleMatrix(dimensions));
                shader.setVec3(gl_, "uTint", color * (wear * variant));
                shader.setFloat(gl_, "uEmissiveStrength", emission);
                drawMesh(box_);
            };
            // Recess the casing beneath the load-bearing frame. The different
            // face depths give the crate a visible silhouette from oblique
            // angles without growing beyond its existing collision AABB.
            cratePart({}, {0.80F, 0.80F, 0.80F},
                cratePalette.frame * 0.74F);
            cratePart({0.0F, 0.0F, 0.414F},
                {0.70F, 0.70F, 0.030F}, cratePalette.frame * 0.58F);
            cratePart({0.0F, 0.0F, 0.442F},
                {0.70F, 0.63F, 0.040F}, cratePalette.shell);
            for (const float side : {-1.0F, 1.0F}) {
                cratePart({side * 0.414F, 0.0F, 0.0F},
                    {0.030F, 0.70F, 0.70F}, cratePalette.frame * 0.58F);
                cratePart({side * 0.442F, 0.0F, 0.0F},
                    {0.040F, 0.63F, 0.63F}, cratePalette.shell * 0.83F);
            }
            // All raised pieces stay within the existing collision AABB.
            for (const float x : {-0.43F, 0.43F}) {
                for (const float z : {-0.43F, 0.43F}) {
                    cratePart({x, 0.0F, z}, {0.10F, 0.88F, 0.10F},
                        cratePalette.frame);
                }
            }
            // Exposed horizontal ribs bridge the corner uprights. The narrow
            // gap behind them remains dark even under a strong room light.
            for (const float y : {-0.35F, 0.35F}) {
                cratePart({0.0F, y, 0.467F},
                    {0.80F, 0.075F, 0.052F}, cratePalette.frame);
                for (const float side : {-1.0F, 1.0F}) {
                    cratePart({side * 0.467F, y, 0.0F},
                        {0.052F, 0.075F, 0.80F}, cratePalette.frame);
                }
            }
            cratePart({0.0F, 0.44F, 0.0F}, {0.96F, 0.08F, 0.96F},
                cratePalette.frame);
            cratePart({0.0F, -0.44F, 0.0F}, {0.92F, 0.07F, 0.92F},
                cratePalette.frame);
            cratePart({0.0F, 0.489F, 0.0F},
                {0.74F, 0.018F, 0.74F}, cratePalette.shell * 0.82F);
            for (const float side : {-1.0F, 1.0F}) {
                // Cast handles: mounting feet, a clear opening, and a bar.
                for (const float y : {-0.095F, 0.095F}) {
                    cratePart({side * 0.468F, y, 0.0F},
                        {0.044F, 0.055F, 0.31F}, cratePalette.frame);
                }
                cratePart({side * 0.489F, 0.0F, 0.0F},
                    {0.018F, 0.032F, 0.31F}, cratePalette.accent * 0.75F);
            }
            for (const float side : {-1.0F, 1.0F}) {
                cratePart({side * 0.23F, 0.0F, 0.465F},
                    {0.050F, 0.62F, 0.052F}, cratePalette.frame);
                cratePart({side * 0.29F, 0.28F, 0.481F},
                    {0.065F, 0.065F, 0.028F},
                    {0.69F, 0.69F, 0.64F});
            }
            // Physical clasp: recessed socket, two projecting hinge blocks,
            // and a coloured release bar at the outermost safe depth.
            cratePart({0.0F, 0.14F, 0.452F},
                {0.48F, 0.17F, 0.024F}, cratePalette.frame * 0.61F);
            for (const float side : {-1.0F, 1.0F}) {
                cratePart({side * 0.19F, 0.14F, 0.472F},
                    {0.070F, 0.17F, 0.055F}, cratePalette.frame);
            }
            cratePart({0.0F, 0.14F, 0.489F},
                {0.34F, 0.052F, 0.018F}, cratePalette.accent, 0.12F);
            const wkc::core::Vec3 contentsColor
                = prop.crateDrop == PickupVisualRole::Health
                ? wkc::core::Vec3{0.52F, 0.83F, 0.59F}
                : wkc::core::Vec3{0.87F, 0.69F, 0.36F};
            if (prop.crateDrop) {
                // A small mark labels the content while the casing remains
                // tied to its room, not to the pickup color.
                cratePart({0.0F, -0.14F, 0.465F},
                    {0.31F, 0.26F, 0.045F}, cratePalette.frame * 0.61F);
                cratePart({0.0F, -0.14F, 0.483F},
                    {0.27F, 0.22F, 0.012F}, cratePalette.frame);
                cratePart({0.0F, -0.14F, 0.491F},
                    {0.18F, 0.040F, 0.010F}, contentsColor, 0.16F);
                if (prop.crateDrop == PickupVisualRole::Health) {
                    cratePart({0.0F, -0.14F, 0.492F},
                        {0.045F, 0.16F, 0.010F}, contentsColor, 0.16F);
                }
            }
            switch (prop.theme) {
            case world::RoomTheme::Storage:
                // Cargo straps match the olive storage racks and boxes.
                for (const float side : {-1.0F, 1.0F}) {
                    cratePart({side * 0.19F, 0.0F, 0.469F},
                        {0.038F, 0.72F, 0.050F}, cratePalette.accent);
                    cratePart({side * 0.19F, 0.490F, 0.0F},
                        {0.038F, 0.010F, 0.72F}, cratePalette.accent);
                }
                break;
            case world::RoomTheme::Cafeteria:
                // A wipe-clean supply carrier with a vented lid.
                for (const float x : {-0.26F, -0.13F, 0.13F, 0.26F}) {
                    cratePart({x, 0.490F, 0.0F},
                        {0.045F, 0.010F, 0.57F}, cratePalette.shell * 0.57F);
                }
                cratePart({0.0F, -0.31F, 0.471F},
                    {0.48F, 0.055F, 0.050F}, cratePalette.accent);
                break;
            case world::RoomTheme::Security:
                // Reinforced vertical spine echoes the security consoles.
                cratePart({0.0F, 0.0F, 0.471F},
                    {0.072F, 0.72F, 0.050F}, cratePalette.frame);
                for (const float side : {-1.0F, 1.0F}) {
                    cratePart({side * 0.34F, 0.0F, 0.471F},
                        {0.055F, 0.74F, 0.050F}, cratePalette.accent * 0.70F);
                }
                break;
            case world::RoomTheme::Laboratory:
                // Sealed sample case with paired containment indicators.
                for (const float side : {-1.0F, 1.0F}) {
                    cratePart({side * 0.35F, 0.0F, 0.471F},
                        {0.025F, 0.59F, 0.050F}, cratePalette.accent, 0.10F);
                }
                cratePart({0.0F, 0.490F, 0.0F},
                    {0.48F, 0.010F, 0.22F}, cratePalette.shell * 0.77F);
                break;
            case world::RoomTheme::Maintenance:
                // Service case with warning bars and a removable top plate.
                for (const float y : {-0.31F, -0.24F}) {
                    cratePart({0.0F, y, 0.471F},
                        {0.48F, 0.025F, 0.050F}, cratePalette.accent);
                }
                cratePart({0.0F, 0.490F, 0.0F},
                    {0.44F, 0.010F, 0.44F}, cratePalette.shell * 0.70F);
                break;
            case world::RoomTheme::Office:
                // A compact document chest, with two drawer seams.
                for (const float y : {-0.29F, -0.36F}) {
                    cratePart({0.0F, y, 0.471F},
                        {0.50F, 0.014F, 0.050F}, cratePalette.frame);
                }
                cratePart({0.0F, -0.325F, 0.494F},
                    {0.13F, 0.025F, 0.008F}, cratePalette.accent);
                break;
            case world::RoomTheme::Industrial:
                // Orange corner rails tie the case to industrial routing.
                for (const float side : {-1.0F, 1.0F}) {
                    cratePart({side * 0.35F, 0.0F, 0.471F},
                        {0.055F, 0.69F, 0.050F}, cratePalette.accent);
                }
                break;
            }
            if (prop.state == PropVisualState::Damaged) {
                cratePart({0.12F, -0.035F, 0.485F},
                    {0.28F, 0.025F, 0.014F},
                    {0.07F, 0.07F, 0.065F});
            }
            shader.setFloat(gl_, "uEmissiveStrength", 0.0F);
            fallbackTexture_.bind(gl_, textureBindings_);
            continue;
        }
        shader.setVec3(gl_, "uTint", tint);
        shader.setInt(gl_, "uDetailedMaterial", 0);
        shader.setVec2(gl_, "uUvScale", storageBox
            ? wkc::core::Vec2{1.0F, 1.0F}
            : wkc::core::Vec2{3.0F, 3.0F});
        shader.setVec2(gl_, "uUvOffset", {});
        shader.setFloat(gl_, "uEmissiveStrength", 0.0F);
        const bool solidLooseProp = barricade
            || (prop.state == PropVisualState::Destroyed
                && (supplyCrate || canister));
        if (storageBox) {
            storageBoxTextures_[(prop.visualSeed & 1ULL) != 0U ? 1U : 0U]
                .bind(gl_, textureBindings_);
        } else if (solidLooseProp) {
            solidColorTexture_.bind(gl_, textureBindings_);
        }
        drawMesh(box_);
        if (storageBox || solidLooseProp) {
            fallbackTexture_.bind(gl_, textureBindings_);
        }
    }
    shader.setFloat(gl_, "uPropIndirectFill", 0.0F);

    // Draw glass after every opaque planter, including the damageable ones.
    if (!glassTestDomes.empty()) {
        solidColorTexture_.bind(gl_, textureBindings_);
        shader.setInt(gl_, "uDetailedMaterial", 0);
        shader.setInt(gl_, "uInstanced", 0);
        shader.setVec2(gl_, "uUvScale", {1.0F, 1.0F});
        shader.setVec2(gl_, "uUvOffset", {});
        shader.setFloat(gl_, "uSurfaceTileMetres", 0.0F);
        shader.setInt(gl_, "uMirrorSurfaceTile", 0);
        shader.setInt(gl_, "uRackPart", 0);
        shader.setInt(gl_, "uFurniturePart", 0);
        shader.setInt(gl_, "uStructuralSteelMaterial", 0);
        shader.setInt(gl_, "uHubWallMaterial", 0);
        shader.setInt(gl_, "uCombatColumnMaterial", 0);
        shader.setInt(gl_, "uCombatRailMaterial", 0);
        shader.setInt(gl_, "uHubFloorMaterial", 0);
        shader.setInt(gl_, "uBossFloorMaterial", 0);
        shader.setInt(gl_, "uTechnicalSteelMaterial", 0);
        shader.setInt(gl_, "uSecurityFloorVariation", 0);
        shader.setFloat(gl_, "uEmissiveStrength", 0.10F);
        shader.setFloat(gl_, "uOpacity", 0.19F);
        shader.setVec3(gl_, "uTint", {0.49F, 0.80F, 0.88F});
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        for (const auto& dome : glassTestDomes) {
            shader.setMat4(gl_, "uModel",
                wkc::core::translationMatrix(dome.center)
                    * wkc::core::scaleMatrix(
                        {dome.radius, dome.height, dome.radius}));
            drawMesh(glassTestDome_);
        }
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        shader.setFloat(gl_, "uOpacity", 1.0F);
        shader.setFloat(gl_, "uEmissiveStrength", 0.0F);
        fallbackTexture_.bind(gl_, textureBindings_);
    }

    std::array<std::uint32_t, maxRenderedRoomLights> shadowLightIndices{};
    std::uint32_t selectedShadowLightCount = 0U;
    if (geometryShadowsActive && lightCount > 0U) {
        std::array<std::pair<float, std::uint32_t>, maxRenderedRoomLights>
            candidates{};
        std::uint32_t candidateCount = 0U;
        for (std::uint32_t index = 0U; index < lightCount; ++index) {
            if (lighting.lights[index].intensity <= 0.05F) continue;
            const wkc::core::Vec3 delta = lighting.lights[index].position
                - camera.position;
            candidates[candidateCount++] = {wkc::core::dot(delta, delta), index};
        }
        for (std::uint32_t index = 1U; index < candidateCount; ++index) {
            const auto candidate = candidates[index];
            std::uint32_t slot = index;
            while (slot > 0U && candidate < candidates[slot - 1U]) {
                candidates[slot] = candidates[slot - 1U];
                --slot;
            }
            candidates[slot] = candidate;
        }
        const std::uint32_t budget = std::min<std::uint32_t>(
            hud.geometryShadowLightBudget,
            static_cast<std::uint32_t>(maxRenderedRoomLights));
        selectedShadowLightCount = std::min(candidateCount, budget);
        for (std::uint32_t slot = 0U; slot < selectedShadowLightCount; ++slot) {
            shadowLightIndices[slot] = candidates[slot].second;
        }
        if (selectedShadowLightCount == 1U && shadowCubeCount_ > 0U) {
            // Retain the existing map owner while it still lights the camera;
            // MEDIUM should not switch shadows on every small movement.
            for (std::uint32_t index = 0U; index < lightCount; ++index) {
                const auto& previous = lighting.lights[index];
                if (previous.intensity > 0.05F
                    && wkc::core::length(previous.position
                        - shadowCubeLightPositions_[0]) <= 0.08F
                    && wkc::core::length(previous.position - camera.position)
                        < previous.range * 0.9F) {
                    shadowLightIndices[0] = index;
                    break;
                }
            }
        }
    }
    if (selectedShadowLightCount > 0U) {
        // Reuse the exact matrices of the color pass. The maps are consumed
        // by the following frame, avoiding a second pose evaluation.
        std::vector<wkc::core::Mat4> boxShadowModels;
        std::vector<const ShadowCaster*> meshShadowCasters;
        boxShadowModels.reserve(shadowCasters.size());
        meshShadowCasters.reserve(shadowCasters.size());
        constexpr float farPlane = 22.0F;
        constexpr std::array directions{
            wkc::core::Vec3{1, 0, 0}, wkc::core::Vec3{-1, 0, 0},
            wkc::core::Vec3{0, 1, 0}, wkc::core::Vec3{0, -1, 0},
            wkc::core::Vec3{0, 0, 1}, wkc::core::Vec3{0, 0, -1}};
        constexpr std::array upVectors{
            wkc::core::Vec3{0, -1, 0}, wkc::core::Vec3{0, -1, 0},
            wkc::core::Vec3{0, 0, 1}, wkc::core::Vec3{0, 0, -1},
            wkc::core::Vec3{0, -1, 0}, wkc::core::Vec3{0, -1, 0}};
        shadowDepthShader_.beginUniformFrame();
        shadowDepthShader_.use(gl_);
        shadowDepthShader_.setFloat(gl_, "uFarPlane", farPlane);
        shadowDepthShader_.setInt(gl_, "uInstanced", 0);
        shadowDepthShader_.setMat4(gl_, "uProjection",
            wkc::core::perspectiveMatrix(std::numbers::pi_v<float> * 0.5F,
                1.0F, 0.05F, farPlane));
        glDepthMask(GL_TRUE);
        for (std::uint32_t slot = 0U; slot < selectedShadowLightCount; ++slot) {
            const auto& light = lighting.lights[shadowLightIndices[slot]];
            const wkc::core::Vec3 lightPosition = light.position;
            boxShadowModels.clear();
            meshShadowCasters.clear();
            for (const ShadowCaster& caster : shadowCasters) {
                const auto& e = caster.model.elements;
                const wkc::core::Vec3 center{e[12], e[13], e[14]};
                const float radius = 0.5F * std::sqrt(
                    e[0] * e[0] + e[1] * e[1] + e[2] * e[2]
                    + e[4] * e[4] + e[5] * e[5] + e[6] * e[6]
                    + e[8] * e[8] + e[9] * e[9] + e[10] * e[10]);
                if (caster.mesh == &box_
                    && wkc::core::length(center - lightPosition)
                        > light.range + radius) {
                    continue;
                }
                if (caster.mesh == &box_) {
                    boxShadowModels.push_back(caster.model);
                } else {
                    meshShadowCasters.push_back(&caster);
                }
            }
            if (!boxShadowModels.empty()
                && !box_.uploadInstanceTransforms(gl_, boxShadowModels,
                    meshBindings_)) {
                return false;
            }
            shadowDepthShader_.setVec3(gl_, "uLightPosition", lightPosition);
            glViewport(0, 0, shadowCubes_[slot].size(),
                shadowCubes_[slot].size());
            for (unsigned int face = 0; face < directions.size(); ++face) {
                shadowCubes_[slot].bindFace(gl_, face);
                glClear(GL_DEPTH_BUFFER_BIT);
                shadowDepthShader_.setMat4(gl_, "uView",
                    wkc::core::lookAtMatrix(lightPosition,
                        lightPosition + directions[face], upVectors[face]));
                for (const ShadowCaster* caster : meshShadowCasters) {
                    shadowDepthShader_.setMat4(gl_, "uModel", caster->model);
                    drawMesh(*caster->mesh);
                }
                if (!boxShadowModels.empty()) {
                    shadowDepthShader_.setInt(gl_, "uInstanced", 1);
                    drawMeshInstanced(box_);
                    shadowDepthShader_.setInt(gl_, "uInstanced", 0);
                }
            }
            shadowCubeLightPositions_[slot] = lightPosition;
        }
        framebuffer_.bind(gl_);
        glViewport(0, 0, renderWidth, renderHeight);
        shader.use(gl_);
        shadowCubeCount_ = selectedShadowLightCount;
        lastFrameStats_.geometryShadowMaps = selectedShadowLightCount;
    } else {
        shadowCubeCount_ = 0U;
    }

    if (playerBody.visible && !hud.mainMenu
        && hud.objective != ObjectiveRenderState::Victory
        && hud.objective != ObjectiveRenderState::Defeat) {
        const auto pose = samplePlayerBodyPose(playerBody);
        if (playerRigPreview_ && playerRigAsset_) {
            const auto palette = samplePlayerRigPalette(
                *playerRigAsset_,playerBody,weaponViewmodel);
            constexpr std::array<const char*,playerRigBoneCount> boneUniforms{
                "uPlayerBones[0]", "uPlayerBones[1]", "uPlayerBones[2]",
                "uPlayerBones[3]", "uPlayerBones[4]", "uPlayerBones[5]",
                "uPlayerBones[6]", "uPlayerBones[7]", "uPlayerBones[8]",
                "uPlayerBones[9]", "uPlayerBones[10]", "uPlayerBones[11]",
                "uPlayerBones[12]", "uPlayerBones[13]", "uPlayerBones[14]",
                "uPlayerBones[15]", "uPlayerBones[16]", "uPlayerBones[17]",
                "uPlayerBones[18]"};
            shader.setInt(gl_,"uPlayerSkinning",1);
            shader.setMat4(gl_,"uModel",pose.root
                * wkc::core::scaleMatrix({1.0F,0.90F,1.0F}));
            for (std::size_t i = 0; i < playerRigBoneCount; ++i)
                shader.setMat4(gl_,boneUniforms[i],palette[i]);
            shader.setVec3(gl_,"uTint",{1,1,1});
            shader.setVec2(gl_,"uUvScale",{1,1});
            shader.setVec2(gl_,"uUvOffset",{});
            shader.setInt(gl_,"uUseEmissionMap",0);
            shader.setInt(gl_,"uDetailedMaterial",0);
            shader.setFloat(gl_,"uEmissiveStrength",0.0F);
            for (std::size_t material = 0; material < playerRigAsset_->materialCount; ++material) {
                shader.setFloat(gl_,"uEmissiveStrength",material == 7U ? 1.6F : 0.0F);
                playerRigAlbedo_[material].bind(gl_,textureBindings_,0U);
                drawMesh(playerRigMeshes_[playerRigAsset_->materialCount+material]);
            }
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
            shader.setInt(gl_,"uPlayerBodyFade",1);
            for (std::size_t material = 0; material < playerRigAsset_->materialCount; ++material) {
                shader.setFloat(gl_,"uEmissiveStrength",material == 7U ? 1.6F : 0.0F);
                playerRigAlbedo_[material].bind(gl_,textureBindings_,0U);
                drawMesh(playerRigMeshes_[material]);
            }
            shader.setFloat(gl_,"uEmissiveStrength",0.0F);
            shader.setInt(gl_,"uPlayerBodyFade",0);
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
            shader.setInt(gl_,"uPlayerSkinning",0);
        } else {
        constexpr std::array<const char*, playerBodyBoneCount> boneUniforms{
            "uPlayerBones[0]", "uPlayerBones[1]", "uPlayerBones[2]", "uPlayerBones[3]",
            "uPlayerBones[4]", "uPlayerBones[5]", "uPlayerBones[6]"};
        shader.setInt(gl_, "uPlayerSkinning", 1);
        shader.setMat4(gl_, "uModel", pose.root);
        for (std::size_t i = 0; i < playerBodyBoneCount; ++i)
            shader.setMat4(gl_, boneUniforms[i], pose.skin[i]);
        shader.setVec3(gl_, "uTint", {1,1,1});
        shader.setVec2(gl_, "uUvScale", {1,1});
        shader.setVec2(gl_, "uUvOffset", {});
        shader.setInt(gl_, "uUseEmissionMap", 0);
        shader.setInt(gl_, "uDetailedMaterial", 1);
        shader.setFloat(gl_, "uEmissiveStrength", 0.0F);
        for (std::size_t material = 0; material < playerBodyMaterialCount; ++material) {
            const auto& gpu = playerBodyMaterials_[material];
            gpu.albedo.bind(gl_, textureBindings_, 0U);
            gpu.normal.bind(gl_, textureBindings_, 1U);
            gpu.properties.bind(gl_, textureBindings_, 2U);
            drawMesh(playerBodyMeshes_[material]);
        }
        shader.setInt(gl_, "uPlayerSkinning", 0);
        }
    }

    // Draw translucent blast volumes after opaque world geometry, so walls
    // and actors still occlude them. A short flash gives way to rising smoke.
    if (!grenadeBlasts.empty()) {
        fallbackTexture_.bind(gl_, textureBindings_);
        shader.setInt(gl_, "uDetailedMaterial", 0);
        shader.setInt(gl_, "uUseEmissionMap", 0);
        shader.setInt(gl_, "uSoftEdges", 1);
        shader.setVec2(gl_, "uUvScale", {1.0F, 1.0F});
        shader.setVec2(gl_, "uUvOffset", {});
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        constexpr std::array<wkc::core::Vec3, 5> puffOffsets{{
            {0.0F, 0.0F, 0.0F}, {-0.85F, 0.12F, 0.18F},
            {0.68F, 0.32F, -0.48F}, {0.42F, -0.08F, 0.82F},
            {-0.33F, 0.28F, -0.82F},
        }};
        for (const GrenadeBlastRenderData& blast : grenadeBlasts) {
            const float remaining = std::clamp(
                std::isfinite(blast.strength) ? blast.strength : 0.0F,
                0.0F, 1.0F);
            const float age = 1.0F - remaining;
            const float flash = std::clamp(1.0F - age / 0.16F, 0.0F, 1.0F);
            if (flash > 0.0F) {
                const float diameter = 0.65F + age * 3.0F;
                shader.setMat4(gl_, "uModel",
                    wkc::core::translationMatrix(blast.position
                        + wkc::core::Vec3{0.0F, 0.14F, 0.0F})
                    * wkc::core::scaleMatrix(
                        {diameter, diameter, diameter}));
                shader.setVec3(gl_, "uTint", {1.0F, 0.57F, 0.18F});
                shader.setFloat(gl_, "uEmissiveStrength", 2.0F);
                shader.setFloat(gl_, "uOpacity", 0.76F * flash);
                shader.setFloat(gl_, "uUnlitMix", 0.92F);
                drawMesh(smokePuff_);
            }
            const float smoke = std::clamp(age / 0.18F, 0.0F, 1.0F)
                * std::clamp((1.0F - age) / 0.22F, 0.0F, 1.0F);
            if (smoke <= 0.0F) continue;
            std::array<std::size_t, puffOffsets.size()> order{
                0U, 1U, 2U, 3U, 4U};
            const auto puffCenter = [&](const std::size_t index) {
                const float spread = 0.26F + age * 0.68F;
                return blast.position + wkc::core::Vec3{
                    puffOffsets[index].x * spread,
                    0.18F + age * 0.82F + puffOffsets[index].y * spread,
                    puffOffsets[index].z * spread};
            };
            std::sort(order.begin(), order.end(), [&](const std::size_t a,
                                                       const std::size_t b) {
                const auto distanceSquared = [&](const std::size_t index) {
                    const wkc::core::Vec3 offset = puffCenter(index) - camera.position;
                    return wkc::core::dot(offset, offset);
                };
                return distanceSquared(a) > distanceSquared(b);
            });
            for (const std::size_t index : order) {
                const float diameter = (index == 0U ? 0.92F : 0.64F)
                    + age * (index == 0U ? 0.75F : 0.58F);
                shader.setMat4(gl_, "uModel",
                    wkc::core::translationMatrix(puffCenter(index))
                    * wkc::core::scaleMatrix({diameter, diameter * 0.82F,
                        diameter}));
                shader.setVec3(gl_, "uTint", index == 0U
                    ? wkc::core::Vec3{0.83F, 0.84F, 0.82F}
                    : wkc::core::Vec3{0.77F, 0.79F, 0.77F});
                shader.setFloat(gl_, "uEmissiveStrength", 0.45F);
                shader.setFloat(gl_, "uOpacity", smoke
                    * (index == 0U ? 0.32F : 0.25F));
                shader.setFloat(gl_, "uUnlitMix", 0.78F);
                drawMesh(smokePuff_);
            }
        }
        shader.setFloat(gl_, "uOpacity", 1.0F);
        shader.setFloat(gl_, "uEmissiveStrength", 0.0F);
        shader.setInt(gl_, "uSoftEdges", 0);
        shader.setFloat(gl_, "uUnlitMix", 0.0F);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    const WeaponEquipPresentation equipPresentation
        = sampleWeaponEquipPresentation(weaponViewmodel);
    const auto& presentedWeapon = equipPresentation.weapon;
    const HudWeaponRole viewmodelRole = presentedWeapon.role;
    if (presentedWeapon.visible && scopeStrength < 0.95F
        && equipPresentation.visible
        && static_cast<std::size_t>(viewmodelRole) < weaponRoleCount) {
        glClear(GL_DEPTH_BUFFER_BIT);
        // The world FOV belongs to the player's camera, not to the weapon.
        // A fixed viewmodel projection keeps its screen footprint stable
        // throughout the supported 60-100 degree settings range.
        constexpr float viewmodelFovDegrees = 68.0F;
        constexpr float degreesToRadians = std::numbers::pi_v<float> / 180.0F;
        shader.setMat4(gl_, "uProjection", wkc::core::perspectiveMatrix(
            viewmodelFovDegrees * degreesToRadians,
            static_cast<float>(renderWidth) / static_cast<float>(renderHeight),
            0.05F, 100.0F));
        shader.setInt(gl_, "uViewmodel", 1);
        shader.setInt(gl_, "uUseEmissionMap", 0);
        shader.setVec2(gl_, "uUvScale", {1.0F, 1.0F});
        shader.setVec2(gl_, "uUvOffset", {});

        const wkc::core::Vec3 forward = wkc::core::normalized(camera.forward);
        wkc::core::Vec3 right = wkc::core::normalized(
            wkc::core::cross(forward, {0.0F, 1.0F, 0.0F}));
        if (wkc::core::length(right) <= 1.0e-7F) {
            right = {1.0F, 0.0F, 0.0F};
        }
        const wkc::core::Vec3 up = wkc::core::normalized(
            wkc::core::cross(right, forward));
        shader.setVec3(gl_, "uViewmodelLightDirection",
            wkc::core::normalized((up * 0.82F) - (right * 0.34F)
                - (forward * 0.44F)));
        const WeaponReloadPose reloadPose = sampleWeaponReloadPose(
            presentedWeapon.role, presentedWeapon.reloadProgress, presentedWeapon.reloadRoundCount);
        const bool carbine = presentedWeapon.role != HudWeaponRole::Pistol
            && presentedWeapon.role != HudWeaponRole::Knife;
        const bool knife = presentedWeapon.role == HudWeaponRole::Knife;
        auto presentationWeapon = presentedWeapon;
        // Modal UI gates scope separately from the raw DTO. The shared hand
        // frame must use that same gate, not retain a hidden zoom-lowered root.
        if (scopeStrength == 0.0F) presentationWeapon.aimStrength = 0.0F;
        // Keep the weapon in the lower-right periphery. Geometry and flash
        // scale together; the camera-space firing ray remains unchanged.
        const wkc::core::Mat4 root
            = wkc::core::translationMatrix(camera.position)
            * basisMatrix(right, up, forward)
            * sampleWeaponPresentationTransform(presentationWeapon);
        const float seedTone = static_cast<float>(
            (presentedWeapon.visualSeed >> 9U) & 0x0fU) / 15.0F * 0.025F;
        const std::array materialColors{
            wkc::core::Vec3{0.45F + seedTone, 0.48F + seedTone, 0.50F + seedTone},
            wkc::core::Vec3{0.64F, 0.67F, 0.68F},
            wkc::core::Vec3{0.23F, 0.24F, 0.24F},
            wkc::core::Vec3{0.075F, 0.085F, 0.09F},
            presentedWeapon.overdrive
                ? wkc::core::Vec3{0.96F, 0.18F, 0.52F}
                : wkc::core::Vec3{0.12F, 0.72F, 0.80F},
                wkc::core::Vec3{1.0F, 1.0F, 1.0F},
        };
        shader.setMat4(gl_, "uModel", root);
        shader.setInt(gl_, "uDetailedMaterial", 1);
        const std::size_t weaponIndex = static_cast<std::size_t>(viewmodelRole);
        // One extra magazine pass exists only while the discarded instance falls.
        for (std::size_t pass = 0;
             !presentedWeapon.grenadeSelected && pass < weaponPartCount + 1U;
             ++pass) {
            const bool discarded = pass == weaponPartCount;
            const std::size_t part = discarded
                ? static_cast<std::size_t>(WeaponPart::Magazine) : pass;
            if (discarded && !reloadPose.discardedMagazineVisible) continue;
            if (part == static_cast<std::size_t>(WeaponPart::Magazine)
                && !discarded && !reloadPose.magazineVisible) continue;
            const bool magazine = part == static_cast<std::size_t>(WeaponPart::Magazine);
            const bool mechanism = part == static_cast<std::size_t>(WeaponPart::Mechanism);
            const wkc::core::Vec3 offset = discarded ? reloadPose.discardedMagazineOffset
                : magazine ? reloadPose.magazineOffset
                : mechanism ? reloadPose.mechanismOffset : wkc::core::Vec3{};
            const wkc::core::Vec3 pivot = mechanism && viewmodelRole == HudWeaponRole::Sniper
                ? wkc::core::Vec3{0.096F, 0.015F, -0.08F} : wkc::core::Vec3{};
            const float rotation = discarded ? reloadPose.discardedMagazineRotationDegrees.z
                : mechanism ? reloadPose.mechanismRotationDegrees.z : 0.0F;
            shader.setMat4(gl_, "uModel", root * wkc::core::translationMatrix(offset)
                * wkc::core::translationMatrix(pivot)
                * wkc::core::rotationZMatrix(rotation * degreesToRadians)
                * wkc::core::rotationXMatrix(discarded
                    ? reloadPose.discardedMagazineRotationDegrees.x * degreesToRadians : 0.0F)
                * wkc::core::translationMatrix(pivot * -1.0F));
            for (std::size_t material = 0; material < weaponMaterialCount; ++material) {
                const wkc::render::Mesh& mesh = weaponPartMeshes_[weaponIndex][part][material];
                if (!mesh.isValid()) {
                    continue;
                }
                const WeaponGpuMaterial& gpuMaterial = weaponMaterials_[material];
                gpuMaterial.albedo.bind(gl_, textureBindings_, 0U);
                gpuMaterial.normal.bind(gl_, textureBindings_, 1U);
                gpuMaterial.properties.bind(gl_, textureBindings_, 2U);
                shader.setVec3(gl_, "uTint", materialColors[material]);
                shader.setFloat(gl_, "uEmissiveStrength",
                    material == static_cast<std::size_t>(WeaponMaterial::Sight)
                        ? 0.22F : 0.0F);
                drawMesh(mesh);
            }
        }

        if (presentedWeapon.handsVisible && !hud.mainMenu
            && hud.objective != ObjectiveRenderState::Victory
            && hud.objective != ObjectiveRenderState::Defeat) {
            const auto hands = samplePlayerHandsPose(presentationWeapon);
            const bool singleHandCarry = presentedWeapon.carryMotion > 0.12F
                && (knife || presentedWeapon.grenadeSelected
                    || viewmodelRole == HudWeaponRole::Pistol);
            if (playerRigPreview_ && playerRigAsset_) {
                constexpr std::array<const char*,playerRigBoneCount> uniforms{
                    "uPlayerBones[0]", "uPlayerBones[1]", "uPlayerBones[2]",
                    "uPlayerBones[3]", "uPlayerBones[4]", "uPlayerBones[5]",
                    "uPlayerBones[6]", "uPlayerBones[7]", "uPlayerBones[8]",
                    "uPlayerBones[9]", "uPlayerBones[10]", "uPlayerBones[11]",
                    "uPlayerBones[12]", "uPlayerBones[13]", "uPlayerBones[14]",
                    "uPlayerBones[15]", "uPlayerBones[16]", "uPlayerBones[17]",
                    "uPlayerBones[18]"};
                const auto palette = samplePlayerRigGripPalette(hands);
                shader.setInt(gl_,"uPlayerSkinning",1);
                shader.setInt(gl_,"uDetailedMaterial",0);
                shader.setMat4(gl_,"uModel",root);
                for (std::size_t bone = 0; bone < playerRigBoneCount; ++bone)
                    shader.setMat4(gl_,uniforms[bone],palette[bone]);
                shader.setVec3(gl_,"uTint",{1,1,1});
                shader.setFloat(gl_,"uEmissiveStrength",0);
                for (std::size_t part = 2U; part < (singleHandCarry ? 3U : 4U); ++part) {
                    for (std::size_t material = 0; material < playerRigAsset_->materialCount; ++material) {
                        const auto meshIndex = part*playerRigAsset_->materialCount+material;
                        shader.setFloat(gl_,"uEmissiveStrength",material == 7U ? 1.6F : 0.0F);
                        playerRigAlbedo_[material].bind(gl_,textureBindings_,0U);
                        drawMesh(playerRigMeshes_[meshIndex]);
                    }
                }
                shader.setFloat(gl_,"uEmissiveStrength",0.0F);
                shader.setInt(gl_,"uPlayerSkinning",0);
            } else {
                shader.setInt(gl_, "uPlayerSkinning", 2);
                shader.setMat4(gl_, "uModel", root);
                constexpr std::array<const char*, playerHandsBoneCount> uniforms{
                    "uPlayerBones[0]", "uPlayerBones[1]", "uPlayerBones[2]", "uPlayerBones[3]",
                    "uPlayerBones[4]", "uPlayerBones[5]", "uPlayerBones[6]", "uPlayerBones[7]",
                    "uPlayerBones[8]", "uPlayerBones[9]", "uPlayerBones[10]", "uPlayerBones[11]",
                    "uPlayerBones[12]", "uPlayerBones[13]", "uPlayerBones[14]", "uPlayerBones[15]",
                    "uPlayerBones[16]", "uPlayerBones[17]", "uPlayerBones[18]", "uPlayerBones[19]",
                    "uPlayerBones[20]", "uPlayerBones[21]", "uPlayerBones[22]", "uPlayerBones[23]",
                    "uPlayerBones[24]", "uPlayerBones[25]", "uPlayerBones[26]", "uPlayerBones[27]",
                    "uPlayerBones[28]", "uPlayerBones[29]", "uPlayerBones[30]", "uPlayerBones[31]",
                    "uPlayerBones[32]", "uPlayerBones[33]", "uPlayerBones[34]", "uPlayerBones[35]"};
                for (std::size_t i = 0; i < playerHandsBoneCount; ++i)
                    shader.setMat4(gl_, uniforms[i], hands.skin[i]);
                shader.setVec3(gl_, "uTint", {1,1,1});
                shader.setFloat(gl_, "uEmissiveStrength", 0);
                for (std::size_t i = 0; i < playerHandsMaterialCount; ++i) {
                    const auto& gpu = playerHandsMaterials_[i];
                    shader.setVec3(gl_, "uTint", {1.0F,1.0F,1.0F});
                    gpu.albedo.bind(gl_, textureBindings_, 0U);
                    gpu.normal.bind(gl_, textureBindings_, 1U);
                    gpu.properties.bind(gl_, textureBindings_, 2U);
                    drawMesh((knife && !presentedWeapon.grenadeSelected) || singleHandCarry
                        ? playerSingleHandMeshes_[i] : playerHandsMeshes_[i]);
                }
                shader.setInt(gl_, "uPlayerSkinning", 0);
            }
        }

        // The flash is transient and remains independent of the approved
        // weapon material. Rigid reload parts reuse their uploaded GPU meshes.
        fallbackTexture_.bind(gl_, textureBindings_);
        shader.setInt(gl_, "uDetailedMaterial", 0);
        shader.setVec2(gl_, "uUvScale", {1.0F, 1.0F});
        const auto drawWeaponPiece = [&](const wkc::core::Vec3 offset,
                                         const wkc::core::Vec3 scale,
                                         const wkc::core::Vec3 rotationDegrees,
                                         const wkc::core::Vec3 color,
                                         const float emissive = 0.0F) {
            const wkc::core::Mat4 model = root
                * wkc::core::translationMatrix(offset)
                * wkc::core::rotationZMatrix(rotationDegrees.z * degreesToRadians)
                * wkc::core::rotationYMatrix(rotationDegrees.y * degreesToRadians)
                * wkc::core::rotationXMatrix(rotationDegrees.x * degreesToRadians)
                * wkc::core::scaleMatrix(scale);
            shader.setMat4(gl_, "uModel", model);
            shader.setVec3(gl_, "uTint", color);
            shader.setFloat(gl_, "uEmissiveStrength", emissive);
            drawMesh(box_);
        };
        if (presentedWeapon.grenadeSelected && presentedWeapon.grenadeVisible) {
            const GrenadeViewmodelPose grenadePose
                = sampleGrenadeViewmodelPose(presentationWeapon);
            const bool pinAttached = presentedWeapon.grenadePinProgress < 0.5F;
            drawGrenadeModel(root, grenadePose.bodyCenter, pinAttached);
            if (grenadePose.pinVisible && !pinAttached) {
                shader.setInt(gl_, "uDetailedMaterial", 0);
                fallbackTexture_.bind(gl_, textureBindings_);
                shader.setMat4(gl_, "uModel", root
                    * wkc::core::translationMatrix(grenadePose.pinCenter)
                    * wkc::core::rotationXMatrix(std::numbers::pi_v<float> * 0.5F)
                    * wkc::core::scaleMatrix(grenadeModelParts_.empty()
                        ? wkc::core::Vec3{0.105F, 0.105F, 0.105F}
                        : wkc::core::Vec3{0.07F, 0.07F, 0.07F}));
                shader.setVec3(gl_, "uTint", {0.70F, 0.73F, 0.75F});
                shader.setFloat(gl_, "uEmissiveStrength", 0.28F);
                drawMesh(grenadeRing_);
                shader.setFloat(gl_, "uEmissiveStrength", 0.0F);
            }
            shader.setInt(gl_, "uDetailedMaterial", 0);
            fallbackTexture_.bind(gl_, textureBindings_);
        }
        const float muzzlePosition = viewmodelRole == HudWeaponRole::Sniper ? 1.25F
            : viewmodelRole == HudWeaponRole::Shotgun ? 1.14F
            : viewmodelRole == HudWeaponRole::SubmachineGun ? 0.81F
            : carbine ? 1.08F : knife ? 0.0F : 0.33F;
        const float muzzleHeight = viewmodelRole == HudWeaponRole::Sniper ? 0.014F
            : viewmodelRole == HudWeaponRole::Shotgun ? 0.046F
            : viewmodelRole == HudWeaponRole::SubmachineGun ? 0.005F : 0.0F;

        const float flash = std::clamp(
            std::isfinite(presentedWeapon.muzzleFlashStrength)
                ? presentedWeapon.muzzleFlashStrength : 0.0F,
            0.0F, 1.0F);
        if (flash > 0.0F && !presentedWeapon.grenadeSelected
            && presentedWeapon.role != HudWeaponRole::Knife) {
            const wkc::core::Vec3 flashColor{1.0F, 0.56F, 0.10F};
            drawWeaponPiece({0.0F, muzzleHeight, muzzlePosition + (0.07F * flash)},
                {0.07F + (0.08F * flash), 0.07F + (0.08F * flash),
                    0.18F + (0.12F * flash)}, {}, flashColor, 1.8F);
            drawWeaponPiece({0.0F, muzzleHeight, muzzlePosition + 0.04F},
                {0.20F * flash, 0.025F, 0.055F},
                {0.0F, 0.0F, 45.0F}, flashColor, 1.5F);
            drawWeaponPiece({0.0F, muzzleHeight, muzzlePosition + 0.04F},
                {0.20F * flash, 0.025F, 0.055F},
                {0.0F, 0.0F, -45.0F}, flashColor, 1.5F);
        }
        shader.setFloat(gl_, "uEmissiveStrength", 0.0F);
        shader.setInt(gl_, "uViewmodel", 0);
    }

    framebuffer_.presentToDefault(gl_, width, height);
    glViewport(0, 0, width, height);
    if (backfaceCulling_.needsUpdate(false)) {
        glDisable(GL_CULL_FACE);
    }
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    const int hudScale = width >= 640 && height >= 360 ? 2 : 1;
    const int hudMargin = 6 * hudScale;
    const int hudPadding = 4 * hudScale;
    const int hudLineHeight = 9 * hudScale;
    const UiVisualTheme uiTheme = industrialUiTheme();
    const wkc::core::Vec3 textColor = uiTheme.text;
    const wkc::core::Vec3 cyanColor = uiTheme.accent;
    // Gameplay HUD uses the same setting as the menus. Keep each group anchored
    // to its screen corner so changing the scale does not move it off-screen.
    const float requestedHudScale = static_cast<float>(
        std::clamp(hud.uiScalePercent, std::uint16_t{75}, std::uint16_t{200})) / 100.0F;
    // On narrow windows, cap enlargement before the left and right resource
    // groups can cover each other. Reducing the setting still works normally.
    const float gameplayHudScale = std::min({requestedHudScale,
        std::max(1.0F, static_cast<float>(width) / 480.0F),
        std::max(1.0F, static_cast<float>(height) / 320.0F)});
    bool scaleGameplayHud = false;
    int hudAnchorX = 0;
    int hudAnchorY = 0;
    const auto scaledHudRect = [&](const UiRect rect) {
        if (!scaleGameplayHud || gameplayHudScale == 1.0F) return rect;
        const auto position = [&](const int coordinate, const int anchor) {
            return anchor + static_cast<int>(std::lround(
                static_cast<float>(coordinate - anchor) * gameplayHudScale));
        };
        const int left = position(rect.x, hudAnchorX);
        const int bottom = position(rect.y, hudAnchorY);
        const int right = position(rect.x + rect.width, hudAnchorX);
        const int top = position(rect.y + rect.height, hudAnchorY);
        return UiRect{left, bottom, right - left, top - bottom};
    };
    const auto drawSolidRect = [&](const int x, const int y, const int rectWidth,
                                   const int rectHeight, const wkc::core::Vec3 color) {
        const UiRect rect = scaledHudRect({x, y, rectWidth, rectHeight});
        gdp::game::visual::drawSolidRect(rect.x, rect.y, rect.width, rect.height, color);
    };
    uiShader_.beginUniformFrame();
    // Cached quad and embedded atlas: no font files, rasterization or blur per frame.
    const auto drawUiQuad = [&](const UiRect unscaledRect, const wkc::core::Vec3 color,
                               const float alpha, const int glyph) {
        const UiRect rect = scaledHudRect(unscaledRect);
        glDisable(GL_SCISSOR_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        uiShader_.use(gl_);
        uiShader_.setVec2(gl_, "uViewport", {static_cast<float>(width), static_cast<float>(height)});
        uiShader_.setVec2(gl_, "uOrigin", {static_cast<float>(rect.x), static_cast<float>(rect.y)});
        uiShader_.setVec2(gl_, "uSize", {static_cast<float>(rect.width), static_cast<float>(rect.height)});
        uiShader_.setVec3(gl_, "uColor", color);
        uiShader_.setFloat(gl_, "uAlpha", alpha);
        uiShader_.setInt(gl_, "uUseFont", glyph == -2 ? 2 : glyph >= 0 ? 1 : 0);
        if (glyph == -2) {
            uiShader_.setFloat(gl_, "uScopeRadius", 0.44F * static_cast<float>(std::min(width, height)));
        }
        if (glyph >= 0) {
            uiFont_.bind(gl_, textureBindings_);
            uiShader_.setInt(gl_, "uAtlas", 0);
            uiShader_.setVec2(gl_, "uGlyphOrigin", {
                static_cast<float>(glyph % uiFontAtlasColumns) / static_cast<float>(uiFontAtlasColumns),
                static_cast<float>(glyph / uiFontAtlasColumns) / static_cast<float>(uiFontAtlasRows)});
            uiShader_.setVec2(gl_, "uGlyphSize", {1.0F / static_cast<float>(uiFontAtlasColumns),
                1.0F / static_cast<float>(uiFontAtlasRows)});
        }
        uiQuad_.draw(gl_, meshBindings_);
        glDisable(GL_BLEND);
        glEnable(GL_SCISSOR_TEST);
    };
    // Native overlay before resource/map/menu HUD: no second world pass.
    if (scopeStrength > 0.0F) {
        drawUiQuad({0, 0, width, height}, {0.005F, 0.007F, 0.008F}, scopeStrength, -2);
    }
    const auto drawBitmapText = [&](const int x, const int y, const auto scale,
                                    const std::string_view source, const wkc::core::Vec3 color) {
        const auto text = localize(hud.language, source);
        if (scale <= 1 && (!scaleGameplayHud || gameplayHudScale == 1.0F)) {
            gdp::game::visual::drawBitmapText(x, y, 1, text, color);
            return;
        }
        const float pixelScale = static_cast<float>(scale);
        int cursor = x;
        for (std::size_t offset = 0U; offset < text.size();) {
            auto character = nextUiCodepoint(text, offset);
            if (character > static_cast<std::uint32_t>(uiFontLastCodepoint)) character = '?';
            if (character > 32U) {
                // Same six-unit advance as the compact fallback and all shared layouts.
                const float cellScale = 0.4F * pixelScale;
                drawUiQuad({cursor - static_cast<int>(std::lround(pixelScale)),
                    y - static_cast<int>(std::round(static_cast<float>(uiFontCellHeight - uiFontBaseline - 1) * cellScale)),
                    static_cast<int>(std::lround(8.0F * pixelScale)),
                    static_cast<int>(std::round(static_cast<float>(uiFontCellHeight) * cellScale))},
                    color, 1.0F, static_cast<int>(character) - uiFontFirstCodepoint);
            }
            cursor += static_cast<int>(std::lround(6.0F * pixelScale));
        }
    };
    const auto bitmapTextWidth = [&](const std::string_view text, const auto scale) {
        return static_cast<int>(std::lround(
            static_cast<float>(uiTextLength(localize(hud.language, text)))
                * 6.0F * static_cast<float>(scale)));
    };
    const auto drawMenuPanel = [&](const UiRect panel,
                                         const int scale,
                                         const wkc::core::Vec3 accent) {
        drawUiQuad(panel, uiTheme.background, 0.78F, -1);
        (void)scale;
        (void)accent;
    };
    const auto drawIndustrialPanel = [&](const UiRect panel, const int scale,
                                         const wkc::core::Vec3 accent) {
        // Preserve the already approved gameplay information/map panels.
        drawSolidRect(panel.x, panel.y, panel.width, panel.height, uiTheme.panel);
        drawSolidRect(panel.x + 8 * scale, panel.y + panel.height - scale,
            std::min(panel.width - 16 * scale, 32 * scale), scale, accent);
    };
    const auto drawTacticalBackdrop = [&](const UiRect panel,
                                     const int,
                                     const wkc::core::Vec3) {
        // A quiet scrim keeps text readable without metal frames or opaque plates.
        drawUiQuad(panel, {0.018F, 0.029F, 0.034F}, 0.48F, -1);
    };
    const bool topRightObjective = hud.objective == ObjectiveRenderState::ReachBoss
        || hud.objective == ObjectiveRenderState::DefeatBoss
        || hud.objective == ObjectiveRenderState::TestArena
        || hud.objective == ObjectiveRenderState::ActivateExit
        || hud.objective == ObjectiveRenderState::SecureArena
        || hud.objective == ObjectiveRenderState::RecoverKey
        || hud.objective == ObjectiveRenderState::ActivateTerminal
        || hud.objective == ObjectiveRenderState::ClearAmbush;
    const bool showBoss = hud.visible && hud.boss.visible
        && hud.objective == ObjectiveRenderState::DefeatBoss
        && std::isfinite(hud.boss.health) && hud.boss.health > 0.0F
        && std::isfinite(hud.boss.maximumHealth) && hud.boss.maximumHealth > 0.0F;
    const float bossHealthRatio = showBoss
        ? std::clamp(hud.boss.health / hud.boss.maximumHealth, 0.0F, 1.0F) : 0.0F;
    const int bossPanelWidth = showBoss
        ? std::min(std::max(1, width - (hudMargin * 2)),
            std::max(220, std::min(240 * hudScale, width * 2 / 5))) : 0;
    const int bossPhaseWidth = std::max({
        bitmapTextWidth("PHASE 1/3 RANGE", hudScale),
        bitmapTextWidth("PHASE 2/3 PRESSURE", hudScale),
        bitmapTextWidth("PHASE 3/3 PURSUIT", hudScale),
    });
    const bool bossTwoLines = bossPanelWidth < (hudPadding * 2)
        + bitmapTextWidth("BOSS 100%", hudScale)
        + bossPhaseWidth + (6 * hudScale);
    const int bossPanelHeight = showBoss
        ? (hudPadding * 2) + (hudLineHeight * (bossTwoLines ? 2 : 1))
            + (6 * hudScale) : 0;
    const int topCornerOffset = showBoss ? bossPanelHeight + (3 * hudScale) : 0;
    const int topRightLines = (topRightObjective ? 2 : 0)
        + (hud.visible && hud.runTimerVisible ? 1 : 0)
        + (hud.visible && hud.runTimerVisible && hud.speedrunMode
                && hud.speedrunRecordVisible ? 1 : 0);
    const int topRightHeight = topRightLines == 0 ? 0
        : (hudPadding * 2) + (topRightLines * hudLineHeight);
    const auto drawSelectedRow = [&](const UiRect row,
                                     const auto scale,
                                     const wkc::core::Vec3 accent) {
        const float pixelScale = static_cast<float>(scale);
        drawUiQuad(row, textColor, 0.10F, -1);
        drawSolidRect(row.x + static_cast<int>(std::lround(6.0F * pixelScale)),
            row.y, row.width - static_cast<int>(std::lround(12.0F * pixelScale)),
            std::max(1, static_cast<int>(std::lround(pixelScale))), accent);
    };
    const auto drawExitConfirmation = [&]() {
        if (!hud.exitConfirmation) {
            return;
        }
        const ExitConfirmationLayout layout
            = makeExitConfirmationLayout(width, height, hud.uiScalePercent);
        const float scale = layout.textScale;
        const wkc::core::Vec3 dangerColor = uiTheme.danger;
        drawMenuPanel(layout.panel, layout.scale, dangerColor);
        const std::string_view title = hud.pauseConfirmation == HudPauseSelection::RestartRun
            ? "RESTART RUN?" : hud.pauseConfirmation == HudPauseSelection::MainMenu
            ? "RETURN TO MAIN MENU?" : "EXIT GAME?";
        const float titleScale = bitmapTextWidth(title, 2.0F * scale)
                > layout.panel.width - static_cast<int>(16.0F * scale)
            ? scale : 2.0F * scale;
        drawBitmapText((width - bitmapTextWidth(title, titleScale)) / 2,
            layout.panel.y + layout.panel.height - static_cast<int>(22.0F * scale),
            titleScale, title, dangerColor);

        const std::array<std::string_view, 2> options{
            "CANCEL", hud.pauseConfirmation == HudPauseSelection::RestartRun
                ? "RESTART" : hud.pauseConfirmation == HudPauseSelection::MainMenu
                ? "RETURN" : "EXIT",
        };
        const std::array<UiRect, 2> buttons{
            layout.cancelButton, layout.exitButton,
        };
        const std::size_t selected = hud.exitConfirmationSelection
                == HudExitConfirmationSelection::ExitGame
            ? 1U
            : 0U;
        for (std::size_t index = 0U; index < buttons.size(); ++index) {
            const UiRect button = buttons[index];
            if (index == selected) {
                drawSelectedRow(button, scale,
                    index == 0U ? cyanColor : dangerColor);
            }
            drawBitmapText(
                button.x
                    + ((button.width - bitmapTextWidth(options[index], scale)) / 2),
                button.y + static_cast<int>(3.0F * scale), scale, options[index],
                index == selected
                    ? (index == 0U ? cyanColor : dangerColor)
                    : textColor);
        }
        constexpr std::string_view controls = "WASD  ARROWS  ENTER  ESC  MOUSE";
        const float footerScale = std::max(1.0F, scale / 2.0F);
        drawBitmapText((width - bitmapTextWidth(controls, footerScale)) / 2,
            layout.panel.y + static_cast<int>(5.0F * scale), footerScale, controls,
            uiTheme.mutedText);
    };

    if (hud.listMenu.active) {
        const auto& menu = hud.listMenu;
        const auto layout = makeListMenuLayout(width, height, menu.rows.size(),
            menu.titleScreen, menu.uiScalePercent, menu.selection,
            menu.configuration && menu.configurationSection == 2U);
        const int scale = layout.scale;
        const float menuScale = layout.textScale;
        const auto configurationAccent = menu.configuration
            ? uiTheme.warning : uiTheme.accent;
        drawUiQuad({0, 0, width, height}, uiTheme.background, 0.16F, -1);
        if (menu.titleScreen) {
            // Quiet left-to-right scrim: the live scene remains the focus on the right.
            const int scrimWidth = std::min(width, layout.panel.x + layout.panel.width + width / 5);
            for (int x = 0; x < scrimWidth; x += 8) {
                const float opacity = 0.62F * (1.0F - static_cast<float>(x) / static_cast<float>(scrimWidth));
                drawUiQuad({x, 0, std::min(8, scrimWidth - x), height}, uiTheme.background, opacity, -1);
            }
        } else {
            drawMenuPanel(layout.panel, scale, textColor);
        }
        const auto fit = [&](const std::string& source, const int available, const float textScale,
            const bool raw = false) {
            std::string text = raw ? source : localize(hud.language, source);
            while (!text.empty() && bitmapTextWidth(text, textScale) > available) popUiCodepoint(text);
            return text;
        };
        const float titleScale = menu.titleScreen ? 3 * menuScale : 2 * menuScale;
        const auto title = fit(menu.title,
            layout.panel.width - static_cast<int>(16.0F * menuScale), titleScale);
        drawBitmapText(layout.panel.x + static_cast<int>(8.0F * menuScale),
            layout.panel.y + layout.panel.height
                - static_cast<int>(8.0F * menuScale + 7.0F * titleScale),
            titleScale, title, textColor);
        if (menu.configuration) {
            constexpr std::array<std::string_view, 4> tabs{
                "VIDEO", "AUDIO", "CONTROLS", "FEEDBACK"};
            const int inset = static_cast<int>(std::lround(8.0F * menuScale));
            const int tabWidth = (layout.panel.width - 2 * inset) / 4;
            const int tabY = layout.panel.y + layout.panel.height
                - static_cast<int>(std::lround(44.0F * menuScale));
            for (std::size_t i = 0; i < tabs.size(); ++i) {
                const UiRect tab{layout.panel.x + inset
                    + static_cast<int>(i) * tabWidth, tabY,
                    tabWidth, static_cast<int>(std::lround(12.0F * menuScale))};
                if (i == menu.configurationSection)
                    drawSelectedRow(tab, menuScale, configurationAccent);
                const auto caption = localize(hud.language, tabs[i]);
                const float tabTextScale = std::min(menuScale,
                    static_cast<float>(tab.width - static_cast<int>(4.0F * menuScale))
                        / std::max(1.0F, static_cast<float>(uiTextLength(caption)) * 6.0F));
                drawBitmapText(tab.x + static_cast<int>(2.0F * menuScale),
                    tab.y + static_cast<int>(2.0F * menuScale), tabTextScale,
                    caption, i == menu.configurationSection
                        ? configurationAccent : uiTheme.mutedText);
            }
            if (menu.configurationSection == 2U) {
                constexpr std::array<std::string_view, 3> controlPages{
                    "KEYBOARD", "CONTROLLER", "MAPPING"};
                const float sideScale = std::max(1.0F, menuScale * 0.70F);
                for (std::size_t i = 0U; i < layout.controlTabs.size(); ++i) {
                    const UiRect tab = layout.controlTabs[i];
                    if (tab.width <= 0 || tab.height <= 0) continue;
                    if (i == menu.configurationControlPage)
                        drawSelectedRow(tab, sideScale, configurationAccent);
                    const auto caption = fit(std::string{controlPages[i]},
                        tab.width - static_cast<int>(4.0F * sideScale),
                        sideScale);
                    drawBitmapText(tab.x + static_cast<int>(2.0F * sideScale),
                        tab.y + static_cast<int>(2.0F * sideScale),
                        sideScale, caption,
                        i == menu.configurationControlPage
                            ? configurationAccent : uiTheme.mutedText);
                }
            }
        } else {
            const auto subtitle = fit(menu.subtitle,
                layout.panel.width - static_cast<int>(16.0F * menuScale), menuScale);
            drawBitmapText(layout.panel.x + static_cast<int>(8.0F * menuScale),
                layout.panel.y + layout.panel.height - static_cast<int>(45.0F * menuScale),
                menuScale, subtitle, uiTheme.mutedText);
        }
        for (std::size_t i = 0; i < layout.rows.size(); ++i) {
            const auto row = layout.rows[i];
            if (row.width <= 0 || row.height <= 0) continue;
            const bool enabled = i >= menu.enabled.size() || menu.enabled[i];
            if (i == menu.selection && enabled) drawSelectedRow(row, scale, textColor);
            const bool adjustable = i < menu.adjustable.size() && menu.adjustable[i];
            const float slider = i < menu.sliderFractions.size()
                ? menu.sliderFractions[i] : -1.0F;
            const int left = row.x + static_cast<int>(6.0F * menuScale);
            const auto rowColor = enabled ? textColor : uiTheme.mutedText;
            if (adjustable) {
                // Settings reserve one line for the label and one for the live
                // value. The atlas glyph can occupy about 13 scale units, so
                // the 16-unit baseline gap leaves clear space between lines.
                const auto separator = menu.rows[i].rfind("  ");
                const std::string label = separator == std::string::npos
                    ? menu.rows[i] : menu.rows[i].substr(0U, separator);
                const std::string value = separator == std::string::npos
                    ? std::string{} : menu.rows[i].substr(separator + 2U);
                drawBitmapText(left,
                    row.y + static_cast<int>(listMenuLabelBaselineUnits * menuScale),
                    menuScale,
                    fit(label, row.width - static_cast<int>(12.0F * menuScale),
                        menuScale), rowColor);
                const int valueWidth = slider >= 0.0F
                    ? static_cast<int>(38.0F * menuScale)
                    : row.width - static_cast<int>(52.0F * menuScale);
                const int valueX = slider >= 0.0F
                    ? row.x + row.width - static_cast<int>(76.0F * menuScale)
                    : left;
                drawBitmapText(valueX,
                    row.y + static_cast<int>(listMenuValueBaselineUnits * menuScale),
                    menuScale, fit(value, valueWidth, menuScale), rowColor);
            } else {
                const auto text = fit(menu.rows[i],
                    row.width - static_cast<int>(12.0F * menuScale), menuScale,
                    i < menu.rawText.size() && menu.rawText[i]);
                drawBitmapText(left,
                    row.y + std::max(0,
                        (row.height - static_cast<int>(7.0F * menuScale)) / 2),
                    menuScale, text, rowColor);
            }
            if (slider >= 0.0F) {
                const UiRect track = listMenuSliderTrack(layout, i);
                drawSolidRect(track.x, track.y, track.width, track.height,
                    uiTheme.panelBorder);
                drawSolidRect(track.x, track.y,
                    static_cast<int>(std::lround(
                        static_cast<float>(track.width) * std::clamp(slider, 0.0F, 1.0F))),
                    track.height, enabled ? configurationAccent : uiTheme.mutedText);
            }
            if (adjustable) {
                const int y = row.y
                    + static_cast<int>(listMenuValueBaselineUnits * menuScale);
                drawBitmapText(row.x + row.width - static_cast<int>(34.0F * menuScale),
                    y, menuScale, "<", textColor);
                drawBitmapText(row.x + row.width - static_cast<int>(14.0F * menuScale),
                    y, menuScale, ">", textColor);
            }
        }
        const auto firstVisible = std::find_if(layout.rows.begin(), layout.rows.end(),
            [](const UiRect& row) { return row.width > 0 && row.height > 0; });
        const auto visibleCount = static_cast<std::size_t>(std::count_if(
            layout.rows.begin(), layout.rows.end(),
            [](const UiRect& row) { return row.width > 0 && row.height > 0; }));
        if (firstVisible != layout.rows.end() && visibleCount < layout.rows.size()) {
            const int trackTop = layout.panel.y + layout.panel.height
                - static_cast<int>(std::lround(52.0F * menuScale));
            const int trackBottom = layout.panel.y
                + static_cast<int>(std::lround(20.0F * menuScale));
            const int trackHeight = trackTop - trackBottom;
            const int barWidth = std::max(2, static_cast<int>(std::lround(2.0F * menuScale)));
            const int barX = layout.panel.x + layout.panel.width
                - static_cast<int>(std::lround(4.0F * menuScale));
            if (trackHeight > 0) {
                const int thumbHeight = std::min(trackHeight, std::max(
                    static_cast<int>(std::lround(12.0F * menuScale)),
                    static_cast<int>(std::lround(static_cast<float>(trackHeight)
                        * static_cast<float>(visibleCount)
                        / static_cast<float>(layout.rows.size())))));
                const auto firstIndex = static_cast<std::size_t>(
                    std::distance(layout.rows.begin(), firstVisible));
                const int thumbY = trackTop - thumbHeight
                    - static_cast<int>(std::lround(
                        static_cast<float>(trackHeight - thumbHeight)
                        * static_cast<float>(firstIndex)
                        / static_cast<float>(layout.rows.size() - visibleCount)));
                drawUiQuad({barX, trackBottom, barWidth, trackHeight},
                    uiTheme.panelBorder, 0.6F, -1);
                drawUiQuad({barX, thumbY, barWidth, thumbHeight},
                    configurationAccent, 0.9F, -1);
            }
        }
        constexpr std::string_view controls = "WASD/ARROWS ENTER ESC MOUSE";
        const auto footer = fit(menu.configuration && !menu.subtitle.empty()
                ? menu.subtitle : std::string{controls},
            layout.panel.width - static_cast<int>(16.0F * menuScale), menuScale);
        drawBitmapText(layout.panel.x + static_cast<int>(8.0F * menuScale),
            layout.panel.y + static_cast<int>(4.0F * menuScale),
            menuScale, footer, uiTheme.mutedText);
        drawExitConfirmation();
        glDisable(GL_SCISSOR_TEST);
        glEnable(GL_DEPTH_TEST);
        return glGetError() == GL_NO_ERROR;
    }
    if (hud.controlsMenu) {
        const ControlsMenuLayout layout = makeControlsMenuLayout(width, height);
        const int scale = layout.scale;
        drawUiQuad({0, 0, width, height}, uiTheme.background, 0.35F, -1);
        drawMenuPanel(layout.panel, scale, cyanColor);
        constexpr std::string_view title = "CONTROLS";
        drawBitmapText((width - bitmapTextWidth(title, 2 * scale)) / 2,
            layout.panel.y + layout.panel.height - (21 * scale),
            2 * scale, title, cyanColor);
        constexpr std::array<std::string_view, 16> bindings{
            "WASD      MOVE",
            "SHIFT     QUIET WALK",
            "CTRL      CROUCH",
            "SPACE     JUMP",
            "MOUSE1    FIRE / KNIFE",
            "MOUSE2    AIM",
            "R         RELOAD",
            "E         INTERACT",
            "F         FLASHLIGHT",
            "G         DROP GUN",
            "1/2/3     CARBINE/PISTOL/KNIFE",
            "Q / WHEEL LAST / CYCLE WEAPON",
            "TAB       RUN INFO",
            "F3        PERFORMANCE",
            "ESC       PAUSE / MENU",
            "MENUS     WS/ARROWS/ENTER/MOUSE",
        };
        const int textX = layout.panel.x + (12 * scale);
        const int firstY = layout.panel.y + layout.panel.height - (37 * scale);
        for (std::size_t index = 0U; index < bindings.size(); ++index) {
            drawBitmapText(textX, firstY - static_cast<int>(index) * (8 * scale),
                scale, bindings[index], textColor);
        }
        drawSelectedRow(layout.backButton, scale, cyanColor);
        constexpr std::string_view back = "BACK";
        drawBitmapText((width - bitmapTextWidth(back, scale)) / 2,
            layout.backButton.y + (3 * scale), scale, back, cyanColor);
        glDisable(GL_SCISSOR_TEST);
        glEnable(GL_DEPTH_TEST);
        const GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            std::ostringstream message;
            message << "OpenGL controls menu render error: 0x"
                    << std::hex << error;
            wkc::core::log(wkc::core::LogLevel::Error, message.str());
            return false;
        }
        return true;
    }

    if (hud.settingsMenu) {
        const SettingsMenuLayout layout = makeSettingsMenuLayout(width, height);
        const int scale = layout.scale;
        drawUiQuad({0, 0, width, height}, uiTheme.background, 0.35F, -1);
        drawMenuPanel(layout.panel, scale, cyanColor);
        constexpr std::string_view title = "SETTINGS";
        drawBitmapText((width - bitmapTextWidth(title, 2 * scale)) / 2,
            layout.panel.y + layout.panel.height - (22 * scale),
            2 * scale, title, cyanColor);

        constexpr std::array<std::string_view, 13> labels{
            "MASTER", "EFFECTS", "AMBIENT", "UI VOLUME",
            "SENSITIVITY", "INVERT Y", "FOV", "QUALITY", "RESOLUTION",
            "FULLSCREEN", "VSYNC", "LANGUAGE", "BACK",
        };
        const std::array<std::uint16_t, 13> numbers{
            hud.masterVolumePercent,
            hud.effectsVolumePercent,
            hud.ambientVolumePercent,
            hud.uiVolumePercent,
            hud.mouseSensitivityPercent,
            0U,
            hud.fieldOfViewDegrees,
            0U,
            0U,
            0U,
            0U,
            0U,
            0U,
        };
        for (std::size_t index = 0U; index < layout.rows.size(); ++index) {
            const UiRect row = layout.rows[index];
            const bool selected = index
                == static_cast<std::size_t>(hud.settingsSelection);
            if (selected) {
                drawSelectedRow(row, scale, cyanColor);
            }
            if (index == static_cast<std::size_t>(HudSettingsSelection::Back)) {
                drawBitmapText(
                    (width - bitmapTextWidth(labels[index], scale)) / 2,
                    row.y + (3 * scale), scale, labels[index],
                    selected ? cyanColor : textColor);
                continue;
            }
            std::string value;
            if (index == static_cast<std::size_t>(HudSettingsSelection::Language)) {
                value = hud.language == gameplay::Language::PortugueseBrazil ? "PORTUGUÊS (BR)" : "ENGLISH";
            } else if (index
                == static_cast<std::size_t>(HudSettingsSelection::InvertMouseY)) {
                value = hud.invertMouseY ? "ON" : "OFF";
            } else if (index
                == static_cast<std::size_t>(HudSettingsSelection::Fullscreen)) {
                value = hud.fullscreen ? "ON" : "OFF";
            } else if (index
                == static_cast<std::size_t>(HudSettingsSelection::VerticalSync)) {
                value = hud.verticalSync ? "ON" : "OFF";
            } else if (index
                == static_cast<std::size_t>(HudSettingsSelection::GraphicsQuality)) {
                switch (hud.graphicsQuality) {
                case HudGraphicsQuality::Potato:
                    value = "POTATO";
                    break;
                case HudGraphicsQuality::Low:
                    value = "LOW";
                    break;
                case HudGraphicsQuality::Medium:
                    value = "MEDIUM";
                    break;
                case HudGraphicsQuality::High:
                    value = "HIGH";
                    break;
                case HudGraphicsQuality::Ultra:
                    value = "ULTRA";
                    break;
                case HudGraphicsQuality::Custom:
                    value = hud.language == gameplay::Language::PortugueseBrazil
                        ? "PERSONALIZADO" : "CUSTOM";
                    break;
                }
            } else if (index
                == static_cast<std::size_t>(HudSettingsSelection::WindowResolution)) {
                value = std::to_string(hud.windowWidth) + "X"
                    + std::to_string(hud.windowHeight);
            } else if (index
                == static_cast<std::size_t>(HudSettingsSelection::FieldOfView)) {
                value = std::to_string(numbers[index]);
            } else {
                value = std::to_string(numbers[index]) + "%";
            }
            drawBitmapText(row.x + (8 * scale), row.y + (3 * scale),
                scale, labels[index], selected ? cyanColor : textColor);
            const UiRect decrement = layout.decrementButtons[index];
            const UiRect increment = layout.incrementButtons[index];
            const wkc::core::Vec3 arrowPanelColor = selected
                ? wkc::core::Vec3{0.10F, 0.27F, 0.30F}
                : wkc::core::Vec3{0.035F, 0.075F, 0.086F};
            drawSolidRect(decrement.x, decrement.y,
                decrement.width, decrement.height, arrowPanelColor);
            drawSolidRect(increment.x, increment.y,
                increment.width, increment.height, arrowPanelColor);
            drawBitmapText(
                decrement.x
                    + ((decrement.width - bitmapTextWidth("<", scale)) / 2),
                decrement.y + (3 * scale), scale, "<",
                selected ? cyanColor : textColor);
            drawBitmapText(
                increment.x
                    + ((increment.width - bitmapTextWidth(">", scale)) / 2),
                increment.y + (3 * scale), scale, ">",
                selected ? cyanColor : textColor);
            const int valueLeft = decrement.x + decrement.width;
            const int valueWidth = std::max(0, increment.x - valueLeft);
            drawBitmapText(valueLeft
                    + ((valueWidth - bitmapTextWidth(value, scale)) / 2),
                row.y + (3 * scale), scale, value,
                selected ? cyanColor : textColor);
        }
        const std::string_view controls = width >= 480
            ? "W S SELECT  A D ADJUST  ENTER BACK  MOUSE"
            : "W S  A D  ENTER  MOUSE";
        drawBitmapText((width - bitmapTextWidth(controls, 1)) / 2,
            layout.panel.y + (7 * scale), 1, controls,
            uiTheme.mutedText);

        glDisable(GL_SCISSOR_TEST);
        glEnable(GL_DEPTH_TEST);
        const GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            std::ostringstream message;
            message << "OpenGL settings menu render error: 0x"
                    << std::hex << error;
            wkc::core::log(wkc::core::LogLevel::Error, message.str());
            return false;
        }
        return true;
    }

    if (hud.mainMenu) {
        const MainMenuLayout layout = makeMainMenuLayout(width, height);
        const int scale = layout.scale;
        drawUiQuad({0, 0, width, height}, uiTheme.background, 0.35F, -1);
        const int railWidth = std::max(2, 2 * scale);
        drawSolidRect(layout.panel.x - (5 * scale), layout.panel.y,
            railWidth, layout.panel.height, uiTheme.panelBorder);
        drawSolidRect(layout.panel.x + layout.panel.width + (3 * scale),
            layout.panel.y, railWidth, layout.panel.height, uiTheme.panelBorder);
        drawMenuPanel(layout.panel, scale, cyanColor);

        constexpr std::string_view title = "IRON SEED";
        constexpr std::string_view subtitle = "INDUSTRIAL COMPLEX";
        drawBitmapText((width - bitmapTextWidth(title, 3 * scale)) / 2,
            layout.panel.y + layout.panel.height - (26 * scale),
            3 * scale, title, cyanColor);
        drawBitmapText((width - bitmapTextWidth(subtitle, 1)) / 2,
            layout.panel.y + layout.panel.height - (38 * scale),
            1, subtitle, {0.48F, 0.62F, 0.66F});

        const std::string bestSeedText = hud.bestSeedAvailable
            ? "BEST SEED " + runTimeText(hud.bestSpeedrunTicks)
            : "BEST SEED --";
        const std::string continueText = hud.campaignContinueAvailable
            ? "CONTINUE F" + std::to_string(hud.campaignContinueFloor) + "/3"
            : "CONTINUE --";
        const std::array<std::string_view, 8> options{
            "START RUN", continueText, "CAMPAIGN", "SPEEDRUN", bestSeedText,
            "SETTINGS", "CONTROLS", "EXIT GAME",
        };
        const std::array<UiRect, 8> buttons{
            layout.startButton, layout.continueButton, layout.campaignButton,
            layout.speedrunButton,
            layout.bestSeedButton, layout.settingsButton,
            layout.controlsButton, layout.exitButton,
        };
        std::size_t selected = 0U;
        if (hud.mainMenuSelection == HudMainMenuSelection::ContinueCampaign) {
            selected = 1U;
        } else if (hud.mainMenuSelection == HudMainMenuSelection::Campaign) {
            selected = 2U;
        } else if (hud.mainMenuSelection == HudMainMenuSelection::Speedrun) {
            selected = 3U;
        } else if (hud.mainMenuSelection == HudMainMenuSelection::BestSeed) {
            selected = 4U;
        } else if (hud.mainMenuSelection == HudMainMenuSelection::Settings) {
            selected = 5U;
        } else if (hud.mainMenuSelection == HudMainMenuSelection::Controls) {
            selected = 6U;
        } else if (hud.mainMenuSelection == HudMainMenuSelection::ExitGame) {
            selected = 7U;
        }
        for (std::size_t index = 0; index < buttons.size(); ++index) {
            const UiRect button = buttons[index];
            if (index == selected) {
                drawSelectedRow(button, scale, cyanColor);
            }
            const int textWidth = bitmapTextWidth(options[index], scale);
            drawBitmapText((width - textWidth) / 2,
                button.y + (3 * scale), scale, options[index],
                (index == 1U && !hud.campaignContinueAvailable)
                        || (index == 4U && !hud.bestSeedAvailable)
                    ? wkc::core::Vec3{0.30F, 0.38F, 0.40F}
                    : index == selected ? cyanColor : textColor);
        }

        const std::string_view controls = width >= 480
            ? "W S SELECT  ENTER CONFIRM  MOUSE SELECT"
            : "W S  ENTER  MOUSE";
        drawBitmapText((width - bitmapTextWidth(controls, 1)) / 2,
            layout.panel.y + (7 * scale), 1, controls,
            uiTheme.mutedText);

        drawExitConfirmation();

        glDisable(GL_SCISSOR_TEST);
        glEnable(GL_DEPTH_TEST);
        const GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            std::ostringstream message;
            message << "OpenGL main menu render error: 0x" << std::hex << error;
            wkc::core::log(wkc::core::LogLevel::Error, message.str());
            return false;
        }
        return true;
    }

    if (hud.visible) {
        const float damageStrength = std::clamp(hud.damageFeedback, 0.0F, 1.0F);
        if (!hud.paused && damageStrength > 0.0F) {
            const int edgeThickness = std::max(1,
                static_cast<int>((2.0F + (4.0F * damageStrength))
                    * static_cast<float>(hudScale)));
            const wkc::core::Vec3 damageColor{
                0.16F + (0.78F * damageStrength),
                0.012F + (0.045F * damageStrength),
                0.008F,
            };
            drawSolidRect(0, 0, width, edgeThickness, damageColor);
            drawSolidRect(0, height - edgeThickness, width, edgeThickness, damageColor);
            drawSolidRect(0, edgeThickness, edgeThickness,
                height - (edgeThickness * 2), damageColor);
            drawSolidRect(width - edgeThickness, edgeThickness, edgeThickness,
                height - (edgeThickness * 2), damageColor);
        }

        scaleGameplayHud = true;
        hudAnchorX = 0;
        hudAnchorY = 0;

        const int health = static_cast<int>(std::round(std::max(hud.health, 0.0F)));
        const int maximumHealth = static_cast<int>(std::round(std::max(hud.maximumHealth, 1.0F)));
        const int armor = static_cast<int>(std::round(std::max(hud.armor, 0.0F)));
        const int maximumArmor = static_cast<int>(std::round(std::max(hud.maximumArmor, 1.0F)));
        const auto tactical = makeTacticalHudLayout(width, height);
        const int leftX = tactical.health.x;
        const int rightEdge = tactical.weapon.x + tactical.weapon.width;
        const int resourceY = tactical.health.y;
        const int largeScale = 2 * hudScale;
        const float healthRatio = std::clamp(
            std::max(hud.health, 0.0F) / std::max(hud.maximumHealth, 1.0F), 0.0F, 1.0F);
        const wkc::core::Vec3 healthColor = healthRatio <= 0.25F ? uiTheme.danger : textColor;
        // No metal frame: two corner groups, with primary numbers larger than labels.
        drawBitmapText(leftX, resourceY + 54 * hudScale, hudScale, "HEALTH", uiTheme.mutedText);
        const std::string healthValue = std::to_string(health);
        drawBitmapText(leftX, resourceY + 34 * hudScale, largeScale, healthValue, healthColor);
        drawBitmapText(leftX + bitmapTextWidth(healthValue, largeScale) + 3 * hudScale,
            resourceY + 34 * hudScale, hudScale, "/ " + std::to_string(maximumHealth), uiTheme.mutedText);
        const std::string armorText = "ARMOR " + std::to_string(armor) + "/" + std::to_string(maximumArmor);
        drawBitmapText(leftX, resourceY + 20 * hudScale, hudScale, armorText, uiTheme.mutedText);
        drawBitmapText(leftX, resourceY + 6 * hudScale, hudScale,
            "KEYS " + std::to_string(hud.keys), uiTheme.mutedText);
        const int healthBarWidth = std::min(tactical.health.width, 90 * hudScale);
        drawSolidRect(leftX, resourceY + 29 * hudScale, healthBarWidth, hudScale, uiTheme.panelBorder);
        drawSolidRect(leftX, resourceY + 29 * hudScale,
            static_cast<int>(std::round(static_cast<float>(healthBarWidth) * healthRatio)),
            hudScale, healthColor);
        const float armorRatio = std::clamp(
            std::max(hud.armor, 0.0F) / std::max(hud.maximumArmor, 1.0F), 0.0F, 1.0F);
        drawSolidRect(leftX, resourceY + 16 * hudScale, healthBarWidth, hudScale, uiTheme.panelBorder);
        drawSolidRect(leftX, resourceY + 16 * hudScale,
            static_cast<int>(std::round(static_cast<float>(healthBarWidth) * armorRatio)),
            hudScale, uiTheme.mutedText);

        hudAnchorX = width;
        const std::string weaponLabel = std::string{hud.grenadeSelected ? "FRAG"
            : hud.weapon == HudWeaponRole::Knife ? "KNIFE"
            : hud.weapon == HudWeaponRole::Sniper ? "SNIPER"
            : hud.weapon == HudWeaponRole::Shotgun ? "SHOTGUN"
            : hud.weapon == HudWeaponRole::SubmachineGun ? "SMG"
            : hud.weapon == HudWeaponRole::Carbine ? "CARBINE" : "PISTOL"}
            + (hud.weaponOverdrive && !hud.grenadeSelected ? " OD" : "");
        drawBitmapText(rightEdge - bitmapTextWidth(weaponLabel, hudScale),
            resourceY + 54 * hudScale, hudScale, weaponLabel, uiTheme.mutedText);
        if (hud.grenadeSelected) {
            const std::string count = std::to_string(hud.grenades);
            const std::string reserve = hud.language == gameplay::Language::PortugueseBrazil
                ? "/ 2 GRANADAS" : "/ 2 GRENADES";
            const int reserveWidth = bitmapTextWidth(reserve, hudScale);
            drawBitmapText(rightEdge - reserveWidth - 3 * hudScale
                    - bitmapTextWidth(count, largeScale),
                resourceY + 34 * hudScale, largeScale, count,
                hud.grenades == 0U ? uiTheme.warning : textColor);
            drawBitmapText(rightEdge - reserveWidth, resourceY + 34 * hudScale,
                hudScale, reserve, uiTheme.mutedText);
            if (hud.grenadeChargeStrength > 0.0F) {
                const int chargeWidth = std::min(tactical.weapon.width, 90 * hudScale);
                drawSolidRect(rightEdge - chargeWidth, resourceY + 29 * hudScale,
                    chargeWidth, hudScale, uiTheme.panelBorder);
                drawSolidRect(rightEdge - chargeWidth, resourceY + 29 * hudScale,
                    static_cast<int>(std::round(static_cast<float>(chargeWidth)
                        * std::clamp(hud.grenadeChargeStrength, 0.0F, 1.0F))),
                    hudScale, uiTheme.warning);
            }
        } else if (hud.weapon == HudWeaponRole::Knife) {
            constexpr std::string_view ready = "READY";
            drawBitmapText(rightEdge - bitmapTextWidth(ready, largeScale),
                resourceY + 34 * hudScale, largeScale, ready, textColor);
        } else {
            const std::string ammo = std::to_string(hud.magazineAmmo);
            const std::string reserve = "/ " + std::to_string(hud.reserveAmmo)
                + (hud.weapon == HudWeaponRole::Shotgun
                    ? (hud.language == gameplay::Language::PortugueseBrazil ? " CART" : " SHELLS")
                    : (hud.language == gameplay::Language::PortugueseBrazil ? " PENTES" : " MAGS"));
            const int reserveWidth = bitmapTextWidth(reserve, hudScale);
            drawBitmapText(rightEdge - reserveWidth - 3 * hudScale - bitmapTextWidth(ammo, largeScale),
                resourceY + 34 * hudScale, largeScale, ammo,
                hud.magazineAmmo == 0U ? uiTheme.warning : textColor);
            drawBitmapText(rightEdge - reserveWidth, resourceY + 34 * hudScale,
                hudScale, reserve, uiTheme.mutedText);
        }
        const bool grenadeFuseActive = hud.grenadeSelected && hud.grenadePriming
            && hud.grenadeFuseRemainingSeconds > 0.0F;
        const int fuseHundredths = static_cast<int>(std::lround(
            std::clamp(hud.grenadeFuseRemainingSeconds,
                0.0F, gameplay::grenadeFuseSeconds) * 100.0F));
        const std::string levelText = "LV " + std::to_string(hud.level);
        const std::string experienceText = "XP " + std::to_string(hud.experience)
            + (hud.experienceForNextLevel > 0U
                ? "/" + std::to_string(hud.experienceForNextLevel) : " MAX");
        const std::string progressText = grenadeFuseActive
            ? std::string{hud.language == gameplay::Language::PortugueseBrazil
                ? "TEMPO " : "FUSE "}
                + std::to_string(fuseHundredths / 100)
                + (hud.language == gameplay::Language::PortugueseBrazil ? "," : ".")
                + (fuseHundredths % 100 < 10 ? "0" : "")
                + std::to_string(fuseHundredths % 100) + "S"
            : levelText + "  " + experienceText;
        const int progressScale = bitmapTextWidth(progressText, hudScale) <= tactical.weapon.width
            ? hudScale : 1;
        drawBitmapText(rightEdge - bitmapTextWidth(progressText, progressScale),
            resourceY + 18 * hudScale, progressScale, progressText,
            grenadeFuseActive && hud.grenadeFuseRemainingSeconds <= 1.0F
                ? uiTheme.danger : uiTheme.mutedText);
        const float experienceRatio = hud.experienceForNextLevel == 0U ? 1.0F
            : std::clamp(static_cast<float>(hud.experience)
                / static_cast<float>(hud.experienceForNextLevel), 0.0F, 1.0F);
        const float progressRatio = grenadeFuseActive
            ? std::clamp(hud.grenadeFuseRemainingSeconds
                / gameplay::grenadeFuseSeconds, 0.0F, 1.0F)
            : experienceRatio;
        const int progressWidth = std::min(tactical.weapon.width, 90 * hudScale);
        drawSolidRect(rightEdge - progressWidth, resourceY + 12 * hudScale,
            progressWidth, hudScale, uiTheme.panelBorder);
        drawSolidRect(rightEdge - progressWidth, resourceY + 12 * hudScale,
            static_cast<int>(std::round(static_cast<float>(progressWidth) * progressRatio)),
            hudScale, grenadeFuseActive ? uiTheme.warning : uiTheme.mutedText);
        hudAnchorX = width / 2;
        if (hud.flashlightOn && width >= 640) {
            const std::string lightText = hud.flashlightBinding + " LIGHT ON";
            drawBitmapText((width - bitmapTextWidth(lightText, hudScale)) / 2,
                hudMargin + 4 * hudScale, hudScale, lightText, uiTheme.mutedText);
        }

        std::string_view eventText;
        wkc::core::Vec3 eventColor{0.48F, 0.62F, 0.66F};
        if (hud.eventLocked) {
            eventText = "LOCKDOWN";
            eventColor = {0.98F, 0.18F, 0.10F};
        } else if (hud.event == HudEventState::Dormant) {
            eventText = "EVENT STANDBY";
        } else if (hud.event == HudEventState::Active) {
            eventText = "EVENT ACTIVE";
            eventColor = {0.98F, 0.62F, 0.14F};
        } else if (hud.event == HudEventState::Completed) {
            eventText = "EVENT CLEAR";
            eventColor = {0.28F, 0.92F, 0.56F};
        }
        hudAnchorX = width;
        hudAnchorY = height;
        if (!eventText.empty()) {
            const int eventWidth = bitmapTextWidth(eventText, hudScale) + (hudPadding * 2);
            const int eventHeight = (7 * hudScale) + (hudPadding * 2);
            const int eventX = std::max(hudMargin, width - hudMargin - eventWidth);
            const int eventY = std::max(hudMargin,
                height - hudMargin - topCornerOffset - topRightHeight
                    - (topRightHeight > 0 ? 3 * hudScale : 0) - eventHeight);
            drawTacticalBackdrop(
                {eventX, eventY, eventWidth, eventHeight}, hudScale, eventColor);
            drawBitmapText(eventX + hudPadding, eventY + hudPadding,
                hudScale, eventText, eventColor);
        }

        std::string promptText;
        wkc::core::Vec3 promptColor{0.78F, 0.90F, 0.92F};
        if (hud.prompt == HudPrompt::OpenDoor) {
            promptText = hud.interactBinding + " OPEN DOOR";
        } else if (hud.prompt == HudPrompt::DoorLocked) {
            promptText = "DOOR LOCKED";
            promptColor = {0.98F, 0.24F, 0.12F};
        } else if (hud.prompt == HudPrompt::UseTerminal) {
            promptText = hud.interactBinding + " USE TERMINAL";
        } else if (hud.prompt == HudPrompt::Restock) {
            promptText = hud.interactBinding + " RESTOCK";
        } else if (hud.prompt == HudPrompt::Reload) {
            promptText = hud.reloadBinding + " RELOAD";
            promptColor = {0.98F, 0.62F, 0.14F};
        } else if (hud.prompt == HudPrompt::ExitTerminalLocked) {
            promptText = "CLEAR ENCOUNTER TO UNLOCK";
            promptColor = uiTheme.warning;
        } else if (hud.prompt == HudPrompt::CampaignMissionRequired) {
            promptText = "COMPLETE MISSION";
            promptColor = uiTheme.warning;
        } else if (hud.prompt == HudPrompt::NextFloorTerminal) {
            promptText = hud.interactBinding + " ASCEND FLOOR";
        } else if (hud.prompt == HudPrompt::FinishCampaignTerminal) {
            promptText = hud.interactBinding + " FINISH CAMPAIGN";
        }
        hudAnchorX = width / 2;
        hudAnchorY = height / 2;
        if (!hud.paused && !promptText.empty()) {
            promptText = localize(hud.language, promptText);
            const int promptWidth = bitmapTextWidth(promptText, hudScale)
                + (hudPadding * 2);
            const int promptHeight = (7 * hudScale) + (hudPadding * 2);
            const int promptX = (width - promptWidth) / 2;
            const int promptY = std::max(hudMargin, height / 5);
            drawTacticalBackdrop(
                {promptX, promptY, promptWidth, promptHeight}, hudScale, promptColor);
            drawBitmapText(promptX + hudPadding, promptY + hudPadding,
                hudScale, promptText, promptColor);
        }

        const float hitStrength = std::clamp(hud.hitFeedbackStrength, 0.0F, 1.0F);
        if (!hud.paused && hud.hitFeedback != HudHitFeedback::None
            && hitStrength > 0.0F) {
            const bool killed = hud.hitFeedback == HudHitFeedback::Kill
                || hud.hitFeedback == HudHitFeedback::WeakKill;
            const bool weakPoint = hud.hitFeedback == HudHitFeedback::WeakHit
                || hud.hitFeedback == HudHitFeedback::WeakKill;
            const wkc::core::Vec3 baseColor = weakPoint
                ? wkc::core::Vec3{1.0F, 0.78F, 0.16F}
                : killed ? wkc::core::Vec3{1.0F, 0.26F, 0.10F}
                         : wkc::core::Vec3{0.82F, 0.94F, 0.96F};
            const wkc::core::Vec3 markerColor = baseColor
                * (0.32F + (0.68F * hitStrength));
            const int centerX = width / 2;
            const int centerY = height / 2;
            const int gap = 10 * hudScale;
            const int markerLength = 5 * hudScale;
            const int markerThickness = std::max(1, hudScale);
            drawSolidRect(centerX - gap - markerLength,
                centerY - (markerThickness / 2), markerLength,
                markerThickness, markerColor);
            drawSolidRect(centerX + gap,
                centerY - (markerThickness / 2), markerLength,
                markerThickness, markerColor);
            drawSolidRect(centerX - (markerThickness / 2),
                centerY - gap - markerLength, markerThickness,
                markerLength, markerColor);
            drawSolidRect(centerX - (markerThickness / 2), centerY + gap,
                markerThickness, markerLength, markerColor);
            if (weakPoint) {
                const int innerGap = 5 * hudScale;
                const int innerLength = 2 * hudScale;
                drawSolidRect(centerX - innerGap - innerLength,
                    centerY - (markerThickness / 2), innerLength,
                    markerThickness, markerColor);
                drawSolidRect(centerX + innerGap,
                    centerY - (markerThickness / 2), innerLength,
                    markerThickness, markerColor);
            }
        }

        std::string_view noticeText;
        wkc::core::Vec3 noticeColor{0.72F, 0.90F, 0.92F};
        if (hud.notice == HudNotice::Pickup) {
            noticeText = "SUPPLIES ACQUIRED";
            noticeColor = {0.28F, 0.92F, 0.56F};
        } else if (hud.notice == HudNotice::EventStart) {
            noticeText = "EVENT START";
            noticeColor = {0.98F, 0.58F, 0.14F};
        } else if (hud.notice == HudNotice::Wave) {
            noticeText = "NEW WAVE";
            noticeColor = {0.98F, 0.38F, 0.12F};
        } else if (hud.notice == HudNotice::EventClear) {
            noticeText = "SECTOR CLEAR";
            noticeColor = {0.28F, 0.92F, 0.56F};
        } else if (hud.notice == HudNotice::LevelUp) {
            noticeText = "LEVEL UP";
            noticeColor = {0.32F, 0.88F, 1.0F};
        } else if (hud.notice == HudNotice::WeaponDropped) {
            noticeText = "WEAPON DROPPED";
            noticeColor = {0.32F, 0.88F, 1.0F};
        } else if (hud.notice == HudNotice::WeaponDropBlocked) {
            noticeText = hud.weapon == HudWeaponRole::Knife
                ? "KNIFE CANNOT DROP" : "NEED SPACE / WAIT";
            noticeColor = uiTheme.warning;
        }
        const float noticeStrength = std::clamp(
            hud.noticeFeedbackStrength, 0.0F, 1.0F);
        hudAnchorY = height;
        if (!hud.paused && !noticeText.empty() && noticeStrength > 0.0F) {
            const int noticeWidth = bitmapTextWidth(noticeText, hudScale)
                + (hudPadding * 2);
            const int noticeHeight = (7 * hudScale) + (hudPadding * 2);
            const int noticeX = (width - noticeWidth) / 2;
            const int noticeY = std::max(hudMargin,
                height - hudMargin - noticeHeight - (18 * hudScale));
            drawTacticalBackdrop(
                {noticeX, noticeY, noticeWidth, noticeHeight}, hudScale, noticeColor);
            drawBitmapText(noticeX + hudPadding, noticeY + hudPadding,
                hudScale, noticeText,
                noticeColor * (0.42F + (0.58F * noticeStrength)));
        }
    }

    UiRect mapPanelRect{};
    bool mapPanelVisible = false;
    const auto drawInfoOverlay = [&] {
        if (hud.infoOverlay && hud.visible && !hud.mainMenu && !hud.paused
            && !hud.settingsMenu) {
            std::array<std::string, 11> lines{
                "RUN STATUS",
                "SEED " + std::to_string(hud.runSeed),
                "SCORE " + std::to_string(hud.score),
                "KILLS " + std::to_string(hud.enemiesKilled),
                "EVENTS " + std::to_string(hud.eventsCompleted),
                "LOOT " + std::to_string(hud.lootCollected),
                "TIME " + runTimeText(hud.elapsedRunTicks),
            };
            std::size_t lineCount = 7U;
            if (hud.campaignMode) {
                std::string mission = "MISSION UNKNOWN";
                if (hud.campaignRouteMission) {
                    const unsigned int key = hud.campaignKeyRecovered ? 1U : 0U;
                    const unsigned int arena = hud.campaignArenaSecured ? 1U : 0U;
                    mission = "MISSION " + std::to_string(key + arena)
                        + "/2: KEY " + std::to_string(key)
                        + "/1 ARENA " + std::to_string(arena) + "/1";
                } else if (hud.campaignMissionCompleted) {
                    mission = "MISSION COMPLETE";
                } else {
                    switch (hud.campaignMission) {
                    case ObjectiveRenderState::SecureArena:
                        mission = "MISSION: SECURE ARENA"; break;
                    case ObjectiveRenderState::RecoverKey:
                        mission = "MISSION: RECOVER KEY"; break;
                    case ObjectiveRenderState::ActivateTerminal:
                        mission = "MISSION: ACTIVATE TERMINAL"; break;
                    case ObjectiveRenderState::ClearAmbush:
                        mission = "MISSION: CLEAR AMBUSH"; break;
                    default: break;
                    }
                }
                lines[lineCount++] = "FLOOR " + std::to_string(hud.campaignFloor)
                    + "/" + std::to_string(hud.campaignFloorCount);
                lines[lineCount++] = std::move(mission);
                lines[lineCount++] = hud.campaignBossDefeated ? "BOSS DOWN" : "BOSS ALIVE";
                lines[lineCount++] = hud.campaignExitReady ? "EXIT READY" : "EXIT LOCKED";
            }
            const int panelScale = height >= 480 ? hudScale : 1;
            const int lineHeight = 8 * panelScale;
            int textWidth = 0;
            for (std::size_t index = 0U; index < lineCount; ++index) {
                textWidth = std::max(textWidth,
                    bitmapTextWidth(lines[index], panelScale));
            }
            const int panelWidth = std::min(width - (hudMargin * 2),
                textWidth + (hudPadding * 4));
            const int panelHeight = static_cast<int>(lineCount) * lineHeight
                + (hudPadding * 2);
            const int mapGap = 3 * hudScale;
            const int belowMapY = mapPanelRect.y - mapGap - panelHeight;
            const bool fitsBelowMap = mapPanelVisible
                && belowMapY >= hudMargin + (65 * hudScale);
            const int panelX = fitsBelowMap ? mapPanelRect.x
                : (width - panelWidth) / 2;
            const int panelY = fitsBelowMap ? belowMapY
                : (height - panelHeight) / 2;
            hudAnchorX = fitsBelowMap ? 0 : width / 2;
            hudAnchorY = fitsBelowMap ? height : height / 2;
            drawIndustrialPanel({panelX, panelY, panelWidth, panelHeight},
                panelScale, cyanColor);
            int lineY = panelY + panelHeight - hudPadding
                - (7 * panelScale);
            for (std::size_t index = 0; index < lineCount; ++index) {
                drawBitmapText(panelX + (hudPadding * 2), lineY,
                    panelScale, lines[index],
                    index == 0U ? cyanColor : textColor);
                lineY -= lineHeight;
            }
        }
    };

    int performancePanelHeight = 0;
    if (hud.performanceOverlay) {
        scaleGameplayHud = true;
        hudAnchorX = 0;
        hudAnchorY = height;
        const wkc::core::Vec3 performanceColor
            = hud.percentile95FrameMilliseconds <= 17U
            ? uiTheme.success
            : hud.percentile95FrameMilliseconds <= 34U
                ? wkc::core::Vec3{0.98F, 0.62F, 0.14F}
                : uiTheme.danger;
        std::string qualityName = "MEDIUM";
        switch (hud.graphicsQuality) {
        case HudGraphicsQuality::Potato:
            qualityName = "POTATO";
            break;
        case HudGraphicsQuality::Low:
            qualityName = "LOW";
            break;
        case HudGraphicsQuality::Medium:
            break;
        case HudGraphicsQuality::High:
            qualityName = "HIGH";
            break;
        case HudGraphicsQuality::Ultra:
            qualityName = "ULTRA";
            break;
        case HudGraphicsQuality::Custom:
            qualityName = hud.language == gameplay::Language::PortugueseBrazil
                ? "PERSONALIZADO" : "CUSTOM";
            break;
        }
        const std::string gpuTime = hud.gpuTimingAvailable
            ? std::to_string(hud.averageGpuMilliseconds) + "MS"
            : "--";
        const std::array<std::string, 15> lines{
            "QUALITY " + qualityName + " SCALE "
                + std::to_string(hud.worldRenderScalePercent) + "%",
            "FPS " + std::to_string(hud.performanceFramesPerSecond)
                + " FRAME " + std::to_string(hud.averageFrameMilliseconds)
                + "MS P95 " + std::to_string(hud.percentile95FrameMilliseconds)
                + "MS",
            "WORST " + std::to_string(hud.worstFrameMilliseconds)
                + "MS SLOW " + std::to_string(hud.framesOverBudgetPercent)
                + "%",
            "CPU UPDATE " + std::to_string(hud.averageUpdateMilliseconds)
                + "MS RENDER " + std::to_string(hud.averageRenderMilliseconds)
                + "MS",
            "GPU " + gpuTime + " WAIT "
                + std::to_string(hud.averagePresentMilliseconds) + "MS",
            "DRAW " + std::to_string(hud.performanceDrawCalls)
                + " TRI " + std::to_string(hud.performanceTriangles),
            "VISIBLE E " + std::to_string(hud.performanceVisibleEnemies)
                + " P " + std::to_string(hud.performanceVisibleProps)
                + " R " + std::to_string(hud.performanceVisibleRooms),
            "LIGHT " + std::to_string(hud.performanceActiveLights)
                + " SHADOW " + std::to_string(hud.performanceShadowLights)
                + "X" + std::to_string(hud.performanceShadowOccluders),
            std::string{"SHADER "}
                + (hud.performanceLowSpecShader ? "LOW SPEC" : "FULL")
                + " SHADOW CAP "
                + std::to_string(
                    hud.performanceShadowShaderOccluderCapacity),
            "CULL DRAW "
                + std::to_string(hud.performanceBackfaceCulledDraws)
                + " STATE "
                + std::to_string(hud.performanceCullStateChanges),
            "INSTANCE DRAW "
                + std::to_string(hud.performanceInstancedDrawCalls)
                + " COUNT "
                + std::to_string(hud.performanceRenderedInstances)
                + " SAVE "
                + std::to_string(hud.performanceAvoidedDrawCalls),
            "UNIFORM UP " + std::to_string(hud.performanceUniformUploads)
                + " SKIP " + std::to_string(hud.performanceUniformSkips),
            "TEXTURE BIND " + std::to_string(hud.performanceTextureBinds)
                + " SKIP " + std::to_string(hud.performanceTextureBindSkips)
                + " UNIT "
                + std::to_string(hud.performanceTextureUnitActivations),
            "MESH BIND " + std::to_string(hud.performanceMeshBinds)
                + " SKIP " + std::to_string(hud.performanceMeshBindSkips),
            "CACHE M " + std::to_string(hud.performanceMeshCacheEntries)
                + " T " + std::to_string(hud.performanceTextureCacheEntries)
                + " LOAD " + std::to_string(hud.worldGenerationMilliseconds)
                + "/" + std::to_string(hud.assetPreparationMilliseconds)
                + "MS",
        };
        constexpr std::string_view title = "F3 PERFORMANCE";
        int textWidth = bitmapTextWidth(title, 1);
        for (const std::string& line : lines) {
            textWidth = std::max(textWidth, bitmapTextWidth(line, 1));
        }
        const int performancePadding = 4;
        const int lineHeight = 9;
        const int panelWidth = std::min(
            std::max(1, width - (hudMargin * 2)),
            textWidth + (performancePadding * 2));
        const int panelHeight = (static_cast<int>(lines.size()) + 1)
            * lineHeight + (performancePadding * 2);
        performancePanelHeight = panelHeight;
        const int panelX = hudMargin;
        const int panelY = std::max(
            hudMargin, height - hudMargin - topCornerOffset - panelHeight);
        drawIndustrialPanel(
            {panelX, panelY, panelWidth, panelHeight}, 1, performanceColor);
        int textY = panelY + panelHeight - performancePadding - 7;
        drawBitmapText(panelX + performancePadding, textY, 1,
            title, performanceColor);
        for (const std::string& line : lines) {
            textY -= lineHeight;
            drawBitmapText(panelX + performancePadding, textY, 1,
                line, textColor);
        }
    }

    if (hud.visible && hud.map.visible && hud.map.roomCount > 0U
        && (!hud.performanceOverlay || height >= 520)) {
        scaleGameplayHud = true;
        hudAnchorX = 0;
        hudAnchorY = height;
        const int mapSize = std::max(24,
            std::min({76 * hudScale, width / 3, height / 3}));
        const int mapWidth = mapSize + (hudPadding * 2);
        const int mapHeight = mapSize + (hudPadding * 2) + (10 * hudScale);
        const int performanceHeight = hud.performanceOverlay
            ? performancePanelHeight + (3 * hudScale) : 0;
        const int mapX = hudMargin;
        const int mapY = height - hudMargin - topCornerOffset
            - mapHeight - performanceHeight;
        const int minimumMapY = hudMargin + (65 * hudScale);
        if (mapY >= minimumMapY && mapWidth + (hudMargin * 2) <= width) {
            mapPanelRect = {mapX, mapY, mapWidth, mapHeight};
            mapPanelVisible = true;
            drawTacticalBackdrop({mapX, mapY, mapWidth, mapHeight},
                hudScale, cyanColor);
            drawBitmapText(mapX + hudPadding,
                mapY + mapHeight - hudPadding - (7 * hudScale),
                hudScale, "MAP", cyanColor);

            float minimumX = hud.map.rooms[0].minimum.x;
            float maximumX = hud.map.rooms[0].maximum.x;
            float minimumZ = hud.map.rooms[0].minimum.y;
            float maximumZ = hud.map.rooms[0].maximum.y;
            const std::size_t roomCount = std::min(
                hud.map.roomCount, hud.map.rooms.size());
            for (std::size_t index = 1U; index < roomCount; ++index) {
                const HudMapRoom& mapRoom = hud.map.rooms[index];
                minimumX = std::min(minimumX, mapRoom.minimum.x);
                maximumX = std::max(maximumX, mapRoom.maximum.x);
                minimumZ = std::min(minimumZ, mapRoom.minimum.y);
                maximumZ = std::max(maximumZ, mapRoom.maximum.y);
            }
            for (std::size_t index = 0U;
                 index < std::min(hud.map.linkCount, hud.map.links.size()); ++index) {
                const HudMapLink& link = hud.map.links[index];
                for (std::size_t point = 0U;
                     point < std::min<std::size_t>(link.pointCount, link.points.size());
                     ++point) {
                    minimumX = std::min(minimumX, link.points[point].x);
                    maximumX = std::max(maximumX, link.points[point].x);
                    minimumZ = std::min(minimumZ, link.points[point].y);
                    maximumZ = std::max(maximumZ, link.points[point].y);
                }
            }
            const float roomSpanX = std::max(1.0F, maximumX - minimumX);
            const float roomSpanZ = std::max(1.0F, maximumZ - minimumZ);
            const int mapInset = 6 * hudScale;
            const float usable = static_cast<float>(std::max(
                1, mapSize - (mapInset * 2)));
            const float mapScale = std::min(usable / roomSpanX,
                usable / roomSpanZ);
            const float offsetX = static_cast<float>(mapX + hudPadding)
                + (static_cast<float>(mapSize) - (roomSpanX * mapScale)) * 0.5F;
            const float offsetY = static_cast<float>(mapY + hudPadding)
                + (static_cast<float>(mapSize) - (roomSpanZ * mapScale)) * 0.5F;
            const auto mapPoint = [&](const wkc::core::Vec2 point) {
                return std::pair{
                    static_cast<int>(std::lround(offsetX
                        + ((point.x - minimumX) * mapScale))),
                    static_cast<int>(std::lround(offsetY
                        + ((maximumZ - point.y) * mapScale))),
                };
            };
            const int lineThickness = std::max(1, hudScale);
            const wkc::core::Vec3 routeColor{0.22F, 0.43F, 0.48F};
            for (std::size_t index = 0U;
                 index < std::min(hud.map.linkCount, hud.map.links.size()); ++index) {
                const HudMapLink& link = hud.map.links[index];
                if (!link.known || link.first >= roomCount || link.second >= roomCount) {
                    continue;
                }
                const auto drawMapSegment = [&](const wkc::core::Vec2 first,
                                                const wkc::core::Vec2 second) {
                    const auto [firstX, firstY] = mapPoint(first);
                    const auto [secondX, secondY] = mapPoint(second);
                    if (firstY == secondY) {
                        drawSolidRect(std::min(firstX, secondX), firstY,
                            std::abs(secondX - firstX) + lineThickness,
                            lineThickness, routeColor);
                    } else if (firstX == secondX) {
                        drawSolidRect(firstX, std::min(firstY, secondY),
                            lineThickness,
                            std::abs(secondY - firstY) + lineThickness,
                            routeColor);
                    }
                };
                for (std::size_t point = 1U;
                     point < std::min<std::size_t>(link.pointCount, link.points.size());
                     ++point) {
                    drawMapSegment(link.points[point - 1U], link.points[point]);
                }
                if (link.pointCount >= 2U) {
                    // Short strokes inside each room attach its icon to the
                    // real connector-to-connector corridor polyline.
                    const auto joinRoom = [&](const wkc::core::Vec2 center,
                                              const wkc::core::Vec2 door) {
                        drawMapSegment(center, {door.x, center.y});
                        drawMapSegment({door.x, center.y}, door);
                    };
                    joinRoom(hud.map.rooms[link.first].center, link.points[0]);
                    joinRoom(hud.map.rooms[link.second].center,
                        link.points[link.pointCount - 1U]);
                }
            }
            for (std::size_t index = 0U; index < roomCount; ++index) {
                const HudMapRoom& mapRoom = hud.map.rooms[index];
                if (!mapRoom.known) {
                    continue;
                }
                const auto [left, top] = mapPoint(
                    {mapRoom.minimum.x, mapRoom.maximum.y});
                const auto [right, bottom] = mapPoint(
                    {mapRoom.maximum.x, mapRoom.minimum.y});
                const wkc::core::Vec3 roomColor = mapRoom.current ? cyanColor
                    : mapRoom.visited ? wkc::core::Vec3{0.46F, 0.66F, 0.70F}
                    : wkc::core::Vec3{0.23F, 0.34F, 0.37F};
                drawSolidRect(std::min(left, right), std::min(top, bottom),
                    std::max(1, std::abs(right - left) + 1),
                    std::max(1, std::abs(bottom - top) + 1), roomColor);
                if (mapRoom.marker != HudMapMarker::None) {
                    const auto [markerX, markerY] = mapPoint(mapRoom.center);
                    const int half = 2 * hudScale;
                    if (mapRoom.marker == HudMapMarker::Mission) {
                        drawSolidRect(markerX - half, markerY - half,
                            2 * half + 1, 2 * half + 1, uiTheme.warning);
                    } else if (mapRoom.marker == HudMapMarker::Boss) {
                        drawSolidRect(markerX - half, markerY - hudScale / 2,
                            2 * half + 1, hudScale, uiTheme.danger);
                        drawSolidRect(markerX - hudScale / 2, markerY - half,
                            hudScale, 2 * half + 1, uiTheme.danger);
                    } else if (mapRoom.marker == HudMapMarker::Exit) {
                        drawSolidRect(markerX - half, markerY - half,
                            2 * half + 1, 2 * half + 1, uiTheme.success);
                        drawSolidRect(markerX - half + hudScale,
                            markerY - half + hudScale,
                            2 * half + 1 - 2 * hudScale,
                            2 * half + 1 - 2 * hudScale, roomColor);
                    }
                }
            }
            const auto [playerX, playerY] = mapPoint(hud.map.playerPosition);
            drawSolidRect(playerX - hudScale, playerY - hudScale,
                2 * hudScale, 2 * hudScale, textColor);
            const int headingX = playerX + static_cast<int>(std::lround(
                hud.map.playerForward.x * static_cast<float>(5 * hudScale)));
            const int headingY = playerY - static_cast<int>(std::lround(
                hud.map.playerForward.y * static_cast<float>(5 * hudScale)));
            drawSolidRect(headingX - hudScale / 2, headingY - hudScale / 2,
                hudScale, hudScale, uiTheme.warning);
        }
    }

    drawInfoOverlay();

    std::string_view objectiveText;
    wkc::core::Vec3 objectiveColor = textColor;
    if (hud.objective == ObjectiveRenderState::ReachBoss) {
        objectiveText = "REACH BOSS";
    } else if (hud.objective == ObjectiveRenderState::ActivateExit) {
        objectiveText = "ACTIVATE EXIT";
    } else if (hud.objective == ObjectiveRenderState::SecureArena) {
        objectiveText = "SECURE ARENA";
    } else if (hud.objective == ObjectiveRenderState::RecoverKey) {
        objectiveText = "RECOVER KEY";
    } else if (hud.objective == ObjectiveRenderState::ActivateTerminal) {
        objectiveText = "ACTIVATE TERMINAL";
    } else if (hud.objective == ObjectiveRenderState::ClearAmbush) {
        objectiveText = "CLEAR AMBUSH";
    } else if (hud.objective == ObjectiveRenderState::TestArena) {
        objectiveText = "TEST ARENA";
    } else if (hud.objective == ObjectiveRenderState::DefeatBoss) {
        objectiveText = "DEFEAT BOSS";
        objectiveColor = uiTheme.warning;
    } else if (hud.objective == ObjectiveRenderState::ClearBossEncounter) {
        objectiveText = "DEFEAT REMAINING ENEMIES";
        objectiveColor = uiTheme.warning;
    }
    std::string timerText;
    std::string bestText;
    std::string bossHealthText;
    std::string_view bossPhaseText;
    if (showBoss) {
        bossHealthText = "BOSS " + std::to_string(
            static_cast<int>(std::ceil(bossHealthRatio * 100.0F))) + "%";
        switch (hud.boss.phase) {
        case HudBossPhase::LongRange:
            bossPhaseText = "PHASE 1/3 RANGE";
            break;
        case HudBossPhase::Pressure:
            bossPhaseText = "PHASE 2/3 PRESSURE";
            break;
        case HudBossPhase::Pursuit:
            bossPhaseText = "PHASE 3/3 PURSUIT";
            break;
        }
    }
    if (hud.visible && hud.runTimerVisible) {
        const std::string timerPrefix = hud.speedrunMode
            ? "SPEEDRUN "
            : hud.campaignMode
                ? "FLOOR " + std::to_string(hud.campaignFloor) + "/"
                    + std::to_string(hud.campaignFloorCount) + "  THREAT "
                    + std::to_string(hud.campaignThreatPercent) + "%  TIME "
                : "TIME ";
        timerText = timerPrefix + runTimeText(hud.elapsedRunTicks);
        if (hud.speedrunMode && hud.speedrunRecordVisible) {
            bestText = "BEST " + runTimeText(hud.bestSpeedrunTicks);
        }
    }
    scaleGameplayHud = hud.visible;
    hudAnchorX = width;
    hudAnchorY = height;
    if (topRightLines > 0) {
        const int topRightWidth = std::max({
            bitmapTextWidth("OBJECTIVE", hudScale),
            bitmapTextWidth(objectiveText, hudScale),
            bitmapTextWidth(timerText, hudScale),
            bitmapTextWidth(bestText, hudScale),
        }) + (hudPadding * 2);
        const int topRightX = std::max(hudMargin,
            width - hudMargin - topRightWidth);
        const int topRightY = std::max(hudMargin,
            height - hudMargin - topCornerOffset - topRightHeight);
        drawTacticalBackdrop({topRightX, topRightY, topRightWidth,
            topRightHeight}, hudScale, objectiveColor);
        int lineY = topRightY + topRightHeight - hudPadding - (7 * hudScale);
        const int textX = topRightX + hudPadding;
        if (!objectiveText.empty()) {
            drawBitmapText(textX, lineY, hudScale, "OBJECTIVE",
                uiTheme.mutedText);
            lineY -= hudLineHeight;
            drawBitmapText(textX, lineY, hudScale, objectiveText,
                objectiveColor);
            lineY -= hudLineHeight;
        }
        if (!timerText.empty()) {
            drawBitmapText(textX, lineY, hudScale, timerText, textColor);
            lineY -= hudLineHeight;
        }
        if (!bestText.empty()) {
            drawBitmapText(textX, lineY, hudScale, bestText,
                uiTheme.warning);
            lineY -= hudLineHeight;
        }
    }
    if (showBoss) {
        hudAnchorX = width / 2;
        const int bossX = (width - bossPanelWidth) / 2;
        const int bossY = height - hudMargin - bossPanelHeight;
        const int textX = bossX + hudPadding;
        const int textY = bossY + bossPanelHeight - hudPadding - (7 * hudScale);
        drawBitmapText(textX, textY, hudScale, bossHealthText, textColor);
        const int phaseX = bossTwoLines ? textX
            : bossX + bossPanelWidth - hudPadding - bitmapTextWidth(bossPhaseText, hudScale);
        drawBitmapText(phaseX, textY - (bossTwoLines ? hudLineHeight : 0),
            hudScale, bossPhaseText, uiTheme.warning);
        const int barWidth = bossPanelWidth - (hudPadding * 2);
        const int filledWidth = std::max(1, static_cast<int>(std::round(
            static_cast<float>(barWidth) * bossHealthRatio)));
        drawSolidRect(textX, bossY + hudPadding, barWidth,
            4 * hudScale, uiTheme.panelBorder);
        drawSolidRect(textX, bossY + hudPadding, filledWidth,
            4 * hudScale, uiTheme.warning);
    }
    scaleGameplayHud = false;
    if (hud.objective == ObjectiveRenderState::Victory
        || hud.objective == ObjectiveRenderState::Defeat) {
        const bool victory = hud.objective == ObjectiveRenderState::Victory;
        const std::string_view result = hud.campaignAdvanceAvailable
            ? "FLOOR CLEAR"
            : victory && hud.campaignMode ? "CAMPAIGN CLEAR"
            : victory ? "VICTORY" : "DEFEAT";
        const ResultMenuLayout layout = makeResultMenuLayout(width, height);
        const int scale = layout.scale;
        const wkc::core::Vec3 resultColor = victory
            ? uiTheme.success : uiTheme.danger;
        drawMenuPanel(layout.panel, scale, resultColor);
        drawBitmapText((width - bitmapTextWidth(result, 2 * scale)) / 2,
            layout.panel.y + layout.panel.height - (24 * scale),
            2 * scale, result, resultColor);

        if (hud.speedrunMode && hud.speedrunRecordVisible
            && layout.panel.height >= 140 * scale) {
            const std::string recordText
                = (hud.newSpeedrunRecord ? "NEW RECORD " : "BEST ")
                + runTimeText(hud.bestSpeedrunTicks);
            const wkc::core::Vec3 recordColor = hud.newSpeedrunRecord
                ? wkc::core::Vec3{0.98F, 0.78F, 0.16F}
                : wkc::core::Vec3{0.64F, 0.78F, 0.82F};
            drawBitmapText(
                (width - bitmapTextWidth(recordText, scale)) / 2,
                layout.panel.y + (104 * scale), scale,
                recordText, recordColor);
        }

        const std::array<std::string_view, 4> options{
            hud.campaignAdvanceAvailable
                ? "NEXT FLOOR" : victory ? "NEW RUN" : "NEXT SEED",
            hud.campaignMode ? "REPLAY FLOOR" : "REPLAY SEED",
            "MAIN MENU", "EXIT GAME",
        };
        const std::array<UiRect, 4> buttons{
            layout.playAgainButton, layout.replaySeedButton,
            layout.mainMenuButton, layout.exitButton,
        };
        std::size_t selected = 0U;
        if (hud.resultMenuSelection == HudResultMenuSelection::ReplaySeed) {
            selected = 1U;
        } else if (hud.resultMenuSelection == HudResultMenuSelection::MainMenu) {
            selected = 2U;
        } else if (hud.resultMenuSelection
            == HudResultMenuSelection::ExitGame) {
            selected = 3U;
        }
        for (std::size_t index = 0; index < buttons.size(); ++index) {
            const UiRect button = buttons[index];
            if (index == selected) {
                drawSelectedRow(button, scale, resultColor);
            }
            const int optionWidth = bitmapTextWidth(options[index], scale);
            drawBitmapText((width - optionWidth) / 2,
                button.y + (3 * scale), scale, options[index],
                index == selected ? resultColor : textColor);
        }
        const std::string_view controls = width >= 480
            ? "W S  ENTER  MOUSE  ESC"
            : "W S  ENTER  MOUSE";
        drawBitmapText((width - bitmapTextWidth(controls, 1)) / 2,
            layout.panel.y + (7 * scale), 1, controls,
            uiTheme.mutedText);
    }
    if (hud.paused) {
        const PauseMenuLayout layout = makePauseMenuLayout(width, height,
            hud.uiScalePercent);
        const float scale = layout.textScale;
        drawMenuPanel(layout.panel, layout.scale, textColor);
        constexpr std::string_view pauseTitle = "PAUSED";
        const int titleWidth = bitmapTextWidth(pauseTitle, scale);
        drawBitmapText((width - titleWidth) / 2,
            layout.panel.y + layout.panel.height - static_cast<int>(14.0F * scale), scale,
            pauseTitle, textColor);

        constexpr std::array<std::string_view, 4> options{
            "RESUME GAME", "RESTART RUN", "RETURN TO MAIN MENU", "CONFIGURATION",
        };
        std::size_t selected = 0U;
        if (hud.pauseSelection == HudPauseSelection::RestartRun) {
            selected = 1U;
        } else if (hud.pauseSelection == HudPauseSelection::MainMenu) {
            selected = 2U;
        } else if (hud.pauseSelection == HudPauseSelection::Settings) {
            selected = 3U;
        }
        const std::array<UiRect, 4> buttons{
            layout.resumeButton, layout.restartButton,
            layout.mainMenuButton, layout.settingsButton,
        };
        for (std::size_t index = 0; index < options.size(); ++index) {
            const UiRect button = buttons[index];
            const int optionWidth = bitmapTextWidth(options[index], scale);
            if (index == selected) {
                drawSelectedRow(button, scale, textColor);
            }
            drawBitmapText((width - optionWidth) / 2,
                button.y + static_cast<int>(2.0F * scale), scale, options[index],
                textColor);
        }
        const std::string_view controls = width >= 480
            ? "W S  ENTER  MOUSE  ESC"
            : "W S  ENTER  MOUSE";
        const float footerScale = std::max(1.0F, scale / 2.0F);
        const int controlsWidth = bitmapTextWidth(controls, footerScale);
        drawBitmapText((width - controlsWidth) / 2,
            layout.panel.y + static_cast<int>(6.0F * scale),
            footerScale, controls, uiTheme.mutedText);
    } else if (scopeStrength == 0.0F
        && (weaponViewmodel.visible ? weaponViewmodel.role : hud.weapon) != HudWeaponRole::Sniper) {
        glClearColor(0.78F, 0.88F, 0.90F, 1.0F);
        const int centerX = width / 2;
        const int centerY = height / 2;
        const int armLength = std::max(1, static_cast<int>(std::lround(
            7.0F * gameplayHudScale)));
        const int thickness = std::max(1, static_cast<int>(std::lround(
            3.0F * gameplayHudScale)));
        const int spreadGap = static_cast<int>(std::lround(
            std::clamp(hud.weaponSpread, 0.0F, 1.0F)
            * 5.0F * static_cast<float>(hudScale) * gameplayHudScale));
        glScissor(centerX - spreadGap - armLength, centerY - thickness / 2,
            armLength, thickness);
        glClear(GL_COLOR_BUFFER_BIT);
        glScissor(centerX + spreadGap + 1, centerY - thickness / 2,
            armLength, thickness);
        glClear(GL_COLOR_BUFFER_BIT);
        glScissor(centerX - thickness / 2, centerY - spreadGap - armLength,
            thickness, armLength);
        glClear(GL_COLOR_BUFFER_BIT);
        glScissor(centerX - thickness / 2, centerY + spreadGap + 1,
            thickness, armLength);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    drawExitConfirmation();
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::ostringstream message;
        message << "OpenGL render error: 0x" << std::hex << error;
        wkc::core::log(wkc::core::LogLevel::Error, message.str());
        return false;
    }
    return true;
}

} // namespace gdp::game::visual
