#pragma once

#include "render/Mesh.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace gdp::game::visual {

// ISEMESH1 stores little-endian counts and float32 values. Positions use the
// assembled model space; the pivot rotates each rigid part around its joint.
// Game axes are X right, Y up, Z front. Rigid parts 0..7 follow the normal
// enemy animation; Worker part 8 is its saw disk, with pivot at the axle.
struct EnemyModelSubmesh {
    std::uint32_t partId{0};
    std::uint32_t materialId{0};
    wkc::core::Vec3 pivot{};
    wkc::render::MeshData mesh{};
};

struct EnemyModelAsset {
    std::vector<EnemyModelSubmesh> submeshes;
};

[[nodiscard]] std::optional<EnemyModelAsset> parseEnemyModelAsset(
    std::span<const std::byte> bytes, std::string* reason = nullptr);

[[nodiscard]] std::optional<EnemyModelAsset> loadEnemyModelAsset(
    const std::filesystem::path& path, std::string* reason = nullptr);

} // namespace gdp::game::visual
