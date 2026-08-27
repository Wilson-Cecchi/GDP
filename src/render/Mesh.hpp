#pragma once

#include "core/Math.hpp"
#include "render/GlApi.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace gdp::render {

struct Vertex {
    gdp::core::Vec3 position{};
    gdp::core::Vec3 normal{};
    gdp::core::Vec2 uv{};
    std::uint32_t semanticRegion{0};
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    gdp::core::Aabb bounds{};
};

[[nodiscard]] MeshData makeBoxMesh();
[[nodiscard]] bool validateMeshData(const MeshData& data, std::string* reason = nullptr);

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
    void reset() noexcept;

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] std::uint32_t indexCount() const noexcept;

private:
    const GlApi* gl_{nullptr};
    GLuint vertexArray_{0};
    GLuint vertexBuffer_{0};
    GLuint indexBuffer_{0};
    std::uint32_t indexCount_{0};
};

} // namespace gdp::render
