#include "procedural/AssetRecipe.hpp"

#include "core/Hash.hpp"

#include <bit>
#include <cmath>

namespace wkc::procedural {
namespace {

bool isFinite(const gdp::core::Vec3 value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool isUnitColor(const gdp::core::Vec3 color) noexcept
{
    return isFinite(color) && color.x >= 0.0F && color.x <= 1.0F
        && color.y >= 0.0F && color.y <= 1.0F
        && color.z >= 0.0F && color.z <= 1.0F;
}

bool isUnitParameter(const float value) noexcept
{
    return std::isfinite(value) && value >= 0.0F && value <= 1.0F;
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

} // namespace

bool RecipeValidationResult::isValid() const noexcept
{
    return errors.empty();
}

RecipeValidationResult validateMeshRecipe(const MeshRecipe& recipe)
{
    RecipeValidationResult result;
    if (recipe.primitives.empty()) {
        result.errors.push_back(RecipeValidationError::EmptyMesh);
    }
    if (recipe.primitives.size() > 1'024U) {
        result.errors.push_back(RecipeValidationError::TooManyPrimitives);
    }
    for (const PrimitiveRecipe& primitive : recipe.primitives) {
        if (primitive.type != PrimitiveType::Box
            && primitive.type != PrimitiveType::Cylinder) {
            result.errors.push_back(RecipeValidationError::UnsupportedPrimitive);
        }
        if (!isFinite(primitive.localTransform.position)
            || !isFinite(primitive.localTransform.rotationDegrees)
            || !isFinite(primitive.localTransform.scale)
            || primitive.localTransform.scale.x <= 0.0F
            || primitive.localTransform.scale.y <= 0.0F
            || primitive.localTransform.scale.z <= 0.0F) {
            result.errors.push_back(RecipeValidationError::InvalidTransform);
        }
        if (!isFinite(primitive.dimensions) || primitive.dimensions.x <= 0.0F
            || primitive.dimensions.y <= 0.0F || primitive.dimensions.z <= 0.0F) {
            result.errors.push_back(RecipeValidationError::InvalidDimensions);
        }
        if (primitive.material >= MaterialSlot::Count) {
            result.errors.push_back(RecipeValidationError::InvalidMaterialSlot);
        }
        if (primitive.semantic >= SemanticRegion::Count) {
            result.errors.push_back(RecipeValidationError::InvalidSemanticRegion);
        }
    }
    return result;
}

RecipeValidationResult validateTextureRecipe(const TextureRecipe& recipe)
{
    RecipeValidationResult result;
    if (recipe.family >= MaterialFamily::Count) {
        result.errors.push_back(RecipeValidationError::InvalidMaterialFamily);
    }
    if (recipe.region >= SemanticRegion::Count) {
        result.errors.push_back(RecipeValidationError::InvalidSemanticRegion);
    }
    if (!isUnitColor(recipe.baseColor) || !isUnitColor(recipe.secondaryColor)) {
        result.errors.push_back(RecipeValidationError::InvalidColor);
    }
    if (!isUnitParameter(recipe.macroVariation)
        || !isUnitParameter(recipe.microVariation)
        || !isUnitParameter(recipe.wearAmount)
        || !isUnitParameter(recipe.dirtAmount)
        || !isUnitParameter(recipe.scratchAmount)
        || !isUnitParameter(recipe.oxidationAmount)
        || !isUnitParameter(recipe.emissiveAmount)) {
        result.errors.push_back(RecipeValidationError::InvalidTextureParameter);
    }
    const bool powerOfTwo = recipe.resolution != 0
        && (recipe.resolution & (recipe.resolution - 1U)) == 0;
    if (!powerOfTwo || recipe.resolution < 8U || recipe.resolution > 1'024U) {
        result.errors.push_back(RecipeValidationError::InvalidTextureResolution);
    }
    return result;
}

MaterialProperties materialProperties(const MaterialFamily family) noexcept
{
    switch (family) {
    case MaterialFamily::PaintedSteel:
        return {0.62F, 0.18F, true, false};
    case MaterialFamily::BareSteel:
        return {0.38F, 0.92F, true, false};
    case MaterialFamily::Aluminum:
        return {0.34F, 0.86F, false, false};
    case MaterialFamily::Fabric:
        return {0.90F, 0.0F, false, false};
    case MaterialFamily::Rubber:
        return {0.82F, 0.0F, false, false};
    case MaterialFamily::Polymer:
        return {0.68F, 0.0F, false, false};
    case MaterialFamily::Polycarbonate:
        return {0.24F, 0.0F, false, true};
    case MaterialFamily::Ceramic:
        return {0.48F, 0.0F, false, false};
    case MaterialFamily::OrganicSkin:
        return {0.72F, 0.0F, false, false};
    case MaterialFamily::Bone:
        return {0.76F, 0.0F, false, false};
    case MaterialFamily::CorruptedOrganic:
        return {0.58F, 0.0F, false, true};
    case MaterialFamily::Count:
        break;
    }
    return {};
}

std::uint64_t meshRecipeHash(const MeshRecipe& recipe) noexcept
{
    std::uint64_t hash = gdp::core::hashCombine(0x4D4553485F524543ULL, recipe.version);
    hash = gdp::core::hashCombine(hash, recipe.seed);
    hash = gdp::core::hashCombine(hash, recipe.primitives.size());
    for (const PrimitiveRecipe& primitive : recipe.primitives) {
        hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(primitive.type));
        hash = addVec3(hash, primitive.localTransform.position);
        hash = addVec3(hash, primitive.localTransform.rotationDegrees);
        hash = addVec3(hash, primitive.localTransform.scale);
        hash = addVec3(hash, primitive.dimensions);
        hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(primitive.material));
        hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(primitive.semantic));
    }
    return hash;
}

std::uint64_t textureRecipeHash(const TextureRecipe& recipe) noexcept
{
    std::uint64_t hash = gdp::core::hashCombine(0x544558545F524543ULL, recipe.version);
    hash = gdp::core::hashCombine(hash, recipe.seed);
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.family));
    hash = addVec3(hash, recipe.baseColor);
    hash = addVec3(hash, recipe.secondaryColor);
    hash = addFloat(hash, recipe.macroVariation);
    hash = addFloat(hash, recipe.microVariation);
    hash = addFloat(hash, recipe.wearAmount);
    hash = addFloat(hash, recipe.dirtAmount);
    hash = addFloat(hash, recipe.scratchAmount);
    hash = addFloat(hash, recipe.oxidationAmount);
    hash = addFloat(hash, recipe.emissiveAmount);
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.region));
    return gdp::core::hashCombine(hash, recipe.resolution);
}

} // namespace wkc::procedural
