#pragma once

#include "game/visual/GdpRenderData.hpp"
#include "render/Mesh.hpp"
#include "render/Texture.hpp"

#include <array>
#include <cstddef>

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
inline constexpr std::size_t weaponRoleCount = 3U;

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
