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

struct EnemyRenderData {
    wkc::core::Vec3 position{};
    bool alive{true};
    wkc::core::Seed visualSeed{0};
    EnemyVisualRole role{EnemyVisualRole::Worker};
    bool active{true};
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
    WindowResolution,
    Fullscreen,
    VerticalSync,
    Back,
    Count,
};

struct UiRect {
    int x{0};
    int y{0};
    int width{0};
    int height{0};

    [[nodiscard]] bool contains(int pointX, int pointY) const noexcept;
};

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
    std::uint16_t windowWidth{1280U};
    std::uint16_t windowHeight{720U};
    bool fullscreen{false};
    bool verticalSync{true};
};

inline constexpr std::size_t maxRenderedRoomLights = 8U;

struct PointLightRenderData {
    wkc::core::Vec3 position{};
    wkc::core::Vec3 color{1.0F, 1.0F, 1.0F};
    float intensity{1.0F};
    float range{6.0F};
};

struct RoomLightingRenderData {
    wkc::core::Vec3 ambientColor{0.70F, 0.78F, 0.88F};
    float ambientIntensity{0.24F};
    std::array<PointLightRenderData, maxRenderedRoomLights> lights{
        PointLightRenderData{{0.0F, 3.4F, 1.0F}, {0.78F, 0.88F, 1.0F}, 1.0F, 6.0F},
    };
    std::uint32_t lightCount{1U};
    float fixtureEmissive{0.75F};
    bool alert{false};
};

} // namespace gdp::game::visual
