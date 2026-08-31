#pragma once

#include "procedural/AssetRecipe.hpp"
#include "render/Texture.hpp"

#include <optional>

namespace wkc::procedural {

[[nodiscard]] std::optional<wkc::render::ImageData> generateAlbedo(
    const TextureRecipe& recipe);

} // namespace wkc::procedural
