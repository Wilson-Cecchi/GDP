#include "game/visual/RoomMaterialGenerator.hpp"

#include "core/Hash.hpp"
#include "core/Random.hpp"
#include "game/SeedDomains.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

namespace gdp::game::visual {
using wkc::procedural::MaterialFamily;
namespace {

constexpr std::size_t surfaceIndex(const IndustrialSurface surface) noexcept
{
    return static_cast<std::size_t>(surface);
}

bool isUnitValue(const float value) noexcept
{
    return std::isfinite(value) && value >= 0.0F && value <= 1.0F;
}

bool isUnitColor(const wkc::core::Vec3 color) noexcept
{
    return isUnitValue(color.x) && isUnitValue(color.y) && isUnitValue(color.z);
}

float unitNoise(const std::uint64_t value) noexcept
{
    return static_cast<float>(wkc::core::mix64(value) & 0xFFFFU) / 65'535.0F;
}

std::uint8_t channelToByte(const float value) noexcept
{
    return static_cast<std::uint8_t>(
        std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
}

wkc::core::Vec3 multiply(const wkc::core::Vec3 color, const float value) noexcept
{
    return {color.x * value, color.y * value, color.z * value};
}

wkc::core::Vec3 add(const wkc::core::Vec3 color, const float value) noexcept
{
    return {color.x + value, color.y + value, color.z + value};
}

wkc::core::Vec3 mix(
    const wkc::core::Vec3 first,
    const wkc::core::Vec3 second,
    const float amount) noexcept
{
    return {
        first.x + ((second.x - first.x) * amount),
        first.y + ((second.y - first.y) * amount),
        first.z + ((second.z - first.z) * amount),
    };
}

float scalarMix(const float first, const float second, const float amount) noexcept
{
    return first + ((second - first) * amount);
}

float smoothNoise(
    const wkc::core::Seed seed,
    const std::uint32_t x,
    const std::uint32_t y,
    const std::uint32_t cellSize) noexcept
{
    const std::uint32_t gridX = x / cellSize;
    const std::uint32_t gridY = y / cellSize;
    const float localX = static_cast<float>(x % cellSize) / static_cast<float>(cellSize);
    const float localY = static_cast<float>(y % cellSize) / static_cast<float>(cellSize);
    const float blendX = localX * localX * (3.0F - (2.0F * localX));
    const float blendY = localY * localY * (3.0F - (2.0F * localY));
    const auto sample = [seed](const std::uint32_t sampleX, const std::uint32_t sampleY) {
        const std::uint64_t id = (static_cast<std::uint64_t>(sampleY) << 32U) | sampleX;
        return unitNoise(wkc::core::hashCombine(seed, id));
    };
    const float top = scalarMix(sample(gridX, gridY), sample(gridX + 1U, gridY), blendX);
    const float bottom = scalarMix(
        sample(gridX, gridY + 1U), sample(gridX + 1U, gridY + 1U), blendX);
    return scalarMix(top, bottom, blendY);
}

// The cover texture repeats at object scale. Wrap lattice samples so that the
// painted grain is continuous across tile boundaries without a panel grid.
float coverTileNoise(
    const wkc::core::Seed seed,
    const std::uint32_t x,
    const std::uint32_t y,
    const std::uint32_t cellSize) noexcept
{
    constexpr std::uint32_t resolution = 256U;
    const std::uint32_t cells = resolution / cellSize;
    const std::uint32_t gridX = x / cellSize;
    const std::uint32_t gridY = y / cellSize;
    const float localX = static_cast<float>(x % cellSize) / static_cast<float>(cellSize);
    const float localY = static_cast<float>(y % cellSize) / static_cast<float>(cellSize);
    const float blendX = localX * localX * (3.0F - 2.0F * localX);
    const float blendY = localY * localY * (3.0F - 2.0F * localY);
    const auto sample = [seed, cells](const std::uint32_t sampleX,
                            const std::uint32_t sampleY) {
        const std::uint64_t id = (static_cast<std::uint64_t>(sampleY % cells) << 32U)
            | (sampleX % cells);
        return unitNoise(wkc::core::hashCombine(seed, id));
    };
    return scalarMix(
        scalarMix(sample(gridX, gridY), sample(gridX + 1U, gridY), blendX),
        scalarMix(sample(gridX, gridY + 1U), sample(gridX + 1U, gridY + 1U), blendX),
        blendY);
}

struct BossArenaSurfaceSample {
    wkc::core::Vec3 albedo{};
    float height{0.5F};
    float roughness{0.5F};
    float metalness{0.8F};
    float ambientOcclusion{1.0F};
};

BossArenaSurfaceSample bossArenaSurfaceSample(
    const CombatSurfaceRecipe& recipe, const std::uint32_t x,
    const std::uint32_t y) noexcept
{
    const bool wall = recipe.surface == RoomSurface::Wall;
    const bool floor = recipe.surface == RoomSurface::Floor;
    const std::uint32_t cellWidth = 256U / recipe.panelColumns;
    const std::uint32_t cellHeight = 256U / recipe.panelRows;
    const std::uint32_t panelX = x % cellWidth;
    const std::uint32_t panelY = y % cellHeight;
    const std::uint32_t edge = std::min({panelX, cellWidth - 1U - panelX,
        panelY, cellHeight - 1U - panelY});
    const std::uint64_t pixelId = (static_cast<std::uint64_t>(y) << 32U) | x;
    const std::uint64_t panelId =
        (static_cast<std::uint64_t>(y / cellHeight) << 32U)
        | (x / cellWidth);
    const float panelTone = unitNoise(wkc::core::hashCombine(
        recipe.seed ^ 0x424F5353504C4154ULL, panelId));
    const float patina = coverTileNoise(
        recipe.seed ^ 0x424F535350415449ULL, x, y, 64U);
    const float fine = coverTileNoise(
        recipe.seed ^ 0x424F535346494E45ULL, x, y, 8U);
    const float grain = unitNoise(wkc::core::hashCombine(
        recipe.seed ^ 0x424F535347524149ULL, pixelId));
    const float bodyTone = 0.89F + 0.10F * patina
        + 0.055F * (fine - 0.5F) + 0.030F * (grain - 0.5F);

    BossArenaSurfaceSample sample;
    sample.albedo = multiply(mix(recipe.baseColor, recipe.secondaryColor,
        0.12F + 0.36F * panelTone), bodyTone);
    sample.height = 0.53F + 0.045F * (patina - 0.5F)
        + 0.017F * (fine - 0.5F);
    sample.roughness = (floor ? 0.42F : wall ? 0.58F : 0.64F)
        + 0.075F * (patina - 0.5F) + 0.035F * (fine - 0.5F);
    sample.metalness = floor ? 0.88F : wall ? 0.72F : 0.78F;
    sample.ambientOcclusion = 1.0F;

    const wkc::core::Vec3 seamColor = wall
        ? wkc::core::Vec3{0.055F, 0.052F, 0.049F}
        : floor ? wkc::core::Vec3{0.032F, 0.034F, 0.035F}
                : wkc::core::Vec3{0.032F, 0.032F, 0.034F};
    if (edge <= 2U) {
        sample.albedo = seamColor;
        sample.height = 0.10F;
        sample.roughness = 0.81F;
        sample.metalness = 0.82F;
        sample.ambientOcclusion = 0.36F;
        return sample;
    }

    // The rolled lip and its shadow remain narrow at the real UV scale:
    // 3-10 texels on a 2 m floor tile or 6 m wall tile.
    if (edge <= 5U) {
        sample.albedo = multiply(sample.albedo, 0.68F);
        sample.height = 0.31F;
        sample.ambientOcclusion = 0.69F;
    } else if (edge <= 9U) {
        sample.albedo = mix(sample.albedo,
            {0.31F, 0.30F, 0.28F}, wall ? 0.12F : 0.16F);
        sample.height += 0.075F;
        sample.roughness -= 0.055F;
    }

    // Sparse strokes vary with the material seed. They are deliberately
    // narrow and short so the repeated tile does not become an obvious stamp.
    const std::uint32_t scuffCount = floor ? 10U : wall ? 6U : 3U;
    for (std::uint32_t mark = 0U; mark < scuffCount; ++mark) {
        const std::uint64_t markSeed = wkc::core::hashCombine(
            recipe.seed ^ 0x424F535353435546ULL, mark);
        const std::uint32_t startX = 18U
            + static_cast<std::uint32_t>(markSeed % 188U);
        const std::uint32_t startY = 18U
            + static_cast<std::uint32_t>((markSeed >> 17U) % 188U);
        const std::uint32_t length = 13U
            + static_cast<std::uint32_t>((markSeed >> 39U) % 36U);
        const bool stroke = floor
            ? y == startY && x >= startX && x < startX + length
            : x == startX && y >= startY && y < startY + length;
        if (stroke && edge > 10U) {
            sample.albedo = mix(sample.albedo,
                {0.34F, 0.33F, 0.30F}, floor ? 0.40F : 0.27F);
            sample.height += 0.023F;
            sample.roughness += 0.13F;
            sample.metalness = 0.91F;
            break;
        }
    }

    // The corner fasteners are sunk into the plate, with a smaller worn cap.
    const int boltInset = floor ? 17 : 13;
    const int boltX = std::min(
        std::abs(static_cast<int>(panelX) - boltInset),
        std::abs(static_cast<int>(panelX)
            - static_cast<int>(cellWidth) + boltInset + 1));
    const int boltY = std::min(
        std::abs(static_cast<int>(panelY) - boltInset),
        std::abs(static_cast<int>(panelY)
            - static_cast<int>(cellHeight) + boltInset + 1));
    const int radiusSquared = boltX * boltX + boltY * boltY;
    if (radiusSquared <= 36) {
        if (radiusSquared > 16) {
            sample.albedo = seamColor;
            sample.height = 0.18F;
            sample.roughness = 0.72F;
            sample.ambientOcclusion = 0.55F;
        } else {
            sample.albedo = {0.25F, 0.25F, 0.24F};
            sample.height = 0.60F;
            sample.roughness = 0.34F;
            sample.metalness = 0.96F;
            sample.ambientOcclusion = 0.85F;
        }
    }
    sample.roughness = std::clamp(sample.roughness, 0.08F, 1.0F);
    return sample;
}

struct IndustrialWallSample {
    wkc::core::Vec3 albedo{};
    float height{0.5F};
    float roughness{0.62F};
    float metalness{0.06F};
    float ambientOcclusion{1.0F};
    bool exposed{false};
};

struct IndustrialCeilingSample {
    wkc::core::Vec3 albedo{};
    float height{0.5F};
    float roughness{0.44F};
    float metalness{0.74F};
    float ambientOcclusion{1.0F};
};

IndustrialCeilingSample industrialCeilingSample(
    const IndustrialSurfaceRecipe& recipe,
    const std::uint32_t x,
    const std::uint32_t y,
    const std::uint32_t cellWidth,
    const std::uint32_t cellHeight) noexcept
{
    constexpr wkc::core::Vec3 recessedJoint{0.055F, 0.063F, 0.068F};
    constexpr wkc::core::Vec3 exposedSteel{0.48F, 0.51F, 0.52F};
    constexpr wkc::core::Vec3 grime{0.16F, 0.15F, 0.14F};

    IndustrialCeilingSample sample;
    const std::uint32_t panelX = x % cellWidth;
    const std::uint32_t panelY = y % cellHeight;
    const std::uint32_t edgeX = std::min(panelX, cellWidth - 1U - panelX);
    const std::uint32_t edgeY = std::min(panelY, cellHeight - 1U - panelY);
    const std::uint32_t edge = std::min(edgeX, edgeY);
    const std::uint64_t panelId = (static_cast<std::uint64_t>(y / cellHeight) << 32U)
        | (x / cellWidth);
    const std::uint64_t pixelId = (static_cast<std::uint64_t>(y) << 32U) | x;
    const std::uint32_t seamWidth = std::max(5U, recipe.resolution / 220U);
    const std::uint32_t beamWidth = std::max(24U, recipe.resolution / 38U);
    const std::uint32_t bevelWidth = beamWidth + std::max(12U, recipe.resolution / 80U);

    const float panelVariation = 0.08F + unitNoise(
        wkc::core::hashCombine(recipe.seed, panelId)) * 0.13F;
    sample.albedo = mix(recipe.baseColor, recipe.secondaryColor, panelVariation);
    const float macro = (smoothNoise(recipe.seed ^ 0x4345494CULL, x, y, 144U) - 0.5F)
        * 0.042F;
    const float brushed = (smoothNoise(recipe.seed ^ 0x42525553ULL, x, y, 9U) - 0.5F)
        * 0.018F;
    const float grain = (unitNoise(wkc::core::hashCombine(recipe.seed, pixelId)) - 0.5F)
        * 0.008F;
    sample.albedo = add(sample.albedo, macro + brushed + grain);
    sample.height = 0.50F + (brushed * 0.32F) + (grain * 0.10F);
    sample.roughness = 0.44F + (brushed * 1.2F);

    const bool seam = edge <= seamWidth;
    const bool beam = !seam && edge <= beamWidth;
    const bool bevel = !seam && !beam && edge <= bevelWidth;
    if (seam) {
        sample.albedo = recessedJoint;
        sample.height = 0.18F;
        sample.roughness = 0.76F;
        sample.metalness = 0.34F;
        sample.ambientOcclusion = 0.48F;
    } else if (beam) {
        sample.albedo = mix(sample.albedo, recipe.secondaryColor, 0.54F);
        sample.height = 0.66F;
        sample.roughness = 0.31F;
        sample.metalness = 0.88F;
    } else if (bevel) {
        const float amount = static_cast<float>(bevelWidth - edge)
            / static_cast<float>(bevelWidth - beamWidth);
        sample.albedo = mix(sample.albedo, exposedSteel, amount * 0.18F);
        sample.height = 0.52F + amount * 0.10F;
        sample.roughness = 0.36F;
        sample.metalness = 0.84F;
    }

    const std::uint32_t hatchLeft = cellWidth * 29U / 100U;
    const std::uint32_t hatchRight = cellWidth * 71U / 100U;
    const std::uint32_t hatchBottom = cellHeight * 31U / 100U;
    const std::uint32_t hatchTop = cellHeight * 69U / 100U;
    const bool insideHatch = panelX >= hatchLeft && panelX <= hatchRight
        && panelY >= hatchBottom && panelY <= hatchTop;
    if (insideHatch) {
        const std::uint32_t hatchEdge = std::min({
            panelX - hatchLeft, hatchRight - panelX,
            panelY - hatchBottom, hatchTop - panelY});
        if (hatchEdge <= seamWidth) {
            sample.albedo = recessedJoint;
            sample.height = 0.30F;
            sample.roughness = 0.70F;
            sample.metalness = 0.42F;
            sample.ambientOcclusion = 0.62F;
        } else if (hatchEdge <= seamWidth + 8U) {
            sample.albedo = mix(sample.albedo, exposedSteel, 0.22F);
            sample.height = 0.58F;
            sample.roughness = 0.34F;
            sample.metalness = 0.90F;
        } else {
            sample.albedo = mix(sample.albedo, recipe.secondaryColor, 0.13F);
            sample.height += 0.018F;
            sample.metalness = 0.76F;
        }
    }

    const std::uint32_t ventBand = std::max(10U, cellWidth / 28U);
    const bool ventColumn = panelX > bevelWidth + 16U
        && panelX < bevelWidth + 16U + ventBand * 3U
        && panelY > cellHeight / 5U && panelY < cellHeight * 4U / 5U;
    if (ventColumn) {
        const bool slot = ((panelY + static_cast<std::uint32_t>(recipe.seed))
            / std::max(8U, recipe.resolution / 96U)) % 3U == 0U;
        if (slot) {
            sample.albedo = recessedJoint;
            sample.height = 0.24F;
            sample.roughness = 0.68F;
            sample.metalness = 0.46F;
            sample.ambientOcclusion = 0.56F;
        }
    }

    const std::uint32_t boltInset = beamWidth + 14U;
    const std::array boltCenters{
        std::pair{boltInset, boltInset},
        std::pair{cellWidth - 1U - boltInset, boltInset},
        std::pair{boltInset, cellHeight - 1U - boltInset},
        std::pair{cellWidth - 1U - boltInset, cellHeight - 1U - boltInset},
    };
    const std::uint32_t boltRadius = std::max(5U, recipe.resolution / 180U);
    for (const auto& [boltX, boltY] : boltCenters) {
        const int deltaX = static_cast<int>(panelX) - static_cast<int>(boltX);
        const int deltaY = static_cast<int>(panelY) - static_cast<int>(boltY);
        const std::uint32_t distanceSquared = static_cast<std::uint32_t>(
            (deltaX * deltaX) + (deltaY * deltaY));
        if (distanceSquared <= boltRadius * boltRadius) {
            const bool core = distanceSquared <= (boltRadius / 2U) * (boltRadius / 2U);
            sample.albedo = core ? recessedJoint : exposedSteel;
            sample.height = core ? 0.43F : 0.68F;
            sample.roughness = core ? 0.39F : 0.23F;
            sample.metalness = core ? 0.82F : 0.96F;
            sample.ambientOcclusion = core ? 0.72F : 0.95F;
        }
    }

    const float dirtNoise = smoothNoise(recipe.seed ^ 0x43444952ULL, x, y, 38U);
    const float dirtAffinity = seam ? 0.55F : (insideHatch ? 0.11F : 0.045F);
    if (dirtNoise < recipe.dirtAmount * dirtAffinity) {
        sample.albedo = mix(sample.albedo, grime, seam ? 0.46F : 0.27F);
        sample.roughness = std::max(sample.roughness, 0.78F);
        sample.metalness = std::max(0.52F, sample.metalness * 0.82F);
        sample.ambientOcclusion *= 0.78F;
    }

    const bool wearZone = beam || bevel || (insideHatch && !seam);
    const float chipNoise = smoothNoise(recipe.seed ^ 0x43484950ULL, x, y, 11U);
    if (wearZone && chipNoise < recipe.wearAmount * (bevel ? 0.31F : 0.13F)) {
        sample.albedo = exposedSteel;
        sample.height -= 0.022F;
        sample.roughness = 0.25F;
        sample.metalness = 0.96F;
    }

    const float scratchNoise = unitNoise(wkc::core::hashCombine(
        recipe.seed ^ 0x43534352ULL, pixelId));
    const bool hairlineScratch = !seam
        && ((x + y * 2U) % 241U == 0U || x % 173U == 0U)
        && scratchNoise < recipe.wearAmount * 0.42F;
    if (hairlineScratch) {
        sample.albedo = mix(sample.albedo, exposedSteel, 0.46F);
        sample.height -= 0.012F;
        sample.roughness = 0.29F;
        sample.metalness = 0.94F;
    }

    sample.roughness = std::clamp(sample.roughness, 0.08F, 1.0F);
    sample.metalness = std::clamp(sample.metalness, 0.0F, 1.0F);
    sample.ambientOcclusion = std::clamp(sample.ambientOcclusion, 0.0F, 1.0F);
    return sample;
}

IndustrialWallSample industrialWallSample(
    const IndustrialSurfaceRecipe& recipe,
    const std::uint32_t x,
    const std::uint32_t y,
    const std::uint32_t cellWidth,
    const std::uint32_t cellHeight,
    const std::uint32_t stripeHeight) noexcept
{
    constexpr wkc::core::Vec3 jointColor{24.0F / 255.0F, 24.0F / 255.0F, 23.0F / 255.0F};
    constexpr wkc::core::Vec3 exposedSteel{
        100.0F / 255.0F, 98.0F / 255.0F, 94.0F / 255.0F};
    constexpr wkc::core::Vec3 grimeColor{73.0F / 255.0F, 56.0F / 255.0F, 43.0F / 255.0F};
    constexpr wkc::core::Vec3 stripeBlack{0.055F, 0.060F, 0.065F};

    IndustrialWallSample sample;
    const std::uint32_t panelX = x % cellWidth;
    const std::uint32_t panelY = y % cellHeight;
    const std::uint64_t panelId = (static_cast<std::uint64_t>(y / cellHeight) << 32U)
        | (x / cellWidth);
    const std::uint64_t pixelId = (static_cast<std::uint64_t>(y) << 32U) | x;

    const std::uint32_t edgeX = std::min(panelX, cellWidth - 1U - panelX);
    const std::uint32_t edgeY = std::min(panelY, cellHeight - 1U - panelY);
    const std::uint32_t edge = std::min(edgeX, edgeY);
    const std::uint32_t gasketWidth = std::max(4U, recipe.resolution / 256U);
    const std::uint32_t frameWidth = std::max(18U, recipe.resolution / 48U);
    const std::uint32_t bevelWidth = frameWidth + std::max(10U, recipe.resolution / 96U);
    const bool seam = edge <= gasketWidth;
    const bool frame = !seam && edge <= frameWidth;
    const bool bevel = !seam && !frame && edge <= bevelWidth;

    const float panelBlend = 0.08F + unitNoise(
        wkc::core::hashCombine(recipe.seed, panelId)) * 0.18F;
    sample.albedo = mix(recipe.baseColor, recipe.secondaryColor, panelBlend);
    const float macro = (smoothNoise(recipe.seed ^ 0x4D414352U, x, y, 128U) - 0.5F)
        * 0.055F;
    const float orangePeel = (smoothNoise(recipe.seed ^ 0x5041494EU, x, y, 12U) - 0.5F)
        * 0.024F;
    const std::uint32_t brushBand = static_cast<std::uint32_t>(
        (static_cast<std::uint64_t>(x) + (recipe.seed & 15U)) % 17U);
    const float brush = (static_cast<float>(brushBand) - 8.0F) * 0.00035F;
    const float grain = (unitNoise(wkc::core::hashCombine(recipe.seed, pixelId)) - 0.5F)
        * 0.009F;
    sample.albedo = add(sample.albedo, macro + orangePeel + brush + grain);
    sample.height = 0.50F + orangePeel * 0.42F + grain * 0.12F;
    sample.roughness = 0.70F + macro * 0.7F + orangePeel * 1.4F;
    sample.metalness = 0.05F;

    if (seam) {
        sample.albedo = jointColor;
        sample.height = 0.20F;
        sample.roughness = 0.76F;
        sample.metalness = 0.16F;
        sample.ambientOcclusion = 0.54F;
    } else if (frame) {
        sample.albedo = mix(sample.albedo, recipe.secondaryColor, 0.62F);
        sample.height = 0.59F;
        sample.roughness = 0.48F;
        sample.metalness = 0.30F;
    } else if (bevel) {
        const float bevelAmount = static_cast<float>(bevelWidth - edge)
            / static_cast<float>(bevelWidth - frameWidth);
        sample.albedo = mix(sample.albedo, exposedSteel, bevelAmount * 0.13F);
        sample.height = 0.50F + bevelAmount * 0.08F;
        sample.roughness -= bevelAmount * 0.06F;
    }

    const bool insetRib = edge >= bevelWidth + 14U && edge <= bevelWidth + 23U;
    if (insetRib) {
        sample.albedo = mix(sample.albedo, recipe.secondaryColor, 0.28F);
        sample.height = 0.535F;
        sample.roughness = 0.54F;
        sample.metalness = 0.16F;
    }

    const bool reinforcedCorner = !seam && !frame
        && edgeX < 82U && edgeY < 82U && edgeX + edgeY < 104U;
    if (reinforcedCorner) {
        sample.albedo = mix(sample.albedo, recipe.secondaryColor, 0.48F);
        sample.height = 0.585F;
        sample.roughness = 0.46F;
        sample.metalness = 0.32F;
    }

    const bool baseRail = y >= stripeHeight && y < stripeHeight + 44U;
    const bool topRail = y + 70U >= recipe.resolution;
    if (baseRail || topRail) {
        sample.albedo = mix(sample.albedo, recipe.secondaryColor, 0.68F);
        sample.height = 0.61F;
        sample.roughness = 0.47F;
        sample.metalness = 0.34F;
    }

    const std::uint32_t plateLeft = cellWidth * 64U / 100U;
    const std::uint32_t plateRight = cellWidth * 88U / 100U;
    const std::uint32_t plateBottom = cellHeight * 34U / 100U;
    const std::uint32_t plateTop = cellHeight * 58U / 100U;
    const bool platePanel = ((x / cellWidth) % 2U) == 1U && (y / cellHeight) == 0U;
    const bool onPlate = platePanel && panelX >= plateLeft && panelX <= plateRight
        && panelY >= plateBottom && panelY <= plateTop;
    if (onPlate) {
        const std::uint32_t plateEdge = std::min({
            panelX - plateLeft, plateRight - panelX,
            panelY - plateBottom, plateTop - panelY});
        sample.albedo = mix(sample.albedo, recipe.secondaryColor,
            plateEdge < 8U ? 0.72F : 0.24F);
        sample.height = plateEdge < 8U ? 0.63F : 0.60F;
        sample.roughness = 0.52F;
        sample.metalness = 0.20F;
    }

    const bool stencil = panelX >= bevelWidth + 38U && panelX <= bevelWidth + 142U
        && panelY + 78U >= cellHeight && panelY + 64U <= cellHeight
        && ((panelId ^ recipe.seed) & 1U) == 0U;
    if (stencil) {
        const bool dash = ((panelX - bevelWidth) / 18U) % 2U == 0U;
        sample.albedo = dash
            ? mix(sample.albedo, jointColor, 0.62F)
            : mix(sample.albedo, recipe.accentColor, 0.30F);
        sample.roughness = 0.68F;
        sample.metalness = 0.04F;
    }

    if (onPlate) {
        const std::array plateHoles{
            std::pair{plateLeft + (plateRight - plateLeft) * 38U / 100U,
                plateBottom + (plateTop - plateBottom) / 2U},
            std::pair{plateLeft + (plateRight - plateLeft) * 66U / 100U,
                plateBottom + (plateTop - plateBottom) / 2U},
        };
        const std::uint32_t holeRadius = std::max(8U, recipe.resolution / 128U);
        for (const auto& [holeX, holeY] : plateHoles) {
            const int deltaX = static_cast<int>(panelX) - static_cast<int>(holeX);
            const int deltaY = static_cast<int>(panelY) - static_cast<int>(holeY);
            const std::uint32_t distanceSquared = static_cast<std::uint32_t>(
                (deltaX * deltaX) + (deltaY * deltaY));
            if (distanceSquared <= holeRadius * holeRadius) {
                const bool core = distanceSquared
                    <= (holeRadius / 2U) * (holeRadius / 2U);
                sample.albedo = core ? jointColor : exposedSteel;
                sample.height = core ? 0.42F : 0.66F;
                sample.roughness = core ? 0.50F : 0.27F;
                sample.metalness = core ? 0.58F : 0.92F;
                sample.ambientOcclusion = core ? 0.58F : 0.90F;
            }
        }
    }

    const std::uint32_t boltInset = frameWidth + 12U;
    const std::array boltCenters{
        std::pair{boltInset, boltInset},
        std::pair{cellWidth - 1U - boltInset, boltInset},
        std::pair{boltInset, cellHeight - 1U - boltInset},
        std::pair{cellWidth - 1U - boltInset, cellHeight - 1U - boltInset},
    };
    const std::uint32_t boltRadius = std::max(6U, recipe.resolution / 150U);
    for (const auto& [boltX, boltY] : boltCenters) {
        const int deltaX = static_cast<int>(panelX) - static_cast<int>(boltX);
        const int deltaY = static_cast<int>(panelY) - static_cast<int>(boltY);
        const std::uint32_t distanceSquared = static_cast<std::uint32_t>(
            (deltaX * deltaX) + (deltaY * deltaY));
        if (distanceSquared <= boltRadius * boltRadius) {
            const bool core = distanceSquared <= (boltRadius / 2U) * (boltRadius / 2U);
            sample.albedo = core ? jointColor : exposedSteel;
            sample.height = core ? 0.48F : 0.67F;
            sample.roughness = core ? 0.42F : 0.28F;
            sample.metalness = core ? 0.68F : 0.90F;
            sample.ambientOcclusion = core ? 0.70F : 0.94F;
        }
    }

    const float dirtNoise = smoothNoise(recipe.seed ^ 0xD17D17U, x, y, 42U);
    const bool lowerContactZone = y < recipe.resolution / 3U;
    const float dirtAffinity = seam ? 0.58F : (lowerContactZone ? 0.26F : 0.028F);
    if (dirtNoise < recipe.dirtAmount * dirtAffinity) {
        const float dirtStrength = seam ? 0.54F : 0.34F;
        sample.albedo = mix(sample.albedo, grimeColor, dirtStrength);
        sample.roughness = std::max(sample.roughness, 0.82F);
        sample.metalness *= 0.30F;
        sample.ambientOcclusion *= 0.78F;
    }

    if (recipe.safetyStripe && y < stripeHeight) {
        const std::uint32_t stripePeriod = std::max(72U, recipe.resolution / 12U);
        const bool yellow = ((x + (y * 2U)) / stripePeriod) % 2U == 0U;
        sample.albedo = yellow ? recipe.accentColor : stripeBlack;
        sample.height = 0.57F + orangePeel * 0.28F;
        sample.roughness = yellow ? 0.64F : 0.70F;
        sample.metalness = 0.10F;
        sample.ambientOcclusion = 0.92F;
    }

    const bool edgeWearZone = bevel || frame || (y < stripeHeight);
    const float chipNoise = smoothNoise(recipe.seed ^ 0xAEA412U, x, y, 10U);
    const float chipThreshold = (bevel ? 0.34F : 0.16F) * recipe.wearAmount;
    if (edgeWearZone && !seam && chipNoise < chipThreshold) {
        sample.albedo = exposedSteel;
        sample.height -= 0.025F;
        sample.roughness = 0.31F;
        sample.metalness = 0.88F;
        sample.exposed = true;
    }

    const float scratchNoise = unitNoise(wkc::core::hashCombine(
        recipe.seed ^ 0x53435241U, pixelId));
    const bool hairlineScratch = !seam && !frame
        && (x % 113U == 0U || (x + (y * 3U)) % 257U == 0U)
        && scratchNoise < recipe.wearAmount * 0.48F;
    if (hairlineScratch) {
        sample.albedo = mix(sample.albedo, exposedSteel, 0.34F);
        sample.height -= 0.014F;
        sample.roughness = std::max(0.34F, sample.roughness - 0.10F);
    }

    sample.roughness = std::clamp(sample.roughness, 0.08F, 1.0F);
    sample.metalness = std::clamp(sample.metalness, 0.0F, 1.0F);
    sample.ambientOcclusion = std::clamp(sample.ambientOcclusion, 0.0F, 1.0F);
    return sample;
}

std::uint64_t addFloat(const std::uint64_t hash, const float value) noexcept
{
    return wkc::core::hashCombine(hash, std::bit_cast<std::uint32_t>(value));
}

std::uint64_t addVec3(std::uint64_t hash, const wkc::core::Vec3 value) noexcept
{
    hash = addFloat(hash, value.x);
    hash = addFloat(hash, value.y);
    return addFloat(hash, value.z);
}

IndustrialCondition selectCondition(const wkc::core::Seed roomSeed) noexcept
{
    wkc::core::RandomStream random{wkc::core::deriveSeed(
        roomSeed, gdp::game::SeedDomain::RoomMaterial, 0)};
    const int roll = random.rangeInt(0, 99);
    if (roll < 20) {
        return IndustrialCondition::Maintained;
    }
    if (roll < 80) {
        return IndustrialCondition::Used;
    }
    return IndustrialCondition::Worn;
}

std::pair<float, float> conditionAmounts(
    const IndustrialCondition condition,
    wkc::core::RandomStream& random) noexcept
{
    switch (condition) {
    case IndustrialCondition::Maintained:
        return {random.rangeFloat(0.04F, 0.14F), random.rangeFloat(0.03F, 0.12F)};
    case IndustrialCondition::Used:
        return {random.rangeFloat(0.18F, 0.38F), random.rangeFloat(0.14F, 0.34F)};
    case IndustrialCondition::Worn:
        return {random.rangeFloat(0.42F, 0.68F), random.rangeFloat(0.34F, 0.58F)};
    case IndustrialCondition::Count:
        break;
    }
    return {};
}

IndustrialSurfaceRecipe makeSurfaceRecipe(
    const wkc::core::Seed roomSeed,
    const IndustrialCondition condition,
    const IndustrialSurface surface)
{
    const std::uint64_t localId = static_cast<std::uint64_t>(surface) + 1U;
    const wkc::core::Seed seed = wkc::core::deriveSeed(
        roomSeed, gdp::game::SeedDomain::RoomMaterial, localId);
    wkc::core::RandomStream random{seed};
    const auto [wear, dirt] = conditionAmounts(condition, random);

    IndustrialSurfaceRecipe recipe;
    recipe.seed = seed;
    recipe.surface = surface;
    recipe.wearAmount = wear;
    recipe.dirtAmount = dirt;
    recipe.resolution = 64;

    const float tone = random.rangeFloat(0.94F, 1.06F);
    switch (surface) {
    case IndustrialSurface::Wall:
        recipe.version = 2;
        recipe.resolution = 1024;
        recipe.family = MaterialFamily::PaintedSteel;
        recipe.baseColor = multiply({0.33F, 0.35F, 0.36F}, tone);
        recipe.secondaryColor = multiply({0.20F, 0.21F, 0.22F}, tone);
        recipe.accentColor = {0.83F, 0.60F, 0.0F};
        recipe.panelColumns = static_cast<std::uint8_t>(random.rangeInt(2, 4));
        recipe.panelRows = 2;
        recipe.safetyStripe = true;
        break;
    case IndustrialSurface::Floor:
        recipe.family = MaterialFamily::BareSteel;
        recipe.baseColor = multiply({0.21F, 0.22F, 0.23F}, tone);
        recipe.secondaryColor = multiply({0.15F, 0.16F, 0.17F}, tone);
        recipe.accentColor = {0.34F, 0.36F, 0.37F};
        recipe.panelColumns = static_cast<std::uint8_t>(random.chance(0.5F) ? 2 : 4);
        recipe.panelRows = 4;
        recipe.safetyStripe = false;
        break;
    case IndustrialSurface::Ceiling:
        recipe.version = 2;
        recipe.resolution = 1024;
        recipe.family = MaterialFamily::BareSteel;
        recipe.baseColor = multiply({0.38F, 0.40F, 0.42F}, tone);
        recipe.secondaryColor = multiply({0.20F, 0.22F, 0.23F}, tone);
        recipe.accentColor = {0.83F, 0.60F, 0.0F};
        recipe.panelColumns = 2;
        recipe.panelRows = static_cast<std::uint8_t>(random.rangeInt(2, 4));
        recipe.emissiveAmount = random.rangeFloat(0.38F, 0.62F);
        recipe.safetyStripe = false;
        break;
    case IndustrialSurface::Count:
        break;
    }
    return recipe;
}

StorageCondition selectStorageCondition(const wkc::core::Seed roomSeed) noexcept
{
    wkc::core::RandomStream random{wkc::core::deriveSeed(
        roomSeed, gdp::game::SeedDomain::RoomMaterial, 100)};
    const int roll = random.rangeInt(0, 99);
    if (roll < 18) {
        return StorageCondition::Orderly;
    }
    if (roll < 78) {
        return StorageCondition::Dusty;
    }
    return StorageCondition::Neglected;
}

struct StorageConditionAmounts {
    float wear;
    float dirt;
    float oxidation;
};

StorageConditionAmounts storageConditionAmounts(
    const StorageCondition condition,
    wkc::core::RandomStream& random) noexcept
{
    switch (condition) {
    case StorageCondition::Orderly:
        return {random.rangeFloat(0.10F, 0.24F), random.rangeFloat(0.14F, 0.28F),
            random.rangeFloat(0.03F, 0.10F)};
    case StorageCondition::Dusty:
        return {random.rangeFloat(0.26F, 0.48F), random.rangeFloat(0.40F, 0.64F),
            random.rangeFloat(0.09F, 0.20F)};
    case StorageCondition::Neglected:
        return {random.rangeFloat(0.50F, 0.74F), random.rangeFloat(0.62F, 0.84F),
            random.rangeFloat(0.22F, 0.42F)};
    case StorageCondition::Count:
        break;
    }
    return {};
}

StorageSurfaceRecipe makeStorageSurfaceRecipe(
    const wkc::core::Seed roomSeed,
    const StorageCondition condition,
    const RoomSurface surface)
{
    const std::uint64_t localId = static_cast<std::uint64_t>(surface) + 101U;
    const wkc::core::Seed seed = wkc::core::deriveSeed(
        roomSeed, gdp::game::SeedDomain::RoomMaterial, localId);
    wkc::core::RandomStream random{seed};
    const StorageConditionAmounts amounts = storageConditionAmounts(condition, random);

    StorageSurfaceRecipe recipe;
    recipe.seed = seed;
    recipe.surface = surface;
    recipe.wearAmount = amounts.wear;
    recipe.dirtAmount = amounts.dirt;
    recipe.oxidationAmount = amounts.oxidation;
    recipe.detailAmount = random.rangeFloat(0.12F, 0.28F);
    recipe.resolution = 64;

    const float tone = random.rangeFloat(0.92F, 1.06F);
    switch (surface) {
    case RoomSurface::Wall:
        recipe.family = MaterialFamily::PaintedSteel;
        recipe.baseColor = multiply({0.36F, 0.32F, 0.27F}, tone);
        recipe.secondaryColor = multiply({0.22F, 0.21F, 0.20F}, tone);
        recipe.accentColor = {0.45F, 0.28F, 0.18F};
        recipe.panelColumns = static_cast<std::uint8_t>(random.rangeInt(2, 3));
        recipe.panelRows = 2;
        break;
    case RoomSurface::Floor:
        recipe.family = MaterialFamily::Ceramic;
        recipe.baseColor = multiply({0.33F, 0.33F, 0.32F}, tone);
        recipe.secondaryColor = multiply({0.25F, 0.25F, 0.24F}, tone);
        recipe.accentColor = {0.44F, 0.41F, 0.36F};
        recipe.panelColumns = 2;
        recipe.panelRows = 2;
        recipe.oxidationAmount = 0.0F;
        break;
    case RoomSurface::Ceiling:
        recipe.family = MaterialFamily::BareSteel;
        recipe.baseColor = multiply({0.16F, 0.16F, 0.15F}, tone);
        recipe.secondaryColor = multiply({0.10F, 0.11F, 0.10F}, tone);
        recipe.accentColor = {0.70F, 0.67F, 0.56F};
        recipe.panelColumns = 2;
        recipe.panelRows = static_cast<std::uint8_t>(random.rangeInt(2, 3));
        recipe.emissiveAmount = random.rangeFloat(0.16F, 0.30F);
        break;
    case RoomSurface::Count:
        break;
    }
    return recipe;
}

CombatCondition selectCombatCondition(const wkc::core::Seed roomSeed) noexcept
{
    wkc::core::RandomStream random{wkc::core::deriveSeed(
        roomSeed, gdp::game::SeedDomain::RoomMaterial, 200)};
    const int roll = random.rangeInt(0, 99);
    if (roll < 18) {
        return CombatCondition::Ready;
    }
    if (roll < 78) {
        return CombatCondition::Active;
    }
    return CombatCondition::Scarred;
}

std::pair<float, float> combatConditionAmounts(
    const CombatCondition condition,
    wkc::core::RandomStream& random) noexcept
{
    switch (condition) {
    case CombatCondition::Ready:
        return {random.rangeFloat(0.12F, 0.26F), random.rangeFloat(0.04F, 0.10F)};
    case CombatCondition::Active:
        return {random.rangeFloat(0.30F, 0.52F), random.rangeFloat(0.12F, 0.24F)};
    case CombatCondition::Scarred:
        return {random.rangeFloat(0.56F, 0.78F), random.rangeFloat(0.28F, 0.44F)};
    case CombatCondition::Count:
        break;
    }
    return {};
}

CombatSurfaceRecipe makeCombatSurfaceRecipe(
    const wkc::core::Seed roomSeed,
    const CombatCondition condition,
    const RoomSurface surface)
{
    const std::uint64_t localId = static_cast<std::uint64_t>(surface) + 201U;
    const wkc::core::Seed seed = wkc::core::deriveSeed(
        roomSeed, gdp::game::SeedDomain::RoomMaterial, localId);
    wkc::core::RandomStream random{seed};
    const auto [wear, impacts] = combatConditionAmounts(condition, random);

    CombatSurfaceRecipe recipe;
    recipe.seed = seed;
    recipe.surface = surface;
    recipe.wearAmount = wear;
    recipe.impactAmount = impacts;
    recipe.reinforcementAmount = random.rangeFloat(0.34F, 0.62F);
    recipe.markingAmount = random.rangeFloat(0.08F, 0.18F);
    recipe.resolution = 64;

    const float tone = random.rangeFloat(0.94F, 1.06F);
    switch (surface) {
    case RoomSurface::Wall:
        recipe.family = MaterialFamily::PaintedSteel;
        recipe.baseColor = multiply({0.31F, 0.35F, 0.24F}, tone);
        recipe.secondaryColor = multiply({0.22F, 0.24F, 0.17F}, tone);
        recipe.accentColor = {0.71F, 0.51F, 0.07F};
        recipe.panelColumns = static_cast<std::uint8_t>(random.rangeInt(1, 4));
        recipe.panelRows = 2;
        break;
    case RoomSurface::Floor:
        recipe.family = MaterialFamily::Ceramic;
        recipe.baseColor = multiply({0.24F, 0.24F, 0.21F}, tone);
        recipe.secondaryColor = multiply({0.16F, 0.16F, 0.15F}, tone);
        recipe.accentColor = {0.55F, 0.40F, 0.08F};
        recipe.panelColumns = 2;
        recipe.panelRows = 2;
        recipe.reinforcementAmount = 0.0F;
        break;
    case RoomSurface::Ceiling:
        recipe.family = MaterialFamily::BareSteel;
        recipe.baseColor = multiply({0.15F, 0.16F, 0.14F}, tone);
        recipe.secondaryColor = multiply({0.08F, 0.09F, 0.08F}, tone);
        recipe.accentColor = {0.78F, 0.72F, 0.58F};
        recipe.panelColumns = 2;
        recipe.panelRows = static_cast<std::uint8_t>(random.rangeInt(2, 3));
        recipe.impactAmount = 0.0F;
        recipe.markingAmount = 0.0F;
        recipe.emissiveAmount = random.rangeFloat(0.24F, 0.42F);
        break;
    case RoomSurface::Count:
        break;
    }
    return recipe;
}

CafeteriaCondition selectCafeteriaCondition(const wkc::core::Seed roomSeed) noexcept
{
    wkc::core::RandomStream random{wkc::core::deriveSeed(
        roomSeed, gdp::game::SeedDomain::RoomMaterial, 300)};
    const int roll = random.rangeInt(0, 99);
    if (roll < 22) {
        return CafeteriaCondition::Clean;
    }
    if (roll < 84) {
        return CafeteriaCondition::InUse;
    }
    return CafeteriaCondition::Faded;
}

std::pair<float, float> cafeteriaConditionAmounts(
    const CafeteriaCondition condition,
    wkc::core::RandomStream& random) noexcept
{
    switch (condition) {
    case CafeteriaCondition::Clean:
        return {random.rangeFloat(0.04F, 0.12F), random.rangeFloat(0.02F, 0.08F)};
    case CafeteriaCondition::InUse:
        return {random.rangeFloat(0.14F, 0.30F), random.rangeFloat(0.08F, 0.18F)};
    case CafeteriaCondition::Faded:
        return {random.rangeFloat(0.32F, 0.48F), random.rangeFloat(0.16F, 0.28F)};
    case CafeteriaCondition::Count:
        break;
    }
    return {};
}

CafeteriaSurfaceRecipe makeCafeteriaSurfaceRecipe(
    const wkc::core::Seed roomSeed,
    const CafeteriaCondition condition,
    const RoomSurface surface)
{
    const std::uint64_t localId = static_cast<std::uint64_t>(surface) + 301U;
    const wkc::core::Seed seed = wkc::core::deriveSeed(
        roomSeed, gdp::game::SeedDomain::RoomMaterial, localId);
    wkc::core::RandomStream random{seed};
    const auto [wear, stain] = cafeteriaConditionAmounts(condition, random);

    CafeteriaSurfaceRecipe recipe;
    recipe.seed = seed;
    recipe.surface = surface;
    recipe.wearAmount = wear;
    recipe.stainAmount = stain;
    recipe.resolution = 64;

    const float tone = random.rangeFloat(0.96F, 1.04F);
    switch (surface) {
    case RoomSurface::Wall:
        recipe.family = MaterialFamily::Ceramic;
        recipe.baseColor = multiply({0.78F, 0.78F, 0.76F}, tone);
        recipe.secondaryColor = multiply({0.62F, 0.62F, 0.61F}, tone);
        recipe.accentColor = {0.62F, 0.18F, 0.18F};
        recipe.panelColumns = static_cast<std::uint8_t>(random.rangeInt(2, 4));
        recipe.panelRows = 2;
        recipe.stripeAmount = random.rangeFloat(0.08F, 0.12F);
        break;
    case RoomSurface::Floor:
        recipe.family = MaterialFamily::Ceramic;
        recipe.baseColor = multiply({0.41F, 0.42F, 0.42F}, tone);
        recipe.secondaryColor = multiply({0.33F, 0.35F, 0.35F}, tone);
        recipe.accentColor = {0.47F, 0.48F, 0.48F};
        recipe.panelColumns = static_cast<std::uint8_t>(random.chance(0.55F) ? 2 : 4);
        recipe.panelRows = recipe.panelColumns;
        recipe.stripeAmount = 0.0F;
        recipe.furnitureMarkAmount = random.rangeFloat(0.08F, 0.18F);
        break;
    case RoomSurface::Ceiling:
        recipe.family = MaterialFamily::Aluminum;
        recipe.baseColor = multiply({0.62F, 0.62F, 0.61F}, tone);
        recipe.secondaryColor = multiply({0.43F, 0.44F, 0.43F}, tone);
        recipe.accentColor = {0.92F, 0.90F, 0.82F};
        recipe.panelColumns = 2;
        recipe.panelRows = static_cast<std::uint8_t>(random.rangeInt(2, 4));
        recipe.stripeAmount = 0.0F;
        recipe.stainAmount *= 0.45F;
        recipe.emissiveAmount = random.rangeFloat(0.48F, 0.68F);
        break;
    case RoomSurface::Count:
        break;
    }
    return recipe;
}

LaboratoryCondition selectLaboratoryCondition(const wkc::core::Seed roomSeed) noexcept
{
    wkc::core::RandomStream random{wkc::core::deriveSeed(
        roomSeed, gdp::game::SeedDomain::RoomMaterial, 400)};
    const int roll = random.rangeInt(0, 99);
    if (roll < 24) {
        return LaboratoryCondition::Sterile;
    }
    if (roll < 86) {
        return LaboratoryCondition::Operational;
    }
    return LaboratoryCondition::Contained;
}

std::pair<float, float> laboratoryConditionAmounts(
    const LaboratoryCondition condition,
    wkc::core::RandomStream& random) noexcept
{
    switch (condition) {
    case LaboratoryCondition::Sterile:
        return {random.rangeFloat(0.02F, 0.08F), random.rangeFloat(0.0F, 0.02F)};
    case LaboratoryCondition::Operational:
        return {random.rangeFloat(0.08F, 0.18F), random.rangeFloat(0.01F, 0.05F)};
    case LaboratoryCondition::Contained:
        return {random.rangeFloat(0.16F, 0.28F), random.rangeFloat(0.08F, 0.16F)};
    case LaboratoryCondition::Count:
        break;
    }
    return {};
}

LaboratorySurfaceRecipe makeLaboratorySurfaceRecipe(
    const wkc::core::Seed roomSeed,
    const LaboratoryCondition condition,
    const RoomSurface surface)
{
    const std::uint64_t localId = static_cast<std::uint64_t>(surface) + 401U;
    const wkc::core::Seed seed = wkc::core::deriveSeed(
        roomSeed, gdp::game::SeedDomain::RoomMaterial, localId);
    wkc::core::RandomStream random{seed};
    const auto [wear, contamination] = laboratoryConditionAmounts(condition, random);

    LaboratorySurfaceRecipe recipe;
    recipe.seed = seed;
    recipe.surface = surface;
    recipe.wearAmount = wear;
    recipe.detailAmount = random.rangeFloat(0.10F, 0.22F);
    recipe.contaminationAmount = contamination;
    recipe.resolution = 64;

    const float tone = random.rangeFloat(0.97F, 1.03F);
    switch (surface) {
    case RoomSurface::Wall:
        recipe.family = MaterialFamily::Ceramic;
        recipe.baseColor = multiply({0.78F, 0.80F, 0.80F}, tone);
        recipe.secondaryColor = multiply({0.66F, 0.68F, 0.68F}, tone);
        recipe.accentColor = {0.27F, 0.41F, 0.54F};
        recipe.panelColumns = static_cast<std::uint8_t>(random.rangeInt(2, 6));
        recipe.panelRows = 2;
        recipe.contaminationAmount = 0.0F;
        break;
    case RoomSurface::Floor:
        recipe.family = MaterialFamily::Polymer;
        recipe.baseColor = multiply({0.20F, 0.27F, 0.36F}, tone);
        recipe.secondaryColor = multiply({0.15F, 0.21F, 0.28F}, tone);
        recipe.accentColor = {0.25F, 0.33F, 0.42F};
        recipe.panelColumns = static_cast<std::uint8_t>(random.chance(0.55F) ? 2 : 4);
        recipe.panelRows = recipe.panelColumns;
        break;
    case RoomSurface::Ceiling:
        recipe.family = MaterialFamily::Aluminum;
        recipe.baseColor = multiply({0.66F, 0.68F, 0.68F}, tone);
        recipe.secondaryColor = multiply({0.41F, 0.44F, 0.45F}, tone);
        recipe.accentColor = {0.91F, 0.94F, 0.94F};
        recipe.panelColumns = 2;
        recipe.panelRows = static_cast<std::uint8_t>(random.rangeInt(2, 4));
        recipe.contaminationAmount = 0.0F;
        recipe.lightingCoverage = random.rangeFloat(0.25F, 0.34F);
        recipe.emissiveAmount = random.rangeFloat(0.62F, 0.82F);
        break;
    case RoomSurface::Count:
        break;
    }
    return recipe;
}

TechnicalCondition selectTechnicalCondition(const wkc::core::Seed roomSeed) noexcept
{
    wkc::core::RandomStream random{wkc::core::deriveSeed(
        roomSeed, gdp::game::SeedDomain::RoomMaterial, 500)};
    const int roll = random.rangeInt(0, 99);
    if (roll < 20) {
        return TechnicalCondition::Serviced;
    }
    if (roll < 82) {
        return TechnicalCondition::Working;
    }
    return TechnicalCondition::Overloaded;
}

struct TechnicalConditionAmounts {
    float wear;
    float dirt;
    float oil;
};

TechnicalConditionAmounts technicalConditionAmounts(
    const TechnicalCondition condition,
    wkc::core::RandomStream& random) noexcept
{
    switch (condition) {
    case TechnicalCondition::Serviced:
        return {random.rangeFloat(0.12F, 0.24F), random.rangeFloat(0.12F, 0.26F),
            random.rangeFloat(0.05F, 0.10F)};
    case TechnicalCondition::Working:
        return {random.rangeFloat(0.28F, 0.48F), random.rangeFloat(0.30F, 0.52F),
            random.rangeFloat(0.10F, 0.20F)};
    case TechnicalCondition::Overloaded:
        return {random.rangeFloat(0.50F, 0.70F), random.rangeFloat(0.52F, 0.74F),
            random.rangeFloat(0.20F, 0.34F)};
    case TechnicalCondition::Count:
        break;
    }
    return {};
}

TechnicalSurfaceRecipe makeTechnicalSurfaceRecipe(
    const wkc::core::Seed roomSeed,
    const TechnicalCondition condition,
    const RoomSurface surface)
{
    const std::uint64_t localId = static_cast<std::uint64_t>(surface) + 501U;
    const wkc::core::Seed seed = wkc::core::deriveSeed(
        roomSeed, gdp::game::SeedDomain::RoomMaterial, localId);
    wkc::core::RandomStream random{seed};
    const TechnicalConditionAmounts amounts = technicalConditionAmounts(condition, random);

    TechnicalSurfaceRecipe recipe;
    recipe.seed = seed;
    recipe.surface = surface;
    recipe.wearAmount = amounts.wear;
    recipe.dirtAmount = amounts.dirt;
    recipe.oilAmount = amounts.oil;
    // v4 keeps room-scale atlases but draws broad steel sheets instead of
    // repeating one-metre plates. The ceiling retains its neutral v2 tile.
    recipe.resolution = surface == RoomSurface::Ceiling ? 128U : 64U;
    recipe.version = surface == RoomSurface::Ceiling ? 2U : 4U;

    const float tone = random.rangeFloat(0.94F, 1.05F);
    switch (surface) {
    case RoomSurface::Wall:
        recipe.family = MaterialFamily::PaintedSteel;
        recipe.baseColor = multiply({0.35F, 0.34F, 0.32F}, tone);
        recipe.secondaryColor = multiply({0.29F, 0.28F, 0.27F}, tone);
        recipe.accentColor = {0.63F, 0.43F, 0.15F};
        recipe.panelColumns = 2;
        recipe.panelRows = 2;
        recipe.pipeAmount = 0.0F;
        recipe.oilAmount = 0.0F;
        recipe.mechanicalAmount = random.rangeFloat(0.12F, 0.28F);
        break;
    case RoomSurface::Floor:
        recipe.family = MaterialFamily::BareSteel;
        recipe.baseColor = multiply({0.105F, 0.11F, 0.12F}, tone);
        recipe.secondaryColor = multiply({0.075F, 0.08F, 0.09F}, tone);
        recipe.accentColor = {0.38F, 0.40F, 0.39F};
        recipe.panelColumns = 2;
        recipe.panelRows = 2;
        recipe.pipeAmount = 0.0F;
        recipe.mechanicalAmount = random.rangeFloat(0.10F, 0.24F);
        break;
    case RoomSurface::Ceiling:
        recipe.family = MaterialFamily::BareSteel;
        recipe.baseColor = multiply({0.19F, 0.21F, 0.22F}, tone);
        recipe.secondaryColor = multiply({0.13F, 0.15F, 0.16F}, tone);
        recipe.accentColor = {0.30F, 0.32F, 0.33F};
        recipe.panelColumns = 2;
        recipe.panelRows = 2;
        recipe.pipeAmount = 0.0F;
        recipe.oilAmount = 0.0F;
        recipe.mechanicalAmount = random.rangeFloat(0.35F, 0.60F);
        recipe.emissiveAmount = 0.0F;
        break;
    case RoomSurface::Count:
        break;
    }
    return recipe;
}

SecurityCondition selectSecurityCondition(const wkc::core::Seed roomSeed) noexcept
{
    wkc::core::RandomStream random{wkc::core::deriveSeed(
        roomSeed, gdp::game::SeedDomain::RoomMaterial, 600)};
    const int roll = random.rangeInt(0, 99);
    if (roll < 24) {
        return SecurityCondition::Secured;
    }
    if (roll < 88) {
        return SecurityCondition::Monitored;
    }
    return SecurityCondition::Alert;
}

std::pair<float, float> securityConditionAmounts(
    const SecurityCondition condition,
    wkc::core::RandomStream& random) noexcept
{
    switch (condition) {
    case SecurityCondition::Secured:
        return {random.rangeFloat(0.04F, 0.10F), random.rangeFloat(0.0F, 0.03F)};
    case SecurityCondition::Monitored:
        return {random.rangeFloat(0.10F, 0.20F), random.rangeFloat(0.03F, 0.08F)};
    case SecurityCondition::Alert:
        return {random.rangeFloat(0.18F, 0.30F), random.rangeFloat(0.20F, 0.40F)};
    case SecurityCondition::Count:
        break;
    }
    return {};
}

SecuritySurfaceRecipe makeSecuritySurfaceRecipe(
    const wkc::core::Seed roomSeed,
    const SecurityCondition condition,
    const RoomSurface surface)
{
    const std::uint64_t localId = static_cast<std::uint64_t>(surface) + 601U;
    const wkc::core::Seed seed = wkc::core::deriveSeed(
        roomSeed, gdp::game::SeedDomain::RoomMaterial, localId);
    wkc::core::RandomStream random{seed};
    const auto [wear, alert] = securityConditionAmounts(condition, random);

    SecuritySurfaceRecipe recipe;
    recipe.seed = seed;
    recipe.surface = surface;
    recipe.wearAmount = wear;
    recipe.alertAmount = alert;
    recipe.resolution = 64;

    const float tone = random.rangeFloat(0.96F, 1.04F);
    switch (surface) {
    case RoomSurface::Wall:
        recipe.family = MaterialFamily::PaintedSteel;
        recipe.baseColor = multiply({0.23F, 0.24F, 0.25F}, tone);
        recipe.secondaryColor = multiply({0.15F, 0.16F, 0.17F}, tone);
        recipe.accentColor = {0.23F, 0.40F, 0.47F};
        recipe.panelColumns = static_cast<std::uint8_t>(random.rangeInt(2, 6));
        recipe.panelRows = 2;
        recipe.monitorAmount = random.rangeFloat(0.12F, 0.28F);
        recipe.statusAmount = random.rangeFloat(0.10F, 0.22F);
        break;
    case RoomSurface::Floor:
        recipe.family = MaterialFamily::BareSteel;
        recipe.baseColor = multiply({0.24F, 0.25F, 0.27F}, tone);
        recipe.secondaryColor = multiply({0.13F, 0.15F, 0.15F}, tone);
        recipe.accentColor = {0.35F, 0.37F, 0.38F};
        recipe.panelColumns = static_cast<std::uint8_t>(random.chance(0.55F) ? 2 : 4);
        recipe.panelRows = recipe.panelColumns;
        recipe.monitorAmount = 0.0F;
        recipe.statusAmount = 0.0F;
        recipe.accessAmount = random.rangeFloat(0.08F, 0.18F);
        recipe.alertAmount = 0.0F;
        break;
    case RoomSurface::Ceiling:
        recipe.family = MaterialFamily::Aluminum;
        recipe.baseColor = multiply({0.33F, 0.35F, 0.36F}, tone);
        recipe.secondaryColor = multiply({0.20F, 0.21F, 0.22F}, tone);
        recipe.accentColor = {0.86F, 0.87F, 0.84F};
        recipe.panelColumns = 2;
        recipe.panelRows = static_cast<std::uint8_t>(random.rangeInt(2, 3));
        recipe.monitorAmount = 0.0F;
        recipe.statusAmount = random.rangeFloat(0.05F, 0.14F);
        recipe.alertAmount = 0.0F;
        recipe.emissiveAmount = random.rangeFloat(0.52F, 0.72F);
        break;
    case RoomSurface::Count:
        break;
    }
    return recipe;
}

struct CentralHubSelection {
    CentralHubCondition condition;
    CentralHubRouteAxis routeAxis;
};

CentralHubSelection selectCentralHubState(const wkc::core::Seed roomSeed) noexcept
{
    wkc::core::RandomStream random{wkc::core::deriveSeed(
        roomSeed, gdp::game::SeedDomain::RoomMaterial, 700)};
    const int roll = random.rangeInt(0, 99);
    CentralHubCondition condition = CentralHubCondition::Active;
    if (roll < 28) {
        condition = CentralHubCondition::Open;
    } else if (roll >= 88) {
        condition = CentralHubCondition::Rerouted;
    }
    const auto routeAxis = static_cast<CentralHubRouteAxis>(random.rangeInt(
        0, static_cast<int>(CentralHubRouteAxis::Count) - 1));
    return {condition, routeAxis};
}

struct CentralHubConditionAmounts {
    float wear;
    float navigation;
    float focal;
};

CentralHubConditionAmounts centralHubConditionAmounts(
    const CentralHubCondition condition,
    wkc::core::RandomStream& random) noexcept
{
    switch (condition) {
    case CentralHubCondition::Open:
        return {random.rangeFloat(0.02F, 0.08F), random.rangeFloat(0.08F, 0.12F),
            random.rangeFloat(0.025F, 0.045F)};
    case CentralHubCondition::Active:
        return {random.rangeFloat(0.07F, 0.14F), random.rangeFloat(0.11F, 0.17F),
            random.rangeFloat(0.035F, 0.065F)};
    case CentralHubCondition::Rerouted:
        return {random.rangeFloat(0.12F, 0.20F), random.rangeFloat(0.15F, 0.22F),
            random.rangeFloat(0.05F, 0.08F)};
    case CentralHubCondition::Count:
        break;
    }
    return {};
}

CentralHubSurfaceRecipe makeCentralHubSurfaceRecipe(
    const wkc::core::Seed roomSeed,
    const CentralHubCondition condition,
    const CentralHubRouteAxis routeAxis,
    const RoomSurface surface)
{
    const std::uint64_t localId = static_cast<std::uint64_t>(surface) + 701U;
    const wkc::core::Seed seed = wkc::core::deriveSeed(
        roomSeed, gdp::game::SeedDomain::RoomMaterial, localId);
    wkc::core::RandomStream random{seed};
    const CentralHubConditionAmounts amounts = centralHubConditionAmounts(condition, random);

    CentralHubSurfaceRecipe recipe;
    recipe.seed = seed;
    recipe.surface = surface;
    recipe.routeAxis = routeAxis;
    recipe.wearAmount = amounts.wear;
    recipe.structureAmount = random.rangeFloat(0.15F, 0.25F);
    recipe.navigationAmount = amounts.navigation;
    recipe.focalAmount = amounts.focal;
    recipe.resolution = 64;

    const float tone = random.rangeFloat(0.97F, 1.03F);
    switch (surface) {
    case RoomSurface::Wall:
        recipe.family = MaterialFamily::PaintedSteel;
        recipe.baseColor = multiply({0.275F, 0.294F, 0.302F}, tone);
        recipe.secondaryColor = multiply({0.173F, 0.188F, 0.196F}, tone);
        recipe.accentColor = {0.169F, 0.537F, 0.580F};
        recipe.panelColumns = static_cast<std::uint8_t>(random.rangeInt(1, 3));
        recipe.panelRows = 2;
        break;
    case RoomSurface::Floor:
        recipe.family = MaterialFamily::BareSteel;
        recipe.baseColor = multiply({0.196F, 0.212F, 0.220F}, tone);
        recipe.secondaryColor = multiply({0.161F, 0.176F, 0.184F}, tone);
        recipe.accentColor = {0.153F, 0.431F, 0.463F};
        recipe.panelColumns = 2;
        recipe.panelRows = static_cast<std::uint8_t>(random.chance(0.6F) ? 2 : 4);
        recipe.structureAmount = 0.0F;
        recipe.focalAmount = 0.0F;
        break;
    case RoomSurface::Ceiling:
        recipe.family = MaterialFamily::BareSteel;
        recipe.baseColor = multiply({0.173F, 0.188F, 0.196F}, tone);
        recipe.secondaryColor = multiply({0.090F, 0.106F, 0.114F}, tone);
        recipe.accentColor = {0.82F, 0.88F, 0.88F};
        recipe.panelColumns = 2;
        recipe.panelRows = 2;
        recipe.navigationAmount = 0.0F;
        recipe.focalAmount = 0.0F;
        recipe.emissiveAmount = random.rangeFloat(0.42F, 0.62F);
        break;
    case RoomSurface::Count:
        break;
    }
    return recipe;
}

void writePixel(
    wkc::render::ImageData& image,
    const std::uint32_t x,
    const std::uint32_t y,
    const wkc::core::Vec3 color)
{
    const std::size_t index = (static_cast<std::size_t>(y)
        * static_cast<std::size_t>(image.width) + x) * 4U;
    image.rgba[index] = channelToByte(color.x);
    image.rgba[index + 1U] = channelToByte(color.y);
    image.rgba[index + 2U] = channelToByte(color.z);
    image.rgba[index + 3U] = 255U;
}

struct CorridorSample {
    wkc::core::Vec3 albedo{};
    float height{0.5F};
    float roughness{0.55F};
    float metalness{0.72F};
    float occlusion{1.0F};
};

CorridorSample officeSample(
    const CorridorSurfaceRecipe& recipe, const std::uint32_t x,
    const std::uint32_t y) noexcept
{
    const bool wall = recipe.surface == RoomSurface::Wall;
    const bool floor = recipe.surface == RoomSurface::Floor;
    const std::uint64_t pixel = (static_cast<std::uint64_t>(y) << 32U) | x;
    const float grain = (unitNoise(wkc::core::hashCombine(recipe.seed, pixel))
        - 0.5F) * 0.034F;
    const float patina = coverTileNoise(
        recipe.seed ^ 0x4F46464943455054ULL, x, y, 64U) - 0.5F;
    CorridorSample sample;
    sample.albedo = add(recipe.baseColor, grain + patina * 0.050F);
    sample.height = 0.53F + grain * 0.2F + patina * 0.025F;
    sample.roughness = (wall ? 0.54F : floor ? 0.48F : 0.62F)
        + patina * 0.075F;
    sample.metalness = wall ? 0.67F : 0.79F;

    if (wall) {
        // One broad, recessed wall cassette per tile. The lower service panel
        // is a separate finish, rather than the corridor's narrow sheets.
        const bool rim = x < 3U || x > 252U || y < 3U || y > 252U;
        const bool inset = x < 8U || x > 247U || y < 8U || y > 247U;
        if (rim) {
            sample.albedo = mix(recipe.secondaryColor, recipe.baseColor, 0.42F);
            sample.height = 0.26F;
            sample.occlusion = 0.61F;
        } else if (inset) {
            sample.albedo = mix(sample.albedo, recipe.secondaryColor, 0.22F);
            sample.height = 0.64F;
            sample.roughness = 0.43F;
        }
        if (y >= 184U && y < 240U && x >= 10U && x <= 245U) {
            sample.albedo = mix(recipe.secondaryColor, recipe.baseColor,
                0.47F + patina * 0.10F);
            sample.height = 0.59F;
            sample.roughness = 0.47F;
        }
        if (y == 181U || y == 239U) {
            sample.albedo = mix(recipe.secondaryColor, recipe.baseColor, 0.30F);
            sample.height = 0.25F;
            sample.occlusion = 0.64F;
        }
        if (y >= 185U && y <= 187U && x >= 20U && x <= 235U) {
            sample.albedo = recipe.accentColor;
            sample.height = 0.61F;
            sample.roughness = 0.51F;
        }
        // Fine horizontal brushing is only visible close to the surface.
        if (y > 18U && y < 176U && y % 13U == 0U) {
            sample.albedo = mix(sample.albedo, recipe.secondaryColor, 0.08F);
        }
        const bool boltX = (x >= 16U && x <= 19U)
            || (x >= 236U && x <= 239U);
        const bool boltY = (y >= 16U && y <= 19U)
            || (y >= 170U && y <= 173U)
            || (y >= 228U && y <= 231U);
        if (boltX && boltY) {
            sample.albedo = {0.15F, 0.18F, 0.18F};
            sample.height = 0.72F;
            sample.metalness = 0.9F;
        }
    } else if (floor) {
        // Wide offset metal plates; no repeated corridor grip strips or rails.
        const std::uint32_t row = y / 128U;
        const std::uint32_t localX = (x + row * 64U) % 128U;
        const std::uint32_t localY = y % 128U;
        if (localX <= 3U || localY <= 3U) {
            sample.albedo = recipe.secondaryColor;
            sample.height = 0.19F;
            sample.occlusion = 0.48F;
        } else if (localX <= 7U || localY <= 7U) {
            sample.albedo = mix(sample.albedo, recipe.secondaryColor, 0.34F);
            sample.height = 0.66F;
        }
        if (localX >= 14U && localX <= 18U
            && localY >= 14U && localY <= 18U) {
            sample.albedo = recipe.secondaryColor;
            sample.height = 0.69F;
        }
    } else {
        // Large ceiling cassette with a recessed central service channel.
        if (x < 5U || x > 250U || y < 5U || y > 250U) {
            sample.albedo = recipe.secondaryColor;
            sample.height = 0.18F;
            sample.occlusion = 0.49F;
        } else if (x >= 108U && x <= 148U) {
            sample.albedo = mix(recipe.secondaryColor, recipe.baseColor, 0.18F);
            sample.height = 0.34F;
            sample.roughness = 0.73F;
        }
    }

    const float wear = unitNoise(wkc::core::hashCombine(
        recipe.seed ^ 0x4F46464943455752ULL, pixel));
    if (wear < recipe.wearAmount * 0.018F) {
        sample.albedo = mix(sample.albedo, {0.53F, 0.55F, 0.53F}, 0.38F);
        sample.roughness = 0.4F;
    }
    const float dirt = unitNoise(wkc::core::hashCombine(
        recipe.seed ^ 0x4F46464943454454ULL, pixel));
    if (dirt < recipe.dirtAmount * 0.025F) {
        sample.albedo = multiply(sample.albedo, 0.74F);
        sample.roughness = 0.79F;
    }
    return sample;
}

CorridorSample corridorSample(
    const CorridorSurfaceRecipe& recipe, const std::uint32_t x,
    const std::uint32_t y) noexcept
{
    if (recipe.pattern == CorridorSurfacePattern::Office) {
        return officeSample(recipe, x, y);
    }
    const bool transit = recipe.pattern == CorridorSurfacePattern::StructuralTransit;
    const bool wall = recipe.surface == RoomSurface::Wall;
    const bool floor = recipe.surface == RoomSurface::Floor;
    const std::uint64_t pixel = (static_cast<std::uint64_t>(y) << 32U) | x;
    const float patina = coverTileNoise(
        recipe.seed ^ 0x434F525250415449ULL, x, y, 64U) - 0.5F;
    const float fine = coverTileNoise(
        recipe.seed ^ 0x434F525246494E45ULL, x, y, 8U) - 0.5F;
    const float grain = (unitNoise(wkc::core::hashCombine(recipe.seed, pixel)) - 0.5F)
        * 0.015F;
    // The service wall has two broad 2 m cassettes per tile; transit uses one
    // calmer 4 m cassette. Structural portals and rail lights are separate 3D.
    const std::uint32_t wallPanel = transit ? 256U : 128U;
    const std::uint32_t section = wall ? x / wallPanel
        : floor ? y / 128U : x / 128U;
    const float sectionTone = (unitNoise(wkc::core::hashCombine(
        recipe.seed ^ 0x434F525250414E4CULL, section)) - 0.5F)
        * (transit ? 0.045F : 0.055F);
    CorridorSample sample;
    sample.albedo = add(recipe.baseColor,
        grain + sectionTone + patina * 0.043F + fine * 0.014F);
    sample.height = 0.52F + patina * 0.045F + fine * 0.025F;
    sample.roughness = wall ? 0.67F : floor ? 0.54F : 0.72F;
    sample.roughness += patina * 0.09F + fine * 0.04F;
    sample.metalness = wall ? 0.18F : floor ? 0.78F : 0.50F;

    if (wall) {
        const std::uint32_t seam = x % wallPanel;
        const std::uint32_t opposite = wallPanel - 1U - seam;
        const std::uint32_t edge = std::min({seam, opposite, y, 255U - y});
        if (edge <= 2U) {
            sample.albedo = recipe.secondaryColor;
            sample.height = 0.13F;
            sample.roughness = 0.84F;
            sample.occlusion = 0.34F;
        } else if (edge <= 7U) {
            sample.albedo = multiply(sample.albedo, 0.80F);
            sample.height = 0.35F;
            sample.occlusion = 0.76F;
        }
        if (!transit && y >= 210U && y < 248U) {
            // Dark replaceable impact skirt. The amber route line sits on a
            // distinct physical rail, so it is absent from this albedo.
            sample.albedo = mix(recipe.secondaryColor, recipe.baseColor, 0.12F);
            sample.height = 0.58F;
            sample.roughness = 0.51F;
            sample.metalness = 0.81F;
        }
        if (!transit && (y == 209U || y == 248U)) {
            sample.albedo = recipe.secondaryColor;
            sample.height = 0.18F;
            sample.occlusion = 0.52F;
        }
        // Sparse recessed fasteners belong only to actual panel corners.
        const int cornerX = std::min(
            std::abs(static_cast<int>(seam) - 16),
            std::abs(static_cast<int>(opposite) - 16));
        const int cornerY = std::min(
            std::abs(static_cast<int>(y) - 16),
            std::abs(static_cast<int>(y) - 239));
        if (cornerX * cornerX + cornerY * cornerY <= 15) {
            sample.albedo = {0.16F, 0.16F, 0.15F};
            sample.height = 0.27F;
            sample.roughness = 0.42F;
            sample.metalness = 0.90F;
            sample.occlusion = 0.64F;
        }
    } else if (floor) {
        // Broad graphite plates; the hatches and rail are modeled separately.
        const std::uint32_t seam = y % 128U;
        if (seam <= 2U || x <= 2U || x >= 253U) {
            sample.albedo = recipe.secondaryColor;
            sample.height = 0.14F;
            sample.occlusion = 0.38F;
        } else if (seam <= 7U || x <= 7U || x >= 248U) {
            sample.albedo = multiply(sample.albedo, 0.78F);
            sample.height = 0.40F;
            sample.occlusion = 0.76F;
        }
    } else {
        // A neutral ceiling behind physical ducts, cable trays and fixtures.
        const std::uint32_t edge = x % 128U;
        if (edge <= 3U || edge >= 125U || y <= 2U || y >= 253U) {
            sample.albedo = recipe.secondaryColor;
            sample.height = 0.18F;
            sample.occlusion = 0.47F;
        }
    }

    // Scuffs cluster near seams and the base, avoiding uniform random specks.
    const std::uint32_t nearbyEdge = wall
        ? std::min(x % wallPanel, wallPanel - 1U - x % wallPanel)
        : floor ? std::min(y % 128U, 127U - y % 128U)
                : std::min(x % 128U, 127U - x % 128U);
    const float wear = coverTileNoise(
        recipe.seed ^ 0x434F525257454152ULL, x, y, 16U);
    if (nearbyEdge >= 5U && nearbyEdge <= 20U
        && wear < 0.18F + recipe.wearAmount * 0.54F) {
        sample.albedo = mix(sample.albedo, {0.35F, 0.36F, 0.35F}, 0.26F);
        sample.roughness = 0.47F;
        sample.metalness = std::max(sample.metalness, 0.63F);
    }
    const float dirt = coverTileNoise(
        recipe.seed ^ 0x434F525244495254ULL, x, y, 32U);
    if (dirt < 0.16F + recipe.dirtAmount * 0.50F
        && (!wall || y >= 192U)) {
        sample.albedo = multiply(sample.albedo, 0.84F);
        sample.roughness = 0.82F;
    }
    sample.roughness = std::clamp(sample.roughness, 0.08F, 1.0F);
    return sample;
}

} // namespace

bool IndustrialMaterialValidationResult::isValid() const noexcept
{
    return errors.empty();
}

bool CorridorMaterialValidationResult::isValid() const noexcept
{
    return errors.empty();
}

bool StorageMaterialValidationResult::isValid() const noexcept
{
    return errors.empty();
}

bool CombatMaterialValidationResult::isValid() const noexcept
{
    return errors.empty();
}

bool CafeteriaMaterialValidationResult::isValid() const noexcept
{
    return errors.empty();
}

bool LaboratoryMaterialValidationResult::isValid() const noexcept
{
    return errors.empty();
}

bool TechnicalMaterialValidationResult::isValid() const noexcept
{
    return errors.empty();
}

bool SecurityMaterialValidationResult::isValid() const noexcept
{
    return errors.empty();
}

bool CentralHubMaterialValidationResult::isValid() const noexcept
{
    return errors.empty();
}

IndustrialRoomMaterialSet generateIndustrialRoomMaterials(const wkc::core::Seed roomSeed)
{
    IndustrialRoomMaterialSet materials;
    materials.roomSeed = roomSeed;
    materials.condition = selectCondition(roomSeed);
    materials.surfaces = {
        makeSurfaceRecipe(roomSeed, materials.condition, IndustrialSurface::Wall),
        makeSurfaceRecipe(roomSeed, materials.condition, IndustrialSurface::Floor),
        makeSurfaceRecipe(roomSeed, materials.condition, IndustrialSurface::Ceiling),
    };
    materials.materialHash = industrialRoomMaterialHash(materials);
    return materials;
}

CorridorRoomMaterialSet generateCorridorRoomMaterials(
    const wkc::core::Seed corridorSeed,
    const CorridorSurfacePattern pattern)
{
    CorridorRoomMaterialSet materials;
    materials.corridorSeed = corridorSeed;
    materials.condition = static_cast<CorridorCondition>(
        wkc::core::mix64(corridorSeed ^ 0x434F52524D415456ULL) % 3U);
    for (std::size_t i = 0; i < materials.surfaces.size(); ++i) {
        CorridorSurfaceRecipe& recipe = materials.surfaces[i];
        recipe.seed = wkc::core::deriveSeed(
            corridorSeed, gdp::game::SeedDomain::Corridor, 0x4D415400ULL + i);
        recipe.surface = static_cast<RoomSurface>(i);
        recipe.pattern = pattern;
        recipe.family = i == 0U ? MaterialFamily::PaintedSteel : MaterialFamily::BareSteel;
        if (pattern == CorridorSurfacePattern::StructuralTransit) {
            recipe.baseColor = i == 0U ? wkc::core::Vec3{0.17F, 0.23F, 0.27F}
                : i == 1U ? wkc::core::Vec3{0.080F, 0.094F, 0.104F}
                          : wkc::core::Vec3{0.10F, 0.12F, 0.13F};
            recipe.secondaryColor = {0.040F, 0.052F, 0.058F};
            recipe.accentColor = {0.12F, 0.56F, 0.62F};
        } else if (pattern == CorridorSurfacePattern::ServiceCorridor) {
            recipe.baseColor = i == 0U ? wkc::core::Vec3{0.22F, 0.205F, 0.18F}
                : i == 1U ? wkc::core::Vec3{0.075F, 0.073F, 0.068F}
                          : wkc::core::Vec3{0.11F, 0.108F, 0.102F};
            recipe.secondaryColor = {0.043F, 0.044F, 0.043F};
            recipe.accentColor = {0.58F, 0.29F, 0.075F};
        }
        constexpr std::array wear{0.07F, 0.25F, 0.52F};
        constexpr std::array dirt{0.05F, 0.20F, 0.43F};
        const std::size_t condition = static_cast<std::size_t>(materials.condition);
        recipe.wearAmount = wear[condition];
        recipe.dirtAmount = dirt[condition];
    }
    materials.materialHash = hashCorridorRoomMaterials(materials);
    return materials;
}

CorridorMaterialValidationResult validateCorridorSurfaceRecipe(
    const CorridorSurfaceRecipe& recipe)
{
    CorridorMaterialValidationResult result;
    if (recipe.pattern >= CorridorSurfacePattern::Count) {
        result.errors.push_back(CorridorMaterialValidationError::InvalidParameter);
    }
    if (recipe.surface >= RoomSurface::Count) {
        result.errors.push_back(CorridorMaterialValidationError::InvalidSurface);
    }
    const MaterialFamily requiredFamily = recipe.surface == RoomSurface::Wall
        ? MaterialFamily::PaintedSteel : MaterialFamily::BareSteel;
    if (recipe.family != requiredFamily) {
        result.errors.push_back(CorridorMaterialValidationError::InvalidFamily);
    }
    if (!isUnitColor(recipe.baseColor) || !isUnitColor(recipe.secondaryColor)
        || !isUnitColor(recipe.accentColor)) {
        result.errors.push_back(CorridorMaterialValidationError::InvalidColor);
    }
    if (!isUnitValue(recipe.wearAmount) || !isUnitValue(recipe.dirtAmount)
        || recipe.emissiveAmount != 0.0F) {
        result.errors.push_back(CorridorMaterialValidationError::InvalidParameter);
    }
    if (recipe.version != 2U || recipe.resolution != 256U) {
        result.errors.push_back(CorridorMaterialValidationError::InvalidResolution);
    }
    return result;
}

CorridorMaterialValidationResult validateCorridorRoomMaterials(
    const CorridorRoomMaterialSet& materials)
{
    CorridorMaterialValidationResult result;
    if (materials.condition >= CorridorCondition::Count) {
        result.errors.push_back(CorridorMaterialValidationError::InvalidCondition);
    }
    for (std::size_t i = 0; i < materials.surfaces.size(); ++i) {
        const CorridorSurfaceRecipe& recipe = materials.surfaces[i];
        const auto checked = validateCorridorSurfaceRecipe(recipe);
        result.errors.insert(result.errors.end(),
            checked.errors.begin(), checked.errors.end());
        if (recipe.surface != static_cast<RoomSurface>(i)
            || recipe.seed != wkc::core::deriveSeed(materials.corridorSeed,
                gdp::game::SeedDomain::Corridor, 0x4D415400ULL + i)) {
            result.errors.push_back(CorridorMaterialValidationError::InvalidSurfaceOrder);
        }
    }
    if (materials.materialHash == 0U
        || materials.materialHash != hashCorridorRoomMaterials(materials)) {
        result.errors.push_back(CorridorMaterialValidationError::InvalidMaterialHash);
    }
    return result;
}

std::uint64_t corridorSurfaceRecipeHash(const CorridorSurfaceRecipe& recipe) noexcept
{
    std::uint64_t hash = wkc::core::hashCombine(0x434F525253555246ULL, recipe.version);
    hash = wkc::core::hashCombine(hash, recipe.seed);
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.surface));
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.pattern));
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.family));
    hash = addVec3(hash, recipe.baseColor);
    hash = addVec3(hash, recipe.secondaryColor);
    hash = addVec3(hash, recipe.accentColor);
    hash = addFloat(hash, recipe.wearAmount);
    hash = addFloat(hash, recipe.dirtAmount);
    hash = addFloat(hash, recipe.emissiveAmount);
    return wkc::core::hashCombine(hash, recipe.resolution);
}

std::uint64_t hashCorridorRoomMaterials(
    const CorridorRoomMaterialSet& materials) noexcept
{
    std::uint64_t hash = wkc::core::hashCombine(
        0x434F5252524F4F4DULL, materials.corridorSeed);
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(materials.condition));
    for (const CorridorSurfaceRecipe& recipe : materials.surfaces) {
        hash = wkc::core::hashCombine(hash, corridorSurfaceRecipeHash(recipe));
    }
    return hash;
}

std::optional<CorridorSurfaceMaterialImages>
generateCorridorSurfaceMaterialImages(const CorridorSurfaceRecipe& recipe)
{
    if (!validateCorridorSurfaceRecipe(recipe).isValid()) {
        return std::nullopt;
    }
    CorridorSurfaceMaterialImages images;
    for (wkc::render::ImageData* const image
        : {&images.albedo, &images.normal, &images.properties}) {
        image->width = static_cast<int>(recipe.resolution);
        image->height = static_cast<int>(recipe.resolution);
        image->rgba.resize(static_cast<std::size_t>(recipe.resolution)
            * recipe.resolution * 4U);
    }
    std::vector<float> heights(static_cast<std::size_t>(recipe.resolution)
        * recipe.resolution);
    for (std::uint32_t y = 0; y < recipe.resolution; ++y) {
        for (std::uint32_t x = 0; x < recipe.resolution; ++x) {
            const CorridorSample sample = corridorSample(recipe, x, y);
            writePixel(images.albedo, x, y, sample.albedo);
            writePixel(images.properties, x, y,
                {sample.roughness, sample.metalness, sample.occlusion});
            heights[static_cast<std::size_t>(y) * recipe.resolution + x] = sample.height;
        }
    }
    const auto at = [&](std::uint32_t x, std::uint32_t y) {
        return heights[static_cast<std::size_t>(y) * recipe.resolution + x];
    };
    for (std::uint32_t y = 0; y < recipe.resolution; ++y) {
        for (std::uint32_t x = 0; x < recipe.resolution; ++x) {
            const std::uint32_t left = x == 0U ? 0U : x - 1U;
            const std::uint32_t right = std::min(x + 1U, recipe.resolution - 1U);
            const std::uint32_t down = y == 0U ? 0U : y - 1U;
            const std::uint32_t up = std::min(y + 1U, recipe.resolution - 1U);
            const wkc::core::Vec3 normal = wkc::core::normalized({
                (at(left, y) - at(right, y)) * 2.6F,
                (at(x, down) - at(x, up)) * 2.6F,
                1.0F,
            });
            writePixel(images.normal, x, y, {
                normal.x * 0.5F + 0.5F,
                normal.y * 0.5F + 0.5F,
                normal.z * 0.5F + 0.5F,
            });
        }
    }
    return images;
}

IndustrialSurfaceRecipe makeIndustrialWallArchetypeRecipe(
    const IndustrialCondition condition)
{
    IndustrialSurfaceRecipe recipe;
    recipe.version = 3;
    recipe.seed = 0x48445F57414C4C30ULL + static_cast<std::uint64_t>(condition);
    recipe.surface = IndustrialSurface::Wall;
    recipe.family = MaterialFamily::PaintedSteel;
    // Neutral graphite stays distinct from the rusty Storage palette.
    // The dark joints and controlled roughness give
    // the wide plates depth under both intact and broken fixtures.
    recipe.baseColor = {0.19F, 0.18F, 0.17F};
    recipe.secondaryColor = {0.13F, 0.125F, 0.12F};
    recipe.accentColor = {0.83F, 0.60F, 0.0F};
    recipe.panelColumns = 2U;
    recipe.panelRows = 2U;
    recipe.safetyStripe = true;
    recipe.resolution = 1024U;
    switch (condition) {
    case IndustrialCondition::Maintained:
        recipe.wearAmount = 0.09F;
        recipe.dirtAmount = 0.07F;
        break;
    case IndustrialCondition::Used:
        recipe.wearAmount = 0.29F;
        recipe.dirtAmount = 0.24F;
        break;
    case IndustrialCondition::Worn:
        recipe.wearAmount = 0.56F;
        recipe.dirtAmount = 0.48F;
        break;
    case IndustrialCondition::Count:
        recipe.wearAmount = 0.29F;
        recipe.dirtAmount = 0.24F;
        break;
    }
    return recipe;
}

IndustrialSurfaceRecipe makeIndustrialCeilingArchetypeRecipe(
    const IndustrialCondition condition)
{
    IndustrialSurfaceRecipe recipe;
    recipe.version = 2;
    recipe.seed = 0x48445F4345494C00ULL + static_cast<std::uint64_t>(condition);
    recipe.surface = IndustrialSurface::Ceiling;
    recipe.family = MaterialFamily::BareSteel;
    recipe.baseColor = {0.38F, 0.40F, 0.42F};
    recipe.secondaryColor = {0.20F, 0.22F, 0.23F};
    recipe.accentColor = {0.83F, 0.60F, 0.0F};
    recipe.panelColumns = 2U;
    recipe.panelRows = 2U;
    recipe.emissiveAmount = 0.50F;
    recipe.safetyStripe = false;
    recipe.resolution = 1024U;
    switch (condition) {
    case IndustrialCondition::Maintained:
        recipe.wearAmount = 0.09F;
        recipe.dirtAmount = 0.07F;
        break;
    case IndustrialCondition::Used:
        recipe.wearAmount = 0.29F;
        recipe.dirtAmount = 0.24F;
        break;
    case IndustrialCondition::Worn:
        recipe.wearAmount = 0.56F;
        recipe.dirtAmount = 0.48F;
        break;
    case IndustrialCondition::Count:
        recipe.wearAmount = 0.29F;
        recipe.dirtAmount = 0.24F;
        break;
    }
    return recipe;
}

IndustrialMaterialValidationResult validateIndustrialSurfaceRecipe(
    const IndustrialSurfaceRecipe& recipe)
{
    IndustrialMaterialValidationResult result;
    if (recipe.surface >= IndustrialSurface::Count) {
        result.errors.push_back(IndustrialMaterialValidationError::InvalidSurface);
    }
    if (recipe.family != MaterialFamily::PaintedSteel
        && recipe.family != MaterialFamily::BareSteel
        && recipe.family != MaterialFamily::Aluminum) {
        result.errors.push_back(IndustrialMaterialValidationError::InvalidFamily);
    }
    if (!isUnitColor(recipe.baseColor) || !isUnitColor(recipe.secondaryColor)
        || !isUnitColor(recipe.accentColor)) {
        result.errors.push_back(IndustrialMaterialValidationError::InvalidColor);
    }
    if (recipe.panelColumns < 2U || recipe.panelColumns > 4U
        || recipe.panelRows < 2U || recipe.panelRows > 4U) {
        result.errors.push_back(IndustrialMaterialValidationError::InvalidPanelGrid);
    }
    if (!isUnitValue(recipe.wearAmount) || !isUnitValue(recipe.dirtAmount)
        || !isUnitValue(recipe.emissiveAmount)) {
        result.errors.push_back(IndustrialMaterialValidationError::InvalidParameter);
    }
    const bool powerOfTwo = recipe.resolution != 0U
        && (recipe.resolution & (recipe.resolution - 1U)) == 0U;
    const bool hdDetailedSurface = recipe.resolution == 1024U
        && ((recipe.surface == IndustrialSurface::Wall
                && (recipe.version == 2U || recipe.version == 3U))
            || (recipe.surface == IndustrialSurface::Ceiling
                && recipe.version == 2U));
    const bool legacyFloor = recipe.surface == IndustrialSurface::Floor
        && recipe.resolution >= 32U && recipe.resolution <= 256U;
    if (!powerOfTwo || (!hdDetailedSurface && !legacyFloor)) {
        result.errors.push_back(IndustrialMaterialValidationError::InvalidResolution);
    }
    if (recipe.surface < IndustrialSurface::Count) {
        const bool isWall = recipe.surface == IndustrialSurface::Wall;
        const bool isCeiling = recipe.surface == IndustrialSurface::Ceiling;
        if (recipe.safetyStripe != isWall
            || (isCeiling && recipe.emissiveAmount <= 0.0F)
            || (!isCeiling && recipe.emissiveAmount != 0.0F)) {
            result.errors.push_back(IndustrialMaterialValidationError::InvalidSurfaceRule);
        }
    }
    return result;
}

IndustrialMaterialValidationResult validateIndustrialRoomMaterials(
    const IndustrialRoomMaterialSet& materials)
{
    IndustrialMaterialValidationResult result;
    if (materials.condition >= IndustrialCondition::Count) {
        result.errors.push_back(IndustrialMaterialValidationError::InvalidCondition);
    }
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        const IndustrialSurfaceRecipe& recipe = materials.surfaces[index];
        const auto surfaceValidation = validateIndustrialSurfaceRecipe(recipe);
        result.errors.insert(
            result.errors.end(), surfaceValidation.errors.begin(), surfaceValidation.errors.end());
        if (surfaceIndex(recipe.surface) != index) {
            result.errors.push_back(IndustrialMaterialValidationError::InvalidSurfaceOrder);
        }
        const wkc::core::Seed expectedSeed = wkc::core::deriveSeed(
            materials.roomSeed, gdp::game::SeedDomain::RoomMaterial, index + 1U);
        if (recipe.seed != expectedSeed) {
            result.errors.push_back(IndustrialMaterialValidationError::InvalidSurfaceOrder);
        }
    }
    if (materials.materialHash == 0U
        || materials.materialHash != industrialRoomMaterialHash(materials)) {
        result.errors.push_back(IndustrialMaterialValidationError::InvalidMaterialHash);
    }
    return result;
}

std::uint64_t industrialSurfaceRecipeHash(const IndustrialSurfaceRecipe& recipe) noexcept
{
    std::uint64_t hash = wkc::core::hashCombine(0x494E445F53555246ULL, recipe.version);
    hash = wkc::core::hashCombine(hash, recipe.seed);
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.surface));
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.family));
    hash = addVec3(hash, recipe.baseColor);
    hash = addVec3(hash, recipe.secondaryColor);
    hash = addVec3(hash, recipe.accentColor);
    hash = wkc::core::hashCombine(hash, recipe.panelColumns);
    hash = wkc::core::hashCombine(hash, recipe.panelRows);
    hash = addFloat(hash, recipe.wearAmount);
    hash = addFloat(hash, recipe.dirtAmount);
    hash = addFloat(hash, recipe.emissiveAmount);
    hash = wkc::core::hashCombine(hash, recipe.safetyStripe ? 1U : 0U);
    return wkc::core::hashCombine(hash, recipe.resolution);
}

std::uint64_t industrialRoomMaterialHash(
    const IndustrialRoomMaterialSet& materials) noexcept
{
    std::uint64_t hash = wkc::core::hashCombine(
        0x494E445F524F4F4DULL, materials.roomSeed);
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(materials.condition));
    for (const IndustrialSurfaceRecipe& recipe : materials.surfaces) {
        hash = wkc::core::hashCombine(hash, industrialSurfaceRecipeHash(recipe));
    }
    return hash;
}

std::optional<wkc::render::ImageData> generateIndustrialSurfaceAlbedo(
    const IndustrialSurfaceRecipe& recipe)
{
    if (!validateIndustrialSurfaceRecipe(recipe).isValid()) {
        return std::nullopt;
    }

    wkc::render::ImageData image;
    image.width = static_cast<int>(recipe.resolution);
    image.height = static_cast<int>(recipe.resolution);
    image.rgba.resize(static_cast<std::size_t>(recipe.resolution)
        * static_cast<std::size_t>(recipe.resolution) * 4U);
    const std::uint32_t cellWidth = recipe.resolution / recipe.panelColumns;
    const std::uint32_t cellHeight = recipe.resolution / recipe.panelRows;
    const std::uint32_t stripeHeight = std::max(4U, recipe.resolution / 12U);

    for (std::uint32_t y = 0; y < recipe.resolution; ++y) {
        for (std::uint32_t x = 0; x < recipe.resolution; ++x) {
            if (recipe.surface == IndustrialSurface::Wall) {
                writePixel(image, x, y,
                    industrialWallSample(
                        recipe, x, y, cellWidth, cellHeight, stripeHeight).albedo);
                continue;
            }
            if (recipe.surface == IndustrialSurface::Ceiling) {
                writePixel(image, x, y,
                    industrialCeilingSample(
                        recipe, x, y, cellWidth, cellHeight).albedo);
                continue;
            }
            const std::uint64_t pixelId = (static_cast<std::uint64_t>(y) << 32U) | x;
            const std::uint64_t panelId = (static_cast<std::uint64_t>(y / cellHeight) << 32U)
                | (x / cellWidth);
            const float panelBlend = 0.12F + (unitNoise(
                wkc::core::hashCombine(recipe.seed, panelId)) * 0.18F);
            wkc::core::Vec3 color = mix(
                recipe.baseColor, recipe.secondaryColor, panelBlend);
            const float grain = (unitNoise(
                wkc::core::hashCombine(recipe.seed, pixelId)) - 0.5F) * 0.035F;
            color = add(color, grain);

            const std::uint32_t panelX = x % cellWidth;
            const std::uint32_t panelY = y % cellHeight;
            const bool seam = panelX <= 1U || panelY <= 1U;
            const bool rim = panelX == 2U || panelY == 2U;
            if (seam) {
                color = multiply(color, 0.46F);
            } else if (rim) {
                color = add(color, 0.045F);
            }

            const float dirtNoise = unitNoise(wkc::core::hashCombine(
                pixelId, recipe.seed ^ 0xD17D17U));
            if (dirtNoise < recipe.dirtAmount * (seam ? 0.28F : 0.055F)) {
                color = multiply(color, 0.62F);
            }
            const float wearNoise = unitNoise(wkc::core::hashCombine(
                pixelId, recipe.seed ^ 0xAEA412U));
            if (wearNoise < recipe.wearAmount * 0.055F) {
                color = add(color, 0.10F);
            }

            if (recipe.surface == IndustrialSurface::Floor) {
                const float normalizedX = static_cast<float>(x)
                    / static_cast<float>(recipe.resolution - 1U);
                color = multiply(color,
                    normalizedX > 0.30F && normalizedX < 0.70F ? 1.08F : 0.93F);
                if ((y % 8U) == 4U && (x % 8U) >= 2U && (x % 8U) <= 5U) {
                    color = multiply(color, 0.78F);
                }
            }

            writePixel(image, x, y, color);
        }
    }
    return image;
}

std::optional<IndustrialWallMaterialImages> generateIndustrialWallMaterialImages(
    const IndustrialSurfaceRecipe& recipe)
{
    if (!validateIndustrialSurfaceRecipe(recipe).isValid()
        || recipe.surface != IndustrialSurface::Wall) {
        return std::nullopt;
    }

    IndustrialWallMaterialImages images;
    for (wkc::render::ImageData* const image
        : {&images.albedo, &images.normal, &images.properties}) {
        image->width = static_cast<int>(recipe.resolution);
        image->height = static_cast<int>(recipe.resolution);
        image->rgba.resize(static_cast<std::size_t>(recipe.resolution)
            * static_cast<std::size_t>(recipe.resolution) * 4U);
    }
    const std::uint32_t cellWidth = recipe.resolution / recipe.panelColumns;
    const std::uint32_t cellHeight = recipe.resolution / recipe.panelRows;
    const std::uint32_t stripeHeight = std::max(4U, recipe.resolution / 12U);
    std::vector<float> heightMap(static_cast<std::size_t>(recipe.resolution)
        * static_cast<std::size_t>(recipe.resolution));

    for (std::uint32_t y = 0; y < recipe.resolution; ++y) {
        for (std::uint32_t x = 0; x < recipe.resolution; ++x) {
            const IndustrialWallSample sample = industrialWallSample(
                recipe, x, y, cellWidth, cellHeight, stripeHeight);
            writePixel(images.albedo, x, y, sample.albedo);
            writePixel(images.properties, x, y,
                {sample.roughness, sample.metalness, sample.ambientOcclusion});
            heightMap[static_cast<std::size_t>(y) * recipe.resolution + x] = sample.height;
        }
    }
    const auto heightAt = [&](const std::uint32_t sampleX, const std::uint32_t sampleY) {
        return heightMap[static_cast<std::size_t>(sampleY) * recipe.resolution + sampleX];
    };
    for (std::uint32_t y = 0; y < recipe.resolution; ++y) {
        for (std::uint32_t x = 0; x < recipe.resolution; ++x) {
            const std::uint32_t left = x == 0U ? x : x - 1U;
            const std::uint32_t right = std::min(x + 1U, recipe.resolution - 1U);
            const std::uint32_t down = y == 0U ? y : y - 1U;
            const std::uint32_t up = std::min(y + 1U, recipe.resolution - 1U);
            const wkc::core::Vec3 normal = wkc::core::normalized({
                (heightAt(left, y) - heightAt(right, y)) * 2.8F,
                (heightAt(x, down) - heightAt(x, up)) * 2.8F,
                1.0F,
            });
            writePixel(images.normal, x, y, {
                normal.x * 0.5F + 0.5F,
                normal.y * 0.5F + 0.5F,
                normal.z * 0.5F + 0.5F,
            });
        }
    }
    return images;
}

std::optional<IndustrialCeilingMaterialImages> generateIndustrialCeilingMaterialImages(
    const IndustrialSurfaceRecipe& recipe)
{
    if (!validateIndustrialSurfaceRecipe(recipe).isValid()
        || recipe.surface != IndustrialSurface::Ceiling) {
        return std::nullopt;
    }

    IndustrialCeilingMaterialImages images;
    for (wkc::render::ImageData* const image
        : {&images.albedo, &images.normal, &images.properties}) {
        image->width = static_cast<int>(recipe.resolution);
        image->height = static_cast<int>(recipe.resolution);
        image->rgba.resize(static_cast<std::size_t>(recipe.resolution)
            * static_cast<std::size_t>(recipe.resolution) * 4U);
    }
    const std::uint32_t cellWidth = recipe.resolution / recipe.panelColumns;
    const std::uint32_t cellHeight = recipe.resolution / recipe.panelRows;
    std::vector<float> heightMap(static_cast<std::size_t>(recipe.resolution)
        * static_cast<std::size_t>(recipe.resolution));

    for (std::uint32_t y = 0; y < recipe.resolution; ++y) {
        for (std::uint32_t x = 0; x < recipe.resolution; ++x) {
            const IndustrialCeilingSample sample = industrialCeilingSample(
                recipe, x, y, cellWidth, cellHeight);
            writePixel(images.albedo, x, y, sample.albedo);
            writePixel(images.properties, x, y,
                {sample.roughness, sample.metalness, sample.ambientOcclusion});
            heightMap[static_cast<std::size_t>(y) * recipe.resolution + x]
                = sample.height;
        }
    }
    const auto heightAt = [&](const std::uint32_t sampleX, const std::uint32_t sampleY) {
        return heightMap[static_cast<std::size_t>(sampleY) * recipe.resolution + sampleX];
    };
    for (std::uint32_t y = 0; y < recipe.resolution; ++y) {
        for (std::uint32_t x = 0; x < recipe.resolution; ++x) {
            const std::uint32_t left = x == 0U ? x : x - 1U;
            const std::uint32_t right = std::min(x + 1U, recipe.resolution - 1U);
            const std::uint32_t down = y == 0U ? y : y - 1U;
            const std::uint32_t up = std::min(y + 1U, recipe.resolution - 1U);
            const wkc::core::Vec3 normal = wkc::core::normalized({
                (heightAt(left, y) - heightAt(right, y)) * 3.2F,
                (heightAt(x, down) - heightAt(x, up)) * 3.2F,
                1.0F,
            });
            writePixel(images.normal, x, y, {
                normal.x * 0.5F + 0.5F,
                normal.y * 0.5F + 0.5F,
                normal.z * 0.5F + 0.5F,
            });
        }
    }
    return images;
}

StorageRoomMaterialSet generateStorageRoomMaterials(const wkc::core::Seed roomSeed)
{
    StorageRoomMaterialSet materials;
    materials.roomSeed = roomSeed;
    materials.condition = selectStorageCondition(roomSeed);
    materials.surfaces = {
        makeStorageSurfaceRecipe(roomSeed, materials.condition, RoomSurface::Wall),
        makeStorageSurfaceRecipe(roomSeed, materials.condition, RoomSurface::Floor),
        makeStorageSurfaceRecipe(roomSeed, materials.condition, RoomSurface::Ceiling),
    };
    materials.materialHash = storageRoomMaterialHash(materials);
    return materials;
}

StorageMaterialValidationResult validateStorageSurfaceRecipe(
    const StorageSurfaceRecipe& recipe)
{
    StorageMaterialValidationResult result;
    if (recipe.surface >= RoomSurface::Count) {
        result.errors.push_back(StorageMaterialValidationError::InvalidSurface);
    }
    if (recipe.family != MaterialFamily::PaintedSteel
        && recipe.family != MaterialFamily::BareSteel
        && recipe.family != MaterialFamily::Ceramic) {
        result.errors.push_back(StorageMaterialValidationError::InvalidFamily);
    }
    if (!isUnitColor(recipe.baseColor) || !isUnitColor(recipe.secondaryColor)
        || !isUnitColor(recipe.accentColor)) {
        result.errors.push_back(StorageMaterialValidationError::InvalidColor);
    }
    if (recipe.panelColumns < 2U || recipe.panelColumns > 3U
        || recipe.panelRows < 2U || recipe.panelRows > 3U) {
        result.errors.push_back(StorageMaterialValidationError::InvalidPanelGrid);
    }
    if (!isUnitValue(recipe.wearAmount) || !isUnitValue(recipe.dirtAmount)
        || !isUnitValue(recipe.oxidationAmount) || !isUnitValue(recipe.detailAmount)
        || !isUnitValue(recipe.emissiveAmount)) {
        result.errors.push_back(StorageMaterialValidationError::InvalidParameter);
    }
    const bool powerOfTwo = recipe.resolution != 0U
        && (recipe.resolution & (recipe.resolution - 1U)) == 0U;
    if (!powerOfTwo || recipe.resolution < 32U || recipe.resolution > 256U) {
        result.errors.push_back(StorageMaterialValidationError::InvalidResolution);
    }
    if (recipe.surface < RoomSurface::Count) {
        const bool isWall = recipe.surface == RoomSurface::Wall;
        const bool isFloor = recipe.surface == RoomSurface::Floor;
        const bool isCeiling = recipe.surface == RoomSurface::Ceiling;
        if ((isWall && (recipe.family != MaterialFamily::PaintedSteel
                           || recipe.oxidationAmount <= 0.0F))
            || (isFloor && (recipe.family != MaterialFamily::Ceramic
                               || recipe.oxidationAmount != 0.0F))
            || (isCeiling && (recipe.family != MaterialFamily::BareSteel
                                 || recipe.emissiveAmount <= 0.0F))
            || (!isCeiling && recipe.emissiveAmount != 0.0F)) {
            result.errors.push_back(StorageMaterialValidationError::InvalidSurfaceRule);
        }
    }
    return result;
}

StorageMaterialValidationResult validateStorageRoomMaterials(
    const StorageRoomMaterialSet& materials)
{
    StorageMaterialValidationResult result;
    if (materials.condition >= StorageCondition::Count) {
        result.errors.push_back(StorageMaterialValidationError::InvalidCondition);
    }
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        const StorageSurfaceRecipe& recipe = materials.surfaces[index];
        const auto surfaceValidation = validateStorageSurfaceRecipe(recipe);
        result.errors.insert(
            result.errors.end(), surfaceValidation.errors.begin(), surfaceValidation.errors.end());
        if (surfaceIndex(recipe.surface) != index) {
            result.errors.push_back(StorageMaterialValidationError::InvalidSurfaceOrder);
        }
        const wkc::core::Seed expectedSeed = wkc::core::deriveSeed(
            materials.roomSeed, gdp::game::SeedDomain::RoomMaterial, index + 101U);
        if (recipe.seed != expectedSeed) {
            result.errors.push_back(StorageMaterialValidationError::InvalidSurfaceOrder);
        }
    }
    if (materials.materialHash == 0U
        || materials.materialHash != storageRoomMaterialHash(materials)) {
        result.errors.push_back(StorageMaterialValidationError::InvalidMaterialHash);
    }
    return result;
}

std::uint64_t storageSurfaceRecipeHash(const StorageSurfaceRecipe& recipe) noexcept
{
    std::uint64_t hash = wkc::core::hashCombine(0x53544F5F53555246ULL, recipe.version);
    hash = wkc::core::hashCombine(hash, recipe.seed);
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.surface));
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.family));
    hash = addVec3(hash, recipe.baseColor);
    hash = addVec3(hash, recipe.secondaryColor);
    hash = addVec3(hash, recipe.accentColor);
    hash = wkc::core::hashCombine(hash, recipe.panelColumns);
    hash = wkc::core::hashCombine(hash, recipe.panelRows);
    hash = addFloat(hash, recipe.wearAmount);
    hash = addFloat(hash, recipe.dirtAmount);
    hash = addFloat(hash, recipe.oxidationAmount);
    hash = addFloat(hash, recipe.detailAmount);
    hash = addFloat(hash, recipe.emissiveAmount);
    return wkc::core::hashCombine(hash, recipe.resolution);
}

std::uint64_t storageRoomMaterialHash(const StorageRoomMaterialSet& materials) noexcept
{
    std::uint64_t hash = wkc::core::hashCombine(
        0x53544F5F524F4F4DULL, materials.roomSeed);
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(materials.condition));
    for (const StorageSurfaceRecipe& recipe : materials.surfaces) {
        hash = wkc::core::hashCombine(hash, storageSurfaceRecipeHash(recipe));
    }
    return hash;
}

std::optional<wkc::render::ImageData> generateStorageSurfaceAlbedo(
    const StorageSurfaceRecipe& recipe)
{
    if (!validateStorageSurfaceRecipe(recipe).isValid()) {
        return std::nullopt;
    }

    wkc::render::ImageData image;
    image.width = static_cast<int>(recipe.resolution);
    image.height = static_cast<int>(recipe.resolution);
    image.rgba.resize(static_cast<std::size_t>(recipe.resolution)
        * static_cast<std::size_t>(recipe.resolution) * 4U);
    const std::uint32_t cellWidth = recipe.resolution / recipe.panelColumns;
    const std::uint32_t cellHeight = recipe.resolution / recipe.panelRows;

    for (std::uint32_t y = 0; y < recipe.resolution; ++y) {
        for (std::uint32_t x = 0; x < recipe.resolution; ++x) {
            const std::uint64_t pixelId = (static_cast<std::uint64_t>(y) << 32U) | x;
            const std::uint64_t panelId = (static_cast<std::uint64_t>(y / cellHeight) << 32U)
                | (x / cellWidth);
            const float panelVariation = 0.10F + (unitNoise(
                wkc::core::hashCombine(recipe.seed, panelId)) * 0.30F);
            wkc::core::Vec3 color = mix(
                recipe.baseColor, recipe.secondaryColor, panelVariation);
            const std::uint32_t panelX = x % cellWidth;
            const std::uint32_t panelY = y % cellHeight;
            const bool seam = panelX <= 1U || panelY <= 1U;
            if (seam) {
                color = multiply(color, 0.52F);
            }

            const std::uint64_t coarseId = (static_cast<std::uint64_t>(y / 8U) << 32U)
                | (x / 8U);
            const float coarseDirt = unitNoise(wkc::core::hashCombine(
                recipe.seed ^ 0x5702A6EULL, coarseId));
            if (coarseDirt < recipe.dirtAmount * 0.34F) {
                color = multiply(color, 0.74F);
            }
            const float wearNoise = unitNoise(wkc::core::hashCombine(
                recipe.seed ^ 0x5C0FF5ULL, pixelId));
            if (wearNoise < recipe.wearAmount * 0.035F) {
                color = add(color, 0.075F);
            }

            if (recipe.surface == RoomSurface::Wall) {
                const float rustNoise = unitNoise(wkc::core::hashCombine(
                    recipe.seed ^ 0x2A057ULL, pixelId));
                if ((seam || y < recipe.resolution / 6U)
                    && rustNoise < recipe.oxidationAmount * 0.22F) {
                    color = mix(color, recipe.accentColor, 0.72F);
                }
                const std::uint32_t markY = recipe.resolution / 3U;
                const std::uint32_t markStart = recipe.resolution / 8U;
                const std::uint32_t markEnd = markStart + (recipe.resolution / 4U);
                if (y >= markY && y <= markY + 1U && x >= markStart && x < markEnd) {
                    color = multiply(recipe.accentColor, 1.12F);
                }
                if (y < recipe.resolution / 4U && (y % 7U) == 3U
                    && x > recipe.resolution / 3U && x < recipe.resolution * 2U / 3U) {
                    color = multiply(color, 0.68F);
                }
            } else if (recipe.surface == RoomSurface::Floor) {
                const std::uint32_t dragY = recipe.resolution / 3U;
                if ((y == dragY || y == dragY + 4U)
                    && x > recipe.resolution / 5U && x < recipe.resolution * 4U / 5U) {
                    color = multiply(color, 0.70F);
                }
                const std::uint32_t crackOffset = static_cast<std::uint32_t>(recipe.seed & 7U);
                const bool crack = recipe.detailAmount > 0.16F
                    && x > recipe.resolution / 2U && x < recipe.resolution * 4U / 5U
                    && y == (x / 3U) + crackOffset;
                if (crack) {
                    color = multiply(color, 0.58F);
                }
            } else {
                const bool beam = panelX <= 2U;
                if (beam) {
                    color = multiply(color, 0.58F);
                }
            }
            writePixel(image, x, y, color);
        }
    }
    return image;
}

CombatRoomMaterialSet generateCombatRoomMaterials(
    const wkc::core::Seed roomSeed, const bool galleryWallV1,
    const bool bossArenaV1)
{
    CombatRoomMaterialSet materials;
    materials.roomSeed = roomSeed;
    materials.condition = selectCombatCondition(roomSeed);
    materials.surfaces = {
        makeCombatSurfaceRecipe(roomSeed, materials.condition, RoomSurface::Wall),
        makeCombatSurfaceRecipe(roomSeed, materials.condition, RoomSurface::Floor),
        makeCombatSurfaceRecipe(roomSeed, materials.condition, RoomSurface::Ceiling),
    };
    if (galleryWallV1 || bossArenaV1) {
        // Both arenas use physical-scale plates. Their palettes remain separate.
        for (CombatSurfaceRecipe& recipe : materials.surfaces) {
            recipe.version = 2U;
            recipe.resolution = 256U;
            recipe.panelColumns = 2U;
            recipe.panelRows = 2U;
        }
        CombatSurfaceRecipe& wall = materials.surfaces[surfaceIndex(RoomSurface::Wall)];
        wall.baseColor = {0.112F, 0.123F, 0.105F};
        wall.secondaryColor = {0.091F, 0.102F, 0.088F};
        wall.accentColor = {0.23F, 0.24F, 0.20F};
        CombatSurfaceRecipe& floor = materials.surfaces[surfaceIndex(RoomSurface::Floor)];
        // One 2 m plate per tile. Version three invalidates cached 1 m grids.
        floor.version = 3U;
        floor.panelColumns = 1U;
        floor.panelRows = 1U;
        floor.baseColor = {0.079F, 0.083F, 0.087F};
        floor.secondaryColor = {0.062F, 0.066F, 0.070F};
        floor.accentColor = {0.19F, 0.17F, 0.12F};
        CombatSurfaceRecipe& ceiling = materials.surfaces[surfaceIndex(RoomSurface::Ceiling)];
        ceiling.version = 3U;
        ceiling.baseColor = {0.110F, 0.115F, 0.115F};
        ceiling.secondaryColor = {0.075F, 0.080F, 0.085F};
        ceiling.accentColor = {0.19F, 0.19F, 0.17F};
        if (bossArenaV1) {
            // Version four uses dedicated albedo, normal and property maps.
            // It also keeps the Boss cache independent from the gallery.
            wall.version = 4U;
            floor.version = 4U;
            ceiling.version = 4U;
            wall.baseColor = {0.265F, 0.255F, 0.242F};
            wall.secondaryColor = {0.205F, 0.199F, 0.191F};
            wall.accentColor = {0.45F, 0.31F, 0.18F};
            floor.baseColor = {0.125F, 0.127F, 0.126F};
            floor.secondaryColor = {0.086F, 0.089F, 0.088F};
            floor.accentColor = {0.38F, 0.20F, 0.075F};
            ceiling.baseColor = {0.105F, 0.107F, 0.107F};
            ceiling.secondaryColor = {0.070F, 0.073F, 0.075F};
            ceiling.accentColor = {0.28F, 0.20F, 0.12F};
        }
    }
    materials.materialHash = combatRoomMaterialHash(materials);
    return materials;
}

CombatMaterialValidationResult validateCombatSurfaceRecipe(
    const CombatSurfaceRecipe& recipe)
{
    CombatMaterialValidationResult result;
    if (recipe.version != 1U && recipe.version != 2U
        && recipe.version != 3U && recipe.version != 4U) {
        result.errors.push_back(CombatMaterialValidationError::InvalidSurfaceRule);
    }
    if (recipe.surface >= RoomSurface::Count) {
        result.errors.push_back(CombatMaterialValidationError::InvalidSurface);
    }
    if (recipe.family != MaterialFamily::PaintedSteel
        && recipe.family != MaterialFamily::BareSteel
        && recipe.family != MaterialFamily::Ceramic) {
        result.errors.push_back(CombatMaterialValidationError::InvalidFamily);
    }
    if (!isUnitColor(recipe.baseColor) || !isUnitColor(recipe.secondaryColor)
        || !isUnitColor(recipe.accentColor)) {
        result.errors.push_back(CombatMaterialValidationError::InvalidColor);
    }
    if (recipe.panelColumns < 1U || recipe.panelColumns > 4U
        || recipe.panelRows < 1U || recipe.panelRows > 3U) {
        result.errors.push_back(CombatMaterialValidationError::InvalidPanelGrid);
    }
    if (!isUnitValue(recipe.wearAmount) || !isUnitValue(recipe.impactAmount)
        || !isUnitValue(recipe.reinforcementAmount) || !isUnitValue(recipe.markingAmount)
        || !isUnitValue(recipe.emissiveAmount)) {
        result.errors.push_back(CombatMaterialValidationError::InvalidParameter);
    }
    const bool powerOfTwo = recipe.resolution != 0U
        && (recipe.resolution & (recipe.resolution - 1U)) == 0U;
    if (!powerOfTwo || recipe.resolution < 32U || recipe.resolution > 256U) {
        result.errors.push_back(CombatMaterialValidationError::InvalidResolution);
    }
    if (recipe.surface < RoomSurface::Count) {
        const bool isWall = recipe.surface == RoomSurface::Wall;
        const bool isFloor = recipe.surface == RoomSurface::Floor;
        const bool isCeiling = recipe.surface == RoomSurface::Ceiling;
        if (recipe.version == 3U
            && !(isFloor || isCeiling)) {
            result.errors.push_back(CombatMaterialValidationError::InvalidSurfaceRule);
        }
        if (recipe.version == 4U && (recipe.resolution != 256U
                || (isFloor && (recipe.panelColumns != 1U || recipe.panelRows != 1U))
                || (!isFloor && (recipe.panelColumns != 2U || recipe.panelRows != 2U)))) {
            result.errors.push_back(CombatMaterialValidationError::InvalidSurfaceRule);
        }
        if (recipe.version != 3U && recipe.version != 4U
            && recipe.panelRows < 2U) {
            result.errors.push_back(CombatMaterialValidationError::InvalidPanelGrid);
        }
        if ((isWall && (recipe.family != MaterialFamily::PaintedSteel
                           || recipe.impactAmount <= 0.0F
                           || recipe.reinforcementAmount <= 0.0F
                           || recipe.markingAmount <= 0.0F))
            || (isFloor && (recipe.family != MaterialFamily::Ceramic
                               || recipe.impactAmount <= 0.0F
                               || recipe.markingAmount <= 0.0F
                               || recipe.reinforcementAmount != 0.0F))
            || (isCeiling && (recipe.family != MaterialFamily::BareSteel
                                 || recipe.impactAmount != 0.0F
                                 || recipe.reinforcementAmount <= 0.0F
                                 || recipe.markingAmount != 0.0F
                                 || recipe.emissiveAmount <= 0.0F))
            || (!isCeiling && recipe.emissiveAmount != 0.0F)) {
            result.errors.push_back(CombatMaterialValidationError::InvalidSurfaceRule);
        }
    }
    return result;
}

CombatMaterialValidationResult validateCombatRoomMaterials(
    const CombatRoomMaterialSet& materials)
{
    CombatMaterialValidationResult result;
    if (materials.condition >= CombatCondition::Count) {
        result.errors.push_back(CombatMaterialValidationError::InvalidCondition);
    }
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        const CombatSurfaceRecipe& recipe = materials.surfaces[index];
        const auto surfaceValidation = validateCombatSurfaceRecipe(recipe);
        result.errors.insert(
            result.errors.end(), surfaceValidation.errors.begin(), surfaceValidation.errors.end());
        if (surfaceIndex(recipe.surface) != index) {
            result.errors.push_back(CombatMaterialValidationError::InvalidSurfaceOrder);
        }
        const wkc::core::Seed expectedSeed = wkc::core::deriveSeed(
            materials.roomSeed, gdp::game::SeedDomain::RoomMaterial, index + 201U);
        if (recipe.seed != expectedSeed) {
            result.errors.push_back(CombatMaterialValidationError::InvalidSurfaceOrder);
        }
    }
    if (materials.materialHash == 0U
        || materials.materialHash != combatRoomMaterialHash(materials)) {
        result.errors.push_back(CombatMaterialValidationError::InvalidMaterialHash);
    }
    return result;
}

std::uint64_t combatSurfaceRecipeHash(const CombatSurfaceRecipe& recipe) noexcept
{
    std::uint64_t hash = wkc::core::hashCombine(0x434D425F53555246ULL, recipe.version);
    hash = wkc::core::hashCombine(hash, recipe.seed);
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.surface));
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.family));
    hash = addVec3(hash, recipe.baseColor);
    hash = addVec3(hash, recipe.secondaryColor);
    hash = addVec3(hash, recipe.accentColor);
    hash = wkc::core::hashCombine(hash, recipe.panelColumns);
    hash = wkc::core::hashCombine(hash, recipe.panelRows);
    hash = addFloat(hash, recipe.wearAmount);
    hash = addFloat(hash, recipe.impactAmount);
    hash = addFloat(hash, recipe.reinforcementAmount);
    hash = addFloat(hash, recipe.markingAmount);
    hash = addFloat(hash, recipe.emissiveAmount);
    return wkc::core::hashCombine(hash, recipe.resolution);
}

std::uint64_t combatRoomMaterialHash(const CombatRoomMaterialSet& materials) noexcept
{
    std::uint64_t hash = wkc::core::hashCombine(
        0x434D425F524F4F4DULL, materials.roomSeed);
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(materials.condition));
    for (const CombatSurfaceRecipe& recipe : materials.surfaces) {
        hash = wkc::core::hashCombine(hash, combatSurfaceRecipeHash(recipe));
    }
    return hash;
}

std::uint64_t combatCoverMaterialHash(const wkc::core::Seed roomSeed) noexcept
{
    // Versioned independently from the Arena wall/floor/ceiling recipes.
    return wkc::core::hashCombine(
        wkc::core::hashCombine(0x434F5645525F5631ULL, roomSeed), 256U);
}

std::optional<wkc::render::ImageData> generateCombatCoverAlbedo(
    const wkc::core::Seed roomSeed)
{
    constexpr std::uint32_t resolution = 256U;
    constexpr wkc::core::Vec3 paintedSteel{0.174F, 0.190F, 0.157F};
    constexpr wkc::core::Vec3 wornSteel{0.30F, 0.31F, 0.28F};
    const wkc::core::Seed materialSeed = combatCoverMaterialHash(roomSeed);
    wkc::render::ImageData image;
    image.width = static_cast<int>(resolution);
    image.height = static_cast<int>(resolution);
    image.rgba.resize(static_cast<std::size_t>(resolution) * resolution * 4U);
    for (std::uint32_t y = 0U; y < resolution; ++y) {
        for (std::uint32_t x = 0U; x < resolution; ++x) {
            const std::uint64_t pixelId = (static_cast<std::uint64_t>(y) << 32U) | x;
            const float patina = coverTileNoise(materialSeed ^ 0x504154494E41ULL,
                x, y, 64U);
            const float brushed = coverTileNoise(materialSeed ^ 0x42525553484544ULL,
                x, y, 8U);
            const float grain = unitNoise(wkc::core::hashCombine(
                materialSeed ^ 0x4D4943524F4752ULL, pixelId));
            // Broad olive paint variation and fine horizontal brushing read as
            // metal on a 1-2 m object, without copying the 6 m wall plates.
            const float tone = 0.88F + 0.18F * patina
                + 0.060F * (brushed - 0.5F)
                + 0.035F * (grain - 0.5F);
            wkc::core::Vec3 color = multiply(paintedSteel, tone);
            for (std::uint32_t mark = 0U; mark < 2U; ++mark) {
                const auto markSeed = wkc::core::hashCombine(materialSeed, mark);
                const std::uint32_t scratchY = 32U
                    + static_cast<std::uint32_t>(markSeed % 192U);
                const std::uint32_t scratchX = 24U
                    + static_cast<std::uint32_t>((markSeed >> 12U) % 145U);
                const std::uint32_t scratchLength = 23U
                    + static_cast<std::uint32_t>((markSeed >> 28U) % 40U);
                if (y == scratchY && x >= scratchX
                    && x < scratchX + scratchLength) {
                    const float wear = 0.12F + 0.13F * brushed;
                    color = mix(color, wornSteel, wear);
                }
            }
            writePixel(image, x, y, color);
        }
    }
    return image;
}

std::uint64_t combatColumnMaterialHash(const wkc::core::Seed roomSeed) noexcept
{
    // Keep the column finish independent of both the wall and cover recipes.
    return wkc::core::hashCombine(
        wkc::core::hashCombine(0x434F4C554D4E5632ULL, roomSeed), 256U);
}

std::optional<wkc::render::ImageData> generateCombatColumnAlbedo(
    const wkc::core::Seed roomSeed)
{
    constexpr std::uint32_t resolution = 256U;
    constexpr wkc::core::Vec3 paintedSteel{0.68F, 0.71F, 0.72F};
    constexpr wkc::core::Vec3 exposedSteel{0.82F, 0.84F, 0.84F};
    constexpr wkc::core::Vec3 recessedSteel{0.23F, 0.25F, 0.26F};
    const wkc::core::Seed materialSeed = combatColumnMaterialHash(roomSeed);
    wkc::render::ImageData image;
    image.width = static_cast<int>(resolution);
    image.height = static_cast<int>(resolution);
    image.rgba.resize(static_cast<std::size_t>(resolution) * resolution * 4U);

    for (std::uint32_t y = 0U; y < resolution; ++y) {
        for (std::uint32_t x = 0U; x < resolution; ++x) {
            const std::uint64_t pixelId = (static_cast<std::uint64_t>(y) << 32U) | x;
            const float patina = coverTileNoise(materialSeed ^ 0x504154494E41ULL,
                x, y, 64U);
            const float fineGrain = coverTileNoise(materialSeed ^ 0x475241494EULL,
                x, y, 8U);
            // Fine column-direction brushing reads as metal without turning
            // each narrow face into a repeated wall-sized panel grid.
            const float brushed = unitNoise(wkc::core::hashCombine(
                materialSeed ^ 0x4252555348ULL, x / 2U));
            const float speckle = unitNoise(wkc::core::hashCombine(
                materialSeed ^ 0x535045434BULL, pixelId));
            const float tone = 0.88F + 0.19F * patina
                + 0.070F * (brushed - 0.5F)
                + 0.045F * (fineGrain - 0.5F)
                + 0.028F * (speckle - 0.5F);
            wkc::core::Vec3 color = multiply(paintedSteel, tone);

            // The real 3D braces define the column sections. A single broad
            // plate in this tile avoids several dark joints across one face.
            const bool plateJoint = x <= 2U || x >= 253U
                || y <= 2U || y >= 253U;
            if (plateJoint) {
                color = mix(color, recessedSteel, 0.66F);
            } else if (x == 4U || x == 251U
                || y == 4U || y == 251U) {
                color = mix(color, exposedSteel, 0.38F);
            }

            // Four small bolt heads sit inside the plate corners, with a
            // dark socket and a restrained highlight on the raised cap.
            for (const std::uint32_t boltX : {18U, 238U}) {
                for (const std::uint32_t boltY : {19U, 237U}) {
                    const int dx = static_cast<int>(x) - static_cast<int>(boltX);
                    const int dy = static_cast<int>(y) - static_cast<int>(boltY);
                    const int radiusSquared = dx * dx + dy * dy;
                    if (radiusSquared <= 25) {
                        color = mix(color, recessedSteel,
                            radiusSquared >= 16 ? 0.84F : 0.32F);
                        if (radiusSquared <= 9 && dy <= 0) {
                            color = mix(color, exposedSteel, 0.44F);
                        }
                    }
                }
            }

            // Sparse vertical scuffs expose the metal without making every
            // tile look like a repeated high-contrast wall decal.
            for (std::uint32_t mark = 0U; mark < 10U; ++mark) {
                const auto markSeed = wkc::core::hashCombine(materialSeed, mark);
                const std::uint32_t scratchX = 10U
                    + static_cast<std::uint32_t>(markSeed % 236U);
                const std::uint32_t scratchY = 10U
                    + static_cast<std::uint32_t>((markSeed >> 12U) % 188U);
                const std::uint32_t scratchLength = 12U
                    + static_cast<std::uint32_t>((markSeed >> 28U) % 38U);
                if (x == scratchX && y >= scratchY
                    && y < scratchY + scratchLength) {
                    color = mix(color, exposedSteel, 0.26F + 0.18F * patina);
                }
            }
            writePixel(image, x, y, color);
        }
    }
    return image;
}

std::uint64_t combatRailMaterialHash(const wkc::core::Seed roomSeed) noexcept
{
    return wkc::core::hashCombine(
        wkc::core::hashCombine(0x434F4D425241494CULL, roomSeed), 128U);
}

std::optional<wkc::render::ImageData> generateCombatRailAlbedo(
    const wkc::core::Seed roomSeed)
{
    constexpr std::uint32_t resolution = 128U;
    constexpr wkc::core::Vec3 paint{0.84F, 0.79F, 0.72F};
    constexpr wkc::core::Vec3 exposedSteel{0.69F, 0.72F, 0.72F};
    const wkc::core::Seed materialSeed = combatRailMaterialHash(roomSeed);
    wkc::render::ImageData image;
    image.width = static_cast<int>(resolution);
    image.height = static_cast<int>(resolution);
    image.rgba.resize(static_cast<std::size_t>(resolution) * resolution * 4U);

    for (std::uint32_t y = 0U; y < resolution; ++y) {
        for (std::uint32_t x = 0U; x < resolution; ++x) {
            const std::uint64_t pixelId = (static_cast<std::uint64_t>(y) << 32U) | x;
            const float patina = coverTileNoise(materialSeed ^ 0x504154494E41ULL,
                x, y, 32U);
            const float brush = unitNoise(wkc::core::hashCombine(
                materialSeed ^ 0x4252555348ULL, y / 2U));
            const float grain = unitNoise(wkc::core::hashCombine(
                materialSeed ^ 0x475241494EULL, pixelId));
            wkc::core::Vec3 color = multiply(paint,
                0.89F + 0.15F * patina + 0.09F * (brush - 0.5F)
                    + 0.04F * (grain - 0.5F));
            // Sparse exposed-metal scratches keep the painted railing from
            // reading as a flat emissive orange strip at close range.
            for (std::uint32_t mark = 0U; mark < 12U; ++mark) {
                const auto markSeed = wkc::core::hashCombine(materialSeed, mark);
                const std::uint32_t scratchX = 4U
                    + static_cast<std::uint32_t>(markSeed % 116U);
                const std::uint32_t scratchY = 3U
                    + static_cast<std::uint32_t>((markSeed >> 12U) % 120U);
                const std::uint32_t length = 5U
                    + static_cast<std::uint32_t>((markSeed >> 28U) % 12U);
                if (y == scratchY && x >= scratchX && x < scratchX + length) {
                    color = mix(color, exposedSteel, 0.42F);
                }
            }
            writePixel(image, x, y, color);
        }
    }
    return image;
}

std::optional<CombatSurfaceMaterialImages> generateBossArenaSurfaceMaterialImages(
    const CombatSurfaceRecipe& recipe)
{
    if (recipe.version != 4U || !validateCombatSurfaceRecipe(recipe).isValid()) {
        return std::nullopt;
    }

    CombatSurfaceMaterialImages images;
    for (wkc::render::ImageData* const image
        : {&images.albedo, &images.normal, &images.properties}) {
        image->width = static_cast<int>(recipe.resolution);
        image->height = static_cast<int>(recipe.resolution);
        image->rgba.resize(static_cast<std::size_t>(recipe.resolution)
            * static_cast<std::size_t>(recipe.resolution) * 4U);
    }
    std::vector<float> heights(static_cast<std::size_t>(recipe.resolution)
        * static_cast<std::size_t>(recipe.resolution));
    for (std::uint32_t y = 0U; y < recipe.resolution; ++y) {
        for (std::uint32_t x = 0U; x < recipe.resolution; ++x) {
            const BossArenaSurfaceSample sample = bossArenaSurfaceSample(recipe, x, y);
            writePixel(images.albedo, x, y, sample.albedo);
            writePixel(images.properties, x, y,
                {sample.roughness, sample.metalness, sample.ambientOcclusion});
            heights[static_cast<std::size_t>(y) * recipe.resolution + x]
                = sample.height;
        }
    }
    const auto heightAt = [&](const std::uint32_t x, const std::uint32_t y) {
        return heights[static_cast<std::size_t>(y) * recipe.resolution + x];
    };
    for (std::uint32_t y = 0U; y < recipe.resolution; ++y) {
        for (std::uint32_t x = 0U; x < recipe.resolution; ++x) {
            const std::uint32_t left = x == 0U ? x : x - 1U;
            const std::uint32_t right = std::min(x + 1U, recipe.resolution - 1U);
            const std::uint32_t down = y == 0U ? y : y - 1U;
            const std::uint32_t up = std::min(y + 1U, recipe.resolution - 1U);
            const wkc::core::Vec3 normal = wkc::core::normalized({
                (heightAt(left, y) - heightAt(right, y)) * 2.6F,
                (heightAt(x, down) - heightAt(x, up)) * 2.6F,
                1.0F,
            });
            writePixel(images.normal, x, y, {
                normal.x * 0.5F + 0.5F,
                normal.y * 0.5F + 0.5F,
                normal.z * 0.5F + 0.5F,
            });
        }
    }
    return images;
}

std::optional<wkc::render::ImageData> generateCombatSurfaceAlbedo(
    const CombatSurfaceRecipe& recipe)
{
    if (!validateCombatSurfaceRecipe(recipe).isValid()) {
        return std::nullopt;
    }

    if (recipe.version == 4U) {
        const auto images = generateBossArenaSurfaceMaterialImages(recipe);
        if (!images) {
            return std::nullopt;
        }
        return images->albedo;
    }

    wkc::render::ImageData image;
    image.width = static_cast<int>(recipe.resolution);
    image.height = static_cast<int>(recipe.resolution);
    image.rgba.resize(static_cast<std::size_t>(recipe.resolution)
        * static_cast<std::size_t>(recipe.resolution) * 4U);
    const std::uint32_t cellWidth = recipe.resolution / recipe.panelColumns;
    const std::uint32_t cellHeight = recipe.resolution / recipe.panelRows;
    const std::uint32_t impactCount = 1U
        + (recipe.impactAmount > 0.18F ? 1U : 0U)
        + (recipe.impactAmount > 0.34F ? 1U : 0U);

    if (recipe.version == 3U) {
        // The floor is one 2 m graphite plate. The ceiling remains four
        // dark panels per tile; fixtures, grilles and ducts are separate 3D
        // objects. Both materials keep their boundary in a recessed joint.
        const bool floor = recipe.surface == RoomSurface::Floor;
        const wkc::core::Vec3 joint = floor
            ? wkc::core::Vec3{0.043F, 0.047F, 0.050F}
            : wkc::core::Vec3{0.038F, 0.042F, 0.046F};
        const wkc::core::Vec3 edgeSteel = floor
            ? wkc::core::Vec3{0.155F, 0.160F, 0.160F}
            : wkc::core::Vec3{0.132F, 0.137F, 0.140F};
        for (std::uint32_t y = 0U; y < recipe.resolution; ++y) {
            for (std::uint32_t x = 0U; x < recipe.resolution; ++x) {
                const std::uint32_t panelX = x % cellWidth;
                const std::uint32_t panelY = y % cellHeight;
                const std::uint32_t edge = std::min({panelX,
                    cellWidth - 1U - panelX, panelY, cellHeight - 1U - panelY});
                const std::uint64_t pixelId = (static_cast<std::uint64_t>(y) << 32U) | x;
                const std::uint64_t panelId =
                    (static_cast<std::uint64_t>(y / cellHeight) << 32U)
                    | (x / cellWidth);
                const float plateTone = 0.19F + 0.17F * unitNoise(
                    wkc::core::hashCombine(recipe.seed ^ 0x434F4D42504C4154ULL, panelId));
                wkc::core::Vec3 color = mix(
                    recipe.baseColor, recipe.secondaryColor, plateTone);
                if (edge <= 2U) {
                    color = joint;
                } else {
                    const float patina = coverTileNoise(
                        recipe.seed ^ 0x434F4D4250415449ULL, x, y, 64U);
                    const float brush = coverTileNoise(
                        recipe.seed ^ 0x434F4D4242525553ULL, x, y, 8U);
                    const float grain = unitNoise(wkc::core::hashCombine(
                        recipe.seed ^ 0x434F4D4247524149ULL, pixelId));
                    color = multiply(color, 0.92F + 0.10F * patina
                        + 0.035F * (brush - 0.5F)
                        + 0.025F * (grain - 0.5F));
                    if (edge <= (floor ? 10U : 6U)) {
                        // A thin, asymmetric bevel reads as sheet-metal
                        // thickness without a repeated painted light strip.
                        color = mix(color, edgeSteel,
                            (panelX < cellWidth / 2U || panelY < cellHeight / 2U)
                                ? 0.12F : 0.035F);
                        const float chip = unitNoise(wkc::core::hashCombine(
                            recipe.seed ^ 0x434F4D4245444745ULL, pixelId));
                        if (chip < recipe.wearAmount * (floor ? 0.13F : 0.055F)) {
                            color = mix(color, edgeSteel, floor ? 0.35F : 0.17F);
                        }
                    } else if (floor) {
                        // Scuffs are short and low contrast so a 2 m repeat
                        // does not turn into a visible stamp across the room.
                        const float scuff = unitNoise(wkc::core::hashCombine(
                            recipe.seed ^ 0x434F4D4253435546ULL, pixelId));
                        if (scuff < recipe.wearAmount * 0.028F) {
                            color = mix(color, edgeSteel, 0.18F);
                        }
                    }
                    const int boltInset = floor ? 17 : 12;
                    const int nearX = std::min(
                        std::abs(static_cast<int>(panelX) - boltInset),
                        std::abs(static_cast<int>(panelX)
                            - static_cast<int>(cellWidth) + boltInset + 1));
                    const int nearY = std::min(
                        std::abs(static_cast<int>(panelY) - boltInset),
                        std::abs(static_cast<int>(panelY)
                            - static_cast<int>(cellHeight) + boltInset + 1));
                    const int radiusSquared = nearX * nearX + nearY * nearY;
                    const int outerRadiusSquared = floor ? 36 : 16;
                    if (radiusSquared <= outerRadiusSquared) {
                        color = radiusSquared <= (floor ? 9 : 4)
                            ? mix(joint, edgeSteel, 0.36F) : joint;
                    }
                }
                writePixel(image, x, y, color);
            }
        }
        return image;
    }

    if (recipe.version == 2U) {
        // Wall tiles cover six metres in the arena renderer; floor and
        // ceiling tiles cover two. Fine seams are a material detail; lamps,
        // beams, railings and route markings are geometry.
        // This avoids multiplying painted lights and warning boxes per tile.
        const bool wall = recipe.surface == RoomSurface::Wall;
        const bool floor = recipe.surface == RoomSurface::Floor;
        const std::uint32_t jointWidth = wall ? 2U : 1U;
        const std::uint32_t bevelWidth = wall ? 6U : 4U;
        const wkc::core::Vec3 joint = wall
            ? wkc::core::Vec3{0.067F, 0.069F, 0.059F}
            : floor ? wkc::core::Vec3{0.055F, 0.058F, 0.059F}
                    : wkc::core::Vec3{0.043F, 0.047F, 0.051F};
        const wkc::core::Vec3 exposedSteel = wall
            ? wkc::core::Vec3{0.40F, 0.39F, 0.32F}
            : wkc::core::Vec3{0.28F, 0.28F, 0.27F};
        for (std::uint32_t y = 0; y < recipe.resolution; ++y) {
            for (std::uint32_t x = 0; x < recipe.resolution; ++x) {
                const std::uint32_t panelX = x % cellWidth;
                const std::uint32_t panelY = y % cellHeight;
                const std::uint32_t edge = std::min({panelX, cellWidth - 1U - panelX,
                    panelY, cellHeight - 1U - panelY});
                const std::uint64_t panelId = (static_cast<std::uint64_t>(y / cellHeight) << 32U)
                    | (x / cellWidth);
                const std::uint64_t pixelId = (static_cast<std::uint64_t>(y) << 32U) | x;
                const float tone = 0.15F + 0.34F * unitNoise(
                    wkc::core::hashCombine(recipe.seed ^ 0x47414C4C455259ULL, panelId));
                wkc::core::Vec3 color = mix(recipe.baseColor, recipe.secondaryColor, tone);
                if (edge <= jointWidth) {
                    color = joint;
                } else {
                    const float patina = smoothNoise(recipe.seed ^ 0x434F4D4257414C4CULL,
                        x, y, wall ? 23U : 31U);
                    const float grain = unitNoise(wkc::core::hashCombine(
                        recipe.seed ^ 0x475241494E434D42ULL, pixelId));
                    color = multiply(color, 0.93F + 0.10F * patina
                        + (grain - 0.5F) * (floor ? 0.055F : 0.035F));
                    if (edge <= bevelWidth) {
                        color = multiply(color, wall ? 0.84F + 0.10F * patina
                            : 0.87F + 0.07F * patina);
                        const float chip = unitNoise(wkc::core::hashCombine(
                            recipe.seed ^ 0x4348495045444745ULL, pixelId));
                        if (chip < recipe.wearAmount * (wall ? 0.13F : 0.065F)) {
                            color = mix(color, exposedSteel, wall ? 0.48F : 0.24F);
                        }
                    } else {
                        // Fine wear rises through Ready, Active and Scarred.
                        // Impacts are handled by world-space decals, so this
                        // tile contains no duplicated bullet-hole pattern.
                        const float abrasion = unitNoise(wkc::core::hashCombine(
                            recipe.seed ^ 0x4142524153494F4EULL, pixelId));
                        if (abrasion < recipe.wearAmount * (wall ? 0.028F : 0.018F)) {
                            color = mix(color, exposedSteel, wall ? 0.25F : 0.12F);
                        }
                        const float grime = unitNoise(wkc::core::hashCombine(
                            recipe.seed ^ 0x4752494D4557414CULL, pixelId));
                        if (grime < recipe.impactAmount * (wall ? 0.018F : 0.007F)) {
                            color = multiply(color, 0.68F);
                        }
                    }
                    if (wall) {
                        // Small corner screws belong to each steel plate.
                        const int dx = std::min(std::abs(static_cast<int>(panelX) - 12),
                            std::abs(static_cast<int>(panelX) - static_cast<int>(cellWidth) + 13));
                        const int dy = std::min(std::abs(static_cast<int>(panelY) - 12),
                            std::abs(static_cast<int>(panelY) - static_cast<int>(cellHeight) + 13));
                        const int radius2 = dx * dx + dy * dy;
                        if (radius2 <= 7) {
                            color = radius2 <= 2 ? joint : exposedSteel;
                        }
                    }
                }
                writePixel(image, x, y, color);
            }
        }
        return image;
    }

    for (std::uint32_t y = 0; y < recipe.resolution; ++y) {
        for (std::uint32_t x = 0; x < recipe.resolution; ++x) {
            const std::uint64_t pixelId = (static_cast<std::uint64_t>(y) << 32U) | x;
            const std::uint64_t panelId = (static_cast<std::uint64_t>(y / cellHeight) << 32U)
                | (x / cellWidth);
            const float panelBlend = 0.08F + (unitNoise(
                wkc::core::hashCombine(recipe.seed, panelId)) * 0.22F);
            wkc::core::Vec3 color = mix(
                recipe.baseColor, recipe.secondaryColor, panelBlend);
            const std::uint32_t panelX = x % cellWidth;
            const std::uint32_t panelY = y % cellHeight;
            const bool seam = panelX <= 2U || panelY <= 2U;
            if (seam) {
                color = multiply(color, 0.45F);
            } else if (panelX == 3U || panelY == 3U) {
                color = add(color, 0.055F);
            }

            const float wearNoise = unitNoise(wkc::core::hashCombine(
                recipe.seed ^ 0xBA771EULL, pixelId));
            if (wearNoise < recipe.wearAmount * 0.055F) {
                color = mix(color, {0.41F, 0.42F, 0.39F}, 0.64F);
            }

            if (recipe.surface == RoomSurface::Wall) {
                const std::uint32_t border = recipe.reinforcementAmount > 0.48F ? 4U : 3U;
                if (panelX < border || panelX + border >= cellWidth
                    || panelY < border || panelY + border >= cellHeight) {
                    color = multiply(color, 0.62F);
                }
                if (recipe.reinforcementAmount > 0.52F
                    && std::abs(static_cast<int>(panelX)
                        - static_cast<int>(panelY)) <= 1) {
                    color = multiply(color, 0.72F);
                }
                for (std::uint32_t impact = 0; impact < impactCount; ++impact) {
                    const std::uint64_t impactSeed = wkc::core::hashCombine(
                        recipe.seed ^ 0x1A4AC7ULL, impact);
                    const int centerX = 8 + static_cast<int>(impactSeed % 48U);
                    const int centerY = 8 + static_cast<int>((impactSeed >> 16U) % 48U);
                    const int radius = 2 + static_cast<int>((impactSeed >> 32U) % 2U);
                    const int dx = static_cast<int>(x) - centerX;
                    const int dy = static_cast<int>(y) - centerY;
                    const int distanceSquared = (dx * dx) + (dy * dy);
                    if (distanceSquared <= radius * radius) {
                        color = distanceSquared <= 1
                            ? wkc::core::Vec3{0.045F, 0.05F, 0.04F}
                            : wkc::core::Vec3{0.45F, 0.46F, 0.42F};
                    }
                }
                const std::uint32_t markY = recipe.resolution * 3U / 4U;
                if (y >= markY && y <= markY + 1U
                    && x > recipe.resolution / 8U && x < recipe.resolution * 3U / 8U) {
                    color = recipe.accentColor;
                }
            } else if (recipe.surface == RoomSurface::Floor) {
                const std::uint32_t inset = recipe.resolution / 8U;
                const bool arenaLine = ((x == inset || x == recipe.resolution - inset - 1U)
                        && y >= inset && y < recipe.resolution - inset)
                    || ((y == inset || y == recipe.resolution - inset - 1U)
                        && x >= inset && x < recipe.resolution - inset);
                if (arenaLine) {
                    color = recipe.accentColor;
                }
                const float scuffNoise = unitNoise(wkc::core::hashCombine(
                    recipe.seed ^ 0xF1002ULL, pixelId));
                if (!arenaLine && scuffNoise < recipe.impactAmount * 0.028F) {
                    color = multiply(color, 0.62F);
                }
            } else {
                const std::uint32_t beamWidth = recipe.reinforcementAmount > 0.48F ? 6U : 4U;
                if (panelX < beamWidth) {
                    color = multiply(color, 0.48F);
                }
            }
            writePixel(image, x, y, color);
        }
    }
    return image;
}

CafeteriaRoomMaterialSet generateCafeteriaRoomMaterials(const wkc::core::Seed roomSeed)
{
    CafeteriaRoomMaterialSet materials;
    materials.roomSeed = roomSeed;
    materials.condition = selectCafeteriaCondition(roomSeed);
    materials.surfaces = {
        makeCafeteriaSurfaceRecipe(roomSeed, materials.condition, RoomSurface::Wall),
        makeCafeteriaSurfaceRecipe(roomSeed, materials.condition, RoomSurface::Floor),
        makeCafeteriaSurfaceRecipe(roomSeed, materials.condition, RoomSurface::Ceiling),
    };
    materials.materialHash = cafeteriaRoomMaterialHash(materials);
    return materials;
}

CafeteriaMaterialValidationResult validateCafeteriaSurfaceRecipe(
    const CafeteriaSurfaceRecipe& recipe)
{
    CafeteriaMaterialValidationResult result;
    if (recipe.surface >= RoomSurface::Count) {
        result.errors.push_back(CafeteriaMaterialValidationError::InvalidSurface);
    }
    if (recipe.family != MaterialFamily::Ceramic
        && recipe.family != MaterialFamily::Aluminum) {
        result.errors.push_back(CafeteriaMaterialValidationError::InvalidFamily);
    }
    if (!isUnitColor(recipe.baseColor) || !isUnitColor(recipe.secondaryColor)
        || !isUnitColor(recipe.accentColor)) {
        result.errors.push_back(CafeteriaMaterialValidationError::InvalidColor);
    }
    if (recipe.panelColumns < 2U || recipe.panelColumns > 4U
        || recipe.panelRows < 2U || recipe.panelRows > 4U) {
        result.errors.push_back(CafeteriaMaterialValidationError::InvalidPanelGrid);
    }
    if (!isUnitValue(recipe.wearAmount) || !isUnitValue(recipe.stainAmount)
        || !isUnitValue(recipe.stripeAmount) || !isUnitValue(recipe.furnitureMarkAmount)
        || !isUnitValue(recipe.emissiveAmount)) {
        result.errors.push_back(CafeteriaMaterialValidationError::InvalidParameter);
    }
    const bool powerOfTwo = recipe.resolution != 0U
        && (recipe.resolution & (recipe.resolution - 1U)) == 0U;
    if (!powerOfTwo || recipe.resolution < 32U || recipe.resolution > 256U) {
        result.errors.push_back(CafeteriaMaterialValidationError::InvalidResolution);
    }
    if (recipe.surface < RoomSurface::Count) {
        const bool isWall = recipe.surface == RoomSurface::Wall;
        const bool isFloor = recipe.surface == RoomSurface::Floor;
        const bool isCeiling = recipe.surface == RoomSurface::Ceiling;
        const bool floorGrid = recipe.panelColumns == recipe.panelRows
            && (recipe.panelColumns == 2U || recipe.panelColumns == 4U);
        if ((isWall && (recipe.family != MaterialFamily::Ceramic
                           || recipe.panelRows != 2U
                           || recipe.stripeAmount < 0.08F
                           || recipe.stripeAmount > 0.12F
                           || recipe.furnitureMarkAmount != 0.0F))
            || (isFloor && (recipe.family != MaterialFamily::Ceramic
                               || !floorGrid
                               || recipe.stripeAmount != 0.0F
                               || recipe.furnitureMarkAmount <= 0.0F))
            || (isCeiling && (recipe.family != MaterialFamily::Aluminum
                                 || recipe.panelColumns != 2U
                                 || recipe.stripeAmount != 0.0F
                                 || recipe.furnitureMarkAmount != 0.0F
                                 || recipe.emissiveAmount <= 0.0F))
            || (!isCeiling && recipe.emissiveAmount != 0.0F)) {
            result.errors.push_back(CafeteriaMaterialValidationError::InvalidSurfaceRule);
        }
    }
    return result;
}

CafeteriaMaterialValidationResult validateCafeteriaRoomMaterials(
    const CafeteriaRoomMaterialSet& materials)
{
    CafeteriaMaterialValidationResult result;
    if (materials.condition >= CafeteriaCondition::Count) {
        result.errors.push_back(CafeteriaMaterialValidationError::InvalidCondition);
    }
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        const CafeteriaSurfaceRecipe& recipe = materials.surfaces[index];
        const auto surfaceValidation = validateCafeteriaSurfaceRecipe(recipe);
        result.errors.insert(
            result.errors.end(), surfaceValidation.errors.begin(), surfaceValidation.errors.end());
        if (surfaceIndex(recipe.surface) != index) {
            result.errors.push_back(CafeteriaMaterialValidationError::InvalidSurfaceOrder);
        }
        const wkc::core::Seed expectedSeed = wkc::core::deriveSeed(
            materials.roomSeed, gdp::game::SeedDomain::RoomMaterial, index + 301U);
        if (recipe.seed != expectedSeed) {
            result.errors.push_back(CafeteriaMaterialValidationError::InvalidSurfaceOrder);
        }
    }
    if (materials.materialHash == 0U
        || materials.materialHash != cafeteriaRoomMaterialHash(materials)) {
        result.errors.push_back(CafeteriaMaterialValidationError::InvalidMaterialHash);
    }
    return result;
}

std::uint64_t cafeteriaSurfaceRecipeHash(const CafeteriaSurfaceRecipe& recipe) noexcept
{
    std::uint64_t hash = wkc::core::hashCombine(0x4341465F53555246ULL, recipe.version);
    hash = wkc::core::hashCombine(hash, recipe.seed);
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.surface));
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.family));
    hash = addVec3(hash, recipe.baseColor);
    hash = addVec3(hash, recipe.secondaryColor);
    hash = addVec3(hash, recipe.accentColor);
    hash = wkc::core::hashCombine(hash, recipe.panelColumns);
    hash = wkc::core::hashCombine(hash, recipe.panelRows);
    hash = addFloat(hash, recipe.wearAmount);
    hash = addFloat(hash, recipe.stainAmount);
    hash = addFloat(hash, recipe.stripeAmount);
    hash = addFloat(hash, recipe.furnitureMarkAmount);
    hash = addFloat(hash, recipe.emissiveAmount);
    return wkc::core::hashCombine(hash, recipe.resolution);
}

std::uint64_t cafeteriaRoomMaterialHash(const CafeteriaRoomMaterialSet& materials) noexcept
{
    std::uint64_t hash = wkc::core::hashCombine(
        0x4341465F524F4F4DULL, materials.roomSeed);
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(materials.condition));
    for (const CafeteriaSurfaceRecipe& recipe : materials.surfaces) {
        hash = wkc::core::hashCombine(hash, cafeteriaSurfaceRecipeHash(recipe));
    }
    return hash;
}

std::optional<wkc::render::ImageData> generateCafeteriaSurfaceAlbedo(
    const CafeteriaSurfaceRecipe& recipe)
{
    if (!validateCafeteriaSurfaceRecipe(recipe).isValid()) {
        return std::nullopt;
    }

    wkc::render::ImageData image;
    image.width = static_cast<int>(recipe.resolution);
    image.height = static_cast<int>(recipe.resolution);
    image.rgba.resize(static_cast<std::size_t>(recipe.resolution)
        * static_cast<std::size_t>(recipe.resolution) * 4U);
    const std::uint32_t cellWidth = recipe.resolution / recipe.panelColumns;
    const std::uint32_t cellHeight = recipe.resolution / recipe.panelRows;

    for (std::uint32_t y = 0; y < recipe.resolution; ++y) {
        for (std::uint32_t x = 0; x < recipe.resolution; ++x) {
            const std::uint64_t pixelId = (static_cast<std::uint64_t>(y) << 32U) | x;
            const std::uint64_t panelId = (static_cast<std::uint64_t>(y / cellHeight) << 32U)
                | (x / cellWidth);
            const float panelBlend = 0.05F + unitNoise(
                wkc::core::hashCombine(recipe.seed, panelId)) * 0.10F;
            wkc::core::Vec3 color = mix(
                recipe.baseColor, recipe.secondaryColor, panelBlend);
            const std::uint32_t panelX = x % cellWidth;
            const std::uint32_t panelY = y % cellHeight;
            const bool seam = panelX <= 1U || panelY <= 1U;
            if (seam) {
                color = multiply(color, 0.72F);
            }

            const float wearNoise = unitNoise(wkc::core::hashCombine(
                recipe.seed ^ 0xCAFE71AULL, pixelId));
            if (!seam && wearNoise < recipe.wearAmount * 0.022F) {
                color = mix(color, recipe.secondaryColor, 0.48F);
            }

            if (recipe.surface == RoomSurface::Wall) {
                const std::uint32_t stripeHeight = std::max(5U,
                    static_cast<std::uint32_t>(std::lround(
                        static_cast<float>(recipe.resolution) * recipe.stripeAmount)));
                const std::uint32_t stripeStart = recipe.resolution * 2U / 5U;
                if (y >= stripeStart && y < stripeStart + stripeHeight) {
                    color = wearNoise < recipe.wearAmount * 0.018F
                        ? mix(recipe.accentColor, recipe.secondaryColor, 0.38F)
                        : recipe.accentColor;
                } else if (y < recipe.resolution / 10U) {
                    const float baseDirt = unitNoise(wkc::core::hashCombine(
                        recipe.seed ^ 0xBA5ED17ULL, x / 4U));
                    if (baseDirt < recipe.stainAmount) {
                        color = multiply(color, 0.80F);
                    }
                }
            } else if (recipe.surface == RoomSurface::Floor) {
                const bool trafficLane = x >= recipe.resolution * 3U / 8U
                    && x < recipe.resolution * 5U / 8U;
                if (!seam && trafficLane) {
                    color = mix(color, recipe.accentColor, 0.28F);
                }
                const std::uint64_t stainCell = (static_cast<std::uint64_t>(y / 8U) << 32U)
                    | (x / 8U);
                if (!seam && unitNoise(wkc::core::hashCombine(
                        recipe.seed ^ 0x57A1AULL, stainCell)) < recipe.stainAmount * 0.18F) {
                    color = multiply(color, 0.82F);
                }
                for (std::uint32_t mark = 0; mark < 4U; ++mark) {
                    const std::uint64_t markSeed = wkc::core::hashCombine(
                        recipe.seed ^ 0xF007ULL, mark);
                    const int baseX = (mark % 2U == 0U) ? 14 : 50;
                    const int baseY = (mark < 2U) ? 14 : 50;
                    const int centerX = baseX + static_cast<int>(markSeed % 5U) - 2;
                    const int centerY = baseY + static_cast<int>((markSeed >> 8U) % 5U) - 2;
                    const int dx = static_cast<int>(x) - centerX;
                    const int dy = static_cast<int>(y) - centerY;
                    const int distanceSquared = (dx * dx) + (dy * dy);
                    if (distanceSquared >= 3 && distanceSquared <= 6) {
                        color = multiply(recipe.secondaryColor,
                            0.68F + recipe.furnitureMarkAmount);
                    }
                }
            }
            writePixel(image, x, y, color);
        }
    }
    return image;
}

LaboratoryRoomMaterialSet generateLaboratoryRoomMaterials(const wkc::core::Seed roomSeed)
{
    LaboratoryRoomMaterialSet materials;
    materials.roomSeed = roomSeed;
    materials.condition = selectLaboratoryCondition(roomSeed);
    materials.surfaces = {
        makeLaboratorySurfaceRecipe(roomSeed, materials.condition, RoomSurface::Wall),
        makeLaboratorySurfaceRecipe(roomSeed, materials.condition, RoomSurface::Floor),
        makeLaboratorySurfaceRecipe(roomSeed, materials.condition, RoomSurface::Ceiling),
    };
    materials.materialHash = laboratoryRoomMaterialHash(materials);
    return materials;
}

LaboratoryMaterialValidationResult validateLaboratorySurfaceRecipe(
    const LaboratorySurfaceRecipe& recipe)
{
    LaboratoryMaterialValidationResult result;
    if (recipe.surface >= RoomSurface::Count) {
        result.errors.push_back(LaboratoryMaterialValidationError::InvalidSurface);
    }
    if (recipe.family != MaterialFamily::Ceramic
        && recipe.family != MaterialFamily::Polymer
        && recipe.family != MaterialFamily::Aluminum) {
        result.errors.push_back(LaboratoryMaterialValidationError::InvalidFamily);
    }
    if (!isUnitColor(recipe.baseColor) || !isUnitColor(recipe.secondaryColor)
        || !isUnitColor(recipe.accentColor)) {
        result.errors.push_back(LaboratoryMaterialValidationError::InvalidColor);
    }
    if (recipe.panelColumns < 2U || recipe.panelColumns > 6U
        || recipe.panelRows < 2U || recipe.panelRows > 4U) {
        result.errors.push_back(LaboratoryMaterialValidationError::InvalidPanelGrid);
    }
    if (!isUnitValue(recipe.wearAmount) || !isUnitValue(recipe.detailAmount)
        || !isUnitValue(recipe.contaminationAmount) || !isUnitValue(recipe.lightingCoverage)
        || !isUnitValue(recipe.emissiveAmount)) {
        result.errors.push_back(LaboratoryMaterialValidationError::InvalidParameter);
    }
    const bool powerOfTwo = recipe.resolution != 0U
        && (recipe.resolution & (recipe.resolution - 1U)) == 0U;
    if (!powerOfTwo || recipe.resolution < 32U || recipe.resolution > 256U) {
        result.errors.push_back(LaboratoryMaterialValidationError::InvalidResolution);
    }
    if (recipe.surface < RoomSurface::Count) {
        const bool isWall = recipe.surface == RoomSurface::Wall;
        const bool isFloor = recipe.surface == RoomSurface::Floor;
        const bool isCeiling = recipe.surface == RoomSurface::Ceiling;
        const bool floorGrid = recipe.panelColumns == recipe.panelRows
            && (recipe.panelColumns == 2U || recipe.panelColumns == 4U);
        if ((isWall && (recipe.family != MaterialFamily::Ceramic
                           || recipe.panelRows != 2U
                           || recipe.detailAmount <= 0.0F
                           || recipe.contaminationAmount != 0.0F))
            || (isFloor && (recipe.family != MaterialFamily::Polymer
                               || !floorGrid
                               || recipe.detailAmount <= 0.0F))
            || (isCeiling && (recipe.family != MaterialFamily::Aluminum
                                 || recipe.panelColumns != 2U
                                 || recipe.contaminationAmount != 0.0F
                                 || recipe.lightingCoverage < 0.25F
                                 || recipe.lightingCoverage > 0.34F
                                 || recipe.emissiveAmount <= 0.0F))
            || (!isCeiling
                && (recipe.lightingCoverage != 0.0F || recipe.emissiveAmount != 0.0F))) {
            result.errors.push_back(LaboratoryMaterialValidationError::InvalidSurfaceRule);
        }
    }
    return result;
}

LaboratoryMaterialValidationResult validateLaboratoryRoomMaterials(
    const LaboratoryRoomMaterialSet& materials)
{
    LaboratoryMaterialValidationResult result;
    if (materials.condition >= LaboratoryCondition::Count) {
        result.errors.push_back(LaboratoryMaterialValidationError::InvalidCondition);
    }
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        const LaboratorySurfaceRecipe& recipe = materials.surfaces[index];
        const auto surfaceValidation = validateLaboratorySurfaceRecipe(recipe);
        result.errors.insert(
            result.errors.end(), surfaceValidation.errors.begin(), surfaceValidation.errors.end());
        if (surfaceIndex(recipe.surface) != index) {
            result.errors.push_back(LaboratoryMaterialValidationError::InvalidSurfaceOrder);
        }
        const wkc::core::Seed expectedSeed = wkc::core::deriveSeed(
            materials.roomSeed, gdp::game::SeedDomain::RoomMaterial, index + 401U);
        if (recipe.seed != expectedSeed) {
            result.errors.push_back(LaboratoryMaterialValidationError::InvalidSurfaceOrder);
        }
    }
    if (materials.materialHash == 0U
        || materials.materialHash != laboratoryRoomMaterialHash(materials)) {
        result.errors.push_back(LaboratoryMaterialValidationError::InvalidMaterialHash);
    }
    return result;
}

std::uint64_t laboratorySurfaceRecipeHash(const LaboratorySurfaceRecipe& recipe) noexcept
{
    std::uint64_t hash = wkc::core::hashCombine(0x4C41425F53555246ULL, recipe.version);
    hash = wkc::core::hashCombine(hash, recipe.seed);
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.surface));
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.family));
    hash = addVec3(hash, recipe.baseColor);
    hash = addVec3(hash, recipe.secondaryColor);
    hash = addVec3(hash, recipe.accentColor);
    hash = wkc::core::hashCombine(hash, recipe.panelColumns);
    hash = wkc::core::hashCombine(hash, recipe.panelRows);
    hash = addFloat(hash, recipe.wearAmount);
    hash = addFloat(hash, recipe.detailAmount);
    hash = addFloat(hash, recipe.contaminationAmount);
    hash = addFloat(hash, recipe.lightingCoverage);
    hash = addFloat(hash, recipe.emissiveAmount);
    return wkc::core::hashCombine(hash, recipe.resolution);
}

std::uint64_t laboratoryRoomMaterialHash(const LaboratoryRoomMaterialSet& materials) noexcept
{
    std::uint64_t hash = wkc::core::hashCombine(
        0x4C41425F524F4F4DULL, materials.roomSeed);
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(materials.condition));
    for (const LaboratorySurfaceRecipe& recipe : materials.surfaces) {
        hash = wkc::core::hashCombine(hash, laboratorySurfaceRecipeHash(recipe));
    }
    return hash;
}

std::optional<wkc::render::ImageData> generateLaboratorySurfaceAlbedo(
    const LaboratorySurfaceRecipe& recipe)
{
    if (!validateLaboratorySurfaceRecipe(recipe).isValid()) {
        return std::nullopt;
    }

    wkc::render::ImageData image;
    image.width = static_cast<int>(recipe.resolution);
    image.height = static_cast<int>(recipe.resolution);
    image.rgba.resize(static_cast<std::size_t>(recipe.resolution)
        * static_cast<std::size_t>(recipe.resolution) * 4U);
    const std::uint32_t cellWidth = recipe.resolution / recipe.panelColumns;
    const std::uint32_t cellHeight = recipe.resolution / recipe.panelRows;

    for (std::uint32_t y = 0; y < recipe.resolution; ++y) {
        for (std::uint32_t x = 0; x < recipe.resolution; ++x) {
            const std::uint64_t pixelId = (static_cast<std::uint64_t>(y) << 32U) | x;
            const std::uint64_t panelId = (static_cast<std::uint64_t>(y / cellHeight) << 32U)
                | (x / cellWidth);
            const float panelBlend = 0.025F + unitNoise(
                wkc::core::hashCombine(recipe.seed, panelId)) * 0.05F;
            wkc::core::Vec3 color = mix(
                recipe.baseColor, recipe.secondaryColor, panelBlend);
            const std::uint32_t panelX = x % cellWidth;
            const std::uint32_t panelY = y % cellHeight;
            const bool seam = panelX == 0U || panelY == 0U;
            if (seam) {
                color = multiply(color, 0.66F);
            }
            const float wearNoise = unitNoise(wkc::core::hashCombine(
                recipe.seed ^ 0x1AB71AULL, pixelId));
            if (!seam && wearNoise < recipe.wearAmount * 0.012F) {
                color = mix(color, recipe.secondaryColor, 0.36F);
            }

            if (recipe.surface == RoomSurface::Wall) {
                const std::uint32_t moduleLeft = recipe.resolution * 9U / 16U;
                const std::uint32_t moduleRight = recipe.resolution * 15U / 16U;
                const std::uint32_t moduleBottom = recipe.resolution / 4U;
                const std::uint32_t moduleTop = recipe.resolution * 2U / 3U;
                const bool module = x >= moduleLeft && x < moduleRight
                    && y >= moduleBottom && y < moduleTop;
                const bool moduleBorder = module && (x < moduleLeft + 2U
                    || x + 2U >= moduleRight || y < moduleBottom + 2U
                    || y + 2U >= moduleTop);
                if (module) {
                    color = moduleBorder ? recipe.accentColor
                                         : wkc::core::Vec3{0.12F, 0.16F, 0.19F};
                }
                const bool indicatorA = x >= moduleLeft + 5U && x < moduleLeft + 7U
                    && y >= moduleBottom + 6U && y < moduleBottom + 8U;
                const bool indicatorB = x >= moduleLeft + 11U && x < moduleLeft + 13U
                    && y >= moduleBottom + 6U && y < moduleBottom + 8U;
                if (indicatorA || indicatorB) {
                    color = {0.25F, 0.55F, 0.32F};
                }
            } else if (recipe.surface == RoomSurface::Floor) {
                const bool cornerLock = panelX >= 2U && panelX <= 3U
                    && panelY >= 2U && panelY <= 3U;
                if (cornerLock) {
                    color = multiply(recipe.secondaryColor, 0.68F);
                }
                const std::uint32_t hatchMin = recipe.resolution * 3U / 8U;
                const std::uint32_t hatchMax = recipe.resolution * 5U / 8U;
                const bool hatch = ((x == hatchMin || x == hatchMax)
                        && y >= hatchMin && y <= hatchMax)
                    || ((y == hatchMin || y == hatchMax)
                        && x >= hatchMin && x <= hatchMax);
                if (hatch) {
                    color = recipe.accentColor;
                }
                if (recipe.contaminationAmount > 0.06F) {
                    const int centerX = static_cast<int>(recipe.resolution * 3U / 4U);
                    const int centerY = static_cast<int>(recipe.resolution / 4U);
                    const int dx = static_cast<int>(x) - centerX;
                    const int dy = static_cast<int>(y) - centerY;
                    const int ellipse = (dx * dx * 25) + (dy * dy * 64);
                    if (ellipse <= 1'600) {
                        color = mix(color, {0.25F, 0.34F, 0.27F},
                            0.34F + recipe.contaminationAmount);
                    }
                }
            }
            writePixel(image, x, y, color);
        }
    }
    return image;
}

TechnicalRoomMaterialSet generateTechnicalRoomMaterials(const wkc::core::Seed roomSeed)
{
    TechnicalRoomMaterialSet materials;
    materials.roomSeed = roomSeed;
    materials.condition = selectTechnicalCondition(roomSeed);
    materials.surfaces = {
        makeTechnicalSurfaceRecipe(roomSeed, materials.condition, RoomSurface::Wall),
        makeTechnicalSurfaceRecipe(roomSeed, materials.condition, RoomSurface::Floor),
        makeTechnicalSurfaceRecipe(roomSeed, materials.condition, RoomSurface::Ceiling),
    };
    materials.materialHash = technicalRoomMaterialHash(materials);
    return materials;
}

TechnicalMaterialValidationResult validateTechnicalSurfaceRecipe(
    const TechnicalSurfaceRecipe& recipe)
{
    TechnicalMaterialValidationResult result;
    if (recipe.surface >= RoomSurface::Count) {
        result.errors.push_back(TechnicalMaterialValidationError::InvalidSurface);
    }
    if (recipe.family != MaterialFamily::PaintedSteel
        && recipe.family != MaterialFamily::BareSteel) {
        result.errors.push_back(TechnicalMaterialValidationError::InvalidFamily);
    }
    if (!isUnitColor(recipe.baseColor) || !isUnitColor(recipe.secondaryColor)
        || !isUnitColor(recipe.accentColor)) {
        result.errors.push_back(TechnicalMaterialValidationError::InvalidColor);
    }
    if (recipe.panelColumns != 2U || recipe.panelRows < 2U || recipe.panelRows > 3U) {
        result.errors.push_back(TechnicalMaterialValidationError::InvalidPanelGrid);
    }
    if (!isUnitValue(recipe.wearAmount) || !isUnitValue(recipe.dirtAmount)
        || !isUnitValue(recipe.pipeAmount) || !isUnitValue(recipe.oilAmount)
        || !isUnitValue(recipe.mechanicalAmount) || !isUnitValue(recipe.emissiveAmount)) {
        result.errors.push_back(TechnicalMaterialValidationError::InvalidParameter);
    }
    const bool powerOfTwo = recipe.resolution != 0U
        && (recipe.resolution & (recipe.resolution - 1U)) == 0U;
    if (!powerOfTwo || recipe.resolution < 32U || recipe.resolution > 256U) {
        result.errors.push_back(TechnicalMaterialValidationError::InvalidResolution);
    }
    if (recipe.surface < RoomSurface::Count) {
        const bool isWall = recipe.surface == RoomSurface::Wall;
        const bool isFloor = recipe.surface == RoomSurface::Floor;
        const bool isCeiling = recipe.surface == RoomSurface::Ceiling;
        if ((isWall && (recipe.family != MaterialFamily::PaintedSteel
                           || recipe.panelRows != 2U
                           || recipe.pipeAmount != 0.0F
                           || recipe.oilAmount != 0.0F
                           || recipe.mechanicalAmount <= 0.0F))
            || (isFloor && (recipe.family != MaterialFamily::BareSteel
                               || recipe.panelRows != 2U
                               || recipe.pipeAmount != 0.0F
                               || recipe.oilAmount <= 0.0F
                               || recipe.mechanicalAmount <= 0.0F))
            || (isCeiling && (recipe.family != MaterialFamily::BareSteel
                                 || recipe.pipeAmount != 0.0F
                                 || recipe.oilAmount != 0.0F
                                 || recipe.mechanicalAmount <= 0.0F
                                 || recipe.emissiveAmount != 0.0F))
            || (!isCeiling && recipe.emissiveAmount != 0.0F)) {
            result.errors.push_back(TechnicalMaterialValidationError::InvalidSurfaceRule);
        }
    }
    return result;
}

TechnicalMaterialValidationResult validateTechnicalRoomMaterials(
    const TechnicalRoomMaterialSet& materials)
{
    TechnicalMaterialValidationResult result;
    if (materials.condition >= TechnicalCondition::Count) {
        result.errors.push_back(TechnicalMaterialValidationError::InvalidCondition);
    }
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        const TechnicalSurfaceRecipe& recipe = materials.surfaces[index];
        const auto surfaceValidation = validateTechnicalSurfaceRecipe(recipe);
        result.errors.insert(
            result.errors.end(), surfaceValidation.errors.begin(), surfaceValidation.errors.end());
        if (surfaceIndex(recipe.surface) != index) {
            result.errors.push_back(TechnicalMaterialValidationError::InvalidSurfaceOrder);
        }
        const wkc::core::Seed expectedSeed = wkc::core::deriveSeed(
            materials.roomSeed, gdp::game::SeedDomain::RoomMaterial, index + 501U);
        if (recipe.seed != expectedSeed) {
            result.errors.push_back(TechnicalMaterialValidationError::InvalidSurfaceOrder);
        }
    }
    if (materials.materialHash == 0U
        || materials.materialHash != technicalRoomMaterialHash(materials)) {
        result.errors.push_back(TechnicalMaterialValidationError::InvalidMaterialHash);
    }
    return result;
}

std::uint64_t technicalSurfaceRecipeHash(const TechnicalSurfaceRecipe& recipe) noexcept
{
    std::uint64_t hash = wkc::core::hashCombine(0x544543485F535552ULL, recipe.version);
    hash = wkc::core::hashCombine(hash, recipe.seed);
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.surface));
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.family));
    hash = addVec3(hash, recipe.baseColor);
    hash = addVec3(hash, recipe.secondaryColor);
    hash = addVec3(hash, recipe.accentColor);
    hash = wkc::core::hashCombine(hash, recipe.panelColumns);
    hash = wkc::core::hashCombine(hash, recipe.panelRows);
    hash = addFloat(hash, recipe.wearAmount);
    hash = addFloat(hash, recipe.dirtAmount);
    hash = addFloat(hash, recipe.pipeAmount);
    hash = addFloat(hash, recipe.oilAmount);
    hash = addFloat(hash, recipe.mechanicalAmount);
    hash = addFloat(hash, recipe.emissiveAmount);
    return wkc::core::hashCombine(hash, recipe.resolution);
}

std::uint64_t technicalRoomMaterialHash(const TechnicalRoomMaterialSet& materials) noexcept
{
    std::uint64_t hash = wkc::core::hashCombine(
        0x544543485F524F4FULL, materials.roomSeed);
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(materials.condition));
    for (const TechnicalSurfaceRecipe& recipe : materials.surfaces) {
        hash = wkc::core::hashCombine(hash, technicalSurfaceRecipeHash(recipe));
    }
    return hash;
}

std::optional<wkc::render::ImageData> generateTechnicalSurfaceAlbedo(
    const TechnicalSurfaceRecipe& recipe)
{
    if (!validateTechnicalSurfaceRecipe(recipe).isValid()) {
        return std::nullopt;
    }

    if (recipe.version == 4U && recipe.surface != RoomSurface::Ceiling) {
        // One atlas covers the room at 32 px/m. Wall U follows the complete
        // perimeter; floor UVs start at the room's minimum X/Z. The plate
        // layout is therefore continuous at corners and cannot form a short
        // repeating pattern within a room.
        const bool wall = recipe.surface == RoomSurface::Wall;
        wkc::render::ImageData image;
        image.width = static_cast<int>((wall ? 48U : 12U) * recipe.resolution);
        image.height = static_cast<int>((wall ? 4U : 12U) * recipe.resolution);
        image.rgba.resize(static_cast<std::size_t>(image.width)
            * static_cast<std::size_t>(image.height) * 4U);
        constexpr std::array wallRowStarts{0U, 32U, 88U, 144U, 200U};
        constexpr std::array wallPlateWidths{256U, 128U, 112U, 144U, 120U};
        constexpr std::array wallOffsets{0U, 0U, 56U, 104U, 32U};
        constexpr std::array oilCenters{
            std::pair{106, 155}, std::pair{438, 281}, std::pair{272, 522}};
        for (std::uint32_t y = 0; y < static_cast<std::uint32_t>(image.height); ++y) {
            const std::uint32_t course = wall
                ? (y < 32U ? 0U : y < 88U ? 1U : y < 144U ? 2U
                    : y < 200U ? 3U : 4U)
                : y / 96U;
            const std::uint32_t rowStart = wall ? wallRowStarts[course] : course * 96U;
            const std::uint32_t rowHeight = wall
                ? (course == 0U ? 32U : course == 4U ? 56U
                    : wallRowStarts[course + 1U] - rowStart)
                : 96U;
            const std::uint32_t plateWidth = wall ? wallPlateWidths[course] : 128U;
            const std::uint32_t offset = wall ? wallOffsets[course]
                : (course % 2U == 0U ? 0U : 64U);
            for (std::uint32_t x = 0; x < static_cast<std::uint32_t>(image.width);
                 ++x) {
                const std::uint32_t along = x + offset;
                const std::uint32_t panelX = along % plateWidth;
                const std::uint32_t panelY = y - rowStart;
                const std::uint64_t panelId
                    = (static_cast<std::uint64_t>(course) << 32U)
                    | (along / plateWidth);
                const std::uint64_t pixelId
                    = (static_cast<std::uint64_t>(y) << 32U) | x;
                const float panelTone = 0.82F + 0.32F * unitNoise(
                    wkc::core::hashCombine(recipe.seed, panelId));
                wkc::core::Vec3 color = multiply(mix(recipe.baseColor,
                    recipe.secondaryColor, 0.22F), panelTone);
                if (wall && course == 0U) {
                    // A continuous one-metre plinth, without square modules.
                    color = multiply(color, 0.31F);
                }
                const float brush = (smoothNoise(recipe.seed ^ 0x4252555348ULL,
                    x, y / 3U, 31U) - 0.5F) * (wall ? 0.060F : 0.038F);
                const float macro = (smoothNoise(recipe.seed ^ 0x4D4143524FULL,
                    x, y, 177U) - 0.5F) * (wall ? 0.065F : 0.035F);
                const float grain = (unitNoise(wkc::core::hashCombine(
                    recipe.seed ^ 0x475241494EULL, pixelId)) - 0.5F) * 0.022F;
                color = add(color, brush + macro + grain);
                const std::uint32_t edge = std::min({panelX,
                    plateWidth - 1U - panelX, panelY, rowHeight - 1U - panelY});
                const bool verticalSeam = panelX < 2U && !(wall && course == 0U);
                if (verticalSeam || panelY < 2U) {
                    color = wall ? wkc::core::Vec3{0.10F, 0.10F, 0.10F}
                                 : wkc::core::Vec3{0.065F, 0.075F, 0.075F};
                } else if (edge <= 5U) {
                    color = mix(color, recipe.secondaryColor, 0.27F);
                    const float chip = unitNoise(wkc::core::hashCombine(
                        recipe.seed ^ 0x4544474557ULL, pixelId));
                    if (chip < recipe.wearAmount * 0.10F) {
                        color = mix(color, {0.43F, 0.42F, 0.39F}, 0.27F);
                    }
                }
                if (wall && course > 0U && edge > 5U) {
                    // Broad, low-frequency grime at service height and a few
                    // fine brush marks make the painted sheets read as metal.
                    const float grime = smoothNoise(recipe.seed ^ 0x534D55444745ULL,
                        x, y, 83U);
                    const float lower = course == 1U ? 0.11F : 0.035F;
                    color = mix(color, {0.22F, 0.19F, 0.17F},
                        std::max(0.0F, grime - 0.48F) * lower
                            * (0.5F + recipe.dirtAmount));
                    const float scratch = unitNoise(wkc::core::hashCombine(
                        recipe.seed ^ 0x53435241544348ULL,
                        static_cast<std::uint64_t>(x / 2U) * 131U + y / 17U));
                    if (scratch > 0.992F && (y % 17U) < 11U) {
                        color = mix(color, {0.63F, 0.57F, 0.51F},
                            0.16F * recipe.wearAmount);
                    }
                }
                if (wall && course != 0U && panelX >= 5U && panelX < 7U
                    && (panelY == 5U || panelY + 6U == rowHeight)) {
                    color = {0.12F, 0.12F, 0.12F};
                }
                if (!wall && edge > 7U && recipe.oilAmount > 0.0F) {
                    for (const auto& [centerX, centerY] : oilCenters) {
                        const int dx = static_cast<int>(x) - centerX;
                        const int dy = static_cast<int>(y) - centerY;
                        const float distance = static_cast<float>(dx * dx) / 6'400.0F
                            + static_cast<float>(dy * dy) / 2'025.0F;
                        if (distance < 1.0F) {
                            const float opacity = (1.0F - distance)
                                * recipe.oilAmount * 0.22F;
                            color = mix(color, {0.09F, 0.10F, 0.10F}, opacity);
                        }
                    }
                }
                writePixel(image, x, y, color);
            }
        }
        return image;
    }

    if (recipe.version == 3U && recipe.surface != RoomSurface::Ceiling) {
        // The atlas covers an entire Technical room at a constant 32 px/m.
        // Every 2 m cell has its own steel wear and panel tone, so neither
        // wall nor floor repeats the same four plates across the room.
        const std::uint32_t columns = recipe.surface == RoomSurface::Wall ? 48U : 12U;
        const std::uint32_t rows = recipe.surface == RoomSurface::Wall ? 4U : 12U;
        wkc::render::ImageData atlas;
        atlas.width = static_cast<int>(columns * recipe.resolution);
        atlas.height = static_cast<int>(rows * recipe.resolution);
        atlas.rgba.resize(static_cast<std::size_t>(atlas.width)
            * static_cast<std::size_t>(atlas.height) * 4U);
        for (std::uint32_t row = 0; row < rows; ++row) {
            for (std::uint32_t column = 0; column < columns; ++column) {
                TechnicalSurfaceRecipe cellRecipe = recipe;
                cellRecipe.version = 2U;
                cellRecipe.seed = wkc::core::hashCombine(recipe.seed,
                    0x544543485F43454CULL + row * columns + column);
                const float tone = 0.84F + 0.30F * unitNoise(
                    wkc::core::hashCombine(cellRecipe.seed, 0x544F4E45ULL));
                const auto feature = wkc::core::hashCombine(
                    cellRecipe.seed, 0x46454154555245ULL) % 13U;
                cellRecipe.baseColor = multiply(recipe.baseColor, tone);
                cellRecipe.secondaryColor = multiply(recipe.secondaryColor, tone);
                const auto cell = generateTechnicalSurfaceAlbedo(cellRecipe);
                if (!cell) return std::nullopt;
                for (std::uint32_t y = 0; y < recipe.resolution; ++y) {
                    const auto source = cell->rgba.begin()
                        + static_cast<std::ptrdiff_t>(y * recipe.resolution * 4U);
                    const auto destination = atlas.rgba.begin()
                        + static_cast<std::ptrdiff_t>(
                            ((row * recipe.resolution + y) * columns * recipe.resolution
                                + column * recipe.resolution) * 4U);
                    std::copy_n(source, recipe.resolution * 4U, destination);
                    for (std::uint32_t x = 0; x < recipe.resolution; ++x) {
                        const bool accessPanel = recipe.surface == RoomSurface::Wall
                            && feature <= 1U && x >= 7U && x < 57U
                            && y >= 10U && y < 54U;
                        const bool wallStripe = recipe.surface == RoomSurface::Wall
                            && feature == 2U && x >= 6U && x < 58U
                            && y >= 28U && y < 36U;
                        const bool floorHatch = recipe.surface == RoomSurface::Floor
                            && feature <= 1U && x >= 8U && x < 56U
                            && y >= 8U && y < 56U;
                        const bool floorRepair = recipe.surface == RoomSurface::Floor
                            && feature == 2U && x >= 8U && x < 56U
                            && y >= 12U && y < 52U;
                        if (!accessPanel && !wallStripe && !floorHatch && !floorRepair)
                            continue;
                        auto* pixel = &*(destination + static_cast<std::ptrdiff_t>(x * 4U));
                        const bool border = x <= 10U || x >= 53U
                            || y <= (floorHatch ? 11U : 14U)
                            || y >= (floorRepair ? 48U : 51U);
                        if (border) {
                            pixel[0] = 27U; pixel[1] = 32U; pixel[2] = 34U;
                        } else if (wallStripe) {
                            pixel[0] = 120U; pixel[1] = 87U; pixel[2] = 48U;
                        } else if (floorHatch && y % 8U < 3U) {
                            pixel[0] = 23U; pixel[1] = 29U; pixel[2] = 30U;
                        } else {
                            const float shade = floorRepair ? 0.87F : 0.72F;
                            for (std::size_t channel = 0; channel < 3U; ++channel) {
                                pixel[channel] = static_cast<std::uint8_t>(
                                    static_cast<float>(pixel[channel]) * shade);
                            }
                        }
                    }
                }
            }
        }
        return atlas;
    }

    // One image covers 2 x 2 metres. Each half is a one-metre steel plate;
    // pipes, vents, grates, warning signs and lights belong to room geometry.
    wkc::render::ImageData image;
    image.width = static_cast<int>(recipe.resolution);
    image.height = static_cast<int>(recipe.resolution);
    image.rgba.resize(static_cast<std::size_t>(recipe.resolution)
        * static_cast<std::size_t>(recipe.resolution) * 4U);
    const std::uint32_t cellWidth = recipe.resolution / recipe.panelColumns;
    const std::uint32_t cellHeight = recipe.resolution / recipe.panelRows;
    const std::uint32_t seamWidth = std::max(1U, recipe.resolution / 96U);
    const std::uint32_t bevelWidth = std::max(4U, recipe.resolution / 32U);
    const std::uint32_t boltInset = std::max(6U, recipe.resolution / 20U);
    const std::uint32_t boltRadius = std::max(2U, recipe.resolution / 64U);
    const wkc::core::Vec3 joint = recipe.surface == RoomSurface::Wall
        ? wkc::core::Vec3{0.14F, 0.14F, 0.13F}
        : wkc::core::Vec3{0.07F, 0.09F, 0.10F};
    const wkc::core::Vec3 exposedSteel = recipe.surface == RoomSurface::Wall
        ? wkc::core::Vec3{0.51F, 0.49F, 0.45F}
        : wkc::core::Vec3{0.35F, 0.38F, 0.38F};
    const std::array boltCenters{
        std::pair{boltInset, boltInset},
        std::pair{cellWidth - 1U - boltInset, boltInset},
        std::pair{boltInset, cellHeight - 1U - boltInset},
        std::pair{cellWidth - 1U - boltInset, cellHeight - 1U - boltInset},
    };

    for (std::uint32_t y = 0; y < recipe.resolution; ++y) {
        for (std::uint32_t x = 0; x < recipe.resolution; ++x) {
            const std::uint32_t panelX = x % cellWidth;
            const std::uint32_t panelY = y % cellHeight;
            const std::uint32_t edge = std::min({panelX, panelY,
                cellWidth - 1U - panelX, cellHeight - 1U - panelY});
            const std::uint64_t panelId
                = (static_cast<std::uint64_t>(y / cellHeight) << 32U)
                | (x / cellWidth);
            const std::uint64_t pixelId = (static_cast<std::uint64_t>(y) << 32U) | x;
            const float panelVariation = 0.10F + 0.18F * unitNoise(
                wkc::core::hashCombine(recipe.seed, panelId));
            wkc::core::Vec3 color = mix(
                recipe.baseColor, recipe.secondaryColor, panelVariation);
            const float brushing = (smoothNoise(recipe.seed ^ 0x544543485F425255ULL,
                x, y, 13U) - 0.5F) * 0.025F;
            const float grain = (unitNoise(wkc::core::hashCombine(
                recipe.seed ^ 0x544543485F47524EULL, pixelId)) - 0.5F) * 0.014F;
            color = add(color, brushing + grain);

            if (edge <= seamWidth) {
                color = joint;
            } else if (edge <= bevelWidth) {
                const float bevel = static_cast<float>(bevelWidth - edge + 1U)
                    / static_cast<float>(bevelWidth - seamWidth);
                color = mix(color, exposedSteel, bevel * 0.24F);
            }

            const float wearNoise = unitNoise(wkc::core::hashCombine(
                recipe.seed ^ 0x544543485F574541ULL, pixelId));
            if (edge > seamWidth && edge <= bevelWidth + 7U
                && wearNoise < recipe.wearAmount * 0.14F) {
                color = mix(color, exposedSteel, 0.48F);
            }
            const float grime = smoothNoise(recipe.seed ^ 0x544543485F444952ULL,
                x, y, 19U);
            const float dirtThreshold = recipe.dirtAmount
                * (edge <= bevelWidth + 4U ? 0.45F : 0.08F);
            if (grime < dirtThreshold) {
                color = mix(color, joint, recipe.surface == RoomSurface::Wall
                    ? 0.17F : 0.25F);
            }
            if (recipe.surface == RoomSurface::Floor && edge > bevelWidth + 4U
                && smoothNoise(recipe.seed ^ 0x544543485F4F494CULL,
                    x, y, 23U) < recipe.oilAmount * 0.45F) {
                color = mix(color, joint, 0.17F);
            }

            for (const auto& [boltX, boltY] : boltCenters) {
                const int dx = static_cast<int>(panelX) - static_cast<int>(boltX);
                const int dy = static_cast<int>(panelY) - static_cast<int>(boltY);
                const auto distanceSquared = static_cast<std::uint32_t>(
                    dx * dx + dy * dy);
                if (distanceSquared <= boltRadius * boltRadius) {
                    color = distanceSquared <= (boltRadius / 2U) * (boltRadius / 2U)
                        ? joint : exposedSteel;
                    break;
                }
            }
            writePixel(image, x, y, color);
        }
    }
    return image;
}

SecurityRoomMaterialSet generateSecurityRoomMaterials(const wkc::core::Seed roomSeed)
{
    SecurityRoomMaterialSet materials;
    materials.roomSeed = roomSeed;
    materials.condition = selectSecurityCondition(roomSeed);
    materials.surfaces = {
        makeSecuritySurfaceRecipe(roomSeed, materials.condition, RoomSurface::Wall),
        makeSecuritySurfaceRecipe(roomSeed, materials.condition, RoomSurface::Floor),
        makeSecuritySurfaceRecipe(roomSeed, materials.condition, RoomSurface::Ceiling),
    };
    materials.materialHash = securityRoomMaterialHash(materials);
    return materials;
}

SecurityMaterialValidationResult validateSecuritySurfaceRecipe(
    const SecuritySurfaceRecipe& recipe)
{
    SecurityMaterialValidationResult result;
    if (recipe.surface >= RoomSurface::Count) {
        result.errors.push_back(SecurityMaterialValidationError::InvalidSurface);
    }
    if (recipe.family != MaterialFamily::PaintedSteel
        && recipe.family != MaterialFamily::BareSteel
        && recipe.family != MaterialFamily::Aluminum) {
        result.errors.push_back(SecurityMaterialValidationError::InvalidFamily);
    }
    if (!isUnitColor(recipe.baseColor) || !isUnitColor(recipe.secondaryColor)
        || !isUnitColor(recipe.accentColor)) {
        result.errors.push_back(SecurityMaterialValidationError::InvalidColor);
    }
    if (recipe.panelColumns < 2U || recipe.panelColumns > 6U
        || recipe.panelRows < 2U || recipe.panelRows > 4U) {
        result.errors.push_back(SecurityMaterialValidationError::InvalidPanelGrid);
    }
    if (!isUnitValue(recipe.wearAmount) || !isUnitValue(recipe.monitorAmount)
        || !isUnitValue(recipe.statusAmount) || !isUnitValue(recipe.accessAmount)
        || !isUnitValue(recipe.alertAmount) || !isUnitValue(recipe.emissiveAmount)) {
        result.errors.push_back(SecurityMaterialValidationError::InvalidParameter);
    }
    const bool powerOfTwo = recipe.resolution != 0U
        && (recipe.resolution & (recipe.resolution - 1U)) == 0U;
    if (!powerOfTwo || recipe.resolution < 32U || recipe.resolution > 256U) {
        result.errors.push_back(SecurityMaterialValidationError::InvalidResolution);
    }
    if (recipe.surface < RoomSurface::Count) {
        const bool isWall = recipe.surface == RoomSurface::Wall;
        const bool isFloor = recipe.surface == RoomSurface::Floor;
        const bool isCeiling = recipe.surface == RoomSurface::Ceiling;
        const bool floorGrid = recipe.panelColumns == recipe.panelRows
            && (recipe.panelColumns == 2U || recipe.panelColumns == 4U);
        if ((isWall && (recipe.family != MaterialFamily::PaintedSteel
                           || recipe.panelRows != 2U
                           || recipe.monitorAmount <= 0.0F
                           || recipe.statusAmount <= 0.0F
                           || recipe.accessAmount != 0.0F))
            || (isFloor && (recipe.family != MaterialFamily::BareSteel
                               || !floorGrid
                               || recipe.monitorAmount != 0.0F
                               || recipe.statusAmount != 0.0F
                               || recipe.accessAmount <= 0.0F
                               || recipe.alertAmount != 0.0F))
            || (isCeiling && (recipe.family != MaterialFamily::Aluminum
                                 || recipe.panelColumns != 2U
                                 || recipe.monitorAmount != 0.0F
                                 || recipe.statusAmount <= 0.0F
                                 || recipe.accessAmount != 0.0F
                                 || recipe.alertAmount != 0.0F
                                 || recipe.emissiveAmount <= 0.0F))
            || (!isCeiling && recipe.emissiveAmount != 0.0F)) {
            result.errors.push_back(SecurityMaterialValidationError::InvalidSurfaceRule);
        }
    }
    return result;
}

SecurityMaterialValidationResult validateSecurityRoomMaterials(
    const SecurityRoomMaterialSet& materials)
{
    SecurityMaterialValidationResult result;
    if (materials.condition >= SecurityCondition::Count) {
        result.errors.push_back(SecurityMaterialValidationError::InvalidCondition);
    }
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        const SecuritySurfaceRecipe& recipe = materials.surfaces[index];
        const auto surfaceValidation = validateSecuritySurfaceRecipe(recipe);
        result.errors.insert(
            result.errors.end(), surfaceValidation.errors.begin(), surfaceValidation.errors.end());
        if (surfaceIndex(recipe.surface) != index) {
            result.errors.push_back(SecurityMaterialValidationError::InvalidSurfaceOrder);
        }
        const wkc::core::Seed expectedSeed = wkc::core::deriveSeed(
            materials.roomSeed, gdp::game::SeedDomain::RoomMaterial, index + 601U);
        if (recipe.seed != expectedSeed) {
            result.errors.push_back(SecurityMaterialValidationError::InvalidSurfaceOrder);
        }
    }
    if (materials.materialHash == 0U
        || materials.materialHash != securityRoomMaterialHash(materials)) {
        result.errors.push_back(SecurityMaterialValidationError::InvalidMaterialHash);
    }
    return result;
}

std::uint64_t securitySurfaceRecipeHash(const SecuritySurfaceRecipe& recipe) noexcept
{
    std::uint64_t hash = wkc::core::hashCombine(0x5345435F53555246ULL, recipe.version);
    hash = wkc::core::hashCombine(hash, recipe.seed);
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.surface));
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.family));
    hash = addVec3(hash, recipe.baseColor);
    hash = addVec3(hash, recipe.secondaryColor);
    hash = addVec3(hash, recipe.accentColor);
    hash = wkc::core::hashCombine(hash, recipe.panelColumns);
    hash = wkc::core::hashCombine(hash, recipe.panelRows);
    hash = addFloat(hash, recipe.wearAmount);
    hash = addFloat(hash, recipe.monitorAmount);
    hash = addFloat(hash, recipe.statusAmount);
    hash = addFloat(hash, recipe.accessAmount);
    hash = addFloat(hash, recipe.alertAmount);
    hash = addFloat(hash, recipe.emissiveAmount);
    return wkc::core::hashCombine(hash, recipe.resolution);
}

std::uint64_t securityRoomMaterialHash(const SecurityRoomMaterialSet& materials) noexcept
{
    std::uint64_t hash = wkc::core::hashCombine(
        0x5345435F524F4F4DULL, materials.roomSeed);
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(materials.condition));
    for (const SecuritySurfaceRecipe& recipe : materials.surfaces) {
        hash = wkc::core::hashCombine(hash, securitySurfaceRecipeHash(recipe));
    }
    return hash;
}

std::optional<wkc::render::ImageData> generateSecuritySurfaceAlbedo(
    const SecuritySurfaceRecipe& recipe)
{
    if (!validateSecuritySurfaceRecipe(recipe).isValid()) {
        return std::nullopt;
    }

    wkc::render::ImageData image;
    image.width = static_cast<int>(recipe.resolution);
    image.height = static_cast<int>(recipe.resolution);
    image.rgba.resize(static_cast<std::size_t>(recipe.resolution)
        * static_cast<std::size_t>(recipe.resolution) * 4U);
    const std::uint32_t cellWidth = recipe.resolution / recipe.panelColumns;
    const std::uint32_t cellHeight = recipe.resolution / recipe.panelRows;

    for (std::uint32_t y = 0; y < recipe.resolution; ++y) {
        for (std::uint32_t x = 0; x < recipe.resolution; ++x) {
            const std::uint64_t pixelId = (static_cast<std::uint64_t>(y) << 32U) | x;
            const std::uint64_t panelId = (static_cast<std::uint64_t>(y / cellHeight) << 32U)
                | (x / cellWidth);
            const float panelBlend = 0.06F + unitNoise(
                wkc::core::hashCombine(recipe.seed, panelId)) * 0.10F;
            wkc::core::Vec3 color = mix(
                recipe.baseColor, recipe.secondaryColor, panelBlend);
            const std::uint32_t panelX = x % cellWidth;
            const std::uint32_t panelY = y % cellHeight;
            const bool seam = panelX <= 1U || panelY <= 1U;
            if (seam) {
                color = multiply(color, 0.56F);
            }
            const float wearNoise = unitNoise(wkc::core::hashCombine(
                recipe.seed ^ 0x5EC017ULL, pixelId));
            if (!seam && wearNoise < recipe.wearAmount * 0.015F) {
                color = mix(color, {0.35F, 0.37F, 0.38F}, 0.42F);
            }

            if (recipe.surface == RoomSurface::Wall) {
                const bool console = x >= 35U && x < 60U && y >= 12U && y < 45U;
                if (console) {
                    const bool frame = x < 38U || x >= 57U || y < 15U || y >= 42U;
                    color = frame ? wkc::core::Vec3{0.33F, 0.36F, 0.38F}
                                  : wkc::core::Vec3{0.08F, 0.10F, 0.11F};
                }
                const bool screen = x >= 40U && x < 56U && y >= 18U && y < 31U;
                if (screen) {
                    const float screenState = unitNoise(wkc::core::hashCombine(
                        recipe.seed ^ 0x5C2EEAULL, 0));
                    if (recipe.alertAmount > 0.15F) {
                        color = {0.35F, 0.07F, 0.07F};
                        if (y == 24U || y == 25U) {
                            color = {0.66F, 0.20F, 0.20F};
                        }
                    } else if (screenState < 0.55F) {
                        color = {0.06F, 0.09F, 0.10F};
                    } else if (screenState < 0.80F) {
                        color = {0.07F, 0.16F, 0.15F};
                    } else if (screenState < 0.95F) {
                        color = (x + y) % 5U == 0U
                            ? wkc::core::Vec3{0.16F, 0.32F, 0.38F}
                            : wkc::core::Vec3{0.06F, 0.12F, 0.15F};
                    } else {
                        color = {0.49F, 0.31F, 0.08F};
                    }
                }
                for (std::uint32_t indicator = 0; indicator < 4U; ++indicator) {
                    const std::uint32_t indicatorX = 39U + indicator * 4U;
                    if (x >= indicatorX && x < indicatorX + 2U
                        && y >= 35U && y < 37U) {
                        color = recipe.alertAmount > 0.15F && indicator == 0U
                            ? wkc::core::Vec3{0.66F, 0.20F, 0.20F}
                            : wkc::core::Vec3{0.23F, 0.61F, 0.28F};
                    }
                }
                if (x >= 47U && x < 49U && y >= 45U) {
                    color = {0.11F, 0.13F, 0.14F};
                }
            } else if (recipe.surface == RoomSurface::Floor) {
                const bool lock = panelX >= 3U && panelX <= 4U
                    && panelY >= 3U && panelY <= 4U;
                if (lock) {
                    color = {0.43F, 0.45F, 0.46F};
                }
                constexpr std::uint32_t hatchMin = 24U;
                constexpr std::uint32_t hatchMax = 40U;
                const bool hatch = ((x == hatchMin || x == hatchMax)
                        && y >= hatchMin && y <= hatchMax)
                    || ((y == hatchMin || y == hatchMax)
                        && x >= hatchMin && x <= hatchMax);
                if (hatch) {
                    color = recipe.accentColor;
                }
                const int dx = static_cast<int>(x) - 48;
                const int dy = static_cast<int>(y) - 48;
                const int distanceSquared = (dx * dx) + (dy * dy);
                if (distanceSquared >= 56 && distanceSquared <= 72 && y <= 48U) {
                    color = multiply(recipe.secondaryColor, 0.72F);
                }
            } else {
                const std::uint32_t panelCount
                    = static_cast<std::uint32_t>(recipe.panelColumns)
                    * static_cast<std::uint32_t>(recipe.panelRows);
                const std::uint32_t panelLinear = (y / cellHeight) * recipe.panelColumns
                    + (x / cellWidth);
                const std::uint32_t sensorPanel
                    = static_cast<std::uint32_t>(recipe.seed % panelCount);
                if (panelLinear == sensorPanel) {
                    const int dx = static_cast<int>(panelX) - 3;
                    const int dy = static_cast<int>(panelY) - 3;
                    if ((dx * dx) + (dy * dy) <= 4) {
                        color = {0.08F, 0.10F, 0.11F};
                    }
                }
            }
            writePixel(image, x, y, color);
        }
    }
    return image;
}

CentralHubRoomMaterialSet generateCentralHubRoomMaterials(const wkc::core::Seed roomSeed)
{
    const CentralHubSelection selection = selectCentralHubState(roomSeed);
    CentralHubRoomMaterialSet materials;
    materials.roomSeed = roomSeed;
    materials.condition = selection.condition;
    materials.routeAxis = selection.routeAxis;
    materials.surfaces = {
        makeCentralHubSurfaceRecipe(
            roomSeed, materials.condition, materials.routeAxis, RoomSurface::Wall),
        makeCentralHubSurfaceRecipe(
            roomSeed, materials.condition, materials.routeAxis, RoomSurface::Floor),
        makeCentralHubSurfaceRecipe(
            roomSeed, materials.condition, materials.routeAxis, RoomSurface::Ceiling),
    };
    materials.materialHash = centralHubRoomMaterialHash(materials);
    return materials;
}

CentralHubMaterialValidationResult validateCentralHubSurfaceRecipe(
    const CentralHubSurfaceRecipe& recipe)
{
    CentralHubMaterialValidationResult result;
    if (recipe.surface >= RoomSurface::Count) {
        result.errors.push_back(CentralHubMaterialValidationError::InvalidSurface);
    }
    if (recipe.routeAxis >= CentralHubRouteAxis::Count) {
        result.errors.push_back(CentralHubMaterialValidationError::InvalidRouteAxis);
    }
    if (recipe.family != MaterialFamily::PaintedSteel
        && recipe.family != MaterialFamily::BareSteel) {
        result.errors.push_back(CentralHubMaterialValidationError::InvalidFamily);
    }
    if (!isUnitColor(recipe.baseColor) || !isUnitColor(recipe.secondaryColor)
        || !isUnitColor(recipe.accentColor)) {
        result.errors.push_back(CentralHubMaterialValidationError::InvalidColor);
    }
    if (recipe.panelColumns < 1U || recipe.panelColumns > 3U
        || recipe.panelRows < 2U || recipe.panelRows > 4U) {
        result.errors.push_back(CentralHubMaterialValidationError::InvalidPanelGrid);
    }
    if (!isUnitValue(recipe.wearAmount) || !isUnitValue(recipe.structureAmount)
        || !isUnitValue(recipe.navigationAmount) || !isUnitValue(recipe.focalAmount)
        || !isUnitValue(recipe.emissiveAmount)) {
        result.errors.push_back(CentralHubMaterialValidationError::InvalidParameter);
    }
    const bool powerOfTwo = recipe.resolution != 0U
        && (recipe.resolution & (recipe.resolution - 1U)) == 0U;
    if (!powerOfTwo || recipe.resolution < 32U || recipe.resolution > 256U) {
        result.errors.push_back(CentralHubMaterialValidationError::InvalidResolution);
    }
    if (recipe.surface < RoomSurface::Count) {
        const bool isWall = recipe.surface == RoomSurface::Wall;
        const bool isFloor = recipe.surface == RoomSurface::Floor;
        const bool isCeiling = recipe.surface == RoomSurface::Ceiling;
        const bool floorGrid = recipe.panelColumns == 2U
            && (recipe.panelRows == 2U || recipe.panelRows == 4U);
        const bool cyanOrNeutralAccent = recipe.accentColor.z >= recipe.accentColor.x
            && recipe.accentColor.z >= recipe.accentColor.y * 0.95F;
        if (!cyanOrNeutralAccent
            || (isWall && (recipe.family != MaterialFamily::PaintedSteel
                              || recipe.panelRows != 2U
                              || recipe.structureAmount <= 0.0F
                              || recipe.navigationAmount <= 0.0F
                              || recipe.focalAmount <= 0.0F
                              || recipe.emissiveAmount != 0.0F))
            || (isFloor && (recipe.family != MaterialFamily::BareSteel
                               || !floorGrid
                               || recipe.structureAmount != 0.0F
                               || recipe.navigationAmount <= 0.0F
                               || recipe.focalAmount != 0.0F
                               || recipe.emissiveAmount != 0.0F))
            || (isCeiling && (recipe.family != MaterialFamily::BareSteel
                                 || recipe.panelColumns != 2U
                                 || recipe.panelRows != 2U
                                 || recipe.structureAmount <= 0.0F
                                 || recipe.navigationAmount != 0.0F
                                 || recipe.focalAmount != 0.0F
                                 || recipe.emissiveAmount <= 0.0F))) {
            result.errors.push_back(CentralHubMaterialValidationError::InvalidSurfaceRule);
        }
    }
    return result;
}

CentralHubMaterialValidationResult validateCentralHubRoomMaterials(
    const CentralHubRoomMaterialSet& materials)
{
    CentralHubMaterialValidationResult result;
    if (materials.condition >= CentralHubCondition::Count) {
        result.errors.push_back(CentralHubMaterialValidationError::InvalidCondition);
    }
    if (materials.routeAxis >= CentralHubRouteAxis::Count) {
        result.errors.push_back(CentralHubMaterialValidationError::InvalidRouteAxis);
    }
    for (std::size_t index = 0; index < materials.surfaces.size(); ++index) {
        const CentralHubSurfaceRecipe& recipe = materials.surfaces[index];
        const auto surfaceValidation = validateCentralHubSurfaceRecipe(recipe);
        result.errors.insert(
            result.errors.end(), surfaceValidation.errors.begin(), surfaceValidation.errors.end());
        if (surfaceIndex(recipe.surface) != index || recipe.routeAxis != materials.routeAxis) {
            result.errors.push_back(CentralHubMaterialValidationError::InvalidSurfaceOrder);
        }
        const wkc::core::Seed expectedSeed = wkc::core::deriveSeed(
            materials.roomSeed, gdp::game::SeedDomain::RoomMaterial, index + 701U);
        if (recipe.seed != expectedSeed) {
            result.errors.push_back(CentralHubMaterialValidationError::InvalidSurfaceOrder);
        }
    }
    if (materials.materialHash == 0U
        || materials.materialHash != centralHubRoomMaterialHash(materials)) {
        result.errors.push_back(CentralHubMaterialValidationError::InvalidMaterialHash);
    }
    return result;
}

std::uint64_t centralHubSurfaceRecipeHash(const CentralHubSurfaceRecipe& recipe) noexcept
{
    std::uint64_t hash = wkc::core::hashCombine(0x4855425F53555246ULL, recipe.version);
    hash = wkc::core::hashCombine(hash, recipe.seed);
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.surface));
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.family));
    hash = addVec3(hash, recipe.baseColor);
    hash = addVec3(hash, recipe.secondaryColor);
    hash = addVec3(hash, recipe.accentColor);
    hash = wkc::core::hashCombine(hash, recipe.panelColumns);
    hash = wkc::core::hashCombine(hash, recipe.panelRows);
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.routeAxis));
    hash = addFloat(hash, recipe.wearAmount);
    hash = addFloat(hash, recipe.structureAmount);
    hash = addFloat(hash, recipe.navigationAmount);
    hash = addFloat(hash, recipe.focalAmount);
    hash = addFloat(hash, recipe.emissiveAmount);
    return wkc::core::hashCombine(hash, recipe.resolution);
}

std::uint64_t centralHubRoomMaterialHash(const CentralHubRoomMaterialSet& materials) noexcept
{
    std::uint64_t hash = wkc::core::hashCombine(
        0x4855425F524F4F4DULL, materials.roomSeed);
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(materials.condition));
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(materials.routeAxis));
    for (const CentralHubSurfaceRecipe& recipe : materials.surfaces) {
        hash = wkc::core::hashCombine(hash, centralHubSurfaceRecipeHash(recipe));
    }
    return hash;
}

std::optional<wkc::render::ImageData> generateCentralHubSurfaceAlbedo(
    const CentralHubSurfaceRecipe& recipe)
{
    if (!validateCentralHubSurfaceRecipe(recipe).isValid()) {
        return std::nullopt;
    }

    wkc::render::ImageData image;
    image.width = static_cast<int>(recipe.resolution);
    image.height = static_cast<int>(recipe.resolution);
    image.rgba.resize(static_cast<std::size_t>(recipe.resolution)
        * static_cast<std::size_t>(recipe.resolution) * 4U);
    const std::uint32_t cellWidth = recipe.resolution / recipe.panelColumns;
    const std::uint32_t cellHeight = recipe.resolution / recipe.panelRows;

    for (std::uint32_t y = 0; y < recipe.resolution; ++y) {
        for (std::uint32_t x = 0; x < recipe.resolution; ++x) {
            const std::uint64_t pixelId = (static_cast<std::uint64_t>(y) << 32U) | x;
            const std::uint64_t panelId = (static_cast<std::uint64_t>(y / cellHeight) << 32U)
                | (x / cellWidth);
            const float panelBlend = 0.04F + unitNoise(
                wkc::core::hashCombine(recipe.seed, panelId)) * 0.08F;
            wkc::core::Vec3 color = mix(
                recipe.baseColor, recipe.secondaryColor, panelBlend);
            const std::uint32_t panelX = x % cellWidth;
            const std::uint32_t panelY = y % cellHeight;
            const bool seam = panelX <= 1U || panelY <= 1U;
            if (seam) {
                color = multiply(color, 0.55F);
            }
            const float wearNoise = unitNoise(wkc::core::hashCombine(
                recipe.seed ^ 0x48554217ULL, pixelId));
            if (!seam && wearNoise < recipe.wearAmount * 0.012F) {
                color = mix(color, {0.35F, 0.38F, 0.39F}, 0.32F);
            }

            if (recipe.surface == RoomSurface::Wall) {
                const std::uint32_t signMinX = (recipe.seed & 1U) == 0U ? 8U : 42U;
                const bool sign = x >= signMinX && x < signMinX + 14U
                    && y >= 20U && y < 25U;
                if (sign) {
                    const bool signFrame = x == signMinX || x == signMinX + 13U
                        || y == 20U || y == 24U;
                    color = signFrame ? multiply(recipe.secondaryColor, 0.55F)
                                      : recipe.accentColor;
                }
            } else if (recipe.surface == RoomSurface::Floor) {
                const bool horizontalRoute
                    = recipe.routeAxis == CentralHubRouteAxis::Horizontal
                    || recipe.routeAxis == CentralHubRouteAxis::Cross;
                const bool verticalRoute = recipe.routeAxis == CentralHubRouteAxis::Vertical
                    || recipe.routeAxis == CentralHubRouteAxis::Cross;
                if ((horizontalRoute && y >= 31U && y <= 32U)
                    || (verticalRoute && x >= 31U && x <= 32U)) {
                    color = recipe.accentColor;
                }
            }
            writePixel(image, x, y, color);
        }
    }
    return image;
}

} // namespace gdp::game::visual
