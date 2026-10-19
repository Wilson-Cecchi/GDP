#pragma once

#include "core/Math.hpp"
#include "core/Seed.hpp"
#include "gameplay/Language.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <span>
#include <vector>

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
    DroppedPistol,
    Special,
    DroppedSniper,
    DroppedShotgun,
    DroppedSubmachineGun,
};

enum class HudWeaponRole : std::uint8_t {
    Pistol,
    Carbine,
    Knife,
    Sniper,
    Shotgun,
    SubmachineGun,
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

// An actual hit or shot starts the visual pulse. This is separate from
// EnemyAnimationState::Attack, which also describes aiming while on cooldown.
enum class EnemyAttackVisualKind : std::uint8_t {
    None,
    Ranged,
    Melee,
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
    EnemyVisualRole role = EnemyVisualRole::Worker,
    EnemyAttackVisualKind attackKind = EnemyAttackVisualKind::None,
    float attackPulse = 0.0F) noexcept;

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
    EnemyAttackVisualKind attackVisualKind{EnemyAttackVisualKind::None};
    float attackPulse{0.0F};
};

struct PickupRenderData {
    wkc::core::Vec3 position{};
    PickupVisualRole role{PickupVisualRole::Health};
    wkc::core::Seed visualSeed{0U};
    float fallProgress{1.0F};
    float baseYawDegrees{0.0F};
};

struct DroppedWeaponPose {
    float yawDegrees{0.0F};
    float rollDegrees{90.0F};
    float height{0.0F};

    constexpr bool operator==(const DroppedWeaponPose&) const noexcept = default;
};

[[nodiscard]] DroppedWeaponPose sampleDroppedWeaponPose(
    wkc::core::Seed visualSeed, PickupVisualRole role, float progress) noexcept;

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

struct EnemyProjectileRenderData {
    wkc::core::Vec3 position{};
    wkc::core::Vec3 direction{0.0F, 0.0F, 1.0F};
};

struct GrenadeRenderData {
    wkc::core::Vec3 position{};
};

struct GrenadeBlastRenderData {
    wkc::core::Vec3 position{};
    float strength{0.0F};
};

[[nodiscard]] wkc::core::Vec3 impactVisualColor(
    ImpactVisualMaterial material,
    float strength = 1.0F) noexcept;
[[nodiscard]] float impactVisualSize(float strength) noexcept;

struct PlayerBodyRenderData {
    bool visible{false};
    wkc::core::Vec3 feetPosition{};
    float yawDegrees{-90.0F};
    float crouchAmount{0.0F};
    float gaitPhase{0.0F};
    float gaitWeight{0.0F};
    float forwardMovement{0.0F};
    float rightMovement{0.0F};
    float airborneStrength{0.0F};
};

struct WeaponViewmodelRenderData {
    bool visible{false};
    bool grenadeSelected{false};
    bool grenadePriming{false};
    float grenadePinProgress{0.0F};
    float grenadeChargeStrength{0.0F};
    float grenadeThrowStrength{0.0F};
    bool grenadeVisible{true};
    HudWeaponRole role{HudWeaponRole::Pistol};
    float recoilStrength{0.0F};
    float reloadProgress{0.0F};
    float equipStrength{0.0F};
    float meleeStrength{0.0F};
    float crouchStrength{0.0F};
    float airborneStrength{0.0F};
    float muzzleFlashStrength{0.0F};
    float gaitPhase{0.0F};
    float gaitWeight{0.0F};
    float carryMotion{0.0F};
    bool equipTransitionActive{false};
    HudWeaponRole equipOutgoingRole{HudWeaponRole::Pistol};
    bool equipOutgoingGrenade{false};
    bool equipOutgoingVisible{true};
    float equipTransitionProgress{1.0F};
    bool overdrive{false};
    wkc::core::Seed visualSeed{0U};
    std::uint32_t reloadRoundCount{6U};
    float aimStrength{0.0F};
    // Fixture/diagnostic switch; gameplay always supplies the approved hands.
    bool handsVisible{true};
};

struct WeaponEquipPresentation {
    WeaponViewmodelRenderData weapon{};
    bool visible{true};
};

[[nodiscard]] WeaponEquipPresentation sampleWeaponEquipPresentation(
    const WeaponViewmodelRenderData& weapon) noexcept;

// Grenade and grip share weapon-local coordinates, including the throw wind-up.
inline constexpr wkc::core::Vec3 grenadeViewmodelBodyCenter{0.04F, 0.105F, 0.10F};
inline constexpr wkc::core::Vec3 grenadeViewmodelGrip{0.055F, 0.045F, 0.075F};
inline constexpr float grenadeViewmodelChargeTravel = 0.14F;

// Presentation only: no camera, input, damage or ammo ownership.
[[nodiscard]] float sniperScopeStrength(const WeaponViewmodelRenderData& weapon) noexcept;

struct WeaponViewmodelPose {
    wkc::core::Vec3 offset{};
    wkc::core::Vec3 rotationDegrees{};

    constexpr bool operator==(const WeaponViewmodelPose&) const noexcept = default;
};

struct WeaponReloadPose {
    WeaponViewmodelPose weapon{};
    wkc::core::Vec3 magazineOffset{};
    wkc::core::Vec3 mechanismOffset{};
    wkc::core::Vec3 mechanismRotationDegrees{};
    bool magazineVisible{true};
    wkc::core::Vec3 discardedMagazineOffset{};
    wkc::core::Vec3 discardedMagazineRotationDegrees{};
    bool discardedMagazineVisible{false};

    constexpr bool operator==(const WeaponReloadPose&) const noexcept = default;
};

[[nodiscard]] WeaponReloadPose sampleWeaponReloadPose(
    HudWeaponRole role, float progress, std::uint32_t roundCount = 6U) noexcept;

[[nodiscard]] WeaponViewmodelPose sampleWeaponViewmodelPose(
    const WeaponViewmodelRenderData& weapon) noexcept;
// Coordinates are camera right/up/forward. Shared by weapon and arm anchors.
[[nodiscard]] wkc::core::Mat4 sampleWeaponPresentationTransform(
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
    ClearBossEncounter,
    Victory,
    Defeat,
    TestArena,
    ActivateExit,
    SecureArena,
    RecoverKey,
    ActivateTerminal,
    ClearAmbush,
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
    Restock,
    Reload,
    ExitTerminalLocked,
    NextFloorTerminal,
    FinishCampaignTerminal,
    CampaignMissionRequired,
};

enum class HudHitFeedback : std::uint8_t {
    None,
    Hit,
    Kill,
    WeakHit,
    WeakKill,
};

enum class HudNotice : std::uint8_t {
    None,
    Pickup,
    EventStart,
    Wave,
    EventClear,
    LevelUp,
    WeaponDropped,
    WeaponDropBlocked,
};

enum class HudPauseSelection : std::uint8_t {
    Resume,
    RestartRun,
    Settings,
    Controls,
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
    Controls,
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
    Language,
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

struct TacticalHudLayout {
    UiRect health{};
    UiRect weapon{};
};
[[nodiscard]] TacticalHudLayout makeTacticalHudLayout(int width, int height) noexcept;

struct MainMenuLayout {
    int scale{1};
    UiRect panel{};
    UiRect startButton{};
    UiRect continueButton{};
    UiRect campaignButton{};
    UiRect speedrunButton{};
    UiRect bestSeedButton{};
    UiRect settingsButton{};
    UiRect controlsButton{};
    UiRect exitButton{};
};

struct PauseMenuLayout {
    int scale{1};
    UiRect panel{};
    UiRect resumeButton{};
    UiRect restartButton{};
    UiRect settingsButton{};
    UiRect controlsButton{};
    UiRect mainMenuButton{};
    UiRect exitButton{};
};

struct ListMenuLayout {
    int scale{1};
    UiRect panel{};
    std::vector<UiRect> rows;
};
struct ListMenuRenderData {
    bool active{false};
    bool titleScreen{false};
    std::string title;
    std::string subtitle;
    std::vector<std::string> rows;
    std::vector<bool> enabled;
    std::vector<bool> adjustable;
    std::vector<bool> rawText;
    std::size_t selection{0U};
};
[[nodiscard]] ListMenuLayout makeListMenuLayout(int width, int height, std::size_t count,
    bool titleScreen = false);
[[nodiscard]] std::optional<std::size_t> listMenuSelectionAt(int width, int height,
    std::size_t count, float pointerX, float pointerY, bool titleScreen = false);
[[nodiscard]] std::optional<int> listMenuAdjustmentDirectionAt(int width, int height,
    std::size_t count, float pointerX, float pointerY, bool titleScreen = false);

struct ControlsMenuLayout {
    int scale{1};
    UiRect panel{};
    UiRect backButton{};
};

[[nodiscard]] ControlsMenuLayout makeControlsMenuLayout(
    int width, int height) noexcept;
[[nodiscard]] bool controlsMenuBackAt(
    int width, int height, float pointerX, float pointerY) noexcept;

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

inline constexpr std::size_t maximumHudMapRooms = 32U;
inline constexpr std::size_t maximumHudMapLinks = 64U;
inline constexpr std::size_t maximumHudMapCorridorPoints = 8U;

enum class HudMapMarker : std::uint8_t {
    None,
    Mission,
    Boss,
    Exit,
};

struct HudMapRoom {
    std::uint32_t id{0U};
    wkc::core::Vec2 center{};
    bool known{false};
    bool visited{false};
    bool current{false};
    wkc::core::Vec2 minimum{};
    wkc::core::Vec2 maximum{};
    HudMapMarker marker{HudMapMarker::None};
};

struct HudMapLink {
    std::uint8_t first{0U};
    std::uint8_t second{0U};
    bool known{false};
    std::array<wkc::core::Vec2, maximumHudMapCorridorPoints> points{};
    std::uint8_t pointCount{0U};
};

struct HudMapData {
    bool visible{false};
    std::array<HudMapRoom, maximumHudMapRooms> rooms{};
    std::size_t roomCount{0U};
    std::array<HudMapLink, maximumHudMapLinks> links{};
    std::size_t linkCount{0U};
    wkc::core::Vec2 playerPosition{};
    wkc::core::Vec2 playerForward{0.0F, -1.0F};
};

enum class HudBossPhase : std::uint8_t {
    LongRange,
    Pressure,
    Pursuit,
};

struct HudBossData {
    bool visible{false};
    float health{0.0F};
    float maximumHealth{0.0F};
    HudBossPhase phase{HudBossPhase::LongRange};
};

struct HudRenderData {
    ObjectiveRenderState objective{ObjectiveRenderState::None};
    bool visible{false};
    bool flashlightOn{false};
    std::string interactBinding{"E"};
    std::string reloadBinding{"R"};
    std::string flashlightBinding{"F"};
    bool infoOverlay{false};
    HudMapData map{};
    HudBossData boss{};
    std::uint64_t runSeed{0U};
    std::uint64_t score{0U};
    std::uint32_t enemiesKilled{0U};
    std::uint32_t eventsCompleted{0U};
    std::uint32_t lootCollected{0U};
    float health{100.0F};
    float maximumHealth{100.0F};
    float armor{0.0F};
    float maximumArmor{50.0F};
    std::uint32_t magazineAmmo{0};
    std::uint32_t reserveAmmo{0};
    std::uint32_t grenades{2U};
    bool grenadeSelected{false};
    bool grenadePriming{false};
    float grenadeChargeStrength{0.0F};
    float grenadeFuseRemainingSeconds{0.0F};
    HudWeaponRole weapon{HudWeaponRole::Pistol};
    bool weaponOverdrive{false};
    float weaponSpread{0.0F};
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
    ObjectiveRenderState campaignMission{ObjectiveRenderState::None};
    bool campaignRouteMission{false};
    bool campaignKeyRecovered{false};
    bool campaignArenaSecured{false};
    bool campaignMissionCompleted{false};
    bool campaignBossDefeated{false};
    bool campaignExitReady{false};
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
    ListMenuRenderData listMenu{};
    HudMainMenuSelection mainMenuSelection{HudMainMenuSelection::StartRun};
    bool campaignContinueAvailable{false};
    std::uint32_t campaignContinueFloor{1U};
    bool bestSeedAvailable{false};
    HudResultMenuSelection resultMenuSelection{HudResultMenuSelection::PlayAgain};
    bool exitConfirmation{false};
    HudExitConfirmationSelection exitConfirmationSelection{
        HudExitConfirmationSelection::Cancel};
    bool settingsMenu{false};
    bool controlsMenu{false};
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
    std::uint64_t performanceTextureUnitActivations{0U};
    std::uint64_t performanceTextureBinds{0U};
    std::uint64_t performanceTextureBindSkips{0U};
    std::uint64_t performanceMeshBinds{0U};
    std::uint64_t performanceMeshBindSkips{0U};
    bool performanceLowSpecShader{false};
    std::uint64_t performanceBackfaceCulledDraws{0U};
    std::uint64_t performanceCullStateChanges{0U};
    std::uint64_t performanceInstancedDrawCalls{0U};
    std::uint64_t performanceRenderedInstances{0U};
    std::uint64_t performanceAvoidedDrawCalls{0U};
    std::uint32_t performanceShadowShaderOccluderCapacity{0U};
    gameplay::Language language{gameplay::Language::English};
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
    bool flashlightEnabled{false};
    wkc::core::Vec3 flashlightPosition{};
    wkc::core::Vec3 flashlightDirection{0.0F, 0.0F, -1.0F};
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
