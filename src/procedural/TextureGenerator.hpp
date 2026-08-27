#pragma once

#include "procedural/AssetRecipe.hpp"
#include "render/Texture.hpp"

#include <optional>

namespace gdp::procedural {

[[nodiscard]] std::optional<gdp::render::ImageData> generateAlbedo(
    const TextureRecipe& recipe);

} // namespace gdp::procedural
