#include "procedural/RoomMaterialGenerator.hpp"

#include "core/Hash.hpp"
#include "core/Random.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

namespace gdp::procedural {
namespace {

constexpr std::size_t surfaceIndex(const IndustrialSurface surface) noexcept
{
    return static_cast<std::size_t>(surface);
}

bool isUnitValue(const float value) noexcept
{
    return std::isfinite(value) && value >= 0.0F && value <= 1.0F;
}

bool isUnitColor(const gdp::core::Vec3 color) noexcept
{
    return isUnitValue(color.x) && isUnitValue(color.y) && isUnitValue(color.z);
}

float unitNoise(const std::uint64_t value) noexcept
{
    return static_cast<float>(gdp::core::mix64(value) & 0xFFFFU) / 65'535.0F;
}

std::uint8_t channelToByte(const float value) noexcept
{
    return static_cast<std::uint8_t>(
        std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
}

gdp::core::Vec3 multiply(const gdp::core::Vec3 color, const float value) noexcept
{
    return {color.x * value, color.y * value, color.z * value};
}

gdp::core::Vec3 add(const gdp::core::Vec3 color, const float value) noexcept
{
    return {color.x + value, color.y + value, color.z + value};
}

gdp::core::Vec3 mix(
    const gdp::core::Vec3 first,
    const gdp::core::Vec3 second,
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
    const gdp::core::Seed seed,
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
        return unitNoise(gdp::core::hashCombine(seed, id));
    };
    const float top = scalarMix(sample(gridX, gridY), sample(gridX + 1U, gridY), blendX);
    const float bottom = scalarMix(
        sample(gridX, gridY + 1U), sample(gridX + 1U, gridY + 1U), blendX);
    return scalarMix(top, bottom, blendY);
}

struct IndustrialWallSample {
    gdp::core::Vec3 albedo{};
    float height{0.5F};
    float roughness{0.62F};
    float metalness{0.06F};
    float ambientOcclusion{1.0F};
    bool exposed{false};
};

IndustrialWallSample industrialWallSample(
    const IndustrialSurfaceRecipe& recipe,
    const std::uint32_t x,
    const std::uint32_t y,
    const std::uint32_t cellWidth,
    const std::uint32_t cellHeight,
    const std::uint32_t stripeHeight) noexcept
{
    constexpr gdp::core::Vec3 jointColor{21.0F / 255.0F, 26.0F / 255.0F, 28.0F / 255.0F};
    constexpr gdp::core::Vec3 exposedSteel{
        139.0F / 255.0F, 144.0F / 255.0F, 145.0F / 255.0F};
    constexpr gdp::core::Vec3 grimeColor{73.0F / 255.0F, 56.0F / 255.0F, 43.0F / 255.0F};
    constexpr gdp::core::Vec3 stripeBlack{0.055F, 0.060F, 0.065F};

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
        gdp::core::hashCombine(recipe.seed, panelId)) * 0.10F;
    sample.albedo = mix(recipe.baseColor, recipe.secondaryColor, panelBlend);
    const float macro = (smoothNoise(recipe.seed ^ 0x4D414352U, x, y, 128U) - 0.5F)
        * 0.055F;
    const float orangePeel = (smoothNoise(recipe.seed ^ 0x5041494EU, x, y, 12U) - 0.5F)
        * 0.024F;
    const std::uint32_t brushBand = static_cast<std::uint32_t>(
        (static_cast<std::uint64_t>(x) + (recipe.seed & 15U)) % 17U);
    const float brush = (static_cast<float>(brushBand) - 8.0F) * 0.00035F;
    const float grain = (unitNoise(gdp::core::hashCombine(recipe.seed, pixelId)) - 0.5F)
        * 0.009F;
    sample.albedo = add(sample.albedo, macro + orangePeel + brush + grain);
    sample.height = 0.50F + orangePeel * 0.42F + grain * 0.12F;
    sample.roughness = 0.61F + orangePeel * 1.4F;

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

    const float scratchNoise = unitNoise(gdp::core::hashCombine(
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
    return gdp::core::hashCombine(hash, std::bit_cast<std::uint32_t>(value));
}

std::uint64_t addVec3(std::uint64_t hash, const gdp::core::Vec3 value) noexcept
{
    hash = addFloat(hash, value.x);
    hash = addFloat(hash, value.y);
    return addFloat(hash, value.z);
}

IndustrialCondition selectCondition(const gdp::core::Seed roomSeed) noexcept
{
    gdp::core::RandomStream random{gdp::core::deriveSeed(
        roomSeed, gdp::core::SeedDomain::RoomMaterial, 0)};
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
    gdp::core::RandomStream& random) noexcept
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
    const gdp::core::Seed roomSeed,
    const IndustrialCondition condition,
    const IndustrialSurface surface)
{
    const std::uint64_t localId = static_cast<std::uint64_t>(surface) + 1U;
    const gdp::core::Seed seed = gdp::core::deriveSeed(
        roomSeed, gdp::core::SeedDomain::RoomMaterial, localId);
    gdp::core::RandomStream random{seed};
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
        recipe.family = MaterialFamily::Aluminum;
        recipe.baseColor = multiply({0.17F, 0.18F, 0.19F}, tone);
        recipe.secondaryColor = multiply({0.11F, 0.12F, 0.13F}, tone);
        recipe.accentColor = {0.88F, 0.87F, 0.80F};
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

StorageCondition selectStorageCondition(const gdp::core::Seed roomSeed) noexcept
{
    gdp::core::RandomStream random{gdp::core::deriveSeed(
        roomSeed, gdp::core::SeedDomain::RoomMaterial, 100)};
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
    gdp::core::RandomStream& random) noexcept
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
    const gdp::core::Seed roomSeed,
    const StorageCondition condition,
    const RoomSurface surface)
{
    const std::uint64_t localId = static_cast<std::uint64_t>(surface) + 101U;
    const gdp::core::Seed seed = gdp::core::deriveSeed(
        roomSeed, gdp::core::SeedDomain::RoomMaterial, localId);
    gdp::core::RandomStream random{seed};
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

CombatCondition selectCombatCondition(const gdp::core::Seed roomSeed) noexcept
{
    gdp::core::RandomStream random{gdp::core::deriveSeed(
        roomSeed, gdp::core::SeedDomain::RoomMaterial, 200)};
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
    gdp::core::RandomStream& random) noexcept
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
    const gdp::core::Seed roomSeed,
    const CombatCondition condition,
    const RoomSurface surface)
{
    const std::uint64_t localId = static_cast<std::uint64_t>(surface) + 201U;
    const gdp::core::Seed seed = gdp::core::deriveSeed(
        roomSeed, gdp::core::SeedDomain::RoomMaterial, localId);
    gdp::core::RandomStream random{seed};
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

CafeteriaCondition selectCafeteriaCondition(const gdp::core::Seed roomSeed) noexcept
{
    gdp::core::RandomStream random{gdp::core::deriveSeed(
        roomSeed, gdp::core::SeedDomain::RoomMaterial, 300)};
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
    gdp::core::RandomStream& random) noexcept
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
    const gdp::core::Seed roomSeed,
    const CafeteriaCondition condition,
    const RoomSurface surface)
{
    const std::uint64_t localId = static_cast<std::uint64_t>(surface) + 301U;
    const gdp::core::Seed seed = gdp::core::deriveSeed(
        roomSeed, gdp::core::SeedDomain::RoomMaterial, localId);
    gdp::core::RandomStream random{seed};
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

LaboratoryCondition selectLaboratoryCondition(const gdp::core::Seed roomSeed) noexcept
{
    gdp::core::RandomStream random{gdp::core::deriveSeed(
        roomSeed, gdp::core::SeedDomain::RoomMaterial, 400)};
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
    gdp::core::RandomStream& random) noexcept
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
    const gdp::core::Seed roomSeed,
    const LaboratoryCondition condition,
    const RoomSurface surface)
{
    const std::uint64_t localId = static_cast<std::uint64_t>(surface) + 401U;
    const gdp::core::Seed seed = gdp::core::deriveSeed(
        roomSeed, gdp::core::SeedDomain::RoomMaterial, localId);
    gdp::core::RandomStream random{seed};
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

TechnicalCondition selectTechnicalCondition(const gdp::core::Seed roomSeed) noexcept
{
    gdp::core::RandomStream random{gdp::core::deriveSeed(
        roomSeed, gdp::core::SeedDomain::RoomMaterial, 500)};
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
    gdp::core::RandomStream& random) noexcept
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
    const gdp::core::Seed roomSeed,
    const TechnicalCondition condition,
    const RoomSurface surface)
{
    const std::uint64_t localId = static_cast<std::uint64_t>(surface) + 501U;
    const gdp::core::Seed seed = gdp::core::deriveSeed(
        roomSeed, gdp::core::SeedDomain::RoomMaterial, localId);
    gdp::core::RandomStream random{seed};
    const TechnicalConditionAmounts amounts = technicalConditionAmounts(condition, random);

    TechnicalSurfaceRecipe recipe;
    recipe.seed = seed;
    recipe.surface = surface;
    recipe.wearAmount = amounts.wear;
    recipe.dirtAmount = amounts.dirt;
    recipe.oilAmount = amounts.oil;
    recipe.resolution = 64;

    const float tone = random.rangeFloat(0.94F, 1.05F);
    switch (surface) {
    case RoomSurface::Wall:
        recipe.family = MaterialFamily::PaintedSteel;
        recipe.baseColor = multiply({0.23F, 0.23F, 0.22F}, tone);
        recipe.secondaryColor = multiply({0.13F, 0.14F, 0.13F}, tone);
        recipe.accentColor = {0.71F, 0.48F, 0.0F};
        recipe.panelColumns = 2;
        recipe.panelRows = 2;
        recipe.pipeAmount = random.rangeFloat(0.30F, 0.55F);
        recipe.oilAmount = 0.0F;
        recipe.mechanicalAmount = random.rangeFloat(0.12F, 0.28F);
        break;
    case RoomSurface::Floor:
        recipe.family = MaterialFamily::BareSteel;
        recipe.baseColor = multiply({0.20F, 0.21F, 0.20F}, tone);
        recipe.secondaryColor = multiply({0.12F, 0.13F, 0.12F}, tone);
        recipe.accentColor = {0.31F, 0.33F, 0.31F};
        recipe.panelColumns = 2;
        recipe.panelRows = 2;
        recipe.pipeAmount = 0.0F;
        recipe.mechanicalAmount = random.rangeFloat(0.10F, 0.24F);
        break;
    case RoomSurface::Ceiling:
        recipe.family = MaterialFamily::BareSteel;
        recipe.baseColor = multiply({0.18F, 0.19F, 0.18F}, tone);
        recipe.secondaryColor = multiply({0.10F, 0.11F, 0.10F}, tone);
        recipe.accentColor = {0.74F, 0.69F, 0.55F};
        recipe.panelColumns = 2;
        recipe.panelRows = static_cast<std::uint8_t>(random.rangeInt(2, 3));
        recipe.pipeAmount = random.rangeFloat(0.18F, 0.40F);
        recipe.oilAmount = 0.0F;
        recipe.mechanicalAmount = random.rangeFloat(0.35F, 0.60F);
        recipe.emissiveAmount = random.rangeFloat(0.18F, 0.34F);
        break;
    case RoomSurface::Count:
        break;
    }
    return recipe;
}

SecurityCondition selectSecurityCondition(const gdp::core::Seed roomSeed) noexcept
{
    gdp::core::RandomStream random{gdp::core::deriveSeed(
        roomSeed, gdp::core::SeedDomain::RoomMaterial, 600)};
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
    gdp::core::RandomStream& random) noexcept
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
    const gdp::core::Seed roomSeed,
    const SecurityCondition condition,
    const RoomSurface surface)
{
    const std::uint64_t localId = static_cast<std::uint64_t>(surface) + 601U;
    const gdp::core::Seed seed = gdp::core::deriveSeed(
        roomSeed, gdp::core::SeedDomain::RoomMaterial, localId);
    gdp::core::RandomStream random{seed};
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

CentralHubSelection selectCentralHubState(const gdp::core::Seed roomSeed) noexcept
{
    gdp::core::RandomStream random{gdp::core::deriveSeed(
        roomSeed, gdp::core::SeedDomain::RoomMaterial, 700)};
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
    gdp::core::RandomStream& random) noexcept
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
    const gdp::core::Seed roomSeed,
    const CentralHubCondition condition,
    const CentralHubRouteAxis routeAxis,
    const RoomSurface surface)
{
    const std::uint64_t localId = static_cast<std::uint64_t>(surface) + 701U;
    const gdp::core::Seed seed = gdp::core::deriveSeed(
        roomSeed, gdp::core::SeedDomain::RoomMaterial, localId);
    gdp::core::RandomStream random{seed};
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
    gdp::render::ImageData& image,
    const std::uint32_t x,
    const std::uint32_t y,
    const gdp::core::Vec3 color)
{
    const std::size_t index = (static_cast<std::size_t>(y)
        * static_cast<std::size_t>(image.width) + x) * 4U;
    image.rgba[index] = channelToByte(color.x);
    image.rgba[index + 1U] = channelToByte(color.y);
    image.rgba[index + 2U] = channelToByte(color.z);
    image.rgba[index + 3U] = 255U;
}

} // namespace

bool IndustrialMaterialValidationResult::isValid() const noexcept
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

IndustrialRoomMaterialSet generateIndustrialRoomMaterials(const gdp::core::Seed roomSeed)
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

IndustrialSurfaceRecipe makeIndustrialWallArchetypeRecipe(
    const IndustrialCondition condition)
{
    IndustrialSurfaceRecipe recipe;
    recipe.version = 2;
    recipe.seed = 0x48445F57414C4C00ULL + static_cast<std::uint64_t>(condition);
    recipe.surface = IndustrialSurface::Wall;
    recipe.family = MaterialFamily::PaintedSteel;
    recipe.baseColor = {0.33F, 0.35F, 0.36F};
    recipe.secondaryColor = {0.20F, 0.21F, 0.22F};
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
    const bool hdWall = recipe.surface == IndustrialSurface::Wall
        && recipe.version == 2U && recipe.resolution == 1024U;
    const bool legacySurface = recipe.surface != IndustrialSurface::Wall
        && recipe.resolution >= 32U && recipe.resolution <= 256U;
    if (!powerOfTwo || (!hdWall && !legacySurface)) {
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
        const gdp::core::Seed expectedSeed = gdp::core::deriveSeed(
            materials.roomSeed, gdp::core::SeedDomain::RoomMaterial, index + 1U);
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
    std::uint64_t hash = gdp::core::hashCombine(0x494E445F53555246ULL, recipe.version);
    hash = gdp::core::hashCombine(hash, recipe.seed);
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.surface));
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.family));
    hash = addVec3(hash, recipe.baseColor);
    hash = addVec3(hash, recipe.secondaryColor);
    hash = addVec3(hash, recipe.accentColor);
    hash = gdp::core::hashCombine(hash, recipe.panelColumns);
    hash = gdp::core::hashCombine(hash, recipe.panelRows);
    hash = addFloat(hash, recipe.wearAmount);
    hash = addFloat(hash, recipe.dirtAmount);
    hash = addFloat(hash, recipe.emissiveAmount);
    hash = gdp::core::hashCombine(hash, recipe.safetyStripe ? 1U : 0U);
    return gdp::core::hashCombine(hash, recipe.resolution);
}

std::uint64_t industrialRoomMaterialHash(
    const IndustrialRoomMaterialSet& materials) noexcept
{
    std::uint64_t hash = gdp::core::hashCombine(
        0x494E445F524F4F4DULL, materials.roomSeed);
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(materials.condition));
    for (const IndustrialSurfaceRecipe& recipe : materials.surfaces) {
        hash = gdp::core::hashCombine(hash, industrialSurfaceRecipeHash(recipe));
    }
    return hash;
}

std::optional<gdp::render::ImageData> generateIndustrialSurfaceAlbedo(
    const IndustrialSurfaceRecipe& recipe)
{
    if (!validateIndustrialSurfaceRecipe(recipe).isValid()) {
        return std::nullopt;
    }

    gdp::render::ImageData image;
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
            const std::uint64_t pixelId = (static_cast<std::uint64_t>(y) << 32U) | x;
            const std::uint64_t panelId = (static_cast<std::uint64_t>(y / cellHeight) << 32U)
                | (x / cellWidth);
            const float panelBlend = 0.12F + (unitNoise(
                gdp::core::hashCombine(recipe.seed, panelId)) * 0.18F);
            gdp::core::Vec3 color = mix(
                recipe.baseColor, recipe.secondaryColor, panelBlend);
            const float grain = (unitNoise(
                gdp::core::hashCombine(recipe.seed, pixelId)) - 0.5F) * 0.035F;
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

            const float dirtNoise = unitNoise(gdp::core::hashCombine(
                pixelId, recipe.seed ^ 0xD17D17U));
            if (dirtNoise < recipe.dirtAmount * (seam ? 0.28F : 0.055F)) {
                color = multiply(color, 0.62F);
            }
            const float wearNoise = unitNoise(gdp::core::hashCombine(
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
    for (gdp::render::ImageData* const image
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
            const gdp::core::Vec3 normal = gdp::core::normalized({
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

StorageRoomMaterialSet generateStorageRoomMaterials(const gdp::core::Seed roomSeed)
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
        const gdp::core::Seed expectedSeed = gdp::core::deriveSeed(
            materials.roomSeed, gdp::core::SeedDomain::RoomMaterial, index + 101U);
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
    std::uint64_t hash = gdp::core::hashCombine(0x53544F5F53555246ULL, recipe.version);
    hash = gdp::core::hashCombine(hash, recipe.seed);
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.surface));
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.family));
    hash = addVec3(hash, recipe.baseColor);
    hash = addVec3(hash, recipe.secondaryColor);
    hash = addVec3(hash, recipe.accentColor);
    hash = gdp::core::hashCombine(hash, recipe.panelColumns);
    hash = gdp::core::hashCombine(hash, recipe.panelRows);
    hash = addFloat(hash, recipe.wearAmount);
    hash = addFloat(hash, recipe.dirtAmount);
    hash = addFloat(hash, recipe.oxidationAmount);
    hash = addFloat(hash, recipe.detailAmount);
    hash = addFloat(hash, recipe.emissiveAmount);
    return gdp::core::hashCombine(hash, recipe.resolution);
}

std::uint64_t storageRoomMaterialHash(const StorageRoomMaterialSet& materials) noexcept
{
    std::uint64_t hash = gdp::core::hashCombine(
        0x53544F5F524F4F4DULL, materials.roomSeed);
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(materials.condition));
    for (const StorageSurfaceRecipe& recipe : materials.surfaces) {
        hash = gdp::core::hashCombine(hash, storageSurfaceRecipeHash(recipe));
    }
    return hash;
}

std::optional<gdp::render::ImageData> generateStorageSurfaceAlbedo(
    const StorageSurfaceRecipe& recipe)
{
    if (!validateStorageSurfaceRecipe(recipe).isValid()) {
        return std::nullopt;
    }

    gdp::render::ImageData image;
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
                gdp::core::hashCombine(recipe.seed, panelId)) * 0.30F);
            gdp::core::Vec3 color = mix(
                recipe.baseColor, recipe.secondaryColor, panelVariation);
            const std::uint32_t panelX = x % cellWidth;
            const std::uint32_t panelY = y % cellHeight;
            const bool seam = panelX <= 1U || panelY <= 1U;
            if (seam) {
                color = multiply(color, 0.52F);
            }

            const std::uint64_t coarseId = (static_cast<std::uint64_t>(y / 8U) << 32U)
                | (x / 8U);
            const float coarseDirt = unitNoise(gdp::core::hashCombine(
                recipe.seed ^ 0x5702A6EULL, coarseId));
            if (coarseDirt < recipe.dirtAmount * 0.34F) {
                color = multiply(color, 0.74F);
            }
            const float wearNoise = unitNoise(gdp::core::hashCombine(
                recipe.seed ^ 0x5C0FF5ULL, pixelId));
            if (wearNoise < recipe.wearAmount * 0.035F) {
                color = add(color, 0.075F);
            }

            if (recipe.surface == RoomSurface::Wall) {
                const float rustNoise = unitNoise(gdp::core::hashCombine(
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

CombatRoomMaterialSet generateCombatRoomMaterials(const gdp::core::Seed roomSeed)
{
    CombatRoomMaterialSet materials;
    materials.roomSeed = roomSeed;
    materials.condition = selectCombatCondition(roomSeed);
    materials.surfaces = {
        makeCombatSurfaceRecipe(roomSeed, materials.condition, RoomSurface::Wall),
        makeCombatSurfaceRecipe(roomSeed, materials.condition, RoomSurface::Floor),
        makeCombatSurfaceRecipe(roomSeed, materials.condition, RoomSurface::Ceiling),
    };
    materials.materialHash = combatRoomMaterialHash(materials);
    return materials;
}

CombatMaterialValidationResult validateCombatSurfaceRecipe(
    const CombatSurfaceRecipe& recipe)
{
    CombatMaterialValidationResult result;
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
        || recipe.panelRows < 2U || recipe.panelRows > 3U) {
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
        const gdp::core::Seed expectedSeed = gdp::core::deriveSeed(
            materials.roomSeed, gdp::core::SeedDomain::RoomMaterial, index + 201U);
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
    std::uint64_t hash = gdp::core::hashCombine(0x434D425F53555246ULL, recipe.version);
    hash = gdp::core::hashCombine(hash, recipe.seed);
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.surface));
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.family));
    hash = addVec3(hash, recipe.baseColor);
    hash = addVec3(hash, recipe.secondaryColor);
    hash = addVec3(hash, recipe.accentColor);
    hash = gdp::core::hashCombine(hash, recipe.panelColumns);
    hash = gdp::core::hashCombine(hash, recipe.panelRows);
    hash = addFloat(hash, recipe.wearAmount);
    hash = addFloat(hash, recipe.impactAmount);
    hash = addFloat(hash, recipe.reinforcementAmount);
    hash = addFloat(hash, recipe.markingAmount);
    hash = addFloat(hash, recipe.emissiveAmount);
    return gdp::core::hashCombine(hash, recipe.resolution);
}

std::uint64_t combatRoomMaterialHash(const CombatRoomMaterialSet& materials) noexcept
{
    std::uint64_t hash = gdp::core::hashCombine(
        0x434D425F524F4F4DULL, materials.roomSeed);
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(materials.condition));
    for (const CombatSurfaceRecipe& recipe : materials.surfaces) {
        hash = gdp::core::hashCombine(hash, combatSurfaceRecipeHash(recipe));
    }
    return hash;
}

std::optional<gdp::render::ImageData> generateCombatSurfaceAlbedo(
    const CombatSurfaceRecipe& recipe)
{
    if (!validateCombatSurfaceRecipe(recipe).isValid()) {
        return std::nullopt;
    }

    gdp::render::ImageData image;
    image.width = static_cast<int>(recipe.resolution);
    image.height = static_cast<int>(recipe.resolution);
    image.rgba.resize(static_cast<std::size_t>(recipe.resolution)
        * static_cast<std::size_t>(recipe.resolution) * 4U);
    const std::uint32_t cellWidth = recipe.resolution / recipe.panelColumns;
    const std::uint32_t cellHeight = recipe.resolution / recipe.panelRows;
    const std::uint32_t impactCount = 1U
        + (recipe.impactAmount > 0.18F ? 1U : 0U)
        + (recipe.impactAmount > 0.34F ? 1U : 0U);

    for (std::uint32_t y = 0; y < recipe.resolution; ++y) {
        for (std::uint32_t x = 0; x < recipe.resolution; ++x) {
            const std::uint64_t pixelId = (static_cast<std::uint64_t>(y) << 32U) | x;
            const std::uint64_t panelId = (static_cast<std::uint64_t>(y / cellHeight) << 32U)
                | (x / cellWidth);
            const float panelBlend = 0.08F + (unitNoise(
                gdp::core::hashCombine(recipe.seed, panelId)) * 0.22F);
            gdp::core::Vec3 color = mix(
                recipe.baseColor, recipe.secondaryColor, panelBlend);
            const std::uint32_t panelX = x % cellWidth;
            const std::uint32_t panelY = y % cellHeight;
            const bool seam = panelX <= 2U || panelY <= 2U;
            if (seam) {
                color = multiply(color, 0.45F);
            } else if (panelX == 3U || panelY == 3U) {
                color = add(color, 0.055F);
            }

            const float wearNoise = unitNoise(gdp::core::hashCombine(
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
                    const std::uint64_t impactSeed = gdp::core::hashCombine(
                        recipe.seed ^ 0x1A4AC7ULL, impact);
                    const int centerX = 8 + static_cast<int>(impactSeed % 48U);
                    const int centerY = 8 + static_cast<int>((impactSeed >> 16U) % 48U);
                    const int radius = 2 + static_cast<int>((impactSeed >> 32U) % 2U);
                    const int dx = static_cast<int>(x) - centerX;
                    const int dy = static_cast<int>(y) - centerY;
                    const int distanceSquared = (dx * dx) + (dy * dy);
                    if (distanceSquared <= radius * radius) {
                        color = distanceSquared <= 1
                            ? gdp::core::Vec3{0.045F, 0.05F, 0.04F}
                            : gdp::core::Vec3{0.45F, 0.46F, 0.42F};
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
                const float scuffNoise = unitNoise(gdp::core::hashCombine(
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

CafeteriaRoomMaterialSet generateCafeteriaRoomMaterials(const gdp::core::Seed roomSeed)
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
        const gdp::core::Seed expectedSeed = gdp::core::deriveSeed(
            materials.roomSeed, gdp::core::SeedDomain::RoomMaterial, index + 301U);
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
    std::uint64_t hash = gdp::core::hashCombine(0x4341465F53555246ULL, recipe.version);
    hash = gdp::core::hashCombine(hash, recipe.seed);
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.surface));
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.family));
    hash = addVec3(hash, recipe.baseColor);
    hash = addVec3(hash, recipe.secondaryColor);
    hash = addVec3(hash, recipe.accentColor);
    hash = gdp::core::hashCombine(hash, recipe.panelColumns);
    hash = gdp::core::hashCombine(hash, recipe.panelRows);
    hash = addFloat(hash, recipe.wearAmount);
    hash = addFloat(hash, recipe.stainAmount);
    hash = addFloat(hash, recipe.stripeAmount);
    hash = addFloat(hash, recipe.furnitureMarkAmount);
    hash = addFloat(hash, recipe.emissiveAmount);
    return gdp::core::hashCombine(hash, recipe.resolution);
}

std::uint64_t cafeteriaRoomMaterialHash(const CafeteriaRoomMaterialSet& materials) noexcept
{
    std::uint64_t hash = gdp::core::hashCombine(
        0x4341465F524F4F4DULL, materials.roomSeed);
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(materials.condition));
    for (const CafeteriaSurfaceRecipe& recipe : materials.surfaces) {
        hash = gdp::core::hashCombine(hash, cafeteriaSurfaceRecipeHash(recipe));
    }
    return hash;
}

std::optional<gdp::render::ImageData> generateCafeteriaSurfaceAlbedo(
    const CafeteriaSurfaceRecipe& recipe)
{
    if (!validateCafeteriaSurfaceRecipe(recipe).isValid()) {
        return std::nullopt;
    }

    gdp::render::ImageData image;
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
                gdp::core::hashCombine(recipe.seed, panelId)) * 0.10F;
            gdp::core::Vec3 color = mix(
                recipe.baseColor, recipe.secondaryColor, panelBlend);
            const std::uint32_t panelX = x % cellWidth;
            const std::uint32_t panelY = y % cellHeight;
            const bool seam = panelX <= 1U || panelY <= 1U;
            if (seam) {
                color = multiply(color, 0.72F);
            }

            const float wearNoise = unitNoise(gdp::core::hashCombine(
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
                    const float baseDirt = unitNoise(gdp::core::hashCombine(
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
                if (!seam && unitNoise(gdp::core::hashCombine(
                        recipe.seed ^ 0x57A1AULL, stainCell)) < recipe.stainAmount * 0.18F) {
                    color = multiply(color, 0.82F);
                }
                for (std::uint32_t mark = 0; mark < 4U; ++mark) {
                    const std::uint64_t markSeed = gdp::core::hashCombine(
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

LaboratoryRoomMaterialSet generateLaboratoryRoomMaterials(const gdp::core::Seed roomSeed)
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
        const gdp::core::Seed expectedSeed = gdp::core::deriveSeed(
            materials.roomSeed, gdp::core::SeedDomain::RoomMaterial, index + 401U);
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
    std::uint64_t hash = gdp::core::hashCombine(0x4C41425F53555246ULL, recipe.version);
    hash = gdp::core::hashCombine(hash, recipe.seed);
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.surface));
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.family));
    hash = addVec3(hash, recipe.baseColor);
    hash = addVec3(hash, recipe.secondaryColor);
    hash = addVec3(hash, recipe.accentColor);
    hash = gdp::core::hashCombine(hash, recipe.panelColumns);
    hash = gdp::core::hashCombine(hash, recipe.panelRows);
    hash = addFloat(hash, recipe.wearAmount);
    hash = addFloat(hash, recipe.detailAmount);
    hash = addFloat(hash, recipe.contaminationAmount);
    hash = addFloat(hash, recipe.lightingCoverage);
    hash = addFloat(hash, recipe.emissiveAmount);
    return gdp::core::hashCombine(hash, recipe.resolution);
}

std::uint64_t laboratoryRoomMaterialHash(const LaboratoryRoomMaterialSet& materials) noexcept
{
    std::uint64_t hash = gdp::core::hashCombine(
        0x4C41425F524F4F4DULL, materials.roomSeed);
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(materials.condition));
    for (const LaboratorySurfaceRecipe& recipe : materials.surfaces) {
        hash = gdp::core::hashCombine(hash, laboratorySurfaceRecipeHash(recipe));
    }
    return hash;
}

std::optional<gdp::render::ImageData> generateLaboratorySurfaceAlbedo(
    const LaboratorySurfaceRecipe& recipe)
{
    if (!validateLaboratorySurfaceRecipe(recipe).isValid()) {
        return std::nullopt;
    }

    gdp::render::ImageData image;
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
                gdp::core::hashCombine(recipe.seed, panelId)) * 0.05F;
            gdp::core::Vec3 color = mix(
                recipe.baseColor, recipe.secondaryColor, panelBlend);
            const std::uint32_t panelX = x % cellWidth;
            const std::uint32_t panelY = y % cellHeight;
            const bool seam = panelX == 0U || panelY == 0U;
            if (seam) {
                color = multiply(color, 0.66F);
            }
            const float wearNoise = unitNoise(gdp::core::hashCombine(
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
                                         : gdp::core::Vec3{0.12F, 0.16F, 0.19F};
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

TechnicalRoomMaterialSet generateTechnicalRoomMaterials(const gdp::core::Seed roomSeed)
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
                           || recipe.pipeAmount <= 0.0F
                           || recipe.oilAmount != 0.0F
                           || recipe.mechanicalAmount <= 0.0F))
            || (isFloor && (recipe.family != MaterialFamily::BareSteel
                               || recipe.panelRows != 2U
                               || recipe.pipeAmount != 0.0F
                               || recipe.oilAmount <= 0.0F
                               || recipe.mechanicalAmount <= 0.0F))
            || (isCeiling && (recipe.family != MaterialFamily::BareSteel
                                 || recipe.pipeAmount <= 0.0F
                                 || recipe.oilAmount != 0.0F
                                 || recipe.mechanicalAmount <= 0.0F
                                 || recipe.emissiveAmount <= 0.0F))
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
        const gdp::core::Seed expectedSeed = gdp::core::deriveSeed(
            materials.roomSeed, gdp::core::SeedDomain::RoomMaterial, index + 501U);
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
    std::uint64_t hash = gdp::core::hashCombine(0x544543485F535552ULL, recipe.version);
    hash = gdp::core::hashCombine(hash, recipe.seed);
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.surface));
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.family));
    hash = addVec3(hash, recipe.baseColor);
    hash = addVec3(hash, recipe.secondaryColor);
    hash = addVec3(hash, recipe.accentColor);
    hash = gdp::core::hashCombine(hash, recipe.panelColumns);
    hash = gdp::core::hashCombine(hash, recipe.panelRows);
    hash = addFloat(hash, recipe.wearAmount);
    hash = addFloat(hash, recipe.dirtAmount);
    hash = addFloat(hash, recipe.pipeAmount);
    hash = addFloat(hash, recipe.oilAmount);
    hash = addFloat(hash, recipe.mechanicalAmount);
    hash = addFloat(hash, recipe.emissiveAmount);
    return gdp::core::hashCombine(hash, recipe.resolution);
}

std::uint64_t technicalRoomMaterialHash(const TechnicalRoomMaterialSet& materials) noexcept
{
    std::uint64_t hash = gdp::core::hashCombine(
        0x544543485F524F4FULL, materials.roomSeed);
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(materials.condition));
    for (const TechnicalSurfaceRecipe& recipe : materials.surfaces) {
        hash = gdp::core::hashCombine(hash, technicalSurfaceRecipeHash(recipe));
    }
    return hash;
}

std::optional<gdp::render::ImageData> generateTechnicalSurfaceAlbedo(
    const TechnicalSurfaceRecipe& recipe)
{
    if (!validateTechnicalSurfaceRecipe(recipe).isValid()) {
        return std::nullopt;
    }

    gdp::render::ImageData image;
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
            const float panelBlend = 0.08F + unitNoise(
                gdp::core::hashCombine(recipe.seed, panelId)) * 0.16F;
            gdp::core::Vec3 color = mix(
                recipe.baseColor, recipe.secondaryColor, panelBlend);
            const std::uint32_t panelX = x % cellWidth;
            const std::uint32_t panelY = y % cellHeight;
            const bool seam = panelX <= 2U || panelY <= 2U;
            if (seam) {
                color = multiply(color, 0.48F);
            }
            const float wearNoise = unitNoise(gdp::core::hashCombine(
                recipe.seed ^ 0x7EC471AULL, pixelId));
            if (!seam && wearNoise < recipe.wearAmount * 0.035F) {
                color = mix(color, {0.31F, 0.32F, 0.30F}, 0.58F);
            }

            if (recipe.surface == RoomSurface::Wall) {
                const std::uint32_t pipeCount = recipe.pipeAmount > 0.42F ? 2U : 1U;
                const int pipeHalfWidth = 2 + static_cast<int>(recipe.pipeAmount * 4.0F);
                for (std::uint32_t pipe = 0; pipe < pipeCount; ++pipe) {
                    const int centerX = pipe == 0U ? 14 : 34;
                    const int dx = static_cast<int>(x) - centerX;
                    if (std::abs(dx) <= pipeHalfWidth) {
                        if (dx <= -pipeHalfWidth + 1) {
                            color = {0.10F, 0.11F, 0.10F};
                        } else if (dx >= pipeHalfWidth - 1) {
                            color = {0.48F, 0.49F, 0.45F};
                        } else {
                            color = {0.31F, 0.32F, 0.30F};
                        }
                    }
                }
                if (recipe.mechanicalAmount > 0.20F) {
                    const int dx = static_cast<int>(x) - 14;
                    const int dy = static_cast<int>(y) - 32;
                    const int distanceSquared = (dx * dx) + (dy * dy);
                    if (distanceSquared <= 25 && distanceSquared >= 10) {
                        color = {0.48F, 0.12F, 0.10F};
                    } else if (distanceSquared < 10) {
                        color = recipe.accentColor;
                    }
                }
                const bool box = x >= 44U && x < 59U && y >= 17U && y < 36U;
                if (box) {
                    const bool border = x < 46U || x >= 57U || y < 19U || y >= 34U;
                    color = border ? gdp::core::Vec3{0.32F, 0.33F, 0.31F}
                                   : gdp::core::Vec3{0.15F, 0.16F, 0.15F};
                }
                if (y < 7U && (x < 22U || (pipeCount == 2U && x > 27U && x < 42U))) {
                    const float dirt = unitNoise(gdp::core::hashCombine(
                        recipe.seed ^ 0xD127ULL, x / 2U));
                    if (dirt < recipe.dirtAmount) {
                        color = multiply(color, 0.62F);
                    }
                }
            } else if (recipe.surface == RoomSurface::Floor) {
                const bool bolt = ((panelX >= 5U && panelX <= 6U)
                        || (panelX + 7U >= cellWidth && panelX + 5U < cellWidth))
                    && ((panelY >= 5U && panelY <= 6U)
                        || (panelY + 7U >= cellHeight && panelY + 5U < cellHeight));
                if (bolt) {
                    color = {0.38F, 0.39F, 0.36F};
                }
                if (recipe.mechanicalAmount > 0.17F) {
                    const bool grate = x >= 8U && x < 29U && y >= 36U && y < 57U;
                    if (grate) {
                        const bool frame = x < 11U || x >= 26U || y < 39U || y >= 54U;
                        color = frame || (x % 4U <= 1U)
                            ? gdp::core::Vec3{0.27F, 0.28F, 0.26F}
                            : gdp::core::Vec3{0.07F, 0.08F, 0.07F};
                    }
                }
                const int radiusX = 4 + static_cast<int>(recipe.oilAmount * 18.0F);
                const int radiusY = std::max(3, radiusX * 2 / 3);
                const int dx = static_cast<int>(x) - 47;
                const int dy = static_cast<int>(y) - 17;
                const int ellipse = (dx * dx * radiusY * radiusY)
                    + (dy * dy * radiusX * radiusX);
                const int radiusSquared = radiusX * radiusX * radiusY * radiusY;
                if (ellipse <= radiusSquared) {
                    color = ellipse < radiusSquared * 3 / 5
                        ? gdp::core::Vec3{0.09F, 0.10F, 0.09F}
                        : mix(color, {0.09F, 0.10F, 0.09F}, 0.56F);
                }
            } else {
                const std::uint32_t panelLinear = (y / cellHeight) * recipe.panelColumns
                    + (x / cellWidth);
                const float category = unitNoise(gdp::core::hashCombine(
                    recipe.seed ^ 0xCE111A6ULL, panelLinear));
                const std::uint32_t centerX = cellWidth / 2U;
                const std::uint32_t centerY = cellHeight / 2U;
                if (category < 0.60F) {
                    if (panelY >= centerY - 3U && panelY <= centerY + 3U) {
                        color = panelY == centerY - 3U
                            ? gdp::core::Vec3{0.08F, 0.09F, 0.08F}
                            : (panelY == centerY + 3U
                                    ? gdp::core::Vec3{0.42F, 0.43F, 0.39F}
                                    : gdp::core::Vec3{0.27F, 0.28F, 0.26F});
                    }
                } else if (category < 0.75F) {
                    const bool duct = panelX >= 7U && panelX + 7U < cellWidth
                        && panelY >= centerY - 6U && panelY <= centerY + 6U;
                    if (duct) {
                        const bool border = panelX < 10U || panelX + 10U >= cellWidth
                            || panelY == centerY - 6U || panelY == centerY + 6U;
                        color = border ? gdp::core::Vec3{0.08F, 0.09F, 0.08F}
                                       : gdp::core::Vec3{0.25F, 0.26F, 0.24F};
                    }
                } else if (category < 0.90F
                    && (panelX == centerX || panelX == centerX + 2U)) {
                    color = {0.32F, 0.20F, 0.08F};
                }
            }
            writePixel(image, x, y, color);
        }
    }
    return image;
}

SecurityRoomMaterialSet generateSecurityRoomMaterials(const gdp::core::Seed roomSeed)
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
        const gdp::core::Seed expectedSeed = gdp::core::deriveSeed(
            materials.roomSeed, gdp::core::SeedDomain::RoomMaterial, index + 601U);
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
    std::uint64_t hash = gdp::core::hashCombine(0x5345435F53555246ULL, recipe.version);
    hash = gdp::core::hashCombine(hash, recipe.seed);
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.surface));
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.family));
    hash = addVec3(hash, recipe.baseColor);
    hash = addVec3(hash, recipe.secondaryColor);
    hash = addVec3(hash, recipe.accentColor);
    hash = gdp::core::hashCombine(hash, recipe.panelColumns);
    hash = gdp::core::hashCombine(hash, recipe.panelRows);
    hash = addFloat(hash, recipe.wearAmount);
    hash = addFloat(hash, recipe.monitorAmount);
    hash = addFloat(hash, recipe.statusAmount);
    hash = addFloat(hash, recipe.accessAmount);
    hash = addFloat(hash, recipe.alertAmount);
    hash = addFloat(hash, recipe.emissiveAmount);
    return gdp::core::hashCombine(hash, recipe.resolution);
}

std::uint64_t securityRoomMaterialHash(const SecurityRoomMaterialSet& materials) noexcept
{
    std::uint64_t hash = gdp::core::hashCombine(
        0x5345435F524F4F4DULL, materials.roomSeed);
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(materials.condition));
    for (const SecuritySurfaceRecipe& recipe : materials.surfaces) {
        hash = gdp::core::hashCombine(hash, securitySurfaceRecipeHash(recipe));
    }
    return hash;
}

std::optional<gdp::render::ImageData> generateSecuritySurfaceAlbedo(
    const SecuritySurfaceRecipe& recipe)
{
    if (!validateSecuritySurfaceRecipe(recipe).isValid()) {
        return std::nullopt;
    }

    gdp::render::ImageData image;
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
                gdp::core::hashCombine(recipe.seed, panelId)) * 0.10F;
            gdp::core::Vec3 color = mix(
                recipe.baseColor, recipe.secondaryColor, panelBlend);
            const std::uint32_t panelX = x % cellWidth;
            const std::uint32_t panelY = y % cellHeight;
            const bool seam = panelX <= 1U || panelY <= 1U;
            if (seam) {
                color = multiply(color, 0.56F);
            }
            const float wearNoise = unitNoise(gdp::core::hashCombine(
                recipe.seed ^ 0x5EC017ULL, pixelId));
            if (!seam && wearNoise < recipe.wearAmount * 0.015F) {
                color = mix(color, {0.35F, 0.37F, 0.38F}, 0.42F);
            }

            if (recipe.surface == RoomSurface::Wall) {
                const bool console = x >= 35U && x < 60U && y >= 12U && y < 45U;
                if (console) {
                    const bool frame = x < 38U || x >= 57U || y < 15U || y >= 42U;
                    color = frame ? gdp::core::Vec3{0.33F, 0.36F, 0.38F}
                                  : gdp::core::Vec3{0.08F, 0.10F, 0.11F};
                }
                const bool screen = x >= 40U && x < 56U && y >= 18U && y < 31U;
                if (screen) {
                    const float screenState = unitNoise(gdp::core::hashCombine(
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
                            ? gdp::core::Vec3{0.16F, 0.32F, 0.38F}
                            : gdp::core::Vec3{0.06F, 0.12F, 0.15F};
                    } else {
                        color = {0.49F, 0.31F, 0.08F};
                    }
                }
                for (std::uint32_t indicator = 0; indicator < 4U; ++indicator) {
                    const std::uint32_t indicatorX = 39U + indicator * 4U;
                    if (x >= indicatorX && x < indicatorX + 2U
                        && y >= 35U && y < 37U) {
                        color = recipe.alertAmount > 0.15F && indicator == 0U
                            ? gdp::core::Vec3{0.66F, 0.20F, 0.20F}
                            : gdp::core::Vec3{0.23F, 0.61F, 0.28F};
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

CentralHubRoomMaterialSet generateCentralHubRoomMaterials(const gdp::core::Seed roomSeed)
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
        const gdp::core::Seed expectedSeed = gdp::core::deriveSeed(
            materials.roomSeed, gdp::core::SeedDomain::RoomMaterial, index + 701U);
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
    std::uint64_t hash = gdp::core::hashCombine(0x4855425F53555246ULL, recipe.version);
    hash = gdp::core::hashCombine(hash, recipe.seed);
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.surface));
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.family));
    hash = addVec3(hash, recipe.baseColor);
    hash = addVec3(hash, recipe.secondaryColor);
    hash = addVec3(hash, recipe.accentColor);
    hash = gdp::core::hashCombine(hash, recipe.panelColumns);
    hash = gdp::core::hashCombine(hash, recipe.panelRows);
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.routeAxis));
    hash = addFloat(hash, recipe.wearAmount);
    hash = addFloat(hash, recipe.structureAmount);
    hash = addFloat(hash, recipe.navigationAmount);
    hash = addFloat(hash, recipe.focalAmount);
    hash = addFloat(hash, recipe.emissiveAmount);
    return gdp::core::hashCombine(hash, recipe.resolution);
}

std::uint64_t centralHubRoomMaterialHash(const CentralHubRoomMaterialSet& materials) noexcept
{
    std::uint64_t hash = gdp::core::hashCombine(
        0x4855425F524F4F4DULL, materials.roomSeed);
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(materials.condition));
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(materials.routeAxis));
    for (const CentralHubSurfaceRecipe& recipe : materials.surfaces) {
        hash = gdp::core::hashCombine(hash, centralHubSurfaceRecipeHash(recipe));
    }
    return hash;
}

std::optional<gdp::render::ImageData> generateCentralHubSurfaceAlbedo(
    const CentralHubSurfaceRecipe& recipe)
{
    if (!validateCentralHubSurfaceRecipe(recipe).isValid()) {
        return std::nullopt;
    }

    gdp::render::ImageData image;
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
                gdp::core::hashCombine(recipe.seed, panelId)) * 0.08F;
            gdp::core::Vec3 color = mix(
                recipe.baseColor, recipe.secondaryColor, panelBlend);
            const std::uint32_t panelX = x % cellWidth;
            const std::uint32_t panelY = y % cellHeight;
            const bool seam = panelX <= 1U || panelY <= 1U;
            if (seam) {
                color = multiply(color, 0.55F);
            }
            const float wearNoise = unitNoise(gdp::core::hashCombine(
                recipe.seed ^ 0x48554217ULL, pixelId));
            if (!seam && wearNoise < recipe.wearAmount * 0.012F) {
                color = mix(color, {0.35F, 0.38F, 0.39F}, 0.32F);
            }

            if (recipe.surface == RoomSurface::Wall) {
                if (x >= 30U && x <= 33U) {
                    color = multiply(recipe.secondaryColor, 0.66F);
                }
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

} // namespace gdp::procedural
