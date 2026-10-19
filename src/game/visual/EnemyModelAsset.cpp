#include "game/visual/EnemyModelAsset.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <limits>
#include <string_view>
#include <utility>

namespace gdp::game::visual {
namespace {

constexpr std::size_t maxFileBytes = 32U * 1024U * 1024U;
constexpr std::uint32_t maxSubmeshes = 256U;
constexpr std::uint32_t maxTotalVertices = 750'000U;
constexpr std::uint32_t maxTotalIndices = 2'250'000U;
constexpr std::size_t vertexBytes = 8U * sizeof(std::uint32_t);
constexpr std::size_t submeshHeaderBytes = 7U * sizeof(std::uint32_t);
constexpr std::string_view magic = "ISEMESH1";

void fail(std::string* const reason, const std::string_view message)
{
    if (reason != nullptr) {
        *reason = message;
    }
}

class Reader {
public:
    explicit Reader(const std::span<const std::byte> bytes) : bytes_(bytes) {}

    [[nodiscard]] bool has(const std::size_t count) const noexcept
    {
        return count <= bytes_.size() - offset_;
    }

    [[nodiscard]] std::size_t remaining() const noexcept
    {
        return bytes_.size() - offset_;
    }

    [[nodiscard]] bool magicMatches() noexcept
    {
        if (!has(magic.size())) {
            return false;
        }
        for (const char expected : magic) {
            if (std::to_integer<unsigned char>(bytes_[offset_++])
                != static_cast<unsigned char>(expected)) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool u32(std::uint32_t& value) noexcept
    {
        if (!has(4U)) {
            return false;
        }
        value = 0U;
        for (unsigned shift = 0U; shift < 32U; shift += 8U) {
            value |= static_cast<std::uint32_t>(
                         std::to_integer<unsigned char>(bytes_[offset_++]))
                << shift;
        }
        return true;
    }

    [[nodiscard]] bool f32(float& value) noexcept
    {
        std::uint32_t bits = 0U;
        if (!u32(bits)) {
            return false;
        }
        value = std::bit_cast<float>(bits);
        return true;
    }

private:
    std::span<const std::byte> bytes_;
    std::size_t offset_{0U};
};

[[nodiscard]] bool vec3(Reader& reader, wkc::core::Vec3& value) noexcept
{
    return reader.f32(value.x) && reader.f32(value.y) && reader.f32(value.z)
        && std::isfinite(value.x) && std::isfinite(value.y)
        && std::isfinite(value.z);
}

} // namespace

std::optional<EnemyModelAsset> parseEnemyModelAsset(
    const std::span<const std::byte> bytes, std::string* const reason)
{
    if (bytes.size() > maxFileBytes) {
        fail(reason, "enemy model exceeds 32 MiB limit");
        return std::nullopt;
    }
    Reader reader(bytes);
    if (!reader.magicMatches()) {
        fail(reason, "enemy model magic/version is not ISEMESH1");
        return std::nullopt;
    }
    std::uint32_t submeshCount = 0U;
    if (!reader.u32(submeshCount) || submeshCount == 0U
        || submeshCount > maxSubmeshes) {
        fail(reason, "invalid enemy model submesh count");
        return std::nullopt;
    }

    EnemyModelAsset result;
    result.submeshes.reserve(submeshCount);
    std::uint32_t totalVertices = 0U;
    std::uint32_t totalIndices = 0U;
    for (std::uint32_t submeshIndex = 0U; submeshIndex < submeshCount;
         ++submeshIndex) {
        if (!reader.has(submeshHeaderBytes)) {
            fail(reason, "truncated enemy model submesh header");
            return std::nullopt;
        }
        EnemyModelSubmesh submesh;
        std::uint32_t vertexCount = 0U;
        std::uint32_t indexCount = 0U;
        if (!reader.u32(submesh.partId) || !reader.u32(submesh.materialId)
            || !vec3(reader, submesh.pivot) || !reader.u32(vertexCount)
            || !reader.u32(indexCount)) {
            fail(reason, "invalid enemy model submesh header");
            return std::nullopt;
        }
        if (submesh.partId > 9U || submesh.materialId > 5U) {
            fail(reason, "enemy model part or material ID is out of range");
            return std::nullopt;
        }
        if (vertexCount == 0U || indexCount == 0U
            || indexCount % 3U != 0U || vertexCount > maxTotalVertices - totalVertices
            || indexCount > maxTotalIndices - totalIndices) {
            fail(reason, "invalid enemy model vertex or triangle count");
            return std::nullopt;
        }
        // Division avoids overflow even when the file supplies u32 maximum counts.
        if (vertexCount > reader.remaining() / vertexBytes
            || indexCount > (reader.remaining() - vertexCount * vertexBytes)
                                  / sizeof(std::uint32_t)) {
            fail(reason, "truncated enemy model geometry");
            return std::nullopt;
        }

        auto& mesh = submesh.mesh;
        mesh.vertices.reserve(vertexCount);
        mesh.indices.reserve(indexCount);
        wkc::core::Vec3 minimum{std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
        wkc::core::Vec3 maximum{-std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()};
        for (std::uint32_t index = 0U; index < vertexCount; ++index) {
            wkc::render::Vertex vertex;
            if (!vec3(reader, vertex.position) || !vec3(reader, vertex.normal)
                || !reader.f32(vertex.uv.x) || !reader.f32(vertex.uv.y)
                || !std::isfinite(vertex.uv.x) || !std::isfinite(vertex.uv.y)) {
                fail(reason, "enemy model has a non-finite or truncated vertex");
                return std::nullopt;
            }
            const float normalLengthSquared = wkc::core::dot(vertex.normal, vertex.normal);
            if (!std::isfinite(normalLengthSquared)
                || normalLengthSquared < 0.25F || normalLengthSquared > 2.25F) {
                fail(reason, "enemy model has an invalid vertex normal");
                return std::nullopt;
            }
            minimum.x = std::min(minimum.x, vertex.position.x);
            minimum.y = std::min(minimum.y, vertex.position.y);
            minimum.z = std::min(minimum.z, vertex.position.z);
            maximum.x = std::max(maximum.x, vertex.position.x);
            maximum.y = std::max(maximum.y, vertex.position.y);
            maximum.z = std::max(maximum.z, vertex.position.z);
            mesh.vertices.push_back(vertex);
        }
        for (std::uint32_t index = 0U; index < indexCount; ++index) {
            std::uint32_t vertexIndex = 0U;
            if (!reader.u32(vertexIndex) || vertexIndex >= vertexCount) {
                fail(reason, "enemy model has a truncated or invalid index");
                return std::nullopt;
            }
            mesh.indices.push_back(vertexIndex);
        }
        mesh.bounds = {minimum, maximum};
        if (!wkc::render::validateMeshData(mesh, reason)) {
            return std::nullopt;
        }
        totalVertices += vertexCount;
        totalIndices += indexCount;
        result.submeshes.push_back(std::move(submesh));
    }
    if (reader.remaining() != 0U) {
        fail(reason, "enemy model has trailing bytes");
        return std::nullopt;
    }
    if (reason != nullptr) {
        reason->clear();
    }
    return result;
}

std::optional<EnemyModelAsset> loadEnemyModelAsset(
    const std::filesystem::path& path, std::string* const reason)
{
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error) {
        fail(reason, "cannot stat enemy model file");
        return std::nullopt;
    }
    if (size > maxFileBytes) {
        fail(reason, "enemy model exceeds 32 MiB limit");
        return std::nullopt;
    }
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        fail(reason, "cannot open enemy model file");
        return std::nullopt;
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    if (!file.read(reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()))) {
        fail(reason, "cannot read complete enemy model file");
        return std::nullopt;
    }
    if (file.peek() != std::char_traits<char>::eof()) {
        fail(reason, "enemy model file changed while reading");
        return std::nullopt;
    }
    return parseEnemyModelAsset(bytes, reason);
}

} // namespace gdp::game::visual
