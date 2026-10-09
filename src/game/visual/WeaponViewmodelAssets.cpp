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
    template <typename Build>
    void part(const WeaponPart part, Build&& build)
    {
        std::array<std::size_t, weaponMaterialCount> starts{};
        for (std::size_t i = 0; i < weaponMaterialCount; ++i) starts[i] = meshes_[i].vertices.size();
        build();
        for (std::size_t i = 0; i < weaponMaterialCount; ++i)
            for (std::size_t j = starts[i]; j < meshes_[i].vertices.size(); ++j)
                meshes_[i].vertices[j].semanticRegion = static_cast<std::uint32_t>(part) + 1U;
    }

    struct LoftSection {
        float z;
        float halfWidth;
        float centerY;
        float halfHeight;
        float bevel;
    };

    template <std::size_t count>
    void beveledLoft(const WeaponMaterial material,
        const std::array<LoftSection, count>& sections)
    {
        static_assert(count >= 2U);
        MeshData& mesh = meshes_[indexOf(material)];
        const auto corner = [](const LoftSection& section,
                                const std::size_t index) -> Vec3 {
            const float x = section.halfWidth;
            const float y = section.halfHeight;
            const float b = section.bevel;
            switch (index) {
            case 0U: return {x - b, section.centerY + y, section.z};
            case 1U: return {-x + b, section.centerY + y, section.z};
            case 2U: return {-x, section.centerY + y - b, section.z};
            case 3U: return {-x, section.centerY - y + b, section.z};
            case 4U: return {-x + b, section.centerY - y, section.z};
            case 5U: return {x - b, section.centerY - y, section.z};
            case 6U: return {x, section.centerY - y + b, section.z};
            default: return {x, section.centerY + y - b, section.z};
            }
        };
        for (std::size_t section = 0U; section + 1U < count; ++section) {
            for (std::size_t side = 0U; side < 8U; ++side) {
                const Vec3 a = corner(sections[section], side);
                const Vec3 b = corner(sections[section], (side + 1U) % 8U);
                const Vec3 c = corner(sections[section + 1U], side);
                const Vec3 d = corner(sections[section + 1U], (side + 1U) % 8U);
                const Vec3 normal = wkc::core::normalized(
                    wkc::core::cross(b - a, c - a));
                const std::uint32_t first = static_cast<std::uint32_t>(
                    mesh.vertices.size());
                mesh.vertices.insert(mesh.vertices.end(), {
                    {a, normal, {0.0F, 0.0F}, 0U},
                    {b, normal, {1.0F, 0.0F}, 0U},
                    {c, normal, {0.0F, 1.0F}, 0U},
                    {d, normal, {1.0F, 1.0F}, 0U},
                });
                mesh.indices.insert(mesh.indices.end(), {
                    first, first + 1U, first + 2U,
                    first + 1U, first + 3U, first + 2U});
            }
        }
        for (const std::size_t end : std::array<std::size_t, 2U>{0U, count - 1U}) {
            const float sign = end == 0U ? -1.0F : 1.0F;
            const Vec3 normal{0.0F, 0.0F, sign};
            const std::uint32_t first = static_cast<std::uint32_t>(
                mesh.vertices.size());
            mesh.vertices.push_back({{0.0F, sections[end].centerY,
                sections[end].z}, normal, {0.5F, 0.5F}, 0U});
            for (std::size_t side = 0U; side < 8U; ++side) {
                const Vec3 a = corner(sections[end], side);
                const Vec3 b = corner(sections[end], (side + 1U) % 8U);
                mesh.vertices.insert(mesh.vertices.end(), {
                    {a, normal, {0.0F, 0.0F}, 0U},
                    {b, normal, {1.0F, 0.0F}, 0U},
                });
                mesh.indices.insert(mesh.indices.end(), {
                    first, first + 1U + static_cast<std::uint32_t>(side) * 2U,
                    first + 2U + static_cast<std::uint32_t>(side) * 2U});
            }
        }
    }

    void box(const WeaponMaterial material, const Vec3 center,
        const Vec3 size, const float tiltDegrees = 0.0F,
        const std::uint32_t semanticRegion = 0U)
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
            vertex.semanticRegion = semanticRegion;
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

    void sideDisc(const WeaponMaterial material, const float x,
        const float y, const float z, const float radius)
    {
        MeshData& mesh = meshes_[indexOf(material)];
        constexpr std::uint32_t sides = 10U;
        const float facing = x < 0.0F ? -1.0F : 1.0F;
        const std::uint32_t first = static_cast<std::uint32_t>(
            mesh.vertices.size());
        mesh.vertices.push_back({{x, y, z}, {facing, 0.0F, 0.0F},
            {0.5F, 0.5F}, 0U});
        for (std::uint32_t side = 0; side <= sides; ++side) {
            const float angle = static_cast<float>(side)
                * (2.0F * std::numbers::pi_v<float> / static_cast<float>(sides));
            mesh.vertices.push_back({{x, y + radius * std::sin(angle),
                z + radius * std::cos(angle)}, {facing, 0.0F, 0.0F},
                {0.5F + 0.5F * std::cos(angle),
                    0.5F + 0.5F * std::sin(angle)}, 0U});
            if (side < sides) {
                mesh.indices.insert(mesh.indices.end(), {
                    first, first + side + 1U, first + side + 2U});
            }
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
            float centerX;
            float width;
            float thickness;
        };
        constexpr std::array sections{
            Section{0.12F, 0.0F, 0.065F, 0.035F},
            Section{0.21F, 0.0F, 0.105F, 0.033F},
            Section{0.39F, 0.0F, 0.105F, 0.028F},
            Section{0.49F, -0.014F, 0.080F, 0.021F},
            Section{0.55F, -0.041F, 0.049F, 0.014F},
            Section{0.62F, -0.070F, 0.003F, 0.002F},
        };
        // Offset diamond sections create a clip point, edge and central ridge.
        for (std::size_t section = 0; section + 1U < sections.size(); ++section) {
            const auto ring = [](const Section value, const std::size_t corner)
                -> Vec3 {
                switch (corner) {
                case 0U: return {value.centerX - value.width, 0.015F, value.z};
                case 1U: return {value.centerX, 0.015F + value.thickness, value.z};
                case 2U: return {value.centerX + value.width, 0.015F, value.z};
                default: return {value.centerX, 0.015F - value.thickness, value.z};
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
    mesh.beveledPrism(Polymer, {0.0F, -0.078F, 0.125F},
        {0.17F, 0.086F, 0.20F}, 0.015F);
    mesh.box(Polymer, {0.0F, -0.205F, -0.115F},
        {0.125F, 0.29F, 0.15F}, 22.0F, weaponGripSemanticRegion);
    mesh.part(WeaponPart::Magazine, [&] {
        mesh.box(Recess, {0.0F, -0.355F, -0.072F}, {0.13F, 0.025F, 0.17F});
        mesh.box(CoatedSteel, {0.0F, -0.245F, -0.115F}, {0.105F, 0.22F, 0.11F}, 22.0F);
    });
    mesh.part(WeaponPart::Mechanism, [&] {
        mesh.beveledLoft(CoatedSteel,
            std::array{WeaponMeshBuilder::LoftSection{-0.26F, 0.095F, 0.054F,
                           0.057F, 0.023F},
                WeaponMeshBuilder::LoftSection{-0.205F, 0.1025F, 0.055F,
                    0.0625F, 0.022F},
                WeaponMeshBuilder::LoftSection{0.17F, 0.1025F, 0.055F,
                    0.0625F, 0.022F},
                WeaponMeshBuilder::LoftSection{0.29F, 0.085F, 0.048F,
                    0.053F, 0.019F}});
        mesh.box(BareSteel, {0.0F, 0.115F, 0.018F}, {0.153F, 0.008F, 0.44F});
        mesh.box(Recess, {0.0F, 0.122F, 0.045F}, {0.094F, 0.004F, 0.112F});
        mesh.box(BareSteel, {0.0F, 0.124F, 0.055F}, {0.065F, 0.004F, 0.077F});
    });
    mesh.cylinder(Recess, {0.0F, 0.055F, 0.0F}, 0.046F, 0.046F, 0.288F, 0.323F);
    mesh.cylinder(BareSteel, {0.0F, 0.055F, 0.0F}, 0.035F, 0.035F, 0.246F, 0.296F);
    mesh.box(Polymer, {0.0F, -0.145F, 0.04F}, {0.025F, 0.026F, 0.17F});
    mesh.box(Polymer, {0.0F, -0.20F, 0.11F}, {0.025F, 0.105F, 0.024F});
    mesh.box(Polymer, {0.0F, -0.25F, 0.033F}, {0.025F, 0.022F, 0.18F});
    mesh.box(CoatedSteel, {0.0F, -0.17F, 0.022F}, {0.017F, 0.078F, 0.018F}, -17.0F);
    for (const float facing : {-1.0F, 1.0F}) {
        // Both faces are authored: either can become camera-facing with sway.
        const float slideX = facing * 0.104F;
        mesh.part(WeaponPart::Mechanism, [&] {
            mesh.box(BareSteel, {slideX, 0.014F, -0.012F},
                {0.004F, 0.004F, 0.41F});
            mesh.box(Recess, {slideX, 0.105F, -0.03F},
                {0.004F, 0.004F, 0.31F});
            for (int groove = 0; groove < 6; ++groove) {
                mesh.box(Recess, {slideX, 0.069F,
                        -0.216F + 0.013F * static_cast<float>(groove)},
                    {0.005F, 0.065F, 0.005F}, 14.0F);
                mesh.box(Recess, {slideX, 0.069F,
                        0.172F + 0.013F * static_cast<float>(groove)},
                    {0.005F, 0.065F, 0.005F}, 14.0F);
            }
        });
        mesh.box(Recess, {facing * 0.093F, -0.071F, -0.04F},
            {0.006F, 0.017F, 0.31F});
        mesh.box(BareSteel, {facing * 0.096F, -0.081F, 0.012F},
            {0.01F, 0.012F, 0.031F});
        mesh.sideDisc(BareSteel, facing * 0.099F, -0.072F, -0.155F, 0.013F);
        mesh.sideDisc(Recess, facing * 0.101F, -0.072F, -0.155F, 0.005F);
        mesh.box(Recess, {facing * 0.064F, -0.239F, -0.103F},
            {0.005F, 0.203F, 0.097F}, 22.0F);
        mesh.box(Polymer, {facing * 0.068F, -0.239F, -0.103F},
            {0.005F, 0.183F, 0.084F}, 22.0F);
        for (int row = 0; row < 6; ++row) {
            mesh.box(Recess, {facing * 0.072F,
                    -0.18F - static_cast<float>(row) * 0.026F, -0.085F},
                {0.003F, 0.004F, 0.074F});
        }
        mesh.sideDisc(BareSteel, facing * 0.075F, -0.177F, -0.098F, 0.006F);
        mesh.sideDisc(BareSteel, facing * 0.075F, -0.302F, -0.098F, 0.006F);
    }
    mesh.part(WeaponPart::Mechanism, [&] {
        mesh.box(Recess, {0.0F, 0.13F, -0.215F}, {0.085F, 0.014F, 0.038F});
        mesh.box(Recess, {-0.034F, 0.155F, -0.215F},
            {0.017F, 0.045F, 0.032F});
        mesh.box(Recess, {0.034F, 0.155F, -0.215F},
            {0.017F, 0.045F, 0.032F});
        mesh.box(Recess, {0.0F, 0.134F, 0.247F}, {0.043F, 0.014F, 0.023F});
        mesh.box(Recess, {0.0F, 0.152F, 0.247F}, {0.016F, 0.032F, 0.018F});
        mesh.box(Sight, {0.0F, 0.171F, 0.247F}, {0.014F, 0.006F, 0.007F});
    });
}

void makeCarbine(WeaponMeshBuilder& mesh)
{
    using enum WeaponMaterial;
    mesh.beveledLoft(CoatedSteel,
        std::array{WeaponMeshBuilder::LoftSection{-0.26F, 0.113F, 0.025F,
                       0.091F, 0.023F},
            WeaponMeshBuilder::LoftSection{-0.20F, 0.1225F, 0.025F,
                0.10F, 0.024F},
            WeaponMeshBuilder::LoftSection{0.18F, 0.1225F, 0.025F,
                0.10F, 0.024F},
            WeaponMeshBuilder::LoftSection{0.27F, 0.105F, 0.022F,
                0.082F, 0.02F}});
    mesh.beveledPrism(CoatedSteel, {0.0F, -0.048F, 0.03F},
        {0.215F, 0.085F, 0.47F}, 0.012F);
    mesh.box(BareSteel, {0.0F, 0.128F, 0.005F}, {0.20F, 0.009F, 0.49F});
    mesh.beveledLoft(Polymer,
        std::array{WeaponMeshBuilder::LoftSection{0.27F, 0.11F, 0.012F,
                       0.0875F, 0.024F},
            WeaponMeshBuilder::LoftSection{0.67F, 0.105F, 0.012F,
                0.085F, 0.022F},
            WeaponMeshBuilder::LoftSection{0.805F, 0.086F, 0.009F,
                0.072F, 0.02F}});
    mesh.beveledPrism(CoatedSteel, {0.0F, 0.018F, 0.275F},
        {0.19F, 0.14F, 0.067F}, 0.018F);
    // An open, adjustable stock reads as separate tube, cheek rest and rails
    // instead of the old uninterrupted rectangular block near the camera.
    mesh.cylinder(CoatedSteel, {0.0F, -0.019F, 0.0F},
        0.047F, 0.047F, -0.64F, -0.22F);
    mesh.cylinder(Recess, {0.0F, -0.019F, 0.0F},
        0.055F, 0.055F, -0.62F, -0.59F);
    mesh.beveledLoft(Polymer,
        std::array{WeaponMeshBuilder::LoftSection{-0.63F, 0.085F, 0.066F,
                       0.029F, 0.012F},
            WeaponMeshBuilder::LoftSection{-0.35F, 0.083F, 0.072F,
                0.031F, 0.013F},
            WeaponMeshBuilder::LoftSection{-0.25F, 0.069F, 0.066F,
                0.025F, 0.01F}});
    for (const float facing : {-1.0F, 1.0F}) {
        mesh.box(Polymer, {facing * 0.078F, -0.068F, -0.47F},
            {0.028F, 0.035F, 0.34F});
        mesh.box(BareSteel, {facing * 0.095F, -0.068F, -0.50F},
            {0.005F, 0.006F, 0.16F});
    }
    mesh.box(Polymer, {0.0F, -0.035F, -0.655F}, {0.225F, 0.20F, 0.07F});
    mesh.box(Recess, {0.0F, -0.035F, -0.697F}, {0.218F, 0.20F, 0.019F});
    mesh.box(Polymer, {0.0F, -0.185F, -0.095F},
        {0.13F, 0.275F, 0.155F}, 18.0F, weaponGripSemanticRegion);
    mesh.box(Recess, {0.0F, -0.16F, 0.015F}, {0.025F, 0.025F, 0.185F});
    mesh.box(Recess, {0.0F, -0.222F, 0.093F}, {0.025F, 0.105F, 0.026F});
    mesh.box(Recess, {0.0F, -0.27F, 0.025F}, {0.025F, 0.021F, 0.14F});
    mesh.part(WeaponPart::Magazine, [&] {
        mesh.box(CoatedSteel, {0.0F, -0.185F, 0.225F},
            {0.14F, 0.34F, 0.115F}, -8.0F);
        mesh.box(Polymer, {0.0F, -0.34F, 0.268F}, {0.153F, 0.055F, 0.13F});
    });
    for (const float facing : {-1.0F, 1.0F}) {
        mesh.box(Recess, {facing * 0.124F, 0.048F, -0.04F},
            {0.007F, 0.063F, 0.135F});
        mesh.box(BareSteel, {facing * 0.129F, 0.057F, -0.041F},
            {0.005F, 0.037F, 0.102F});
        mesh.box(CoatedSteel, {facing * 0.132F, 0.043F, -0.041F},
            {0.005F, 0.007F, 0.085F});
        mesh.part(WeaponPart::Mechanism, [&] {
            mesh.box(BareSteel, {facing * 0.126F, -0.017F, -0.17F},
                {0.007F, 0.013F, 0.088F});
        });
        mesh.sideDisc(BareSteel, facing * 0.129F, -0.033F, 0.092F, 0.025F);
        mesh.sideDisc(Recess, facing * 0.131F, -0.033F, 0.092F, 0.008F);
        for (const float screwZ : {-0.205F, 0.193F}) {
            mesh.sideDisc(BareSteel, facing * 0.126F, 0.091F, screwZ, 0.009F);
            mesh.sideDisc(Recess, facing * 0.128F, 0.091F, screwZ, 0.003F);
        }
        mesh.box(Recess, {facing * 0.112F, -0.056F, 0.51F},
            {0.006F, 0.014F, 0.46F});
        mesh.box(BareSteel, {facing * 0.113F, 0.084F, 0.52F},
            {0.005F, 0.005F, 0.40F});
        mesh.part(WeaponPart::Magazine, [&] {
            mesh.box(Recess, {facing * 0.073F, -0.2F, 0.225F},
                {0.005F, 0.25F, 0.086F}, -8.0F);
            for (int rib = 0; rib < 5; ++rib) {
                mesh.box(BareSteel, {facing * 0.078F,
                        -0.075F - static_cast<float>(rib) * 0.048F, 0.24F},
                    {0.004F, 0.004F, 0.066F}, -8.0F);
            }
        });
        for (int row = 0; row < 5; ++row) {
            mesh.box(Recess, {facing * 0.067F,
                    -0.18F - static_cast<float>(row) * 0.029F, -0.091F},
                {0.004F, 0.006F, 0.083F});
        }
    }
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
    mesh.box(Recess, {0.0F, 0.145F, -0.205F}, {0.095F, 0.018F, 0.045F});
    mesh.box(Recess, {-0.037F, 0.185F, -0.205F},
        {0.018F, 0.07F, 0.032F});
    mesh.box(Recess, {0.037F, 0.185F, -0.205F},
        {0.018F, 0.07F, 0.032F});
    mesh.box(Recess, {0.0F, 0.145F, 0.72F}, {0.07F, 0.016F, 0.033F});
    mesh.box(Recess, {0.0F, 0.18F, 0.72F}, {0.018F, 0.067F, 0.025F});
    mesh.box(Sight, {0.0F, 0.216F, 0.72F}, {0.014F, 0.008F, 0.009F});
}

void makeSubmachineGun(WeaponMeshBuilder& mesh)
{
    using enum WeaponMaterial;
    // Approved Used v1: short receiver/fore-end and folding stock, not a
    // scaled-down carbine. The existing metallic material family is reused.
    mesh.beveledLoft(CoatedSteel, std::array{
        WeaponMeshBuilder::LoftSection{-0.28F, 0.102F, 0.015F, 0.077F, 0.017F},
        WeaponMeshBuilder::LoftSection{0.19F, 0.105F, 0.015F, 0.079F, 0.019F},
        WeaponMeshBuilder::LoftSection{0.24F, 0.089F, 0.015F, 0.068F, 0.016F}});
    mesh.beveledLoft(Polymer, std::array{
        WeaponMeshBuilder::LoftSection{0.22F, 0.091F, 0.004F, 0.075F, 0.017F},
        WeaponMeshBuilder::LoftSection{0.57F, 0.087F, 0.004F, 0.071F, 0.015F},
        WeaponMeshBuilder::LoftSection{0.63F, 0.076F, 0.004F, 0.059F, 0.014F}});
    mesh.box(CoatedSteel, {0.0F, 0.098F, 0.025F}, {0.135F, 0.016F, 0.77F});
    for (int notch = 0; notch < 8; ++notch) {
        mesh.box(Recess, {0.0F, 0.109F, -0.22F + 0.082F * float(notch)},
            {0.105F, 0.006F, 0.013F});
    }
    mesh.cylinder(BareSteel, {0.0F, 0.005F, 0.0F},
        0.032F, 0.032F, 0.62F, 0.76F);
    mesh.cylinder(CoatedSteel, {0.0F, 0.005F, 0.0F},
        0.047F, 0.047F, 0.74F, 0.81F);
    mesh.cylinder(Recess, {0.0F, 0.005F, 0.0F},
        0.027F, 0.027F, 0.808F, 0.812F);

    // The exposed pair of rails and triangular side braces read as a folding
    // stock even when the weapon is a small ground pickup.
    mesh.box(CoatedSteel, {0.0F, -0.012F, -0.30F}, {0.15F, 0.095F, 0.10F});
    for (const float side : {-1.0F, 1.0F}) {
        mesh.box(BareSteel, {side * 0.083F, 0.015F, -0.43F},
            {0.018F, 0.023F, 0.30F});
        mesh.box(Polymer, {side * 0.081F, -0.065F, -0.45F},
            {0.023F, 0.025F, 0.24F}, 17.0F);
    }
    mesh.box(Polymer, {0.0F, -0.035F, -0.59F}, {0.21F, 0.17F, 0.045F});
    mesh.box(Recess, {0.0F, -0.035F, -0.616F}, {0.205F, 0.164F, 0.008F});
    mesh.box(Polymer, {0.0F, -0.185F, -0.11F},
        {0.13F, 0.28F, 0.14F}, 18.0F, weaponGripSemanticRegion);
    mesh.box(Recess, {0.0F, -0.119F, 0.014F}, {0.022F, 0.020F, 0.18F});
    mesh.box(Recess, {0.0F, -0.18F, 0.094F}, {0.022F, 0.112F, 0.021F});
    mesh.box(Recess, {0.0F, -0.24F, 0.015F}, {0.022F, 0.020F, 0.175F});
    mesh.box(BareSteel, {0.0F, -0.17F, 0.017F},
        {0.014F, 0.061F, 0.015F}, -15.0F);

    mesh.part(WeaponPart::Magazine, [&] {
        mesh.box(CoatedSteel, {0.0F, -0.18F, 0.20F},
            {0.14F, 0.30F, 0.115F}, -8.0F);
        mesh.box(CoatedSteel, {0.0F, -0.315F, 0.225F},
            {0.14F, 0.095F, 0.117F}, -17.0F);
        mesh.box(Polymer, {0.0F, -0.372F, 0.242F},
            {0.152F, 0.032F, 0.125F}, -17.0F);
        for (const float side : {-1.0F, 1.0F}) {
            for (int rib = 0; rib < 4; ++rib) {
                mesh.box(Recess, {side * 0.072F,
                        -0.09F - 0.049F * float(rib), 0.208F},
                    {0.004F, 0.006F, 0.083F}, -8.0F);
            }
        }
    });
    for (const float side : {-1.0F, 1.0F}) {
        for (int slot = 0; slot < 3; ++slot) {
            mesh.box(Recess, {side * 0.09F, 0.026F,
                    0.32F + 0.085F * float(slot)},
                {0.006F, 0.027F, 0.055F});
        }
        mesh.box(BareSteel, {side * 0.108F, 0.032F, -0.06F},
            {0.004F, 0.005F, 0.18F});
        mesh.sideDisc(BareSteel, side * 0.108F, -0.03F, 0.10F, 0.012F);
        mesh.sideDisc(Recess, side * 0.110F, -0.03F, 0.10F, 0.004F);
        for (int groove = 0; groove < 4; ++groove) {
            mesh.box(Recess, {side * 0.068F,
                    -0.16F - 0.034F * float(groove), -0.10F},
                {0.004F, 0.006F, 0.087F});
        }
    }
    mesh.part(WeaponPart::Mechanism, [&] {
        mesh.box(BareSteel, {-0.124F, -0.017F, -0.17F},
            {0.058F, 0.023F, 0.026F});
        mesh.box(CoatedSteel, {-0.151F, -0.019F, -0.17F},
            {0.018F, 0.039F, 0.032F});
    });
    mesh.box(CoatedSteel, {0.0F, 0.124F, -0.20F}, {0.084F, 0.050F, 0.035F});
    mesh.box(Recess, {0.0F, 0.155F, -0.20F}, {0.027F, 0.031F, 0.028F});
    mesh.box(CoatedSteel, {0.0F, 0.117F, 0.57F}, {0.068F, 0.041F, 0.033F});
    mesh.box(Recess, {0.0F, 0.144F, 0.57F}, {0.020F, 0.028F, 0.026F});
    mesh.box(Sight, {0.0F, 0.161F, 0.57F}, {0.012F, 0.006F, 0.007F});
}

void makeSniper(WeaponMeshBuilder& mesh)
{
    using enum WeaponMaterial;
    // Approved rifle Used v1, optic Used v2: receiver/stock/bolt stay unchanged.
    mesh.beveledLoft(CoatedSteel, std::array{
        WeaponMeshBuilder::LoftSection{-0.24F, 0.096F, 0.015F, 0.069F, 0.016F},
        WeaponMeshBuilder::LoftSection{0.21F, 0.096F, 0.015F, 0.069F, 0.016F},
        WeaponMeshBuilder::LoftSection{0.29F, 0.075F, 0.015F, 0.055F, 0.014F}});
    mesh.beveledLoft(Polymer, std::array{
        WeaponMeshBuilder::LoftSection{-0.69F, 0.11F, -0.025F, 0.09F, 0.02F},
        WeaponMeshBuilder::LoftSection{-0.43F, 0.095F, 0.018F, 0.065F, 0.018F},
        WeaponMeshBuilder::LoftSection{-0.24F, 0.075F, 0.018F, 0.044F, 0.012F}});
    mesh.box(Recess, {0.0F, -0.025F, -0.72F}, {0.21F, 0.19F, 0.025F});
    mesh.box(Polymer, {0.0F, 0.092F, -0.47F}, {0.16F, 0.026F, 0.24F});
    mesh.box(Polymer, {0.0F, -0.178F, -0.13F},
        {0.12F, 0.26F, 0.135F}, 18.0F, weaponGripSemanticRegion);
    mesh.box(Recess, {0.0F, -0.275F, -0.11F}, {0.13F, 0.025F, 0.15F});
    mesh.part(WeaponPart::Magazine, [&] {
        mesh.box(CoatedSteel, {0.0F, -0.17F, 0.095F}, {0.115F, 0.19F, 0.16F});
        mesh.box(Recess, {0.0F, -0.272F, 0.095F}, {0.12F, 0.02F, 0.165F});
    });
    mesh.box(Recess, {0.0F, -0.126F, -0.01F}, {0.024F, 0.022F, 0.15F});
    mesh.box(Recess, {0.0F, -0.204F, 0.035F}, {0.024F, 0.13F, 0.024F});
    mesh.box(Recess, {0.0F, -0.264F, -0.02F}, {0.024F, 0.022F, 0.12F});
    mesh.box(BareSteel, {0.0F, -0.18F, -0.017F}, {0.014F, 0.062F, 0.018F}, -15.0F);
    mesh.beveledLoft(CoatedSteel, std::array{
        WeaponMeshBuilder::LoftSection{0.28F, 0.095F, -0.005F, 0.07F, 0.018F},
        WeaponMeshBuilder::LoftSection{0.70F, 0.083F, -0.005F, 0.056F, 0.016F}});
    mesh.cylinder(BareSteel, {0.0F, 0.014F, 0.0F}, 0.035F, 0.035F, 0.65F, 1.17F);
    mesh.cylinder(CoatedSteel, {0.0F, 0.014F, 0.0F}, 0.052F, 0.052F, 1.15F, 1.25F);
    mesh.cylinder(Recess, {0.0F, 0.014F, 0.0F}, 0.027F, 0.027F, 1.25F, 1.252F);
    mesh.box(CoatedSteel, {0.0F, 0.105F, 0.02F}, {0.105F, 0.028F, 0.46F});
    constexpr Vec3 opticCenter{0.0F, 0.265F, 0.0F};
    for (const float z : {-0.09F, 0.12F}) {
        mesh.box(CoatedSteel, {0.0F, 0.17F, z}, {0.11F, 0.106F, 0.044F});
        mesh.cylinder(CoatedSteel, opticCenter, 0.075F, 0.075F, z - 0.026F, z + 0.026F);
        mesh.sideDisc(BareSteel, 0.076F, 0.265F, z, 0.012F);
        mesh.sideDisc(Recess, 0.077F, 0.265F, z, 0.005F);
    }
    mesh.cylinder(CoatedSteel, opticCenter, 0.056F, 0.056F, -0.20F, 0.19F);
    mesh.cylinder(CoatedSteel, opticCenter, 0.056F, 0.098F, 0.18F, 0.35F);
    mesh.cylinder(CoatedSteel, opticCenter, 0.098F, 0.098F, 0.35F, 0.415F);
    mesh.cylinder(BareSteel, opticCenter, 0.099F, 0.099F, 0.406F, 0.413F);
    mesh.cylinder(Recess, opticCenter, 0.085F, 0.085F, 0.416F, 0.418F);
    mesh.cylinder(CoatedSteel, opticCenter, 0.078F, 0.071F, -0.34F, -0.19F);
    mesh.cylinder(BareSteel, opticCenter, 0.079F, 0.079F, -0.328F, -0.322F);
    mesh.cylinder(Recess, opticCenter, 0.065F, 0.065F, -0.342F, -0.339F);
    mesh.box(CoatedSteel, {0.0F, 0.348F, 0.015F}, {0.070F, 0.060F, 0.070F});
    mesh.sideDisc(CoatedSteel, 0.072F, 0.265F, 0.015F, 0.037F);
    mesh.part(WeaponPart::Mechanism, [&] {
        mesh.box(BareSteel, {0.148F, -0.012F, -0.08F}, {0.136F, 0.027F, 0.025F}, 25.0F);
        mesh.cylinder(BareSteel, {0.215F, -0.07F, 0.0F}, 0.025F, 0.025F, -0.10F, -0.06F);
    });
    for (const float side : {-1.0F, 1.0F}) {
        mesh.box(Recess, {side * 0.097F, 0.045F, 0.025F}, {0.003F, 0.035F, 0.23F});
        mesh.box(BareSteel, {side * 0.10F, 0.04F, 0.025F}, {0.002F, 0.015F, 0.17F});
        for (int slot = 0; slot < 4; ++slot) {
            mesh.box(Recess, {side * 0.095F, -0.003F, 0.34F + 0.075F * float(slot)},
                {0.003F, 0.022F, 0.049F});
        }
        mesh.sideDisc(BareSteel, side * 0.112F, -0.012F, -0.57F, 0.012F);
        mesh.sideDisc(Recess, side * 0.113F, -0.012F, -0.57F, 0.004F);
    }
}

void makeShotgun(WeaponMeshBuilder& mesh)
{
    using enum WeaponMaterial;
    // Approved Used v1: conventional stock, pump and paired barrel/tube.
    mesh.beveledLoft(CoatedSteel, std::array{
        WeaponMeshBuilder::LoftSection{-0.26F, 0.102F, 0.0F, 0.077F, 0.018F},
        WeaponMeshBuilder::LoftSection{0.20F, 0.102F, 0.0F, 0.077F, 0.018F},
        WeaponMeshBuilder::LoftSection{0.27F, 0.075F, 0.0F, 0.055F, 0.014F}});
    mesh.beveledLoft(Polymer, std::array{
        WeaponMeshBuilder::LoftSection{-0.70F, 0.115F, -0.045F, 0.11F, 0.024F},
        WeaponMeshBuilder::LoftSection{-0.39F, 0.081F, -0.008F, 0.075F, 0.018F},
        WeaponMeshBuilder::LoftSection{-0.26F, 0.063F, 0.002F, 0.049F, 0.013F}});
    mesh.box(Recess, {0.0F, -0.045F, -0.73F}, {0.22F, 0.23F, 0.025F});
    mesh.box(Polymer, {0.0F, -0.173F, -0.21F},
        {0.12F, 0.25F, 0.13F}, 24.0F, weaponGripSemanticRegion);
    mesh.box(Recess, {0.0F, -0.104F, -0.074F}, {0.024F, 0.025F, 0.145F});
    mesh.box(Recess, {0.0F, -0.17F, -0.015F}, {0.024F, 0.12F, 0.025F});
    mesh.box(Recess, {0.0F, -0.23F, -0.074F}, {0.024F, 0.024F, 0.15F});
    mesh.box(BareSteel, {0.0F, -0.163F, -0.088F}, {0.014F, 0.067F, 0.018F}, -16.0F);
    mesh.cylinder(BareSteel, {0.0F, 0.046F, 0.0F}, 0.048F, 0.048F, 0.18F, 1.14F);
    mesh.cylinder(Recess, {0.0F, 0.046F, 0.0F}, 0.035F, 0.035F, 1.14F, 1.142F);
    mesh.cylinder(CoatedSteel, {0.0F, -0.063F, 0.0F}, 0.046F, 0.046F, 0.21F, 1.03F);
    mesh.cylinder(CoatedSteel, {0.0F, -0.063F, 0.0F}, 0.053F, 0.053F, 0.97F, 1.035F);
    mesh.part(WeaponPart::Mechanism, [&] {
        mesh.cylinder(Polymer, {0.0F, -0.035F, 0.0F}, 0.083F, 0.079F, 0.33F, 0.66F);
        for (int rib = 0; rib < 8; ++rib) {
            const float z = 0.35F + float(rib) * 0.039F;
            mesh.cylinder(Recess, {0.0F, -0.035F, 0.0F}, 0.087F, 0.082F, z, z + 0.009F);
        }
    });
    mesh.box(CoatedSteel, {0.0F, 0.105F, -0.19F}, {0.068F, 0.05F, 0.04F});
    mesh.box(Recess, {0.0F, 0.112F, -0.188F}, {0.023F, 0.038F, 0.042F});
    mesh.box(CoatedSteel, {0.0F, 0.10F, 1.04F}, {0.043F, 0.055F, 0.038F});
    for (const float side : {-1.0F, 1.0F}) {
        mesh.box(Recess, {side * 0.103F, 0.022F, 0.01F}, {0.003F, 0.065F, 0.17F});
        mesh.box(BareSteel, {side * 0.105F, 0.022F, 0.01F}, {0.002F, 0.04F, 0.12F});
        mesh.sideDisc(BareSteel, side * 0.105F, -0.046F, -0.17F, 0.008F);
        mesh.sideDisc(BareSteel, side * 0.105F, -0.046F, 0.16F, 0.008F);
    }
}

void makeKnife(WeaponMeshBuilder& mesh)
{
    using enum WeaponMaterial;
    mesh.box(BareSteel, {0.0F, 0.015F, -0.115F},
        {0.137F, 0.047F, 0.44F});
    mesh.beveledLoft(Polymer,
        std::array{WeaponMeshBuilder::LoftSection{-0.345F, 0.052F, 0.015F,
                       0.037F, 0.013F},
            WeaponMeshBuilder::LoftSection{-0.275F, 0.063F, 0.015F,
                0.049F, 0.016F},
            WeaponMeshBuilder::LoftSection{-0.105F, 0.059F, 0.015F,
                0.046F, 0.015F},
            WeaponMeshBuilder::LoftSection{0.02F, 0.065F, 0.015F,
                0.049F, 0.016F},
            WeaponMeshBuilder::LoftSection{0.12F, 0.053F, 0.015F,
                0.041F, 0.013F}});
    mesh.box(Recess, {0.0F, 0.015F, -0.357F}, {0.15F, 0.106F, 0.04F});
    mesh.box(CoatedSteel, {0.0F, 0.015F, 0.126F}, {0.255F, 0.073F, 0.055F});
    mesh.box(CoatedSteel, {0.0F, 0.015F, 0.06F}, {0.08F, 0.064F, 0.16F});
    for (const float facing : {-1.0F, 1.0F}) {
        for (int groove = 0; groove < 8; ++groove) {
            mesh.box(Recess, {facing * 0.065F, 0.018F,
                    -0.29F + static_cast<float>(groove) * 0.05F},
                {0.005F, 0.052F, 0.009F});
        }
        for (const float pinZ : {-0.255F, 0.03F}) {
            mesh.sideDisc(BareSteel, facing * 0.067F, 0.016F, pinZ, 0.011F);
            mesh.sideDisc(Recess, facing * 0.069F, 0.016F, pinZ, 0.004F);
        }
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
    case HudWeaponRole::SubmachineGun: makeSubmachineGun(builder); break;
    case HudWeaponRole::Sniper: makeSniper(builder); break;
    case HudWeaponRole::Shotgun: makeShotgun(builder); break;
    case HudWeaponRole::Knife: makeKnife(builder); break;
    }
    return builder.finish();
}

WeaponPartMeshes makeWeaponViewmodelParts(const HudWeaponRole role)
{
    const auto complete = makeWeaponViewmodelMeshes(role);
    WeaponPartMeshes parts;
    for (std::size_t m = 0; m < weaponMaterialCount; ++m) {
        const auto& source = complete[m];
        std::array<std::vector<std::uint32_t>, weaponPartCount> remaps;
        for (auto& map : remaps) map.assign(source.vertices.size(), UINT32_MAX);
        for (const auto index : source.indices) {
            const auto region = source.vertices[index].semanticRegion;
            const auto part = region == 2U ? WeaponPart::Magazine
                : region == 3U ? WeaponPart::Mechanism : WeaponPart::Body;
            const auto p = static_cast<std::size_t>(part);
            auto& output = parts[p][m];
            auto& mapped = remaps[p][index];
            if (mapped == UINT32_MAX) {
                mapped = static_cast<std::uint32_t>(output.vertices.size());
                output.vertices.push_back(source.vertices[index]);
            }
            output.indices.push_back(mapped);
        }
    }
    if (role == HudWeaponRole::Shotgun) {
        // A transient cartridge, never present in the idle or floor mesh.
        WeaponMeshBuilder shell;
        shell.cylinder(WeaponMaterial::CoatedSteel, {0.0F, -0.10F, 0.0F},
            0.025F, 0.025F, 0.025F, 0.12F);
        shell.cylinder(WeaponMaterial::BareSteel, {0.0F, -0.10F, 0.0F},
            0.028F, 0.028F, 0.01F, 0.025F);
        parts[static_cast<std::size_t>(WeaponPart::Magazine)] = shell.finish();
    }
    for (auto& part : parts) for (auto& mesh : part) {
        if (mesh.vertices.empty()) continue;
        Vec3 minimum{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()};
        Vec3 maximum{-minimum.x, -minimum.y, -minimum.z};
        for (const auto& vertex : mesh.vertices) {
            minimum.x = std::min(minimum.x, vertex.position.x);
            minimum.y = std::min(minimum.y, vertex.position.y);
            minimum.z = std::min(minimum.z, vertex.position.z);
            maximum.x = std::max(maximum.x, vertex.position.x);
            maximum.y = std::max(maximum.y, vertex.position.y);
            maximum.z = std::max(maximum.z, vertex.position.z);
        }
        mesh.bounds = {minimum, maximum};
        mesh.backfaceCullingSafe = false;
    }
    return parts;
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
    const int base = coated ? 216 : bare ? 234 : polymer ? 224 : recess ? 235 : 255;
    const int roughness = coated ? 105 : bare ? 76 : polymer ? 209 : 180;
    const int metalness = coated ? 156 : bare ? 211 : polymer ? 0 : recess ? 12 : 18;
    for (int y = 0; y < resolution; ++y) {
        for (int x = 0; x < resolution; ++x) {
            const std::uint32_t noise = pixelNoise(x, y,
                0x76532U + static_cast<std::uint32_t>(material));
            const int grain = static_cast<int>(noise & 7U) - 3;
            const int brush = (bare || coated) ? ((x + y / 17) & 7) - 3 : 0;
            const int molded = polymer ? ((x / 3 + y / 5) & 3) - 1 : 0;
            const bool hairline = (bare || coated) && (noise & 1023U) < 2U
                && (y % 37) < 2;
            const bool contactWear = coated && ((x > resolution - 11
                    || y > resolution - 9)
                && (noise & 15U) < 5U);
            const int tone = std::clamp(base + grain + brush + molded
                + (contactWear ? 17 : 0) - (hairline ? 19 : 0), 0, 255);
            const std::size_t offset = static_cast<std::size_t>(
                (y * resolution + x) * 4);
            const auto byte = [](const int value) {
                return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
            };
            images.albedo.rgba[offset] = byte(tone - (bare ? 3 : coated ? 6 : 0));
            images.albedo.rgba[offset + 1U] = byte(tone);
            images.albedo.rgba[offset + 2U] = byte(tone + (bare ? 4 : coated ? 5 : 0));
            images.albedo.rgba[offset + 3U] = 255U;
            images.normal.rgba[offset] = byte(128 + grain / 2 + molded);
            images.normal.rgba[offset + 1U] = byte(128 + brush * 2
                + (hairline ? 12 : 0));
            images.normal.rgba[offset + 2U] = 255U;
            images.normal.rgba[offset + 3U] = 255U;
            images.properties.rgba[offset] = byte(roughness + grain + brush
                + (hairline ? 24 : 0) + (contactWear ? -17 : 0));
            images.properties.rgba[offset + 1U] = byte(metalness
                + (contactWear ? 37 : 0));
            images.properties.rgba[offset + 2U] = 245U;
            images.properties.rgba[offset + 3U] = 255U;
        }
    }
    return images;
}

} // namespace gdp::game::visual
