#include "game/Application.hpp"

#include "game/audio/AudioSystem.hpp"
#include "game/HudBoss.hpp"
#include "game/HudMap.hpp"
#include "game/Localization.hpp"
#include "game/SeedDomains.hpp"
#if GDP_ENABLE_TEST_MODE
#include "game/TestArena.hpp"
#endif
#include "core/Hash.hpp"
#include "core/Log.hpp"
#include "gameplay/Combat.hpp"
#include "gameplay/Enemy.hpp"
#include "gameplay/Loot.hpp"
#include "gameplay/MainMenu.hpp"
#include "gameplay/Props.hpp"
#include "gameplay/RoomEvents.hpp"
#include "gameplay/RoomTraversal.hpp"
#include "gameplay/Speedrun.hpp"
#include "persistence/CampaignCheckpointStore.hpp"
#include "persistence/SpeedrunRecordStore.hpp"
#include "persistence/SpeedrunLeaderboardStore.hpp"
#include "persistence/WorkerBrainStore.hpp"
#include "persistence/SettingsStore.hpp"
#include "platform/SdlInput.hpp"
#include "game/visual/GdpRenderData.hpp"
#include "game/visual/GdpRenderer.hpp"
#include "game/visual/MenuBackdrop.hpp"
#include "world/EventGenerator.hpp"
#include "world/LightingGenerator.hpp"
#include "world/WorldEntities.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cmath>
#include <numbers>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
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

constexpr float impactVisualDuration = 0.32F;
constexpr float grenadeBlastVisualDuration = 1.10F;
constexpr float weaponRecoilDuration = 0.12F;
constexpr float weaponMeleeDuration = 0.28F;
constexpr float weaponMuzzleFlashDuration = 0.055F;
constexpr float weaponEquipDuration = 0.48F;

visual::ObjectiveRenderState campaignMissionObjective(
    const gameplay::CampaignMissionState& mission) noexcept
{
    switch (mission.kind) {
    case gameplay::CampaignMissionKind::RecoverKey:
        return visual::ObjectiveRenderState::RecoverKey;
    case gameplay::CampaignMissionKind::SecureTerminal:
        return mission.activated ? visual::ObjectiveRenderState::ClearAmbush
                                 : visual::ObjectiveRenderState::ActivateTerminal;
    case gameplay::CampaignMissionKind::RecoverAndSecure:
        return mission.keyRecovered ? visual::ObjectiveRenderState::SecureArena
                                    : visual::ObjectiveRenderState::RecoverKey;
    case gameplay::CampaignMissionKind::SecureArena:
        return visual::ObjectiveRenderState::SecureArena;
    }
    return visual::ObjectiveRenderState::None;
}

std::uint32_t roundedPerformanceMetric(const double value) noexcept
{
    if (!std::isfinite(value) || value <= 0.0) {
        return 0U;
    }
    const double maximum = static_cast<double>(
        std::numeric_limits<std::uint32_t>::max());
    return static_cast<std::uint32_t>(std::lround(std::min(value, maximum)));
}

std::string benchmarkToken(const std::string_view value)
{
    std::string token;
    token.reserve(value.size());
    bool previousSeparator = false;
    for (const char character : value) {
        const unsigned char byte = static_cast<unsigned char>(character);
        if (std::isalnum(byte) != 0 || character == '.' || character == '-') {
            token.push_back(character);
            previousSeparator = false;
        } else if (!previousSeparator && !token.empty()) {
            token.push_back('_');
            previousSeparator = true;
        }
    }
    while (!token.empty() && token.back() == '_') {
        token.pop_back();
    }
    return token.empty() ? std::string{"unknown"} : token;
}

visual::ImpactVisualMaterial impactVisualMaterial(
    const gameplay::ImpactMaterial material) noexcept
{
    switch (material) {
    case gameplay::ImpactMaterial::Metal:
        return visual::ImpactVisualMaterial::Metal;
    case gameplay::ImpactMaterial::Composite:
        return visual::ImpactVisualMaterial::Composite;
    case gameplay::ImpactMaterial::Flesh:
        return visual::ImpactVisualMaterial::Flesh;
    }
    return visual::ImpactVisualMaterial::Metal;
}

static_assert(static_cast<std::uint8_t>(gameplay::SettingsMenuOption::MasterVolume)
    == static_cast<std::uint8_t>(visual::HudSettingsSelection::MasterVolume));
static_assert(static_cast<std::uint8_t>(gameplay::SettingsMenuOption::EffectsVolume)
    == static_cast<std::uint8_t>(visual::HudSettingsSelection::EffectsVolume));
static_assert(static_cast<std::uint8_t>(gameplay::SettingsMenuOption::AmbientVolume)
    == static_cast<std::uint8_t>(visual::HudSettingsSelection::AmbientVolume));
static_assert(static_cast<std::uint8_t>(gameplay::SettingsMenuOption::UiVolume)
    == static_cast<std::uint8_t>(visual::HudSettingsSelection::UiVolume));
static_assert(static_cast<std::uint8_t>(gameplay::SettingsMenuOption::MouseSensitivity)
    == static_cast<std::uint8_t>(visual::HudSettingsSelection::MouseSensitivity));
static_assert(static_cast<std::uint8_t>(gameplay::SettingsMenuOption::InvertMouseY)
    == static_cast<std::uint8_t>(visual::HudSettingsSelection::InvertMouseY));
static_assert(static_cast<std::uint8_t>(gameplay::SettingsMenuOption::FieldOfView)
    == static_cast<std::uint8_t>(visual::HudSettingsSelection::FieldOfView));
static_assert(static_cast<std::uint8_t>(gameplay::SettingsMenuOption::GraphicsQuality)
    == static_cast<std::uint8_t>(visual::HudSettingsSelection::GraphicsQuality));
static_assert(static_cast<std::uint8_t>(gameplay::SettingsMenuOption::WindowResolution)
    == static_cast<std::uint8_t>(visual::HudSettingsSelection::WindowResolution));
static_assert(static_cast<std::uint8_t>(gameplay::SettingsMenuOption::Fullscreen)
    == static_cast<std::uint8_t>(visual::HudSettingsSelection::Fullscreen));
static_assert(static_cast<std::uint8_t>(gameplay::SettingsMenuOption::VerticalSync)
    == static_cast<std::uint8_t>(visual::HudSettingsSelection::VerticalSync));
static_assert(static_cast<std::uint8_t>(gameplay::SettingsMenuOption::Language)
    == static_cast<std::uint8_t>(visual::HudSettingsSelection::Language));
static_assert(static_cast<std::uint8_t>(gameplay::SettingsMenuOption::Back)
    == static_cast<std::uint8_t>(visual::HudSettingsSelection::Back));
static_assert(static_cast<std::uint8_t>(gameplay::SettingsMenuOption::Count)
    == static_cast<std::uint8_t>(visual::HudSettingsSelection::Count));
static_assert(static_cast<std::uint8_t>(gameplay::GraphicsQuality::Potato)
    == static_cast<std::uint8_t>(visual::HudGraphicsQuality::Potato));
static_assert(static_cast<std::uint8_t>(gameplay::GraphicsQuality::Ultra)
    == static_cast<std::uint8_t>(visual::HudGraphicsQuality::Ultra));

std::string sdlError(const std::string_view operation)
{
    std::ostringstream stream;
    stream << operation << ": " << SDL_GetError();
    return stream.str();
}

visual::EnemyAnimationState animationStateFor(
    const entities::EnemyData& enemy) noexcept
{
    if (enemy.behavior == entities::EnemyBehaviorState::Dead) {
        return visual::EnemyAnimationState::Dead;
    }
    if (enemy.hitReactionTime > 0.0F) {
        return visual::EnemyAnimationState::Hit;
    }
    if (enemy.behavior == entities::EnemyBehaviorState::Alert) {
        return visual::EnemyAnimationState::Alert;
    }
    if (enemy.behavior == entities::EnemyBehaviorState::Recover) {
        return visual::EnemyAnimationState::Recover;
    }
    if (enemy.behavior == entities::EnemyBehaviorState::Chase
        || enemy.behavior == entities::EnemyBehaviorState::Reposition
        || enemy.behavior == entities::EnemyBehaviorState::Patrol
        || enemy.behavior == entities::EnemyBehaviorState::Investigate) {
        return visual::EnemyAnimationState::Walk;
    }
    if (enemy.behavior == entities::EnemyBehaviorState::Attack) {
        return visual::EnemyAnimationState::Attack;
    }
    return visual::EnemyAnimationState::Idle;
}

visual::EnemyVisualRole visualRoleFor(
    const entities::EnemyData& enemy) noexcept
{
    switch (enemy.archetype) {
    case entities::EnemyArchetype::Worker:
        return visual::EnemyVisualRole::Worker;
    case entities::EnemyArchetype::Guard:
        return visual::EnemyVisualRole::Guard;
    case entities::EnemyArchetype::Officer:
        return visual::EnemyVisualRole::Officer;
    case entities::EnemyArchetype::Engineer:
        return visual::EnemyVisualRole::Engineer;
    case entities::EnemyArchetype::IndustrialRobot:
        return visual::EnemyVisualRole::IndustrialRobot;
    }
    return visual::EnemyVisualRole::Worker;
}

visual::RoomLightingRenderData makeRenderLighting(
    const gdp::world::RoomLightingPlan& plan,
    const bool alert,
    const entities::EntityManager& entities,
    const std::unordered_map<gdp::world::RoomId,
        std::vector<entities::EntityId>>& roomFixtureEntities,
    const gdp::world::RoomId roomId)
{
    static_assert(gdp::world::maxRoomLights <= visual::maxRenderedRoomLights);
    visual::RoomLightingRenderData lighting;
    lighting.ambientColor = plan.ambientColor;
    lighting.ambientIntensity = plan.ambientIntensity;
    lighting.lightCount = static_cast<std::uint32_t>(plan.lights.size());
    lighting.fixtureEmissive = plan.fixtureEmissive;
    lighting.alert = alert && plan.supportsAlert;
    const auto fixtureIds = roomFixtureEntities.find(roomId);
    for (std::size_t index = 0; index < plan.lights.size(); ++index) {
        const gdp::world::RoomPointLight& source = plan.lights[index];
        visual::PropVisualState fixtureState = visual::PropVisualState::Intact;
        if (fixtureIds != roomFixtureEntities.end()
            && index < fixtureIds->second.size()) {
            const entities::PropData* prop = entities.findData<entities::PropData>(
                fixtureIds->second[index]);
            if (prop != nullptr
                && prop->type == entities::PropType::CeilingFixture) {
                if (prop->state == entities::PropState::Destroyed) {
                    fixtureState = visual::PropVisualState::Destroyed;
                } else if (prop->state == entities::PropState::Damaged) {
                    fixtureState = visual::PropVisualState::Damaged;
                }
            }
        }
        const float outputScale
            = fixtureState == visual::PropVisualState::Destroyed ? 0.0F
            : fixtureState == visual::PropVisualState::Damaged ? 0.45F : 1.0F;
        lighting.lights[index] = {
            source.position,
            source.color,
            source.intensity * outputScale,
            source.range,
            fixtureState,
            outputScale,
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
    const entities::EntityManager& entities,
    const std::unordered_map<gdp::world::RoomId,
        std::vector<entities::EntityId>>& roomFixtureEntities,
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
        const visual::RoomLightingRenderData rendered
            = makeRenderLighting(
                plan, false, entities, roomFixtureEntities, roomId);
        for (std::size_t lightIndex = 0; lightIndex < rendered.lightCount; ++lightIndex) {
            const visual::PointLightRenderData& source = rendered.lights[lightIndex];
            const wkc::core::Vec3 offset = source.position - observerPosition;
            candidates.push_back({
                source,
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
        return "Capacity +1 magazine / +4 shells";
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

bool positionNearDoor(
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
    if (config_.testMode) (void)session_.selectRunMode(gameplay::RunMode::TestMode);
}

Application::~Application()
{
    shutdown();
}

int Application::run()
{
    using Clock = std::chrono::steady_clock;
    const bool benchmarkMode = config_.benchmarkQuality.has_value();
    bool beginAnotherRun = false;
    bool launchRunImmediately = config_.headless || benchmarkMode;
    do {
        restartRequested_ = false;
        restartMode_ = gameplay::RunRestartMode::NextSeed;
        fatalError_ = false;
        renderedFrames_ = 0U;
        fixedUpdates_ = 0U;
        performanceMonitor_.reset();
        currentFramePerformance_ = {};
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

            const double frameDelta = (config_.headless || benchmarkMode)
                ? clock_.fixedStepSeconds() : elapsed.count();
            clock_.advanceBy(frameDelta);

            currentFramePerformance_ = {};
            const auto updateStart = Clock::now();
            input_.beginFrame();
            if (benchmarkMode) {
                SDL_Event event;
                while (SDL_PollEvent(&event)) {
                    if (event.type == SDL_EVENT_QUIT) {
                        running_ = false;
                    }
                }
                input_ = Input{};
            } else {
                processEvents();
            }
            if (!running_) {
                continue;
            }
            if (session_.state().phase == gameplay::SessionPhase::Playing
                && !mainMenu_.active && !pauseMenu_.paused
                && !settingsMenu_.active) {
                const Vec2 rawLook = runtimeSettings_.gamepadSouthpaw
                    ? input_.gamepadLeftStick() : input_.gamepadRightStick();
                const Vec2 gamepadLook = wkc::platform::gamepadDeadzone(rawLook,
                    static_cast<float>(runtimeSettings_.gamepadLookDeadzonePercent) / 100.0F);
                const bool aiming = gameplay::controlDown(runtimeSettings_.controls,
                    runtimeSettings_.gamepadControls, gameplay::ControlAction::Aim, input_);
                const float aimScale = aiming
                    ? static_cast<float>(runtimeSettings_.gamepadAimSensitivityPercent) / 100.0F : 1.0F;
                player_.applyLook(input_.mouseDelta());
                player_.applyGamepadLook(gameplay::gamepadLookDegrees(gamepadLook,
                    static_cast<float>(frameDelta),
                    aimScale * static_cast<float>(runtimeSettings_.gamepadLookHorizontalPercent) / 100.0F,
                    aimScale * static_cast<float>(runtimeSettings_.gamepadLookVerticalPercent) / 100.0F),
                    runtimeSettings_.invertGamepadY);
            }

            std::uint32_t fixedStepsThisFrame = 0U;
            while (clock_.consumeFixedStep()) {
                fixedUpdate(clock_.fixedStepSeconds());
                ++fixedStepsThisFrame;
            }
            const auto updateEnd = Clock::now();
            currentFramePerformance_.updateMilliseconds
                = std::chrono::duration<double, std::milli>(
                    updateEnd - updateStart).count();
            currentFramePerformance_.fixedUpdates = fixedStepsThisFrame;

            render();
            const auto frameEnd = Clock::now();
            currentFramePerformance_.frameMilliseconds
                = std::chrono::duration<double, std::milli>(
                    frameEnd - currentTime).count();
            performanceMonitor_.record(currentFramePerformance_);
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
        const PerformanceSnapshot performance = performanceMonitor_.snapshot();
        if (performance.sampleCount > 0U) {
            std::ostringstream performanceSummary;
            performanceSummary << std::fixed << std::setprecision(2)
                << "Performance " << performance.sampleCount << " frame window: avg "
                << performance.averageFrameMilliseconds << " ms, P95 "
                << performance.percentile95FrameMilliseconds << " ms, update "
                << performance.averageUpdateMilliseconds << " ms, render "
                << performance.averageRenderMilliseconds << " ms, present "
                << performance.averagePresentMilliseconds << " ms";
            if (performance.gpuSampleCount > 0U) {
                performanceSummary << ", GPU "
                    << performance.averageGpuMilliseconds << " ms";
            }
            performanceSummary << ", load "
                << performance.worldGenerationMilliseconds << "/"
                << performance.assetPreparationMilliseconds << " ms";
            log(LogLevel::Info, performanceSummary.str());
        }
        if (benchmarkMode && performance.sampleCount > 0U) {
            int benchmarkWidth = config_.windowWidth;
            int benchmarkHeight = config_.windowHeight;
            if (window_ != nullptr) {
                (void)SDL_GetWindowSizeInPixels(
                    window_, &benchmarkWidth, &benchmarkHeight);
            }
            const FramePerformanceSample& latest = performance.latest;
            std::ostringstream benchmarkSummary;
            benchmarkSummary << std::fixed << std::setprecision(2)
                << "BENCHMARK profile="
                << gameplay::graphicsQualityName(*config_.benchmarkQuality)
                << " seed=" << config_.worldSeed
                << " resolution=" << benchmarkWidth << 'x' << benchmarkHeight
                << " frames=" << renderedFrames_
                << " samples=" << performance.sampleCount
                << " avg_ms=" << performance.averageFrameMilliseconds
                << " p95_ms=" << performance.percentile95FrameMilliseconds
                << " worst_ms=" << performance.worstFrameMilliseconds
                << " fps=" << performance.estimatedFramesPerSecond
                << " over_budget_pct=" << performance.framesOverBudgetPercent
                << " update_ms=" << performance.averageUpdateMilliseconds
                << " render_ms=" << performance.averageRenderMilliseconds
                << " present_ms=" << performance.averagePresentMilliseconds
                << " gpu_ms=" << performance.averageGpuMilliseconds
                << " gpu_samples=" << performance.gpuSampleCount
                << " draws=" << latest.drawCalls
                << " triangles=" << latest.triangles
                << " rooms=" << latest.visibleRooms
                << " enemies=" << latest.visibleEnemies
                << " props=" << latest.visibleProps
                << " lights=" << latest.activeLights
                << " shadow_lights=" << latest.shadowLights
                << " shadow_occluders=" << latest.shadowOccluders
                << " low_spec=" << (latest.lowSpecShader ? 1 : 0)
                << " culled_draws=" << latest.backfaceCulledDraws
                << " instanced_draws=" << latest.instancedDrawCalls
                << " instances=" << latest.renderedInstances
                << " saved_draws=" << latest.avoidedDrawCalls
                << " shadow_capacity="
                << latest.shadowShaderOccluderCapacity
                << " gpu_vendor=" << benchmarkToken(renderer_->gpuVendor())
                << " gpu_renderer=" << benchmarkToken(renderer_->gpuRenderer())
                << " gpu_version=" << benchmarkToken(renderer_->gpuVersion());
            log(LogLevel::Info, benchmarkSummary.str());
        }
        beginAnotherRun = restartRequested_;
        shutdown();
        if (fatalError_) {
            return 1;
        }
        if (beginAnotherRun) {
            const auto nextSeed = session_.restart(restartMode_);
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

    const SDL_InitFlags flags = config_.headless ? SDL_INIT_EVENTS
        : SDL_INIT_VIDEO;
    if (!SDL_Init(flags)) {
        log(LogLevel::Error, sdlError("SDL_Init failed"));
        return false;
    }
    sdlInitialized_ = true;
    if (!config_.headless && config_.enableGamepad) {
        if (SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
            gamepad_.refresh();
        } else {
            log(LogLevel::Warning,
                sdlError("Gamepad unavailable; keyboard and mouse remain active"));
        }
    }

    loadPersistedSettings();
    loadPersistedSpeedrunRecord();
    loadPersistedCampaignCheckpoint();

    audio_ = std::make_unique<gdp::audio::AudioSystem>();
    (void)audio_->initialize(!config_.headless && !config_.benchmarkQuality);
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
    const gameplay::WindowResolutionSize windowSize
        = gameplay::windowResolutionSize(runtimeSettings_.windowResolution);
    window_ = SDL_CreateWindow("GDP", static_cast<int>(windowSize.width),
        static_cast<int>(windowSize.height), windowFlags);
    if (window_ == nullptr) {
        log(LogLevel::Error, sdlError("SDL_CreateWindow failed"));
        return false;
    }
    applyRuntimeSettings();

    glContext_ = SDL_GL_CreateContext(window_);
    if (glContext_ == nullptr) {
        log(LogLevel::Error, sdlError("SDL_GL_CreateContext failed"));
        return false;
    }

    if (!SDL_GL_MakeCurrent(window_, glContext_)) {
        log(LogLevel::Error, sdlError("SDL_GL_MakeCurrent failed"));
        return false;
    }

    applyRuntimeSettings();
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
    const bool fullscreen = (SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN) != 0U;
    int actualWindowWidth = 0;
    int actualWindowHeight = 0;
    (void)SDL_GetWindowSize(window_, &actualWindowWidth, &actualWindowHeight);
    std::ostringstream windowSummary;
    windowSummary << "Window initialized in "
                  << (fullscreen ? "fullscreen" : "windowed")
                  << " mode at " << actualWindowWidth << 'x' << actualWindowHeight;
    log(LogLevel::Info, windowSummary.str());
    const gameplay::GraphicsQualitySettings initializedQuality
        = gameplay::graphicsQualitySettings(runtimeSettings_.graphicsQuality);
    std::ostringstream qualitySummary;
    qualitySummary << "Graphics quality "
                   << gameplay::graphicsQualityName(runtimeSettings_.graphicsQuality)
                   << ": scale " << initializedQuality.renderScalePercent
                   << "%, lights " << static_cast<unsigned>(initializedQuality.maximumLights)
                   << ", shadows "
                   << static_cast<unsigned>(initializedQuality.maximumShadowLights)
                   << 'x'
                   << static_cast<unsigned>(initializedQuality.maximumShadowOccluders)
                   << ", rooms "
                   << static_cast<unsigned>(initializedQuality.visibleRoomDepth)
                   << ", detailed materials "
                   << (initializedQuality.detailedMaterials ? "on" : "off");
    log(LogLevel::Info, qualitySummary.str());
    int swapInterval = 0;
    if (SDL_GL_GetSwapInterval(&swapInterval)) {
        log(LogLevel::Info, swapInterval > 0
                ? "VSync initialized enabled"
                : "VSync initialized disabled");
    }
    gameplay::openMainMenu(mainMenu_);
    log(LogLevel::Info, "GDP initialized with an OpenGL 3.3 core context");
    log(LogLevel::Info, "Main menu initialized before world generation");
    if (config_.testMode) return initializeRun();
    return true;
}

bool Application::initializeRun()
{
    using Clock = std::chrono::steady_clock;
    performanceMonitor_.reset();
    campaignMission_ = {};
    for (auto& brain : activeEnemyBrains_) {
        brain.reset();
    }
    if (config_.scriptedEnemies) {
        neuralEnemyExperiment_.reset();
        log(LogLevel::Info, "Scripted enemy AI selected");
    } else if (config_.experimentalNeuralEnemy) {
        neuralEnemyExperiment_.emplace(wkc::core::deriveSeed(
            config_.worldSeed, SeedDomain::NeuralWorker,
            static_cast<std::uint64_t>(*config_.experimentalNeuralEnemy)),
            *config_.experimentalNeuralEnemy);
        if (config_.neuralBrainPath) {
            const auto record = persistence::loadEnemyBrain(*config_.neuralBrainPath);
            if (!record || record->archetype != *config_.experimentalNeuralEnemy) {
                log(LogLevel::Error, "Cannot load matching experimental enemy brain");
                return false;
            }
            neuralEnemyExperiment_->genome = record->genome;
            neuralEnemyExperiment_->contract = record->contract;
        }
    } else {
        neuralEnemyExperiment_.reset();
        const char* const basePath = SDL_GetBasePath();
        if (basePath == nullptr && !config_.testBrainDirectory) {
            log(LogLevel::Warning,
                "Cannot find bundled enemy brains; using scripted AI");
        } else {
            const std::filesystem::path brainDirectory
                = config_.testBrainDirectory.value_or(
                    std::filesystem::path{basePath != nullptr ? basePath : ""}
                        / "ai");
            auto records = ai::loadActiveEnemyBrains(brainDirectory);
            constexpr std::array<std::string_view, ai::activeEnemyBrainCount> names{
                "worker", "guard", "officer", "engineer", "robot"};
            if (config_.testBrainDirectory
                && std::any_of(records.begin(), records.end(),
                    [](const auto& record) { return !record; })) {
                log(LogLevel::Error,
                    "Test brain directory needs five valid matching TYPE.brain files");
                return false;
            }
            for (std::size_t index = 0U; index < records.size(); ++index) {
                if (!records[index]) {
                    log(LogLevel::Warning,
                        std::string{"No valid bundled "} + std::string{names[index]}
                            + " brain; using scripted AI for that type");
                    continue;
                }
                const auto archetype
                    = static_cast<entities::EnemyArchetype>(index);
                const std::uint32_t brainGeneration
                    = records[index]->genome.generation;
                auto& brain = activeEnemyBrains_[index];
                brain.emplace(wkc::core::deriveSeed(
                    config_.worldSeed, SeedDomain::NeuralWorker,
                    static_cast<std::uint64_t>(archetype)), archetype);
                brain->genome = std::move(records[index]->genome);
                brain->contract = records[index]->contract;
                brain->controlAll = true;
                brain->logSelection = false;
                log(LogLevel::Info, config_.testBrainDirectory
                    ? std::string{"Test enemy brain active: "}
                        + std::string{names[index]} + " generation "
                        + std::to_string(brainGeneration)
                    : std::string{"Trained enemy brain active: "}
                        + std::string{names[index]});
            }
        }
    }
    currentFramePerformance_ = {};
    enemyAnimationTicks_ = 0U;
    enemyAnimationTransitions_.clear();
    activeImpacts_.clear();
    activeGrenadeBlasts_.clear();
    playerGrenades_.clear();
    grenadeSelected_ = false;
    grenadeCook_.reset();
    grenadeThrowRemaining_ = 0.0F;
    grenadeThrowChargeStrength_ = 0.0F;
    enemyProjectiles_.clear();
    roomFixtureEntities_.clear();
    visitedRooms_.clear();
    roomEntryGraceRemaining_.clear();
    impactVisualCounter_ = 0U;
    jumpPending_ = false;
    weaponSwitchPending_.reset();
    dropPending_ = false;
    weaponRecoilRemaining_ = 0.0F;
    aimBlend_ = 0.0F;
    flashlightOn_ = false;
    weaponMeleeRemaining_ = 0.0F;
    weaponMuzzleFlashRemaining_ = 0.0F;
    weaponEquipRemaining_ = weaponEquipDuration;
    latestSpeedrunRecordResult_ = gameplay::SpeedrunRecordResult::Ineligible;
    runDifficulty_ = session_.state().mode == gameplay::RunMode::Campaign
        ? gameplay::campaignDifficultyMultiplier(session_.state().campaignFloor, session_.state().difficulty)
        : gameplay::baseDifficultyMultiplier(session_.state().difficulty);
    const auto worldGenerationStart = Clock::now();
    world::WorldGenerationConfig worldConfig;
    if (session_.state().mode == gameplay::RunMode::Campaign) {
        const auto layout = gameplay::campaignFloorLayout(
            config_.worldSeed, session_.state().campaignFloor);
        worldConfig.roomCount = layout.roomCount;
        worldConfig.loopBudget = layout.loopBudget;
        worldConfig.maximumAttempts = 64U;
    }
    world::WorldGenerationResult generation = world::generateWorld(
        config_.worldSeed, worldConfig);
#if GDP_ENABLE_TEST_MODE
    if (session_.state().mode == gameplay::RunMode::TestMode) generation.world = makeTestArena(config_.worldSeed);
#endif
    performanceMonitor_.setWorldGenerationMilliseconds(
        std::chrono::duration<double, std::milli>(
            Clock::now() - worldGenerationStart).count());
    if (!generation.succeeded()) {
        log(LogLevel::Error, "World generation exhausted all deterministic attempts");
        return false;
    }
    world_ = std::move(generation.world);
    const auto bossRoom = std::find_if(world_->rooms.begin(), world_->rooms.end(),
        [](const world::RoomInstance& room) {
            return room.role == world::RoomRole::Boss;
        });
    bossRoomId_ = bossRoom == world_->rooms.end() ? 0U : bossRoom->id;
    const auto startRoom = std::find_if(
        world_->rooms.begin(), world_->rooms.end(),
        [](const world::RoomInstance& room) { return room.role == world::RoomRole::Start; });
    if (startRoom == world_->rooms.end()) {
        log(LogLevel::Error, "Generated world does not contain a START room");
        return false;
    }
    visitedRooms_.push_back(startRoom->id);

    const wkc::core::Vec3 startForward = wkc::core::rotateQuarterTurns(
        {0.0F, 0.0F, -1.0F}, startRoom->rotation);
    const wkc::core::Vec3 startPosition = startRoom->transform.position
        + wkc::core::rotateQuarterTurns({0.0F, 0.0F, 4.0F}, startRoom->rotation);
    player_.spawn(startPosition, yawForDirection(startForward));
    playerBodyMotion_ = {};
    if (campaignCarryover_) {
        if (!gameplay::restoreCampaignCarryover(
                *campaignCarryover_, player_, weapon_, progression_)) {
            log(LogLevel::Error,
                "Campaign carry-over failed validation on the next floor");
            return false;
        }
        campaignCarryover_.reset();
        log(LogLevel::Info, "Campaign progress restored on the next floor");
    }
    if (session_.state().mode == gameplay::RunMode::Campaign) {
        gameplay::CampaignCarryover floorEntry
            = gameplay::captureCampaignCarryover(
                player_, weapon_, progression_);
        if (!gameplay::validateCampaignCarryover(floorEntry)) {
            log(LogLevel::Error,
                "Campaign floor entry produced invalid carry-over state");
            return false;
        }
        campaignFloorEntry_ = std::move(floorEntry);
    } else {
        campaignFloorEntry_.reset();
    }
    world::InitialEntitySet initialEntities;
#if GDP_ENABLE_TEST_MODE
    if (session_.state().mode == gameplay::RunMode::TestMode) {
        initialEntities = populateTestArena(*world_, entities_, {
            .transform = {.position = player_.state().position}, .health = player_.state().health,
            .armor = player_.state().armor, .maximumArmor = player_.state().maximumArmor});
        (void)weapon_.equip(gameplay::WeaponArchetype::Carbine);
    } else
#endif
    initialEntities = world::populateInitialEntities(
        *world_, entities_,
        {
            .transform = {.position = player_.state().position},
            .health = player_.state().health,
            .armor = player_.state().armor,
            .maximumArmor = player_.state().maximumArmor,
        },
        runDifficulty_, session_.state().mode == gameplay::RunMode::Campaign,
        session_.state().mode == gameplay::RunMode::Campaign
            ? session_.state().campaignFloor : 1U);
    if (!initialEntities.isValid()) {
        log(LogLevel::Error, "Initial entity population did not find a START room");
        return false;
    }
    player_.bindEntity(initialEntities.player);
    terminalEntity_ = initialEntities.terminal;
    exitTerminalEntity_ = initialEntities.exitTerminal;
    if (session_.state().mode == gameplay::RunMode::Campaign
        && !exitTerminalEntity_.isValid()) {
        log(LogLevel::Error, "Campaign Boss room is missing its exit terminal");
        return false;
    }
    if (session_.state().mode == gameplay::RunMode::Campaign) {
        campaignMission_.kind = gameplay::campaignMissionKind(
            session_.state().initialSeed, session_.state().campaignFloor);
        const world::RoomRole targetRole = (campaignMission_.kind
                == gameplay::CampaignMissionKind::RecoverKey
                || campaignMission_.kind == gameplay::CampaignMissionKind::RecoverAndSecure)
            ? world::RoomRole::Secret
            : campaignMission_.kind == gameplay::CampaignMissionKind::SecureTerminal
                ? world::RoomRole::Start : world::RoomRole::Arena;
        const auto target = std::find_if(world_->rooms.begin(), world_->rooms.end(),
            [targetRole](const world::RoomInstance& room) {
                return room.role == targetRole;
            });
        if (target == world_->rooms.end()) {
            log(LogLevel::Error, "Campaign mission target room is missing");
            return false;
        }
        campaignMission_.targetRoomId = target->id;
        if (campaignMission_.kind == gameplay::CampaignMissionKind::RecoverAndSecure) {
            const auto arena = std::find_if(world_->rooms.begin(), world_->rooms.end(),
                [](const world::RoomInstance& room) {
                    return room.role == world::RoomRole::Arena;
                });
            if (arena == world_->rooms.end()) {
                log(LogLevel::Error, "Campaign route mission has no Arena");
                return false;
            }
            campaignMission_.secondaryRoomId = arena->id;
        }
        if (campaignMission_.kind == gameplay::CampaignMissionKind::RecoverKey
            || campaignMission_.kind == gameplay::CampaignMissionKind::RecoverAndSecure) {
            const bool keyPresent = std::any_of(target->entities.begin(),
                target->entities.end(), [this](const entities::EntityId id) {
                    const auto* item = entities_.findData<entities::ItemData>(id);
                    return item && item->type == entities::ItemType::Key;
                });
            if (!keyPresent) {
                log(LogLevel::Error, "Campaign key mission has no collectible key");
                return false;
            }
        } else if (campaignMission_.kind
                == gameplay::CampaignMissionKind::SecureTerminal
            && !entities_.find(terminalEntity_)) {
            log(LogLevel::Error, "Campaign terminal mission has no usable terminal");
            return false;
        }
        log(LogLevel::Info, "Campaign floor "
            + std::to_string(session_.state().campaignFloor)
            + " layout rooms=" + std::to_string(worldConfig.roomCount)
            + " loops=" + std::to_string(worldConfig.loopBudget)
            + " mission=" + gameplay::campaignMissionName(campaignMission_.kind));
    }

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
    for (world::RoomInstance& room : world_->rooms) {
        const auto node = std::find_if(
            world_->graph.nodes.begin(), world_->graph.nodes.end(),
            [&room](const world::RoomNode& candidate) { return candidate.id == room.id; });
        if (node == world_->graph.nodes.end()) {
            log(LogLevel::Error, "Room event generation did not find its graph node");
            return false;
        }
        world::RoomEventPlan plan = world::generateRoomEventPlan(
            room.seed, room.role, room.theme, node->depth, room.enemyZones,
            runDifficulty_);
        if (session_.state().mode == gameplay::RunMode::TestMode) {
            plan = {};
            plan.seed = room.seed; plan.role = room.role; plan.theme = room.theme;
            plan.planHash = world::roomEventPlanHash(plan);
        }
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
        for (std::size_t fixtureIndex = 0U;
             fixtureIndex < lighting.lights.size(); ++fixtureIndex) {
            const world::RoomPointLight& fixture = lighting.lights[fixtureIndex];
            const entities::EntityId fixtureEntity = entities_.createProp({
                .transform = {
                    .position = fixture.position,
                    .scale = {1.20F, 0.18F, 0.28F},
                },
                .roomId = room.id,
                .type = entities::PropType::CeilingFixture,
                .solid = false,
                .health = 24.0F,
                .maximumHealth = 24.0F,
                .visualSeed = wkc::core::deriveSeed(
                    lighting.seed, gdp::game::SeedDomain::Lighting,
                    100U + fixtureIndex),
            });
            room.entities.push_back(fixtureEntity);
            roomFixtureEntities_[room.id].push_back(fixtureEntity);
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
    const auto assetPreparationStart = Clock::now();
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
    performanceMonitor_.setAssetPreparationMilliseconds(
        std::chrono::duration<double, std::milli>(
            Clock::now() - assetPreparationStart).count());
    if (session_.state().mode == gameplay::RunMode::Campaign
        && !saveCurrentCampaignCheckpoint()) {
        log(LogLevel::Warning,
            "Campaign started without a persistent floor checkpoint");
    }
    gameplay::closeMainMenu(mainMenu_);
    controlsMenuActive_ = false;
    input_ = Input{};

    std::ostringstream worldSummary;
    worldSummary << "Generated world seed " << config_.worldSeed << " with "
                 << world_->rooms.size() << " rooms and " << world_->corridors.size()
                 << " corridors and " << entities_.activeCount()
                 << " entities on attempt " << world_->acceptedAttempt;
    worldSummary << " in " << gameplay::runModeName(session_.state().mode)
                 << " mode at threat " << runDifficulty_ << 'x';
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
        if (config_.enableGamepad) gamepad_.handleEvent(input_, event);
        if (event.type == SDL_EVENT_TEXT_INPUT && menuFlow_.feedbackEditing
            && menuFlow_.page == MenuPage::FeedbackCompose) {
            const std::string_view incoming{event.text.text};
            std::size_t offset = 0U;
            while (offset < incoming.size()
                && uiTextLength(menuFlow_.feedbackDraft) < maximumFeedbackCharacters) {
                const auto start = offset;
                const auto codepoint = nextUiCodepoint(incoming, offset);
                if (codepoint >= 0x20U && codepoint != 0x7fU)
                    menuFlow_.feedbackDraft.append(incoming.substr(start, offset - start));
            }
        }
        if (event.type == SDL_EVENT_QUIT) {
            running_ = false;
        }
    }
    if (config_.enableGamepad) gamepad_.poll(input_);
    if (gamepad_.connected() != gamepadConnected_) {
        gamepadConnected_ = gamepad_.connected();
        if (gamepadConnected_) {
            const char* const name = gamepad_.name();
            log(LogLevel::Info, std::string{"Gamepad connected: "}
                + (name != nullptr ? name : "unknown"));
        } else {
            log(LogLevel::Info, "Gamepad disconnected");
        }
    }

    if (!running_) {
        return;
    }

    using wkc::platform::GamepadButton;
    const auto padPressed = [this](const GamepadButton button) {
        return input_.wasGamepadButtonPressed(button);
    };
    const bool padUp = padPressed(GamepadButton::DpadUp)
        || padPressed(GamepadButton::StickUp);
    const bool padDown = padPressed(GamepadButton::DpadDown)
        || padPressed(GamepadButton::StickDown);
    const bool padLeft = padPressed(GamepadButton::DpadLeft)
        || padPressed(GamepadButton::StickLeft);
    const bool padRight = padPressed(GamepadButton::DpadRight)
        || padPressed(GamepadButton::StickRight);

    if (input_.wasKeyPressed(Key::F3)) {
        performanceOverlayVisible_ = !performanceOverlayVisible_;
        log(LogLevel::Info, performanceOverlayVisible_
                ? "Performance overlay enabled"
                : "Performance overlay disabled");
    }

    const auto playUiAudio = [this](const gdp::audio::AudioEventKind kind) {
        if (audio_) {
            audio_->playUi(
                kind, config_.worldSeed, uiAudioEventCounter_++);
        }
    };

    if (exitConfirmation_.active) {
        playerCommand_ = {};
        interactionPending_ = false;
        jumpPending_ = false;
        firePending_ = false;
        quickMeleePending_ = false;
        fireHeld_ = false;
        reloadPending_ = false;
        weaponSwitchPending_.reset();
        dropPending_ = false;

        if (input_.wasKeyPressed(Key::Escape)
            || padPressed(GamepadButton::East)) {
            gameplay::closeExitConfirmation(exitConfirmation_);
            playUiAudio(gdp::audio::AudioEventKind::UiBack);
            return;
        }

        std::optional<visual::HudExitConfirmationSelection> pointedSelection;
        if (window_ != nullptr) {
            int windowWidth = 0;
            int windowHeight = 0;
            int drawableWidth = 0;
            int drawableHeight = 0;
            if (SDL_GetWindowSize(window_, &windowWidth, &windowHeight)
                && SDL_GetWindowSizeInPixels(
                    window_, &drawableWidth, &drawableHeight)
                && windowWidth > 0 && windowHeight > 0) {
                const Vec2 pointer = input_.mousePosition();
                pointedSelection = visual::exitConfirmationSelectionAt(
                    drawableWidth, drawableHeight,
                    pointer.x * static_cast<float>(drawableWidth)
                        / static_cast<float>(windowWidth),
                    pointer.y * static_cast<float>(drawableHeight)
                        / static_cast<float>(windowHeight));
            }
        }
        const auto confirmationOptionFor
            = [](const visual::HudExitConfirmationSelection selection) {
                  return selection
                          == visual::HudExitConfirmationSelection::ExitGame
                      ? gameplay::ExitConfirmationOption::ExitGame
                      : gameplay::ExitConfirmationOption::Cancel;
              };
        if (pointedSelection && input_.mouseDelta() != Vec2{}) {
            const gameplay::ExitConfirmationOption previousSelection
                = exitConfirmation_.selection;
            gameplay::selectExitConfirmationOption(
                exitConfirmation_, confirmationOptionFor(*pointedSelection));
            if (exitConfirmation_.selection != previousSelection) {
                playUiAudio(gdp::audio::AudioEventKind::UiNavigate);
            }
        }

        const bool movePrevious = input_.wasKeyPressed(Key::Up)
            || input_.wasKeyPressed(Key::Left)
            || input_.wasKeyPressed(Key::W)
            || input_.wasKeyPressed(Key::A) || padUp || padLeft;
        const bool moveNext = input_.wasKeyPressed(Key::Down)
            || input_.wasKeyPressed(Key::Right)
            || input_.wasKeyPressed(Key::S)
            || input_.wasKeyPressed(Key::D) || padDown || padRight;
        if (movePrevious != moveNext) {
            gameplay::moveExitConfirmationSelection(
                exitConfirmation_, movePrevious ? -1 : 1);
            playUiAudio(gdp::audio::AudioEventKind::UiNavigate);
        }

        gameplay::ExitConfirmationCommand command
            = gameplay::ExitConfirmationCommand::None;
        if (input_.wasKeyPressed(Key::Enter)
            || padPressed(GamepadButton::South)) {
            command = gameplay::activateExitConfirmationSelection(
                exitConfirmation_);
        } else if (pointedSelection
            && input_.wasMouseButtonPressed(MouseButton::Primary)) {
            gameplay::selectExitConfirmationOption(
                exitConfirmation_, confirmationOptionFor(*pointedSelection));
            command = gameplay::activateExitConfirmationSelection(
                exitConfirmation_);
        }
        if (command == gameplay::ExitConfirmationCommand::Cancel) {
            playUiAudio(gdp::audio::AudioEventKind::UiBack);
        } else if (command == gameplay::ExitConfirmationCommand::ExitGame) {
            playUiAudio(gdp::audio::AudioEventKind::UiConfirm);
            log(LogLevel::Info, "Game exit confirmed");
            restartRequested_ = false;
            running_ = false;
        }
        return;
    }

    if (controlsMenuActive_) {
        playerCommand_ = {};
        interactionPending_ = false;
        jumpPending_ = false;
        firePending_ = false;
        quickMeleePending_ = false;
        fireHeld_ = false;
        reloadPending_ = false;
        weaponSwitchPending_.reset();
        dropPending_ = false;

        bool backClicked = false;
        if (window_ != nullptr
            && input_.wasMouseButtonPressed(MouseButton::Primary)) {
            int windowWidth = 0;
            int windowHeight = 0;
            int drawableWidth = 0;
            int drawableHeight = 0;
            if (SDL_GetWindowSize(window_, &windowWidth, &windowHeight)
                && SDL_GetWindowSizeInPixels(window_, &drawableWidth, &drawableHeight)
                && windowWidth > 0 && windowHeight > 0) {
                const Vec2 pointer = input_.mousePosition();
                backClicked = visual::controlsMenuBackAt(
                    drawableWidth, drawableHeight,
                    pointer.x * static_cast<float>(drawableWidth)
                        / static_cast<float>(windowWidth),
                    pointer.y * static_cast<float>(drawableHeight)
                        / static_cast<float>(windowHeight));
            }
        }
        if (input_.wasKeyPressed(Key::Escape)
            || input_.wasKeyPressed(Key::Enter)
            || padPressed(GamepadButton::East)
            || padPressed(GamepadButton::South) || backClicked) {
            controlsMenuActive_ = false;
            playUiAudio(gdp::audio::AudioEventKind::UiBack);
        }
        return;
    }

    if (settingsMenu_.active || mainMenu_.active) {
        processFrontendMenu();
        return;
    }

    if (session_.state().phase != gameplay::SessionPhase::Playing) {
        playerCommand_ = {};
        interactionPending_ = false;
        jumpPending_ = false;
        firePending_ = false;
        quickMeleePending_ = false;
        fireHeld_ = false;
        reloadPending_ = false;
        weaponSwitchPending_.reset();
        dropPending_ = false;
        if (input_.wasKeyPressed(Key::Escape)
            || padPressed(GamepadButton::East)) {
            gameplay::openExitConfirmation(exitConfirmation_);
            playUiAudio(gdp::audio::AudioEventKind::UiConfirm);
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
                  if (selection == visual::HudResultMenuSelection::ReplaySeed) {
                      return gameplay::ResultMenuOption::ReplaySeed;
                  }
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
            || input_.wasKeyPressed(Key::W) || padUp;
        const bool moveDown = input_.wasKeyPressed(Key::Down)
            || input_.wasKeyPressed(Key::S) || padDown;
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
        if (input_.wasKeyPressed(Key::Enter)
            || padPressed(GamepadButton::South)) {
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
            const bool nextCampaignFloor
                = gameplay::canAdvanceCampaign(session_.state());
            if (nextCampaignFloor) {
                gameplay::CampaignCarryover carryover
                    = gameplay::prepareNextCampaignFloorCarryover(
                        gameplay::captureCampaignCarryover(
                            player_, weapon_, progression_));
                if (!gameplay::validateCampaignCarryover(carryover)) {
                    log(LogLevel::Error,
                        "Campaign result produced invalid carry-over state");
                    fatalError_ = true;
                    running_ = false;
                    return;
                }
                campaignCarryover_ = std::move(carryover);
            } else {
                campaignCarryover_.reset();
            }
            log(LogLevel::Info, nextCampaignFloor
                    ? "Result menu requested the next Campaign floor"
                    : "Result menu requested the next seed");
            restartMode_ = nextCampaignFloor
                ? gameplay::RunRestartMode::NextCampaignFloor
                : gameplay::RunRestartMode::NextSeed;
            restartRequested_ = true;
            running_ = false;
        } else if (command == gameplay::ResultMenuCommand::ReplaySeed) {
            if (!stageCampaignFloorReplay()) {
                fatalError_ = true;
                running_ = false;
                return;
            }
            log(LogLevel::Info, "Result menu requested the same seed replay");
            restartMode_ = gameplay::RunRestartMode::SameSeed;
            restartRequested_ = true;
            running_ = false;
        } else if (command == gameplay::ResultMenuCommand::MainMenu) {
            log(LogLevel::Info, "Result menu requested main menu");
            if (!returnToMainMenu()) {
                fatalError_ = true;
                running_ = false;
            }
        } else if (command == gameplay::ResultMenuCommand::ExitGame) {
            gameplay::openExitConfirmation(exitConfirmation_);
            log(LogLevel::Info, "Result menu opened exit confirmation");
        }
        return;
    }

    if (input_.wasKeyPressed(Key::Escape)
        || padPressed(GamepadButton::Start)) {
        playUiAudio(pauseMenu_.paused
                ? gdp::audio::AudioEventKind::UiBack
                : gdp::audio::AudioEventKind::UiConfirm);
        setPaused(!pauseMenu_.paused);
        return;
    }

    if (pauseMenu_.paused) {
        playerCommand_ = {};
        interactionPending_ = false;
        jumpPending_ = false;
        firePending_ = false;
        quickMeleePending_ = false;
        fireHeld_ = false;
        reloadPending_ = false;
        weaponSwitchPending_.reset();
        dropPending_ = false;

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
            case visual::HudPauseSelection::Controls:
                return gameplay::PauseMenuOption::Controls;
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
            || input_.wasKeyPressed(Key::W) || padUp;
        const bool moveDown = input_.wasKeyPressed(Key::Down)
            || input_.wasKeyPressed(Key::S) || padDown;
        if (moveUp != moveDown) {
            const gameplay::PauseMenuOption previousSelection
                = pauseMenu_.selection;
            gameplay::movePauseSelection(pauseMenu_, moveUp ? -1 : 1);
            if (pauseMenu_.selection != previousSelection) {
                playUiAudio(gdp::audio::AudioEventKind::UiNavigate);
            }
        }

        gameplay::PauseMenuCommand command = gameplay::PauseMenuCommand::None;
        if (input_.wasKeyPressed(Key::Enter)
            || padPressed(GamepadButton::South)) {
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
            if (!stageCampaignFloorReplay()) {
                fatalError_ = true;
                running_ = false;
                return;
            }
            log(LogLevel::Info, "Pause menu requested the same seed restart");
            (void)session_.update(
                false, world::RoomRole::Count, false, false);
            restartMode_ = gameplay::RunRestartMode::SameSeed;
            restartRequested_ = true;
            running_ = false;
        } else if (command == gameplay::PauseMenuCommand::Settings) {
            menuFlow_ = {}; menuFlow_.page = MenuPage::Settings;
            gameplay::openSettingsMenu(
                settingsMenu_, gameplay::SettingsMenuOrigin::PauseMenu);
            log(LogLevel::Info, "Pause menu opened settings");
        } else if (command == gameplay::PauseMenuCommand::Controls) {
            menuFlow_ = {}; menuFlow_.page = MenuPage::Controls;
            gameplay::openSettingsMenu(settingsMenu_, gameplay::SettingsMenuOrigin::PauseMenu);
            log(LogLevel::Info, "Pause menu opened controls");
        } else if (command == gameplay::PauseMenuCommand::MainMenu) {
            log(LogLevel::Info, "Pause menu requested main menu");
            if (!returnToMainMenu()) {
                fatalError_ = true;
                running_ = false;
            }
        } else if (command == gameplay::PauseMenuCommand::ExitGame) {
            gameplay::openExitConfirmation(exitConfirmation_);
            log(LogLevel::Info, "Pause menu opened exit confirmation");
        }
        return;
    }

    using gameplay::ControlAction;
    const auto down = [&](ControlAction action) { return gameplay::controlDown(runtimeSettings_.controls, runtimeSettings_.gamepadControls, action, input_); };
    const auto pressed = [&](ControlAction action) { return gameplay::controlPressed(runtimeSettings_.controls, runtimeSettings_.gamepadControls, action, input_); };
    const Vec2 rawMove = runtimeSettings_.gamepadSouthpaw
        ? input_.gamepadRightStick() : input_.gamepadLeftStick();
    const Vec2 gamepadMove = wkc::platform::gamepadDeadzone(rawMove,
        static_cast<float>(runtimeSettings_.gamepadMoveDeadzonePercent) / 100.0F);
    playerCommand_.moveForward = std::clamp(
        (down(ControlAction::Forward) ? 1.0F : 0.0F)
            - (down(ControlAction::Backward) ? 1.0F : 0.0F)
            - gamepadMove.y, -1.0F, 1.0F);
    playerCommand_.moveRight = std::clamp(
        (down(ControlAction::Right) ? 1.0F : 0.0F)
            - (down(ControlAction::Left) ? 1.0F : 0.0F)
            + gamepadMove.x, -1.0F, 1.0F);
    playerCommand_.crouch = down(ControlAction::Crouch);
    playerCommand_.quietWalk = down(ControlAction::QuietWalk);
    jumpPending_ = jumpPending_ || pressed(ControlAction::Jump);
    playerCommand_.jump = jumpPending_;
    interactionPending_ = interactionPending_ || pressed(ControlAction::Interact);
    firePending_ = firePending_ || pressed(ControlAction::Fire);
    fireHeld_ = down(ControlAction::Fire);
    quickMeleePending_ = quickMeleePending_ || ((grenadeCook_.active()
        || grenadeThrowRemaining_ > 0.0F)
        ? false : pressed(ControlAction::QuickMelee));
    reloadPending_ = reloadPending_ || (!grenadeSelected_ && pressed(ControlAction::Reload));
    dropPending_ = dropPending_ || (!grenadeSelected_ && pressed(ControlAction::Drop));
    if (pressed(ControlAction::Flashlight)) {
        flashlightOn_ = !flashlightOn_;
        log(LogLevel::Info, flashlightOn_ ? "Flashlight enabled" : "Flashlight disabled");
    }
    const bool grenadeAnimationLocked = grenadeCook_.active()
        || grenadeThrowRemaining_ > 0.0F;
    if (!grenadeAnimationLocked && pressed(ControlAction::Equipment)) {
        grenadeSelected_ = true;
        weapon_.cancelQueuedShotgunFire();
        weaponSwitchPending_.reset();
        firePending_ = false;
        reloadPending_ = false;
        dropPending_ = false;
    } else if (!grenadeAnimationLocked && pressed(ControlAction::Primary)) {
        grenadeSelected_ = false;
        weaponSwitchPending_ = weapon_.archetypeForSlot(1U);
    } else if (!grenadeAnimationLocked && pressed(ControlAction::Secondary)) {
        grenadeSelected_ = false;
        weaponSwitchPending_ = gameplay::weaponArchetypeForSlot(2U);
    } else if (!grenadeAnimationLocked && pressed(ControlAction::Melee)) {
        grenadeSelected_ = false;
        weaponSwitchPending_ = gameplay::weaponArchetypeForSlot(3U);
    } else if (!grenadeAnimationLocked && pressed(ControlAction::LastWeapon)) {
        if (!grenadeSelected_) weaponSwitchPending_ = weapon_.lastWeaponArchetype();
        grenadeSelected_ = false;
    } else if (!grenadeAnimationLocked && pressed(ControlAction::NextWeapon)) {
        grenadeSelected_ = false;
        weaponSwitchPending_ = weapon_.nextWeaponArchetype();
    } else if (!grenadeAnimationLocked && pressed(ControlAction::PreviousWeapon)) {
        grenadeSelected_ = false;
        weaponSwitchPending_ = weapon_.previousWeaponArchetype();
    }
    playerCommand_.interact = interactionPending_;
}

void Application::setPaused(const bool paused)
{
    if (paused && weapon_.definition().archetype == gameplay::WeaponArchetype::Sniper)
        aimBlend_ = 0.0F;
    if (paused) {
        gameplay::openPauseMenu(pauseMenu_);
    } else {
        gameplay::closePauseMenu(pauseMenu_);
    }
    playerCommand_ = {};
    interactionPending_ = false;
    jumpPending_ = false;
    firePending_ = false;
    quickMeleePending_ = false;
    fireHeld_ = false;
    reloadPending_ = false;
    weaponSwitchPending_.reset();
    dropPending_ = false;
    if (!config_.headless && window_ != nullptr
        && !SDL_SetWindowRelativeMouseMode(window_, !paused)) {
        log(LogLevel::Warning, sdlError("Pause mouse mode update failed"));
    }
    log(LogLevel::Info, paused ? "Game paused" : "Game resumed");
}

bool Application::stageCampaignFloorReplay()
{
    campaignCarryover_.reset();
    if (session_.state().mode != gameplay::RunMode::Campaign) {
        return true;
    }
    if (!campaignFloorEntry_
        || !gameplay::validateCampaignCarryover(*campaignFloorEntry_)) {
        log(LogLevel::Error,
            "Campaign floor replay is missing a valid entry snapshot");
        return false;
    }
    campaignCarryover_ = *campaignFloorEntry_;
    return true;
}

bool Application::returnToMainMenu()
{
    campaignCarryover_.reset();
    campaignFloorEntry_.reset();
    campaignMission_ = {};
    runDifficulty_ = 1.0F;
    if (session_.state().phase == gameplay::SessionPhase::Playing) {
        (void)session_.update(
            false, world::RoomRole::Count, false, false);
    }
    const std::optional<wkc::core::Seed> nextSeed
        = session_.restart(gameplay::RunRestartMode::NextSeed);
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
    roomFixtureEntities_.clear();
    roomMaterialKeys_.clear();
    doorRuntimes_.clear();
    doorAudioEventCounters_.clear();
    activeImpacts_.clear();
    activeGrenadeBlasts_.clear();
    playerGrenades_.clear();
    grenadeSelected_ = false;
    grenadeCook_.reset();
    grenadeThrowRemaining_ = 0.0F;
    grenadeThrowChargeStrength_ = 0.0F;
    enemyProjectiles_.clear();
    input_ = Input{};
    player_ = gameplay::Player{};
    weapon_ = gameplay::Weapon{};
    progression_ = gameplay::ProgressionSystem{};
    gameplay::clearFeedback(feedback_);
    gameplay::resetFootsteps(footsteps_);
    gameplay::resetViewMotion(viewMotion_);
    playerBodyMotion_ = {};
    gameplay::openMainMenu(mainMenu_);
    gameplay::closePauseMenu(pauseMenu_);
    menuFlow_ = {};
    gameplay::closeResultMenu(resultMenu_);
    gameplay::closeSettingsMenu(settingsMenu_);
    controlsMenuActive_ = false;
    baseCollisionWorld_ = gameplay::CollisionWorld::fixedRoom();
    collisionWorld_ = baseCollisionWorld_;
    playerCommand_ = {};
    interactionPending_ = false;
    jumpPending_ = false;
    firePending_ = false;
    quickMeleePending_ = false;
    fireHeld_ = false;
    reloadPending_ = false;
    weaponSwitchPending_.reset();
    dropPending_ = false;
    weaponRecoilRemaining_ = 0.0F;
    aimBlend_ = 0.0F;
    flashlightOn_ = false;
    weaponMeleeRemaining_ = 0.0F;
    weaponMuzzleFlashRemaining_ = 0.0F;
    weaponEquipRemaining_ = 0.0F;
    restartRequested_ = false;
    audioEventCounter_ = 0U;
    uiAudioEventCounter_ = 0U;
    impactVisualCounter_ = 0U;
    terminalEntity_ = {};
    exitTerminalEntity_ = {};
    bossRoomId_ = 0U;
    world_.reset();
    visitedRooms_.clear();
    roomEntryGraceRemaining_.clear();
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
    if (window_ != nullptr) {
        const bool fullscreen
            = (SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN) != 0U;
        if (fullscreen != runtimeSettings_.fullscreen
            && !SDL_SetWindowFullscreen(window_, runtimeSettings_.fullscreen)) {
            runtimeSettings_.fullscreen = fullscreen;
            settingsDirty_ = true;
            log(LogLevel::Warning,
                sdlError("Fullscreen mode update failed; restored actual state"));
        }
        if (!runtimeSettings_.fullscreen) {
            const gameplay::WindowResolutionSize desired
                = gameplay::windowResolutionSize(runtimeSettings_.windowResolution);
            int actualWidth = 0;
            int actualHeight = 0;
            if (!SDL_GetWindowSize(window_, &actualWidth, &actualHeight)) {
                log(LogLevel::Warning, sdlError("Window size query failed"));
            } else if (actualWidth != static_cast<int>(desired.width)
                || actualHeight != static_cast<int>(desired.height)) {
                if (!SDL_SetWindowSize(window_, static_cast<int>(desired.width),
                        static_cast<int>(desired.height))) {
                    if (actualWidth > 0 && actualHeight > 0
                        && actualWidth <= 65'535 && actualHeight <= 65'535) {
                        const std::optional<gameplay::WindowResolution> actual
                            = gameplay::windowResolutionForSize(
                                static_cast<std::uint16_t>(actualWidth),
                                static_cast<std::uint16_t>(actualHeight));
                        if (actual) {
                            runtimeSettings_.windowResolution = *actual;
                            settingsDirty_ = true;
                        }
                    }
                    log(LogLevel::Warning,
                        sdlError("Window size update failed"));
                } else if (!SDL_SetWindowPosition(window_,
                        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED)) {
                    log(LogLevel::Warning,
                        sdlError("Window centering after resize failed"));
                }
            }
        }
    }
    if (glContext_ != nullptr) {
        const int desiredSwapInterval = runtimeSettings_.verticalSync ? 1 : 0;
        int actualSwapInterval = 0;
        const bool queried = SDL_GL_GetSwapInterval(&actualSwapInterval);
        if ((!queried || actualSwapInterval != desiredSwapInterval)
            && !SDL_GL_SetSwapInterval(desiredSwapInterval)) {
            log(LogLevel::Warning,
                sdlError(runtimeSettings_.verticalSync
                        ? "VSync enable failed"
                        : "VSync disable failed"));
        }
    }
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
    if (config_.benchmarkQuality) {
        runtimeSettings_ = {};
        runtimeSettings_.graphicsQuality = *config_.benchmarkQuality;
        runtimeSettings_.windowResolution
            = gameplay::WindowResolution::R1280x720;
        runtimeSettings_.fullscreen = false;
        runtimeSettings_.verticalSync = false;
        log(LogLevel::Info,
            "Benchmark settings isolated from persisted player preferences");
        return;
    }
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
            "Runtime settings migrated in memory from an older schema");
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

void Application::loadPersistedSpeedrunRecord()
{
    if (speedrunRecordLoaded_) {
        return;
    }
    speedrunRecordLoaded_ = true;
    if (config_.headless || config_.benchmarkQuality) {
        return;
    }

    char* const preferenceDirectory = SDL_GetPrefPath("WKC", "GDP");
    if (preferenceDirectory == nullptr) {
        log(LogLevel::Warning,
            sdlError("Speedrun record preference path unavailable"));
        return;
    }
    speedrunRecordPath_
        = std::filesystem::path{preferenceDirectory} / "speedrun_record.toml";
    speedrunLeaderboardPath_ = std::filesystem::path{preferenceDirectory} / "speedrun_leaderboard.txt";
    SDL_free(preferenceDirectory);

    const persistence::SpeedrunRecordLoadResult loaded
        = persistence::loadSpeedrunRecord(*speedrunRecordPath_);
    speedrunRecord_ = loaded.record;
    speedrunLeaderboard_ = persistence::loadSpeedrunLeaderboard(*speedrunLeaderboardPath_);
    if (speedrunLeaderboard_.empty() && speedrunRecord_) speedrunLeaderboard_.push_back(*speedrunRecord_);
    if (!speedrunLeaderboard_.empty()) speedrunRecord_ = speedrunLeaderboard_.front();
    switch (loaded.status) {
    case persistence::SpeedrunRecordLoadStatus::Loaded:
        log(LogLevel::Info, "Local Speedrun record loaded from disk");
        break;
    case persistence::SpeedrunRecordLoadStatus::Missing:
        log(LogLevel::Info, "No local Speedrun record found");
        break;
    case persistence::SpeedrunRecordLoadStatus::Invalid:
        log(LogLevel::Warning, "Invalid Speedrun record file ignored");
        break;
    case persistence::SpeedrunRecordLoadStatus::UnsupportedVersion:
        log(LogLevel::Warning, "Unsupported Speedrun record file ignored");
        break;
    case persistence::SpeedrunRecordLoadStatus::IncompatibleRules:
        log(LogLevel::Info,
            "Speedrun record belongs to an older rules version and was ignored");
        break;
    case persistence::SpeedrunRecordLoadStatus::IoError:
        log(LogLevel::Warning, "Speedrun record could not be read");
        break;
    }
}

bool Application::savePersistedSpeedrunRecord()
{
    if (!speedrunRecord_ || !speedrunRecordPath_) {
        log(LogLevel::Warning,
            "Speedrun record has no writable preference path");
        return false;
    }
    if (!persistence::saveSpeedrunRecord(
            *speedrunRecordPath_, *speedrunRecord_)) {
        log(LogLevel::Warning,
            "Speedrun record could not be saved; previous file was preserved");
        return false;
    }
    log(LogLevel::Info, "Local Speedrun record saved to disk");
    return true;
}

void Application::loadPersistedCampaignCheckpoint()
{
    if (campaignCheckpointLoaded_) return;
    campaignCheckpointLoaded_ = true;
    if (config_.headless || config_.benchmarkQuality) return;
    char* const pref = SDL_GetPrefPath("WKC", "GDP");
    if (!pref) { log(LogLevel::Warning, "Campaign save directory unavailable"); return; }
    const std::filesystem::path directory{pref};
    SDL_free(pref);
    for (std::size_t i = 0; i < campaignSlots_.size(); ++i) {
        campaignSlotPaths_[i] = directory / ("campaign_slot_" + std::to_string(i + 1U) + ".toml");
        const auto loaded = persistence::loadCampaignCheckpoint(*campaignSlotPaths_[i]);
        if (loaded.migratedRules)
            log(LogLevel::Info, "Campaign slot " + std::to_string(i + 1U)
                + " migrated to current rules; floor-entry resources preserved");
        campaignSlots_[i] = {loaded.checkpoint, loaded.status != persistence::CampaignCheckpointLoadStatus::Missing};
        if (loaded.status != persistence::CampaignCheckpointLoadStatus::Loaded
            && loaded.status != persistence::CampaignCheckpointLoadStatus::Missing)
            log(LogLevel::Warning, "Campaign slot " + std::to_string(i + 1U) + " unavailable; existing file preserved");
    }
    // Import the single legacy save only into an empty slot 1; retain a recoverable backup.
    const auto legacy = directory / "campaign_checkpoint.toml";
    const auto backup = directory / "campaign_checkpoint.legacy.toml";
    std::error_code error;
    if (!campaignSlots_[0].occupied && !std::filesystem::exists(backup, error) && !error) {
        const auto loaded = persistence::loadCampaignCheckpoint(legacy);
        if (loaded.checkpoint && persistence::saveCampaignCheckpoint(*campaignSlotPaths_[0], *loaded.checkpoint)) {
            campaignSlots_[0] = {loaded.checkpoint, true};
            std::filesystem::rename(legacy, backup, error);
            log(LogLevel::Info, error ? "Legacy save imported; original retained" : "Legacy save imported into slot 1; backup retained");
        }
    }
    campaignCheckpointPath_ = campaignSlotPaths_[0];
    campaignCheckpoint_ = campaignSlots_[0].checkpoint;
}

bool Application::saveCurrentCampaignCheckpoint()
{
    if (session_.state().mode != gameplay::RunMode::Campaign
        || session_.state().phase != gameplay::SessionPhase::Playing
        || !campaignFloorEntry_) {
        return false;
    }
    const gameplay::SessionState& state = session_.state();
    const gameplay::CampaignCheckpoint checkpoint{
        gameplay::currentCampaignRulesVersion,
        state.initialSeed,
        state.currentSeed,
        state.runIndex,
        state.seedIndex,
        state.campaignFloor,
        *campaignFloorEntry_,
        state.difficulty,
    };
    if (!gameplay::validateCampaignCheckpoint(checkpoint)) {
        log(LogLevel::Warning,
            "Campaign floor checkpoint failed validation and was not saved");
        return false;
    }
    if (!campaignCheckpointPath_) {
        log(LogLevel::Warning,
            "Campaign checkpoint has no writable preference path");
        return false;
    }
    if (!persistence::saveCampaignCheckpoint(
            *campaignCheckpointPath_, checkpoint)) {
        log(LogLevel::Warning,
            "Campaign checkpoint could not be saved; previous file was preserved");
        return false;
    }
    campaignCheckpoint_ = checkpoint;
    campaignSlots_[activeCampaignSlot_] = {checkpoint, true};
    std::ostringstream message;
    message << "Campaign floor " << state.campaignFloor
            << " checkpoint saved to disk";
    log(LogLevel::Info, message.str());
    return true;
}

bool Application::clearPersistedCampaignCheckpoint()
{
    if (!campaignCheckpointPath_) {
        campaignCheckpoint_.reset();
        return config_.headless;
    }
    if (!persistence::removeCampaignCheckpoint(*campaignCheckpointPath_)) {
        log(LogLevel::Warning,
            "Completed Campaign checkpoint could not be removed");
        return false;
    }
    campaignCheckpoint_.reset();
    log(LogLevel::Info, "Completed Campaign checkpoint removed from disk");
    campaignSlots_[activeCampaignSlot_] = {};
    return true;
}

bool Application::isCampaignExitTargeted() const
{
    if (session_.state().mode != gameplay::RunMode::Campaign
        || session_.state().phase != gameplay::SessionPhase::Playing) return false;
    const entities::Entity* terminal = entities_.find(exitTerminalEntity_);
    const entities::Entity* player = entities_.find(player_.entity());
    const entities::PropData* prop = entities_.findData<entities::PropData>(exitTerminalEntity_);
    if (!terminal || !player || !prop || terminal->roomId != player->roomId
        || prop->type != entities::PropType::Terminal) return false;
    const std::array targets{gameplay::InteractionTarget{
        exitTerminalEntity_, gameplay::propCollisionBounds(*terminal, *prop)}};
    return player_.findInteraction(targets, 2.25F, collisionWorld_.solids())
        == exitTerminalEntity_;
}

void Application::fixedUpdate(const double fixedStep)
{
    const float step = static_cast<float>(fixedStep);
    const bool gameplayActive
        = !mainMenu_.active && !pauseMenu_.paused && !settingsMenu_.active
        && !controlsMenuActive_;
    if (world_) {
        session_.advanceRunTimer(gameplayActive);
    }
    if (!gameplayActive) {
        if (mainMenu_.active && std::isfinite(fixedStep) && fixedStep > 0.0) {
            menuBackdropSeconds_ = std::fmod(menuBackdropSeconds_ + fixedStep, 240.0);
        }
        if (audio_) {
            audio_->updateUi(step);
        }
        return;
    }
    gameplay::updateFeedback(feedback_, step);
    const bool aimRequested = player_.state().alive
        && !grenadeSelected_
        && weapon_.definition().archetype != gameplay::WeaponArchetype::Knife
        && gameplay::controlDown(runtimeSettings_.controls, runtimeSettings_.gamepadControls, gameplay::ControlAction::Aim, input_);
    const bool scopeBlocked = weapon_.definition().archetype == gameplay::WeaponArchetype::Sniper
        && (weapon_.reloading() || weapon_.switching() || weaponEquipRemaining_ > 0.0F);
    aimBlend_ = gameplay::advanceWeaponAimBlend(aimBlend_, step,
        weapon_.definition().archetype, aimRequested,
        scopeBlocked || grenadeSelected_ || !player_.state().alive
            || session_.state().phase != gameplay::SessionPhase::Playing);
    weaponRecoilRemaining_
        = std::max(0.0F, weaponRecoilRemaining_ - step);
    weaponMeleeRemaining_
        = std::max(0.0F, weaponMeleeRemaining_ - step);
    weaponMuzzleFlashRemaining_
        = std::max(0.0F, weaponMuzzleFlashRemaining_ - step);
    weaponEquipRemaining_
        = std::max(0.0F, weaponEquipRemaining_ - step);
    if (grenadeThrowRemaining_ > 0.0F) {
        grenadeThrowRemaining_ = std::max(0.0F, grenadeThrowRemaining_ - step);
        if (grenadeThrowRemaining_ == 0.0F) {
            grenadeThrowChargeStrength_ = 0.0F;
            if (player_.state().grenades == 0U && !grenadeCook_.active())
                grenadeSelected_ = false;
        }
    }
    for (ActiveImpact& impact : activeImpacts_) {
        impact.remaining = std::max(0.0F, impact.remaining - step);
        impact.render.strength = impact.remaining / impactVisualDuration;
    }
    std::erase_if(activeImpacts_, [](const ActiveImpact& impact) {
        return impact.remaining <= 0.0F;
    });
    for (ActiveGrenadeBlast& blast : activeGrenadeBlasts_)
        blast.remaining = std::max(0.0F, blast.remaining - step);
    std::erase_if(activeGrenadeBlasts_, [](const ActiveGrenadeBlast& blast) {
        return blast.remaining <= 0.0F;
    });
    if (session_.state().phase != gameplay::SessionPhase::Playing) {
        updateEnemyAnimationTransitions(true);
        if (audio_) {
            audio_->update(step);
        }
        ++fixedUpdates_;
        return;
    }
    if (!player_.state().alive) {
        weapon_.cancelQueuedShotgunFire();
    } else if (firePending_ && !grenadeSelected_ && !quickMeleePending_) {
        (void)weapon_.queueShotgunFireDuringReload();
    }
    weapon_.fixedUpdate(step);
    for (auto it = roomEntryGraceRemaining_.begin();
         it != roomEntryGraceRemaining_.end();) {
        it->second = std::max(0.0F, it->second - step);
        if (it->second <= 0.0F) {
            it = roomEntryGraceRemaining_.erase(it);
        } else {
            ++it;
        }
    }
    constexpr float roomEntryGraceSeconds = 1.25F;
    const auto grantRoomEntryGrace = [this, roomEntryGraceSeconds](
        const world::RoomId roomId) {
        if (roomId != 0U) {
            float& remaining = roomEntryGraceRemaining_[roomId];
            remaining = std::max(remaining, roomEntryGraceSeconds);
        }
    };
    if (weaponSwitchPending_ && player_.state().alive
        && weapon_.switchTo(*weaponSwitchPending_)) {
        aimBlend_ = 0.0F;
        reloadPending_ = false;
        firePending_ = false;
        quickMeleePending_ = false;
        fireHeld_ = false;
        log(LogLevel::Info,
            std::string{"Weapon selected: "}
                + gameplay::weaponArchetypeName(
                    weapon_.definition().archetype));
    }
    weaponSwitchPending_.reset();
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
        && gameplay::shouldSealRoomDoors(*collisionEvent, entities_);
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
            *world_, player_.state().position, player_.collider())) {
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
        const std::vector<entities::EntityId> enemyIds
            = entities_.query(entities::EntityKind::Enemy);
        for (auto& [edge, door] : doorRuntimes_) {
            const bool locked = doorsLocked
                && edgeTouchesRoom(*world_, edge, playerRoom);
            const auto corridor = std::find_if(world_->corridors.begin(),
                world_->corridors.end(), [edge](const world::CorridorPath& path) {
                    return path.edge == edge;
                });
            const bool occupied = occupiedCorridorEdge == edge
                || (corridor != world_->corridors.end()
                    && gameplay::livingEnemyOccupiesCorridor(
                        *world_, *corridor, entities_))
                || positionNearDoor(*world_, edge, player_.state().position)
                || std::any_of(
                    enemyIds.begin(), enemyIds.end(),
                    [this, edge](const entities::EntityId id) {
                        const entities::Entity* entity = entities_.find(id);
                        const entities::EnemyData* enemy
                            = entities_.findData<entities::EnemyData>(id);
                        return entity != nullptr && enemy != nullptr
                            && enemy->active
                            && enemy->behavior
                                != entities::EnemyBehaviorState::Dead
                            && positionNearDoor(
                                *world_, edge, entity->transform.position);
                    });
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
    jumpPending_ = false;
    playerCommand_.jump = false;
    const std::optional<std::uint64_t> footstepEvent = gameplay::updateFootsteps(
        footsteps_, previousPlayerPosition, player_.state().position,
        player_.state().grounded, player_.state().alive);
    if (footstepEvent) {
        const bool quietStep = playerCommand_.quietWalk
            || player_.state().crouching;
        (void)gameplay::alertEnemiesToNoise(
            entities_, playerRoom,
            {.position = player_.state().position,
                .radius = quietStep ? 0.75F : 4.0F});
        if (audio_) {
            audio_->playAt(quietStep
                    ? gdp::audio::AudioEventKind::QuietFootstep
                    : gdp::audio::AudioEventKind::Footstep,
                session_.state().currentSeed, *footstepEvent,
                player_.state().position, player_.state().position,
                player_.planarForwardDirection());
        }
    }
    gameplay::updateViewMotion(
        viewMotion_, previousPlayerPosition, player_.state().position,
        player_.state().grounded, player_.state().alive, step);
    visual::updatePlayerBodyMotion(playerBodyMotion_, previousPlayerPosition,
        player_.state().position, player_.state().yawDegrees,
        player_.state().grounded, player_.state().alive, step);

    const auto adoptRoom = [this, &playerEntity, &playerRoom,
                            &grantRoomEntryGrace](
        const world::RoomInstance& destination,
        const std::string_view transitionKind) {
        const std::uint32_t sourceRoom = playerRoom;
        playerRoom = destination.id;
        const bool firstVisit = std::find(
            visitedRooms_.begin(), visitedRooms_.end(), playerRoom)
            == visitedRooms_.end();
        if (firstVisit) {
            visitedRooms_.push_back(playerRoom);
            grantRoomEntryGrace(playerRoom);
        }
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
            *world_, player_.state().position, player_.collider());
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
                && gameplay::shouldSealRoomDoors(*currentEvent, entities_);
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
                        if (std::find(visitedRooms_.begin(), visitedRooms_.end(),
                                transition->destinationRoom) == visitedRooms_.end()) {
                            grantRoomEntryGrace(transition->destinationRoom);
                        }
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

    if (dropPending_) {
        if (playerEntity != nullptr && world_) {
            const auto room = std::find_if(world_->rooms.begin(), world_->rooms.end(),
                [playerRoom](const world::RoomInstance& candidate) {
                    return candidate.id == playerRoom;
                });
            if (room != world_->rooms.end()) {
                const auto dropped = gameplay::dropSelectedWeapon(
                    entities_, player_, weapon_, collisionWorld_, playerRoom);
                if (dropped) {
                    aimBlend_ = 0.0F;
                    room->entities.push_back(*dropped);
                    if (entities::ItemData* item
                        = entities_.findData<entities::ItemData>(*dropped)) {
                        item->droppedVisualStartTick = fixedUpdates_;
                        const wkc::core::Vec3 forward
                            = player_.planarForwardDirection();
                        item->droppedVisualYawDegrees = std::atan2(
                            forward.x, forward.z) * (180.0F / std::numbers::pi_v<float>);
                    }
                    gameplay::signalNotice(
                        feedback_, gameplay::NoticeFeedbackKind::WeaponDropped);
                    const entities::ItemData* item
                        = entities_.findData<entities::ItemData>(*dropped);
                    log(LogLevel::Info, item && item->recoveredWeapon
                            ? "Weapon dropped: " + std::string{
                                gameplay::weaponArchetypeName(
                                    static_cast<gameplay::WeaponArchetype>(
                                        item->recoveredWeapon->archetype))}
                            : "Weapon dropped");
                    reloadPending_ = false;
                    firePending_ = false;
                    quickMeleePending_ = false;
                    fireHeld_ = false;
                } else {
                    gameplay::signalNotice(
                        feedback_, gameplay::NoticeFeedbackKind::WeaponDropBlocked);
                }
            }
        }
        dropPending_ = false;
    }

    const gameplay::LootCollectionSummary loot = gameplay::collectNearbyLoot(
        entities_, player_, weapon_, playerRoom);
    if (loot.collected > 0U) {
        if (gameplay::advanceCampaignMission(campaignMission_, playerRoom,
                {.keysCollected = loot.keysAdded})) {
            log(LogLevel::Info, "Campaign mission completed: "
                + std::string{gameplay::campaignMissionName(campaignMission_.kind)});
        }
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
        if (loot.weaponsEquipped > 0U) {
            aimBlend_ = 0.0F;
            message << " | equipped "
                    << gameplay::weaponArchetypeName(
                        weapon_.definition().archetype);
        }
        if (loot.specialsActivated > 0U) {
            message << " | OVERDRIVE activated";
        }
        log(LogLevel::Info, message.str());
        if (audio_) {
            audio_->play(gdp::audio::AudioEventKind::Pickup,
                config_.worldSeed, audioEventCounter_++);
        }
        gameplay::signalNotice(feedback_, gameplay::NoticeFeedbackKind::Pickup);
        awardProgress(gameplay::ProgressionEvent::LootCollected,
            static_cast<std::uint32_t>(loot.rewardableCollected));
    }
    if (reloadPending_ && player_.state().alive) {
        if (weapon_.reload()) {
            if (weapon_.definition().archetype == gameplay::WeaponArchetype::Sniper)
                aimBlend_ = 0.0F;
            std::ostringstream message;
            message << "Weapon reload started: " << weapon_.magazineAmmo() << '/'
                    << weapon_.reserveAmmo();
            log(LogLevel::Info, message.str());
            if (audio_) {
                audio_->play(gdp::audio::AudioEventKind::Reload,
                    config_.worldSeed, audioEventCounter_++);
            }
        }
    }
    reloadPending_ = false;
    gameplay::updateEnemyHitReactions(entities_, step);
    std::vector<std::uint32_t> reachableShotRooms;
    if (world_) {
        reachableShotRooms.reserve(4U);
        for (const world::CorridorPath& corridor : world_->corridors) {
            const world::RoomId neighbor = corridor.firstRoom == playerRoom
                ? corridor.secondRoom
                : corridor.secondRoom == playerRoom ? corridor.firstRoom : 0U;
            if (neighbor == 0U) continue;
            const auto door = doorRuntimes_.find(corridor.edge);
            if (door != doorRuntimes_.end()
                && gameplay::isDoorPassable(door->second)) {
                reachableShotRooms.push_back(neighbor);
            }
        }
    }
    bool grenadeThrowPending = false;
    bool grenadeHandBlastPending = false;
    float grenadeThrowChargeSeconds = 0.0F;
    float grenadeThrowFuseSeconds = gameplay::grenadeFuseSeconds;
    if (grenadeSelected_ && player_.state().alive) {
        if (firePending_ && !grenadeCook_.active()
            && grenadeThrowRemaining_ <= 0.0F) {
            if (player_.state().grenades > 0U) {
                if (grenadeCook_.begin()) (void)player_.useGrenade();
            } else if (audio_) {
                audio_->playAt(gdp::audio::AudioEventKind::DryFire,
                    config_.worldSeed, audioEventCounter_++,
                    player_.cameraPosition(), player_.cameraPosition(),
                    player_.forwardDirection());
            }
        }
        if (grenadeCook_.active()) {
            const gameplay::GrenadeCookOutcome outcome
                = grenadeCook_.update(step, fireHeld_);
            if (outcome != gameplay::GrenadeCookOutcome::None) {
                grenadeThrowChargeSeconds = grenadeCook_.chargeSeconds();
                grenadeThrowFuseSeconds = grenadeCook_.remainingFuseSeconds();
                grenadeThrowChargeStrength_ = grenadeCook_.windupProgress();
                grenadeThrowRemaining_ = 0.28F;
                grenadeThrowPending = outcome == gameplay::GrenadeCookOutcome::Throw;
                grenadeHandBlastPending
                    = outcome == gameplay::GrenadeCookOutcome::DetonateInHand;
                grenadeCook_.reset();
            }
        }
    }
    const bool quickMelee = quickMeleePending_;
    const bool fireRequested = quickMelee || (!grenadeSelected_
        && gameplay::weaponFireRequested(
            weapon_, firePending_, fireHeld_));
    if (fireRequested && player_.state().alive) {
        const bool meleeAttack = quickMelee || weapon_.definition().archetype
            == gameplay::WeaponArchetype::Knife;
        const bool wasReloading = weapon_.reloading();
        const gameplay::ShotResult shot = quickMelee
            ? gameplay::fireQuickMelee(weapon_, player_.cameraPosition(),
                player_.forwardDirection(), playerRoom, collisionWorld_, entities_,
                reachableShotRooms)
            : gameplay::fireHitscan(weapon_, player_.cameraPosition(),
                player_.forwardDirection(), playerRoom, collisionWorld_, entities_,
                1.0F - aimBlend_ * 0.60F, reachableShotRooms,
                gameplay::weaponMovementInaccuracyDegrees(
                    weapon_.definition().archetype, player_.state().velocity,
                    player_.state().grounded, player_.state().crouching, aimBlend_));
        if (!wasReloading && weapon_.reloading()) {
            if (weapon_.definition().archetype == gameplay::WeaponArchetype::Sniper)
                aimBlend_ = 0.0F;
            log(LogLevel::Info, "Weapon empty; reload started on trigger");
            if (audio_) {
                audio_->play(gdp::audio::AudioEventKind::Reload,
                    config_.worldSeed, audioEventCounter_++);
            }
        }
        const gameplay::CombatSignalBatch signals
            = gameplay::combatSignalsForShot(shot);
        const entities::PropData* hitProp = shot.hit
            ? entities_.findData<entities::PropData>(shot.hit->target) : nullptr;
        const bool hitCeilingFixture = hitProp != nullptr
            && hitProp->type == entities::PropType::CeilingFixture;
        for (std::uint8_t index = 0; index < signals.count; ++index) {
            const gameplay::CombatSignal& signal = signals.signals[index];
            if (signal.impact) {
                activeImpacts_.push_back({
                    .render = {
                        .position = signal.impact->point,
                        .normal = signal.impact->normal,
                        .material = impactVisualMaterial(signal.impact->material),
                        .strength = 1.0F,
                        .visualSeed = wkc::core::hashCombine(
                            config_.worldSeed, impactVisualCounter_++),
                    },
                    .roomId = playerRoom,
                    .remaining = impactVisualDuration,
                });
            }
            if (signal.kind != gameplay::CombatSignalKind::WeaponFired) {
                continue;
            }
            if (config_.enableGamepad && runtimeSettings_.gamepadVibration
                && runtimeSettings_.gamepadVibrationPercent > 0U) {
                const float strength = static_cast<float>(runtimeSettings_.gamepadVibrationPercent) / 100.0F;
                (void)gamepad_.rumble((meleeAttack ? 0.12F : 0.28F) * strength,
                    (meleeAttack ? 0.18F : 0.36F) * strength, meleeAttack ? 70U : 100U);
            }
            if (meleeAttack) {
                weaponMeleeRemaining_ = weaponMeleeDuration;
            } else {
                weaponRecoilRemaining_ = weaponRecoilDuration;
                weaponMuzzleFlashRemaining_ = weaponMuzzleFlashDuration;
            }
            (void)gameplay::alertEnemiesToNoise(
                entities_, playerRoom,
                {.position = player_.state().position,
                    .radius = meleeAttack ? 4.0F : 18.0F});
            for (const world::RoomId neighbor : reachableShotRooms) {
                (void)gameplay::alertEnemiesToNoise(
                    entities_, neighbor,
                    {.position = player_.state().position,
                        .radius = meleeAttack ? 4.0F : 18.0F});
            }
            if (audio_) {
                audio_->playAt(meleeAttack ? gdp::audio::AudioEventKind::MeleeSwing
                                           : gdp::audio::AudioEventKind::WeaponFire,
                    config_.worldSeed, audioEventCounter_++,
                    player_.cameraPosition(), player_.cameraPosition(),
                    player_.forwardDirection());
            }
        }
        // The hitscan above uses the pre-kick aim. Only a successfully fired
        // round moves the camera, so the next shot requires counter-steering.
        if (!meleeAttack && signals.count > 0U) {
            player_.applyRecoil(gameplay::weaponRecoilKickDegrees(
                weapon_.definition().archetype, weapon_.spreadShotIndex(),
                aimBlend_, player_.state().crouching));
        }
        if (audio_ && shot.outcome == gameplay::ShotOutcome::Empty) {
            audio_->playAt(gdp::audio::AudioEventKind::DryFire,
                config_.worldSeed, audioEventCounter_++,
                player_.cameraPosition(), player_.cameraPosition(),
                player_.forwardDirection());
        }
        for (std::uint8_t index = 0U; index < signals.count; ++index) {
            const auto& signal = signals.signals[index];
            if (!signal.spawnedItem.isValid() || !world_) continue;
            const entities::Entity* item = entities_.find(signal.spawnedItem);
            if (item == nullptr) continue;
            const auto room = std::find_if(
                world_->rooms.begin(), world_->rooms.end(),
                [item](const world::RoomInstance& candidate) {
                    return candidate.id == item->roomId;
                });
            if (room != world_->rooms.end()) {
                room->entities.push_back(signal.spawnedItem);
            }
        }
        if (std::any_of(signals.signals.begin(), signals.signals.begin() + signals.count,
                [](const auto& signal) { return signal.outcome == gameplay::ShotOutcome::Destroyed; })) {
            collisionWorld_ = gameplay::collisionWorldWithProps(
                baseCollisionWorld_, entities_, playerRoom);
        }
        for (std::uint8_t index = 0; index < signals.count; ++index) {
            const gameplay::CombatSignal& signal = signals.signals[index];
            if (signal.kind == gameplay::CombatSignalKind::EntityDamaged
                || signal.kind == gameplay::CombatSignalKind::EntityKilled) {
                gameplay::signalHit(feedback_,
                    signal.outcome == gameplay::ShotOutcome::Killed,
                    signal.zone == gameplay::HitZone::WeakPoint);
            }
            if (!audio_) {
                continue;
            }
            if (signal.kind == gameplay::CombatSignalKind::EntityHit) {
                std::optional<gdp::audio::AudioEventKind> impactKind;
                if (signal.outcome == gameplay::ShotOutcome::Hit) {
                    const entities::Entity* target = entities_.find(signal.target);
                    impactKind = target != nullptr
                        && target->kind == entities::EntityKind::Prop
                        ? gdp::audio::AudioEventKind::PropImpact
                        : gdp::audio::AudioEventKind::EnemyHit;
                } else if (signal.outcome == gameplay::ShotOutcome::Killed) {
                    impactKind = gdp::audio::AudioEventKind::EnemyDeath;
                } else if (signal.outcome == gameplay::ShotOutcome::Destroyed) {
                    impactKind = gdp::audio::AudioEventKind::PropDestroyed;
                } else if (signal.outcome == gameplay::ShotOutcome::ImpactProp) {
                    impactKind = gdp::audio::AudioEventKind::PropImpact;
                }
                if (impactKind) {
                    audio_->playAt(*impactKind, config_.worldSeed, audioEventCounter_++,
                        signal.point, player_.cameraPosition(),
                        player_.forwardDirection());
                }
            } else if (signal.kind == gameplay::CombatSignalKind::ImpactWorld) {
                audio_->playAt(gdp::audio::AudioEventKind::PropImpact,
                    config_.worldSeed, audioEventCounter_++,
                    signal.impact ? signal.point : player_.cameraPosition(),
                    player_.cameraPosition(), player_.forwardDirection());
            }
        }
        for (std::uint8_t index = 0U; index < signals.count; ++index) {
            const auto& signal = signals.signals[index];
            if (signal.kind != gameplay::CombatSignalKind::EntityKilled) continue;
            const auto* killed = entities_.findData<entities::EnemyData>(signal.target);
            if (killed && signal.outcome == gameplay::ShotOutcome::Killed)
                awardProgress(progressionEventFor(killed->archetype), 1U);
            const auto* prop = entities_.findData<entities::PropData>(signal.target);
            if (signal.outcome == gameplay::ShotOutcome::Destroyed && prop
                && prop->type != entities::PropType::CeilingFixture)
                awardProgress(gameplay::ProgressionEvent::PropDestroyed, 1U);
        }
        std::ostringstream message;
        if (shot.outcome == gameplay::ShotOutcome::Hit
            || shot.outcome == gameplay::ShotOutcome::Killed
            || shot.outcome == gameplay::ShotOutcome::Destroyed) {
            const bool weakPoint = shot.hit
                && shot.hit->zone == gameplay::HitZone::WeakPoint;
            message << (weakPoint ? "Weak point | " : "")
                    << (shot.outcome == gameplay::ShotOutcome::Killed
                    ? "Enemy killed: "
                    : shot.outcome == gameplay::ShotOutcome::Destroyed
                        ? hitCeilingFixture ? "Ceiling fixture destroyed: "
                                            : "Prop destroyed: "
                        : "Target hit: ")
                    << shot.hit->target.packed() << " | ammo "
                    << weapon_.magazineAmmo() << '/' << weapon_.reserveAmmo();
            log(LogLevel::Info, message.str());
        } else if (shot.outcome == gameplay::ShotOutcome::Empty) {
            log(LogLevel::Info, "Weapon empty; no reserve ammo");
        } else if (shot.outcome == gameplay::ShotOutcome::ImpactProp) {
            log(LogLevel::Info, "Protected prop absorbed the shot");
        }
    }
    firePending_ = false;
    quickMeleePending_ = false;

    const bool playerWasAlive = player_.state().alive;
    bool grenadeHitPlayer = false;
    auto grenadeDetonations = playerGrenades_.update(step, entities_);
    if (grenadeThrowPending) {
        if (playerGrenades_.throwGrenade(player_.cameraPosition(),
                player_.forwardDirection(), grenadeThrowChargeSeconds,
                playerRoom, collisionWorld_, reachableShotRooms,
                grenadeThrowFuseSeconds)) {
            log(LogLevel::Info, "Fragmentation grenade thrown");
            if (audio_) {
                audio_->playAt(gdp::audio::AudioEventKind::MeleeSwing,
                    config_.worldSeed, audioEventCounter_++,
                    player_.cameraPosition(), player_.cameraPosition(),
                    player_.forwardDirection());
            }
        } else {
            grenadeHandBlastPending = true;
        }
    }
    if (grenadeHandBlastPending) {
        grenadeDetonations.push_back({player_.cameraPosition(), playerRoom,
            collisionWorld_, reachableShotRooms});
        log(LogLevel::Info, "Fragmentation grenade detonated in hand");
    }
    for (const gameplay::GrenadeDetonation& blast : grenadeDetonations) {
        activeGrenadeBlasts_.push_back({blast.position, blast.roomId,
            grenadeBlastVisualDuration});
        const gameplay::GrenadeBlastResult result
            = gameplay::resolveGrenadeBlast(blast, player_, entities_);
        grenadeHitPlayer = grenadeHitPlayer || result.playerDamaged;
        bool destroyedProp = false;
        for (const gameplay::GrenadeHit& hit : result.hits) {
            const auto* target = entities_.find(hit.target);
            if (!target) continue;
            if (hit.prop) {
                destroyedProp = destroyedProp || hit.killed;
                const auto* prop = entities_.findData<entities::PropData>(hit.target);
                if (hit.killed && prop
                    && prop->type != entities::PropType::CeilingFixture)
                    awardProgress(gameplay::ProgressionEvent::PropDestroyed, 1U);
            } else {
                const auto* enemy = entities_.findData<entities::EnemyData>(hit.target);
                if (hit.killed && enemy)
                    awardProgress(progressionEventFor(enemy->archetype), 1U);
            }
            gameplay::signalHit(feedback_, hit.killed, false);
            if (hit.spawnedDrop.isValid() && world_) {
                const auto* item = entities_.find(hit.spawnedDrop);
                if (item) {
                    const auto room = std::find_if(world_->rooms.begin(), world_->rooms.end(),
                        [item](const world::RoomInstance& candidate) {
                            return candidate.id == item->roomId;
                        });
                    if (room != world_->rooms.end()) room->entities.push_back(hit.spawnedDrop);
                }
            }
        }
        if (destroyedProp) {
            collisionWorld_ = gameplay::collisionWorldWithProps(
                baseCollisionWorld_, entities_, playerRoom);
        }
        (void)gameplay::alertEnemiesToNoise(entities_, blast.roomId,
            {.position = blast.position, .radius = 20.0F});
        for (const auto roomId : blast.reachableRooms) {
            (void)gameplay::alertEnemiesToNoise(entities_, roomId,
                {.position = blast.position, .radius = 20.0F});
        }
        if (audio_) {
            audio_->playAt(gdp::audio::AudioEventKind::PropDestroyed,
                config_.worldSeed, audioEventCounter_++, blast.position,
                player_.cameraPosition(), player_.forwardDirection());
        }
        log(LogLevel::Info, "Fragmentation grenade detonated");
    }
    const gameplay::EnemyDifficultyConfigs difficultyConfigs
        = gameplay::makeEnemyDifficultyConfigs(runDifficulty_);
    std::vector<gameplay::EnemyProjectile> spawnedProjectiles;
    const gameplay::EnemyPerception perception{
        .playerFlashlightOn = flashlightOn_,
        .projectiles = &spawnedProjectiles,
        .difficulty = session_.state().difficulty,
    };
    const auto brainFor = [this](const entities::EnemyArchetype archetype)
        -> gameplay::NeuralEnemyExperiment* {
        if (neuralEnemyExperiment_) {
            return &*neuralEnemyExperiment_;
        }
        auto& brain = activeEnemyBrains_[static_cast<std::size_t>(archetype)];
        return brain ? &*brain : nullptr;
    };
    const auto openNeighbors = [this, &blockedDoorEdges](const world::RoomId roomId) {
        std::vector<world::RoomId> neighbors;
        if (!world_) {
            return neighbors;
        }
        for (const world::CorridorPath& corridor : world_->corridors) {
            if (std::find(blockedDoorEdges.begin(), blockedDoorEdges.end(), corridor.edge)
                != blockedDoorEdges.end()) {
                continue;
            }
            const world::RoomId neighbor = corridor.firstRoom == roomId
                ? corridor.secondRoom
                : corridor.secondRoom == roomId ? corridor.firstRoom : 0U;
            if (neighbor != 0U
                && std::find(neighbors.begin(), neighbors.end(), neighbor)
                    == neighbors.end()) {
                neighbors.push_back(neighbor);
            }
        }
        return neighbors;
    };
    std::vector<world::RoomId> activeEnemyRooms{playerRoom};
    const auto playerRoomNeighbors = openNeighbors(playerRoom);
    activeEnemyRooms.insert(activeEnemyRooms.end(),
        playerRoomNeighbors.begin(), playerRoomNeighbors.end());
    // Keep updating an enemy already between rooms even if the player has
    // moved beyond the adjacent room. Its source room remains authoritative
    // until its whole collider enters the destination.
    if (world_) {
        for (const world::CorridorPath& corridor : world_->corridors) {
            if (!gameplay::livingEnemyOccupiesCorridor(
                    *world_, corridor, entities_)) {
                continue;
            }
            for (const world::RoomId endpoint
                 : {corridor.firstRoom, corridor.secondRoom}) {
                if (std::find(activeEnemyRooms.begin(), activeEnemyRooms.end(),
                        endpoint) == activeEnemyRooms.end()) {
                    activeEnemyRooms.push_back(endpoint);
                }
            }
        }
    }
    std::size_t attacksLanded = grenadeHitPlayer ? 1U : 0U;
    for (const world::RoomId enemyRoom : activeEnemyRooms) {
        gameplay::EnemyPerception roomPerception = perception;
        roomPerception.allowAttacks = !roomEntryGraceRemaining_.contains(enemyRoom);
        const auto neighbors = openNeighbors(enemyRoom);
        const gameplay::CollisionWorld enemyBase = enemyRoom == playerRoom
            ? baseCollisionWorld_
            : gameplay::makeRoomNavigationCollision(
                *world_, enemyRoom, false, blockedDoorEdges);
        const gameplay::CollisionWorld enemyCollision
            = gameplay::collisionWorldWithProps(
                enemyBase, entities_, enemyRoom, neighbors);
        attacksLanded += gameplay::updateWorkers(
            entities_, player_, enemyRoom, enemyCollision, step,
            difficultyConfigs.worker, roomPerception,
            brainFor(entities::EnemyArchetype::Worker)).attacksLanded;
        attacksLanded += gameplay::updateGuards(
            entities_, player_, enemyRoom, enemyCollision, step,
            difficultyConfigs.guard, roomPerception,
            brainFor(entities::EnemyArchetype::Guard)).attacksLanded;
        attacksLanded += gameplay::updateOfficers(
            entities_, player_, enemyRoom, enemyCollision, step,
            difficultyConfigs.officer, roomPerception,
            brainFor(entities::EnemyArchetype::Officer)).attacksLanded;
        attacksLanded += gameplay::updateEngineers(
            entities_, player_, enemyRoom, enemyCollision, step,
            difficultyConfigs.engineer, roomPerception,
            brainFor(entities::EnemyArchetype::Engineer)).attacksLanded;
        attacksLanded += gameplay::updateIndustrialRobots(
            entities_, player_, enemyRoom, enemyCollision, step,
            difficultyConfigs.industrialRobot, roomPerception,
            brainFor(entities::EnemyArchetype::IndustrialRobot)).attacksLanded;
    }
    enemyProjectiles_.spawn(spawnedProjectiles);
    for (const gameplay::EnemyProjectileEvent& event
         : enemyProjectiles_.update(step, player_, collisionWorld_, entities_)) {
        if (event.hitPlayer) {
            ++attacksLanded;
        }
        if (event.hitEnemy.isValid() && audio_) {
            if (const entities::Entity* target = entities_.find(event.hitEnemy)) {
                audio_->playAt(event.killedEnemy
                        ? gdp::audio::AudioEventKind::EnemyDeath
                        : gdp::audio::AudioEventKind::EnemyHit,
                    config_.worldSeed, audioEventCounter_++,
                    target->transform.position
                        + wkc::core::Vec3{0.0F, 1.0F, 0.0F},
                    player_.cameraPosition(), player_.forwardDirection());
            }
        }
    }
    if (world_) {
        gameplay::updateEnemyRoomMembership(*world_, entities_);
    }
    if (audio_ && attacksLanded > 0U) {
        audio_->play(gdp::audio::AudioEventKind::PlayerHurt,
            config_.worldSeed, audioEventCounter_++);
    }
    if (attacksLanded > 0U) {
        gameplay::signalPlayerDamage(feedback_);
        if (config_.enableGamepad && runtimeSettings_.gamepadVibration
            && runtimeSettings_.gamepadVibrationPercent > 0U) {
            const float strength = static_cast<float>(runtimeSettings_.gamepadVibrationPercent) / 100.0F;
            (void)gamepad_.rumble(0.8F * strength, 0.45F * strength, 220U);
        }
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
    const bool exitTerminalUsed = playerCommand_.interact
        && campaignMission_.completed && isCampaignExitTargeted();
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
    if (terminalUsed && session_.state().mode == gameplay::RunMode::TestMode) {
        weapon_.restockAllFirearms();
        player_.refillGrenades();
        player_.heal(player_.state().maximumHealth);
        player_.addArmor(player_.state().maximumArmor);
        if (auto* data = entities_.findData<entities::PlayerData>(player_.entity())) {
            data->health = player_.state().health;
            data->armor = player_.state().armor;
        }
        gameplay::signalNotice(feedback_, gameplay::NoticeFeedbackKind::Pickup);
        log(LogLevel::Info, "Test Mode supplies replenished");
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
            (void)gameplay::advanceCampaignMission(campaignMission_,
                activeEvent->roomId, {
                    .terminalActivated = activeEvent->plan.role == world::RoomRole::Start
                        && activeEvent->plan.type == world::RoomEventType::TerminalAmbush,
                });
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
            const gameplay::CampaignMissionProgress missionProgress{
                .arenaCleared = activeEvent->plan.role == world::RoomRole::Arena
                    && activeEvent->plan.type == world::RoomEventType::Lockdown,
                .terminalAmbushCleared = activeEvent->plan.role == world::RoomRole::Start
                    && activeEvent->plan.type == world::RoomEventType::TerminalAmbush,
            };
            if (gameplay::advanceCampaignMission(campaignMission_,
                    activeEvent->roomId, missionProgress)) {
                log(LogLevel::Info, "Campaign mission completed: "
                    + std::string{gameplay::campaignMissionName(campaignMission_.kind)});
            }
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
    bool bossAccessible = false;
    if (world_) {
        const auto currentRoom = std::find_if(
            world_->rooms.begin(), world_->rooms.end(),
            [playerRoom](const world::RoomInstance& room) {
                return room.id == playerRoom;
            });
        if (currentRoom != world_->rooms.end()) {
            currentRoomRole = currentRoom->role;
        }
        const auto bossRoom = std::find_if(world_->rooms.begin(),
            world_->rooms.end(), [](const world::RoomInstance& room) {
                return room.role == world::RoomRole::Boss;
            });
        if (bossRoom != world_->rooms.end()) {
            boss = gameplay::bossStatus(entities_, 0U);
            bossAccessible = bossRoom->id == playerRoom;
            for (const world::CorridorPath& corridor : world_->corridors) {
                const bool connectsPlayerToBoss
                    = (corridor.firstRoom == playerRoom
                        && corridor.secondRoom == bossRoom->id)
                    || (corridor.secondRoom == playerRoom
                        && corridor.firstRoom == bossRoom->id);
                if (!connectsPlayerToBoss) {
                    continue;
                }
                const auto door = doorRuntimes_.find(corridor.edge);
                bossAccessible = bossAccessible
                    || (door != doorRuntimes_.end()
                        && gameplay::isDoorPassable(door->second));
            }
        }
    }
    const bool exitWasReady = session_.state().campaignExitReady;
    const gameplay::SessionUpdate sessionUpdate = session_.update(
        player_.state().alive, currentRoomRole, boss.exists,
        gameplay::bossEncounterAlive(entities_, bossRoomId_),
        bossAccessible, exitTerminalUsed, campaignMission_.completed);
    if (!exitWasReady && session_.state().campaignExitReady) {
        log(LogLevel::Info, "Campaign exit terminal unlocked after Boss encounter clear");
    }
    if (sessionUpdate.changed) {
        interactionPending_ = false;
        jumpPending_ = false;
        playerCommand_.jump = false;
        firePending_ = false;
        quickMeleePending_ = false;
        fireHeld_ = false;
        reloadPending_ = false;
        weaponSwitchPending_.reset();
        dropPending_ = false;
        latestSpeedrunRecordResult_ = gameplay::considerSpeedrunResult(
            session_.state(), speedrunRecord_);
        if (persistence::addSpeedrunLeaderboardResult(speedrunLeaderboard_, session_.state())
            && speedrunLeaderboardPath_ && !persistence::saveSpeedrunLeaderboard(*speedrunLeaderboardPath_, speedrunLeaderboard_))
            log(LogLevel::Warning, "Speedrun leaderboard could not be saved; previous file preserved");
        if (gameplay::speedrunRecordChanged(latestSpeedrunRecordResult_)) {
            std::ostringstream recordMessage;
            recordMessage << "New local Speedrun record: "
                          << visual::runTimeText(speedrunRecord_->bestTicks)
                          << " | seed " << speedrunRecord_->seed;
            log(LogLevel::Info, recordMessage.str());
            (void)savePersistedSpeedrunRecord();
        }
        if (sessionUpdate.phase == gameplay::SessionPhase::Victory
            && session_.state().mode == gameplay::RunMode::Campaign
            && session_.state().campaignFloor
                >= gameplay::campaignFloorCount) {
            (void)clearPersistedCampaignCheckpoint();
        }
        std::ostringstream message;
        message << "Session result: "
                << gameplay::sessionPhaseName(sessionUpdate.phase)
                << " | run " << session_.state().runIndex
                << " | seed " << session_.state().currentSeed
                << " | mode " << gameplay::runModeName(session_.state().mode)
                << " | time "
                << visual::runTimeText(session_.state().elapsedRunTicks);
        log(LogLevel::Info, message.str());
        if (audio_ && sessionUpdate.phase == gameplay::SessionPhase::Victory) {
            audio_->play(gdp::audio::AudioEventKind::EventComplete,
                session_.state().currentSeed, audioEventCounter_++);
        }
        if (gameplay::canAdvanceCampaign(session_.state())) {
            gameplay::CampaignCarryover carryover
                = gameplay::prepareNextCampaignFloorCarryover(
                    gameplay::captureCampaignCarryover(
                        player_, weapon_, progression_));
            if (!gameplay::validateCampaignCarryover(carryover)) {
                log(LogLevel::Error, "Campaign exit produced invalid carry-over state");
                fatalError_ = true;
                running_ = false;
                return;
            }
            campaignCarryover_ = std::move(carryover);
            restartMode_ = gameplay::RunRestartMode::NextCampaignFloor;
            restartRequested_ = true;
            running_ = false;
            log(LogLevel::Info, "Exit terminal requested the next Campaign floor");
            return;
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
    updateEnemyAnimationTransitions(false);
    ++enemyAnimationTicks_;
    ++fixedUpdates_;
}

void Application::updateEnemyAnimationTransitions(const bool deadOnly)
{
    for (const entities::EntityId id
        : entities_.query(entities::EntityKind::Enemy)) {
        const entities::EnemyData* enemy
            = entities_.findData<entities::EnemyData>(id);
        if (enemy == nullptr
            || (deadOnly
                && enemy->behavior != entities::EnemyBehaviorState::Dead)) {
            continue;
        }
        visual::advanceEnemyAnimationTransition(
            enemyAnimationTransitions_[id.packed()], animationStateFor(*enemy),
            visualRoleFor(*enemy));
    }
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
    const gameplay::GraphicsQualitySettings graphicsQuality
        = gameplay::graphicsQualitySettings(runtimeSettings_.graphicsQuality);
    const visual::HudGraphicsQuality hudGraphicsQuality
        = static_cast<visual::HudGraphicsQuality>(runtimeSettings_.graphicsQuality);

    const auto renderAndPresent = [this](auto&& drawFrame,
                                      const std::string_view errorMessage) {
        using Clock = std::chrono::steady_clock;
        const auto renderStart = Clock::now();
        const bool rendered = renderer_ != nullptr && drawFrame();
        const auto renderEnd = Clock::now();
        currentFramePerformance_.renderMilliseconds
            = std::chrono::duration<double, std::milli>(
                renderEnd - renderStart).count();
        if (!rendered) {
            log(LogLevel::Error, errorMessage);
            fatalError_ = true;
            running_ = false;
            return false;
        }
        const visual::RendererFrameStats rendererStats
            = renderer_->lastFrameStats();
        currentFramePerformance_.drawCalls = rendererStats.drawCalls;
        currentFramePerformance_.triangles = rendererStats.triangles;
        currentFramePerformance_.gpuMilliseconds
            = rendererStats.gpuMilliseconds;
        currentFramePerformance_.gpuTimingAvailable
            = rendererStats.gpuTimingAvailable;
        currentFramePerformance_.meshCacheEntries = static_cast<std::uint32_t>(
            std::min<std::size_t>(rendererStats.meshCacheEntries,
                std::numeric_limits<std::uint32_t>::max()));
        currentFramePerformance_.textureCacheEntries = static_cast<std::uint32_t>(
            std::min<std::size_t>(rendererStats.textureCacheEntries,
                std::numeric_limits<std::uint32_t>::max()));
        currentFramePerformance_.uniformUploads = rendererStats.uniformUploads;
        currentFramePerformance_.uniformSkips = rendererStats.uniformSkips;
        currentFramePerformance_.textureUnitActivations
            = rendererStats.textureUnitActivations;
        currentFramePerformance_.textureBinds = rendererStats.textureBinds;
        currentFramePerformance_.textureBindSkips
            = rendererStats.textureBindSkips;
        currentFramePerformance_.meshBinds = rendererStats.meshBinds;
        currentFramePerformance_.meshBindSkips = rendererStats.meshBindSkips;
        currentFramePerformance_.lowSpecShader = rendererStats.lowSpecShader;
        currentFramePerformance_.backfaceCulledDraws
            = rendererStats.backfaceCulledDraws;
        currentFramePerformance_.cullStateChanges
            = rendererStats.cullStateChanges;
        currentFramePerformance_.instancedDrawCalls
            = rendererStats.instancedDrawCalls;
        currentFramePerformance_.renderedInstances
            = rendererStats.renderedInstances;
        currentFramePerformance_.avoidedDrawCalls
            = rendererStats.avoidedDrawCalls;
        currentFramePerformance_.shadowShaderOccluderCapacity
            = rendererStats.shadowShaderOccluderCapacity;
        const auto presentStart = Clock::now();
        SDL_GL_SwapWindow(window_);
        currentFramePerformance_.presentMilliseconds
            = std::chrono::duration<double, std::milli>(
                Clock::now() - presentStart).count();
        return true;
    };

    if (controlsMenuActive_) {
        SDL_SetWindowTitle(window_, ("GDP | " + localize(runtimeSettings_.language, "CONTROLS")).c_str());
        visual::HudRenderData hud;
        hud.language = runtimeSettings_.language;
        hud.controlsMenu = true;
        if (!renderAndPresent([&] {
                return renderer_->render(
                    width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, hud);
            }, "Renderer failed while drawing controls")) {
            return;
        }
        return;
    }

    if (settingsMenu_.active || mainMenu_.active) {
        const auto view = frontendMenuView();
        SDL_SetWindowTitle(window_, ("GDP | " + view.title).c_str());
        visual::HudRenderData hud;
        hud.language = runtimeSettings_.language;
        hud.listMenu.active = true;
        hud.listMenu.titleScreen = menuFlow_.page == MenuPage::Root;
        hud.listMenu.title = view.title;
        hud.listMenu.subtitle = view.subtitle;
        hud.listMenu.selection = menuFlow_.selection;
        for (const auto& row : view.rows) {
            hud.listMenu.rows.push_back(row.text);
            hud.listMenu.enabled.push_back(row.enabled);
            hud.listMenu.adjustable.push_back(row.setting.has_value());
            hud.listMenu.rawText.push_back(row.rawText);
        }
        hud.exitConfirmation = exitConfirmation_.active;
        if (exitConfirmation_.selection == gameplay::ExitConfirmationOption::ExitGame)
            hud.exitConfirmationSelection = visual::HudExitConfirmationSelection::ExitGame;
        const auto backdrop = visual::sampleMenuBackdrop(menuBackdropSeconds_);
        hud.worldRenderScalePercent = graphicsQuality.renderScalePercent;
        hud.detailedMaterials = graphicsQuality.detailedMaterials;
        hud.graphicsQuality = hudGraphicsQuality;
        (void)renderAndPresent([&] {
            return renderer_->render(width, height, backdrop.camera, backdrop.enemies,
                {}, backdrop.props, backdrop.lighting, backdrop.room, {}, {}, {}, hud);
        }, "Renderer failed while drawing frontend menu");
        return;
    }

    const wkc::core::Vec3 renderCameraPosition = player_.cameraPosition()
        + gameplay::viewMotionOffset(viewMotion_, player_.rightDirection());
    const visual::CameraData camera{
        renderCameraPosition,
        player_.forwardDirection(),
        static_cast<float>(runtimeSettings_.fieldOfViewDegrees)
            * (1.0F - aimBlend_ * (1.0F
                - gameplay::weaponAimFovScale(weapon_.definition().archetype))),
    };
    const float cameraAspect = height > 0
        ? static_cast<float>(width) / static_cast<float>(height)
        : 1.0F;
    const auto sphereIsVisible = [&camera, cameraAspect](
                                     const wkc::core::Vec3 center,
                                     const float radius) {
        return visual::isSpherePotentiallyVisible(
            camera, center, radius, cameraAspect);
    };
    std::vector<visual::EnemyRenderData> visibleEnemies;
    std::vector<visual::PickupRenderData> visiblePickups;
    std::vector<visual::PropRenderData> visibleProps;
    const entities::Entity* playerEntity = entities_.find(player_.entity());
    const std::uint32_t playerRoom = playerEntity == nullptr ? 0 : playerEntity->roomId;
    std::vector<world::RoomId> visibleRoomIds = world_
        ? roomsWithinGraphDistance(
            *world_, playerRoom, graphicsQuality.visibleRoomDepth)
        : std::vector<world::RoomId>{playerRoom};
    if (world_) {
        visibleRoomIds.erase(std::remove_if(
            visibleRoomIds.begin(), visibleRoomIds.end(),
            [this, playerRoom, &camera, cameraAspect](const world::RoomId roomId) {
                if (roomId == playerRoom) {
                    return false;
                }
                const auto room = std::find_if(
                    world_->rooms.begin(), world_->rooms.end(),
                    [roomId](const world::RoomInstance& candidate) {
                        return candidate.id == roomId;
                    });
                return room != world_->rooms.end()
                    && !visual::isAabbPotentiallyVisible(
                        camera, room->worldBounds, cameraAspect);
            }),
            visibleRoomIds.end());
    }
    const auto isVisibleRoom = [&visibleRoomIds](const world::RoomId roomId) {
        return std::find(visibleRoomIds.begin(), visibleRoomIds.end(), roomId)
            != visibleRoomIds.end();
    };
    for (const entities::EntityId id : entities_.query(entities::EntityKind::Enemy)) {
        const entities::Entity* entity = entities_.find(id);
        const entities::EnemyData* enemy = entities_.findData<entities::EnemyData>(id);
        if (entity != nullptr && enemy != nullptr && isVisibleRoom(entity->roomId)
            && sphereIsVisible(
                entity->transform.position + wkc::core::Vec3{0.0F, 0.9F, 0.0F},
                1.5F)
            && (enemy->archetype == entities::EnemyArchetype::Worker
                || enemy->archetype == entities::EnemyArchetype::Guard
                || enemy->archetype == entities::EnemyArchetype::Officer
                || enemy->archetype == entities::EnemyArchetype::Engineer
                || enemy->archetype == entities::EnemyArchetype::IndustrialRobot)) {
            const visual::EnemyVisualRole role = visualRoleFor(*enemy);
            const visual::EnemyAnimationState animationState
                = animationStateFor(*enemy);
            visual::EnemyAnimationState previousAnimationState = animationState;
            float animationTransitionBlend = 1.0F;
            const auto transition
                = enemyAnimationTransitions_.find(id.packed());
            if (transition != enemyAnimationTransitions_.end()
                && transition->second.currentState == animationState) {
                previousAnimationState = transition->second.previousState;
                animationTransitionBlend
                    = visual::enemyAnimationTransitionBlend(transition->second);
            }
            visibleEnemies.push_back({
                entity->transform.position,
                enemy->behavior != entities::EnemyBehaviorState::Dead,
                enemy->visualSeed,
                role,
                enemy->active,
                animationState,
                enemyAnimationTicks_,
                entity->transform.rotationDegrees.y,
                std::clamp(
                    enemy->hitReactionTime / gameplay::enemyHitReactionDuration,
                    0.0F, 1.0F),
                previousAnimationState,
                animationTransitionBlend,
            });
        }
    }
    for (const entities::EntityId id : entities_.query(entities::EntityKind::Item)) {
        const entities::Entity* entity = entities_.find(id);
        const entities::ItemData* item = entities_.findData<entities::ItemData>(id);
        if (entity == nullptr || item == nullptr || !isVisibleRoom(entity->roomId)
            || !sphereIsVisible(
                entity->transform.position + wkc::core::Vec3{0.0F, 0.35F, 0.0F},
                0.75F)) {
            continue;
        }
        visual::PickupVisualRole role = visual::PickupVisualRole::Health;
        if (item->type == entities::ItemType::Ammo) {
            role = visual::PickupVisualRole::Ammo;
        } else if (item->type == entities::ItemType::Key) {
            role = visual::PickupVisualRole::Key;
        } else if (item->type == entities::ItemType::Armor) {
            role = visual::PickupVisualRole::Armor;
        } else if (item->type == entities::ItemType::Weapon) {
            const auto type = item->recoveredWeapon
                ? std::optional{item->recoveredWeapon->archetype} : item->spawnedWeaponArchetype;
            role = type == std::optional{static_cast<std::uint32_t>(gameplay::WeaponArchetype::Pistol)}
                ? visual::PickupVisualRole::DroppedPistol
                : type == std::optional{static_cast<std::uint32_t>(gameplay::WeaponArchetype::Sniper)}
                    ? visual::PickupVisualRole::DroppedSniper
                    : type == std::optional{static_cast<std::uint32_t>(gameplay::WeaponArchetype::Shotgun)}
                        ? visual::PickupVisualRole::DroppedShotgun
                    : type == std::optional{static_cast<std::uint32_t>(gameplay::WeaponArchetype::SubmachineGun)}
                        ? visual::PickupVisualRole::DroppedSubmachineGun
                        : visual::PickupVisualRole::Weapon;
        } else if (item->type == entities::ItemType::Special) {
            role = visual::PickupVisualRole::Special;
        }
        float fallProgress = 1.0F;
        if (item->droppedVisualStartTick) {
            constexpr float fallDurationTicks = 25.0F;
            const std::uint64_t elapsedTicks
                = fixedUpdates_ >= *item->droppedVisualStartTick
                ? fixedUpdates_ - *item->droppedVisualStartTick : 0U;
            fallProgress = std::clamp(
                static_cast<float>(elapsedTicks) / fallDurationTicks,
                0.0F, 1.0F);
        }
        visiblePickups.push_back({entity->transform.position, role,
            id.packed(), fallProgress, item->droppedVisualYawDegrees});
    }
    for (const entities::EntityId id : entities_.query(entities::EntityKind::Prop)) {
        const entities::Entity* entity = entities_.find(id);
        const entities::PropData* prop = entities_.findData<entities::PropData>(id);
        if (entity == nullptr || prop == nullptr || !isVisibleRoom(entity->roomId)
            || !sphereIsVisible(
                entity->transform.position
                    + wkc::core::Vec3{
                        0.0F, entity->transform.scale.y * 0.5F, 0.0F},
                std::max(0.75F,
                    wkc::core::length(entity->transform.scale) * 0.65F))
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
          << " | " << gameplay::runModeName(session_.state().mode)
          << " | HP " << static_cast<int>(player_.state().health)
          << " | Armor " << static_cast<int>(player_.state().armor)
          << " | " << gameplay::weaponArchetypeName(weapon_.definition().archetype)
          << ' ' << weapon_.magazineAmmo() << '/' << weapon_.reserveAmmo();
    title << " | Keys " << player_.state().keys;
    if (weapon_.overdriveActive()) {
        title << " | OVERDRIVE "
              << static_cast<unsigned>(std::ceil(weapon_.overdriveRemaining()))
              << 's';
    }
    const gameplay::ProgressionState& progress = progression_.state();
    title << " | Lv " << progress.level << " XP " << progress.experienceIntoLevel;
    const std::uint32_t nextLevelXp = gameplay::experienceForNextLevel(progress.level);
    if (nextLevelXp > 0U) {
        title << '/' << nextLevelXp;
    } else {
        title << " MAX";
    }
    title << " | Score " << progress.score;
    if (session_.state().mode == gameplay::RunMode::Campaign) {
        title << " | Floor " << session_.state().campaignFloor
              << '/' << gameplay::campaignFloorCount
              << " | Threat "
              << static_cast<unsigned>(std::lround(runDifficulty_ * 100.0F))
              << '%';
    }
    title << " | Time "
          << visual::runTimeText(session_.state().elapsedRunTicks);
    if (session_.state().mode == gameplay::RunMode::Speedrun
        && speedrunRecord_) {
        title << " | Best " << visual::runTimeText(speedrunRecord_->bestTicks);
        if (gameplay::speedrunRecordChanged(latestSpeedrunRecordResult_)) {
            title << " [NEW RECORD]";
        }
    }
    title << " | Room " << playerRoom;
    if (session_.state().phase == gameplay::SessionPhase::Victory) {
        title << " | VICTORY";
    } else if (session_.state().phase == gameplay::SessionPhase::Defeat) {
        title << " | DEFEAT";
    }
    const auto activeEvent = std::find_if(
        roomEvents_.begin(), roomEvents_.end(),
        [playerRoom](const gameplay::RoomEventRuntime& runtime) {
            return runtime.roomId == playerRoom;
        });
    if (activeEvent != roomEvents_.end()) {
        title << " | Event " << gameplay::roomEventTypeName(activeEvent->plan.type)
              << ':' << gameplay::roomEventPhaseName(activeEvent->phase);
        if (gameplay::shouldSealRoomDoors(*activeEvent, entities_)) {
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
                lighting = makeRenderLighting(
                    roomLighting_[roomIndex], alert, entities_,
                    roomFixtureEntities_, currentRoom->id);
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
                        = makeRenderLighting(
                            roomLighting_[otherIndex], false, entities_,
                            roomFixtureEntities_, otherRoomId);
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
                lighting, *world_, roomLighting_, entities_,
                roomFixtureEntities_, playerRoom, player_.state().position);
            lighting.lightCount = std::min<std::uint32_t>(
                lighting.lightCount, graphicsQuality.maximumLights);
            lighting.flashlightEnabled = flashlightOn_
                && player_.state().alive;
            lighting.flashlightPosition = player_.cameraPosition();
            lighting.flashlightDirection = player_.forwardDirection();
            if (graphicsQuality.maximumShadowLights > 0U
                && graphicsQuality.maximumShadowOccluders > 0U) {
                std::vector<visual::ShadowOccluderRenderData> shadowCandidates;
                shadowCandidates.reserve(visibleEnemies.size() + visibleProps.size());
                const auto appendShadowOccluder
                    = [&shadowCandidates](const wkc::core::Aabb bounds) {
                          shadowCandidates.push_back({bounds});
                      };
                for (const visual::EnemyRenderData& enemy : visibleEnemies) {
                    if (!enemy.active || !enemy.alive) {
                        continue;
                    }
                    appendShadowOccluder({
                        enemy.position + wkc::core::Vec3{-0.38F, 0.0F, -0.30F},
                        enemy.position + wkc::core::Vec3{0.38F, 1.80F, 0.30F},
                    });
                }
                for (const visual::PropRenderData& prop : visibleProps) {
                    if (prop.state == visual::PropVisualState::Destroyed) {
                        continue;
                    }
                    const wkc::core::Vec3 half{
                        prop.scale.x * 0.5F, 0.0F, prop.scale.z * 0.5F};
                    appendShadowOccluder({
                        prop.position - half,
                        prop.position + wkc::core::Vec3{
                            half.x, prop.scale.y, half.z},
                    });
                }
                (void)visual::optimizeShadowBudget(
                    lighting, player_.state().position, shadowCandidates);
                lighting.shadowLightCount = std::min<std::uint32_t>(
                    lighting.shadowLightCount,
                    graphicsQuality.maximumShadowLights);
                lighting.shadowOccluderCount = std::min<std::uint32_t>(
                    lighting.shadowOccluderCount,
                    graphicsQuality.maximumShadowOccluders);
            } else {
                lighting.shadowLightCount = 0U;
                lighting.shadowOccluderCount = 0U;
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
            const std::uint32_t geometryDepth
                = graphicsQuality.visibleRoomDepth;
            const std::uint32_t corridorAnchorDepth
                = geometryDepth > 0U ? geometryDepth - 1U : 0U;
            const std::vector<world::RoomId> geometryAnchors
                = roomsWithinGraphDistance(
                    *world_, playerRoom, corridorAnchorDepth);
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
                const visual::CorridorRenderData renderedCorridor{
                    std::span<const wkc::core::Vec3>{corridor.points},
                    corridor.width,
                    corridor.height,
                    roomMaterialKeys_.contains(corridor.firstRoom)
                        ? roomMaterialKeys_.at(corridor.firstRoom) : 0U,
                };
                const std::optional<wkc::core::Aabb> corridorBounds
                    = visual::corridorRenderBounds(renderedCorridor);
                if (corridorBounds
                    && !visual::isAabbPotentiallyVisible(
                        camera, *corridorBounds, cameraAspect)) {
                    continue;
                }
                visibleEdges.push_back(corridor.edge);
                visibleCorridors.push_back(renderedCorridor);
            }
            const std::vector<world::RoomId>& geometryRooms = visibleRoomIds;
            for (const world::RoomInstance& renderedRoom : world_->rooms) {
                if (renderedRoom.id == playerRoom
                    || std::find(geometryRooms.begin(), geometryRooms.end(), renderedRoom.id)
                        == geometryRooms.end()
                    || !visual::isAabbPotentiallyVisible(
                        camera, renderedRoom.worldBounds, cameraAspect)) {
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
                        return gameplay::shouldSealRoomDoors(runtime, entities_)
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
                        door == doorRuntimes_.end() ? 0.0F
                            : gameplay::doorPanelTravel(door->second.openness),
                    });
                }
            }
        }
    }
    if (session_.state().phase == gameplay::SessionPhase::Playing) {
        title << (session_.state().mode == gameplay::RunMode::TestMode
                ? " | OBJECTIVE: TEST ARENA"
                : session_.state().bossEngaged
                || renderedRoomRole == world::RoomRole::Boss
                ? " | OBJECTIVE: DEFEAT BOSS"
                : " | OBJECTIVE: REACH BOSS");
    }
    if (pauseMenu_.paused) {
        title << " | PAUSED";
    }
    SDL_SetWindowTitle(window_, localize(runtimeSettings_.language, title.str()).c_str());
    visual::HudRenderData hud;
    hud.language = runtimeSettings_.language;
    hud.visible = true;
    hud.flashlightOn = flashlightOn_ && player_.state().alive;
    hud.interactBinding = gameplay::bindingName(runtimeSettings_.controls.values[static_cast<std::size_t>(gameplay::ControlAction::Interact)]);
    hud.reloadBinding = gameplay::bindingName(runtimeSettings_.controls.values[static_cast<std::size_t>(gameplay::ControlAction::Reload)]);
    hud.flashlightBinding = gameplay::bindingName(runtimeSettings_.controls.values[static_cast<std::size_t>(gameplay::ControlAction::Flashlight)]);
    hud.infoOverlay = gameplay::controlDown(runtimeSettings_.controls, runtimeSettings_.gamepadControls, gameplay::ControlAction::Information, input_);
    if (world_) {
        HudMapTarget mapTarget;
        if (session_.state().mode == gameplay::RunMode::Campaign
            && session_.state().phase == gameplay::SessionPhase::Playing) {
            if (!campaignMission_.completed) {
                mapTarget = {gameplay::campaignMissionPendingRoom(campaignMission_),
                    visual::HudMapMarker::Mission};
            } else if (const auto* exitTerminal = entities_.find(exitTerminalEntity_)) {
                mapTarget = {exitTerminal->roomId,
                    session_.state().campaignExitReady
                        ? visual::HudMapMarker::Exit
                        : visual::HudMapMarker::Boss};
            }
        }
        hud.map = buildHudMap(*world_, visitedRooms_, playerRoom,
            player_.state().position, player_.planarForwardDirection(), mapTarget);
    }
    hud.runSeed = session_.state().currentSeed;
    hud.score = progress.score;
    hud.enemiesKilled = progress.enemiesKilled;
    hud.eventsCompleted = progress.eventsCompleted;
    hud.lootCollected = progress.lootCollected;
    hud.graphicsQuality = hudGraphicsQuality;
    hud.worldRenderScalePercent = graphicsQuality.renderScalePercent;
    hud.detailedMaterials = graphicsQuality.detailedMaterials;
    const PerformanceSnapshot performance = performanceMonitor_.snapshot();
    hud.performanceOverlay
        = performanceOverlayVisible_ && performance.sampleCount > 0U;
    hud.performanceFramesPerSecond = roundedPerformanceMetric(
        performance.estimatedFramesPerSecond);
    hud.averageFrameMilliseconds = roundedPerformanceMetric(
        performance.averageFrameMilliseconds);
    hud.percentile95FrameMilliseconds = roundedPerformanceMetric(
        performance.percentile95FrameMilliseconds);
    hud.worstFrameMilliseconds = roundedPerformanceMetric(
        performance.worstFrameMilliseconds);
    hud.averageUpdateMilliseconds = roundedPerformanceMetric(
        performance.averageUpdateMilliseconds);
    hud.averageRenderMilliseconds = roundedPerformanceMetric(
        performance.averageRenderMilliseconds);
    hud.averagePresentMilliseconds = roundedPerformanceMetric(
        performance.averagePresentMilliseconds);
    hud.averageGpuMilliseconds = roundedPerformanceMetric(
        performance.averageGpuMilliseconds);
    hud.gpuTimingAvailable = performance.gpuSampleCount > 0U;
    hud.framesOverBudgetPercent = roundedPerformanceMetric(
        performance.framesOverBudgetPercent);
    hud.worldGenerationMilliseconds = roundedPerformanceMetric(
        performance.worldGenerationMilliseconds);
    hud.assetPreparationMilliseconds = roundedPerformanceMetric(
        performance.assetPreparationMilliseconds);
    hud.performanceDrawCalls = performance.latest.drawCalls;
    hud.performanceTriangles = performance.latest.triangles;
    hud.performanceVisibleEnemies = performance.latest.visibleEnemies;
    hud.performanceVisibleProps = performance.latest.visibleProps;
    hud.performanceVisibleRooms = performance.latest.visibleRooms;
    hud.performanceActiveLights = performance.latest.activeLights;
    hud.performanceShadowLights = performance.latest.shadowLights;
    hud.performanceShadowOccluders = performance.latest.shadowOccluders;
    hud.performanceMeshCacheEntries = performance.latest.meshCacheEntries;
    hud.performanceTextureCacheEntries = performance.latest.textureCacheEntries;
    hud.performanceUniformUploads = performance.latest.uniformUploads;
    hud.performanceUniformSkips = performance.latest.uniformSkips;
    hud.performanceTextureUnitActivations
        = performance.latest.textureUnitActivations;
    hud.performanceTextureBinds = performance.latest.textureBinds;
    hud.performanceTextureBindSkips = performance.latest.textureBindSkips;
    hud.performanceMeshBinds = performance.latest.meshBinds;
    hud.performanceMeshBindSkips = performance.latest.meshBindSkips;
    hud.performanceLowSpecShader = performance.latest.lowSpecShader;
    hud.performanceBackfaceCulledDraws
        = performance.latest.backfaceCulledDraws;
    hud.performanceCullStateChanges
        = performance.latest.cullStateChanges;
    hud.performanceInstancedDrawCalls
        = performance.latest.instancedDrawCalls;
    hud.performanceRenderedInstances
        = performance.latest.renderedInstances;
    hud.performanceAvoidedDrawCalls
        = performance.latest.avoidedDrawCalls;
    hud.performanceShadowShaderOccluderCapacity
        = performance.latest.shadowShaderOccluderCapacity;
    hud.health = player_.state().health;
    hud.maximumHealth = player_.state().maximumHealth;
    hud.armor = player_.state().armor;
    hud.maximumArmor = player_.state().maximumArmor;
    hud.magazineAmmo = weapon_.magazineAmmo();
    hud.reserveAmmo = weapon_.reserveAmmo();
    hud.grenades = player_.state().grenades;
    hud.grenadeSelected = grenadeSelected_;
    hud.grenadePriming = grenadeCook_.active();
    hud.grenadeChargeStrength = grenadeCook_.active()
        ? grenadeCook_.windupProgress() : grenadeThrowChargeStrength_;
    hud.grenadeFuseRemainingSeconds = grenadeCook_.active()
        && grenadeCook_.pinProgress() >= 1.0F
        ? grenadeCook_.remainingFuseSeconds() : 0.0F;
    switch (weapon_.definition().archetype) {
    case gameplay::WeaponArchetype::Carbine:
        hud.weapon = visual::HudWeaponRole::Carbine;
        break;
    case gameplay::WeaponArchetype::Knife:
        hud.weapon = visual::HudWeaponRole::Knife;
        break;
    case gameplay::WeaponArchetype::Pistol:
        hud.weapon = visual::HudWeaponRole::Pistol;
        break;
    case gameplay::WeaponArchetype::Sniper:
        hud.weapon = visual::HudWeaponRole::Sniper;
        break;
    case gameplay::WeaponArchetype::Shotgun:
        hud.weapon = visual::HudWeaponRole::Shotgun;
        break;
    case gameplay::WeaponArchetype::SubmachineGun:
        hud.weapon = visual::HudWeaponRole::SubmachineGun;
        break;
    case gameplay::WeaponArchetype::Count:
        break;
    }
    hud.weaponOverdrive = weapon_.overdriveActive();
    hud.weaponSpread = std::clamp(weapon_.spreadStrength()
        + gameplay::weaponMovementInaccuracyDegrees(
            weapon_.definition().archetype, player_.state().velocity,
            player_.state().grounded, player_.state().crouching, aimBlend_) / 1.5F,
        0.0F, 1.0F);
    hud.keys = player_.state().keys;
    hud.level = progress.level;
    hud.runTimerVisible = true;
    hud.elapsedRunTicks = session_.state().elapsedRunTicks;
    hud.campaignMode = session_.state().mode == gameplay::RunMode::Campaign;
    hud.campaignFloor = session_.state().campaignFloor;
    hud.campaignFloorCount = gameplay::campaignFloorCount;
    if (hud.campaignMode) {
        hud.campaignMission = campaignMissionObjective(campaignMission_);
        hud.campaignRouteMission = campaignMission_.kind
            == gameplay::CampaignMissionKind::RecoverAndSecure;
        hud.campaignKeyRecovered = campaignMission_.keyRecovered;
        hud.campaignArenaSecured = campaignMission_.arenaSecured;
        hud.campaignMissionCompleted = campaignMission_.completed;
        const auto boss = gameplay::bossStatus(entities_, 0U);
        hud.campaignBossDefeated = boss.exists && !boss.alive;
        hud.campaignExitReady = session_.state().campaignExitReady;
    }
    hud.campaignThreatPercent = static_cast<std::uint16_t>(
        std::lround(runDifficulty_ * 100.0F));
    hud.campaignAdvanceAvailable
        = gameplay::canAdvanceCampaign(session_.state());
    hud.speedrunMode = session_.state().mode == gameplay::RunMode::Speedrun;
    hud.speedrunRecordVisible = hud.speedrunMode && speedrunRecord_.has_value();
    hud.bestSpeedrunTicks
        = speedrunRecord_ ? speedrunRecord_->bestTicks : 0U;
    hud.newSpeedrunRecord
        = gameplay::speedrunRecordChanged(latestSpeedrunRecordResult_);
    hud.experience = progress.experienceIntoLevel;
    hud.experienceForNextLevel = nextLevelXp;
    hud.damageFeedback = gameplay::damageFeedbackStrength(feedback_);
    hud.hitFeedbackStrength = gameplay::hitFeedbackStrength(feedback_);
    if (feedback_.hit == gameplay::HitFeedbackKind::Hit) {
        hud.hitFeedback = visual::HudHitFeedback::Hit;
    } else if (feedback_.hit == gameplay::HitFeedbackKind::Kill) {
        hud.hitFeedback = visual::HudHitFeedback::Kill;
    } else if (feedback_.hit == gameplay::HitFeedbackKind::WeakHit) {
        hud.hitFeedback = visual::HudHitFeedback::WeakHit;
    } else if (feedback_.hit == gameplay::HitFeedbackKind::WeakKill) {
        hud.hitFeedback = visual::HudHitFeedback::WeakKill;
    }
    hud.noticeFeedbackStrength = gameplay::noticeFeedbackStrength(feedback_);
    hud.paused = pauseMenu_.paused;
    hud.exitConfirmation = exitConfirmation_.active;
    if (exitConfirmation_.selection
        == gameplay::ExitConfirmationOption::ExitGame) {
        hud.exitConfirmationSelection
            = visual::HudExitConfirmationSelection::ExitGame;
    }
    if (pauseMenu_.selection == gameplay::PauseMenuOption::RestartRun) {
        hud.pauseSelection = visual::HudPauseSelection::RestartRun;
    } else if (pauseMenu_.selection == gameplay::PauseMenuOption::Settings) {
        hud.pauseSelection = visual::HudPauseSelection::Settings;
    } else if (pauseMenu_.selection == gameplay::PauseMenuOption::Controls) {
        hud.pauseSelection = visual::HudPauseSelection::Controls;
    } else if (pauseMenu_.selection == gameplay::PauseMenuOption::MainMenu) {
        hud.pauseSelection = visual::HudPauseSelection::MainMenu;
    } else if (pauseMenu_.selection == gameplay::PauseMenuOption::ExitGame) {
        hud.pauseSelection = visual::HudPauseSelection::ExitGame;
    }
    if (resultMenu_.selection == gameplay::ResultMenuOption::ReplaySeed) {
        hud.resultMenuSelection = visual::HudResultMenuSelection::ReplaySeed;
    } else if (resultMenu_.selection == gameplay::ResultMenuOption::MainMenu) {
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
    case gameplay::NoticeFeedbackKind::WeaponDropped:
        hud.notice = visual::HudNotice::WeaponDropped;
        break;
    case gameplay::NoticeFeedbackKind::WeaponDropBlocked:
        hud.notice = visual::HudNotice::WeaponDropBlocked;
        break;
    case gameplay::NoticeFeedbackKind::None:
        break;
    }
    if (session_.state().phase == gameplay::SessionPhase::Victory) {
        hud.objective = visual::ObjectiveRenderState::Victory;
    } else if (session_.state().phase == gameplay::SessionPhase::Defeat) {
        hud.objective = visual::ObjectiveRenderState::Defeat;
    } else if (session_.state().mode == gameplay::RunMode::TestMode) {
        hud.objective = visual::ObjectiveRenderState::TestArena;
    } else if (session_.state().campaignExitReady) {
        hud.objective = visual::ObjectiveRenderState::ActivateExit;
    } else if (session_.state().mode == gameplay::RunMode::Campaign
        && !campaignMission_.completed
        && (!gameplay::bossEncounterAlive(entities_, bossRoomId_)
            || (!session_.state().bossEngaged
                && renderedRoomRole != world::RoomRole::Boss))) {
        hud.objective = campaignMissionObjective(campaignMission_);
    } else if (session_.state().bossEngaged
        || renderedRoomRole == world::RoomRole::Boss) {
        hud.objective = gameplay::bossStatus(entities_, 0U).alive
            ? visual::ObjectiveRenderState::DefeatBoss
            : visual::ObjectiveRenderState::ClearBossEncounter;
    } else {
        hud.objective = visual::ObjectiveRenderState::ReachBoss;
    }
    hud.boss = buildBossHud(entities_, session_.state());
    if (activeEvent != roomEvents_.end()) {
        if (activeEvent->phase == gameplay::RoomEventPhase::Dormant) {
            hud.event = visual::HudEventState::Dormant;
        } else if (activeEvent->phase == gameplay::RoomEventPhase::Active) {
            hud.event = visual::HudEventState::Active;
        } else if (activeEvent->plan.type != world::RoomEventType::None) {
            hud.event = visual::HudEventState::Completed;
        }
        hud.eventLocked = gameplay::shouldSealRoomDoors(*activeEvent, entities_);
    }
    if (session_.state().phase == gameplay::SessionPhase::Playing && world_) {
        if (isCampaignExitTargeted()) {
            hud.prompt = !gameplay::bossEncounterAlive(entities_, bossRoomId_)
                    && !campaignMission_.completed
                ? visual::HudPrompt::CampaignMissionRequired
                : !session_.state().campaignExitReady
                    ? visual::HudPrompt::ExitTerminalLocked
                : session_.state().campaignFloor < gameplay::campaignFloorCount
                    ? visual::HudPrompt::NextFloorTerminal
                    : visual::HudPrompt::FinishCampaignTerminal;
        }
        if (hud.prompt == visual::HudPrompt::None
            && session_.state().mode == gameplay::RunMode::TestMode) {
            const entities::Entity* terminal = entities_.find(terminalEntity_);
            if (terminal != nullptr && terminal->roomId == playerRoom) {
                const wkc::core::Vec3 position = terminal->transform.position;
                const std::array targets{gameplay::InteractionTarget{
                    terminalEntity_,
                    {{position.x - 0.4F, position.y, position.z - 0.15F},
                        {position.x + 0.4F, position.y + 2.0F,
                            position.z + 0.15F}}}};
                if (player_.findInteraction(targets) == terminalEntity_) {
                    hud.prompt = visual::HudPrompt::Restock;
                }
            }
        }
        const std::optional<gameplay::RoomTransition> transition
            = gameplay::findRoomTransition(
                *world_, playerRoom, player_.state().position,
                player_.planarForwardDirection());
        if (transition && hud.prompt == visual::HudPrompt::None) {
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
        if (hud.prompt == visual::HudPrompt::None && !grenadeSelected_
            && !weapon_.reloading() && !weapon_.switching()
            && weapon_.magazineAmmo() == 0U
            && weapon_.reserveAmmo() > 0U) {
            hud.prompt = visual::HudPrompt::Reload;
        }
    }
    std::vector<visual::ImpactRenderData> visibleImpacts;
    visibleImpacts.reserve(activeImpacts_.size());
    for (const ActiveImpact& impact : activeImpacts_) {
        if (impact.roomId == playerRoom
            && sphereIsVisible(impact.render.position, 0.25F)) {
            visibleImpacts.push_back(impact.render);
        }
    }
    std::vector<visual::EnemyProjectileRenderData> visibleEnemyProjectiles;
    for (const gameplay::EnemyProjectile& projectile
         : enemyProjectiles_.projectiles()) {
        if (sphereIsVisible(projectile.position, 0.4F)) {
            visibleEnemyProjectiles.push_back({projectile.position,
                projectile.velocity});
        }
    }
    std::vector<visual::GrenadeRenderData> visibleGrenades;
    for (const gameplay::PlayerGrenade& grenade : playerGrenades_.grenades()) {
        if (sphereIsVisible(grenade.position, 0.25F))
            visibleGrenades.push_back({grenade.position});
    }
    std::vector<visual::GrenadeBlastRenderData> visibleGrenadeBlasts;
    for (const ActiveGrenadeBlast& blast : activeGrenadeBlasts_) {
        if (sphereIsVisible(blast.position, gameplay::grenadeBlastRadius))
            visibleGrenadeBlasts.push_back({blast.position,
                blast.remaining / grenadeBlastVisualDuration});
    }
    const visual::WeaponViewmodelRenderData weaponViewmodel{
        .visible = player_.state().alive
            && session_.state().phase == gameplay::SessionPhase::Playing,
        .grenadeSelected = grenadeSelected_ && weaponMeleeRemaining_ <= 0.0F,
        .grenadePriming = hud.grenadePriming,
        .grenadePinProgress = grenadeCook_.active()
            ? grenadeCook_.pinProgress() : 0.0F,
        .grenadeChargeStrength = hud.grenadeChargeStrength,
        .grenadeThrowStrength = grenadeThrowRemaining_ > 0.0F
            ? 1.0F - grenadeThrowRemaining_ / 0.28F : 0.0F,
        .grenadeVisible = grenadeThrowRemaining_ <= 0.0F,
        // SMG retains its animation identity even while sharing Carbine geometry.
        .role = grenadeSelected_ || weaponMeleeRemaining_ > 0.0F
            ? visual::HudWeaponRole::Knife : hud.weapon,
        .recoilStrength = weaponRecoilRemaining_ / weaponRecoilDuration,
        .reloadProgress = grenadeSelected_ ? 0.0F : weapon_.reloadProgress(),
        .equipStrength = std::max(
            weaponEquipRemaining_ / weaponEquipDuration,
            weapon_.switchStrength()),
        .meleeStrength = weaponMeleeRemaining_ / weaponMeleeDuration,
        .crouchStrength = player_.state().crouchAmount,
        .airborneStrength = player_.state().grounded ? 0.0F
            : std::clamp(
                0.35F + (std::abs(player_.state().velocity.y) / 9.0F),
                0.0F, 1.0F),
        .muzzleFlashStrength
            = weaponMuzzleFlashRemaining_ / weaponMuzzleFlashDuration,
        .overdrive = weapon_.overdriveActive(),
        .visualSeed = wkc::core::hashCombine(
            config_.worldSeed,
            hud.weapon == visual::HudWeaponRole::Carbine
                ? 0x43415242494e45ULL
                : hud.weapon == visual::HudWeaponRole::Knife
                    ? 0x4b4e494645ULL : 0x504953544f4cULL),
        .reloadRoundCount = weapon_.reloadRoundCount(),
        .aimStrength = !grenadeSelected_ && !pauseMenu_.paused && !controlsMenuActive_
                && !weapon_.reloading() && !weapon_.switching()
            ? aimBlend_ : 0.0F,
    };
    currentFramePerformance_.visibleEnemies = static_cast<std::uint32_t>(
        visibleEnemies.size());
    currentFramePerformance_.visibleProps = static_cast<std::uint32_t>(
        visibleProps.size());
    currentFramePerformance_.visibleRooms = static_cast<std::uint32_t>(
        connectedRooms.size() + (world_ ? 1U : 0U));
    currentFramePerformance_.activeLights = lighting.lightCount;
    currentFramePerformance_.shadowLights = lighting.shadowLightCount;
    currentFramePerformance_.shadowOccluders = lighting.shadowOccluderCount;
    if (!renderAndPresent([&] {
            return renderer_->render(
                width, height, camera, visibleEnemies, visiblePickups, visibleProps,
                lighting, roomRender, visibleDoors, visibleCorridors, connectedRooms,
                hud, visibleImpacts, weaponViewmodel, visibleEnemyProjectiles,
                {.visible = player_.state().alive && session_.state().phase == gameplay::SessionPhase::Playing,
                    .feetPosition = player_.state().position,
                    .yawDegrees = player_.state().yawDegrees,
                    .crouchAmount = player_.state().crouchAmount,
                    .gaitPhase = playerBodyMotion_.phaseRadians,
                    .gaitWeight = playerBodyMotion_.weight,
                    .forwardMovement = playerBodyMotion_.forward,
                    .rightMovement = playerBodyMotion_.right,
                    .airborneStrength = playerBodyMotion_.airborne},
                visibleGrenades, visibleGrenadeBlasts);
        }, "Renderer failed while drawing the frame")) {
        return;
    }
}

void Application::shutdown()
{
    running_ = false;
    runDifficulty_ = 1.0F;

    (void)savePersistedSettings();

    renderer_.reset();
    audio_.reset();
    entities_.clear();
    roomEvents_.clear();
    roomLighting_.clear();
    roomFixtureEntities_.clear();
    roomMaterialKeys_.clear();
    doorRuntimes_.clear();
    doorAudioEventCounters_.clear();
    activeImpacts_.clear();
    activeGrenadeBlasts_.clear();
    playerGrenades_.clear();
    grenadeSelected_ = false;
    grenadeCook_.reset();
    grenadeThrowRemaining_ = 0.0F;
    grenadeThrowChargeStrength_ = 0.0F;
    enemyProjectiles_.clear();
    gamepad_.close(input_);
    gamepadConnected_ = false;
    input_ = Input{};
    player_ = gameplay::Player{};
    weapon_ = gameplay::Weapon{};
    progression_ = gameplay::ProgressionSystem{};
    gameplay::clearFeedback(feedback_);
    gameplay::resetFootsteps(footsteps_);
    gameplay::resetViewMotion(viewMotion_);
    playerBodyMotion_ = {};
    gameplay::openMainMenu(mainMenu_);
    gameplay::closePauseMenu(pauseMenu_);
    controlsMenuActive_ = false;
    gameplay::closeResultMenu(resultMenu_);
    gameplay::closeSettingsMenu(settingsMenu_);
    baseCollisionWorld_ = gameplay::CollisionWorld::fixedRoom();
    collisionWorld_ = baseCollisionWorld_;
    playerCommand_ = {};
    interactionPending_ = false;
    jumpPending_ = false;
    firePending_ = false;
    quickMeleePending_ = false;
    fireHeld_ = false;
    reloadPending_ = false;
    weaponSwitchPending_.reset();
    dropPending_ = false;
    weaponRecoilRemaining_ = 0.0F;
    aimBlend_ = 0.0F;
    flashlightOn_ = false;
    weaponMeleeRemaining_ = 0.0F;
    weaponMuzzleFlashRemaining_ = 0.0F;
    weaponEquipRemaining_ = 0.0F;
    audioEventCounter_ = 0U;
    uiAudioEventCounter_ = 0U;
    impactVisualCounter_ = 0U;
    terminalEntity_ = {};
    exitTerminalEntity_ = {};
    bossRoomId_ = 0U;
    world_.reset();
    visitedRooms_.clear();
    roomEntryGraceRemaining_.clear();

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
