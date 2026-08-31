#include "procedural/TextureGenerator.hpp"

#include "core/Hash.hpp"

#include <algorithm>
#include <cmath>

namespace wkc::procedural {
namespace {

float unitNoise(const std::uint64_t value) noexcept
{
    return static_cast<float>(wkc::core::mix64(value) & 0xFFFFU) / 65'535.0F;
}

std::uint8_t channelToByte(const float value) noexcept
{
    return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
}

} // namespace

std::optional<wkc::render::ImageData> generateAlbedo(const TextureRecipe& recipe)
{
    if (!validateTextureRecipe(recipe).isValid()) {
        return std::nullopt;
    }

    wkc::render::ImageData image;
    image.width = static_cast<int>(recipe.resolution);
    image.height = static_cast<int>(recipe.resolution);
    const std::size_t pixelCount = static_cast<std::size_t>(recipe.resolution)
        * static_cast<std::size_t>(recipe.resolution);
    image.rgba.resize(pixelCount * 4U);
    const MaterialProperties properties = materialProperties(recipe.family);

    for (std::uint32_t y = 0; y < recipe.resolution; ++y) {
        for (std::uint32_t x = 0; x < recipe.resolution; ++x) {
            const std::uint64_t pixelId = (static_cast<std::uint64_t>(y) << 32U) | x;
            const std::size_t pixelIndex = static_cast<std::size_t>(y)
                * static_cast<std::size_t>(recipe.resolution) + x;
            const float macro = unitNoise(wkc::core::hashCombine(
                recipe.seed,
                (static_cast<std::uint64_t>(y / 8U) << 32U) | (x / 8U)));
            const float micro = unitNoise(wkc::core::hashCombine(recipe.seed, pixelId));
            const float macroBlend = recipe.macroVariation * macro;
            wkc::core::Vec3 color{
                recipe.baseColor.x + ((recipe.secondaryColor.x - recipe.baseColor.x) * macroBlend),
                recipe.baseColor.y + ((recipe.secondaryColor.y - recipe.baseColor.y) * macroBlend),
                recipe.baseColor.z + ((recipe.secondaryColor.z - recipe.baseColor.z) * macroBlend),
            };
            const float grain = (micro - 0.5F) * recipe.microVariation;
            color = color + wkc::core::Vec3{grain, grain, grain};

            const float dirtNoise = unitNoise(wkc::core::hashCombine(pixelId, recipe.seed ^ 0xD17U));
            if (dirtNoise < recipe.dirtAmount * 0.22F) {
                color = color * 0.62F;
            }
            const float wearNoise = unitNoise(wkc::core::hashCombine(pixelId, recipe.seed ^ 0xA11U));
            if (wearNoise < recipe.wearAmount * 0.16F) {
                color = color + wkc::core::Vec3{0.13F, 0.12F, 0.10F};
            }
            if (properties.oxidationAllowed) {
                const float oxidationNoise = unitNoise(wkc::core::hashCombine(
                    pixelId, recipe.seed ^ 0x0D1DA710U));
                if (oxidationNoise < recipe.oxidationAmount * 0.12F) {
                    color = {color.x * 0.78F, color.y * 0.52F, color.z * 0.32F};
                }
            }

            const std::size_t pixel = pixelIndex * 4U;
            image.rgba[pixel] = channelToByte(color.x);
            image.rgba[pixel + 1U] = channelToByte(color.y);
            image.rgba[pixel + 2U] = channelToByte(color.z);
            image.rgba[pixel + 3U] = 255U;
        }
    }
    return image;
}

} // namespace wkc::procedural
