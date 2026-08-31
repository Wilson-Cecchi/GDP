#pragma once

#include "core/Math.hpp"
#include "core/Seed.hpp"

#include <cstdint>
#include <vector>

namespace wkc::procedural {

enum class PrimitiveType : std::uint8_t {
    Box,
    RoundedBox,
    Cylinder,
    Capsule,
    Sphere,
    Cone,
    ExtrudedProfile,
};

enum class SemanticRegion : std::uint8_t {
    Generic,
    ArmorPlate,
    Fabric,
    Rubber,
    Joint,
    Piston,
    Cable,
    Lens,
    Tool,
    Weapon,
    Skin,
    Bone,
    MechanicalCore,
    FootContact,
    HandContact,
    Count,
};

enum class MaterialFamily : std::uint8_t {
    PaintedSteel,
    BareSteel,
    Aluminum,
    Fabric,
    Rubber,
    Polymer,
    Polycarbonate,
    Ceramic,
    OrganicSkin,
    Bone,
    CorruptedOrganic,
    Count,
};

enum class MaterialSlot : std::uint8_t {
    Primary,
    Secondary,
    Armor,
    Fabric,
    Rubber,
    Lens,
    Organic,
    Emissive,
    Count,
};

struct PrimitiveRecipe {
    PrimitiveType type{PrimitiveType::Box};
    gdp::core::Transform localTransform{};
    gdp::core::Vec3 dimensions{1.0F, 1.0F, 1.0F};
    MaterialSlot material{MaterialSlot::Primary};
    SemanticRegion semantic{SemanticRegion::Generic};

    constexpr bool operator==(const PrimitiveRecipe&) const noexcept = default;
};

struct MeshRecipe {
    std::uint32_t version{1};
    gdp::core::Seed seed{0};
    std::vector<PrimitiveRecipe> primitives;

    bool operator==(const MeshRecipe&) const noexcept = default;
};

struct TextureRecipe {
    std::uint32_t version{1};
    gdp::core::Seed seed{0};
    MaterialFamily family{MaterialFamily::PaintedSteel};
    gdp::core::Vec3 baseColor{0.5F, 0.5F, 0.5F};
    gdp::core::Vec3 secondaryColor{0.25F, 0.25F, 0.25F};
    float macroVariation{0.2F};
    float microVariation{0.1F};
    float wearAmount{0.0F};
    float dirtAmount{0.0F};
    float scratchAmount{0.0F};
    float oxidationAmount{0.0F};
    float emissiveAmount{0.0F};
    SemanticRegion region{SemanticRegion::Generic};
    std::uint32_t resolution{64};

    constexpr bool operator==(const TextureRecipe&) const noexcept = default;
};

enum class RecipeValidationError : std::uint8_t {
    EmptyMesh,
    TooManyPrimitives,
    UnsupportedPrimitive,
    InvalidTransform,
    InvalidDimensions,
    InvalidMaterialSlot,
    InvalidSemanticRegion,
    InvalidMaterialFamily,
    InvalidColor,
    InvalidTextureParameter,
    InvalidTextureResolution,
};

struct RecipeValidationResult {
    std::vector<RecipeValidationError> errors;

    [[nodiscard]] bool isValid() const noexcept;
};

struct MaterialProperties {
    float roughness{0.7F};
    float metallic{0.0F};
    bool oxidationAllowed{false};
    bool emissiveAllowed{false};
};

[[nodiscard]] RecipeValidationResult validateMeshRecipe(const MeshRecipe& recipe);
[[nodiscard]] RecipeValidationResult validateTextureRecipe(const TextureRecipe& recipe);
[[nodiscard]] MaterialProperties materialProperties(MaterialFamily family) noexcept;
[[nodiscard]] std::uint64_t meshRecipeHash(const MeshRecipe& recipe) noexcept;
[[nodiscard]] std::uint64_t textureRecipeHash(const TextureRecipe& recipe) noexcept;

} // namespace wkc::procedural
