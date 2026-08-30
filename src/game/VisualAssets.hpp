#pragma once

#include "procedural/AssetRecipe.hpp"
#include "render/Texture.hpp"

namespace gdp::game {

struct CeilingFixtureMaterialImages {
    gdp::render::ImageData albedo;
    gdp::render::ImageData normal;
    gdp::render::ImageData properties;
    gdp::render::ImageData emission;
};

[[nodiscard]] gdp::render::ImageData makeIndustrialCheckerImage(
    int width = 8,
    int height = 8);
[[nodiscard]] CeilingFixtureMaterialImages makeCeilingFixtureMaterialImages();

[[nodiscard]] gdp::procedural::MeshRecipe makeWorkerFoundationMeshRecipe(
    gdp::core::Seed seed = 0);
[[nodiscard]] gdp::procedural::TextureRecipe makeWorkerFoundationTextureRecipe(
    gdp::core::Seed seed = 0);

} // namespace gdp::game
