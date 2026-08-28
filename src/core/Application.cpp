#include "core/Application.hpp"

#include "core/Log.hpp"
#include "gameplay/Combat.hpp"
#include "gameplay/Enemy.hpp"
#include "gameplay/Loot.hpp"
#include "gameplay/Props.hpp"
#include "gameplay/RoomEvents.hpp"
#include "render/Renderer.hpp"
#include "world/EventGenerator.hpp"
#include "world/LightingGenerator.hpp"
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

gdp::render::RoomLightingRenderData makeRenderLighting(
    const gdp::world::RoomLightingPlan& plan,
    const bool alert)
{
    static_assert(gdp::world::maxRoomLights == gdp::render::maxRenderedRoomLights);
    gdp::render::RoomLightingRenderData lighting;
    lighting.ambientColor = plan.ambientColor;
    lighting.ambientIntensity = plan.ambientIntensity;
    lighting.lightCount = static_cast<std::uint32_t>(plan.lights.size());
    lighting.fixtureEmissive = plan.fixtureEmissive;
    lighting.alert = alert && plan.supportsAlert;
    for (std::size_t index = 0; index < plan.lights.size(); ++index) {
        const gdp::world::RoomPointLight& source = plan.lights[index];
        lighting.lights[index] = {
            source.position,
            source.color,
            source.intensity,
            source.range,
        };
    }
    return lighting;
}

} // namespace

Application::Application(ApplicationConfig config)
    : config_(std::move(config))
    , baseCollisionWorld_(gameplay::CollisionWorld::fixedRoom())
    , collisionWorld_(baseCollisionWorld_)
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
    roomEvents_.reserve(world_->rooms.size());
    roomLighting_.reserve(world_->rooms.size());
    for (const world::RoomInstance& room : world_->rooms) {
        const auto node = std::find_if(
            world_->graph.nodes.begin(), world_->graph.nodes.end(),
            [&room](const world::RoomNode& candidate) { return candidate.id == room.id; });
        if (node == world_->graph.nodes.end()) {
            log(LogLevel::Error, "Room event generation did not find its graph node");
            return false;
        }
        world::RoomEventPlan plan = world::generateRoomEventPlan(
            room.seed, room.role, room.theme, node->depth, room.enemyZones);
        if (!world::validateRoomEventPlan(plan, room.enemyZones).isValid()) {
            log(LogLevel::Error, "Room event plan failed deterministic validation");
            return false;
        }
        roomEvents_.push_back(gameplay::makeRoomEventRuntime(std::move(plan), room.id));
        world::RoomLightingPlan lighting = world::generateRoomLightingPlan(
            room.seed, room.role, room.theme, room.worldBounds);
        if (!world::validateRoomLightingPlan(lighting, room.worldBounds).isValid()) {
            log(LogLevel::Error, "Room lighting plan failed deterministic validation");
            return false;
        }
        roomLighting_.push_back(std::move(lighting));
    }
    if (const entities::Entity* playerEntity = entities_.find(player_.entity())) {
        collisionWorld_ = gameplay::collisionWorldWithProps(
            baseCollisionWorld_, entities_, playerEntity->roomId);
    }
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
        } else if (enemy->archetype == entities::EnemyArchetype::Engineer) {
            if (!renderer_->prepareEngineerVisual(enemy->visualSeed)) {
                log(LogLevel::Error, "Engineer visual preparation failed");
                return false;
            }
        } else if (enemy->archetype == entities::EnemyArchetype::IndustrialRobot) {
            if (!renderer_->prepareIndustrialRobotVisual(enemy->visualSeed)) {
                log(LogLevel::Error, "IndustrialRobot visual preparation failed");
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
    entities::Entity* playerEntity = entities_.find(player_.entity());
    const std::uint32_t playerRoom = playerEntity == nullptr ? 0 : playerEntity->roomId;
    collisionWorld_ = gameplay::collisionWorldWithProps(
        baseCollisionWorld_, entities_, playerRoom);
    player_.fixedUpdate(playerCommand_, step, collisionWorld_);

    const gameplay::LootCollectionSummary loot = gameplay::collectNearbyLoot(
        entities_, player_, weapon_, playerRoom);
    if (loot.collected > 0U) {
        if (world_) {
            const auto room = std::find_if(
                world_->rooms.begin(), world_->rooms.end(),
                [playerRoom](const world::RoomInstance& candidate) {
                    return candidate.id == playerRoom;
                });
            if (room != world_->rooms.end()) {
                room->entities.erase(std::remove_if(
                    room->entities.begin(), room->entities.end(),
                    [&loot](const entities::EntityId id) {
                        return std::find(
                            loot.collectedItems.begin(), loot.collectedItems.end(), id)
                            != loot.collectedItems.end();
                    }), room->entities.end());
            }
        }
        (void)entities_.flushDestroyed();
        std::ostringstream message;
        message << "Loot collected: " << loot.collected
                << " | health +" << static_cast<int>(loot.healthRestored)
                << " | ammo +" << loot.ammoAdded
                << " | keys +" << loot.keysAdded;
        log(LogLevel::Info, message.str());
    }
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
        if (shot.spawnedItem.isValid() && world_) {
            const auto room = std::find_if(
                world_->rooms.begin(), world_->rooms.end(),
                [playerRoom](const world::RoomInstance& candidate) {
                    return candidate.id == playerRoom;
                });
            if (room != world_->rooms.end()) {
                room->entities.push_back(shot.spawnedItem);
            }
        }
        if (shot.outcome == gameplay::ShotOutcome::Destroyed) {
            collisionWorld_ = gameplay::collisionWorldWithProps(
                baseCollisionWorld_, entities_, playerRoom);
        }
        std::ostringstream message;
        if (shot.outcome == gameplay::ShotOutcome::Hit
            || shot.outcome == gameplay::ShotOutcome::Killed
            || shot.outcome == gameplay::ShotOutcome::Destroyed) {
            message << (shot.outcome == gameplay::ShotOutcome::Killed
                    ? "Enemy killed: "
                    : shot.outcome == gameplay::ShotOutcome::Destroyed
                        ? "Prop destroyed: " : "Target hit: ")
                    << shot.hit->target.packed() << " | ammo "
                    << weapon_.magazineAmmo() << '/' << weapon_.reserveAmmo();
            log(LogLevel::Info, message.str());
        } else if (shot.outcome == gameplay::ShotOutcome::Empty) {
            log(LogLevel::Info, "Weapon empty; press R to reload");
        } else if (shot.outcome == gameplay::ShotOutcome::ImpactProp) {
            log(LogLevel::Info, "Protected prop absorbed the shot");
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
    (void)gameplay::updateEngineers(
        entities_, player_, playerRoom, collisionWorld_, step);
    (void)gameplay::updateIndustrialRobots(
        entities_, player_, playerRoom, collisionWorld_, step);
    if (playerWasAlive && !player_.state().alive) {
        log(LogLevel::Info, "Player died");
    }

    playerEntity = entities_.find(player_.entity());
    if (playerEntity != nullptr) {
        playerEntity->transform.position = player_.state().position;
        if (entities::PlayerData* data = std::get_if<entities::PlayerData>(&playerEntity->data)) {
            data->health = player_.state().health;
            data->keys = player_.state().keys;
        }
    }
    bool terminalUsed = false;
    if (playerCommand_.interact) {
        const std::array targets{
            gameplay::InteractionTarget{
                terminalEntity_,
                {{-0.4F, 0.0F, 2.05F}, {0.4F, 2.0F, 2.35F}}},
        };
        if (const auto target = player_.findInteraction(targets)) {
            terminalUsed = *target == terminalEntity_;
            std::ostringstream message;
            message << "Player interacted with entity " << target->packed();
            log(LogLevel::Info, message.str());
        }
        interactionPending_ = false;
        playerCommand_.interact = false;
    }
    const auto activeEvent = std::find_if(
        roomEvents_.begin(), roomEvents_.end(),
        [playerRoom](const gameplay::RoomEventRuntime& runtime) {
            return runtime.roomId == playerRoom;
        });
    if (activeEvent != roomEvents_.end()) {
        const gameplay::RoomEventUpdateSummary event = gameplay::updateRoomEvent(
            *activeEvent, entities_, true, terminalUsed, step);
        if (!event.createdEntities.empty() && world_) {
            const auto room = std::find_if(
                world_->rooms.begin(), world_->rooms.end(),
                [playerRoom](const world::RoomInstance& candidate) {
                    return candidate.id == playerRoom;
                });
            if (room != world_->rooms.end()) {
                room->entities.insert(
                    room->entities.end(),
                    event.createdEntities.begin(), event.createdEntities.end());
            }
        }
        for (const entities::EntityId id : event.spawnedEnemies) {
            const entities::EnemyData* enemy = entities_.findData<entities::EnemyData>(id);
            if (renderer_ == nullptr || enemy == nullptr) {
                continue;
            }
            const bool prepared = enemy->archetype == entities::EnemyArchetype::Guard
                ? renderer_->prepareGuardVisual(enemy->visualSeed)
                : renderer_->prepareWorkerVisual(enemy->visualSeed);
            if (!prepared) {
                log(LogLevel::Error, "Procedural event enemy visual preparation failed");
                running_ = false;
            }
        }
        if (event.activated) {
            std::ostringstream message;
            message << "Room event activated: "
                    << gameplay::roomEventTypeName(activeEvent->plan.type);
            log(LogLevel::Info, message.str());
        }
        if (event.wavesSpawned > 0U) {
            std::ostringstream message;
            message << "Event wave " << activeEvent->nextWave << '/'
                    << activeEvent->plan.waveCount << " spawned "
                    << event.enemiesSpawned << " enemy(s)";
            log(LogLevel::Info, message.str());
        }
        if (event.completed) {
            std::ostringstream message;
            message << "Room event completed: "
                    << gameplay::roomEventTypeName(activeEvent->plan.type)
                    << " | rewards " << event.rewardsSpawned;
            log(LogLevel::Info, message.str());
        }
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
    std::vector<gdp::render::PickupRenderData> visiblePickups;
    std::vector<gdp::render::PropRenderData> visibleProps;
    const entities::Entity* playerEntity = entities_.find(player_.entity());
    const std::uint32_t playerRoom = playerEntity == nullptr ? 0 : playerEntity->roomId;
    for (const entities::EntityId id : entities_.query(entities::EntityKind::Enemy)) {
        const entities::Entity* entity = entities_.find(id);
        const entities::EnemyData* enemy = entities_.findData<entities::EnemyData>(id);
        if (entity != nullptr && enemy != nullptr && entity->roomId == playerRoom
            && (enemy->archetype == entities::EnemyArchetype::Worker
                || enemy->archetype == entities::EnemyArchetype::Guard
                || enemy->archetype == entities::EnemyArchetype::Officer
                || enemy->archetype == entities::EnemyArchetype::Engineer
                || enemy->archetype == entities::EnemyArchetype::IndustrialRobot)) {
            gdp::render::EnemyVisualRole role = gdp::render::EnemyVisualRole::Worker;
            if (enemy->archetype == entities::EnemyArchetype::Guard) {
                role = gdp::render::EnemyVisualRole::Guard;
            } else if (enemy->archetype == entities::EnemyArchetype::Officer) {
                role = gdp::render::EnemyVisualRole::Officer;
            } else if (enemy->archetype == entities::EnemyArchetype::Engineer) {
                role = gdp::render::EnemyVisualRole::Engineer;
            } else if (enemy->archetype == entities::EnemyArchetype::IndustrialRobot) {
                role = gdp::render::EnemyVisualRole::IndustrialRobot;
            }
            visibleEnemies.push_back({
                entity->transform.position,
                enemy->behavior != entities::EnemyBehaviorState::Dead,
                enemy->visualSeed,
                role,
            });
        }
    }
    for (const entities::EntityId id : entities_.query(entities::EntityKind::Item)) {
        const entities::Entity* entity = entities_.find(id);
        const entities::ItemData* item = entities_.findData<entities::ItemData>(id);
        if (entity == nullptr || item == nullptr || entity->roomId != playerRoom) {
            continue;
        }
        gdp::render::PickupVisualRole role = gdp::render::PickupVisualRole::Health;
        if (item->type == entities::ItemType::Ammo) {
            role = gdp::render::PickupVisualRole::Ammo;
        } else if (item->type == entities::ItemType::Key) {
            role = gdp::render::PickupVisualRole::Key;
        }
        visiblePickups.push_back({entity->transform.position, role});
    }
    for (const entities::EntityId id : entities_.query(entities::EntityKind::Prop)) {
        const entities::Entity* entity = entities_.find(id);
        const entities::PropData* prop = entities_.findData<entities::PropData>(id);
        if (entity == nullptr || prop == nullptr || entity->roomId != playerRoom
            || (prop->type != entities::PropType::Crate
                && prop->type != entities::PropType::Canister
                && prop->type != entities::PropType::Barricade)) {
            continue;
        }
        gdp::render::PropVisualRole role = gdp::render::PropVisualRole::Crate;
        if (prop->type == entities::PropType::Canister) {
            role = gdp::render::PropVisualRole::Canister;
        } else if (prop->type == entities::PropType::Barricade) {
            role = gdp::render::PropVisualRole::Barricade;
        }
        gdp::render::PropVisualState state = gdp::render::PropVisualState::Intact;
        if (prop->state == entities::PropState::Damaged) {
            state = gdp::render::PropVisualState::Damaged;
        } else if (prop->state == entities::PropState::Destroyed) {
            state = gdp::render::PropVisualState::Destroyed;
        }
        visibleProps.push_back({
            entity->transform.position,
            entity->transform.scale,
            role,
            state,
            prop->visualSeed,
        });
    }
    std::ostringstream title;
    title << "GDP | HP " << static_cast<int>(player_.state().health)
          << " | Ammo " << weapon_.magazineAmmo() << '/' << weapon_.reserveAmmo();
    title << " | Keys " << player_.state().keys;
    const auto activeEvent = std::find_if(
        roomEvents_.begin(), roomEvents_.end(),
        [playerRoom](const gameplay::RoomEventRuntime& runtime) {
            return runtime.roomId == playerRoom;
        });
    if (activeEvent != roomEvents_.end()) {
        title << " | Event " << gameplay::roomEventTypeName(activeEvent->plan.type)
              << ':' << gameplay::roomEventPhaseName(activeEvent->phase);
        if (gameplay::isRoomLocked(*activeEvent)) {
            title << " [LOCKDOWN]";
        }
    }
    gdp::render::RoomLightingRenderData lighting;
    if (world_) {
        const auto currentRoom = std::find_if(
            world_->rooms.begin(), world_->rooms.end(),
            [playerRoom](const world::RoomInstance& room) {
                return room.id == playerRoom;
            });
        if (currentRoom != world_->rooms.end()) {
            const std::size_t roomIndex = static_cast<std::size_t>(
                std::distance(world_->rooms.begin(), currentRoom));
            if (roomIndex < roomLighting_.size()) {
                const bool alert = activeEvent != roomEvents_.end()
                    && gameplay::isRoomLocked(*activeEvent);
                lighting = makeRenderLighting(roomLighting_[roomIndex], alert);
            }
        }
    }
    SDL_SetWindowTitle(window_, title.str().c_str());
    if (renderer_ == nullptr
        || !renderer_->render(
            width, height, camera, visibleEnemies, visiblePickups, visibleProps, lighting)) {
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
    roomEvents_.clear();
    roomLighting_.clear();
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
