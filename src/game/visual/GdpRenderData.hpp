#pragma once

#include "core/Math.hpp"
#include "core/Seed.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <span>

namespace gdp::game::visual {

struct CameraData {
    wkc::core::Vec3 position{0.0F, 1.62F, 4.0F};
    wkc::core::Vec3 forward{0.0F, 0.0F, -1.0F};
    float verticalFieldOfViewDegrees{68.0F};
};

enum class EnemyVisualRole : std::uint8_t {
    Worker,
    Guard,
    Officer,
    Engineer,
    IndustrialRobot,
};

enum class PickupVisualRole : std::uint8_t {
    Health,
    Ammo,
    Key,
    Armor,
    Weapon,
    Special,
};

enum class HudWeaponRole : std::uint8_t {
    Pistol,
    Carbine,
};

enum class PropVisualRole : std::uint8_t {
    Crate,
    Canister,
    Barricade,
};

enum class PropVisualState : std::uint8_t {
    Intact,
    Damaged,
    Destroyed,
};

enum class EnemyAnimationState : std::uint8_t {
    Idle,
    Walk,
    Attack,
    Hit,
    Dead,
    Alert,
    Recover,
};

struct EnemyRootPose {
    wkc::core::Vec3 offset{};
    wkc::core::Vec3 scale{1.0F, 1.0F, 1.0F};
    wkc::core::Vec3 rotationDegrees{};

    constexpr bool operator==(const EnemyRootPose&) const noexcept = default;
};

struct EnemyPartPose {
    wkc::core::Vec3 offset{};
    wkc::core::Vec3 rotationDegrees{};

    constexpr bool operator==(const EnemyPartPose&) const noexcept = default;
};

struct EnemyAnimationTransition {
    EnemyAnimationState previousState{EnemyAnimationState::Idle};
    EnemyAnimationState currentState{EnemyAnimationState::Idle};
    std::uint32_t elapsedTicks{0U};
    std::uint32_t durationTicks{0U};
    bool initialized{false};
};

void advanceEnemyAnimationTransition(
    EnemyAnimationTransition& transition,
    EnemyAnimationState requestedState,
    EnemyVisualRole role = EnemyVisualRole::Worker) noexcept;

[[nodiscard]] float enemyAnimationTransitionBlend(
    const EnemyAnimationTransition& transition) noexcept;

[[nodiscard]] EnemyRootPose blendEnemyRootPoses(
    const EnemyRootPose& previous,
    const EnemyRootPose& current,
    float blend) noexcept;

[[nodiscard]] EnemyPartPose blendEnemyPartPoses(
    const EnemyPartPose& previous,
    const EnemyPartPose& current,
    float blend) noexcept;

[[nodiscard]] EnemyRootPose sampleEnemyRagdollRootPose(
    const EnemyRootPose& previous,
    wkc::core::Seed visualSeed,
    EnemyVisualRole role,
    float progress) noexcept;

[[nodiscard]] EnemyPartPose sampleEnemyRagdollPartPose(
    const EnemyPartPose& previous,
    wkc::core::Seed visualSeed,
    EnemyVisualRole role,
    std::uint32_t partIndex,
    std::uint32_t partCount,
    float progress) noexcept;

[[nodiscard]] EnemyRootPose sampleEnemyRootPose(
    EnemyAnimationState state,
    std::uint64_t animationTick,
    wkc::core::Seed visualSeed,
    bool active = true,
    EnemyVisualRole role = EnemyVisualRole::Worker) noexcept;

[[nodiscard]] EnemyPartPose sampleEnemyPartPose(
    EnemyAnimationState state,
    std::uint64_t animationTick,
    wkc::core::Seed visualSeed,
    std::uint32_t partIndex,
    std::uint32_t partCount,
    float hitReactionStrength = 0.0F,
    bool active = true,
    EnemyVisualRole role = EnemyVisualRole::Worker) noexcept;

struct EnemyRenderData {
    wkc::core::Vec3 position{};
    bool alive{true};
    wkc::core::Seed visualSeed{0};
    EnemyVisualRole role{EnemyVisualRole::Worker};
    bool active{true};
    EnemyAnimationState animationState{EnemyAnimationState::Idle};
    std::uint64_t animationTick{0U};
    float yawDegrees{0.0F};
    float hitReactionStrength{0.0F};
    EnemyAnimationState previousAnimationState{EnemyAnimationState::Idle};
    float animationTransitionBlend{1.0F};
};

struct PickupRenderData {
    wkc::core::Vec3 position{};
    PickupVisualRole role{PickupVisualRole::Health};
};

struct PropRenderData {
    wkc::core::Vec3 position{};
    wkc::core::Vec3 scale{1.0F, 1.0F, 1.0F};
    PropVisualRole role{PropVisualRole::Crate};
    PropVisualState state{PropVisualState::Intact};
    wkc::core::Seed visualSeed{0};
};

enum class ImpactVisualMaterial : std::uint8_t {
    Metal,
    Composite,
    Flesh,
};

struct ImpactRenderData {
    wkc::core::Vec3 position{};
    wkc::core::Vec3 normal{0.0F, 0.0F, 1.0F};
    ImpactVisualMaterial material{ImpactVisualMaterial::Metal};
    float strength{1.0F};
    wkc::core::Seed visualSeed{0U};
};

[[nodiscard]] wkc::core::Vec3 impactVisualColor(
    ImpactVisualMaterial material,
    float strength = 1.0F) noexcept;
[[nodiscard]] float impactVisualSize(float strength) noexcept;

struct WeaponViewmodelRenderData {
    bool visible{false};
    HudWeaponRole role{HudWeaponRole::Pistol};
    float recoilStrength{0.0F};
    float reloadProgress{0.0F};
    float equipStrength{0.0F};
    float muzzleFlashStrength{0.0F};
    bool overdrive{false};
    wkc::core::Seed visualSeed{0U};
};

struct WeaponViewmodelPose {
    wkc::core::Vec3 offset{};
    wkc::core::Vec3 rotationDegrees{};

    constexpr bool operator==(const WeaponViewmodelPose&) const noexcept = default;
};

[[nodiscard]] WeaponViewmodelPose sampleWeaponViewmodelPose(
    const WeaponViewmodelRenderData& weapon) noexcept;

struct RoomRenderData {
    wkc::core::Vec3 origin{};
    wkc::core::QuarterTurn rotation{wkc::core::QuarterTurn::Degrees0};
    std::uint64_t materialKey{0};
    wkc::core::Aabb worldBounds{
        {-3.85F, 0.0F, -4.85F},
        {3.85F, 4.0F, 4.85F},
    };
};

struct DoorRenderData {
    wkc::core::Vec3 position{};
    wkc::core::Vec3 outwardNormal{0.0F, 0.0F, 1.0F};
    float width{1.2F};
    float height{2.2F};
    bool locked{false};
    float openFraction{1.0F};
};

struct CorridorRenderData {
    std::span<const wkc::core::Vec3> points{};
    float width{1.2F};
    float height{2.2F};
    std::uint64_t materialKey{0};
};

[[nodiscard]] bool isSpherePotentiallyVisible(
    const CameraData& camera,
    wkc::core::Vec3 center,
    float radius,
    float aspectRatio) noexcept;
[[nodiscard]] bool isAabbPotentiallyVisible(
    const CameraData& camera,
    const wkc::core::Aabb& bounds,
    float aspectRatio) noexcept;
[[nodiscard]] std::optional<wkc::core::Aabb> corridorRenderBounds(
    const CorridorRenderData& corridor) noexcept;

enum class ObjectiveRenderState : std::uint8_t {
    None,
    ReachBoss,
    DefeatBoss,
    Victory,
    Defeat,
};

enum class HudEventState : std::uint8_t {
    None,
    Dormant,
    Active,
    Completed,
};

enum class HudPrompt : std::uint8_t {
    None,
    OpenDoor,
    DoorLocked,
    UseTerminal,
    Reload,
};

enum class HudHitFeedback : std::uint8_t {
    None,
    Hit,
    Kill,
};

enum class HudNotice : std::uint8_t {
    None,
    Pickup,
    EventStart,
    Wave,
    EventClear,
    LevelUp,
};

enum class HudPauseSelection : std::uint8_t {
    Resume,
    RestartRun,
    Settings,
    MainMenu,
    ExitGame,
};

enum class HudMainMenuSelection : std::uint8_t {
    StartRun,
    ContinueCampaign,
    Campaign,
    Speedrun,
    BestSeed,
    Settings,
    ExitGame,
};

enum class HudResultMenuSelection : std::uint8_t {
    PlayAgain,
    ReplaySeed,
    MainMenu,
    ExitGame,
};

enum class HudExitConfirmationSelection : std::uint8_t {
    Cancel,
    ExitGame,
};

enum class HudSettingsSelection : std::uint8_t {
    MasterVolume,
    EffectsVolume,
    AmbientVolume,
    UiVolume,
    MouseSensitivity,
    InvertMouseY,
    FieldOfView,
    GraphicsQuality,
    WindowResolution,
    Fullscreen,
    VerticalSync,
    Back,
    Count,
};

enum class HudGraphicsQuality : std::uint8_t {
    Potato,
    Low,
    Medium,
    High,
    Ultra,
};

struct UiRect {
    int x{0};
    int y{0};
    int width{0};
    int height{0};

    [[nodiscard]] bool contains(int pointX, int pointY) const noexcept;
};

struct UiVisualTheme {
    wkc::core::Vec3 background{};
    wkc::core::Vec3 panel{};
    wkc::core::Vec3 panelBorder{};
    wkc::core::Vec3 selection{};
    wkc::core::Vec3 text{};
    wkc::core::Vec3 mutedText{};
    wkc::core::Vec3 accent{};
    wkc::core::Vec3 warning{};
    wkc::core::Vec3 danger{};
    wkc::core::Vec3 success{};
};

[[nodiscard]] UiVisualTheme industrialUiTheme() noexcept;

struct MainMenuLayout {
    int scale{1};
    UiRect panel{};
    UiRect startButton{};
    UiRect continueButton{};
    UiRect campaignButton{};
    UiRect speedrunButton{};
    UiRect bestSeedButton{};
    UiRect settingsButton{};
    UiRect exitButton{};
};

struct PauseMenuLayout {
    int scale{1};
    UiRect panel{};
    UiRect resumeButton{};
    UiRect restartButton{};
    UiRect settingsButton{};
    UiRect mainMenuButton{};
    UiRect exitButton{};
};

struct ResultMenuLayout {
    int scale{1};
    UiRect panel{};
    UiRect playAgainButton{};
    UiRect replaySeedButton{};
    UiRect mainMenuButton{};
    UiRect exitButton{};
};

struct ExitConfirmationLayout {
    int scale{1};
    UiRect panel{};
    UiRect cancelButton{};
    UiRect exitButton{};
};

struct SettingsMenuLayout {
    int scale{1};
    UiRect panel{};
    std::array<UiRect, static_cast<std::size_t>(HudSettingsSelection::Count)> rows{};
    std::array<UiRect, static_cast<std::size_t>(HudSettingsSelection::Count)>
        decrementButtons{};
    std::array<UiRect, static_cast<std::size_t>(HudSettingsSelection::Count)>
        incrementButtons{};
};

[[nodiscard]] MainMenuLayout makeMainMenuLayout(int width, int height) noexcept;
[[nodiscard]] std::optional<HudMainMenuSelection> mainMenuSelectionAt(
    int width,
    int height,
    float pointerX,
    float pointerY) noexcept;
[[nodiscard]] PauseMenuLayout makePauseMenuLayout(int width, int height) noexcept;
[[nodiscard]] std::optional<HudPauseSelection> pauseMenuSelectionAt(
    int width,
    int height,
    float pointerX,
    float pointerY) noexcept;
[[nodiscard]] ResultMenuLayout makeResultMenuLayout(int width, int height) noexcept;
[[nodiscard]] std::optional<HudResultMenuSelection> resultMenuSelectionAt(
    int width,
    int height,
    float pointerX,
    float pointerY) noexcept;
[[nodiscard]] ExitConfirmationLayout makeExitConfirmationLayout(
    int width,
    int height) noexcept;
[[nodiscard]] std::optional<HudExitConfirmationSelection>
exitConfirmationSelectionAt(
    int width,
    int height,
    float pointerX,
    float pointerY) noexcept;
[[nodiscard]] SettingsMenuLayout makeSettingsMenuLayout(int width, int height) noexcept;
[[nodiscard]] std::optional<HudSettingsSelection> settingsMenuSelectionAt(
    int width,
    int height,
    float pointerX,
    float pointerY) noexcept;
[[nodiscard]] std::optional<int> settingsMenuAdjustmentDirectionAt(
    int width,
    int height,
    float pointerX,
    float pointerY) noexcept;
[[nodiscard]] std::string runTimeText(std::uint64_t elapsedTicks);

struct HudRenderData {
    ObjectiveRenderState objective{ObjectiveRenderState::None};
    bool visible{false};
    float health{100.0F};
    float maximumHealth{100.0F};
    float armor{0.0F};
    float maximumArmor{50.0F};
    std::uint32_t magazineAmmo{0};
    std::uint32_t reserveAmmo{0};
    HudWeaponRole weapon{HudWeaponRole::Pistol};
    bool weaponOverdrive{false};
    std::uint32_t keys{0};
    std::uint32_t level{1};
    std::uint32_t experience{0};
    std::uint32_t experienceForNextLevel{0};
    bool runTimerVisible{false};
    std::uint64_t elapsedRunTicks{0U};
    bool campaignMode{false};
    std::uint32_t campaignFloor{1U};
    std::uint32_t campaignFloorCount{1U};
    std::uint16_t campaignThreatPercent{100U};
    bool campaignAdvanceAvailable{false};
    bool speedrunMode{false};
    bool speedrunRecordVisible{false};
    std::uint64_t bestSpeedrunTicks{0U};
    bool newSpeedrunRecord{false};
    HudEventState event{HudEventState::None};
    bool eventLocked{false};
    HudPrompt prompt{HudPrompt::None};
    float damageFeedback{0.0F};
    HudHitFeedback hitFeedback{HudHitFeedback::None};
    float hitFeedbackStrength{0.0F};
    HudNotice notice{HudNotice::None};
    float noticeFeedbackStrength{0.0F};
    bool paused{false};
    HudPauseSelection pauseSelection{HudPauseSelection::Resume};
    bool mainMenu{false};
    HudMainMenuSelection mainMenuSelection{HudMainMenuSelection::StartRun};
    bool campaignContinueAvailable{false};
    std::uint32_t campaignContinueFloor{1U};
    bool bestSeedAvailable{false};
    HudResultMenuSelection resultMenuSelection{HudResultMenuSelection::PlayAgain};
    bool exitConfirmation{false};
    HudExitConfirmationSelection exitConfirmationSelection{
        HudExitConfirmationSelection::Cancel};
    bool settingsMenu{false};
    HudSettingsSelection settingsSelection{HudSettingsSelection::MasterVolume};
    std::uint16_t masterVolumePercent{100U};
    std::uint16_t effectsVolumePercent{100U};
    std::uint16_t ambientVolumePercent{100U};
    std::uint16_t uiVolumePercent{100U};
    std::uint16_t mouseSensitivityPercent{100U};
    bool invertMouseY{false};
    std::uint16_t fieldOfViewDegrees{68U};
    HudGraphicsQuality graphicsQuality{HudGraphicsQuality::Medium};
    std::uint16_t worldRenderScalePercent{100U};
    bool detailedMaterials{true};
    std::uint16_t windowWidth{1280U};
    std::uint16_t windowHeight{720U};
    bool fullscreen{false};
    bool verticalSync{true};
    bool performanceOverlay{false};
    std::uint32_t performanceFramesPerSecond{0U};
    std::uint32_t averageFrameMilliseconds{0U};
    std::uint32_t percentile95FrameMilliseconds{0U};
    std::uint32_t worstFrameMilliseconds{0U};
    std::uint32_t averageUpdateMilliseconds{0U};
    std::uint32_t averageRenderMilliseconds{0U};
    std::uint32_t averagePresentMilliseconds{0U};
    std::uint32_t averageGpuMilliseconds{0U};
    bool gpuTimingAvailable{false};
    std::uint32_t framesOverBudgetPercent{0U};
    std::uint32_t worldGenerationMilliseconds{0U};
    std::uint32_t assetPreparationMilliseconds{0U};
    std::uint64_t performanceDrawCalls{0U};
    std::uint64_t performanceTriangles{0U};
    std::uint32_t performanceVisibleEnemies{0U};
    std::uint32_t performanceVisibleProps{0U};
    std::uint32_t performanceVisibleRooms{0U};
    std::uint32_t performanceActiveLights{0U};
    std::uint32_t performanceShadowLights{0U};
    std::uint32_t performanceShadowOccluders{0U};
    std::uint32_t performanceMeshCacheEntries{0U};
    std::uint32_t performanceTextureCacheEntries{0U};
    std::uint64_t performanceUniformUploads{0U};
    std::uint64_t performanceUniformSkips{0U};
};

inline constexpr std::size_t maxRenderedRoomLights = 8U;
inline constexpr std::size_t maxShadowOccluders = 16U;
inline constexpr std::size_t maxShadowCastingLights = 2U;
inline constexpr std::size_t maxActiveShadowOccluders = 8U;

struct ShadowOccluderRenderData {
    wkc::core::Aabb bounds{};

    constexpr bool operator==(const ShadowOccluderRenderData&) const noexcept
        = default;
};

struct PointLightRenderData {
    wkc::core::Vec3 position{};
    wkc::core::Vec3 color{1.0F, 1.0F, 1.0F};
    float intensity{1.0F};
    float range{6.0F};
    PropVisualState fixtureState{PropVisualState::Intact};
    float fixtureEmissionScale{1.0F};
};

struct RoomLightingRenderData {
    wkc::core::Vec3 ambientColor{0.70F, 0.78F, 0.88F};
    float ambientIntensity{0.24F};
    std::array<PointLightRenderData, maxRenderedRoomLights> lights{
        PointLightRenderData{{0.0F, 3.4F, 1.0F}, {0.78F, 0.88F, 1.0F}, 1.0F, 6.0F},
    };
    std::uint32_t lightCount{1U};
    std::array<ShadowOccluderRenderData, maxShadowOccluders> shadowOccluders{};
    std::uint32_t shadowOccluderCount{0U};
    std::uint32_t shadowLightCount{0U};
    float fixtureEmissive{0.75F};
    bool alert{false};
};

struct ShadowBudgetResult {
    std::uint32_t activeLights{0U};
    std::uint32_t candidateOccluders{0U};
    std::uint32_t selectedOccluders{0U};

    constexpr bool operator==(const ShadowBudgetResult&) const noexcept = default;
};

[[nodiscard]] ShadowBudgetResult optimizeShadowBudget(
    RoomLightingRenderData& lighting,
    wkc::core::Vec3 observerPosition,
    std::span<const ShadowOccluderRenderData> candidates) noexcept;

} // namespace gdp::game::visual
