#pragma once

#include "core/Math.hpp"
#include "core/Seed.hpp"
#include "procedural/AssetRecipe.hpp"
#include "render/Texture.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace gdp::game::visual {

enum class RoomSurface : std::uint8_t {
    Wall,
    Floor,
    Ceiling,
    Count,
};

using IndustrialSurface = RoomSurface;

enum class IndustrialCondition : std::uint8_t {
    Maintained,
    Used,
    Worn,
    Count,
};

struct IndustrialSurfaceRecipe {
    std::uint32_t version{1};
    gdp::core::Seed seed{0};
    IndustrialSurface surface{IndustrialSurface::Wall};
    gdp::procedural::MaterialFamily family{gdp::procedural::MaterialFamily::PaintedSteel};
    gdp::core::Vec3 baseColor{0.33F, 0.35F, 0.36F};
    gdp::core::Vec3 secondaryColor{0.20F, 0.21F, 0.22F};
    gdp::core::Vec3 accentColor{0.83F, 0.60F, 0.0F};
    std::uint8_t panelColumns{2};
    std::uint8_t panelRows{2};
    float wearAmount{0.2F};
    float dirtAmount{0.16F};
    float emissiveAmount{0.0F};
    bool safetyStripe{true};
    std::uint32_t resolution{64};

    constexpr bool operator==(const IndustrialSurfaceRecipe&) const noexcept = default;
};

struct IndustrialWallMaterialImages {
    wkc::render::ImageData albedo;
    wkc::render::ImageData normal;
    wkc::render::ImageData properties;
};

using IndustrialCeilingMaterialImages = IndustrialWallMaterialImages;

enum class StorageCondition : std::uint8_t {
    Orderly,
    Dusty,
    Neglected,
    Count,
};

struct StorageSurfaceRecipe {
    std::uint32_t version{1};
    gdp::core::Seed seed{0};
    RoomSurface surface{RoomSurface::Wall};
    gdp::procedural::MaterialFamily family{gdp::procedural::MaterialFamily::PaintedSteel};
    gdp::core::Vec3 baseColor{0.36F, 0.32F, 0.27F};
    gdp::core::Vec3 secondaryColor{0.22F, 0.21F, 0.20F};
    gdp::core::Vec3 accentColor{0.45F, 0.28F, 0.18F};
    std::uint8_t panelColumns{2};
    std::uint8_t panelRows{2};
    float wearAmount{0.35F};
    float dirtAmount{0.48F};
    float oxidationAmount{0.12F};
    float detailAmount{0.18F};
    float emissiveAmount{0.0F};
    std::uint32_t resolution{64};

    constexpr bool operator==(const StorageSurfaceRecipe&) const noexcept = default;
};

struct StorageRoomMaterialSet {
    gdp::core::Seed roomSeed{0};
    StorageCondition condition{StorageCondition::Dusty};
    std::array<StorageSurfaceRecipe,
        static_cast<std::size_t>(RoomSurface::Count)> surfaces{};
    std::uint64_t materialHash{0};

    constexpr bool operator==(const StorageRoomMaterialSet&) const noexcept = default;
};

enum class CombatCondition : std::uint8_t {
    Ready,
    Active,
    Scarred,
    Count,
};

struct CombatSurfaceRecipe {
    std::uint32_t version{1};
    gdp::core::Seed seed{0};
    RoomSurface surface{RoomSurface::Wall};
    gdp::procedural::MaterialFamily family{gdp::procedural::MaterialFamily::PaintedSteel};
    gdp::core::Vec3 baseColor{0.31F, 0.35F, 0.24F};
    gdp::core::Vec3 secondaryColor{0.22F, 0.24F, 0.17F};
    gdp::core::Vec3 accentColor{0.71F, 0.51F, 0.07F};
    std::uint8_t panelColumns{2};
    std::uint8_t panelRows{2};
    float wearAmount{0.38F};
    float impactAmount{0.16F};
    float reinforcementAmount{0.42F};
    float markingAmount{0.12F};
    float emissiveAmount{0.0F};
    std::uint32_t resolution{64};

    constexpr bool operator==(const CombatSurfaceRecipe&) const noexcept = default;
};

struct CombatRoomMaterialSet {
    gdp::core::Seed roomSeed{0};
    CombatCondition condition{CombatCondition::Active};
    std::array<CombatSurfaceRecipe,
        static_cast<std::size_t>(RoomSurface::Count)> surfaces{};
    std::uint64_t materialHash{0};

    constexpr bool operator==(const CombatRoomMaterialSet&) const noexcept = default;
};

enum class CafeteriaCondition : std::uint8_t {
    Clean,
    InUse,
    Faded,
    Count,
};

struct CafeteriaSurfaceRecipe {
    std::uint32_t version{1};
    gdp::core::Seed seed{0};
    RoomSurface surface{RoomSurface::Wall};
    gdp::procedural::MaterialFamily family{gdp::procedural::MaterialFamily::Ceramic};
    gdp::core::Vec3 baseColor{0.78F, 0.78F, 0.76F};
    gdp::core::Vec3 secondaryColor{0.62F, 0.62F, 0.61F};
    gdp::core::Vec3 accentColor{0.62F, 0.18F, 0.18F};
    std::uint8_t panelColumns{2};
    std::uint8_t panelRows{2};
    float wearAmount{0.18F};
    float stainAmount{0.10F};
    float stripeAmount{0.10F};
    float furnitureMarkAmount{0.0F};
    float emissiveAmount{0.0F};
    std::uint32_t resolution{64};

    constexpr bool operator==(const CafeteriaSurfaceRecipe&) const noexcept = default;
};

struct CafeteriaRoomMaterialSet {
    gdp::core::Seed roomSeed{0};
    CafeteriaCondition condition{CafeteriaCondition::InUse};
    std::array<CafeteriaSurfaceRecipe,
        static_cast<std::size_t>(RoomSurface::Count)> surfaces{};
    std::uint64_t materialHash{0};

    constexpr bool operator==(const CafeteriaRoomMaterialSet&) const noexcept = default;
};

enum class LaboratoryCondition : std::uint8_t {
    Sterile,
    Operational,
    Contained,
    Count,
};

struct LaboratorySurfaceRecipe {
    std::uint32_t version{1};
    gdp::core::Seed seed{0};
    RoomSurface surface{RoomSurface::Wall};
    gdp::procedural::MaterialFamily family{gdp::procedural::MaterialFamily::Ceramic};
    gdp::core::Vec3 baseColor{0.78F, 0.80F, 0.80F};
    gdp::core::Vec3 secondaryColor{0.66F, 0.68F, 0.68F};
    gdp::core::Vec3 accentColor{0.27F, 0.41F, 0.54F};
    std::uint8_t panelColumns{2};
    std::uint8_t panelRows{2};
    float wearAmount{0.10F};
    float detailAmount{0.14F};
    float contaminationAmount{0.0F};
    float lightingCoverage{0.0F};
    float emissiveAmount{0.0F};
    std::uint32_t resolution{64};

    constexpr bool operator==(const LaboratorySurfaceRecipe&) const noexcept = default;
};

struct LaboratoryRoomMaterialSet {
    gdp::core::Seed roomSeed{0};
    LaboratoryCondition condition{LaboratoryCondition::Operational};
    std::array<LaboratorySurfaceRecipe,
        static_cast<std::size_t>(RoomSurface::Count)> surfaces{};
    std::uint64_t materialHash{0};

    constexpr bool operator==(const LaboratoryRoomMaterialSet&) const noexcept = default;
};

enum class TechnicalCondition : std::uint8_t {
    Serviced,
    Working,
    Overloaded,
    Count,
};

struct TechnicalSurfaceRecipe {
    std::uint32_t version{1};
    gdp::core::Seed seed{0};
    RoomSurface surface{RoomSurface::Wall};
    gdp::procedural::MaterialFamily family{gdp::procedural::MaterialFamily::PaintedSteel};
    gdp::core::Vec3 baseColor{0.23F, 0.23F, 0.22F};
    gdp::core::Vec3 secondaryColor{0.13F, 0.14F, 0.13F};
    gdp::core::Vec3 accentColor{0.71F, 0.48F, 0.0F};
    std::uint8_t panelColumns{2};
    std::uint8_t panelRows{2};
    float wearAmount{0.36F};
    float dirtAmount{0.40F};
    float pipeAmount{0.38F};
    float oilAmount{0.0F};
    float mechanicalAmount{0.24F};
    float emissiveAmount{0.0F};
    std::uint32_t resolution{64};

    constexpr bool operator==(const TechnicalSurfaceRecipe&) const noexcept = default;
};

struct TechnicalRoomMaterialSet {
    gdp::core::Seed roomSeed{0};
    TechnicalCondition condition{TechnicalCondition::Working};
    std::array<TechnicalSurfaceRecipe,
        static_cast<std::size_t>(RoomSurface::Count)> surfaces{};
    std::uint64_t materialHash{0};

    constexpr bool operator==(const TechnicalRoomMaterialSet&) const noexcept = default;
};

enum class SecurityCondition : std::uint8_t {
    Secured,
    Monitored,
    Alert,
    Count,
};

struct SecuritySurfaceRecipe {
    std::uint32_t version{1};
    gdp::core::Seed seed{0};
    RoomSurface surface{RoomSurface::Wall};
    gdp::procedural::MaterialFamily family{gdp::procedural::MaterialFamily::PaintedSteel};
    gdp::core::Vec3 baseColor{0.23F, 0.24F, 0.25F};
    gdp::core::Vec3 secondaryColor{0.15F, 0.16F, 0.17F};
    gdp::core::Vec3 accentColor{0.23F, 0.40F, 0.47F};
    std::uint8_t panelColumns{2};
    std::uint8_t panelRows{2};
    float wearAmount{0.12F};
    float monitorAmount{0.18F};
    float statusAmount{0.14F};
    float accessAmount{0.0F};
    float alertAmount{0.04F};
    float emissiveAmount{0.0F};
    std::uint32_t resolution{64};

    constexpr bool operator==(const SecuritySurfaceRecipe&) const noexcept = default;
};

struct SecurityRoomMaterialSet {
    gdp::core::Seed roomSeed{0};
    SecurityCondition condition{SecurityCondition::Monitored};
    std::array<SecuritySurfaceRecipe,
        static_cast<std::size_t>(RoomSurface::Count)> surfaces{};
    std::uint64_t materialHash{0};

    constexpr bool operator==(const SecurityRoomMaterialSet&) const noexcept = default;
};

enum class CentralHubCondition : std::uint8_t {
    Open,
    Active,
    Rerouted,
    Count,
};

enum class CentralHubRouteAxis : std::uint8_t {
    Horizontal,
    Vertical,
    Cross,
    Count,
};

struct CentralHubSurfaceRecipe {
    std::uint32_t version{1};
    gdp::core::Seed seed{0};
    RoomSurface surface{RoomSurface::Wall};
    gdp::procedural::MaterialFamily family{gdp::procedural::MaterialFamily::PaintedSteel};
    gdp::core::Vec3 baseColor{0.27F, 0.29F, 0.30F};
    gdp::core::Vec3 secondaryColor{0.17F, 0.19F, 0.20F};
    gdp::core::Vec3 accentColor{0.17F, 0.54F, 0.58F};
    std::uint8_t panelColumns{2};
    std::uint8_t panelRows{2};
    CentralHubRouteAxis routeAxis{CentralHubRouteAxis::Horizontal};
    float wearAmount{0.08F};
    float structureAmount{0.18F};
    float navigationAmount{0.10F};
    float focalAmount{0.04F};
    float emissiveAmount{0.0F};
    std::uint32_t resolution{64};

    constexpr bool operator==(const CentralHubSurfaceRecipe&) const noexcept = default;
};

struct CentralHubRoomMaterialSet {
    gdp::core::Seed roomSeed{0};
    CentralHubCondition condition{CentralHubCondition::Active};
    CentralHubRouteAxis routeAxis{CentralHubRouteAxis::Horizontal};
    std::array<CentralHubSurfaceRecipe,
        static_cast<std::size_t>(RoomSurface::Count)> surfaces{};
    std::uint64_t materialHash{0};

    constexpr bool operator==(const CentralHubRoomMaterialSet&) const noexcept = default;
};

struct IndustrialRoomMaterialSet {
    gdp::core::Seed roomSeed{0};
    IndustrialCondition condition{IndustrialCondition::Used};
    std::array<IndustrialSurfaceRecipe,
        static_cast<std::size_t>(IndustrialSurface::Count)> surfaces{};
    std::uint64_t materialHash{0};

    constexpr bool operator==(const IndustrialRoomMaterialSet&) const noexcept = default;
};

enum class IndustrialMaterialValidationError : std::uint8_t {
    InvalidCondition,
    InvalidSurface,
    InvalidFamily,
    InvalidColor,
    InvalidPanelGrid,
    InvalidParameter,
    InvalidResolution,
    InvalidSurfaceRule,
    InvalidSurfaceOrder,
    InvalidMaterialHash,
};

struct IndustrialMaterialValidationResult {
    std::vector<IndustrialMaterialValidationError> errors;

    [[nodiscard]] bool isValid() const noexcept;
};

enum class StorageMaterialValidationError : std::uint8_t {
    InvalidCondition,
    InvalidSurface,
    InvalidFamily,
    InvalidColor,
    InvalidPanelGrid,
    InvalidParameter,
    InvalidResolution,
    InvalidSurfaceRule,
    InvalidSurfaceOrder,
    InvalidMaterialHash,
};

struct StorageMaterialValidationResult {
    std::vector<StorageMaterialValidationError> errors;

    [[nodiscard]] bool isValid() const noexcept;
};

enum class CombatMaterialValidationError : std::uint8_t {
    InvalidCondition,
    InvalidSurface,
    InvalidFamily,
    InvalidColor,
    InvalidPanelGrid,
    InvalidParameter,
    InvalidResolution,
    InvalidSurfaceRule,
    InvalidSurfaceOrder,
    InvalidMaterialHash,
};

struct CombatMaterialValidationResult {
    std::vector<CombatMaterialValidationError> errors;

    [[nodiscard]] bool isValid() const noexcept;
};

enum class CafeteriaMaterialValidationError : std::uint8_t {
    InvalidCondition,
    InvalidSurface,
    InvalidFamily,
    InvalidColor,
    InvalidPanelGrid,
    InvalidParameter,
    InvalidResolution,
    InvalidSurfaceRule,
    InvalidSurfaceOrder,
    InvalidMaterialHash,
};

struct CafeteriaMaterialValidationResult {
    std::vector<CafeteriaMaterialValidationError> errors;

    [[nodiscard]] bool isValid() const noexcept;
};

enum class LaboratoryMaterialValidationError : std::uint8_t {
    InvalidCondition,
    InvalidSurface,
    InvalidFamily,
    InvalidColor,
    InvalidPanelGrid,
    InvalidParameter,
    InvalidResolution,
    InvalidSurfaceRule,
    InvalidSurfaceOrder,
    InvalidMaterialHash,
};

struct LaboratoryMaterialValidationResult {
    std::vector<LaboratoryMaterialValidationError> errors;

    [[nodiscard]] bool isValid() const noexcept;
};

enum class TechnicalMaterialValidationError : std::uint8_t {
    InvalidCondition,
    InvalidSurface,
    InvalidFamily,
    InvalidColor,
    InvalidPanelGrid,
    InvalidParameter,
    InvalidResolution,
    InvalidSurfaceRule,
    InvalidSurfaceOrder,
    InvalidMaterialHash,
};

struct TechnicalMaterialValidationResult {
    std::vector<TechnicalMaterialValidationError> errors;

    [[nodiscard]] bool isValid() const noexcept;
};

enum class SecurityMaterialValidationError : std::uint8_t {
    InvalidCondition,
    InvalidSurface,
    InvalidFamily,
    InvalidColor,
    InvalidPanelGrid,
    InvalidParameter,
    InvalidResolution,
    InvalidSurfaceRule,
    InvalidSurfaceOrder,
    InvalidMaterialHash,
};

struct SecurityMaterialValidationResult {
    std::vector<SecurityMaterialValidationError> errors;

    [[nodiscard]] bool isValid() const noexcept;
};

enum class CentralHubMaterialValidationError : std::uint8_t {
    InvalidCondition,
    InvalidRouteAxis,
    InvalidSurface,
    InvalidFamily,
    InvalidColor,
    InvalidPanelGrid,
    InvalidParameter,
    InvalidResolution,
    InvalidSurfaceRule,
    InvalidSurfaceOrder,
    InvalidMaterialHash,
};

struct CentralHubMaterialValidationResult {
    std::vector<CentralHubMaterialValidationError> errors;

    [[nodiscard]] bool isValid() const noexcept;
};

[[nodiscard]] IndustrialRoomMaterialSet generateIndustrialRoomMaterials(
    gdp::core::Seed roomSeed);
[[nodiscard]] IndustrialMaterialValidationResult validateIndustrialSurfaceRecipe(
    const IndustrialSurfaceRecipe& recipe);
[[nodiscard]] IndustrialMaterialValidationResult validateIndustrialRoomMaterials(
    const IndustrialRoomMaterialSet& materials);
[[nodiscard]] std::uint64_t industrialSurfaceRecipeHash(
    const IndustrialSurfaceRecipe& recipe) noexcept;
[[nodiscard]] std::uint64_t industrialRoomMaterialHash(
    const IndustrialRoomMaterialSet& materials) noexcept;
[[nodiscard]] std::optional<wkc::render::ImageData> generateIndustrialSurfaceAlbedo(
    const IndustrialSurfaceRecipe& recipe);
[[nodiscard]] IndustrialSurfaceRecipe makeIndustrialWallArchetypeRecipe(
    IndustrialCondition condition);
[[nodiscard]] std::optional<IndustrialWallMaterialImages>
generateIndustrialWallMaterialImages(const IndustrialSurfaceRecipe& recipe);
[[nodiscard]] IndustrialSurfaceRecipe makeIndustrialCeilingArchetypeRecipe(
    IndustrialCondition condition);
[[nodiscard]] std::optional<IndustrialCeilingMaterialImages>
generateIndustrialCeilingMaterialImages(const IndustrialSurfaceRecipe& recipe);

[[nodiscard]] StorageRoomMaterialSet generateStorageRoomMaterials(
    gdp::core::Seed roomSeed);
[[nodiscard]] StorageMaterialValidationResult validateStorageSurfaceRecipe(
    const StorageSurfaceRecipe& recipe);
[[nodiscard]] StorageMaterialValidationResult validateStorageRoomMaterials(
    const StorageRoomMaterialSet& materials);
[[nodiscard]] std::uint64_t storageSurfaceRecipeHash(
    const StorageSurfaceRecipe& recipe) noexcept;
[[nodiscard]] std::uint64_t storageRoomMaterialHash(
    const StorageRoomMaterialSet& materials) noexcept;
[[nodiscard]] std::optional<wkc::render::ImageData> generateStorageSurfaceAlbedo(
    const StorageSurfaceRecipe& recipe);

[[nodiscard]] CombatRoomMaterialSet generateCombatRoomMaterials(
    gdp::core::Seed roomSeed);
[[nodiscard]] CombatMaterialValidationResult validateCombatSurfaceRecipe(
    const CombatSurfaceRecipe& recipe);
[[nodiscard]] CombatMaterialValidationResult validateCombatRoomMaterials(
    const CombatRoomMaterialSet& materials);
[[nodiscard]] std::uint64_t combatSurfaceRecipeHash(
    const CombatSurfaceRecipe& recipe) noexcept;
[[nodiscard]] std::uint64_t combatRoomMaterialHash(
    const CombatRoomMaterialSet& materials) noexcept;
[[nodiscard]] std::optional<wkc::render::ImageData> generateCombatSurfaceAlbedo(
    const CombatSurfaceRecipe& recipe);

[[nodiscard]] CafeteriaRoomMaterialSet generateCafeteriaRoomMaterials(
    gdp::core::Seed roomSeed);
[[nodiscard]] CafeteriaMaterialValidationResult validateCafeteriaSurfaceRecipe(
    const CafeteriaSurfaceRecipe& recipe);
[[nodiscard]] CafeteriaMaterialValidationResult validateCafeteriaRoomMaterials(
    const CafeteriaRoomMaterialSet& materials);
[[nodiscard]] std::uint64_t cafeteriaSurfaceRecipeHash(
    const CafeteriaSurfaceRecipe& recipe) noexcept;
[[nodiscard]] std::uint64_t cafeteriaRoomMaterialHash(
    const CafeteriaRoomMaterialSet& materials) noexcept;
[[nodiscard]] std::optional<wkc::render::ImageData> generateCafeteriaSurfaceAlbedo(
    const CafeteriaSurfaceRecipe& recipe);

[[nodiscard]] LaboratoryRoomMaterialSet generateLaboratoryRoomMaterials(
    gdp::core::Seed roomSeed);
[[nodiscard]] LaboratoryMaterialValidationResult validateLaboratorySurfaceRecipe(
    const LaboratorySurfaceRecipe& recipe);
[[nodiscard]] LaboratoryMaterialValidationResult validateLaboratoryRoomMaterials(
    const LaboratoryRoomMaterialSet& materials);
[[nodiscard]] std::uint64_t laboratorySurfaceRecipeHash(
    const LaboratorySurfaceRecipe& recipe) noexcept;
[[nodiscard]] std::uint64_t laboratoryRoomMaterialHash(
    const LaboratoryRoomMaterialSet& materials) noexcept;
[[nodiscard]] std::optional<wkc::render::ImageData> generateLaboratorySurfaceAlbedo(
    const LaboratorySurfaceRecipe& recipe);

[[nodiscard]] TechnicalRoomMaterialSet generateTechnicalRoomMaterials(
    gdp::core::Seed roomSeed);
[[nodiscard]] TechnicalMaterialValidationResult validateTechnicalSurfaceRecipe(
    const TechnicalSurfaceRecipe& recipe);
[[nodiscard]] TechnicalMaterialValidationResult validateTechnicalRoomMaterials(
    const TechnicalRoomMaterialSet& materials);
[[nodiscard]] std::uint64_t technicalSurfaceRecipeHash(
    const TechnicalSurfaceRecipe& recipe) noexcept;
[[nodiscard]] std::uint64_t technicalRoomMaterialHash(
    const TechnicalRoomMaterialSet& materials) noexcept;
[[nodiscard]] std::optional<wkc::render::ImageData> generateTechnicalSurfaceAlbedo(
    const TechnicalSurfaceRecipe& recipe);

[[nodiscard]] SecurityRoomMaterialSet generateSecurityRoomMaterials(
    gdp::core::Seed roomSeed);
[[nodiscard]] SecurityMaterialValidationResult validateSecuritySurfaceRecipe(
    const SecuritySurfaceRecipe& recipe);
[[nodiscard]] SecurityMaterialValidationResult validateSecurityRoomMaterials(
    const SecurityRoomMaterialSet& materials);
[[nodiscard]] std::uint64_t securitySurfaceRecipeHash(
    const SecuritySurfaceRecipe& recipe) noexcept;
[[nodiscard]] std::uint64_t securityRoomMaterialHash(
    const SecurityRoomMaterialSet& materials) noexcept;
[[nodiscard]] std::optional<wkc::render::ImageData> generateSecuritySurfaceAlbedo(
    const SecuritySurfaceRecipe& recipe);

[[nodiscard]] CentralHubRoomMaterialSet generateCentralHubRoomMaterials(
    gdp::core::Seed roomSeed);
[[nodiscard]] CentralHubMaterialValidationResult validateCentralHubSurfaceRecipe(
    const CentralHubSurfaceRecipe& recipe);
[[nodiscard]] CentralHubMaterialValidationResult validateCentralHubRoomMaterials(
    const CentralHubRoomMaterialSet& materials);
[[nodiscard]] std::uint64_t centralHubSurfaceRecipeHash(
    const CentralHubSurfaceRecipe& recipe) noexcept;
[[nodiscard]] std::uint64_t centralHubRoomMaterialHash(
    const CentralHubRoomMaterialSet& materials) noexcept;
[[nodiscard]] std::optional<wkc::render::ImageData> generateCentralHubSurfaceAlbedo(
    const CentralHubSurfaceRecipe& recipe);

} // namespace gdp::game::visual
