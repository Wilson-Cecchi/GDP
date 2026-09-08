#pragma once

#include "core/Math.hpp"
#include "core/Seed.hpp"
#include "procedural/AssetRecipe.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace gdp::game::visual {

using WorkerPartId = std::uint32_t;
using CompatibilityMask = std::uint32_t;

inline constexpr CompatibilityMask compatibilityLight = 1U << 0U;
inline constexpr CompatibilityMask compatibilityStandard = 1U << 1U;
inline constexpr CompatibilityMask compatibilityHeavy = 1U << 2U;
inline constexpr CompatibilityMask compatibilityMechanical = 1U << 3U;

enum class WorkerBodyProfileId : std::uint8_t {
    Standard,
    Compact,
    Lanky,
    Heavy,
    Count,
};

enum class WorkerPartSlot : std::uint8_t {
    Torso,
    Head,
    Arm,
    Leg,
    Back,
    Tool,
    Count,
};

enum class SocketType : std::uint8_t {
    Root,
    Neck,
    ShoulderLeft,
    ShoulderRight,
    HipLeft,
    HipRight,
    Back,
    Tool,
};

struct SocketDefinition {
    SocketType type{SocketType::Root};
    wkc::core::Vec3 position{};
    wkc::core::Vec3 direction{0.0F, 1.0F, 0.0F};
    float radius{0.1F};
    CompatibilityMask compatibility{compatibilityStandard};
};

struct BodyProfile {
    WorkerBodyProfileId id{WorkerBodyProfileId::Standard};
    float heightScale{1.0F};
    float torsoWidth{0.38F};
    float torsoDepth{0.26F};
    float shoulderWidth{0.58F};
    float armLength{0.40F};
    float legLength{0.42F};
    float headScale{0.15F};
    float forwardLean{0.0F};
    float leftMassBias{1.0F};
    float rightMassBias{1.0F};
    CompatibilityMask compatibility{compatibilityStandard};
    std::vector<SocketDefinition> sockets;
};

struct WorkerPartDefinition {
    WorkerPartId id{0};
    WorkerPartSlot slot{WorkerPartSlot::Torso};
    SocketType attachment{SocketType::Neck};
    CompatibilityMask compatibility{compatibilityStandard};
    wkc::procedural::PrimitiveType primitive{wkc::procedural::PrimitiveType::Box};
    wkc::core::Vec3 dimensionScale{1.0F, 1.0F, 1.0F};
    wkc::procedural::MaterialSlot material{wkc::procedural::MaterialSlot::Primary};
    wkc::procedural::SemanticRegion semantic{wkc::procedural::SemanticRegion::Generic};
    std::uint32_t weight{1};
};

enum class WearCondition : std::uint8_t {
    Clean,
    Used,
    Worn,
    Damaged,
};

struct WearProfile {
    WearCondition condition{WearCondition::Used};
    float wear{0.2F};
    float dirt{0.15F};
    float scratch{0.1F};
    float oxidation{0.05F};
};

struct WorkerMaterialSet {
    wkc::procedural::MaterialFamily family{wkc::procedural::MaterialFamily::PaintedSteel};
    wkc::core::Vec3 primaryColor{0.72F, 0.38F, 0.12F};
    wkc::core::Vec3 secondaryColor{0.34F, 0.20F, 0.10F};
};

struct WorkerVisualGenome {
    wkc::core::Seed seed{0};
    WorkerBodyProfileId bodyProfile{WorkerBodyProfileId::Standard};
    WorkerPartId torso{0};
    WorkerPartId head{0};
    WorkerPartId leftArm{0};
    WorkerPartId rightArm{0};
    WorkerPartId leftLeg{0};
    WorkerPartId rightLeg{0};
    WorkerPartId back{0};
    WorkerPartId tool{0};
    std::uint64_t visualHash{0};
};

struct WorkerVisual {
    WorkerVisualGenome genome;
    BodyProfile body;
    WorkerMaterialSet materials;
    WearProfile wear;
    wkc::procedural::MeshRecipe meshRecipe;
    wkc::procedural::TextureRecipe textureRecipe;
    std::uint32_t fallbackCount{0};
};

struct EnemyRigidPartRecipe {
    std::uint32_t index{0};
    wkc::core::Vec3 pivot{};
    wkc::procedural::MeshRecipe meshRecipe;
};

[[nodiscard]] std::vector<EnemyRigidPartRecipe> splitEnemyRigidParts(
    const wkc::procedural::MeshRecipe& assembledRecipe);

enum class WorkerVisualValidationError : std::uint8_t {
    InvalidBodyProfile,
    MissingSocket,
    MissingPart,
    WrongPartSlot,
    IncompatiblePart,
    InvalidMeshRecipe,
    InvalidTextureRecipe,
    InvalidBounds,
    InvalidVisualHash,
};

struct WorkerVisualValidationResult {
    std::vector<WorkerVisualValidationError> errors;

    [[nodiscard]] bool isValid() const noexcept;
};

enum class GuardBodyProfileId : std::uint8_t {
    Patrol,
    Tactical,
    Armored,
    Count,
};

using GuardPartId = std::uint32_t;
using GuardPartDefinition = WorkerPartDefinition;

struct GuardVisualGenome {
    wkc::core::Seed seed{0};
    GuardBodyProfileId bodyProfile{GuardBodyProfileId::Patrol};
    GuardPartId torso{0};
    GuardPartId head{0};
    GuardPartId leftArm{0};
    GuardPartId rightArm{0};
    GuardPartId leftLeg{0};
    GuardPartId rightLeg{0};
    GuardPartId back{0};
    GuardPartId weapon{0};
    std::uint64_t visualHash{0};
};

struct GuardVisual {
    GuardVisualGenome genome;
    BodyProfile body;
    WorkerMaterialSet materials;
    WearProfile wear;
    wkc::procedural::MeshRecipe meshRecipe;
    wkc::procedural::TextureRecipe textureRecipe;
    std::uint32_t fallbackCount{0};
};

enum class GuardVisualValidationError : std::uint8_t {
    InvalidBodyProfile,
    MissingSocket,
    MissingPart,
    WrongPartSlot,
    IncompatiblePart,
    InvalidMeshRecipe,
    InvalidTextureRecipe,
    InvalidBounds,
    InvalidWeapon,
    InvalidVisualHash,
};

struct GuardVisualValidationResult {
    std::vector<GuardVisualValidationError> errors;

    [[nodiscard]] bool isValid() const noexcept;
};

enum class OfficerBodyProfileId : std::uint8_t {
    Field,
    Command,
    Veteran,
    Count,
};

using OfficerPartId = std::uint32_t;
using OfficerPartDefinition = WorkerPartDefinition;

struct OfficerVisualGenome {
    wkc::core::Seed seed{0};
    OfficerBodyProfileId bodyProfile{OfficerBodyProfileId::Field};
    OfficerPartId torso{0};
    OfficerPartId head{0};
    OfficerPartId leftArm{0};
    OfficerPartId rightArm{0};
    OfficerPartId leftLeg{0};
    OfficerPartId rightLeg{0};
    OfficerPartId commandModule{0};
    OfficerPartId sidearm{0};
    std::uint64_t visualHash{0};
};

struct OfficerVisual {
    OfficerVisualGenome genome;
    BodyProfile body;
    WorkerMaterialSet materials;
    WearProfile wear;
    wkc::procedural::MeshRecipe meshRecipe;
    wkc::procedural::TextureRecipe textureRecipe;
    std::uint32_t fallbackCount{0};
};

enum class OfficerVisualValidationError : std::uint8_t {
    InvalidBodyProfile,
    MissingSocket,
    MissingPart,
    WrongPartSlot,
    IncompatiblePart,
    InvalidMeshRecipe,
    InvalidTextureRecipe,
    InvalidBounds,
    InvalidSidearm,
    InvalidCommandModule,
    InvalidVisualHash,
};

struct OfficerVisualValidationResult {
    std::vector<OfficerVisualValidationError> errors;

    [[nodiscard]] bool isValid() const noexcept;
};

enum class EngineerBodyProfileId : std::uint8_t {
    Maintenance,
    Diagnostics,
    Hazmat,
    Count,
};

using EngineerPartId = std::uint32_t;
using EngineerPartDefinition = WorkerPartDefinition;

struct EngineerVisualGenome {
    wkc::core::Seed seed{0};
    EngineerBodyProfileId bodyProfile{EngineerBodyProfileId::Maintenance};
    EngineerPartId torso{0};
    EngineerPartId head{0};
    EngineerPartId leftArm{0};
    EngineerPartId rightArm{0};
    EngineerPartId leftLeg{0};
    EngineerPartId rightLeg{0};
    EngineerPartId diagnosticPack{0};
    EngineerPartId repairTool{0};
    std::uint64_t visualHash{0};
};

struct EngineerVisual {
    EngineerVisualGenome genome;
    BodyProfile body;
    WorkerMaterialSet materials;
    WearProfile wear;
    wkc::procedural::MeshRecipe meshRecipe;
    wkc::procedural::TextureRecipe textureRecipe;
    std::uint32_t fallbackCount{0};
};

enum class EngineerVisualValidationError : std::uint8_t {
    InvalidBodyProfile,
    MissingSocket,
    MissingPart,
    WrongPartSlot,
    IncompatiblePart,
    InvalidMeshRecipe,
    InvalidTextureRecipe,
    InvalidBounds,
    InvalidRepairTool,
    InvalidDiagnosticPack,
    InvalidVisualHash,
};

struct EngineerVisualValidationResult {
    std::vector<EngineerVisualValidationError> errors;

    [[nodiscard]] bool isValid() const noexcept;
};

enum class IndustrialRobotBodyProfileId : std::uint8_t {
    Sentry,
    Foundry,
    Siege,
    Count,
};

using IndustrialRobotPartId = std::uint32_t;
using IndustrialRobotPartDefinition = WorkerPartDefinition;

struct IndustrialRobotVisualGenome {
    wkc::core::Seed seed{0};
    IndustrialRobotBodyProfileId bodyProfile{IndustrialRobotBodyProfileId::Sentry};
    IndustrialRobotPartId torsoCore{0};
    IndustrialRobotPartId sensorHead{0};
    IndustrialRobotPartId leftArm{0};
    IndustrialRobotPartId rightArm{0};
    IndustrialRobotPartId leftLeg{0};
    IndustrialRobotPartId rightLeg{0};
    IndustrialRobotPartId leftReactor{0};
    IndustrialRobotPartId rightReactor{0};
    IndustrialRobotPartId heavyWeapon{0};
    std::uint64_t visualHash{0};
};

struct IndustrialRobotVisual {
    IndustrialRobotVisualGenome genome;
    BodyProfile body;
    WorkerMaterialSet materials;
    WearProfile wear;
    wkc::procedural::MeshRecipe meshRecipe;
    wkc::procedural::TextureRecipe textureRecipe;
    std::uint32_t fallbackCount{0};
};

enum class IndustrialRobotVisualValidationError : std::uint8_t {
    InvalidBodyProfile,
    MissingSocket,
    MissingPart,
    WrongPartSlot,
    IncompatiblePart,
    InvalidMeshRecipe,
    InvalidTextureRecipe,
    InvalidBounds,
    InvalidTorsoCore,
    InvalidSensorHead,
    InvalidReactorModules,
    InvalidHeavyArms,
    InvalidHeavyWeapon,
    InvalidEmissiveCore,
    InvalidVisualHash,
};

struct IndustrialRobotVisualValidationResult {
    std::vector<IndustrialRobotVisualValidationError> errors;

    [[nodiscard]] bool isValid() const noexcept;
};

[[nodiscard]] std::span<const WorkerPartDefinition> workerPartLibrary() noexcept;
[[nodiscard]] WorkerVisual generateWorkerVisual(wkc::core::Seed visualSeed);
[[nodiscard]] WorkerVisualValidationResult validateWorkerVisual(const WorkerVisual& visual);
[[nodiscard]] std::uint64_t workerVisualHash(const WorkerVisual& visual) noexcept;
[[nodiscard]] std::span<const GuardPartDefinition> guardPartLibrary() noexcept;
[[nodiscard]] GuardVisual generateGuardVisual(wkc::core::Seed visualSeed);
[[nodiscard]] GuardVisualValidationResult validateGuardVisual(const GuardVisual& visual);
[[nodiscard]] std::uint64_t guardVisualHash(const GuardVisual& visual) noexcept;
[[nodiscard]] std::span<const OfficerPartDefinition> officerPartLibrary() noexcept;
[[nodiscard]] OfficerVisual generateOfficerVisual(wkc::core::Seed visualSeed);
[[nodiscard]] OfficerVisualValidationResult validateOfficerVisual(const OfficerVisual& visual);
[[nodiscard]] std::uint64_t officerVisualHash(const OfficerVisual& visual) noexcept;
[[nodiscard]] std::span<const EngineerPartDefinition> engineerPartLibrary() noexcept;
[[nodiscard]] EngineerVisual generateEngineerVisual(wkc::core::Seed visualSeed);
[[nodiscard]] EngineerVisualValidationResult validateEngineerVisual(const EngineerVisual& visual);
[[nodiscard]] std::uint64_t engineerVisualHash(const EngineerVisual& visual) noexcept;
[[nodiscard]] std::span<const IndustrialRobotPartDefinition>
industrialRobotPartLibrary() noexcept;
[[nodiscard]] IndustrialRobotVisual generateIndustrialRobotVisual(wkc::core::Seed visualSeed);
[[nodiscard]] IndustrialRobotVisualValidationResult
validateIndustrialRobotVisual(const IndustrialRobotVisual& visual);
[[nodiscard]] std::uint64_t
industrialRobotVisualHash(const IndustrialRobotVisual& visual) noexcept;

} // namespace gdp::game::visual
