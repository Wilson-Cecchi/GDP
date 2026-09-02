#include "game/Application.hpp"

#include "game/audio/AudioSystem.hpp"
#include "core/Log.hpp"
#include "gameplay/Combat.hpp"
#include "gameplay/Enemy.hpp"
#include "gameplay/Loot.hpp"
#include "gameplay/MainMenu.hpp"
#include "gameplay/Props.hpp"
#include "gameplay/RoomEvents.hpp"
#include "gameplay/RoomTraversal.hpp"
#include "persistence/SettingsStore.hpp"
#include "platform/SdlInput.hpp"
#include "game/visual/GdpRenderData.hpp"
#include "game/visual/GdpRenderer.hpp"
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

namespace gdp::game {

using wkc::core::LogLevel;
using wkc::core::Vec2;
using wkc::core::log;
using wkc::platform::Input;
using wkc::platform::Key;
using wkc::platform::MouseButton;
namespace {

std::string sdlError(const std::string_view operation)
{
    std::ostringstream stream;
    stream << operation << ": " << SDL_GetError();
    return stream.str();
}

visual::RoomLightingRenderData makeRenderLighting(
    const gdp::world::RoomLightingPlan& plan,
    const bool alert)
{
    static_assert(gdp::world::maxRoomLights <= visual::maxRenderedRoomLights);
    visual::RoomLightingRenderData lighting;
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

std::vector<gdp::world::RoomId> roomsWithinGraphDistance(
    const gdp::world::World& world,
    const gdp::world::RoomId origin,
    const std::uint32_t maximumDistance)
{
    if (origin == 0U) {
        return {};
    }
    std::vector<gdp::world::RoomId> rooms{origin};
    std::size_t layerBegin = 0U;
    std::size_t layerEnd = rooms.size();
    for (std::uint32_t depth = 0U; depth < maximumDistance; ++depth) {
        for (std::size_t index = layerBegin; index < layerEnd; ++index) {
            const gdp::world::RoomId roomId = rooms[index];
            for (const gdp::world::CorridorPath& corridor : world.corridors) {
                gdp::world::RoomId neighbor = 0U;
                if (corridor.firstRoom == roomId) {
                    neighbor = corridor.secondRoom;
                } else if (corridor.secondRoom == roomId) {
                    neighbor = corridor.firstRoom;
                }
                if (neighbor != 0U
                    && std::find(rooms.begin(), rooms.end(), neighbor) == rooms.end()) {
                    rooms.push_back(neighbor);
                }
            }
        }
        layerBegin = layerEnd;
        layerEnd = rooms.size();
        if (layerBegin == layerEnd) {
            break;
        }
    }
    std::sort(rooms.begin(), rooms.end());
    return rooms;
}

void appendConnectedRoomLights(
    visual::RoomLightingRenderData& lighting,
    const gdp::world::World& world,
    const std::vector<gdp::world::RoomLightingPlan>& roomLighting,
    const gdp::world::RoomId currentRoomId,
    const wkc::core::Vec3 observerPosition)
{
    struct Candidate {
        visual::PointLightRenderData light;
        float distanceSquared{0.0F};
        gdp::world::RoomId roomId{0};
        std::size_t lightIndex{0};
    };

    std::vector<gdp::world::RoomId> connectedRoomIds
        = roomsWithinGraphDistance(world, currentRoomId, 2U);
    connectedRoomIds.erase(
        std::remove(connectedRoomIds.begin(), connectedRoomIds.end(), currentRoomId),
        connectedRoomIds.end());

    std::vector<Candidate> candidates;
    for (const gdp::world::RoomId roomId : connectedRoomIds) {
        const auto room = std::find_if(
            world.rooms.begin(), world.rooms.end(),
            [roomId](const gdp::world::RoomInstance& candidate) {
                return candidate.id == roomId;
            });
        if (room == world.rooms.end()) {
            continue;
        }
        const std::size_t roomIndex = static_cast<std::size_t>(
            std::distance(world.rooms.begin(), room));
        if (roomIndex >= roomLighting.size()) {
            continue;
        }
        const gdp::world::RoomLightingPlan& plan = roomLighting[roomIndex];
        for (std::size_t lightIndex = 0; lightIndex < plan.lights.size(); ++lightIndex) {
            const gdp::world::RoomPointLight& source = plan.lights[lightIndex];
            const wkc::core::Vec3 offset = source.position - observerPosition;
            candidates.push_back({
                {source.position, source.color, source.intensity, source.range},
                wkc::core::dot(offset, offset),
                roomId,
                lightIndex,
            });
        }
    }
    std::sort(candidates.begin(), candidates.end(),
        [](const Candidate& left, const Candidate& right) {
            if (left.distanceSquared < right.distanceSquared) {
                return true;
            }
            if (right.distanceSquared < left.distanceSquared) {
                return false;
            }
            if (left.roomId != right.roomId) {
                return left.roomId < right.roomId;
            }
            return left.lightIndex < right.lightIndex;
        });

    std::size_t lightCount = std::min<std::size_t>(
        lighting.lightCount, visual::maxRenderedRoomLights);
    for (const Candidate& candidate : candidates) {
        if (lightCount >= visual::maxRenderedRoomLights) {
            break;
        }
        lighting.lights[lightCount++] = candidate.light;
    }
    lighting.lightCount = static_cast<std::uint32_t>(lightCount);
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
        return "Vitality +15 HP +10 armor";
    case gameplay::ProgressionReward::Firepower:
        return "Firepower +4 damage";
    case gameplay::ProgressionReward::Capacity:
        return "Capacity +4 reserve";
    }
    return "Unknown";
}

bool prepareRoomMaterials(
    visual::GdpRenderer& renderer,
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

float yawForDirection(const wkc::core::Vec3 direction) noexcept
{
    constexpr float radiansToDegrees = 57.29577951308232F;
    return std::atan2(direction.z, direction.x) * radiansToDegrees;
}

bool edgeTouchesRoom(
    const world::World& world,
    const world::EdgeId edge,
    const world::RoomId roomId) noexcept
{
    const auto corridor = std::find_if(
        world.corridors.begin(), world.corridors.end(),
        [edge](const world::CorridorPath& candidate) {
            return candidate.edge == edge;
        });
    return corridor != world.corridors.end()
        && (corridor->firstRoom == roomId || corridor->secondRoom == roomId);
}

bool playerNearDoor(
    const world::World& world,
    const world::EdgeId edge,
    const wkc::core::Vec3 position) noexcept
{
    constexpr float occupancyDistance = 1.05F;
    for (const world::RoomInstance& room : world.rooms) {
        for (const world::ConnectorInstance& connector : room.connectors) {
            if (!connector.edge || *connector.edge != edge) {
                continue;
            }
            const float deltaX = connector.worldPosition.x - position.x;
            const float deltaZ = connector.worldPosition.z - position.z;
            if ((deltaX * deltaX) + (deltaZ * deltaZ)
                <= occupancyDistance * occupancyDistance) {
                return true;
            }
        }
    }
    return false;
}

std::optional<wkc::core::Vec3> nearestDoorPosition(
    const world::World& world,
    const world::EdgeId edge,
    const wkc::core::Vec3 listenerPosition) noexcept
{
    std::optional<wkc::core::Vec3> nearest;
    float nearestDistanceSquared = 0.0F;
    for (const world::RoomInstance& room : world.rooms) {
        for (const world::ConnectorInstance& connector : room.connectors) {
            if (!connector.edge || *connector.edge != edge) {
                continue;
            }
            const wkc::core::Vec3 offset
                = connector.worldPosition - listenerPosition;
            const float distanceSquared = wkc::core::dot(offset, offset);
            if (!nearest || distanceSquared < nearestDistanceSquared) {
                nearest = connector.worldPosition;
                nearestDistanceSquared = distanceSquared;
            }
        }
    }
    return nearest;
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
    bool launchRunImmediately = config_.headless;
    do {
        restartRequested_ = false;
        fatalError_ = false;
        renderedFrames_ = 0U;
        fixedUpdates_ = 0U;
        if (!initialize()) {
            return 1;
        }
        if (launchRunImmediately && !initializeRun()) {
            shutdown();
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
            if (session_.state().phase == gameplay::SessionPhase::Playing
                && !mainMenu_.active && !pauseMenu_.paused
                && !settingsMenu_.active) {
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

        const bool hadRun = world_.has_value();
        std::ostringstream summary;
        if (hadRun) {
            summary << "Run " << session_.state().runIndex << " ended as "
                    << gameplay::sessionPhaseName(session_.state().phase);
        } else {
            summary << "Application closed from MainMenu";
        }
        summary << " after " << renderedFrames_ << " frame(s) and "
                << fixedUpdates_ << " fixed update(s)";
        log(LogLevel::Info, summary.str());
        beginAnotherRun = restartRequested_;
        shutdown();
        if (fatalError_) {
            return 1;
        }
        if (beginAnotherRun) {
            const auto nextSeed = session_.restart();
            if (!nextSeed) {
                log(LogLevel::Error, "Session rejected a requested restart");
                return 1;
            }
            config_.worldSeed = *nextSeed;
            launchRunImmediately = true;
        }
    } while (beginAnotherRun);
    return 0;
}

bool Application::initialize()
{
    SDL_SetAppMetadata("GDP", "0.1.0", "dev.wkc.gdp");

    const SDL_InitFlags flags = config_.headless ? SDL_INIT_EVENTS : SDL_INIT_VIDEO;
    if (!SDL_Init(flags)) {
        log(LogLevel::Error, sdlError("SDL_Init failed"));
        return false;
    }
    sdlInitialized_ = true;

    loadPersistedSettings();

    audio_ = std::make_unique<gdp::audio::AudioSystem>();
    (void)audio_->initialize(!config_.headless);
    applyRuntimeSettings();

    if (config_.headless) {
        log(LogLevel::Info, "GDP headless platform initialized");
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
    int drawableWidth = 0;
    int drawableHeight = 0;
    if (!SDL_GetWindowSizeInPixels(window_, &drawableWidth, &drawableHeight)) {
        log(LogLevel::Error, sdlError("SDL_GetWindowSizeInPixels failed"));
        return false;
    }
    renderer_ = std::make_unique<visual::GdpRenderer>();
    if (!renderer_->initialize(drawableWidth, drawableHeight)) {
        log(LogLevel::Error, "Renderer initialization failed");
        return false;
    }
    gameplay::openMainMenu(mainMenu_);
    log(LogLevel::Info, "GDP initialized with an OpenGL 3.3 core context");
    log(LogLevel::Info, "Main menu initialized before world generation");
    return true;
}

bool Application::initializeRun()
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

    const wkc::core::Vec3 startForward = wkc::core::rotateQuarterTurns(
        {0.0F, 0.0F, -1.0F}, startRoom->rotation);
    const wkc::core::Vec3 startPosition = startRoom->transform.position
        + wkc::core::rotateQuarterTurns({0.0F, 0.0F, 4.0F}, startRoom->rotation);
    player_.spawn(startPosition, yawForDirection(startForward));
    const world::InitialEntitySet initialEntities = world::populateInitialEntities(
        *world_, entities_,
        {
            .transform = {.position = player_.state().position},
            .health = player_.state().health,
            .armor = player_.state().armor,
            .maximumArmor = player_.state().maximumArmor,
        });
    if (!initialEntities.isValid()) {
        log(LogLevel::Error, "Initial entity population did not find a START room");
        return false;
    }
    player_.bindEntity(initialEntities.player);
    terminalEntity_ = initialEntities.terminal;

    std::vector<world::EdgeId> initiallyBlockedEdges;
    initiallyBlockedEdges.reserve(world_->corridors.size());
    for (const world::CorridorPath& corridor : world_->corridors) {
        doorRuntimes_.emplace(corridor.edge, gameplay::makeDoorRuntime(corridor.edge));
        doorAudioEventCounters_.emplace(corridor.edge, 0U);
        initiallyBlockedEdges.push_back(corridor.edge);
    }
    baseCollisionWorld_ = gameplay::makeRoomNavigationCollision(
        *world_, startRoom->id, false, initiallyBlockedEdges);
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
    for (gameplay::RoomEventRuntime& runtime : roomEvents_) {
        const std::vector<entities::EntityId> staged
            = gameplay::stageRoomEventFirstWave(runtime, entities_);
        if (staged.empty()) {
            continue;
        }
        const auto room = std::find_if(
            world_->rooms.begin(), world_->rooms.end(),
            [&runtime](const world::RoomInstance& candidate) {
                return candidate.id == runtime.roomId;
            });
        if (room != world_->rooms.end()) {
            room->entities.insert(room->entities.end(), staged.begin(), staged.end());
        }
    }
    if (const entities::Entity* playerEntity = entities_.find(player_.entity())) {
        collisionWorld_ = gameplay::collisionWorldWithProps(
            baseCollisionWorld_, entities_, playerEntity->roomId);
    }

    if (audio_) {
        audio_->setAmbience(startRoom->seed, static_cast<std::uint64_t>(startRoom->theme));
    }
    if (!config_.headless) {
        if (renderer_ == nullptr) {
            log(LogLevel::Error, "Run initialization requires an active renderer");
            return false;
        }
        for (const world::RoomInstance& room : world_->rooms) {
            if (!prepareRoomMaterials(*renderer_, room)) {
                log(LogLevel::Error, "World room material preparation failed");
                return false;
            }
            roomMaterialKeys_.emplace(room.id, renderer_->activeRoomMaterialKey());
        }
        for (const entities::EntityId id : entities_.query(entities::EntityKind::Enemy)) {
            const entities::EnemyData* enemy = entities_.findData<entities::EnemyData>(id);
            if (enemy == nullptr) {
                continue;
            }
            bool prepared = true;
            if (enemy->archetype == entities::EnemyArchetype::Worker) {
                prepared = renderer_->prepareWorkerVisual(enemy->visualSeed);
            } else if (enemy->archetype == entities::EnemyArchetype::Guard) {
                prepared = renderer_->prepareGuardVisual(enemy->visualSeed);
            } else if (enemy->archetype == entities::EnemyArchetype::Officer) {
                prepared = renderer_->prepareOfficerVisual(enemy->visualSeed);
            } else if (enemy->archetype == entities::EnemyArchetype::Engineer) {
                prepared = renderer_->prepareEngineerVisual(enemy->visualSeed);
            } else if (enemy->archetype == entities::EnemyArchetype::IndustrialRobot) {
                prepared = renderer_->prepareIndustrialRobotVisual(enemy->visualSeed);
            }
            if (!prepared) {
                log(LogLevel::Error, "Enemy visual preparation failed");
                return false;
            }
        }
        if (!SDL_SetWindowRelativeMouseMode(window_, true)) {
            log(LogLevel::Warning, sdlError("Relative mouse mode unavailable"));
        }
    }
    gameplay::closeMainMenu(mainMenu_);
    input_ = Input{};

    std::ostringstream worldSummary;
    worldSummary << "Generated world seed " << config_.worldSeed << " with "
                 << world_->rooms.size() << " rooms and " << world_->corridors.size()
                 << " corridors and " << entities_.activeCount()
                 << " entities on attempt " << world_->acceptedAttempt;
    log(LogLevel::Info, worldSummary.str());
    if (config_.headless) {
        log(LogLevel::Info, "GDP initialized in headless mode with a validated world");
    } else {
        log(LogLevel::Info, "Run initialized after main menu confirmation");
    }
    return true;
}

void Application::processEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        (void)wkc::platform::handleSdlInputEvent(input_, event);
        if (event.type == SDL_EVENT_QUIT) {
            running_ = false;
        }
    }

    if (!running_) {
        return;
    }

    const auto playUiAudio = [this](const gdp::audio::AudioEventKind kind) {
        if (audio_) {
            audio_->playUi(
                kind, config_.worldSeed, uiAudioEventCounter_++);
        }
    };

    if (settingsMenu_.active) {
        playerCommand_ = {};
        interactionPending_ = false;
        firePending_ = false;
        reloadPending_ = false;

        if (input_.wasKeyPressed(Key::Escape)) {
            gameplay::closeSettingsMenu(settingsMenu_);
            (void)savePersistedSettings();
            playUiAudio(gdp::audio::AudioEventKind::UiBack);
            return;
        }

        std::optional<visual::HudSettingsSelection> pointedSelection;
        float pointerDrawableX = 0.0F;
        int drawableWidth = 0;
        if (window_ != nullptr) {
            int windowWidth = 0;
            int windowHeight = 0;
            int drawableHeight = 0;
            if (SDL_GetWindowSize(window_, &windowWidth, &windowHeight)
                && SDL_GetWindowSizeInPixels(window_, &drawableWidth, &drawableHeight)
                && windowWidth > 0 && windowHeight > 0) {
                const Vec2 pointer = input_.mousePosition();
                pointerDrawableX = pointer.x * static_cast<float>(drawableWidth)
                    / static_cast<float>(windowWidth);
                const float pointerDrawableY
                    = pointer.y * static_cast<float>(drawableHeight)
                    / static_cast<float>(windowHeight);
                pointedSelection = visual::settingsMenuSelectionAt(
                    drawableWidth, drawableHeight,
                    pointerDrawableX, pointerDrawableY);
            }
        }
        if (pointedSelection && input_.mouseDelta() != Vec2{}) {
            const gameplay::SettingsMenuOption previousSelection
                = settingsMenu_.selection;
            gameplay::selectSettingsMenuOption(settingsMenu_,
                static_cast<gameplay::SettingsMenuOption>(*pointedSelection));
            if (settingsMenu_.selection != previousSelection) {
                playUiAudio(gdp::audio::AudioEventKind::UiNavigate);
            }
        }

        const bool moveUp = input_.wasKeyPressed(Key::Up)
            || input_.wasKeyPressed(Key::W);
        const bool moveDown = input_.wasKeyPressed(Key::Down)
            || input_.wasKeyPressed(Key::S);
        if (moveUp != moveDown) {
            const gameplay::SettingsMenuOption previousSelection
                = settingsMenu_.selection;
            gameplay::moveSettingsMenuSelection(
                settingsMenu_, moveUp ? -1 : 1);
            if (settingsMenu_.selection != previousSelection) {
                playUiAudio(gdp::audio::AudioEventKind::UiNavigate);
            }
        }

        const bool adjustDown = input_.wasKeyPressed(Key::Left)
            || input_.wasKeyPressed(Key::A);
        const bool adjustUp = input_.wasKeyPressed(Key::Right)
            || input_.wasKeyPressed(Key::D);
        if (adjustDown != adjustUp
            && gameplay::adjustRuntimeSetting(runtimeSettings_,
                settingsMenu_.selection, adjustDown ? -1 : 1)) {
            settingsDirty_ = true;
            applyRuntimeSettings();
            playUiAudio(gdp::audio::AudioEventKind::UiNavigate);
        }

        gameplay::SettingsMenuCommand command
            = gameplay::SettingsMenuCommand::None;
        if (input_.wasKeyPressed(Key::Enter)) {
            command = gameplay::activateSettingsMenuSelection(settingsMenu_);
        } else if (pointedSelection
            && input_.wasMouseButtonPressed(MouseButton::Primary)) {
            gameplay::selectSettingsMenuOption(settingsMenu_,
                static_cast<gameplay::SettingsMenuOption>(*pointedSelection));
            if (settingsMenu_.selection == gameplay::SettingsMenuOption::Back) {
                command = gameplay::activateSettingsMenuSelection(settingsMenu_);
            } else {
                const int direction = pointerDrawableX
                        < (static_cast<float>(drawableWidth) * 0.5F)
                    ? -1 : 1;
                if (gameplay::adjustRuntimeSetting(runtimeSettings_,
                    settingsMenu_.selection, direction)) {
                    settingsDirty_ = true;
                    applyRuntimeSettings();
                    playUiAudio(gdp::audio::AudioEventKind::UiNavigate);
                }
            }
        }
        if (command != gameplay::SettingsMenuCommand::None) {
            (void)savePersistedSettings();
            playUiAudio(gdp::audio::AudioEventKind::UiBack);
        }
        return;
    }

    if (mainMenu_.active) {
        playerCommand_ = {};
        interactionPending_ = false;
        firePending_ = false;
        reloadPending_ = false;

        if (input_.wasKeyPressed(Key::Escape)) {
            log(LogLevel::Info, "Main menu requested game exit");
            running_ = false;
            return;
        }

        std::optional<visual::HudMainMenuSelection> pointedSelection;
        if (window_ != nullptr) {
            int windowWidth = 0;
            int windowHeight = 0;
            int drawableWidth = 0;
            int drawableHeight = 0;
            if (SDL_GetWindowSize(window_, &windowWidth, &windowHeight)
                && SDL_GetWindowSizeInPixels(window_, &drawableWidth, &drawableHeight)
                && windowWidth > 0 && windowHeight > 0) {
                const Vec2 pointer = input_.mousePosition();
                pointedSelection = visual::mainMenuSelectionAt(
                    drawableWidth, drawableHeight,
                    pointer.x * static_cast<float>(drawableWidth)
                        / static_cast<float>(windowWidth),
                    pointer.y * static_cast<float>(drawableHeight)
                        / static_cast<float>(windowHeight));
            }
        }
        const auto mainMenuOptionFor
            = [](const visual::HudMainMenuSelection selection) {
                  if (selection == visual::HudMainMenuSelection::Settings) {
                      return gameplay::MainMenuOption::Settings;
                  }
                  return selection == visual::HudMainMenuSelection::ExitGame
                      ? gameplay::MainMenuOption::ExitGame
                      : gameplay::MainMenuOption::StartRun;
              };
        if (pointedSelection && input_.mouseDelta() != Vec2{}) {
            const gameplay::MainMenuOption previousSelection
                = mainMenu_.selection;
            gameplay::selectMainMenuOption(
                mainMenu_, mainMenuOptionFor(*pointedSelection));
            if (mainMenu_.selection != previousSelection) {
                playUiAudio(gdp::audio::AudioEventKind::UiNavigate);
            }
        }

        const bool moveUp = input_.wasKeyPressed(Key::Up)
            || input_.wasKeyPressed(Key::W);
        const bool moveDown = input_.wasKeyPressed(Key::Down)
            || input_.wasKeyPressed(Key::S);
        if (moveUp != moveDown) {
            const gameplay::MainMenuOption previousSelection
                = mainMenu_.selection;
            gameplay::moveMainMenuSelection(mainMenu_, moveUp ? -1 : 1);
            if (mainMenu_.selection != previousSelection) {
                playUiAudio(gdp::audio::AudioEventKind::UiNavigate);
            }
        }

        gameplay::MainMenuCommand command = gameplay::MainMenuCommand::None;
        if (input_.wasKeyPressed(Key::Enter)) {
            command = gameplay::activateMainMenuSelection(mainMenu_);
        } else if (pointedSelection
            && input_.wasMouseButtonPressed(MouseButton::Primary)) {
            gameplay::selectMainMenuOption(
                mainMenu_, mainMenuOptionFor(*pointedSelection));
            command = gameplay::activateMainMenuSelection(mainMenu_);
        }

        if (command != gameplay::MainMenuCommand::None) {
            playUiAudio(gdp::audio::AudioEventKind::UiConfirm);
        }

        if (command == gameplay::MainMenuCommand::StartRun) {
            log(LogLevel::Info, "Main menu requested run start");
            if (!initializeRun()) {
                fatalError_ = true;
                running_ = false;
            }
        } else if (command == gameplay::MainMenuCommand::Settings) {
            gameplay::openSettingsMenu(
                settingsMenu_, gameplay::SettingsMenuOrigin::MainMenu);
            log(LogLevel::Info, "Main menu opened settings");
        } else if (command == gameplay::MainMenuCommand::ExitGame) {
            log(LogLevel::Info, "Main menu requested game exit");
            running_ = false;
        }
        return;
    }

    if (session_.state().phase != gameplay::SessionPhase::Playing) {
        playerCommand_ = {};
        interactionPending_ = false;
        firePending_ = false;
        reloadPending_ = false;
        if (input_.wasKeyPressed(Key::Escape)) {
            playUiAudio(gdp::audio::AudioEventKind::UiBack);
            log(LogLevel::Info, "Result menu requested game exit");
            restartRequested_ = false;
            running_ = false;
            return;
        }

        std::optional<visual::HudResultMenuSelection> pointedSelection;
        if (window_ != nullptr) {
            int windowWidth = 0;
            int windowHeight = 0;
            int drawableWidth = 0;
            int drawableHeight = 0;
            if (SDL_GetWindowSize(window_, &windowWidth, &windowHeight)
                && SDL_GetWindowSizeInPixels(window_, &drawableWidth, &drawableHeight)
                && windowWidth > 0 && windowHeight > 0) {
                const Vec2 pointer = input_.mousePosition();
                pointedSelection = visual::resultMenuSelectionAt(
                    drawableWidth, drawableHeight,
                    pointer.x * static_cast<float>(drawableWidth)
                        / static_cast<float>(windowWidth),
                    pointer.y * static_cast<float>(drawableHeight)
                        / static_cast<float>(windowHeight));
            }
        }
        const auto resultOptionFor
            = [](const visual::HudResultMenuSelection selection) {
                  if (selection == visual::HudResultMenuSelection::MainMenu) {
                      return gameplay::ResultMenuOption::MainMenu;
                  }
                  return selection == visual::HudResultMenuSelection::ExitGame
                      ? gameplay::ResultMenuOption::ExitGame
                      : gameplay::ResultMenuOption::PlayAgain;
              };
        if (pointedSelection && input_.mouseDelta() != Vec2{}) {
            const gameplay::ResultMenuOption previousSelection
                = resultMenu_.selection;
            gameplay::selectResultMenuOption(
                resultMenu_, resultOptionFor(*pointedSelection));
            if (resultMenu_.selection != previousSelection) {
                playUiAudio(gdp::audio::AudioEventKind::UiNavigate);
            }
        }

        const bool moveUp = input_.wasKeyPressed(Key::Up)
            || input_.wasKeyPressed(Key::W);
        const bool moveDown = input_.wasKeyPressed(Key::Down)
            || input_.wasKeyPressed(Key::S);
        if (moveUp != moveDown) {
            const gameplay::ResultMenuOption previousSelection
                = resultMenu_.selection;
            gameplay::moveResultMenuSelection(
                resultMenu_, moveUp ? -1 : 1);
            if (resultMenu_.selection != previousSelection) {
                playUiAudio(gdp::audio::AudioEventKind::UiNavigate);
            }
        }

        gameplay::ResultMenuCommand command = gameplay::ResultMenuCommand::None;
        if (input_.wasKeyPressed(Key::Enter)) {
            command = gameplay::activateResultMenuSelection(resultMenu_);
        } else if (pointedSelection
            && input_.wasMouseButtonPressed(MouseButton::Primary)) {
            gameplay::selectResultMenuOption(
                resultMenu_, resultOptionFor(*pointedSelection));
            command = gameplay::activateResultMenuSelection(resultMenu_);
        }
        if (command != gameplay::ResultMenuCommand::None) {
            playUiAudio(gdp::audio::AudioEventKind::UiConfirm);
        }
        if (command == gameplay::ResultMenuCommand::PlayAgain) {
            log(LogLevel::Info, "Result menu requested another run");
            restartRequested_ = true;
            running_ = false;
        } else if (command == gameplay::ResultMenuCommand::MainMenu) {
            log(LogLevel::Info, "Result menu requested main menu");
            if (!returnToMainMenu()) {
                fatalError_ = true;
                running_ = false;
            }
        } else if (command == gameplay::ResultMenuCommand::ExitGame) {
            log(LogLevel::Info, "Result menu requested game exit");
            restartRequested_ = false;
            running_ = false;
        }
        return;
    }

    if (input_.wasKeyPressed(Key::Escape)) {
        playUiAudio(pauseMenu_.paused
                ? gdp::audio::AudioEventKind::UiBack
                : gdp::audio::AudioEventKind::UiConfirm);
        setPaused(!pauseMenu_.paused);
        return;
    }

    if (pauseMenu_.paused) {
        playerCommand_ = {};
        interactionPending_ = false;
        firePending_ = false;
        reloadPending_ = false;

        std::optional<visual::HudPauseSelection> pointedSelection;
        if (window_ != nullptr) {
            int windowWidth = 0;
            int windowHeight = 0;
            int drawableWidth = 0;
            int drawableHeight = 0;
            if (SDL_GetWindowSize(window_, &windowWidth, &windowHeight)
                && SDL_GetWindowSizeInPixels(window_, &drawableWidth, &drawableHeight)
                && windowWidth > 0 && windowHeight > 0) {
                const Vec2 pointer = input_.mousePosition();
                pointedSelection = visual::pauseMenuSelectionAt(
                    drawableWidth, drawableHeight,
                    pointer.x * static_cast<float>(drawableWidth)
                        / static_cast<float>(windowWidth),
                    pointer.y * static_cast<float>(drawableHeight)
                        / static_cast<float>(windowHeight));
            }
        }
        const auto pauseOptionFor = [](const visual::HudPauseSelection selection) {
            switch (selection) {
            case visual::HudPauseSelection::RestartRun:
                return gameplay::PauseMenuOption::RestartRun;
            case visual::HudPauseSelection::Settings:
                return gameplay::PauseMenuOption::Settings;
            case visual::HudPauseSelection::MainMenu:
                return gameplay::PauseMenuOption::MainMenu;
            case visual::HudPauseSelection::ExitGame:
                return gameplay::PauseMenuOption::ExitGame;
            case visual::HudPauseSelection::Resume:
                return gameplay::PauseMenuOption::Resume;
            }
            return gameplay::PauseMenuOption::Resume;
        };
        if (pointedSelection && input_.mouseDelta() != Vec2{}) {
            const gameplay::PauseMenuOption previousSelection
                = pauseMenu_.selection;
            gameplay::selectPauseMenuOption(
                pauseMenu_, pauseOptionFor(*pointedSelection));
            if (pauseMenu_.selection != previousSelection) {
                playUiAudio(gdp::audio::AudioEventKind::UiNavigate);
            }
        }

        const bool moveUp = input_.wasKeyPressed(Key::Up)
            || input_.wasKeyPressed(Key::W);
        const bool moveDown = input_.wasKeyPressed(Key::Down)
            || input_.wasKeyPressed(Key::S);
        if (moveUp != moveDown) {
            const gameplay::PauseMenuOption previousSelection
                = pauseMenu_.selection;
            gameplay::movePauseSelection(pauseMenu_, moveUp ? -1 : 1);
            if (pauseMenu_.selection != previousSelection) {
                playUiAudio(gdp::audio::AudioEventKind::UiNavigate);
            }
        }

        gameplay::PauseMenuCommand command = gameplay::PauseMenuCommand::None;
        if (input_.wasKeyPressed(Key::Enter)) {
            command = gameplay::activatePauseSelection(pauseMenu_);
        } else if (pointedSelection
            && input_.wasMouseButtonPressed(MouseButton::Primary)) {
            gameplay::selectPauseMenuOption(
                pauseMenu_, pauseOptionFor(*pointedSelection));
            command = gameplay::activatePauseSelection(pauseMenu_);
        }
        if (command != gameplay::PauseMenuCommand::None) {
            playUiAudio(gdp::audio::AudioEventKind::UiConfirm);
        }
        if (command == gameplay::PauseMenuCommand::Resume) {
            setPaused(false);
        } else if (command == gameplay::PauseMenuCommand::RestartRun) {
            log(LogLevel::Info, "Pause menu requested run restart");
            (void)session_.update(
                false, world::RoomRole::Count, false, false);
            restartRequested_ = true;
            running_ = false;
        } else if (command == gameplay::PauseMenuCommand::Settings) {
            gameplay::openSettingsMenu(
                settingsMenu_, gameplay::SettingsMenuOrigin::PauseMenu);
            log(LogLevel::Info, "Pause menu opened settings");
        } else if (command == gameplay::PauseMenuCommand::MainMenu) {
            log(LogLevel::Info, "Pause menu requested main menu");
            if (!returnToMainMenu()) {
                fatalError_ = true;
                running_ = false;
            }
        } else if (command == gameplay::PauseMenuCommand::ExitGame) {
            log(LogLevel::Info, "Pause menu requested game exit");
            restartRequested_ = false;
            running_ = false;
        }
        return;
    }

    playerCommand_.moveForward = (input_.isKeyDown(Key::W) ? 1.0F : 0.0F)
        - (input_.isKeyDown(Key::S) ? 1.0F : 0.0F);
    playerCommand_.moveRight = (input_.isKeyDown(Key::D) ? 1.0F : 0.0F)
        - (input_.isKeyDown(Key::A) ? 1.0F : 0.0F);
    interactionPending_ = interactionPending_ || input_.wasKeyPressed(Key::E);
    firePending_ = firePending_
        || input_.wasMouseButtonPressed(MouseButton::Primary);
    reloadPending_ = reloadPending_ || input_.wasKeyPressed(Key::R);
    playerCommand_.interact = interactionPending_;
}

void Application::setPaused(const bool paused)
{
    if (paused) {
        gameplay::openPauseMenu(pauseMenu_);
    } else {
        gameplay::closePauseMenu(pauseMenu_);
    }
    playerCommand_ = {};
    interactionPending_ = false;
    firePending_ = false;
    reloadPending_ = false;
    if (!config_.headless && window_ != nullptr
        && !SDL_SetWindowRelativeMouseMode(window_, !paused)) {
        log(LogLevel::Warning, sdlError("Pause mouse mode update failed"));
    }
    log(LogLevel::Info, paused ? "Game paused" : "Game resumed");
}

bool Application::returnToMainMenu()
{
    if (session_.state().phase == gameplay::SessionPhase::Playing) {
        (void)session_.update(
            false, world::RoomRole::Count, false, false);
    }
    const std::optional<wkc::core::Seed> nextSeed = session_.restart();
    if (!nextSeed) {
        log(LogLevel::Error, "Session rejected return to main menu");
        return false;
    }
    config_.worldSeed = *nextSeed;

    if (audio_) {
        audio_->stopAmbience();
        audio_->clearVoices();
    }
    renderer_.reset();
    entities_.clear();
    roomEvents_.clear();
    roomLighting_.clear();
    roomMaterialKeys_.clear();
    doorRuntimes_.clear();
    doorAudioEventCounters_.clear();
    input_ = Input{};
    player_ = gameplay::Player{};
    weapon_ = gameplay::Weapon{};
    progression_ = gameplay::ProgressionSystem{};
    gameplay::clearFeedback(feedback_);
    gameplay::resetFootsteps(footsteps_);
    gameplay::resetViewMotion(viewMotion_);
    gameplay::openMainMenu(mainMenu_);
    gameplay::closePauseMenu(pauseMenu_);
    gameplay::closeResultMenu(resultMenu_);
    gameplay::closeSettingsMenu(settingsMenu_);
    baseCollisionWorld_ = gameplay::CollisionWorld::fixedRoom();
    collisionWorld_ = baseCollisionWorld_;
    playerCommand_ = {};
    interactionPending_ = false;
    firePending_ = false;
    reloadPending_ = false;
    restartRequested_ = false;
    audioEventCounter_ = 0U;
    uiAudioEventCounter_ = 0U;
    terminalEntity_ = {};
    world_.reset();
    clock_.reset();
    applyRuntimeSettings();

    int drawableWidth = 0;
    int drawableHeight = 0;
    if (window_ == nullptr
        || !SDL_GetWindowSizeInPixels(
            window_, &drawableWidth, &drawableHeight)) {
        log(LogLevel::Error,
            sdlError("Main menu framebuffer size query failed"));
        return false;
    }
    renderer_ = std::make_unique<visual::GdpRenderer>();
    if (!renderer_->initialize(drawableWidth, drawableHeight)) {
        log(LogLevel::Error, "Main menu renderer reinitialization failed");
        return false;
    }
    if (!SDL_SetWindowRelativeMouseMode(window_, false)) {
        log(LogLevel::Warning, sdlError("Main menu mouse mode update failed"));
    }
    log(LogLevel::Info, "Returned to main menu with the next deterministic seed");
    return true;
}

void Application::applyRuntimeSettings()
{
    player_.setMouseSensitivity(
        gameplay::mouseSensitivity(runtimeSettings_));
    player_.setInvertMouseY(runtimeSettings_.invertMouseY);
    if (audio_) {
        audio_->setMasterVolume(gameplay::normalizedVolume(
            runtimeSettings_.masterVolumePercent));
        audio_->setEffectsVolume(gameplay::normalizedVolume(
            runtimeSettings_.effectsVolumePercent));
        audio_->setAmbientVolume(gameplay::normalizedVolume(
            runtimeSettings_.ambientVolumePercent));
        audio_->setUiVolume(gameplay::normalizedVolume(
            runtimeSettings_.uiVolumePercent));
    }
}

void Application::loadPersistedSettings()
{
    if (settingsLoaded_) {
        return;
    }
    settingsLoaded_ = true;
    if (config_.headless) {
        return;
    }

    char* const preferenceDirectory = SDL_GetPrefPath("WKC", "GDP");
    if (preferenceDirectory == nullptr) {
        log(LogLevel::Warning,
            sdlError("Settings preference path unavailable; using defaults"));
        return;
    }
    settingsPath_ = std::filesystem::path{preferenceDirectory} / "settings.toml";
    SDL_free(preferenceDirectory);

    const persistence::SettingsLoadResult loaded
        = persistence::loadRuntimeSettings(*settingsPath_);
    runtimeSettings_ = loaded.settings;
    switch (loaded.status) {
    case persistence::SettingsLoadStatus::Loaded:
        log(LogLevel::Info, "Runtime settings loaded from disk");
        break;
    case persistence::SettingsLoadStatus::Migrated:
        log(LogLevel::Info,
            "Runtime settings migrated in memory from schema version 1");
        break;
    case persistence::SettingsLoadStatus::LoadedWithRecovery:
        log(LogLevel::Warning,
            "Runtime settings loaded with invalid values recovered");
        break;
    case persistence::SettingsLoadStatus::Missing:
        log(LogLevel::Info, "No settings file found; using defaults");
        break;
    case persistence::SettingsLoadStatus::Invalid:
        log(LogLevel::Warning, "Invalid settings file; using defaults");
        break;
    case persistence::SettingsLoadStatus::UnsupportedVersion:
        log(LogLevel::Warning,
            "Unsupported settings version; using defaults");
        break;
    case persistence::SettingsLoadStatus::IoError:
        log(LogLevel::Warning, "Settings file could not be read; using defaults");
        break;
    }
}

bool Application::savePersistedSettings()
{
    if (!settingsDirty_) {
        return true;
    }
    if (!settingsPath_) {
        log(LogLevel::Warning,
            "Settings changed but no writable preference path is available");
        return false;
    }
    if (!persistence::saveRuntimeSettings(*settingsPath_, runtimeSettings_)) {
        log(LogLevel::Warning,
            "Runtime settings could not be saved; previous file was preserved");
        return false;
    }
    settingsDirty_ = false;
    log(LogLevel::Info, "Runtime settings saved to disk");
    return true;
}

void Application::fixedUpdate(const double fixedStep)
{
    const float step = static_cast<float>(fixedStep);
    if (mainMenu_.active || pauseMenu_.paused || settingsMenu_.active) {
        if (audio_) {
            audio_->updateUi(step);
        }
        return;
    }
    gameplay::updateFeedback(feedback_, step);
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
        if (update.levelsGained > 0U) {
            gameplay::signalNotice(
                feedback_, gameplay::NoticeFeedbackKind::LevelUp);
        }
    };
    const auto collisionEvent = std::find_if(
        roomEvents_.begin(), roomEvents_.end(),
        [playerRoom](const gameplay::RoomEventRuntime& runtime) {
            return runtime.roomId == playerRoom;
        });
    const bool doorsLocked = collisionEvent != roomEvents_.end()
        && gameplay::isRoomLocked(*collisionEvent);
    const auto playDoorAudio = [this](
        const gdp::audio::AudioEventKind kind,
        const world::EdgeId edge) {
        if (!audio_ || !world_) {
            return;
        }
        const std::optional<wkc::core::Vec3> position = nearestDoorPosition(
            *world_, edge, player_.state().position);
        if (!position) {
            return;
        }
        std::uint64_t& sequence = doorAudioEventCounters_[edge];
        const std::uint64_t eventId
            = (static_cast<std::uint64_t>(edge) << 32U) ^ sequence++;
        audio_->playAt(kind, session_.state().currentSeed, eventId, *position,
            player_.state().position, player_.planarForwardDirection());
    };
    std::optional<world::EdgeId> occupiedCorridorEdge;
    if (world_ && !gameplay::roomContainingCharacter(
            *world_, player_.state().position)) {
        for (const world::CorridorPath& corridor : world_->corridors) {
            if (gameplay::corridorProgressAt(corridor, player_.state().position)) {
                occupiedCorridorEdge = corridor.edge;
                break;
            }
        }
    }
    std::vector<world::EdgeId> blockedDoorEdges;
    blockedDoorEdges.reserve(doorRuntimes_.size());
    if (world_) {
        for (auto& [edge, door] : doorRuntimes_) {
            const bool locked = doorsLocked
                && edgeTouchesRoom(*world_, edge, playerRoom);
            const bool occupied = occupiedCorridorEdge == edge
                || playerNearDoor(*world_, edge, player_.state().position);
            const gameplay::DoorUpdate update
                = gameplay::updateDoor(door, step, locked, occupied);
            if (update.startedOpening) {
                playDoorAudio(gdp::audio::AudioEventKind::DoorOpen, edge);
            }
            if (update.startedClosing) {
                playDoorAudio(gdp::audio::AudioEventKind::DoorClose, edge);
            }
            if (update.fullyClosed && door.state == world::DoorState::Locked) {
                playDoorAudio(gdp::audio::AudioEventKind::DoorLock, edge);
            }
            if (!gameplay::isDoorPassable(door)) {
                blockedDoorEdges.push_back(edge);
            }
        }
    }
    if (world_ && playerEntity != nullptr) {
        // Runtime door passability preserves an occupied lockdown threshold.
        baseCollisionWorld_ = gameplay::makeRoomNavigationCollision(
            *world_, playerRoom, false, blockedDoorEdges);
    }
    collisionWorld_ = gameplay::collisionWorldWithProps(
        baseCollisionWorld_, entities_, playerRoom);
    const wkc::core::Vec3 previousPlayerPosition = player_.state().position;
    player_.fixedUpdate(playerCommand_, step, collisionWorld_);
    const std::optional<std::uint64_t> footstepEvent = gameplay::updateFootsteps(
        footsteps_, previousPlayerPosition, player_.state().position,
        player_.state().grounded, player_.state().alive);
    if (footstepEvent && audio_) {
        audio_->playAt(gdp::audio::AudioEventKind::Footstep,
            session_.state().currentSeed, *footstepEvent,
            player_.state().position, player_.state().position,
            player_.planarForwardDirection());
    }
    gameplay::updateViewMotion(
        viewMotion_, previousPlayerPosition, player_.state().position,
        player_.state().grounded, player_.state().alive, step);

    const auto adoptRoom = [this, &playerEntity, &playerRoom](
        const world::RoomInstance& destination,
        const std::string_view transitionKind) {
        const std::uint32_t sourceRoom = playerRoom;
        playerRoom = destination.id;
        playerEntity->roomId = playerRoom;
        playerEntity->transform.position = player_.state().position;
        std::vector<world::EdgeId> blockedEdges;
        blockedEdges.reserve(doorRuntimes_.size());
        for (const auto& [edge, door] : doorRuntimes_) {
            if (!gameplay::isDoorPassable(door)) {
                blockedEdges.push_back(edge);
            }
        }
        baseCollisionWorld_ = gameplay::makeRoomNavigationCollision(
            *world_, playerRoom, false, blockedEdges);
        collisionWorld_ = gameplay::collisionWorldWithProps(
            baseCollisionWorld_, entities_, playerRoom);
        std::ostringstream message;
        message << transitionKind << ' ' << sourceRoom << " -> " << playerRoom;
        log(LogLevel::Info, message.str());
    };

    if (playerEntity != nullptr && world_) {
        const auto enteredRoom = gameplay::roomContainingCharacter(
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
                const auto door = doorRuntimes_.find(transition->edge);
                const world::DoorState previousState = door == doorRuntimes_.end()
                    ? world::DoorState::Disabled : door->second.state;
                if (door != doorRuntimes_.end()
                    && gameplay::requestDoorOpen(door->second, false)) {
                    if (previousState != world::DoorState::Opening
                        && previousState != world::DoorState::Open) {
                        playDoorAudio(
                            gdp::audio::AudioEventKind::DoorOpen, transition->edge);
                    }
                    std::ostringstream message;
                    message << "Door opening on edge " << transition->edge;
                    log(LogLevel::Info, message.str());
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
                << " | armor +" << static_cast<int>(loot.armorAdded)
                << " | ammo +" << loot.ammoAdded
                << " | keys +" << loot.keysAdded;
        log(LogLevel::Info, message.str());
        if (audio_) {
            audio_->play(gdp::audio::AudioEventKind::Pickup,
                config_.worldSeed, audioEventCounter_++);
        }
        gameplay::signalNotice(feedback_, gameplay::NoticeFeedbackKind::Pickup);
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
        if (shot.outcome == gameplay::ShotOutcome::Hit
            || shot.outcome == gameplay::ShotOutcome::Destroyed) {
            gameplay::signalHit(feedback_, false);
        } else if (shot.outcome == gameplay::ShotOutcome::Killed) {
            gameplay::signalHit(feedback_, true);
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
    if (attacksLanded > 0U) {
        gameplay::signalPlayerDamage(feedback_);
    }
    if (playerWasAlive && !player_.state().alive) {
        log(LogLevel::Info, "Player died");
    }

    playerEntity = entities_.find(player_.entity());
    if (playerEntity != nullptr) {
        playerEntity->transform.position = player_.state().position;
        if (entities::PlayerData* data = std::get_if<entities::PlayerData>(&playerEntity->data)) {
            data->health = player_.state().health;
            data->armor = player_.state().armor;
            data->maximumArmor = player_.state().maximumArmor;
            data->keys = player_.state().keys;
        }
    }
    bool terminalUsed = false;
    if (playerCommand_.interact) {
        const entities::Entity* terminal = entities_.find(terminalEntity_);
        if (terminal != nullptr && terminal->roomId == playerRoom) {
            const wkc::core::Vec3 position = terminal->transform.position;
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
            gameplay::signalNotice(
                feedback_, gameplay::NoticeFeedbackKind::EventStart);
        }
        if (event.wavesSpawned > 0U) {
            std::ostringstream message;
            message << "Event wave " << activeEvent->nextWave << '/'
                    << activeEvent->plan.waveCount << " spawned "
                    << event.enemiesSpawned << " enemy(s)";
            log(LogLevel::Info, message.str());
            if (!event.activated) {
                gameplay::signalNotice(
                    feedback_, gameplay::NoticeFeedbackKind::Wave);
            }
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
            gameplay::signalNotice(
                feedback_, gameplay::NoticeFeedbackKind::EventClear);
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
        gameplay::openResultMenu(resultMenu_);
        if (!config_.headless && window_ != nullptr
            && !SDL_SetWindowRelativeMouseMode(window_, false)) {
            log(LogLevel::Warning, sdlError("Result mouse mode update failed"));
        }
    }
    if (audio_) {
        bool ambienceSelected = false;
        if (world_ && !gameplay::roomContainingPosition(
                *world_, player_.state().position)) {
            for (const world::CorridorPath& corridor : world_->corridors) {
                const std::optional<float> corridorProgress
                    = gameplay::corridorProgressAt(
                        corridor, player_.state().position);
                if (!corridorProgress) {
                    continue;
                }
                const auto firstRoom = std::find_if(
                    world_->rooms.begin(), world_->rooms.end(),
                    [&corridor](const world::RoomInstance& room) {
                        return room.id == corridor.firstRoom;
                    });
                const auto secondRoom = std::find_if(
                    world_->rooms.begin(), world_->rooms.end(),
                    [&corridor](const world::RoomInstance& room) {
                        return room.id == corridor.secondRoom;
                    });
                if (firstRoom == world_->rooms.end()
                    || secondRoom == world_->rooms.end()) {
                    break;
                }
                float blend = *corridorProgress;
                blend = blend * blend * (3.0F - (2.0F * blend));
                audio_->setAmbienceBlend(
                    firstRoom->seed, static_cast<std::uint64_t>(firstRoom->theme),
                    secondRoom->seed, static_cast<std::uint64_t>(secondRoom->theme),
                    blend);
                ambienceSelected = true;
                break;
            }
        }
        if (world_ && !ambienceSelected) {
            const auto currentRoom = std::find_if(
                world_->rooms.begin(), world_->rooms.end(),
                [playerRoom](const world::RoomInstance& room) {
                    return room.id == playerRoom;
                });
            if (currentRoom != world_->rooms.end()) {
                audio_->setAmbience(currentRoom->seed,
                    static_cast<std::uint64_t>(currentRoom->theme));
            }
        }
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

    if (settingsMenu_.active) {
        SDL_SetWindowTitle(window_, "GDP | SETTINGS");
        visual::HudRenderData hud;
        hud.settingsMenu = true;
        hud.settingsSelection = static_cast<visual::HudSettingsSelection>(
            settingsMenu_.selection);
        hud.masterVolumePercent = runtimeSettings_.masterVolumePercent;
        hud.effectsVolumePercent = runtimeSettings_.effectsVolumePercent;
        hud.ambientVolumePercent = runtimeSettings_.ambientVolumePercent;
        hud.uiVolumePercent = runtimeSettings_.uiVolumePercent;
        hud.mouseSensitivityPercent = runtimeSettings_.mouseSensitivityPercent;
        hud.invertMouseY = runtimeSettings_.invertMouseY;
        hud.fieldOfViewDegrees = runtimeSettings_.fieldOfViewDegrees;
        if (renderer_ == nullptr || !renderer_->render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, hud)) {
            log(LogLevel::Error, "Renderer failed while drawing settings");
            fatalError_ = true;
            running_ = false;
            return;
        }
        SDL_GL_SwapWindow(window_);
        return;
    }

    if (mainMenu_.active) {
        SDL_SetWindowTitle(window_, "GDP | MAIN MENU");
        visual::HudRenderData hud;
        hud.mainMenu = true;
        if (mainMenu_.selection == gameplay::MainMenuOption::Settings) {
            hud.mainMenuSelection = visual::HudMainMenuSelection::Settings;
        } else if (mainMenu_.selection == gameplay::MainMenuOption::ExitGame) {
            hud.mainMenuSelection = visual::HudMainMenuSelection::ExitGame;
        }
        if (renderer_ == nullptr || !renderer_->render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, hud)) {
            log(LogLevel::Error, "Renderer failed while drawing the main menu");
            fatalError_ = true;
            running_ = false;
            return;
        }
        SDL_GL_SwapWindow(window_);
        return;
    }

    const wkc::core::Vec3 renderCameraPosition = player_.cameraPosition()
        + gameplay::viewMotionOffset(viewMotion_, player_.rightDirection());
    const visual::CameraData camera{
        renderCameraPosition,
        player_.forwardDirection(),
        static_cast<float>(runtimeSettings_.fieldOfViewDegrees),
    };
    std::vector<visual::EnemyRenderData> visibleEnemies;
    std::vector<visual::PickupRenderData> visiblePickups;
    std::vector<visual::PropRenderData> visibleProps;
    const entities::Entity* playerEntity = entities_.find(player_.entity());
    const std::uint32_t playerRoom = playerEntity == nullptr ? 0 : playerEntity->roomId;
    std::vector<world::RoomId> visibleRoomIds = world_
        ? roomsWithinGraphDistance(*world_, playerRoom, 2U)
        : std::vector<world::RoomId>{playerRoom};
    const auto isVisibleRoom = [&visibleRoomIds](const world::RoomId roomId) {
        return std::find(visibleRoomIds.begin(), visibleRoomIds.end(), roomId)
            != visibleRoomIds.end();
    };
    for (const entities::EntityId id : entities_.query(entities::EntityKind::Enemy)) {
        const entities::Entity* entity = entities_.find(id);
        const entities::EnemyData* enemy = entities_.findData<entities::EnemyData>(id);
        if (entity != nullptr && enemy != nullptr && isVisibleRoom(entity->roomId)
            && (enemy->archetype == entities::EnemyArchetype::Worker
                || enemy->archetype == entities::EnemyArchetype::Guard
                || enemy->archetype == entities::EnemyArchetype::Officer
                || enemy->archetype == entities::EnemyArchetype::Engineer
                || enemy->archetype == entities::EnemyArchetype::IndustrialRobot)) {
            visual::EnemyVisualRole role = visual::EnemyVisualRole::Worker;
            if (enemy->archetype == entities::EnemyArchetype::Guard) {
                role = visual::EnemyVisualRole::Guard;
            } else if (enemy->archetype == entities::EnemyArchetype::Officer) {
                role = visual::EnemyVisualRole::Officer;
            } else if (enemy->archetype == entities::EnemyArchetype::Engineer) {
                role = visual::EnemyVisualRole::Engineer;
            } else if (enemy->archetype == entities::EnemyArchetype::IndustrialRobot) {
                role = visual::EnemyVisualRole::IndustrialRobot;
            }
            visibleEnemies.push_back({
                entity->transform.position,
                enemy->behavior != entities::EnemyBehaviorState::Dead,
                enemy->visualSeed,
                role,
                enemy->active,
            });
        }
    }
    for (const entities::EntityId id : entities_.query(entities::EntityKind::Item)) {
        const entities::Entity* entity = entities_.find(id);
        const entities::ItemData* item = entities_.findData<entities::ItemData>(id);
        if (entity == nullptr || item == nullptr || !isVisibleRoom(entity->roomId)) {
            continue;
        }
        visual::PickupVisualRole role = visual::PickupVisualRole::Health;
        if (item->type == entities::ItemType::Ammo) {
            role = visual::PickupVisualRole::Ammo;
        } else if (item->type == entities::ItemType::Key) {
            role = visual::PickupVisualRole::Key;
        } else if (item->type == entities::ItemType::Armor) {
            role = visual::PickupVisualRole::Armor;
        }
        visiblePickups.push_back({entity->transform.position, role});
    }
    for (const entities::EntityId id : entities_.query(entities::EntityKind::Prop)) {
        const entities::Entity* entity = entities_.find(id);
        const entities::PropData* prop = entities_.findData<entities::PropData>(id);
        if (entity == nullptr || prop == nullptr || !isVisibleRoom(entity->roomId)
            || (prop->type != entities::PropType::Crate
                && prop->type != entities::PropType::Canister
                && prop->type != entities::PropType::Barricade)) {
            continue;
        }
        visual::PropVisualRole role = visual::PropVisualRole::Crate;
        if (prop->type == entities::PropType::Canister) {
            role = visual::PropVisualRole::Canister;
        } else if (prop->type == entities::PropType::Barricade) {
            role = visual::PropVisualRole::Barricade;
        }
        visual::PropVisualState state = visual::PropVisualState::Intact;
        if (prop->state == entities::PropState::Damaged) {
            state = visual::PropVisualState::Damaged;
        } else if (prop->state == entities::PropState::Destroyed) {
            state = visual::PropVisualState::Destroyed;
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
          << " | Armor " << static_cast<int>(player_.state().armor)
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
    visual::RoomLightingRenderData lighting;
    visual::RoomRenderData roomRender;
    std::vector<visual::DoorRenderData> visibleDoors;
    std::vector<visual::CorridorRenderData> visibleCorridors;
    std::vector<visual::RoomRenderData> connectedRooms;
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
            roomRender.worldBounds = currentRoom->worldBounds;
            if (const auto material = roomMaterialKeys_.find(currentRoom->id);
                material != roomMaterialKeys_.end()) {
                roomRender.materialKey = material->second;
            }
            const std::size_t roomIndex = static_cast<std::size_t>(
                std::distance(world_->rooms.begin(), currentRoom));
            if (roomIndex < roomLighting_.size()) {
                const bool alert = activeEvent != roomEvents_.end()
                    && gameplay::isRoomLocked(*activeEvent);
                lighting = makeRenderLighting(roomLighting_[roomIndex], alert);
            }
            if (!gameplay::roomContainingPosition(*world_, player_.state().position)) {
                for (const world::CorridorPath& corridor : world_->corridors) {
                    if (corridor.firstRoom != playerRoom
                        && corridor.secondRoom != playerRoom) {
                        continue;
                    }
                    const std::optional<float> corridorProgress
                        = gameplay::corridorProgressAt(
                        corridor, player_.state().position);
                    if (!corridorProgress) {
                        continue;
                    }
                    const world::RoomId otherRoomId = corridor.firstRoom == playerRoom
                        ? corridor.secondRoom : corridor.firstRoom;
                    const auto otherRoom = std::find_if(
                        world_->rooms.begin(), world_->rooms.end(),
                        [otherRoomId](const world::RoomInstance& candidate) {
                            return candidate.id == otherRoomId;
                        });
                    if (otherRoom == world_->rooms.end()) {
                        break;
                    }
                    const std::size_t otherIndex = static_cast<std::size_t>(
                        std::distance(world_->rooms.begin(), otherRoom));
                    if (otherIndex >= roomLighting_.size()) {
                        break;
                    }
                    float otherWeight = corridor.firstRoom == playerRoom
                        ? *corridorProgress : 1.0F - *corridorProgress;
                    otherWeight = otherWeight * otherWeight * (3.0F - (2.0F * otherWeight));
                    const visual::RoomLightingRenderData otherLighting
                        = makeRenderLighting(roomLighting_[otherIndex], false);
                    lighting.ambientColor = (lighting.ambientColor * (1.0F - otherWeight))
                        + (otherLighting.ambientColor * otherWeight);
                    lighting.ambientIntensity
                        = (lighting.ambientIntensity * (1.0F - otherWeight))
                        + (otherLighting.ambientIntensity * otherWeight);
                    lighting.fixtureEmissive
                        = (lighting.fixtureEmissive * (1.0F - otherWeight))
                        + (otherLighting.fixtureEmissive * otherWeight);
                    break;
                }
            }
            appendConnectedRoomLights(
                lighting, *world_, roomLighting_, playerRoom, player_.state().position);
            if (session_.state().phase == gameplay::SessionPhase::Victory) {
                lighting.ambientColor = {0.35F, 0.82F, 0.72F};
                lighting.ambientIntensity = std::max(lighting.ambientIntensity, 0.34F);
                lighting.alert = false;
            } else if (session_.state().phase == gameplay::SessionPhase::Defeat) {
                lighting.ambientColor = {0.52F, 0.06F, 0.04F};
                lighting.ambientIntensity = 0.14F;
                lighting.alert = true;
            }
            const std::vector<world::RoomId> geometryAnchors
                = roomsWithinGraphDistance(*world_, playerRoom, 1U);
            std::vector<world::EdgeId> visibleEdges;
            for (const world::CorridorPath& corridor : world_->corridors) {
                const bool touchesAnchor = std::find(
                    geometryAnchors.begin(), geometryAnchors.end(), corridor.firstRoom)
                        != geometryAnchors.end()
                    || std::find(
                        geometryAnchors.begin(), geometryAnchors.end(), corridor.secondRoom)
                        != geometryAnchors.end();
                if (!touchesAnchor) {
                    continue;
                }
                visibleEdges.push_back(corridor.edge);
                visibleCorridors.push_back({
                    std::span<const wkc::core::Vec3>{corridor.points},
                    corridor.width,
                    corridor.height,
                    roomMaterialKeys_.contains(corridor.firstRoom)
                        ? roomMaterialKeys_.at(corridor.firstRoom) : 0U,
                });
            }
            const std::vector<world::RoomId> geometryRooms
                = roomsWithinGraphDistance(*world_, playerRoom, 2U);
            for (const world::RoomInstance& renderedRoom : world_->rooms) {
                if (renderedRoom.id == playerRoom
                    || std::find(geometryRooms.begin(), geometryRooms.end(), renderedRoom.id)
                        == geometryRooms.end()) {
                    continue;
                }
                connectedRooms.push_back({
                    renderedRoom.transform.position,
                    renderedRoom.rotation,
                    roomMaterialKeys_.contains(renderedRoom.id)
                        ? roomMaterialKeys_.at(renderedRoom.id) : 0U,
                    renderedRoom.worldBounds,
                });
            }
            const auto edgeIsLocked = [this, &visibleEdges](const world::EdgeId edge) {
                if (std::find(visibleEdges.begin(), visibleEdges.end(), edge)
                    == visibleEdges.end()) {
                    return false;
                }
                return std::any_of(
                    roomEvents_.begin(), roomEvents_.end(),
                    [this, edge](const gameplay::RoomEventRuntime& runtime) {
                        return gameplay::isRoomLocked(runtime)
                            && edgeTouchesRoom(*world_, edge, runtime.roomId);
                    });
            };
            for (const world::RoomInstance& renderedRoom : world_->rooms) {
                if (std::find(geometryRooms.begin(), geometryRooms.end(), renderedRoom.id)
                    == geometryRooms.end()) {
                    continue;
                }
                for (const world::ConnectorInstance& connector : renderedRoom.connectors) {
                    if (connector.sealed || !connector.edge
                        || std::find(visibleEdges.begin(), visibleEdges.end(), *connector.edge)
                            == visibleEdges.end()) {
                        continue;
                    }
                    const auto door = doorRuntimes_.find(*connector.edge);
                    visibleDoors.push_back({
                        connector.worldPosition,
                        connector.worldNormal,
                        connector.width,
                        connector.height,
                        edgeIsLocked(*connector.edge),
                        door == doorRuntimes_.end() ? 0.0F : door->second.openness,
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
    if (pauseMenu_.paused) {
        title << " | PAUSED";
    }
    SDL_SetWindowTitle(window_, title.str().c_str());
    visual::HudRenderData hud;
    hud.visible = true;
    hud.health = player_.state().health;
    hud.maximumHealth = player_.state().maximumHealth;
    hud.armor = player_.state().armor;
    hud.maximumArmor = player_.state().maximumArmor;
    hud.magazineAmmo = weapon_.magazineAmmo();
    hud.reserveAmmo = weapon_.reserveAmmo();
    hud.keys = player_.state().keys;
    hud.level = progress.level;
    hud.experience = progress.experienceIntoLevel;
    hud.experienceForNextLevel = nextLevelXp;
    hud.damageFeedback = gameplay::damageFeedbackStrength(feedback_);
    hud.hitFeedbackStrength = gameplay::hitFeedbackStrength(feedback_);
    if (feedback_.hit == gameplay::HitFeedbackKind::Hit) {
        hud.hitFeedback = visual::HudHitFeedback::Hit;
    } else if (feedback_.hit == gameplay::HitFeedbackKind::Kill) {
        hud.hitFeedback = visual::HudHitFeedback::Kill;
    }
    hud.noticeFeedbackStrength = gameplay::noticeFeedbackStrength(feedback_);
    hud.paused = pauseMenu_.paused;
    if (pauseMenu_.selection == gameplay::PauseMenuOption::RestartRun) {
        hud.pauseSelection = visual::HudPauseSelection::RestartRun;
    } else if (pauseMenu_.selection == gameplay::PauseMenuOption::Settings) {
        hud.pauseSelection = visual::HudPauseSelection::Settings;
    } else if (pauseMenu_.selection == gameplay::PauseMenuOption::MainMenu) {
        hud.pauseSelection = visual::HudPauseSelection::MainMenu;
    } else if (pauseMenu_.selection == gameplay::PauseMenuOption::ExitGame) {
        hud.pauseSelection = visual::HudPauseSelection::ExitGame;
    }
    if (resultMenu_.selection == gameplay::ResultMenuOption::MainMenu) {
        hud.resultMenuSelection = visual::HudResultMenuSelection::MainMenu;
    } else if (resultMenu_.selection == gameplay::ResultMenuOption::ExitGame) {
        hud.resultMenuSelection = visual::HudResultMenuSelection::ExitGame;
    }
    switch (feedback_.notice) {
    case gameplay::NoticeFeedbackKind::Pickup:
        hud.notice = visual::HudNotice::Pickup;
        break;
    case gameplay::NoticeFeedbackKind::EventStart:
        hud.notice = visual::HudNotice::EventStart;
        break;
    case gameplay::NoticeFeedbackKind::Wave:
        hud.notice = visual::HudNotice::Wave;
        break;
    case gameplay::NoticeFeedbackKind::EventClear:
        hud.notice = visual::HudNotice::EventClear;
        break;
    case gameplay::NoticeFeedbackKind::LevelUp:
        hud.notice = visual::HudNotice::LevelUp;
        break;
    case gameplay::NoticeFeedbackKind::None:
        break;
    }
    if (session_.state().phase == gameplay::SessionPhase::Victory) {
        hud.objective = visual::ObjectiveRenderState::Victory;
    } else if (session_.state().phase == gameplay::SessionPhase::Defeat) {
        hud.objective = visual::ObjectiveRenderState::Defeat;
    } else if (renderedRoomRole == world::RoomRole::Boss) {
        hud.objective = visual::ObjectiveRenderState::DefeatBoss;
    } else {
        hud.objective = visual::ObjectiveRenderState::ReachBoss;
    }
    if (activeEvent != roomEvents_.end()) {
        if (activeEvent->phase == gameplay::RoomEventPhase::Dormant) {
            hud.event = visual::HudEventState::Dormant;
        } else if (activeEvent->phase == gameplay::RoomEventPhase::Active) {
            hud.event = visual::HudEventState::Active;
        } else if (activeEvent->plan.type != world::RoomEventType::None) {
            hud.event = visual::HudEventState::Completed;
        }
        hud.eventLocked = gameplay::isRoomLocked(*activeEvent);
    }
    if (session_.state().phase == gameplay::SessionPhase::Playing && world_) {
        const std::optional<gameplay::RoomTransition> transition
            = gameplay::findRoomTransition(
                *world_, playerRoom, player_.state().position,
                player_.planarForwardDirection());
        if (transition) {
            const auto door = doorRuntimes_.find(transition->edge);
            if (hud.eventLocked) {
                hud.prompt = visual::HudPrompt::DoorLocked;
            } else if (door != doorRuntimes_.end()
                && door->second.state != world::DoorState::Opening
                && door->second.state != world::DoorState::Open) {
                hud.prompt = visual::HudPrompt::OpenDoor;
            }
        }
        if (hud.prompt == visual::HudPrompt::None
            && activeEvent != roomEvents_.end()
            && activeEvent->phase == gameplay::RoomEventPhase::Dormant
            && activeEvent->plan.trigger == world::RoomEventTrigger::TerminalUse) {
            const entities::Entity* terminal = entities_.find(terminalEntity_);
            if (terminal != nullptr && terminal->roomId == playerRoom) {
                const wkc::core::Vec3 position = terminal->transform.position;
                const std::array targets{
                    gameplay::InteractionTarget{
                        terminalEntity_,
                        {{position.x - 0.4F, position.y, position.z - 0.15F},
                            {position.x + 0.4F, position.y + 2.0F,
                                position.z + 0.15F}}},
                };
                if (player_.findInteraction(targets)) {
                    hud.prompt = visual::HudPrompt::UseTerminal;
                }
            }
        }
        if (hud.prompt == visual::HudPrompt::None
            && weapon_.magazineAmmo() == 0U && weapon_.reserveAmmo() > 0U) {
            hud.prompt = visual::HudPrompt::Reload;
        }
    }
    if (renderer_ == nullptr
        || !renderer_->render(
            width, height, camera, visibleEnemies, visiblePickups, visibleProps,
            lighting, roomRender, visibleDoors, visibleCorridors, connectedRooms, hud)) {
        log(LogLevel::Error, "Renderer failed while drawing the frame");
        fatalError_ = true;
        running_ = false;
        return;
    }
    SDL_GL_SwapWindow(window_);
}

void Application::shutdown()
{
    running_ = false;

    (void)savePersistedSettings();

    renderer_.reset();
    audio_.reset();
    entities_.clear();
    roomEvents_.clear();
    roomLighting_.clear();
    roomMaterialKeys_.clear();
    doorRuntimes_.clear();
    doorAudioEventCounters_.clear();
    input_ = Input{};
    player_ = gameplay::Player{};
    weapon_ = gameplay::Weapon{};
    progression_ = gameplay::ProgressionSystem{};
    gameplay::clearFeedback(feedback_);
    gameplay::resetFootsteps(footsteps_);
    gameplay::resetViewMotion(viewMotion_);
    gameplay::openMainMenu(mainMenu_);
    gameplay::closePauseMenu(pauseMenu_);
    gameplay::closeResultMenu(resultMenu_);
    gameplay::closeSettingsMenu(settingsMenu_);
    baseCollisionWorld_ = gameplay::CollisionWorld::fixedRoom();
    collisionWorld_ = baseCollisionWorld_;
    playerCommand_ = {};
    interactionPending_ = false;
    firePending_ = false;
    reloadPending_ = false;
    audioEventCounter_ = 0U;
    uiAudioEventCounter_ = 0U;
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

} // namespace gdp::game
