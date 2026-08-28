#include "core/Application.hpp"

#include "audio/AudioSystem.hpp"
#include "core/Log.hpp"
#include "gameplay/Combat.hpp"
#include "gameplay/Enemy.hpp"
#include "gameplay/Loot.hpp"
#include "gameplay/Props.hpp"
#include "gameplay/RoomEvents.hpp"
#include "gameplay/RoomTraversal.hpp"
#include "render/Renderer.hpp"
#include "world/EventGenerator.hpp"
#include "world/LightingGenerator.hpp"
#include "world/WorldEntities.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
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

gameplay::ProgressionEvent progressionEventFor(
    const entities::EnemyArchetype archetype) noexcept
{
    switch (archetype) {
    case entities::EnemyArchetype::Guard:
        return gameplay::ProgressionEvent::GuardKilled;
    case entities::EnemyArchetype::Officer:
        return gameplay::ProgressionEvent::OfficerKilled;
    case entities::EnemyArchetype::Engineer:
        return gameplay::ProgressionEvent::EngineerKilled;
    case entities::EnemyArchetype::IndustrialRobot:
        return gameplay::ProgressionEvent::IndustrialRobotKilled;
    case entities::EnemyArchetype::Worker:
        return gameplay::ProgressionEvent::WorkerKilled;
    }
    return gameplay::ProgressionEvent::WorkerKilled;
}

const char* progressionRewardName(const gameplay::ProgressionReward reward) noexcept
{
    switch (reward) {
    case gameplay::ProgressionReward::Vitality:
        return "Vitality +15 HP";
    case gameplay::ProgressionReward::Firepower:
        return "Firepower +4 damage";
    case gameplay::ProgressionReward::Capacity:
        return "Capacity +4 reserve";
    }
    return "Unknown";
}

bool prepareRoomMaterials(
    gdp::render::Renderer& renderer,
    const world::RoomInstance& room)
{
    if (room.role == world::RoomRole::Hub) {
        return renderer.prepareCentralHubRoomMaterials(room.seed);
    }
    if (room.role == world::RoomRole::Arena || room.role == world::RoomRole::Boss) {
        return renderer.prepareCombatRoomMaterials(room.seed);
    }
    switch (room.theme) {
    case world::RoomTheme::Storage:
        return renderer.prepareStorageRoomMaterials(room.seed);
    case world::RoomTheme::Laboratory:
        return renderer.prepareLaboratoryRoomMaterials(room.seed);
    case world::RoomTheme::Maintenance:
        return renderer.prepareTechnicalRoomMaterials(room.seed);
    case world::RoomTheme::Security:
        return renderer.prepareSecurityRoomMaterials(room.seed);
    case world::RoomTheme::Cafeteria:
        return renderer.prepareCafeteriaRoomMaterials(room.seed);
    case world::RoomTheme::Industrial:
    case world::RoomTheme::Office:
        return renderer.prepareIndustrialRoomMaterials(room.seed);
    }
    return false;
}

float yawForDirection(const gdp::core::Vec3 direction) noexcept
{
    constexpr float radiansToDegrees = 57.29577951308232F;
    return std::atan2(direction.z, direction.x) * radiansToDegrees;
}

} // namespace

Application::Application(ApplicationConfig config)
    : config_(std::move(config))
    , baseCollisionWorld_(gameplay::CollisionWorld::fixedRoom())
    , collisionWorld_(baseCollisionWorld_)
    , session_(config_.worldSeed)
{
}

Application::~Application()
{
    shutdown();
}

int Application::run()
{
    using Clock = std::chrono::steady_clock;
    bool beginAnotherRun = false;
    do {
        restartRequested_ = false;
        renderedFrames_ = 0U;
        fixedUpdates_ = 0U;
        if (!initialize()) {
            return 1;
        }
        auto previousTime = Clock::now();
        clock_.reset();
        running_ = true;

        while (running_) {
            const auto currentTime = Clock::now();
            const std::chrono::duration<double> elapsed = currentTime - previousTime;
            previousTime = currentTime;

            const double frameDelta = config_.headless
                ? clock_.fixedStepSeconds() : elapsed.count();
            clock_.advanceBy(frameDelta);

            input_.beginFrame();
            processEvents();
            if (!running_) {
                continue;
            }
            if (session_.state().phase == gameplay::SessionPhase::Playing) {
                player_.applyLook(input_.mouseDelta());
            }

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
        summary << "Run " << session_.state().runIndex << " ended as "
                << gameplay::sessionPhaseName(session_.state().phase)
                << " after " << renderedFrames_ << " frame(s) and "
                << fixedUpdates_ << " fixed update(s)";
        log(LogLevel::Info, summary.str());
        beginAnotherRun = restartRequested_;
        shutdown();
        if (beginAnotherRun) {
            const auto nextSeed = session_.restart();
            if (!nextSeed) {
                log(LogLevel::Error, "Session rejected a requested restart");
                return 1;
            }
            config_.worldSeed = *nextSeed;
        }
    } while (beginAnotherRun);
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
    const auto startRoom = std::find_if(
        world_->rooms.begin(), world_->rooms.end(),
        [](const world::RoomInstance& room) { return room.role == world::RoomRole::Start; });
    if (startRoom == world_->rooms.end()) {
        log(LogLevel::Error, "Generated world does not contain a START room");
        return false;
    }
    const gdp::core::Vec3 startForward = gdp::core::rotateQuarterTurns(
        {0.0F, 0.0F, -1.0F}, startRoom->rotation);
    const gdp::core::Vec3 startPosition = startRoom->transform.position
        + gdp::core::rotateQuarterTurns({0.0F, 0.0F, 4.0F}, startRoom->rotation);
    player_.spawn(startPosition, yawForDirection(startForward));
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
    baseCollisionWorld_ = gameplay::makeRoomNavigationCollision(
        *world_, startRoom->id, false);
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

    audio_ = std::make_unique<gdp::audio::AudioSystem>();
    (void)audio_->initialize(!config_.headless);
    if (startRoom != world_->rooms.end()) {
        audio_->setAmbience(startRoom->seed, static_cast<std::uint64_t>(startRoom->theme));
    }

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
    const bool roomMaterialsPrepared = prepareRoomMaterials(*renderer_, *startRoom);
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

    if (session_.state().phase != gameplay::SessionPhase::Playing) {
        playerCommand_ = {};
        interactionPending_ = false;
        firePending_ = false;
        reloadPending_ = false;
        if (input_.wasKeyPressed(SDL_SCANCODE_RETURN)) {
            restartRequested_ = true;
            running_ = false;
        }
        return;
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
    if (session_.state().phase != gameplay::SessionPhase::Playing) {
        if (audio_) {
            audio_->update(step);
        }
        ++fixedUpdates_;
        return;
    }
    weapon_.fixedUpdate(step);
    entities::Entity* playerEntity = entities_.find(player_.entity());
    std::uint32_t playerRoom = playerEntity == nullptr ? 0 : playerEntity->roomId;
    const auto awardProgress = [this](
        const gameplay::ProgressionEvent event,
        const std::uint32_t count) {
        const gameplay::ProgressionUpdate update
            = progression_.record(event, count, player_, weapon_);
        if (update.experienceAwarded == 0U) {
            return;
        }
        std::ostringstream message;
        message << "Progress +" << update.experienceAwarded << " XP | score +"
                << update.scoreAwarded << " | level " << progression_.state().level;
        log(LogLevel::Info, message.str());
        for (const gameplay::ProgressionReward reward : update.rewards) {
            std::ostringstream rewardMessage;
            rewardMessage << "Level reward: " << progressionRewardName(reward);
            log(LogLevel::Info, rewardMessage.str());
        }
    };
    const auto collisionEvent = std::find_if(
        roomEvents_.begin(), roomEvents_.end(),
        [playerRoom](const gameplay::RoomEventRuntime& runtime) {
            return runtime.roomId == playerRoom;
        });
    const bool doorsLocked = collisionEvent != roomEvents_.end()
        && gameplay::isRoomLocked(*collisionEvent);
    if (world_ && playerEntity != nullptr) {
        baseCollisionWorld_ = gameplay::makeRoomNavigationCollision(
            *world_, playerRoom, doorsLocked);
    }
    collisionWorld_ = gameplay::collisionWorldWithProps(
        baseCollisionWorld_, entities_, playerRoom);
    player_.fixedUpdate(playerCommand_, step, collisionWorld_);

    const auto adoptRoom = [this, &playerEntity, &playerRoom](
        const world::RoomInstance& destination,
        const std::string_view transitionKind) {
        const std::uint32_t sourceRoom = playerRoom;
        playerRoom = destination.id;
        playerEntity->roomId = playerRoom;
        playerEntity->transform.position = player_.state().position;
        baseCollisionWorld_ = gameplay::makeRoomNavigationCollision(
            *world_, playerRoom, false);
        collisionWorld_ = gameplay::collisionWorldWithProps(
            baseCollisionWorld_, entities_, playerRoom);
        if (renderer_ != nullptr
            && !prepareRoomMaterials(*renderer_, destination)) {
            log(LogLevel::Error, "Destination room material preparation failed");
            running_ = false;
        }
        if (audio_) {
            audio_->setAmbience(destination.seed,
                static_cast<std::uint64_t>(destination.theme));
        }
        std::ostringstream message;
        message << transitionKind << ' ' << sourceRoom << " -> " << playerRoom;
        log(LogLevel::Info, message.str());
    };

    if (playerEntity != nullptr && world_) {
        const auto enteredRoom = gameplay::roomContainingPosition(
            *world_, player_.state().position);
        if (enteredRoom && *enteredRoom != playerRoom) {
            const auto destination = std::find_if(
                world_->rooms.begin(), world_->rooms.end(),
                [&enteredRoom](const world::RoomInstance& room) {
                    return room.id == *enteredRoom;
                });
            if (destination != world_->rooms.end()) {
                adoptRoom(*destination, "Continuous room transition");
            }
        }
    }

    if (playerCommand_.interact && playerEntity != nullptr && world_) {
        const auto transition = gameplay::findRoomTransition(
            *world_, playerRoom, player_.state().position,
            player_.planarForwardDirection());
        if (transition) {
            const auto currentEvent = std::find_if(
                roomEvents_.begin(), roomEvents_.end(),
                [playerRoom](const gameplay::RoomEventRuntime& runtime) {
                    return runtime.roomId == playerRoom;
                });
            const bool locked = currentEvent != roomEvents_.end()
                && gameplay::isRoomLocked(*currentEvent);
            if (locked) {
                log(LogLevel::Info, "Door locked by active room event");
            } else {
                const auto destination = std::find_if(
                    world_->rooms.begin(), world_->rooms.end(),
                    [&transition](const world::RoomInstance& room) {
                        return room.id == transition->destinationRoom;
                    });
                if (destination != world_->rooms.end()
                    && player_.relocate(
                        transition->destinationPosition,
                        transition->destinationYawDegrees)) {
                    adoptRoom(*destination, "Interactive room transition");
                }
            }
            interactionPending_ = false;
            playerCommand_.interact = false;
        }
    }

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
        if (audio_) {
            audio_->play(gdp::audio::AudioEventKind::Pickup,
                config_.worldSeed, audioEventCounter_++);
        }
        awardProgress(gameplay::ProgressionEvent::LootCollected,
            static_cast<std::uint32_t>(loot.collected));
    }
    if (reloadPending_ && player_.state().alive) {
        if (weapon_.reload()) {
            std::ostringstream message;
            message << "Weapon reloaded: " << weapon_.magazineAmmo() << '/'
                    << weapon_.reserveAmmo();
            log(LogLevel::Info, message.str());
            if (audio_) {
                audio_->play(gdp::audio::AudioEventKind::Reload,
                    config_.worldSeed, audioEventCounter_++);
            }
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
        if (audio_ && shot.outcome != gameplay::ShotOutcome::Cooldown) {
            const gdp::audio::AudioEventKind fireKind
                = shot.outcome == gameplay::ShotOutcome::Empty
                ? gdp::audio::AudioEventKind::DryFire
                : gdp::audio::AudioEventKind::WeaponFire;
            audio_->playAt(fireKind, config_.worldSeed, audioEventCounter_++,
                player_.cameraPosition(), player_.cameraPosition(),
                player_.forwardDirection());
        }
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
        if (audio_ && shot.hit) {
            std::optional<gdp::audio::AudioEventKind> impactKind;
            if (shot.outcome == gameplay::ShotOutcome::Hit) {
                const entities::Entity* target = entities_.find(shot.hit->target);
                impactKind = target != nullptr && target->kind == entities::EntityKind::Prop
                    ? gdp::audio::AudioEventKind::PropImpact
                    : gdp::audio::AudioEventKind::EnemyHit;
            } else if (shot.outcome == gameplay::ShotOutcome::Killed) {
                impactKind = gdp::audio::AudioEventKind::EnemyDeath;
            } else if (shot.outcome == gameplay::ShotOutcome::Destroyed) {
                impactKind = gdp::audio::AudioEventKind::PropDestroyed;
            } else if (shot.outcome == gameplay::ShotOutcome::ImpactProp) {
                impactKind = gdp::audio::AudioEventKind::PropImpact;
            }
            if (impactKind) {
                audio_->playAt(*impactKind, config_.worldSeed, audioEventCounter_++,
                    shot.hit->point, player_.cameraPosition(), player_.forwardDirection());
            }
        } else if (audio_ && shot.outcome == gameplay::ShotOutcome::ImpactWorld) {
            audio_->playAt(gdp::audio::AudioEventKind::PropImpact,
                config_.worldSeed, audioEventCounter_++,
                player_.cameraPosition() + (player_.forwardDirection() * 4.0F),
                player_.cameraPosition(), player_.forwardDirection());
        }
        if (shot.outcome == gameplay::ShotOutcome::Killed && shot.hit) {
            const entities::EnemyData* killed
                = entities_.findData<entities::EnemyData>(shot.hit->target);
            if (killed != nullptr) {
                awardProgress(progressionEventFor(killed->archetype), 1U);
            }
        } else if (shot.outcome == gameplay::ShotOutcome::Destroyed) {
            awardProgress(gameplay::ProgressionEvent::PropDestroyed, 1U);
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
    const gameplay::EnemyUpdateSummary workers = gameplay::updateWorkers(
        entities_, player_, playerRoom, collisionWorld_, step);
    const gameplay::EnemyUpdateSummary guards = gameplay::updateGuards(
        entities_, player_, playerRoom, collisionWorld_, step);
    const gameplay::EnemyUpdateSummary officers = gameplay::updateOfficers(
        entities_, player_, playerRoom, collisionWorld_, step);
    const gameplay::EnemyUpdateSummary engineers = gameplay::updateEngineers(
        entities_, player_, playerRoom, collisionWorld_, step);
    const gameplay::EnemyUpdateSummary robots = gameplay::updateIndustrialRobots(
        entities_, player_, playerRoom, collisionWorld_, step);
    const std::size_t attacksLanded = workers.attacksLanded + guards.attacksLanded
        + officers.attacksLanded + engineers.attacksLanded + robots.attacksLanded;
    if (audio_ && attacksLanded > 0U) {
        audio_->play(gdp::audio::AudioEventKind::PlayerHurt,
            config_.worldSeed, audioEventCounter_++);
    }
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
        const entities::Entity* terminal = entities_.find(terminalEntity_);
        if (terminal != nullptr && terminal->roomId == playerRoom) {
            const gdp::core::Vec3 position = terminal->transform.position;
            const std::array targets{
                gameplay::InteractionTarget{
                    terminalEntity_,
                    {{position.x - 0.4F, position.y, position.z - 0.15F},
                        {position.x + 0.4F, position.y + 2.0F, position.z + 0.15F}}},
            };
            if (const auto target = player_.findInteraction(targets)) {
                terminalUsed = *target == terminalEntity_;
                std::ostringstream message;
                message << "Player interacted with entity " << target->packed();
                log(LogLevel::Info, message.str());
            }
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
            if (audio_) {
                audio_->play(gdp::audio::AudioEventKind::EventStart,
                    activeEvent->plan.seed, audioEventCounter_++);
            }
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
            if (audio_) {
                audio_->play(gdp::audio::AudioEventKind::EventComplete,
                    activeEvent->plan.seed, audioEventCounter_++);
            }
            awardProgress(gameplay::ProgressionEvent::RoomEventCompleted, 1U);
        }
    }

    world::RoomRole currentRoomRole = world::RoomRole::Count;
    gameplay::BossStatus boss;
    if (world_) {
        const auto currentRoom = std::find_if(
            world_->rooms.begin(), world_->rooms.end(),
            [playerRoom](const world::RoomInstance& room) {
                return room.id == playerRoom;
            });
        if (currentRoom != world_->rooms.end()) {
            currentRoomRole = currentRoom->role;
        }
    }
    if (currentRoomRole == world::RoomRole::Boss) {
        boss = gameplay::bossStatus(entities_, playerRoom);
    }
    const gameplay::SessionUpdate sessionUpdate = session_.update(
        player_.state().alive, currentRoomRole, boss.exists, boss.alive);
    if (sessionUpdate.changed) {
        interactionPending_ = false;
        firePending_ = false;
        reloadPending_ = false;
        std::ostringstream message;
        message << "Session result: "
                << gameplay::sessionPhaseName(sessionUpdate.phase)
                << " | run " << session_.state().runIndex
                << " | seed " << session_.state().currentSeed;
        log(LogLevel::Info, message.str());
        if (audio_ && sessionUpdate.phase == gameplay::SessionPhase::Victory) {
            audio_->play(gdp::audio::AudioEventKind::EventComplete,
                session_.state().currentSeed, audioEventCounter_++);
        }
    }
    if (audio_) {
        audio_->update(step);
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
    title << "GDP | Run " << (session_.state().runIndex + 1U)
          << " | HP " << static_cast<int>(player_.state().health)
          << " | Ammo " << weapon_.magazineAmmo() << '/' << weapon_.reserveAmmo();
    title << " | Keys " << player_.state().keys;
    const gameplay::ProgressionState& progress = progression_.state();
    title << " | Lv " << progress.level << " XP " << progress.experienceIntoLevel;
    const std::uint32_t nextLevelXp = gameplay::experienceForNextLevel(progress.level);
    if (nextLevelXp > 0U) {
        title << '/' << nextLevelXp;
    } else {
        title << " MAX";
    }
    title << " | Score " << progress.score;
    title << " | Room " << playerRoom;
    if (session_.state().phase == gameplay::SessionPhase::Victory) {
        title << " | VICTORY - ENTER: NEW RUN";
    } else if (session_.state().phase == gameplay::SessionPhase::Defeat) {
        title << " | DEFEAT - ENTER: RETRY";
    }
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
    gdp::render::RoomRenderData roomRender;
    std::vector<gdp::render::DoorRenderData> visibleDoors;
    std::vector<gdp::render::CorridorRenderData> visibleCorridors;
    std::vector<gdp::render::RoomRenderData> connectedRooms;
    world::RoomRole renderedRoomRole = world::RoomRole::Count;
    if (world_) {
        const auto currentRoom = std::find_if(
            world_->rooms.begin(), world_->rooms.end(),
            [playerRoom](const world::RoomInstance& room) {
                return room.id == playerRoom;
            });
        if (currentRoom != world_->rooms.end()) {
            renderedRoomRole = currentRoom->role;
            roomRender.origin = currentRoom->transform.position;
            roomRender.rotation = currentRoom->rotation;
            const std::size_t roomIndex = static_cast<std::size_t>(
                std::distance(world_->rooms.begin(), currentRoom));
            if (roomIndex < roomLighting_.size()) {
                const bool alert = activeEvent != roomEvents_.end()
                    && gameplay::isRoomLocked(*activeEvent);
                lighting = makeRenderLighting(roomLighting_[roomIndex], alert);
            }
            if (session_.state().phase == gameplay::SessionPhase::Victory) {
                lighting.ambientColor = {0.35F, 0.82F, 0.72F};
                lighting.ambientIntensity = std::max(lighting.ambientIntensity, 0.34F);
                lighting.alert = false;
            } else if (session_.state().phase == gameplay::SessionPhase::Defeat) {
                lighting.ambientColor = {0.52F, 0.06F, 0.04F};
                lighting.ambientIntensity = 0.14F;
                lighting.alert = true;
            }
            const bool doorsLocked = activeEvent != roomEvents_.end()
                && gameplay::isRoomLocked(*activeEvent);
            for (const world::ConnectorInstance& connector : currentRoom->connectors) {
                if (!connector.sealed && connector.edge) {
                    visibleDoors.push_back({
                        connector.worldPosition,
                        connector.worldNormal,
                        connector.width,
                        connector.height,
                        doorsLocked,
                    });
                }
            }
            for (const world::CorridorPath& corridor : world_->corridors) {
                world::RoomId connectedRoomId = 0U;
                if (corridor.firstRoom == playerRoom) {
                    connectedRoomId = corridor.secondRoom;
                } else if (corridor.secondRoom == playerRoom) {
                    connectedRoomId = corridor.firstRoom;
                } else {
                    continue;
                }
                visibleCorridors.push_back({
                    std::span<const gdp::core::Vec3>{corridor.points},
                    corridor.width,
                    corridor.height,
                });
                const auto connectedRoom = std::find_if(
                    world_->rooms.begin(), world_->rooms.end(),
                    [connectedRoomId](const world::RoomInstance& candidate) {
                        return candidate.id == connectedRoomId;
                    });
                if (connectedRoom == world_->rooms.end()) {
                    continue;
                }
                connectedRooms.push_back({
                    connectedRoom->transform.position,
                    connectedRoom->rotation,
                });
                const auto entryDoor = std::find_if(
                    connectedRoom->connectors.begin(), connectedRoom->connectors.end(),
                    [&corridor](const world::ConnectorInstance& connector) {
                        return !connector.sealed && connector.edge == corridor.edge;
                    });
                if (entryDoor != connectedRoom->connectors.end()) {
                    visibleDoors.push_back({
                        entryDoor->worldPosition,
                        entryDoor->worldNormal,
                        entryDoor->width,
                        entryDoor->height,
                        false,
                    });
                }
            }
        }
    }
    if (session_.state().phase == gameplay::SessionPhase::Playing) {
        title << (renderedRoomRole == world::RoomRole::Boss
                ? " | OBJECTIVE: DEFEAT BOSS"
                : " | OBJECTIVE: REACH BOSS");
    }
    SDL_SetWindowTitle(window_, title.str().c_str());
    if (renderer_ == nullptr
        || !renderer_->render(
            width, height, camera, visibleEnemies, visiblePickups, visibleProps,
            lighting, roomRender, visibleDoors, visibleCorridors, connectedRooms)) {
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
    audio_.reset();
    entities_.clear();
    roomEvents_.clear();
    roomLighting_.clear();
    input_ = Input{};
    player_ = gameplay::Player{};
    weapon_ = gameplay::Weapon{};
    progression_ = gameplay::ProgressionSystem{};
    baseCollisionWorld_ = gameplay::CollisionWorld::fixedRoom();
    collisionWorld_ = baseCollisionWorld_;
    playerCommand_ = {};
    interactionPending_ = false;
    firePending_ = false;
    reloadPending_ = false;
    audioEventCounter_ = 0U;
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
