#pragma once

#include "game/visual/GdpRenderData.hpp"
#include "render/Mesh.hpp"

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace gdp::game::visual {

inline constexpr std::size_t playerRigBoneCount = 19U;
inline constexpr std::size_t playerRigMaterialCount = 6U;
inline constexpr std::size_t playerRigClipCount = 6U;
using PlayerRigPalette = std::array<wkc::core::Mat4, playerRigBoneCount>;

struct PlayerRigClip {
    std::string name;
    std::uint32_t startFrame{1U};
    std::vector<PlayerRigPalette> frames;
};

struct PlayerRigAsset {
    std::array<wkc::render::MeshData, playerRigMaterialCount> meshes;
    std::array<PlayerRigClip, playerRigClipCount> clips;
};

[[nodiscard]] std::optional<PlayerRigAsset> loadPlayerRigAsset(
    const std::filesystem::path& path, std::string* reason = nullptr);
[[nodiscard]] PlayerRigPalette samplePlayerRigPalette(
    const PlayerRigAsset& asset, const PlayerBodyRenderData& body,
    const WeaponViewmodelRenderData& weapon) noexcept;

} // namespace gdp::game::visual
