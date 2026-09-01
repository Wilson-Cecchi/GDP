#pragma once

#include "core/Time.hpp"
#include "entities/EntityManager.hpp"
#include "gameplay/Collision.hpp"
#include "gameplay/Door.hpp"
#include "gameplay/Feedback.hpp"
#include "gameplay/Footstep.hpp"
#include "gameplay/MainMenu.hpp"
#include "gameplay/Pause.hpp"
#include "gameplay/RoomEvents.hpp"
#include "gameplay/Session.hpp"
#include "gameplay/ViewMotion.hpp"
#include "gameplay/Weapon.hpp"
#include "gameplay/Player.hpp"
#include "gameplay/Progression.hpp"
#include "platform/Input.hpp"
#include "world/LightingGenerator.hpp"
#include "world/WorldGenerator.hpp"

#include <cstdint>
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
    int windowWidth{1280};
    int windowHeight{720};
    wkc::core::Seed worldSeed{0x4744505F574F524CULL};
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
    bool initialize();
    bool initializeRun();
    void processEvents();
    void fixedUpdate(double fixedStepSeconds);
    void render();
    void shutdown();
    void setPaused(bool paused);

    ApplicationConfig config_;
    wkc::platform::Input input_;
    wkc::core::FixedStepClock clock_;
    gameplay::CollisionWorld baseCollisionWorld_;
    gameplay::CollisionWorld collisionWorld_;
    gameplay::Player player_;
    gameplay::Weapon weapon_;
    gameplay::ProgressionSystem progression_;
    gameplay::SessionSystem session_;
    gameplay::FeedbackState feedback_;
    gameplay::FootstepState footsteps_;
    gameplay::ViewMotionState viewMotion_;
    gameplay::MainMenuState mainMenu_;
    gameplay::PauseMenuState pauseMenu_;
    entities::EntityManager entities_;
    entities::EntityId terminalEntity_{};
    gameplay::PlayerCommand playerCommand_{};
    bool interactionPending_{false};
    bool firePending_{false};
    bool reloadPending_{false};
    std::optional<world::World> world_;
    std::vector<gameplay::RoomEventRuntime> roomEvents_;
    std::vector<world::RoomLightingPlan> roomLighting_;
    std::unordered_map<world::RoomId, std::uint64_t> roomMaterialKeys_;
    std::unordered_map<world::EdgeId, gameplay::DoorRuntime> doorRuntimes_;
    std::unordered_map<world::EdgeId, std::uint64_t> doorAudioEventCounters_;
    std::unique_ptr<gdp::audio::AudioSystem> audio_;
    std::unique_ptr<visual::GdpRenderer> renderer_;
    SDL_Window* window_{nullptr};
    SDL_GLContextState* glContext_{nullptr};
    bool sdlInitialized_{false};
    bool running_{false};
    bool restartRequested_{false};
    bool fatalError_{false};
    std::uint64_t renderedFrames_{0};
    std::uint64_t fixedUpdates_{0};
    std::uint64_t audioEventCounter_{0};
};

} // namespace gdp::game
