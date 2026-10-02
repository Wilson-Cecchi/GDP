#pragma once

#include "game/visual/GdpRenderData.hpp"
#include "render/Mesh.hpp"
#include "render/Texture.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace gdp::game::visual {

enum class WeaponMaterial : std::size_t {
    CoatedSteel,
    BareSteel,
    Polymer,
    Recess,
    Sight,
    Count,
};

inline constexpr std::size_t weaponMaterialCount
    = static_cast<std::size_t>(WeaponMaterial::Count);
// Pistol, Carbine, Knife plus the two approved v1 models. SMG uses Carbine.
inline constexpr std::size_t weaponRoleCount = 5U;
inline constexpr std::uint32_t weaponGripSemanticRegion = 1U;

struct WeaponMaterialImages {
    wkc::render::ImageData albedo;
    wkc::render::ImageData normal;
    wkc::render::ImageData properties;
};

[[nodiscard]] std::array<wkc::render::MeshData, weaponMaterialCount>
makeWeaponViewmodelMeshes(HudWeaponRole role);
[[nodiscard]] WeaponMaterialImages makeUsedWeaponMaterialImages(
    WeaponMaterial material);

} // namespace gdp::game::visual
