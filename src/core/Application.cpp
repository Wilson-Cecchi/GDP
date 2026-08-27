#include "core/Application.hpp"

#include "core/Log.hpp"
#include "gameplay/Combat.hpp"
#include "gameplay/Enemy.hpp"
#include "render/Renderer.hpp"
#include "world/WorldEntities.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace gdp::core {
namespace {

std::string sdlError(const std::string_view operation)
{
    std::ostringstream stream;
    stream << operation << ": " << SDL_GetError();
    return stream.str();
}

} // namespace

Application::Application(ApplicationConfig config)
    : config_(std::move(config))
{
}

Application::~Application()
{
    shutdown();
}

int Application::run()
{
    if (!initialize()) {
        return 1;
    }

    using Clock = std::chrono::steady_clock;
    auto previousTime = Clock::now();
    clock_.reset();
    running_ = true;

    while (running_) {
        const auto currentTime = Clock::now();
        const std::chrono::duration<double> elapsed = currentTime - previousTime;
        previousTime = currentTime;

        const double frameDelta = config_.headless ? clock_.fixedStepSeconds() : elapsed.count();
        clock_.advanceBy(frameDelta);

        input_.beginFrame();
        processEvents();
        player_.applyLook(input_.mouseDelta());

        while (clock_.consumeFixedStep()) {
            fixedUpdate(clock_.fixedStepSeconds());
        }

        render();
        ++renderedFrames_;

        if (config_.frameLimit && renderedFrames_ >= *config_.frameLimit) {
            running_ = false;
        }
    }

    std::ostringstream summary;
    summary << "Shutdown after " << renderedFrames_ << " frame(s) and "
            << fixedUpdates_ << " fixed update(s)";
    log(LogLevel::Info, summary.str());
    shutdown();
    return 0;
}

bool Application::initialize()
{
    world::WorldGenerationResult generation = world::generateWorld(config_.worldSeed);
    if (!generation.succeeded()) {
        log(LogLevel::Error, "World generation exhausted all deterministic attempts");
        return false;
    }
    world_ = std::move(generation.world);
    const world::InitialEntitySet initialEntities = world::populateInitialEntities(
        *world_,
        entities_,
        {
            .transform = {.position = player_.state().position},
            .health = player_.state().health,
        });
    if (!initialEntities.isValid()) {
        log(LogLevel::Error, "Initial entity population did not find a START room");
        return false;
    }
    player_.bindEntity(initialEntities.player);
    terminalEntity_ = initialEntities.terminal;
    std::ostringstream worldSummary;
    worldSummary << "Generated world seed " << config_.worldSeed << " with "
                 << world_->rooms.size() << " rooms and " << world_->corridors.size()
                 << " corridors and " << entities_.activeCount()
                 << " entities on attempt " << world_->acceptedAttempt;
    log(LogLevel::Info, worldSummary.str());

    SDL_SetAppMetadata("GDP", "0.1.0", "dev.wkc.gdp");

    const SDL_InitFlags flags = config_.headless ? SDL_INIT_EVENTS : SDL_INIT_VIDEO;
    if (!SDL_Init(flags)) {
        log(LogLevel::Error, sdlError("SDL_Init failed"));
        return false;
    }
    sdlInitialized_ = true;

    if (config_.headless) {
        log(LogLevel::Info, "GDP initialized in headless mode with a validated world");
        return true;
    }

    if (!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3)
        || !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3)
        || !SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE)
        || !SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1)
        || !SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24)) {
        log(LogLevel::Error, sdlError("SDL_GL_SetAttribute failed"));
        return false;
    }

    const SDL_WindowFlags windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
        | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    window_ = SDL_CreateWindow("GDP", config_.windowWidth, config_.windowHeight, windowFlags);
    if (window_ == nullptr) {
        log(LogLevel::Error, sdlError("SDL_CreateWindow failed"));
        return false;
    }

    glContext_ = SDL_GL_CreateContext(window_);
    if (glContext_ == nullptr) {
        log(LogLevel::Error, sdlError("SDL_GL_CreateContext failed"));
        return false;
    }

    if (!SDL_GL_MakeCurrent(window_, glContext_)) {
        log(LogLevel::Error, sdlError("SDL_GL_MakeCurrent failed"));
        return false;
    }

    if (!SDL_GL_SetSwapInterval(1)) {
        log(LogLevel::Warning, sdlError("VSync unavailable"));
    }
    if (!SDL_SetWindowRelativeMouseMode(window_, true)) {
        log(LogLevel::Warning, sdlError("Relative mouse mode unavailable"));
    }

    int drawableWidth = 0;
    int drawableHeight = 0;
    if (!SDL_GetWindowSizeInPixels(window_, &drawableWidth, &drawableHeight)) {
        log(LogLevel::Error, sdlError("SDL_GetWindowSizeInPixels failed"));
        return false;
    }
    renderer_ = std::make_unique<gdp::render::Renderer>();
    if (!renderer_->initialize(drawableWidth, drawableHeight)) {
        log(LogLevel::Error, "Renderer initialization failed");
        return false;
    }
    const auto startRoom = std::find_if(
        world_->rooms.begin(), world_->rooms.end(),
        [](const world::RoomInstance& room) { return room.role == world::RoomRole::Start; });
    bool roomMaterialsPrepared = false;
    if (startRoom != world_->rooms.end()) {
        if (startRoom->theme == world::RoomTheme::Storage) {
            roomMaterialsPrepared = renderer_->prepareStorageRoomMaterials(startRoom->seed);
        } else {
            roomMaterialsPrepared = renderer_->prepareIndustrialRoomMaterials(startRoom->seed);
        }
    }
    if (!roomMaterialsPrepared) {
        log(LogLevel::Error, "START room material preparation failed");
        return false;
    }
    for (const entities::EntityId id : entities_.query(entities::EntityKind::Enemy)) {
        const entities::EnemyData* enemy = entities_.findData<entities::EnemyData>(id);
        if (enemy == nullptr) {
            continue;
        }
        if (enemy->archetype == entities::EnemyArchetype::Worker) {
            if (!renderer_->prepareWorkerVisual(enemy->visualSeed)) {
                log(LogLevel::Error, "Worker visual preparation failed");
                return false;
            }
        } else if (enemy->archetype == entities::EnemyArchetype::Guard) {
            if (!renderer_->prepareGuardVisual(enemy->visualSeed)) {
                log(LogLevel::Error, "Guard visual preparation failed");
                return false;
            }
        } else if (enemy->archetype == entities::EnemyArchetype::Officer) {
            if (!renderer_->prepareOfficerVisual(enemy->visualSeed)) {
                log(LogLevel::Error, "Officer visual preparation failed");
                return false;
            }
        }
    }

    log(LogLevel::Info, "GDP initialized with an OpenGL 3.3 core context");
    return true;
}

void Application::processEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        input_.handleEvent(event);
        if (event.type == SDL_EVENT_QUIT) {
            running_ = false;
        }
    }

    if (input_.wasKeyPressed(SDL_SCANCODE_ESCAPE)) {
        running_ = false;
    }

    playerCommand_.moveForward = (input_.isKeyDown(SDL_SCANCODE_W) ? 1.0F : 0.0F)
        - (input_.isKeyDown(SDL_SCANCODE_S) ? 1.0F : 0.0F);
    playerCommand_.moveRight = (input_.isKeyDown(SDL_SCANCODE_D) ? 1.0F : 0.0F)
        - (input_.isKeyDown(SDL_SCANCODE_A) ? 1.0F : 0.0F);
    interactionPending_ = interactionPending_ || input_.wasKeyPressed(SDL_SCANCODE_E);
    firePending_ = firePending_ || input_.wasMouseButtonPressed(SDL_BUTTON_LEFT);
    reloadPending_ = reloadPending_ || input_.wasKeyPressed(SDL_SCANCODE_R);
    playerCommand_.interact = interactionPending_;
}

void Application::fixedUpdate(const double fixedStep)
{
    const float step = static_cast<float>(fixedStep);
    weapon_.fixedUpdate(step);
    player_.fixedUpdate(playerCommand_, step, collisionWorld_);

    entities::Entity* playerEntity = entities_.find(player_.entity());
    const std::uint32_t playerRoom = playerEntity == nullptr ? 0 : playerEntity->roomId;
    if (reloadPending_ && player_.state().alive) {
        if (weapon_.reload()) {
            std::ostringstream message;
            message << "Weapon reloaded: " << weapon_.magazineAmmo() << '/'
                    << weapon_.reserveAmmo();
            log(LogLevel::Info, message.str());
        }
    }
    reloadPending_ = false;
    if (firePending_ && player_.state().alive) {
        const gameplay::ShotResult shot = gameplay::fireHitscan(
            weapon_,
            player_.cameraPosition(),
            player_.forwardDirection(),
            playerRoom,
            collisionWorld_,
            entities_);
        std::ostringstream message;
        if (shot.outcome == gameplay::ShotOutcome::Hit
            || shot.outcome == gameplay::ShotOutcome::Killed) {
            message << (shot.outcome == gameplay::ShotOutcome::Killed
                    ? "Enemy killed: " : "Enemy hit: ")
                    << shot.hit->target.packed() << " | ammo "
                    << weapon_.magazineAmmo() << '/' << weapon_.reserveAmmo();
            log(LogLevel::Info, message.str());
        } else if (shot.outcome == gameplay::ShotOutcome::Empty) {
            log(LogLevel::Info, "Weapon empty; press R to reload");
        }
    }
    firePending_ = false;

    const bool playerWasAlive = player_.state().alive;
    (void)gameplay::updateWorkers(
        entities_, player_, playerRoom, collisionWorld_, step);
    (void)gameplay::updateGuards(
        entities_, player_, playerRoom, collisionWorld_, step);
    (void)gameplay::updateOfficers(
        entities_, player_, playerRoom, collisionWorld_, step);
    if (playerWasAlive && !player_.state().alive) {
        log(LogLevel::Info, "Player died");
    }

    playerEntity = entities_.find(player_.entity());
    if (playerEntity != nullptr) {
        playerEntity->transform.position = player_.state().position;
        if (entities::PlayerData* data = std::get_if<entities::PlayerData>(&playerEntity->data)) {
            data->health = player_.state().health;
        }
    }
    if (playerCommand_.interact) {
        const std::array targets{
            gameplay::InteractionTarget{
                terminalEntity_,
                {{-0.4F, 0.0F, 2.05F}, {0.4F, 2.0F, 2.35F}}},
        };
        if (const auto target = player_.findInteraction(targets)) {
            std::ostringstream message;
            message << "Player interacted with entity " << target->packed();
            log(LogLevel::Info, message.str());
        }
        interactionPending_ = false;
        playerCommand_.interact = false;
    }
    ++fixedUpdates_;
}

void Application::render()
{
    if (config_.headless || window_ == nullptr || glContext_ == nullptr) {
        return;
    }

    int width = 0;
    int height = 0;
    if (!SDL_GetWindowSizeInPixels(window_, &width, &height)) {
        log(LogLevel::Warning, sdlError("SDL_GetWindowSizeInPixels failed"));
        return;
    }

    const gdp::render::CameraData camera{
        player_.cameraPosition(),
        player_.forwardDirection(),
        68.0F,
    };
    std::vector<gdp::render::EnemyRenderData> visibleEnemies;
    const entities::Entity* playerEntity = entities_.find(player_.entity());
    const std::uint32_t playerRoom = playerEntity == nullptr ? 0 : playerEntity->roomId;
    for (const entities::EntityId id : entities_.query(entities::EntityKind::Enemy)) {
        const entities::Entity* entity = entities_.find(id);
        const entities::EnemyData* enemy = entities_.findData<entities::EnemyData>(id);
        if (entity != nullptr && enemy != nullptr && entity->roomId == playerRoom
            && (enemy->archetype == entities::EnemyArchetype::Worker
                || enemy->archetype == entities::EnemyArchetype::Guard
                || enemy->archetype == entities::EnemyArchetype::Officer)) {
            gdp::render::EnemyVisualRole role = gdp::render::EnemyVisualRole::Worker;
            if (enemy->archetype == entities::EnemyArchetype::Guard) {
                role = gdp::render::EnemyVisualRole::Guard;
            } else if (enemy->archetype == entities::EnemyArchetype::Officer) {
                role = gdp::render::EnemyVisualRole::Officer;
            }
            visibleEnemies.push_back({
                entity->transform.position,
                enemy->behavior != entities::EnemyBehaviorState::Dead,
                enemy->visualSeed,
                role,
            });
        }
    }
    std::ostringstream title;
    title << "GDP | HP " << static_cast<int>(player_.state().health)
          << " | Ammo " << weapon_.magazineAmmo() << '/' << weapon_.reserveAmmo();
    SDL_SetWindowTitle(window_, title.str().c_str());
    if (renderer_ == nullptr || !renderer_->render(width, height, camera, visibleEnemies)) {
        log(LogLevel::Error, "Renderer failed while drawing the frame");
        running_ = false;
        return;
    }
    SDL_GL_SwapWindow(window_);
}

void Application::shutdown()
{
    running_ = false;

    renderer_.reset();
    entities_.clear();
    terminalEntity_ = {};
    world_.reset();

    if (glContext_ != nullptr) {
        SDL_GL_DestroyContext(glContext_);
        glContext_ = nullptr;
    }

    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    if (sdlInitialized_) {
        SDL_Quit();
        sdlInitialized_ = false;
    }
}

} // namespace gdp::core
