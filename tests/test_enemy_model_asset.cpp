#include "game/visual/EnemyModelAsset.hpp"

#include <algorithm>
#include <bit>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void u32(std::vector<std::byte>& bytes, const std::uint32_t value)
{
    for (unsigned shift = 0U; shift < 32U; shift += 8U) {
        bytes.push_back(static_cast<std::byte>((value >> shift) & 0xFFU));
    }
}

void f32(std::vector<std::byte>& bytes, const float value)
{
    u32(bytes, std::bit_cast<std::uint32_t>(value));
}

void setU32(std::vector<std::byte>& bytes, const std::size_t offset,
    const std::uint32_t value)
{
    for (unsigned shift = 0U; shift < 32U; shift += 8U) {
        bytes[offset + shift / 8U] = static_cast<std::byte>((value >> shift) & 0xFFU);
    }
}

std::vector<std::byte> validTriangle()
{
    std::vector<std::byte> bytes;
    for (const char c : std::string_view{"ISEMESH1"}) {
        bytes.push_back(static_cast<std::byte>(c));
    }
    u32(bytes, 1U); // submesh count
    u32(bytes, 2U); // head part
    u32(bytes, 4U); // material
    f32(bytes, 0.0F);
    f32(bytes, 1.0F);
    f32(bytes, 0.0F); // pivot
    u32(bytes, 3U);
    u32(bytes, 3U);
    for (const auto position : std::array<wkc::core::Vec3, 3>{
             wkc::core::Vec3{0.0F, 0.0F, 0.0F},
             wkc::core::Vec3{1.0F, 0.0F, 0.0F},
             wkc::core::Vec3{0.0F, 1.0F, 0.0F}}) {
        f32(bytes, position.x);
        f32(bytes, position.y);
        f32(bytes, position.z);
        f32(bytes, 0.0F);
        f32(bytes, 0.0F);
        f32(bytes, 1.0F);
        f32(bytes, 0.25F);
        f32(bytes, 0.75F);
    }
    u32(bytes, 0U);
    u32(bytes, 1U);
    u32(bytes, 2U);
    return bytes;
}

bool rejected(const std::vector<std::byte>& bytes)
{
    std::string reason;
    return !gdp::game::visual::parseEnemyModelAsset(bytes, &reason) && !reason.empty();
}

void testValid()
{
    const auto bytes = validTriangle();
    std::string reason = "old error";
    const auto asset = gdp::game::visual::parseEnemyModelAsset(bytes, &reason);
    expect(asset.has_value() && reason.empty(), "valid little-endian triangle parses");
    if (asset) {
        expect(asset->submeshes.size() == 1U, "one submesh is returned");
        const auto& part = asset->submeshes.front();
        expect(part.partId == 2U && part.materialId == 4U
                && part.pivot == wkc::core::Vec3{0.0F, 1.0F, 0.0F},
            "part metadata and pivot are preserved");
        expect(part.mesh.vertices.size() == 3U && part.mesh.indices.size() == 3U
                && part.mesh.bounds.maximum.x == 1.0F,
            "mesh payload and derived bounds are valid");
    }

    const auto path = std::filesystem::temp_directory_path()
        / "iron_seed_enemy_model_asset_test.isemesh";
    {
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    }
    expect(gdp::game::visual::loadEnemyModelAsset(path).has_value(),
        "file wrapper reads valid mesh");
    std::error_code error;
    std::filesystem::remove(path, error);
    reason.clear();
    expect(!gdp::game::visual::loadEnemyModelAsset(path, &reason) && !reason.empty(),
        "file wrapper reports missing file");

    auto sawBytes = validTriangle();
    setU32(sawBytes, 12U, 8U);
    const auto saw = gdp::game::visual::parseEnemyModelAsset(sawBytes);
    expect(saw && saw->submeshes.front().partId == 8U,
        "Worker saw disk part is accepted with its axle pivot");
}

void testMalformed()
{
    auto bytes = validTriangle();
    bytes[0] = std::byte{'X'};
    expect(rejected(bytes), "wrong magic is rejected");

    bytes = validTriangle();
    bytes.pop_back();
    expect(rejected(bytes), "truncated index data is rejected");

    bytes = validTriangle();
    bytes.resize(24U);
    expect(rejected(bytes), "truncated submesh header is rejected");

    bytes = validTriangle();
    bytes.push_back(std::byte{0});
    expect(rejected(bytes), "trailing bytes are rejected");

    bytes = validTriangle();
    setU32(bytes, 8U, 257U);
    expect(rejected(bytes), "oversized submesh count is rejected");

    bytes = validTriangle();
    setU32(bytes, 32U, std::numeric_limits<std::uint32_t>::max());
    expect(rejected(bytes), "oversized vertex count is rejected");

    bytes = validTriangle();
    setU32(bytes, bytes.size() - 4U, 3U);
    expect(rejected(bytes), "out-of-range index is rejected");

    bytes = validTriangle();
    setU32(bytes, 12U, 10U);
    expect(rejected(bytes), "out-of-range part is rejected");

    bytes = validTriangle();
    setU32(bytes, 16U, 6U);
    expect(rejected(bytes), "out-of-range material is rejected");

    bytes = validTriangle();
    setU32(bytes, 20U, std::bit_cast<std::uint32_t>(
                            std::numeric_limits<float>::quiet_NaN()));
    expect(rejected(bytes), "non-finite pivot is rejected");

    bytes = validTriangle();
    setU32(bytes, 40U, std::bit_cast<std::uint32_t>(
                            std::numeric_limits<float>::infinity()));
    expect(rejected(bytes), "non-finite position is rejected");

    bytes = validTriangle();
    setU32(bytes, 52U, 0U);
    setU32(bytes, 56U, 0U);
    setU32(bytes, 60U, 0U);
    expect(rejected(bytes), "zero normal is rejected");

    bytes = validTriangle();
    bytes.resize(32U * 1024U * 1024U + 1U);
    expect(rejected(bytes), "file above byte limit is rejected");
}

} // namespace

int main(const int argc, char** argv)
{
    testValid();
    testMalformed();
    for (int index = 1; index < argc; ++index) {
        std::string reason;
        const auto asset = gdp::game::visual::loadEnemyModelAsset(argv[index], &reason);
        expect(asset.has_value(), std::string{"model asset parses: "} + argv[index]
                + " (" + reason + ")");
        if (asset) {
            std::size_t triangles = 0U;
            wkc::core::Vec3 minimum{1000.0F, 1000.0F, 1000.0F};
            wkc::core::Vec3 maximum{-1000.0F, -1000.0F, -1000.0F};
            std::array<bool, 10U> partPresent{};
            for (const auto& submesh : asset->submeshes) {
                triangles += submesh.mesh.indices.size() / 3U;
                partPresent[submesh.partId] = true;
                for (const auto& vertex : submesh.mesh.vertices) {
                    minimum.x = std::min(minimum.x, vertex.position.x);
                    minimum.y = std::min(minimum.y, vertex.position.y);
                    minimum.z = std::min(minimum.z, vertex.position.z);
                    maximum.x = std::max(maximum.x, vertex.position.x);
                    maximum.y = std::max(maximum.y, vertex.position.y);
                    maximum.z = std::max(maximum.z, vertex.position.z);
                }
            }
            expect(minimum.y >= -0.08F && minimum.y <= 0.08F
                    && maximum.y >= 1.65F && maximum.y <= 2.65F
                    && maximum.x - minimum.x >= 0.70F
                    && maximum.z - minimum.z >= 0.30F,
                "reference asset uses plausible assembled metre-scale bounds");
            for (std::size_t part = 0U; part < 8U; ++part) {
                expect(partPresent[part], "reference asset contains each animated part");
            }
            if (std::string_view{argv[index]}.find("worker_reference.isemesh")
                    != std::string_view::npos) {
                expect(partPresent[8U], "Worker reference contains rotating saw disk");
            }
            if (std::string_view{argv[index]}.find("guard_reference.isemesh")
                    != std::string_view::npos) {
                expect(partPresent[8U] && partPresent[9U],
                    "Guard reference has separate left and right forearms");
            }
            std::cout << argv[index] << ": " << asset->submeshes.size()
                      << " submeshes, " << triangles << " triangles\n";
        }
    }
    if (failures == 0) {
        std::cout << "enemy model asset tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
