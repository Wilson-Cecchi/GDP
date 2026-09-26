#include "game/visual/WeaponViewmodelAssets.hpp"

#include "core/Math.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <utility>

namespace gdp::game::visual {
namespace {

using wkc::core::Vec3;
using wkc::render::MeshData;
using wkc::render::Vertex;

constexpr std::size_t indexOf(const WeaponMaterial material) noexcept
{
    return static_cast<std::size_t>(material);
}

class WeaponMeshBuilder {
public:
    void box(const WeaponMaterial material, const Vec3 center,
        const Vec3 size, const float tiltDegrees = 0.0F)
    {
        MeshData& destination = meshes_[indexOf(material)];
        const MeshData source = wkc::render::makeBoxMesh();
        const float tilt = tiltDegrees * std::numbers::pi_v<float> / 180.0F;
        const float sine = std::sin(tilt);
        const float cosine = std::cos(tilt);
        const std::uint32_t first = static_cast<std::uint32_t>(
            destination.vertices.size());
        for (Vertex vertex : source.vertices) {
            const Vec3 scaled{vertex.position.x * size.x,
                vertex.position.y * size.y, vertex.position.z * size.z};
            vertex.position = {center.x + scaled.x,
                center.y + scaled.y * cosine - scaled.z * sine,
                center.z + scaled.y * sine + scaled.z * cosine};
            const Vec3 normal = vertex.normal;
            vertex.normal = {normal.x,
                normal.y * cosine - normal.z * sine,
                normal.y * sine + normal.z * cosine};
            destination.vertices.push_back(vertex);
        }
        for (const std::uint32_t index : source.indices) {
            destination.indices.push_back(first + index);
        }
    }

    void cylinder(const WeaponMaterial material, const Vec3 center,
        const float radiusX, const float radiusY,
        const float fromZ, const float toZ)
    {
        MeshData& mesh = meshes_[indexOf(material)];
        constexpr std::uint32_t sides = 12U;
        const std::uint32_t first = static_cast<std::uint32_t>(
            mesh.vertices.size());
        for (std::uint32_t side = 0; side <= sides; ++side) {
            const float angle = static_cast<float>(side)
                * (2.0F * std::numbers::pi_v<float> / static_cast<float>(sides));
            const float x = std::cos(angle);
            const float y = std::sin(angle);
            const Vec3 normal = wkc::core::normalized(
                {x / radiusX, y / radiusY, 0.0F});
            const float u = static_cast<float>(side) / static_cast<float>(sides);
            mesh.vertices.push_back({
                {center.x + radiusX * x, center.y + radiusY * y, fromZ},
                normal, {u, 0.0F}, 0U});
            mesh.vertices.push_back({
                {center.x + radiusX * x, center.y + radiusY * y, toZ},
                normal, {u, 1.0F}, 0U});
        }
        for (std::uint32_t side = 0; side < sides; ++side) {
            const std::uint32_t lower = first + side * 2U;
            mesh.indices.insert(mesh.indices.end(), {
                lower, lower + 2U, lower + 1U,
                lower + 1U, lower + 2U, lower + 3U});
        }
        const std::uint32_t cap = static_cast<std::uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back({{center.x, center.y, toZ},
            {0.0F, 0.0F, 1.0F}, {0.5F, 0.5F}, 0U});
        mesh.vertices.push_back({{center.x, center.y, fromZ},
            {0.0F, 0.0F, -1.0F}, {0.5F, 0.5F}, 0U});
        for (std::uint32_t side = 0; side < sides; ++side) {
            const std::uint32_t next = (side + 1U) % sides;
            mesh.indices.insert(mesh.indices.end(), {
                cap, first + side * 2U + 1U, first + next * 2U + 1U,
                cap + 1U, first + next * 2U, first + side * 2U});
        }
    }

    void beveledPrism(const WeaponMaterial material, const Vec3 center,
        const Vec3 size, const float bevel)
    {
        MeshData& mesh = meshes_[indexOf(material)];
        const float halfX = size.x * 0.5F;
        const float halfY = size.y * 0.5F;
        const float front = center.z + size.z * 0.5F;
        const float back = center.z - size.z * 0.5F;
        const std::array outline{
            wkc::core::Vec2{halfX - bevel, halfY},
            wkc::core::Vec2{-halfX + bevel, halfY},
            wkc::core::Vec2{-halfX, halfY - bevel},
            wkc::core::Vec2{-halfX, -halfY + bevel},
            wkc::core::Vec2{-halfX + bevel, -halfY},
            wkc::core::Vec2{halfX - bevel, -halfY},
            wkc::core::Vec2{halfX, -halfY + bevel},
            wkc::core::Vec2{halfX, halfY - bevel},
        };
        for (std::size_t side = 0; side < outline.size(); ++side) {
            const auto a = outline[side];
            const auto b = outline[(side + 1U) % outline.size()];
            const Vec3 lowerA{center.x + a.x, center.y + a.y, back};
            const Vec3 lowerB{center.x + b.x, center.y + b.y, back};
            const Vec3 upperA{center.x + a.x, center.y + a.y, front};
            const Vec3 upperB{center.x + b.x, center.y + b.y, front};
            const Vec3 normal = wkc::core::normalized(
                wkc::core::cross(lowerB - lowerA, upperA - lowerA));
            const std::uint32_t first = static_cast<std::uint32_t>(
                mesh.vertices.size());
            mesh.vertices.insert(mesh.vertices.end(), {
                {lowerA, normal, {0.0F, 0.0F}, 0U},
                {lowerB, normal, {1.0F, 0.0F}, 0U},
                {upperA, normal, {0.0F, 1.0F}, 0U},
                {upperB, normal, {1.0F, 1.0F}, 0U},
            });
            mesh.indices.insert(mesh.indices.end(), {
                first, first + 1U, first + 2U,
                first + 1U, first + 3U, first + 2U});
            const std::uint32_t caps = static_cast<std::uint32_t>(
                mesh.vertices.size());
            mesh.vertices.insert(mesh.vertices.end(), {
                {{center.x, center.y, front}, {0.0F, 0.0F, 1.0F},
                    {0.5F, 0.5F}, 0U},
                {upperA, {0.0F, 0.0F, 1.0F}, {0.0F, 0.0F}, 0U},
                {upperB, {0.0F, 0.0F, 1.0F}, {1.0F, 0.0F}, 0U},
                {{center.x, center.y, back}, {0.0F, 0.0F, -1.0F},
                    {0.5F, 0.5F}, 0U},
                {lowerB, {0.0F, 0.0F, -1.0F}, {1.0F, 0.0F}, 0U},
                {lowerA, {0.0F, 0.0F, -1.0F}, {0.0F, 0.0F}, 0U},
            });
            mesh.indices.insert(mesh.indices.end(), {
                caps, caps + 1U, caps + 2U,
                caps + 3U, caps + 4U, caps + 5U});
        }
    }

    void blade()
    {
        MeshData& mesh = meshes_[indexOf(WeaponMaterial::BareSteel)];
        struct Section {
            float z;
            float width;
            float thickness;
        };
        constexpr std::array sections{
            Section{0.12F, 0.065F, 0.035F},
            Section{0.21F, 0.105F, 0.033F},
            Section{0.39F, 0.105F, 0.028F},
            Section{0.51F, 0.073F, 0.020F},
            Section{0.62F, 0.003F, 0.002F},
        };
        // Diamond sections give both the edge and central ridge real depth.
        for (std::size_t section = 0; section + 1U < sections.size(); ++section) {
            const auto ring = [](const Section value, const std::size_t corner)
                -> Vec3 {
                switch (corner) {
                case 0U: return {-value.width, 0.015F, value.z};
                case 1U: return {0.0F, 0.015F + value.thickness, value.z};
                case 2U: return {value.width, 0.015F, value.z};
                default: return {0.0F, 0.015F - value.thickness, value.z};
                }
            };
            for (std::size_t corner = 0; corner < 4U; ++corner) {
                const Vec3 a = ring(sections[section], corner);
                const Vec3 b = ring(sections[section], (corner + 1U) % 4U);
                const Vec3 c = ring(sections[section + 1U], corner);
                const Vec3 d = ring(sections[section + 1U], (corner + 1U) % 4U);
                const Vec3 normal = wkc::core::normalized(
                    wkc::core::cross(c - a, b - a));
                const std::uint32_t first = static_cast<std::uint32_t>(
                    mesh.vertices.size());
                mesh.vertices.insert(mesh.vertices.end(), {
                    {a, normal, {0.0F, 0.0F}, 0U},
                    {b, normal, {1.0F, 0.0F}, 0U},
                    {c, normal, {0.0F, 1.0F}, 0U},
                    {d, normal, {1.0F, 1.0F}, 0U},
                });
                mesh.indices.insert(mesh.indices.end(), {
                    first, first + 2U, first + 1U,
                    first + 1U, first + 2U, first + 3U});
            }
        }
    }

    [[nodiscard]] std::array<MeshData, weaponMaterialCount> finish()
    {
        for (MeshData& mesh : meshes_) {
            if (mesh.vertices.empty()) {
                continue;
            }
            Vec3 minimum{std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max()};
            Vec3 maximum{-std::numeric_limits<float>::max(),
                -std::numeric_limits<float>::max(),
                -std::numeric_limits<float>::max()};
            for (const Vertex& vertex : mesh.vertices) {
                minimum.x = std::min(minimum.x, vertex.position.x);
                minimum.y = std::min(minimum.y, vertex.position.y);
                minimum.z = std::min(minimum.z, vertex.position.z);
                maximum.x = std::max(maximum.x, vertex.position.x);
                maximum.y = std::max(maximum.y, vertex.position.y);
                maximum.z = std::max(maximum.z, vertex.position.z);
            }
            mesh.bounds = {minimum, maximum};
            // A blade's rear bevel and several inset details are open surfaces.
            mesh.backfaceCullingSafe = false;
        }
        return std::move(meshes_);
    }

private:
    std::array<MeshData, weaponMaterialCount> meshes_{};
};

void makePistol(WeaponMeshBuilder& mesh)
{
    using enum WeaponMaterial;
    mesh.box(Polymer, {0.0F, -0.052F, -0.02F}, {0.18F, 0.115F, 0.37F});
    mesh.box(Polymer, {0.0F, -0.205F, -0.115F},
        {0.125F, 0.29F, 0.15F}, -14.0F);
    mesh.box(Recess, {0.0F, -0.355F, -0.072F}, {0.13F, 0.025F, 0.17F});
    mesh.beveledPrism(CoatedSteel, {0.0F, 0.055F, 0.015F},
        {0.205F, 0.125F, 0.55F}, 0.022F);
    mesh.box(BareSteel, {0.0F, 0.115F, 0.018F}, {0.153F, 0.008F, 0.44F});
    mesh.box(Recess, {0.103F, 0.095F, 0.07F}, {0.006F, 0.047F, 0.105F});
    mesh.box(BareSteel, {0.108F, 0.102F, 0.075F}, {0.007F, 0.009F, 0.076F});
    mesh.cylinder(Recess, {0.0F, 0.055F, 0.0F}, 0.046F, 0.046F, 0.288F, 0.323F);
    mesh.cylinder(BareSteel, {0.0F, 0.055F, 0.0F}, 0.035F, 0.035F, 0.246F, 0.296F);
    mesh.box(Polymer, {0.0F, -0.145F, 0.04F}, {0.025F, 0.026F, 0.17F});
    mesh.box(Polymer, {0.0F, -0.20F, 0.11F}, {0.025F, 0.105F, 0.024F});
    mesh.box(Polymer, {0.0F, -0.25F, 0.033F}, {0.025F, 0.022F, 0.18F});
    mesh.box(CoatedSteel, {0.0F, -0.17F, 0.022F}, {0.017F, 0.078F, 0.018F}, -17.0F);
    for (int groove = 0; groove < 5; ++groove) {
        mesh.box(Recess, {0.104F, 0.072F,
                -0.20F + 0.015F * static_cast<float>(groove)},
            {0.007F, 0.051F, 0.005F});
        mesh.box(Recess, {0.104F, 0.072F,
                0.18F + 0.015F * static_cast<float>(groove)},
            {0.007F, 0.051F, 0.005F});
    }
    for (int row = 0; row < 5; ++row) {
        mesh.box(Recess, {0.065F,
                -0.18F - static_cast<float>(row) * 0.035F, -0.085F},
            {0.004F, 0.008F, 0.095F});
    }
    mesh.box(Recess, {0.0F, 0.13F, -0.215F}, {0.085F, 0.014F, 0.038F});
    mesh.box(Recess, {-0.034F, 0.155F, -0.215F},
        {0.017F, 0.045F, 0.032F});
    mesh.box(Recess, {0.034F, 0.155F, -0.215F},
        {0.017F, 0.045F, 0.032F});
    mesh.box(Recess, {0.0F, 0.134F, 0.247F}, {0.043F, 0.014F, 0.023F});
    mesh.box(Recess, {0.0F, 0.152F, 0.247F}, {0.016F, 0.032F, 0.018F});
    mesh.box(Sight, {0.0F, 0.171F, 0.247F}, {0.014F, 0.006F, 0.007F});
}

void makeCarbine(WeaponMeshBuilder& mesh)
{
    using enum WeaponMaterial;
    mesh.beveledPrism(CoatedSteel, {0.0F, 0.025F, 0.005F},
        {0.245F, 0.20F, 0.53F}, 0.024F);
    mesh.box(BareSteel, {0.0F, 0.128F, 0.005F}, {0.20F, 0.009F, 0.49F});
    mesh.beveledPrism(Polymer, {0.0F, 0.012F, 0.53F},
        {0.22F, 0.175F, 0.55F}, 0.024F);
    mesh.beveledPrism(Polymer, {0.0F, -0.018F, -0.44F},
        {0.19F, 0.135F, 0.39F}, 0.018F);
    mesh.box(Polymer, {0.0F, -0.035F, -0.655F}, {0.225F, 0.20F, 0.07F});
    mesh.box(Recess, {0.0F, -0.035F, -0.697F}, {0.218F, 0.20F, 0.019F});
    mesh.box(Polymer, {0.0F, -0.185F, -0.095F},
        {0.13F, 0.275F, 0.155F}, -11.0F);
    mesh.box(Recess, {0.0F, -0.16F, 0.015F}, {0.025F, 0.025F, 0.185F});
    mesh.box(Recess, {0.0F, -0.222F, 0.093F}, {0.025F, 0.105F, 0.026F});
    mesh.box(Recess, {0.0F, -0.27F, 0.025F}, {0.025F, 0.021F, 0.14F});
    mesh.box(CoatedSteel, {0.0F, -0.185F, 0.225F},
        {0.14F, 0.34F, 0.115F}, -8.0F);
    mesh.box(Polymer, {0.0F, -0.34F, 0.268F}, {0.153F, 0.055F, 0.13F});
    mesh.box(Recess, {0.127F, 0.047F, -0.04F}, {0.009F, 0.055F, 0.12F});
    mesh.box(BareSteel, {0.134F, 0.06F, -0.036F}, {0.005F, 0.013F, 0.09F});
    mesh.cylinder(BareSteel, {0.0F, 0.0F, 0.0F}, 0.046F, 0.046F, 0.75F, 1.0F);
    mesh.cylinder(Recess, {0.0F, 0.0F, 0.0F}, 0.064F, 0.064F, 0.99F, 1.075F);
    mesh.cylinder(BareSteel, {0.0F, 0.0F, 0.0F}, 0.052F, 0.052F, 1.03F, 1.07F);
    mesh.box(CoatedSteel, {0.0F, 0.125F, 0.39F}, {0.12F, 0.02F, 0.82F});
    for (int notch = 0; notch < 11; ++notch) {
        mesh.box(Recess, {0.0F, 0.139F,
                0.04F + 0.068F * static_cast<float>(notch)},
            {0.105F, 0.008F, 0.012F});
    }
    for (int slot = 0; slot < 5; ++slot) {
        const float z = 0.34F + 0.085F * static_cast<float>(slot);
        mesh.box(Recess, {0.113F, 0.032F, z},
            {0.008F, 0.034F, 0.058F});
        mesh.box(Recess, {-0.113F, 0.032F, z},
            {0.008F, 0.034F, 0.058F});
    }
    for (int rib = 0; rib < 5; ++rib) {
        mesh.box(Recess, {0.073F,
                -0.20F - static_cast<float>(rib) * 0.036F, 0.24F},
            {0.006F, 0.012F, 0.1F});
    }
    mesh.box(Recess, {0.0F, 0.145F, -0.205F}, {0.095F, 0.018F, 0.045F});
    mesh.box(Recess, {-0.037F, 0.185F, -0.205F},
        {0.018F, 0.07F, 0.032F});
    mesh.box(Recess, {0.037F, 0.185F, -0.205F},
        {0.018F, 0.07F, 0.032F});
    mesh.box(Recess, {0.0F, 0.145F, 0.72F}, {0.07F, 0.016F, 0.033F});
    mesh.box(Recess, {0.0F, 0.18F, 0.72F}, {0.018F, 0.067F, 0.025F});
    mesh.box(Sight, {0.0F, 0.216F, 0.72F}, {0.014F, 0.008F, 0.009F});
}

void makeKnife(WeaponMeshBuilder& mesh)
{
    using enum WeaponMaterial;
    mesh.cylinder(Polymer, {0.0F, 0.015F, 0.0F},
        0.064F, 0.048F, -0.35F, 0.125F);
    mesh.box(Recess, {0.0F, 0.015F, -0.357F}, {0.15F, 0.106F, 0.04F});
    mesh.box(CoatedSteel, {0.0F, 0.015F, 0.126F}, {0.255F, 0.073F, 0.055F});
    mesh.box(CoatedSteel, {0.0F, 0.015F, 0.06F}, {0.08F, 0.064F, 0.16F});
    for (int groove = 0; groove < 8; ++groove) {
        mesh.box(Recess, {0.065F, 0.018F,
                -0.29F + static_cast<float>(groove) * 0.05F},
            {0.005F, 0.052F, 0.009F});
    }
    mesh.blade();
    mesh.box(Recess, {0.0F, 0.052F, 0.205F}, {0.02F, 0.004F, 0.085F});
    mesh.box(BareSteel, {0.0F, 0.015F, -0.28F}, {0.11F, 0.072F, 0.011F});
}

std::uint32_t pixelNoise(const int x, const int y, const std::uint32_t seed)
{
    std::uint32_t value = static_cast<std::uint32_t>(x) * 0x9e3779b9U
        ^ static_cast<std::uint32_t>(y) * 0x85ebca6bU ^ seed;
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    return value;
}

} // namespace

std::array<MeshData, weaponMaterialCount> makeWeaponViewmodelMeshes(
    const HudWeaponRole role)
{
    WeaponMeshBuilder builder;
    switch (role) {
    case HudWeaponRole::Pistol: makePistol(builder); break;
    case HudWeaponRole::Carbine: makeCarbine(builder); break;
    case HudWeaponRole::Knife: makeKnife(builder); break;
    }
    return builder.finish();
}

WeaponMaterialImages makeUsedWeaponMaterialImages(
    const WeaponMaterial material)
{
    // Main surfaces receive enough texels for a close-up at 720p; tiny
    // recess/sight pieces use a quarter-size map on low-memory GPUs.
    const int resolution = material == WeaponMaterial::CoatedSteel
        || material == WeaponMaterial::BareSteel
        || material == WeaponMaterial::Polymer ? 512 : 128;
    WeaponMaterialImages images;
    for (wkc::render::ImageData* const image
        : {&images.albedo, &images.normal, &images.properties}) {
        image->width = resolution;
        image->height = resolution;
        image->rgba.resize(static_cast<std::size_t>(resolution * resolution * 4));
    }
    const bool coated = material == WeaponMaterial::CoatedSteel;
    const bool bare = material == WeaponMaterial::BareSteel;
    const bool polymer = material == WeaponMaterial::Polymer;
    const bool recess = material == WeaponMaterial::Recess;
    const int base = coated ? 220 : bare ? 238 : polymer ? 228 : recess ? 235 : 255;
    const int roughness = coated ? 121 : bare ? 88 : polymer ? 210 : 180;
    const int metalness = coated ? 95 : bare ? 184 : polymer ? 0 : recess ? 12 : 18;
    for (int y = 0; y < resolution; ++y) {
        for (int x = 0; x < resolution; ++x) {
            const std::uint32_t noise = pixelNoise(x, y,
                0x76532U + static_cast<std::uint32_t>(material));
            const int grain = static_cast<int>(noise & 7U) - 3;
            const int brush = bare ? ((x * 3 + y) & 7) - 3 : 0;
            const bool contactWear = coated && ((x > resolution - 11
                    || y > resolution - 9)
                && (noise & 15U) < 5U);
            const int tone = std::clamp(base + grain + brush
                + (contactWear ? 17 : 0), 0, 255);
            const std::size_t offset = static_cast<std::size_t>(
                (y * resolution + x) * 4);
            const auto byte = [](const int value) {
                return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
            };
            images.albedo.rgba[offset] = byte(tone);
            images.albedo.rgba[offset + 1U] = byte(tone);
            images.albedo.rgba[offset + 2U] = byte(tone);
            images.albedo.rgba[offset + 3U] = 255U;
            images.normal.rgba[offset] = byte(128 + (grain / 2));
            images.normal.rgba[offset + 1U] = byte(128 + (bare ? brush : grain / 2));
            images.normal.rgba[offset + 2U] = 255U;
            images.normal.rgba[offset + 3U] = 255U;
            images.properties.rgba[offset] = byte(roughness + grain);
            images.properties.rgba[offset + 1U] = byte(metalness);
            images.properties.rgba[offset + 2U] = 245U;
            images.properties.rgba[offset + 3U] = 255U;
        }
    }
    return images;
}

} // namespace gdp::game::visual
