#include "procedural/MeshGenerator.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace wkc::procedural {
namespace {

wkc::core::Vec3 rotateEuler(
    wkc::core::Vec3 value,
    const wkc::core::Vec3 degrees) noexcept
{
    constexpr float toRadians = std::numbers::pi_v<float> / 180.0F;
    const float x = degrees.x * toRadians;
    const float y = degrees.y * toRadians;
    const float z = degrees.z * toRadians;

    value = {
        value.x,
        (value.y * std::cos(x)) - (value.z * std::sin(x)),
        (value.y * std::sin(x)) + (value.z * std::cos(x)),
    };
    value = {
        (value.x * std::cos(y)) + (value.z * std::sin(y)),
        value.y,
        (-value.x * std::sin(y)) + (value.z * std::cos(y)),
    };
    return {
        (value.x * std::cos(z)) - (value.y * std::sin(z)),
        (value.x * std::sin(z)) + (value.y * std::cos(z)),
        value.z,
    };
}

wkc::core::Vec3 componentMultiply(
    const wkc::core::Vec3 first,
    const wkc::core::Vec3 second) noexcept
{
    return {first.x * second.x, first.y * second.y, first.z * second.z};
}

void expandBounds(wkc::core::Aabb& bounds, const wkc::core::Vec3 point) noexcept
{
    bounds.minimum.x = std::min(bounds.minimum.x, point.x);
    bounds.minimum.y = std::min(bounds.minimum.y, point.y);
    bounds.minimum.z = std::min(bounds.minimum.z, point.z);
    bounds.maximum.x = std::max(bounds.maximum.x, point.x);
    bounds.maximum.y = std::max(bounds.maximum.y, point.y);
    bounds.maximum.z = std::max(bounds.maximum.z, point.z);
}

wkc::render::MeshData makeCylinderMesh()
{
    constexpr std::uint32_t segments = 12;
    constexpr float radius = 0.5F;
    constexpr float halfHeight = 0.5F;
    constexpr float fullCircle = std::numbers::pi_v<float> * 2.0F;
    wkc::render::MeshData mesh;

    for (std::uint32_t segment = 0; segment <= segments; ++segment) {
        const float fraction = static_cast<float>(segment) / static_cast<float>(segments);
        const float angle = fraction * fullCircle;
        const float x = std::cos(angle) * radius;
        const float z = std::sin(angle) * radius;
        const wkc::core::Vec3 normal = wkc::core::normalized({x, 0.0F, z});
        mesh.vertices.push_back({{x, -halfHeight, z}, normal, {fraction, 0.0F}, 0});
        mesh.vertices.push_back({{x, halfHeight, z}, normal, {fraction, 1.0F}, 0});
    }
    for (std::uint32_t segment = 0; segment < segments; ++segment) {
        const std::uint32_t bottom = segment * 2U;
        const std::uint32_t top = bottom + 1U;
        mesh.indices.insert(mesh.indices.end(), {
            bottom, bottom + 2U, top + 2U,
            bottom, top + 2U, top,
        });
    }

    const std::uint32_t topCenter = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({{0.0F, halfHeight, 0.0F}, {0.0F, 1.0F, 0.0F}, {0.5F, 0.5F}, 0});
    for (std::uint32_t segment = 0; segment <= segments; ++segment) {
        const float angle = (static_cast<float>(segment) / static_cast<float>(segments))
            * fullCircle;
        const float x = std::cos(angle) * radius;
        const float z = std::sin(angle) * radius;
        mesh.vertices.push_back({
            {x, halfHeight, z},
            {0.0F, 1.0F, 0.0F},
            {x + 0.5F, z + 0.5F},
            0,
        });
    }
    const std::uint32_t topRing = topCenter + 1U;
    for (std::uint32_t segment = 0; segment < segments; ++segment) {
        mesh.indices.insert(mesh.indices.end(), {
            topCenter, topRing + segment + 1U, topRing + segment,
        });
    }

    const std::uint32_t bottomCenter = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({
        {0.0F, -halfHeight, 0.0F},
        {0.0F, -1.0F, 0.0F},
        {0.5F, 0.5F},
        0,
    });
    for (std::uint32_t segment = 0; segment <= segments; ++segment) {
        const float angle = (static_cast<float>(segment) / static_cast<float>(segments))
            * fullCircle;
        const float x = std::cos(angle) * radius;
        const float z = std::sin(angle) * radius;
        mesh.vertices.push_back({
            {x, -halfHeight, z},
            {0.0F, -1.0F, 0.0F},
            {x + 0.5F, z + 0.5F},
            0,
        });
    }
    const std::uint32_t bottomRing = bottomCenter + 1U;
    for (std::uint32_t segment = 0; segment < segments; ++segment) {
        mesh.indices.insert(mesh.indices.end(), {
            bottomCenter, bottomRing + segment, bottomRing + segment + 1U,
        });
    }
    mesh.bounds = {{-radius, -halfHeight, -radius}, {radius, halfHeight, radius}};
    return mesh;
}

wkc::render::MeshData primitiveMesh(const PrimitiveType type)
{
    return type == PrimitiveType::Cylinder
        ? makeCylinderMesh()
        : wkc::render::makeBoxMesh();
}

} // namespace

std::optional<MeshBuildResult> generateMesh(const MeshRecipe& recipe)
{
    if (!validateMeshRecipe(recipe).isValid()) {
        return std::nullopt;
    }

    MeshBuildResult result;
    const float infinity = std::numeric_limits<float>::infinity();
    result.mesh.bounds = {{infinity, infinity, infinity}, {-infinity, -infinity, -infinity}};
    result.ranges.reserve(recipe.primitives.size());

    for (const PrimitiveRecipe& primitive : recipe.primitives) {
        const wkc::render::MeshData baseMesh = primitiveMesh(primitive.type);
        const std::uint32_t vertexOffset = static_cast<std::uint32_t>(result.mesh.vertices.size());
        const std::uint32_t firstIndex = static_cast<std::uint32_t>(result.mesh.indices.size());
        const wkc::core::Vec3 dimensions = componentMultiply(
            primitive.dimensions,
            primitive.localTransform.scale);
        for (wkc::render::Vertex vertex : baseMesh.vertices) {
            vertex.position = componentMultiply(vertex.position, dimensions);
            vertex.position = rotateEuler(
                vertex.position,
                primitive.localTransform.rotationDegrees);
            vertex.position = vertex.position + primitive.localTransform.position;
            vertex.normal = wkc::core::normalized(rotateEuler(
                vertex.normal,
                primitive.localTransform.rotationDegrees));
            vertex.semanticRegion = static_cast<std::uint32_t>(primitive.semantic);
            expandBounds(result.mesh.bounds, vertex.position);
            result.mesh.vertices.push_back(vertex);
        }
        for (const std::uint32_t index : baseMesh.indices) {
            result.mesh.indices.push_back(vertexOffset + index);
        }
        result.ranges.push_back({
            firstIndex,
            static_cast<std::uint32_t>(baseMesh.indices.size()),
            primitive.material,
            primitive.semantic,
        });
    }
    if (!wkc::render::validateMeshData(result.mesh)) {
        return std::nullopt;
    }
    return result;
}

} // namespace wkc::procedural
