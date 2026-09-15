#pragma once

#include "core/Math.hpp"
#include "render/GlApi.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace wkc::render {

struct Vertex {
    wkc::core::Vec3 position{};
    wkc::core::Vec3 normal{};
    wkc::core::Vec2 uv{};
    std::uint32_t semanticRegion{0};
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    wkc::core::Aabb bounds{};
    bool backfaceCullingSafe{false};
};

[[nodiscard]] MeshData makeBoxMesh();
[[nodiscard]] bool validateMeshData(const MeshData& data, std::string* reason = nullptr);

struct MeshBindingCacheStats {
    std::uint64_t requests{0U};
    std::uint64_t vertexArrayBinds{0U};
    std::uint64_t skippedVertexArrayBinds{0U};

    constexpr bool operator==(const MeshBindingCacheStats&) const noexcept
        = default;
};

class MeshBindingCache {
public:
    [[nodiscard]] bool needsBind(GLuint vertexArray) noexcept;
    void beginFrame() noexcept;
    [[nodiscard]] MeshBindingCacheStats stats() const noexcept;

private:
    GLuint vertexArray_{0U};
    bool vertexArrayKnown_{false};
    MeshBindingCacheStats stats_{};
};

class Mesh {
public:
    Mesh() = default;
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    [[nodiscard]] bool upload(const GlApi& gl, const MeshData& data);
    void draw(const GlApi& gl) const noexcept;
    void draw(const GlApi& gl, MeshBindingCache& cache) const noexcept;
    void reset() noexcept;

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] std::uint32_t indexCount() const noexcept;
    [[nodiscard]] bool backfaceCullingSafe() const noexcept;

private:
    const GlApi* gl_{nullptr};
    GLuint vertexArray_{0};
    GLuint vertexBuffer_{0};
    GLuint indexBuffer_{0};
    std::uint32_t indexCount_{0};
    bool backfaceCullingSafe_{false};
};

} // namespace wkc::render
