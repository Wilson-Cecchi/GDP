#pragma once

#include "procedural/AssetRecipe.hpp"
#include "render/Texture.hpp"

#include <cstdint>

namespace gdp::game {

struct CeilingFixtureMaterialImages {
    wkc::render::ImageData albedo;
    wkc::render::ImageData normal;
    wkc::render::ImageData properties;
    wkc::render::ImageData emission;
};

[[nodiscard]] wkc::render::ImageData makeIndustrialCheckerImage(
    int width = 8,
    int height = 8);
[[nodiscard]] wkc::render::ImageData makeStructuralSteelImage(
    std::uint8_t condition);
[[nodiscard]] CeilingFixtureMaterialImages makeCeilingFixtureMaterialImages();
[[nodiscard]] CeilingFixtureMaterialImages makeMissionTerminalMaterialImages();

[[nodiscard]] wkc::procedural::MeshRecipe makeWorkerFoundationMeshRecipe(
    wkc::core::Seed seed = 0);
[[nodiscard]] wkc::procedural::TextureRecipe makeWorkerFoundationTextureRecipe(
    wkc::core::Seed seed = 0);

} // namespace gdp::game
