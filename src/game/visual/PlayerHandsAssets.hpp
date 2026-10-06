#pragma once

#include "game/visual/PlayerBodyAssets.hpp"

namespace gdp::game::visual {

// GDP presentation rig: upper arm, forearm, palm, five three-joint digits.
inline constexpr std::size_t playerHandBonesPerSide = 18U;
inline constexpr std::size_t playerHandsBoneCount = 2U * playerHandBonesPerSide;
inline constexpr std::size_t playerHandsMaterialCount = 3U;

enum class PlayerHandsMaterial : std::size_t { Cloth, Leather, Reinforcement };
enum class HandContact { Grip, Support, Magazine, Fetch, Mechanism, Shell, Ready };
struct PlayerHandsPose {
    std::array<wkc::core::Mat4, playerHandsBoneCount> skin{};
    std::array<wkc::core::Vec3, 2U> shoulders{}, elbows{}, wrists{};
    // Authored piece surfaces: inner palm, or articulated index pad for shells.
    std::array<wkc::core::Vec3, 2U> contactPoints{};
    std::array<std::array<float, 5U>, 2U> fingerFlexion{};
    HandContact rightContact{HandContact::Grip};
    HandContact leftContact{HandContact::Support};
};

[[nodiscard]] std::array<wkc::render::MeshData, playerHandsMaterialCount>
makePlayerHandsMeshes(bool includeSupportHand = true);
[[nodiscard]] WeaponMaterialImages makeUsedPlayerHandsMaterialImages(PlayerHandsMaterial material);
// Weapon-local coordinates; the weapon's camera-relative root moves both.
[[nodiscard]] PlayerHandsPose samplePlayerHandsPose(
    const WeaponViewmodelRenderData& weapon) noexcept;
[[nodiscard]] wkc::core::Vec3 deformPlayerHandsVertex(
    const wkc::render::Vertex& vertex, const PlayerHandsPose& pose) noexcept;

} // namespace gdp::game::visual
