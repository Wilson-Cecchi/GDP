#include "render/Mesh.hpp"

#include "core/Log.hpp"

#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>

namespace wkc::render {
namespace {

bool isFinite(const wkc::core::Vec3 value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

void setReason(std::string* const reason, const char* const text)
{
    if (reason != nullptr) {
        *reason = text;
    }
}

} // namespace

bool MeshBindingCache::needsBind(const GLuint vertexArray) noexcept
{
    ++stats_.requests;
    if (vertexArrayKnown_ && vertexArray_ == vertexArray) {
        ++stats_.skippedVertexArrayBinds;
        return false;
    }
    vertexArray_ = vertexArray;
    vertexArrayKnown_ = true;
    ++stats_.vertexArrayBinds;
    return true;
}

void MeshBindingCache::beginFrame() noexcept
{
    invalidate();
    stats_ = {};
}

void MeshBindingCache::invalidate() noexcept
{
    vertexArrayKnown_ = false;
}

MeshBindingCacheStats MeshBindingCache::stats() const noexcept
{
    return stats_;
}

MeshData makeBoxMesh()
{
    constexpr float low = -0.5F;
    constexpr float high = 0.5F;
    MeshData data;
    data.vertices = {
        {{low, low, high}, {0, 0, 1}, {0, 0}, 0},
        {{high, low, high}, {0, 0, 1}, {1, 0}, 0},
        {{high, high, high}, {0, 0, 1}, {1, 1}, 0},
        {{low, high, high}, {0, 0, 1}, {0, 1}, 0},
        {{high, low, low}, {0, 0, -1}, {0, 0}, 0},
        {{low, low, low}, {0, 0, -1}, {1, 0}, 0},
        {{low, high, low}, {0, 0, -1}, {1, 1}, 0},
        {{high, high, low}, {0, 0, -1}, {0, 1}, 0},
        {{high, low, high}, {1, 0, 0}, {0, 0}, 1},
        {{high, low, low}, {1, 0, 0}, {1, 0}, 1},
        {{high, high, low}, {1, 0, 0}, {1, 1}, 1},
        {{high, high, high}, {1, 0, 0}, {0, 1}, 1},
        {{low, low, low}, {-1, 0, 0}, {0, 0}, 1},
        {{low, low, high}, {-1, 0, 0}, {1, 0}, 1},
        {{low, high, high}, {-1, 0, 0}, {1, 1}, 1},
        {{low, high, low}, {-1, 0, 0}, {0, 1}, 1},
        {{low, high, high}, {0, 1, 0}, {0, 0}, 2},
        {{high, high, high}, {0, 1, 0}, {1, 0}, 2},
        {{high, high, low}, {0, 1, 0}, {1, 1}, 2},
        {{low, high, low}, {0, 1, 0}, {0, 1}, 2},
        {{low, low, low}, {0, -1, 0}, {0, 0}, 2},
        {{high, low, low}, {0, -1, 0}, {1, 0}, 2},
        {{high, low, high}, {0, -1, 0}, {1, 1}, 2},
        {{low, low, high}, {0, -1, 0}, {0, 1}, 2},
    };
    data.indices = {
        0, 1, 2, 0, 2, 3,
        4, 5, 6, 4, 6, 7,
        8, 9, 10, 8, 10, 11,
        12, 13, 14, 12, 14, 15,
        16, 17, 18, 16, 18, 19,
        20, 21, 22, 20, 22, 23,
    };
    data.bounds = {{low, low, low}, {high, high, high}};
    data.backfaceCullingSafe = true;
    return data;
}

bool validateMeshData(const MeshData& data, std::string* const reason)
{
    if (data.vertices.empty() || data.indices.empty()) {
        setReason(reason, "mesh is empty");
        return false;
    }
    if ((data.indices.size() % 3U) != 0U) {
        setReason(reason, "index count is not a triangle list");
        return false;
    }
    if (!data.bounds.isValid() || !isFinite(data.bounds.minimum)
        || !isFinite(data.bounds.maximum)) {
        setReason(reason, "mesh bounds are invalid");
        return false;
    }
    for (const Vertex& vertex : data.vertices) {
        if (!isFinite(vertex.position) || !isFinite(vertex.normal)
            || !std::isfinite(vertex.uv.x) || !std::isfinite(vertex.uv.y)) {
            setReason(reason, "mesh contains a non-finite vertex");
            return false;
        }
        if (!data.bounds.contains(vertex.position)) {
            setReason(reason, "mesh bounds do not contain all vertices");
            return false;
        }
    }
    for (const std::uint32_t index : data.indices) {
        if (index >= data.vertices.size()) {
            setReason(reason, "mesh contains an invalid index");
            return false;
        }
    }
    return true;
}

bool validateInstanceTransforms(
    const std::span<const wkc::core::Mat4> transforms,
    std::string* const reason)
{
    if (transforms.size()
            > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())
        || transforms.size_bytes()
            > static_cast<std::size_t>(
                std::numeric_limits<GLsizeiptr>::max())) {
        setReason(reason, "instance data exceeds the OpenGL size range");
        return false;
    }
    for (const wkc::core::Mat4& transform : transforms) {
        if (!std::all_of(transform.elements.begin(), transform.elements.end(),
                [](const float component) { return std::isfinite(component); })) {
            setReason(reason, "instance transform contains a non-finite component");
            return false;
        }
    }
    return true;
}

Mesh::~Mesh()
{
    reset();
}

Mesh::Mesh(Mesh&& other) noexcept
    : gl_(std::exchange(other.gl_, nullptr))
    , vertexArray_(std::exchange(other.vertexArray_, 0))
    , vertexBuffer_(std::exchange(other.vertexBuffer_, 0))
    , indexBuffer_(std::exchange(other.indexBuffer_, 0))
    , instanceBuffer_(std::exchange(other.instanceBuffer_, 0))
    , indexCount_(std::exchange(other.indexCount_, 0))
    , instanceCount_(std::exchange(other.instanceCount_, 0))
    , instanceAttributesConfigured_(
          std::exchange(other.instanceAttributesConfigured_, false))
    , backfaceCullingSafe_(std::exchange(other.backfaceCullingSafe_, false))
{
}

Mesh& Mesh::operator=(Mesh&& other) noexcept
{
    if (this != &other) {
        reset();
        gl_ = std::exchange(other.gl_, nullptr);
        vertexArray_ = std::exchange(other.vertexArray_, 0);
        vertexBuffer_ = std::exchange(other.vertexBuffer_, 0);
        indexBuffer_ = std::exchange(other.indexBuffer_, 0);
        instanceBuffer_ = std::exchange(other.instanceBuffer_, 0);
        indexCount_ = std::exchange(other.indexCount_, 0);
        instanceCount_ = std::exchange(other.instanceCount_, 0);
        instanceAttributesConfigured_
            = std::exchange(other.instanceAttributesConfigured_, false);
        backfaceCullingSafe_
            = std::exchange(other.backfaceCullingSafe_, false);
    }
    return *this;
}

bool Mesh::upload(const GlApi& gl, const MeshData& data)
{
    std::string reason;
    if (!validateMeshData(data, &reason)) {
        wkc::core::log(wkc::core::LogLevel::Error, "Mesh upload rejected: " + reason);
        return false;
    }
    if (data.indices.size() > std::numeric_limits<std::uint32_t>::max()) {
        wkc::core::log(wkc::core::LogLevel::Error, "Mesh index count exceeds 32-bit range");
        return false;
    }

    reset();
    gl_ = &gl;
    gl.genVertexArrays(1, &vertexArray_);
    gl.genBuffers(1, &vertexBuffer_);
    gl.genBuffers(1, &indexBuffer_);
    gl.bindVertexArray(vertexArray_);
    gl.bindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
    gl.bufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(data.vertices.size() * sizeof(Vertex)),
        data.vertices.data(),
        GL_STATIC_DRAW);
    gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer_);
    gl.bufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(data.indices.size() * sizeof(std::uint32_t)),
        data.indices.data(),
        GL_STATIC_DRAW);

    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(sizeof(Vertex)),
        reinterpret_cast<const void*>(offsetof(Vertex, position)));
    gl.enableVertexAttribArray(1);
    gl.vertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(sizeof(Vertex)),
        reinterpret_cast<const void*>(offsetof(Vertex, normal)));
    gl.enableVertexAttribArray(2);
    gl.vertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(sizeof(Vertex)),
        reinterpret_cast<const void*>(offsetof(Vertex, uv)));
    gl.bindVertexArray(0);

    indexCount_ = static_cast<std::uint32_t>(data.indices.size());
    backfaceCullingSafe_ = data.backfaceCullingSafe;
    return vertexArray_ != 0 && vertexBuffer_ != 0 && indexBuffer_ != 0;
}

bool Mesh::uploadInstanceTransforms(
    const GlApi& gl,
    const std::span<const wkc::core::Mat4> transforms,
    MeshBindingCache& cache)
{
    std::string reason;
    if (!isValid() || gl_ != &gl) {
        wkc::core::log(wkc::core::LogLevel::Error,
            "Instance upload requires its mesh's active OpenGL context");
        return false;
    }
    if (!validateInstanceTransforms(transforms, &reason)) {
        wkc::core::log(wkc::core::LogLevel::Error,
            "Instance upload rejected: " + reason);
        return false;
    }
    if (transforms.empty()) {
        instanceCount_ = 0U;
        return true;
    }

    if (instanceBuffer_ == 0U) {
        gl.genBuffers(1, &instanceBuffer_);
        if (instanceBuffer_ == 0U) {
            wkc::core::log(wkc::core::LogLevel::Error,
                "Instance buffer allocation failed");
            return false;
        }
    }
    gl.bindBuffer(GL_ARRAY_BUFFER, instanceBuffer_);
    gl.bufferData(GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(transforms.size_bytes()), transforms.data(),
        GL_DYNAMIC_DRAW);

    if (!instanceAttributesConfigured_) {
        gl.bindVertexArray(vertexArray_);
        constexpr std::size_t columnComponents = 4U;
        for (GLuint column = 0U; column < 4U; ++column) {
            const GLuint attribute
                = meshInstanceTransformFirstAttribute + column;
            gl.enableVertexAttribArray(attribute);
            gl.vertexAttribPointer(attribute, 4, GL_FLOAT, GL_FALSE,
                static_cast<GLsizei>(sizeof(wkc::core::Mat4)),
                reinterpret_cast<const void*>(
                    column * columnComponents * sizeof(float)));
            gl.vertexAttribDivisor(attribute, 1U);
        }
        gl.bindVertexArray(0U);
        cache.invalidate();
        instanceAttributesConfigured_ = true;
    }
    gl.bindBuffer(GL_ARRAY_BUFFER, 0U);
    instanceCount_ = static_cast<std::uint32_t>(transforms.size());
    return true;
}

void Mesh::draw(const GlApi& gl) const noexcept
{
    if (!isValid()) {
        return;
    }
    gl.bindVertexArray(vertexArray_);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount_), GL_UNSIGNED_INT, nullptr);
    gl.bindVertexArray(0);
}

void Mesh::draw(const GlApi& gl, MeshBindingCache& cache) const noexcept
{
    if (!isValid()) {
        return;
    }
    if (cache.needsBind(vertexArray_)) {
        gl.bindVertexArray(vertexArray_);
    }
    glDrawElements(
        GL_TRIANGLES, static_cast<GLsizei>(indexCount_), GL_UNSIGNED_INT, nullptr);
}

void Mesh::drawInstanced(
    const GlApi& gl, MeshBindingCache& cache) const noexcept
{
    if (!isValid() || instanceCount_ == 0U) {
        return;
    }
    if (cache.needsBind(vertexArray_)) {
        gl.bindVertexArray(vertexArray_);
    }
    gl.drawElementsInstanced(GL_TRIANGLES,
        static_cast<GLsizei>(indexCount_), GL_UNSIGNED_INT, nullptr,
        static_cast<GLsizei>(instanceCount_));
}

void Mesh::reset() noexcept
{
    if (gl_ != nullptr) {
        if (instanceBuffer_ != 0) {
            gl_->deleteBuffers(1, &instanceBuffer_);
        }
        if (indexBuffer_ != 0) {
            gl_->deleteBuffers(1, &indexBuffer_);
        }
        if (vertexBuffer_ != 0) {
            gl_->deleteBuffers(1, &vertexBuffer_);
        }
        if (vertexArray_ != 0) {
            gl_->deleteVertexArrays(1, &vertexArray_);
        }
    }
    gl_ = nullptr;
    vertexArray_ = 0;
    vertexBuffer_ = 0;
    indexBuffer_ = 0;
    instanceBuffer_ = 0;
    indexCount_ = 0;
    instanceCount_ = 0;
    instanceAttributesConfigured_ = false;
    backfaceCullingSafe_ = false;
}

bool Mesh::isValid() const noexcept
{
    return vertexArray_ != 0 && indexCount_ != 0;
}

std::uint32_t Mesh::indexCount() const noexcept
{
    return indexCount_;
}

std::uint32_t Mesh::instanceCount() const noexcept
{
    return instanceCount_;
}

bool Mesh::backfaceCullingSafe() const noexcept
{
    return backfaceCullingSafe_;
}

} // namespace wkc::render
