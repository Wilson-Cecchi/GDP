#pragma once

#include "procedural/AssetRecipe.hpp"
#include "render/Texture.hpp"

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
[[nodiscard]] CeilingFixtureMaterialImages makeCeilingFixtureMaterialImages();

[[nodiscard]] wkc::procedural::MeshRecipe makeWorkerFoundationMeshRecipe(
    gdp::core::Seed seed = 0);
[[nodiscard]] wkc::procedural::TextureRecipe makeWorkerFoundationTextureRecipe(
    gdp::core::Seed seed = 0);

} // namespace gdp::game
