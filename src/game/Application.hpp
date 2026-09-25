#pragma once

#include "core/Time.hpp"
#include "entities/EntityManager.hpp"
#include "game/LaunchSeed.hpp"
#include "game/PerformanceMonitor.hpp"
#include "game/visual/GdpRenderData.hpp"
#include "gameplay/Collision.hpp"
#include "gameplay/Campaign.hpp"
#include "gameplay/Door.hpp"
#include "gameplay/Feedback.hpp"
#include "gameplay/Footstep.hpp"
#include "gameplay/ExitConfirmation.hpp"
#include "gameplay/MainMenu.hpp"
#include "gameplay/Pause.hpp"
#include "gameplay/RoomEvents.hpp"
#include "gameplay/Session.hpp"
#include "gameplay/SettingsMenu.hpp"
#include "gameplay/Speedrun.hpp"
#include "gameplay/ViewMotion.hpp"
#include "gameplay/Weapon.hpp"
#include "gameplay/Player.hpp"
#include "gameplay/Progression.hpp"
#include "gameplay/ResultMenu.hpp"
#include "platform/Input.hpp"
#include "world/LightingGenerator.hpp"
#include "world/WorldGenerator.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

struct SDL_Window;
struct SDL_GLContextState;

namespace gdp::audio {
class AudioSystem;
}

namespace gdp::game::visual {
class GdpRenderer;
}

namespace gdp::game {

struct ApplicationConfig {
    bool headless{false};
    std::optional<std::uint64_t> frameLimit;
    std::optional<gameplay::GraphicsQuality> benchmarkQuality;
    int windowWidth{1280};
    int windowHeight{720};
    wkc::core::Seed worldSeed{deterministicDefaultWorldSeed};
};

class Application {
public:
    explicit Application(ApplicationConfig config);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    int run();

private:
    struct ActiveImpact {
        visual::ImpactRenderData render{};
        std::uint32_t roomId{0U};
        float remaining{0.0F};
    };

    bool initialize();
    bool initializeRun();
    void processEvents();
    void fixedUpdate(double fixedStepSeconds);
    void updateEnemyAnimationTransitions(bool deadOnly);
    void render();
    void shutdown();
    void setPaused(bool paused);
    [[nodiscard]] bool stageCampaignFloorReplay();
    [[nodiscard]] bool returnToMainMenu();
    void applyRuntimeSettings();
    void loadPersistedSettings();
    [[nodiscard]] bool savePersistedSettings();
    void loadPersistedSpeedrunRecord();
    [[nodiscard]] bool savePersistedSpeedrunRecord();
    void loadPersistedCampaignCheckpoint();
    [[nodiscard]] bool saveCurrentCampaignCheckpoint();
    [[nodiscard]] bool clearPersistedCampaignCheckpoint();

    ApplicationConfig config_;
    wkc::platform::Input input_;
    wkc::core::FixedStepClock clock_;
    gameplay::CollisionWorld baseCollisionWorld_;
    gameplay::CollisionWorld collisionWorld_;
    gameplay::Player player_;
    gameplay::Weapon weapon_;
    gameplay::ProgressionSystem progression_;
    std::optional<gameplay::CampaignCarryover> campaignCarryover_;
    std::optional<gameplay::CampaignCarryover> campaignFloorEntry_;
    std::optional<gameplay::CampaignCheckpoint> campaignCheckpoint_;
    std::optional<std::filesystem::path> campaignCheckpointPath_;
    float runDifficulty_{1.0F};
    gameplay::SessionSystem session_;
    gameplay::FeedbackState feedback_;
    gameplay::FootstepState footsteps_;
    gameplay::ViewMotionState viewMotion_;
    gameplay::MainMenuState mainMenu_;
    gameplay::PauseMenuState pauseMenu_;
    gameplay::ResultMenuState resultMenu_;
    gameplay::SettingsMenuState settingsMenu_;
    gameplay::ExitConfirmationState exitConfirmation_;
    gameplay::RuntimeSettings runtimeSettings_;
    std::optional<std::filesystem::path> settingsPath_;
    std::optional<gameplay::SpeedrunRecord> speedrunRecord_;
    gameplay::SpeedrunRecordResult latestSpeedrunRecordResult_{
        gameplay::SpeedrunRecordResult::Ineligible};
    std::optional<std::filesystem::path> speedrunRecordPath_;
    entities::EntityManager entities_;
    entities::EntityId terminalEntity_{};
    gameplay::PlayerCommand playerCommand_{};
    bool interactionPending_{false};
    bool firePending_{false};
    bool fireHeld_{false};
    bool reloadPending_{false};
    std::optional<gameplay::WeaponArchetype> weaponSwitchPending_;
    float weaponRecoilRemaining_{0.0F};
    float weaponMuzzleFlashRemaining_{0.0F};
    float weaponEquipRemaining_{0.0F};
    std::optional<world::World> world_;
    std::vector<gameplay::RoomEventRuntime> roomEvents_;
    std::vector<world::RoomLightingPlan> roomLighting_;
    std::unordered_map<world::RoomId, std::vector<entities::EntityId>>
        roomFixtureEntities_;
    std::unordered_map<world::RoomId, std::uint64_t> roomMaterialKeys_;
    std::unordered_map<world::EdgeId, gameplay::DoorRuntime> doorRuntimes_;
    std::unordered_map<world::EdgeId, std::uint64_t> doorAudioEventCounters_;
    std::unordered_map<std::uint64_t, visual::EnemyAnimationTransition>
        enemyAnimationTransitions_;
    std::vector<ActiveImpact> activeImpacts_;
    std::unique_ptr<gdp::audio::AudioSystem> audio_;
    std::unique_ptr<visual::GdpRenderer> renderer_;
    SDL_Window* window_{nullptr};
    SDL_GLContextState* glContext_{nullptr};
    bool sdlInitialized_{false};
    bool running_{false};
    bool restartRequested_{false};
    bool fatalError_{false};
    bool settingsLoaded_{false};
    bool settingsDirty_{false};
    bool speedrunRecordLoaded_{false};
    bool campaignCheckpointLoaded_{false};
    gameplay::RunRestartMode restartMode_{
        gameplay::RunRestartMode::NextSeed};
    std::uint64_t renderedFrames_{0};
    std::uint64_t fixedUpdates_{0};
    std::uint64_t enemyAnimationTicks_{0};
    std::uint64_t audioEventCounter_{0};
    std::uint64_t uiAudioEventCounter_{0};
    std::uint64_t impactVisualCounter_{0};
    PerformanceMonitor performanceMonitor_{};
    FramePerformanceSample currentFramePerformance_{};
    bool performanceOverlayVisible_{false};
};

} // namespace gdp::game
