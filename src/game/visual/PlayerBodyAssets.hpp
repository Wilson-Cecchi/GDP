#pragma once

#include "game/visual/GdpRenderData.hpp"
#include "game/visual/WeaponViewmodelAssets.hpp"

#include <array>

namespace gdp::game::visual {

inline constexpr std::size_t playerBodyBoneCount = 7U;
inline constexpr std::size_t playerBodyMaterialCount = 4U;
enum class PlayerBodyMaterial : std::size_t { Cloth, Reinforcement, Leather, Rubber };

// Transient presentation state: actual post-collision distance, never input intent.
struct PlayerBodyMotion {
    float phaseRadians{0.0F};
    float weight{0.0F};
    float forward{0.0F};
    float right{0.0F};
    float airborne{0.0F};
    constexpr bool operator==(const PlayerBodyMotion&) const noexcept = default;
};

void updatePlayerBodyMotion(PlayerBodyMotion& motion,
    wkc::core::Vec3 previousFeet, wkc::core::Vec3 currentFeet,
    float yawDegrees, bool grounded, bool alive, float seconds) noexcept;

struct PlayerBodyPose {
    wkc::core::Mat4 root{wkc::core::Mat4::identity()};
    std::array<wkc::core::Mat4, playerBodyBoneCount> skin{};
    std::array<wkc::core::Vec3, 2U> hips{}, knees{}, ankles{};
};

[[nodiscard]] PlayerBodyPose samplePlayerBodyPose(const PlayerBodyRenderData& body) noexcept;
[[nodiscard]] std::array<wkc::render::MeshData, playerBodyMaterialCount> makePlayerBodyMeshes();
[[nodiscard]] WeaponMaterialImages makeUsedPlayerBodyMaterialImages(PlayerBodyMaterial material);
// Reference deformation for bounds/tests; the live renderer skins on the GPU.
[[nodiscard]] wkc::core::Vec3 deformPlayerBodyVertex(
    const wkc::render::Vertex& vertex, const PlayerBodyPose& pose) noexcept;

} // namespace gdp::game::visual
