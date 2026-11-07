#pragma once

#include "game/visual/GdpRenderData.hpp"
#include "render/Mesh.hpp"

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace gdp::game::visual {

struct PlayerHandsPose;

inline constexpr std::size_t playerRigBoneCount = 19U;
inline constexpr std::size_t playerRigMaterialCount = 6U;
inline constexpr std::size_t playerRigPartCount = 4U;
inline constexpr std::size_t playerRigMeshCount = playerRigMaterialCount * playerRigPartCount;
inline constexpr std::size_t playerExosuitMaterialCount = 8U;
inline constexpr std::size_t playerExosuitPartCount = 5U;
inline constexpr std::size_t playerExosuitMeshCount =
    playerExosuitMaterialCount * playerExosuitPartCount;
inline constexpr std::size_t playerRigClipCount = 6U;
using PlayerRigPalette = std::array<wkc::core::Mat4, playerRigBoneCount>;

struct PlayerRigClip {
    std::string name;
    std::uint32_t startFrame{1U};
    std::vector<PlayerRigPalette> frames;
};

struct PlayerRigAsset {
    std::size_t materialCount{playerRigMaterialCount};
    std::size_t partCount{playerRigPartCount};
    std::vector<wkc::render::MeshData> meshes;
    std::array<PlayerRigClip, playerRigClipCount> clips;
};

[[nodiscard]] std::optional<PlayerRigAsset> loadPlayerRigAsset(
    const std::filesystem::path& path, std::string* reason = nullptr);
[[nodiscard]] PlayerRigPalette samplePlayerRigPalette(
    const PlayerRigAsset& asset, const PlayerBodyRenderData& body,
    const WeaponViewmodelRenderData& weapon) noexcept;
[[nodiscard]] PlayerRigPalette samplePlayerRigGripPalette(
    const PlayerHandsPose& hands) noexcept;

} // namespace gdp::game::visual
