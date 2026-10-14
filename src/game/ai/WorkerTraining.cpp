#include "game/ai/WorkerTraining.hpp"
#include "game/ai/TrainingPopulationStore.hpp"
#include "game/ai/TrainingEncounter.hpp"

#include "core/Random.hpp"
#include "entities/EntityManager.hpp"
#include "gameplay/Combat.hpp"
#include "gameplay/Enemy.hpp"
#include "gameplay/EnemySensors.hpp"
#include "gameplay/Player.hpp"
#include "gameplay/RoomTraversal.hpp"
#include "persistence/WorkerBrainStore.hpp"
#include "world/WorldGenerator.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <limits>
#include <optional>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace gdp::game::ai {
namespace {

constexpr std::uint64_t initialBrainDomain = 0x4149425241494E01ULL;
constexpr std::uint64_t evolutionDomain = 0x414945564F4C5645ULL;
constexpr std::uint64_t trainingScenarioDomain = 0x41495343454E4554ULL;
constexpr std::uint64_t holdoutScenarioDomain = 0x41495343454E4548ULL;
constexpr std::uint64_t cooperativeWorldRetryDomain = 0x4149574F524C4432ULL;
constexpr float fixedStep = 1.0F / 60.0F;
constexpr int episodeSteps = 480;
constexpr int bossEpisodeSteps = 720;
constexpr std::size_t scenarioCount = 7U;
constexpr std::size_t holdoutScenarioCount = 7U;
constexpr std::size_t scenarioRepetitions = 2U;
constexpr std::size_t maximumScenarioRepetitions = scenarioRepetitions * 2U;

using gdp::entities::EnemyArchetype;

const char* archetypeName(const EnemyArchetype archetype) noexcept
{
    switch (archetype) {
    case EnemyArchetype::Worker: return "worker";
    case EnemyArchetype::Guard: return "guard";
    case EnemyArchetype::Officer: return "officer";
    case EnemyArchetype::Engineer: return "engineer";
    case EnemyArchetype::IndustrialRobot: return "robot";
    }
    return "invalid";
}

struct TrainingProfile {
    float spawnMin;
    float spawnMax;
    float health;
    float preferredMinimum;
    float preferredMaximum;
    float playerAttackRange;
};

enum class PlayerPattern : std::uint8_t {
    Sweep,
    Advance,
    Retreat,
    Zigzag,
    Peek,
    Bhop,
};

enum class CoverPattern : std::uint8_t {
    Open,
    Left,
    Right,
    Center,
};

struct TrainingScenario {
    PlayerPattern player;
    CoverPattern cover;
    float facingOffsetDegrees;
    float initialHealthRatio;
    float playerShotInterval;
};

// The last two cases are holdout-only in the default curriculum. The broad
// curriculum trains all seven and evaluates them again with independent seeds.
// In particular, the boss starts in all three health phases.
constexpr std::array<TrainingScenario, 7> workerScenarios{{
    {PlayerPattern::Sweep, CoverPattern::Open, 0.0F, 1.0F, 1.4F},
    {PlayerPattern::Retreat, CoverPattern::Center, -30.0F, 1.0F, 1.4F},
    {PlayerPattern::Advance, CoverPattern::Left, 40.0F, 1.0F, 1.2F},
    {PlayerPattern::Zigzag, CoverPattern::Right, -45.0F, 1.0F, 1.4F},
    {PlayerPattern::Bhop, CoverPattern::Open, 20.0F, 1.0F, 1.4F},
    {PlayerPattern::Peek, CoverPattern::Center, 25.0F, 1.0F, 1.1F},
    {PlayerPattern::Retreat, CoverPattern::Right, -55.0F, 1.0F, 1.1F},
}};
constexpr std::array<TrainingScenario, 7> guardScenarios{{
    {PlayerPattern::Sweep, CoverPattern::Open, 0.0F, 1.0F, 1.4F},
    {PlayerPattern::Advance, CoverPattern::Left, 30.0F, 1.0F, 1.2F},
    {PlayerPattern::Zigzag, CoverPattern::Right, -35.0F, 1.0F, 1.4F},
    {PlayerPattern::Peek, CoverPattern::Center, 45.0F, 1.0F, 1.4F},
    {PlayerPattern::Bhop, CoverPattern::Open, -20.0F, 1.0F, 1.4F},
    {PlayerPattern::Retreat, CoverPattern::Center, -50.0F, 1.0F, 1.1F},
    {PlayerPattern::Advance, CoverPattern::Right, 55.0F, 1.0F, 1.1F},
}};
constexpr std::array<TrainingScenario, 7> officerScenarios{{
    {PlayerPattern::Sweep, CoverPattern::Open, 0.0F, 1.0F, 1.4F},
    {PlayerPattern::Advance, CoverPattern::Center, 35.0F, 1.0F, 1.2F},
    {PlayerPattern::Retreat, CoverPattern::Left, -35.0F, 1.0F, 1.4F},
    {PlayerPattern::Peek, CoverPattern::Right, 45.0F, 1.0F, 1.4F},
    {PlayerPattern::Bhop, CoverPattern::Open, 20.0F, 1.0F, 1.4F},
    {PlayerPattern::Zigzag, CoverPattern::Center, -50.0F, 1.0F, 1.1F},
    {PlayerPattern::Advance, CoverPattern::Left, 55.0F, 1.0F, 1.1F},
}};
constexpr std::array<TrainingScenario, 7> engineerScenarios{{
    {PlayerPattern::Sweep, CoverPattern::Open, 0.0F, 1.0F, 1.4F},
    {PlayerPattern::Retreat, CoverPattern::Right, -30.0F, 1.0F, 1.4F},
    {PlayerPattern::Advance, CoverPattern::Center, 40.0F, 1.0F, 1.2F},
    {PlayerPattern::Peek, CoverPattern::Left, -45.0F, 1.0F, 1.4F},
    {PlayerPattern::Bhop, CoverPattern::Open, 20.0F, 1.0F, 1.4F},
    {PlayerPattern::Zigzag, CoverPattern::Right, 50.0F, 1.0F, 1.1F},
    {PlayerPattern::Advance, CoverPattern::Center, -55.0F, 1.0F, 1.1F},
}};
constexpr std::array<TrainingScenario, 7> bossScenarios{{
    {PlayerPattern::Sweep, CoverPattern::Open, 0.0F, 1.0F, 1.4F},
    {PlayerPattern::Advance, CoverPattern::Left, 35.0F, 0.58F, 1.2F},
    {PlayerPattern::Zigzag, CoverPattern::Right, -35.0F, 0.28F, 1.4F},
    {PlayerPattern::Peek, CoverPattern::Center, 45.0F, 0.70F, 1.1F},
    // The duel evaluator starts the boss healthy. Keep this Bhop exercise in
    // its long-range phase; RealMaps still tests Bhop during pressure phase.
    {PlayerPattern::Bhop, CoverPattern::Open, -20.0F, 1.0F, 1.4F},
    {PlayerPattern::Retreat, CoverPattern::Center, -50.0F, 0.52F, 1.1F},
    {PlayerPattern::Advance, CoverPattern::Right, 55.0F, 0.52F, 1.1F},
}};

const TrainingScenario& scenarioFor(
    const EnemyArchetype archetype, const std::size_t index) noexcept
{
    switch (archetype) {
    case EnemyArchetype::Worker: return workerScenarios[index];
    case EnemyArchetype::Guard: return guardScenarios[index];
    case EnemyArchetype::Officer: return officerScenarios[index];
    case EnemyArchetype::Engineer: return engineerScenarios[index];
    case EnemyArchetype::IndustrialRobot: return bossScenarios[index];
    }
    return workerScenarios[index];
}

TrainingProfile profileFor(const EnemyArchetype archetype,
    const gameplay::EnemyBrainContract contract) noexcept
{
    switch (archetype) {
    case EnemyArchetype::Worker: return {3.4F, 4.6F, 50.0F, 0.0F, 2.0F, 5.0F};
    case EnemyArchetype::Guard: return {6.0F, 8.0F, 75.0F, 4.0F, 8.0F, 10.0F};
    case EnemyArchetype::Officer: return {7.0F, 9.0F, 70.0F, 6.0F, 10.0F, 11.0F};
    case EnemyArchetype::Engineer: return {5.0F, 7.0F, 70.0F, 3.0F, 6.0F, 10.0F};
    case EnemyArchetype::IndustrialRobot:
        return {9.0F, 11.0F,
            contract == gameplay::EnemyBrainContract::Cooperative ? 600.0F : 220.0F,
            4.0F, 13.0F,
            contract == gameplay::EnemyBrainContract::Cooperative ? 32.0F : 14.0F};
    }
    return {};
}

float rangeError(const float distance, const TrainingProfile profile) noexcept
{
    if (distance < profile.preferredMinimum) {
        return profile.preferredMinimum - distance;
    }
    return std::max(0.0F, distance - profile.preferredMaximum);
}

TrainingProfile phaseProfile(
    const EnemyArchetype archetype, TrainingProfile profile,
    const float healthRatio) noexcept
{
    if (archetype != EnemyArchetype::IndustrialRobot) {
        return profile;
    }
    if (healthRatio > 0.66F) {
        profile.preferredMinimum = 8.0F;
        profile.preferredMaximum = 13.0F;
    } else if (healthRatio > 0.33F) {
        profile.preferredMinimum = 4.0F;
        profile.preferredMaximum = 8.0F;
    } else {
        profile.preferredMinimum = 0.0F;
        profile.preferredMaximum = 2.25F;
    }
    return profile;
}

std::vector<wkc::core::Aabb> coverFor(const CoverPattern pattern)
{
    switch (pattern) {
    case CoverPattern::Open: return {};
    case CoverPattern::Left:
        return {{{-1.5F, 0.0F, 1.4F}, {-0.45F, 2.4F, 2.7F}}};
    case CoverPattern::Right:
        return {{{0.45F, 0.0F, 1.4F}, {1.5F, 2.4F, 2.7F}}};
    case CoverPattern::Center:
        return {{{-0.65F, 0.0F, 1.1F}, {0.65F, 2.4F, 2.35F}}};
    }
    return {};
}

wkc::core::Vec3 playerPosition(
    const PlayerPattern pattern, const float time, const float direction,
    const float sweep, const float phase) noexcept
{
    const float sway = direction * sweep * std::sin(time * 0.7F + phase);
    switch (pattern) {
    case PlayerPattern::Sweep: return {sway, 0.0F, 0.0F};
    case PlayerPattern::Advance:
        return {sway * 0.7F, 0.0F, std::min(time * 0.65F, 2.4F)};
    case PlayerPattern::Retreat:
        return {sway * 0.7F, 0.0F, -std::min(time * 0.55F, 4.0F)};
    case PlayerPattern::Zigzag:
        return {direction * (0.9F * std::sin(time * 1.8F + phase)
                + 0.4F * std::sin(time * 3.1F)),
            0.0F, -std::min(time * 0.15F, 1.5F)};
    case PlayerPattern::Peek:
        return {direction * (1.1F + sweep * 0.65F
                * std::sin(time * 0.9F + phase)), 0.0F, 0.3F};
    case PlayerPattern::Bhop: return {};
    }
    return {};
}

wkc::core::Vec3 aimStagePlayerPosition(
    const float time, const float direction) noexcept
{
    if (time < 1.0F) {
        return {};
    }
    if (time < 2.0F) {
        return {direction * (time - 1.0F) * 0.5F, 0.0F, 0.0F};
    }
    if (time < 3.0F) {
        return {direction * (0.5F + (time - 2.0F) * 1.0F), 0.0F, 0.0F};
    }
    if (time < 4.0F) {
        return {direction * (1.5F - (time - 3.0F) * 2.0F), 0.0F, 0.0F};
    }
    if (time < 5.0F) {
        return {direction * (-0.5F + (time - 4.0F) * 1.4F), 0.0F, 0.0F};
    }
    if (time < 6.0F) {
        return {direction * (0.9F + (time - 5.0F) * 0.9F), 0.0F, 0.0F};
    }
    return {direction * (1.8F - (time - 6.0F) * 0.7F), 0.0F,
        (time - 6.0F) * 0.9F};
}

gameplay::EnemyUpdateSummary updateTrainingEnemy(
    const EnemyArchetype archetype,
    entities::EntityManager& manager,
    gameplay::Player& player,
    const std::uint32_t roomId,
    const gameplay::CollisionWorld& arena,
    gameplay::NeuralEnemyExperiment* brain,
    std::vector<gameplay::EnemyProjectile>& shots,
    const TrainingStage stage,
    const bool roomEntryGraceElapsed)
{
    const gameplay::EnemyPerception perception{.projectiles = &shots,
        .allowAttacks = stage >= TrainingStage::FireControl
            && roomEntryGraceElapsed};
    switch (archetype) {
    case EnemyArchetype::Worker:
        return gameplay::updateWorkers(manager, player, roomId, arena,
            fixedStep, {}, perception, brain);
    case EnemyArchetype::Guard:
        return gameplay::updateGuards(manager, player, roomId, arena,
            fixedStep, {}, perception, brain);
    case EnemyArchetype::Officer:
        return gameplay::updateOfficers(manager, player, roomId, arena,
            fixedStep, {}, perception, brain);
    case EnemyArchetype::Engineer:
        return gameplay::updateEngineers(manager, player, roomId, arena,
            fixedStep, {}, perception, brain);
    case EnemyArchetype::IndustrialRobot:
        return gameplay::updateIndustrialRobots(manager, player, roomId, arena,
            fixedStep, {}, perception, brain);
    }
    return {};
}

float planarDistance(
    const wkc::core::Vec3 first,
    const wkc::core::Vec3 second) noexcept
{
    const float x = first.x - second.x;
    const float z = first.z - second.z;
    return std::sqrt(x * x + z * z);
}

float facingAlignment(
    const wkc::core::Vec3 position,
    const float yawDegrees,
    const wkc::core::Vec3 target) noexcept
{
    constexpr float radiansPerDegree = 0.01745329251994329577F;
    const float distance = planarDistance(position, target);
    if (distance <= 1.0e-5F) {
        return 1.0F;
    }
    const float yaw = yawDegrees * radiansPerDegree;
    return std::sin(yaw) * (target.x - position.x) / distance
        + std::cos(yaw) * (target.z - position.z) / distance;
}

struct IncomingTrainingShot {
    wkc::core::Vec3 position{};
    wkc::core::Vec3 velocity{};
    wkc::core::Vec3 originalTarget{};
    float targetDistance{0.0F};
    float traveledDistance{0.0F};
};

float shotLateralDisplacement(const IncomingTrainingShot& shot,
    const wkc::core::Vec3 enemyFeet) noexcept
{
    const float planarSpeed = std::hypot(shot.velocity.x, shot.velocity.z);
    if (planarSpeed <= 1.0e-5F) {
        return 0.0F;
    }
    const wkc::core::Vec3 offset = enemyFeet - shot.originalTarget;
    return std::abs(shot.velocity.x * offset.z
        - shot.velocity.z * offset.x) / planarSpeed;
}

float trainingShotSpeed(const EnemyArchetype archetype) noexcept
{
    // The Worker and Robot cannot move beyond their collision radii before a
    // 13 m/s shot reaches them at the Dodge-stage spawn distances. Give this
    // synthetic anticipation exercise a reachable, role-specific window.
    return archetype == EnemyArchetype::Worker
        || archetype == EnemyArchetype::IndustrialRobot ? 6.0F : 13.0F;
}

void aimTrainingPlayerAt(gameplay::Player& player,
    const wkc::core::Vec3 target) noexcept
{
    constexpr float degreesPerRadian = 57.29577951308232F;
    const wkc::core::Vec3 delta = target - player.cameraPosition();
    const float desiredYaw = std::atan2(delta.z, delta.x) * degreesPerRadian;
    const float yawDelta = std::remainder(
        desiredYaw - player.state().yawDegrees, 360.0F);
    player.applyLook({yawDelta / player.mouseSensitivity(), 0.0F});
}

struct IncomingShotResult {
    bool finished{false};
    bool hit{false};
    bool dodged{false};
};

IncomingShotResult advanceIncomingShot(IncomingTrainingShot& shot,
    const wkc::core::Vec3 enemyFeet, const float enemyRadius,
    const gameplay::CollisionWorld& arena) noexcept
{
    const wkc::core::Vec3 step = shot.velocity * fixedStep;
    const int samples = std::max(1, static_cast<int>(
        std::ceil(wkc::core::length(step) / 0.06F)));
    const wkc::core::Vec3 delta = step * (1.0F / static_cast<float>(samples));
    for (int sample = 0; sample < samples; ++sample) {
        const wkc::core::Vec3 next = shot.position + delta;
        if (!gameplay::hasLineOfSight(shot.position, next, arena)) {
            return {true, false, false};
        }
        shot.position = next;
        shot.traveledDistance += wkc::core::length(delta);
        if (std::hypot(next.x - enemyFeet.x, next.z - enemyFeet.z)
                <= enemyRadius + 0.10F
            && next.y >= enemyFeet.y + 0.10F
            && next.y <= enemyFeet.y + 1.8F) {
            return {true, true, false};
        }
        if (shot.traveledDistance >= shot.targetDistance + 0.5F) {
            return {true, false, true};
        }
    }
    return {};
}

WorkerFitnessMetrics simulateEpisode(
    const wkc::ai::Genome& genome,
    const wkc::core::Seed seed,
    const std::size_t scenario,
    const EnemyArchetype archetype,
    const bool diverseMaps = false,
    const unsigned mapVariant = 0U,
    const unsigned bossPhase = 0U,
    const gameplay::EnemyBrainContract contract
        = gameplay::EnemyBrainContract::Tactical)
{
    const TrainingProfile profile = profileFor(archetype, contract);
    const bool cooperative = contract == gameplay::EnemyBrainContract::Cooperative;
    const TrainingStage stage = static_cast<TrainingStage>(scenario + 1U);
    TrainingScenario plan = scenarioFor(archetype, scenario);
    if (diverseMaps && stage == TrainingStage::RealMaps
        && archetype == EnemyArchetype::IndustrialRobot) {
        plan.initialHealthRatio = bossPhase == 0U ? 1.0F : bossPhase == 1U ? 0.58F : 0.28F;
    }
    wkc::core::RandomStream random{seed};
    const float workerX = random.rangeFloat(-0.8F, 0.8F);
    const float workerZ = stage == TrainingStage::Locomotion
            && archetype != EnemyArchetype::Worker
        ? archetype == EnemyArchetype::IndustrialRobot
            ? random.rangeFloat(14.2F, 14.8F)
            : random.rangeFloat(profile.preferredMaximum + 2.5F,
                profile.preferredMaximum + 3.5F)
        : archetype == EnemyArchetype::IndustrialRobot
            && plan.initialHealthRatio < 0.33F
        ? random.rangeFloat(1.7F, 2.3F)
        : archetype == EnemyArchetype::IndustrialRobot
            && plan.initialHealthRatio < 0.66F
            ? random.rangeFloat(5.0F, 7.0F)
            : random.rangeFloat(profile.spawnMin, profile.spawnMax);
    const float sweep = random.rangeFloat(0.5F, 1.8F);
    const float direction = random.chance(0.5F) ? 1.0F : -1.0F;
    const float phase = random.rangeFloat(0.0F, 3.0F);
    wkc::core::Vec3 arenaOrigin{};
    std::uint32_t roomId = 1U;
    std::optional<world::World> generatedWorld;
    std::optional<wkc::core::Aabb> realRoomBounds;
    const std::vector<wkc::core::Aabb> stageObstacles
        = stage == TrainingStage::Locomotion
            ? std::vector<wkc::core::Aabb>{{{-0.75F, 0.0F, 1.4F},
                {0.75F, 0.75F, 2.3F}}}
        : stage == TrainingStage::Dodge
            ? std::vector<wkc::core::Aabb>{}
        : stage == TrainingStage::Aim
            ? coverFor(CoverPattern::Right) : coverFor(plan.cover);
    gameplay::CollisionWorld arena{
        {{-16.0F, 0.0F, -16.0F}, {16.0F, 4.0F, 16.0F}}, stageObstacles};
    if (stage == TrainingStage::RealMaps) {
        // Wide rooms can exhaust the generator's normal layout attempts for
        // some seeds. Retry with bounded, derived seeds so an otherwise valid
        // training generation remains deterministic and resumable.
        const unsigned worldRetries = cooperative ? 4U : 1U;
        for (unsigned retry = 0U; retry < worldRetries; ++retry) {
            const wkc::core::Seed worldSeed = retry == 0U ? seed
                : wkc::core::deriveSeed(seed, cooperativeWorldRetryDomain, retry);
            auto generated = world::generateWorld(worldSeed,
                {.roomCount = 7U, .loopBudget = 1U,
                    .maximumAttempts = cooperative ? 64U : 32U});
            if (generated.succeeded()) {
                generatedWorld = std::move(*generated.world);
                break;
            }
        }
        if (!generatedWorld) {
            throw std::runtime_error("cannot generate stage-seven training world");
        }
        const auto start = std::find_if(generatedWorld->rooms.begin(),
            generatedWorld->rooms.end(), [](const world::RoomInstance& room) {
                return room.role == world::RoomRole::Start;
            });
        if (start == generatedWorld->rooms.end()) {
            throw std::runtime_error("stage-seven world has no start room");
        }
        const world::RoomInstance* trainingRoom = &*start;
        if (contract == gameplay::EnemyBrainContract::Cooperative) {
            if (archetype == EnemyArchetype::IndustrialRobot) {
                const auto boss = std::find_if(generatedWorld->rooms.begin(),
                    generatedWorld->rooms.end(), [](const world::RoomInstance& room) {
                        return room.role == world::RoomRole::Boss;
                    });
                if (boss == generatedWorld->rooms.end()) {
                    throw std::runtime_error("cooperative training world has no Boss room");
                }
                trainingRoom = &*boss;
            } else {
                for (const world::RoomInstance& room : generatedWorld->rooms) {
                    const auto span = room.worldBounds.maximum - room.worldBounds.minimum;
                    const auto selectedSpan = trainingRoom->worldBounds.maximum
                        - trainingRoom->worldBounds.minimum;
                    if (room.role == world::RoomRole::Normal
                        && span.x * span.z > selectedSpan.x * selectedSpan.z) {
                        trainingRoom = &room;
                    }
                }
            }
        }
        roomId = trainingRoom->id;
        realRoomBounds = trainingRoom->worldBounds;
        arenaOrigin = {
            (trainingRoom->worldBounds.minimum.x + trainingRoom->worldBounds.maximum.x)
                * 0.5F,
            trainingRoom->worldBounds.minimum.y,
            (trainingRoom->worldBounds.minimum.z + trainingRoom->worldBounds.maximum.z)
                * 0.5F};
        arena = gameplay::makeRoomNavigationCollision(
            *generatedWorld, roomId, false);
    }
    entities::EntityManager manager;
    wkc::core::Vec3 enemyStart = arenaOrigin
        + wkc::core::Vec3{workerX, 0.0F, workerZ};
    bool corridorSpawn = false;
    std::optional<TrainingEncounter> encounter;
    if (generatedWorld && diverseMaps && (mapVariant > 0U || cooperative)) {
        const unsigned encounterVariant
            = cooperative && archetype == EnemyArchetype::IndustrialRobot
                ? 6U + (mapVariant % 2U)
                : cooperative && archetype != EnemyArchetype::Worker
                    && realRoomBounds
                    && std::min(realRoomBounds->maximum.x - realRoomBounds->minimum.x,
                        realRoomBounds->maximum.z - realRoomBounds->minimum.z) >= 16.0F
                    && std::max(realRoomBounds->maximum.x - realRoomBounds->minimum.x,
                        realRoomBounds->maximum.z - realRoomBounds->minimum.z) >= 18.0F
                    ? 8U : std::max(mapVariant, 1U);
        encounter = makeTrainingEncounter(*realRoomBounds, seed, encounterVariant);
        arena = encounter->collision;
        arenaOrigin = encounter->player;
        enemyStart = encounter->enemy;
        plan.player = encounter->peekWidth > 0.0F ? PlayerPattern::Peek : PlayerPattern::Sweep;
    }
    if (generatedWorld && !encounter) {
        for (const world::CorridorPath& corridor : generatedWorld->corridors) {
            if ((corridor.firstRoom == roomId || corridor.secondRoom == roomId)
                && corridor.points.size() > 1U) {
                const bool forward = corridor.firstRoom == roomId;
                const wkc::core::Vec3 doorway = forward
                    ? corridor.points.front() : corridor.points.back();
                const wkc::core::Vec3 next = forward
                    ? corridor.points[1U]
                    : corridor.points[corridor.points.size() - 2U];
                const wkc::core::Vec3 outward
                    = wkc::core::normalized(next - doorway);
                const wkc::core::Vec3 towardRoom
                    = wkc::core::normalized(arenaOrigin - doorway);
                const wkc::core::Vec3 playerCandidate
                    = doorway + towardRoom * 1.4F;
                const wkc::core::Vec3 enemyCandidate
                    = doorway + outward * 2.2F;
                if (arena.canOccupy(playerCandidate,
                        {.radius = 0.35F, .height = 1.8F})
                    && arena.canOccupy(enemyCandidate,
                        {.radius = 0.35F, .height = 1.8F})) {
                    arenaOrigin = playerCandidate;
                    enemyStart = enemyCandidate;
                    corridorSpawn = true;
                    break;
                }
            }
        }
    }
    gameplay::Player player;
    player.spawn(arenaOrigin);
    constexpr float degreesPerRadian = 57.29577951308232F;
    const wkc::core::Vec3 initialTarget = arenaOrigin - enemyStart;
    const float initialYaw = generatedWorld
        ? std::atan2(initialTarget.x, initialTarget.z) * degreesPerRadian
            + plan.facingOffsetDegrees
        : 180.0F + plan.facingOffsetDegrees;
    const entities::EntityId worker = manager.createEnemy({
        .transform = {.position = enemyStart,
            .rotationDegrees = {0.0F, initialYaw, 0.0F}},
        .roomId = roomId,
        .archetype = archetype,
        .health = profile.health,
        .visualSeed = seed,
    });
    manager.findData<entities::EnemyData>(worker)->health
        = profile.health * plan.initialHealthRatio;
    std::optional<entities::EntityId> supportAlly;
    std::optional<entities::EntityId> secondSupportAlly;
    if (cooperative) {
        const EnemyArchetype allyArchetype = archetype == EnemyArchetype::Guard
            ? EnemyArchetype::Worker : EnemyArchetype::Guard;
        const wkc::core::Vec3 towardPlayer = wkc::core::normalized(
            wkc::core::Vec3{arenaOrigin.x - enemyStart.x, 0.0F,
                arenaOrigin.z - enemyStart.z});
        const wkc::core::Vec3 lateral{-towardPlayer.z, 0.0F, towardPlayer.x};
        const float advance = std::min(2.5F,
            planarDistance(enemyStart, arenaOrigin) * 0.45F);
        const std::array<wkc::core::Vec3, 5> offsets{{
            towardPlayer * advance,
            towardPlayer * advance + lateral * 1.5F,
            towardPlayer * advance - lateral * 1.5F,
            lateral * 2.0F,
            lateral * -2.0F,
        }};
        for (const auto offset : offsets) {
            const wkc::core::Vec3 position = enemyStart + offset;
            if (planarDistance(position, enemyStart) < 1.0F
                || !arena.canOccupy(position, {.radius = 0.35F, .height = 1.8F})) {
                continue;
            }
            supportAlly = manager.createEnemy({
                .transform = {.position = position},
                .roomId = roomId,
                .archetype = allyArchetype,
                .health = allyArchetype == EnemyArchetype::Worker ? 50.0F : 75.0F,
                .visualSeed = seed,
            });
            if (archetype == EnemyArchetype::Engineer) {
                manager.findData<entities::EnemyData>(*supportAlly)->health = 25.0F;
            }
            break;
        }
        if (archetype == EnemyArchetype::IndustrialRobot) {
            const wkc::core::Vec3 position = enemyStart
                + lateral * -3.0F + towardPlayer * 1.0F;
            if (arena.canOccupy(position, {.radius = 0.35F, .height = 1.8F})
                && (!supportAlly || planarDistance(position,
                    manager.find(*supportAlly)->transform.position) >= 1.2F)) {
                secondSupportAlly = manager.createEnemy({
                    .transform = {.position = position},
                    .roomId = roomId,
                    .archetype = EnemyArchetype::Officer,
                    .health = 65.0F,
                    .visualSeed = seed,
                });
            }
        }
    } else if (archetype == EnemyArchetype::Officer) {
        const auto ally = manager.createEnemy({
            .transform = {.position = arenaOrigin + wkc::core::Vec3{
                scenario % 2U == 0U ? 2.0F : -3.0F, 0.0F,
                scenario % 3U == 0U ? 2.0F : 4.0F}},
            .roomId = roomId,
            .archetype = EnemyArchetype::Guard,
            .health = 75.0F,
            .visualSeed = seed,
        });
        manager.findData<entities::EnemyData>(ally)->attackCooldown = 5.0F;
        supportAlly = ally;
    } else if (archetype == EnemyArchetype::Engineer) {
        const auto ally = manager.createEnemy({
            .transform = {.position = arenaOrigin + wkc::core::Vec3{
                scenario % 2U == 0U ? 2.0F : -3.0F, 0.0F,
                scenario % 3U == 0U ? 2.0F : 4.0F}},
            .roomId = roomId,
            .archetype = EnemyArchetype::Guard,
            .health = 35.0F,
            .visualSeed = seed,
        });
        manager.findData<entities::EnemyData>(ally)->health = 25.0F;
    }
    gameplay::NeuralEnemyExperiment brain{
        seed, archetype, contract};
    brain.genome = genome;
    brain.controlAll = true;
    brain.logSelection = false;
    WorkerFitnessMetrics metrics;
    metrics.groupEpisodes = supportAlly && cooperative ? 1U : 0U;
    metrics.realMapEpisodes = generatedWorld ? 1U : 0U;
    metrics.largeRoomEpisodes = realRoomBounds
        && std::min(realRoomBounds->maximum.x - realRoomBounds->minimum.x,
            realRoomBounds->maximum.z - realRoomBounds->minimum.z) >= 16.0F
        && std::max(realRoomBounds->maximum.x - realRoomBounds->minimum.x,
            realRoomBounds->maximum.z - realRoomBounds->minimum.z) >= 18.0F ? 1U : 0U;
    metrics.corridorSpawns = corridorSpawn ? 1U : 0U;
    metrics.coverMapEpisodes = encounter && !encounter->elevated ? 1U : 0U;
    metrics.peekMapEpisodes = encounter && encounter->peekWidth > 0.0F ? 1U : 0U;
    metrics.heightMapEpisodes = encounter && encounter->elevated ? 1U : 0U;
    metrics.longMapEpisodes = encounter
        && planarDistance(encounter->player, encounter->enemy) >= 10.0F ? 1U : 0U;
    metrics.bossHelperEpisodes = supportAlly && secondSupportAlly ? 1U : 0U;
    gameplay::EnemyProjectileSystem enemyProjectiles;
    std::optional<IncomingTrainingShot> incomingShot;
    float shotCooldown = 1.0F;
    float previousForwardIntent = 0.0F;
    float previousStrafeIntent = 0.0F;
    float totalTacticalProgress = 0.0F;
    bool previousNeuralDecision = false;
    const int steps = archetype == EnemyArchetype::IndustrialRobot
        ? bossEpisodeSteps : episodeSteps;
    for (int tick = 0; tick < steps; ++tick) {
        entities::Entity* entity = manager.find(worker);
        entities::EnemyData* enemy = manager.findData<entities::EnemyData>(worker);
        if (entity == nullptr || enemy == nullptr
            || enemy->behavior == entities::EnemyBehaviorState::Dead
            || !player.state().alive) {
            break;
        }
        const float time = static_cast<float>(tick) * fixedStep;
        if (supportAlly && !cooperative) {
            entities::EnemyData* ally
                = manager.findData<entities::EnemyData>(*supportAlly);
            ally->attackCooldown = std::max(
                0.0F, ally->attackCooldown - fixedStep);
        }
        const PlayerPattern playerPattern = !encounter && stage == TrainingStage::RealMaps
            && (seed & 1U) != 0U ? PlayerPattern::Bhop : plan.player;
        if (playerPattern == PlayerPattern::Bhop) {
            const float previousVerticalSpeed = player.state().velocity.y;
            gameplay::PlayerCommand command;
            command.moveForward = tick < 10 ? 1.0F : 0.0F;
            command.moveRight = ((tick / 45) % 2 == 0 ? direction : -direction);
            command.jump = tick == 0 || player.state().grounded;
            player.fixedUpdate(command, fixedStep, arena);
            const wkc::core::Vec3 velocity = player.state().velocity;
            if (previousVerticalSpeed <= 0.0F && velocity.y > 0.0F) {
                ++metrics.playerJumps;
            }
            metrics.peakPlayerSpeed = std::max(metrics.peakPlayerSpeed,
                std::hypot(velocity.x, velocity.z));
        } else {
            wkc::core::Vec3 nextPlayerPosition
                = (stage == TrainingStage::Aim
                    ? aimStagePlayerPosition(time, direction)
                    : playerPosition(playerPattern, time, direction,
                        sweep, phase)) + arenaOrigin;
            if (encounter) {
                nextPlayerPosition = arenaOrigin;
                nextPlayerPosition.x += encounter->elevated
                    ? 0.05F * std::sin(time + phase)
                    : (encounter->peekWidth > 0.0F ? encounter->peekWidth : 0.65F)
                        * std::sin(time * 1.2F + phase);
            }
            if (arena.canOccupy(nextPlayerPosition,
                    {.radius = 0.35F, .height = 1.8F})) {
                (void)player.relocate(nextPlayerPosition, -90.0F);
            }
        }
        if (stage >= TrainingStage::Dodge) {
            aimTrainingPlayerAt(player,
                entity->transform.position + wkc::core::Vec3{0.0F, 1.0F, 0.0F});
        }
        if (stage == TrainingStage::Dodge && !incomingShot
            && shotCooldown <= 0.0F
            && planarDistance(entity->transform.position,
                player.state().position) <= profile.playerAttackRange) {
            const wkc::core::Vec3 origin = player.cameraPosition();
            const wkc::core::Vec3 target = entity->transform.position
                + wkc::core::Vec3{0.0F, 1.0F, 0.0F};
            if (gameplay::hasLineOfSight(origin, target, arena)) {
                const wkc::core::Vec3 offset = target - origin;
                const float distance = wkc::core::length(offset);
                incomingShot = IncomingTrainingShot{
                    origin, offset * (trainingShotSpeed(archetype) / distance),
                    target, distance, 0.0F};
                ++metrics.dodgeOpportunities;
                shotCooldown = plan.playerShotInterval;
            }
        }
        const wkc::core::Vec3 before = entity->transform.position;
        const bool exposedBefore = gameplay::hasLineOfSight(
            player.cameraPosition(),
            before + wkc::core::Vec3{0.0F, 1.0F, 0.0F}, arena);
        const float previousAlignment = facingAlignment(before,
            entity->transform.rotationDegrees.y, player.state().position);
        const float playerHealthBefore = player.state().health;
        const std::uint32_t magazineBefore = enemy->magazineAmmo;
        const std::uint64_t controlledBefore = brain.controlledTicks;
        std::vector<gameplay::EnemyProjectile> spawnedShots;
        const auto summary = updateTrainingEnemy(
            archetype, manager, player, roomId, arena, &brain, spawnedShots,
            stage, !cooperative || stage != TrainingStage::RealMaps
                || time >= 1.25F);
        const float playerHealthAfterCandidate = player.state().health;
        if (cooperative && supportAlly && player.state().alive) {
            const entities::EnemyData* ally
                = manager.findData<entities::EnemyData>(*supportAlly);
            if (ally && ally->behavior != entities::EnemyBehaviorState::Dead) {
                const EnemyArchetype allyArchetype = ally->archetype;
                (void)updateTrainingEnemy(allyArchetype, manager, player,
                    roomId, arena, nullptr, spawnedShots, stage,
                    stage != TrainingStage::RealMaps || time >= 1.25F);
            }
        }
        if (cooperative && secondSupportAlly && player.state().alive) {
            const entities::EnemyData* ally
                = manager.findData<entities::EnemyData>(*secondSupportAlly);
            if (ally && ally->behavior != entities::EnemyBehaviorState::Dead) {
                (void)updateTrainingEnemy(ally->archetype, manager, player,
                    roomId, arena, nullptr, spawnedShots, stage,
                    stage != TrainingStage::RealMaps || time >= 1.25F);
            }
        }
        const bool controlled = brain.controlledTicks != controlledBefore;
        const auto motor = brain.motors.find(worker.packed());
        const std::array<float, 5> neutralActions{};
        const std::array<float, 5>& actions = motor != brain.motors.end()
            ? motor->second.outputs : neutralActions;
        enemyProjectiles.spawn(spawnedShots);
        const auto projectileEvents = enemyProjectiles.update(
            fixedStep, player, arena, manager);
        bool neuralProjectileElimination = false;
        for (const auto& event : projectileEvents) {
            if (cooperative && event.source != worker) {
                continue;
            }
            if (event.hitEnemy.isValid()) {
                ++metrics.friendlyHits;
                metrics.friendlyDamage += event.damageApplied;
                continue;
            }
            if (event.hitPlayer && event.neural) {
                metrics.damageDealt += event.damageApplied;
                ++metrics.attacksLanded;
                neuralProjectileElimination = !player.state().alive;
            } else if (event.hitPlayer) {
                metrics.fallbackDamageDealt += event.damageApplied;
                ++metrics.fallbackAttacksLanded;
            } else if (event.neural && event.blocked) {
                ++metrics.shotsBlocked;
            } else if (event.neural && event.dodged) {
                ++metrics.shotsDodged;
            }
        }
        if (controlled) {
            ++metrics.neuralTicks;
        } else {
            ++metrics.fallbackTicks;
        }
        const float movement = planarDistance(before, entity->transform.position);
        const float distanceBefore = planarDistance(
            before, player.state().position);
        const float distance = planarDistance(
            entity->transform.position, player.state().position);
        const float healthRatio = enemy->maximumHealth > 0.0F
            ? enemy->health / enemy->maximumHealth : 0.0F;
        const TrainingProfile currentProfile
            = phaseProfile(archetype, profile, healthRatio);
        const float tacticalProgress = rangeError(distanceBefore, currentProfile)
            - rangeError(distance, currentProfile);
        const bool exposedAfter = gameplay::hasLineOfSight(
            player.cameraPosition(), entity->transform.position
                + wkc::core::Vec3{0.0F, 1.0F, 0.0F}, arena);
        metrics.exposedSeconds += exposedAfter ? fixedStep : 0.0F;
        metrics.coverSeconds += exposedAfter ? 0.0F : fixedStep;
        metrics.rangeErrorSeconds += rangeError(distance, currentProfile)
            * fixedStep;
        metrics.distanceSum += distance;
        ++metrics.distanceSamples;
        if (controlled && exposedBefore && !exposedAfter) {
            ++metrics.coverEntries;
        }
        totalTacticalProgress += tacticalProgress;
        if (controlled) {
            metrics.tacticalProgressMeters += tacticalProgress;
            if (archetype == EnemyArchetype::Worker) {
                metrics.damageDealt += std::max(
                    0.0F, playerHealthBefore - playerHealthAfterCandidate);
                metrics.attacksLanded += static_cast<std::uint32_t>(
                    summary.attacksLanded);
            }
            metrics.shotsFired += static_cast<std::uint32_t>(summary.shotsFired);
            metrics.reloadsStarted += static_cast<std::uint32_t>(
                summary.reloadsStarted);
            // A voluntary reload with at least half the magazine remaining
            // delays offense; an empty-magazine reload is not wasteful.
            if (summary.reloadsStarted > 0U && magazineBefore >= 4U) {
                metrics.prematureReloads += static_cast<std::uint32_t>(
                    summary.reloadsStarted);
            }
        } else {
            if (archetype == EnemyArchetype::Worker) {
                metrics.fallbackDamageDealt += std::max(
                    0.0F, playerHealthBefore - playerHealthAfterCandidate);
                metrics.fallbackAttacksLanded += static_cast<std::uint32_t>(
                    summary.attacksLanded);
            }
            metrics.fallbackShotsFired += static_cast<std::uint32_t>(
                summary.shotsFired);
        }
        metrics.supportActions += static_cast<std::uint32_t>(
            summary.commandsIssued + summary.repairsPerformed);
        metrics.friendlyFireHolds += static_cast<std::uint32_t>(
            summary.friendlyFireHolds);
        metrics.bossLongRangeSeconds += static_cast<float>(
            summary.bossLongRange) * fixedStep;
        metrics.bossPressureSeconds += static_cast<float>(
            summary.bossPressure) * fixedStep;
        metrics.bossPursuitSeconds += static_cast<float>(
            summary.bossPursuit) * fixedStep;
        metrics.survivalSeconds += fixedStep;
        if (incomingShot) {
            const IncomingShotResult incoming = advanceIncomingShot(*incomingShot,
                entity->transform.position,
                archetype == EnemyArchetype::IndustrialRobot ? 0.52F : 0.35F,
                arena);
            if (incoming.finished) {
                // Credit only lateral displacement from the actual shot line,
                // and only while neural control still failed to avoid a hit.
                // Generic strafing or movement behind cover earns nothing.
                if (incoming.hit && controlled) {
                    metrics.dodgeLateralMeters += shotLateralDisplacement(
                        *incomingShot, entity->transform.position);
                }
                incomingShot.reset();
                if (incoming.hit) {
                    const float healthBefore = enemy->health;
                    (void)gameplay::applyDamage(manager, worker, 8.0F);
                    ++metrics.incomingProjectileHits;
                    if (controlled) {
                        metrics.damageReceived += healthBefore - enemy->health;
                    } else {
                        metrics.fallbackDamageReceived
                            += healthBefore - enemy->health;
                    }
                } else if (incoming.dodged && controlled) {
                    ++metrics.dodgesSucceeded;
                }
            }
        }
        const bool playerVisible = gameplay::hasLineOfSight(
                entity->transform.position + wkc::core::Vec3{0.0F, 1.0F, 0.0F},
                player.cameraPosition(), arena);
        if (controlled) {
            const float forward = std::clamp(actions[0], -1.0F, 1.0F);
            const float strafe = std::clamp(actions[1], -1.0F, 1.0F);
            const DirectionalCredit credit = directionalMovementCredit(
                forward, strafe, fixedStep);
            metrics.forwardCreditSeconds += credit.forward;
            metrics.retreatCreditSeconds += credit.retreat;
            metrics.strafeCreditSeconds += credit.strafe;
            const bool reversed = previousNeuralDecision
                && ((previousForwardIntent > 0.3F && forward < -0.3F)
                    || (previousForwardIntent < -0.3F && forward > 0.3F)
                    || (previousStrafeIntent > 0.3F && strafe < -0.3F)
                    || (previousStrafeIntent < -0.3F && strafe > 0.3F));
            const bool underThreat = stage >= TrainingStage::Dodge && playerVisible
                && distance <= profile.playerAttackRange
                && shotCooldown <= 0.2F;
            const float currentAlignment = facingAlignment(
                entity->transform.position,
                entity->transform.rotationDegrees.y,
                player.state().position);
            if (reversed && tacticalProgress <= 0.0F
                && currentAlignment <= previousAlignment + 0.01F
                && !underThreat) {
                ++metrics.unproductiveReversals;
            }
            previousForwardIntent = forward;
            previousStrafeIntent = strafe;
        }
        previousNeuralDecision = controlled;
        if (controlled && distance >= currentProfile.preferredMinimum
            && distance <= currentProfile.preferredMaximum && playerVisible) {
            metrics.proximitySeconds += fixedStep;
        }
        const float contactRange = archetype == EnemyArchetype::Worker
            ? gameplay::WorkerConfig{}.attackRange
            : currentProfile.preferredMaximum + 2.0F;
        if (controlled && distance <= contactRange
            && playerVisible) {
            metrics.contactSeconds += fixedStep;
            if (distance > 1.0e-5F) {
                constexpr float radiansPerDegree = 0.01745329251994329577F;
                const float aimCosine = archetype == EnemyArchetype::Worker
                    ? 0.8191520443F : 0.9781476007F;
                const float yaw = entity->transform.rotationDegrees.y
                    * radiansPerDegree;
                const wkc::core::Vec3 forward{
                    std::sin(yaw), 0.0F, std::cos(yaw)};
                const wkc::core::Vec3 towardPlayer{
                    (player.state().position.x - entity->transform.position.x)
                        / distance,
                    0.0F,
                    (player.state().position.z - entity->transform.position.z)
                        / distance};
                if (wkc::core::dot(forward, towardPlayer) >= aimCosine) {
                    metrics.aimSeconds += fixedStep;
                }
            }
        }
        const float requestedMovement = std::hypot(
            actions[0], actions[1]);
        if (controlled && movement < 0.002F
            && rangeError(distance, currentProfile) > 0.5F
            && requestedMovement < 0.1F) {
            metrics.idleSeconds += fixedStep;
        }
        if (controlled && movement < 0.002F && requestedMovement > 0.3F) {
            metrics.blockedSeconds += fixedStep;
        }
        if (controlled && actions[3] > 0.65F
            && summary.attacking == 0U) {
            metrics.wastedAttackSeconds += fixedStep;
        }
        if (!player.state().alive) {
            if (archetype != EnemyArchetype::Worker
                    ? neuralProjectileElimination
                    : controlled && playerHealthAfterCandidate <= 0.0F) {
                metrics.playerEliminations = 1U;
            } else {
                metrics.fallbackPlayerEliminations = 1U;
            }
            break;
        }
        shotCooldown -= fixedStep;
        if (stage >= TrainingStage::Duel && shotCooldown <= 0.0F
            && distance <= profile.playerAttackRange
            && gameplay::hasLineOfSight(
                player.cameraPosition(),
                entity->transform.position
                    + wkc::core::Vec3{0.0F, 1.0F, 0.0F}, arena)) {
            const float healthBefore = enemy->health;
            (void)gameplay::applyDamage(manager, worker, 8.0F);
            if (controlled) {
                metrics.damageReceived += healthBefore - enemy->health;
            } else {
                metrics.fallbackDamageReceived += healthBefore - enemy->health;
            }
            shotCooldown = plan.playerShotInterval;
        }
    }
    metrics.tacticalProgressMeters = creditedTacticalProgress(
        metrics.tacticalProgressMeters, totalTacticalProgress);
    if (const entities::EnemyData* enemy
        = manager.findData<entities::EnemyData>(worker);
        enemy != nullptr && enemy->behavior == entities::EnemyBehaviorState::Dead) {
        metrics.deaths = 1U;
    }
    return metrics;
}

struct WorkerEvaluation {
    float score{0.0F};
    float worstScenario{0.0F};
    WorkerFitnessMetrics metrics{};
    std::array<float, scenarioCount> stageScores{};
    std::array<std::uint32_t, scenarioCount> stageHitEpisodes{};
    std::array<std::uint32_t, scenarioCount> stageDodges{};
    std::array<std::uint32_t, scenarioCount> stageDodgeOpportunities{};
    std::uint32_t combatEpisodesWithHit{0U};
    bool combatCompetent{true};
};

void appendStageDiagnostics(std::ostream& output,
    const WorkerEvaluation& evaluation)
{
    output << " stage_scores=";
    for (std::size_t stage = 0U; stage < scenarioCount; ++stage) {
        if (stage > 0U) output << ',';
        output << evaluation.stageScores[stage];
    }
    output << " stage_hit_episodes=";
    for (std::size_t stage = 0U; stage < scenarioCount; ++stage) {
        if (stage > 0U) output << ',';
        output << evaluation.stageHitEpisodes[stage];
    }
    output << " stage_dodges=";
    for (std::size_t stage = 0U; stage < scenarioCount; ++stage) {
        if (stage > 0U) output << ',';
        output << evaluation.stageDodges[stage] << '/'
               << evaluation.stageDodgeOpportunities[stage];
    }
}

void addMetrics(WorkerFitnessMetrics& total, const WorkerFitnessMetrics& sample)
{
    total.damageDealt += sample.damageDealt;
    total.damageReceived += sample.damageReceived;
    total.fallbackDamageDealt += sample.fallbackDamageDealt;
    total.fallbackDamageReceived += sample.fallbackDamageReceived;
    total.survivalSeconds += sample.survivalSeconds;
    total.tacticalProgressMeters += sample.tacticalProgressMeters;
    total.proximitySeconds += sample.proximitySeconds;
    total.aimSeconds += sample.aimSeconds;
    total.exposedSeconds += sample.exposedSeconds;
    total.coverSeconds += sample.coverSeconds;
    total.rangeErrorSeconds += sample.rangeErrorSeconds;
    total.distanceSum += sample.distanceSum;
    total.bossLongRangeSeconds += sample.bossLongRangeSeconds;
    total.bossPressureSeconds += sample.bossPressureSeconds;
    total.bossPursuitSeconds += sample.bossPursuitSeconds;
    total.dodgeLateralMeters += sample.dodgeLateralMeters;
    total.idleSeconds += sample.idleSeconds;
    total.contactSeconds += sample.contactSeconds;
    total.blockedSeconds += sample.blockedSeconds;
    total.wastedAttackSeconds += sample.wastedAttackSeconds;
    total.forwardCreditSeconds += sample.forwardCreditSeconds;
    total.retreatCreditSeconds += sample.retreatCreditSeconds;
    total.strafeCreditSeconds += sample.strafeCreditSeconds;
    total.peakPlayerSpeed = std::max(
        total.peakPlayerSpeed, sample.peakPlayerSpeed);
    total.attacksLanded += sample.attacksLanded;
    total.shotsFired += sample.shotsFired;
    total.shotsBlocked += sample.shotsBlocked;
    total.shotsDodged += sample.shotsDodged;
    total.reloadsStarted += sample.reloadsStarted;
    total.prematureReloads += sample.prematureReloads;
    total.dodgeOpportunities += sample.dodgeOpportunities;
    total.dodgesSucceeded += sample.dodgesSucceeded;
    total.incomingProjectileHits += sample.incomingProjectileHits;
    total.coverEntries += sample.coverEntries;
    total.distanceSamples += sample.distanceSamples;
    total.realMapEpisodes += sample.realMapEpisodes;
    total.largeRoomEpisodes += sample.largeRoomEpisodes;
    total.corridorSpawns += sample.corridorSpawns;
    total.coverMapEpisodes += sample.coverMapEpisodes;
    total.peekMapEpisodes += sample.peekMapEpisodes;
    total.heightMapEpisodes += sample.heightMapEpisodes;
    total.longMapEpisodes += sample.longMapEpisodes;
    total.bossHelperEpisodes += sample.bossHelperEpisodes;
    total.fallbackAttacksLanded += sample.fallbackAttacksLanded;
    total.fallbackShotsFired += sample.fallbackShotsFired;
    total.playerEliminations += sample.playerEliminations;
    total.fallbackPlayerEliminations += sample.fallbackPlayerEliminations;
    total.deaths += sample.deaths;
    total.supportActions += sample.supportActions;
    total.unproductiveReversals += sample.unproductiveReversals;
    total.neuralTicks += sample.neuralTicks;
    total.fallbackTicks += sample.fallbackTicks;
    total.playerJumps += sample.playerJumps;
    total.groupEpisodes += sample.groupEpisodes;
    total.friendlyHits += sample.friendlyHits;
    total.friendlyFireHolds += sample.friendlyFireHolds;
    total.friendlyDamage += sample.friendlyDamage;
}

WorkerEvaluation evaluateGenome(
    const wkc::ai::Genome& genome,
    const wkc::core::Seed scenarioSeed,
    const EnemyArchetype archetype,
    const bool holdout = false,
    const bool trainAllScenarios = false,
    const std::size_t unlockedStages = scenarioCount,
    const bool diverseMaps = false,
    const gameplay::EnemyBrainContract contract
        = gameplay::EnemyBrainContract::Tactical)
{
    WorkerEvaluation evaluation;
    float worst = std::numeric_limits<float>::infinity();
    const std::size_t count = holdout ? holdoutScenarioCount
        : std::clamp(unlockedStages, std::size_t{1}, scenarioCount);
    const std::size_t repetitions = trainAllScenarios
        ? scenarioRepetitions * 2U : scenarioRepetitions;
    for (std::size_t offset = 0U; offset < count; ++offset) {
        const std::size_t scenario = offset;
        for (std::size_t repetition = 0U;
             repetition < repetitions; ++repetition) {
            const WorkerFitnessMetrics metrics = simulateEpisode(genome,
                wkc::core::deriveSeed(scenarioSeed, trainingScenarioDomain,
                    scenario * maximumScenarioRepetitions + repetition), scenario,
                archetype, diverseMaps,
                static_cast<unsigned>((scenarioSeed % 6U + repetition) % 6U),
                static_cast<unsigned>((scenarioSeed % 3U + repetition) % 3U),
                contract);
            const TrainingStage evaluatedStage
                = static_cast<TrainingStage>(scenario + 1U);
            const float scenarioScore
                = contract == gameplay::EnemyBrainContract::Cooperative
                    ? scoreCooperativeTrainingStage(metrics, archetype,
                        evaluatedStage)
                    : scoreTrainingStage(metrics, archetype, evaluatedStage);
            evaluation.score += scenarioScore;
            evaluation.stageScores[scenario] += scenarioScore
                / static_cast<float>(repetitions);
            evaluation.stageHitEpisodes[scenario] += metrics.attacksLanded > 0U;
            evaluation.stageDodges[scenario] += metrics.dodgesSucceeded;
            evaluation.stageDodgeOpportunities[scenario]
                += metrics.dodgeOpportunities;
            worst = std::min(worst, scenarioScore);
            addMetrics(evaluation.metrics, metrics);
            if (scenario + 1U >= static_cast<std::size_t>(TrainingStage::Duel)
                && metrics.attacksLanded > 0U) {
                ++evaluation.combatEpisodesWithHit;
            }
        }
    }
    evaluation.worstScenario = worst;
    evaluation.score = 0.75F
        * (evaluation.score
            / static_cast<float>(count * repetitions))
        + 0.25F * worst;
    // A melee attacker or Engineer that never participates in a representative
    // share of the combat stages must not beat a risky but useful policy by
    // merely avoiding the terminal death penalty. Early curriculum stages
    // remain unconstrained so they can learn movement and aim first.
    if (count == scenarioCount
        && (archetype == EnemyArchetype::Worker
            || archetype == EnemyArchetype::Engineer)
        && evaluation.combatEpisodesWithHit < repetitions) {
        evaluation.combatCompetent = false;
        evaluation.score = -20.0F + evaluation.score * 0.01F;
    }
    return evaluation;
}

wkc::ai::Genome freshGenome(
    const wkc::core::Seed seed,
    const std::uint32_t generation,
    const std::uint32_t id,
    const gameplay::EnemyBrainContract contract)
{
    const auto shape = gameplay::enemyBrainShape(contract);
    wkc::ai::Genome genome{
        wkc::ai::NeuralNetwork{shape.inputs, shape.hidden, shape.outputs},
        0.0F, generation, id};
    genome.network.randomize(seed, 0.5F);
    return genome;
}

wkc::ai::Genome curriculumGenome(
    const EnemyArchetype archetype,
    const std::uint32_t generation,
    const std::uint32_t id,
    const gameplay::EnemyBrainContract contract)
{
    const auto shape = gameplay::enemyBrainShape(contract);
    wkc::ai::Genome genome{
        wkc::ai::NeuralNetwork{shape.inputs, shape.hidden, shape.outputs},
        0.0F, generation, id};
    auto parameters = genome.network.parameters();
    // A small, directionally correct starting policy. Sensor angle is positive
    // to the enemy's left, while positive yaw turns right. Hidden 1 uses
    // distance for approach/retreat; the attack neuron is gated by visibility.
    const std::size_t hiddenStride = shape.inputs + 1U;
    const std::size_t outputStride = shape.hidden + 1U;
    const std::size_t outputBase = shape.hidden * hiddenStride;
    parameters[1U] = 1.0F;
    parameters[hiddenStride] = -1.0F;
    parameters[2U * hiddenStride + 2U] = 1.0F;
    parameters[3U * hiddenStride + static_cast<std::size_t>(
        gameplay::TacticalEnemySensor::Ammo)] = -1.0F;
    parameters[4U * hiddenStride + static_cast<std::size_t>(
        gameplay::TacticalEnemySensor::PlayerAimThreat)] = 1.0F;
    if (archetype == EnemyArchetype::Worker) {
        parameters[outputBase + shape.hidden] = 0.7F;
    } else {
        parameters[outputBase + 1U] = 1.0F;
    }
    parameters[outputBase + outputStride + 4U] = id % 2U == 0U ? 1.0F : -1.0F;
    parameters[outputBase + outputStride + shape.hidden]
        = id % 2U == 0U ? 0.2F : -0.2F;
    parameters[outputBase + 2U * outputStride] = -1.0F;
    parameters[outputBase + 3U * outputStride + 2U] = 1.0F;
    parameters[outputBase + 3U * outputStride + shape.hidden] = 0.2F;
    parameters[outputBase + 4U * outputStride + 3U] = 1.0F;
    parameters[outputBase + 4U * outputStride + shape.hidden] = 0.25F;
    return genome;
}

std::size_t tournamentWinner(
    const std::vector<wkc::ai::Genome>& population,
    wkc::core::RandomStream& random)
{
    std::size_t winner = static_cast<std::size_t>(
        random.rangeInt(0, static_cast<int>(population.size()) - 1));
    for (int contestant = 1; contestant < 3; ++contestant) {
        const std::size_t candidate = static_cast<std::size_t>(
            random.rangeInt(0, static_cast<int>(population.size()) - 1));
        if (population[candidate].fitness > population[winner].fitness) {
            winner = candidate;
        }
    }
    return winner;
}

std::vector<wkc::ai::Genome> nextPopulation(
    const std::vector<wkc::ai::Genome>& ranked,
    const wkc::core::Seed seed,
    const std::uint32_t generation,
    const gameplay::EnemyBrainContract contract)
{
    wkc::core::RandomStream random{
        wkc::core::deriveSeed(seed, evolutionDomain, generation)};
    std::vector<wkc::ai::Genome> next;
    next.reserve(ranked.size());
    const std::size_t eliteCount = std::max<std::size_t>(1, ranked.size() / 10U);
    for (std::size_t index = 0; index < ranked.size(); ++index) {
        wkc::ai::Genome child = index < eliteCount
            ? ranked[index]
            : random.chance(0.10F)
                ? freshGenome(random.nextU64(), generation,
                    static_cast<std::uint32_t>(index), contract)
                : ranked[tournamentWinner(ranked, random)];
        if (index >= eliteCount && child.generation != generation) {
            wkc::ai::mutate(child, random.nextU64(), 0.12F, 0.18F);
        }
        child.generation = generation;
        child.id = static_cast<std::uint32_t>(index);
        child.fitness = 0.0F;
        next.push_back(std::move(child));
    }
    return next;
}

std::filesystem::path checkpointPath(
    const std::filesystem::path& directory,
    const std::uint32_t generation,
    const EnemyArchetype archetype)
{
    std::ostringstream name;
    name << archetypeName(archetype) << "_generation_"
         << std::setw(6) << std::setfill('0')
         << generation << ".brain";
    return directory / name.str();
}

bool prepareOutputDirectory(const std::filesystem::path& directory)
{
    std::error_code error;
    if (directory.empty()) {
        return false;
    }
    if (std::filesystem::exists(directory, error)) {
        return !error && std::filesystem::is_directory(directory, error)
            && !error && std::filesystem::is_empty(directory, error) && !error;
    }
    return !error && std::filesystem::create_directories(directory, error) && !error;
}

} // namespace

float evaluateWorkerGenome(
    const wkc::ai::Genome& genome,
    const wkc::core::Seed scenarioSeed)
{
    return evaluateGenome(genome, scenarioSeed, EnemyArchetype::Worker).score;
}

WorkerTrainingResult runWorkerTraining(
    const WorkerTrainingOptions& options,
    std::ostream& output)
{
    if (options.archetype != EnemyArchetype::Worker) {
        WorkerTrainingResult result;
        result.error = "runWorkerTraining requires the Worker archetype";
        return result;
    }
    return runEnemyTraining(options, output);
}

WorkerTrainingResult runEnemyTraining(
    const WorkerTrainingOptions& options,
    std::ostream& output)
{
    WorkerTrainingResult result;
    if (options.archetype > EnemyArchetype::IndustrialRobot
        || (options.brainContract != gameplay::EnemyBrainContract::Tactical
            && options.brainContract != gameplay::EnemyBrainContract::Cooperative)
        || options.population < 2U || options.population > 128U
        || options.generations == 0U || options.generations > 10'000U
        || options.cooldownMilliseconds > 10'000U
        || options.curriculumGenerations > 10'000U
        || (options.resumeBrain && options.resumePopulation)) {
        result.error = "population must be 2..128, generations 1..10000, "
            "and cooldown 0..10000 ms";
        return result;
    }
    try {
        wkc::core::Seed seed = options.seed;
        std::optional<persistence::EnemyBrainRecord> resume;
        std::optional<TrainingPopulationRecord> fullResume;
        std::uint32_t firstGeneration = 1U;
        const bool cooperative
            = options.brainContract == gameplay::EnemyBrainContract::Cooperative;
        const std::uint32_t curriculum = cooperative
            ? (options.diverseMaps
                ? (options.trainAllScenarios ? 19U : 18U)
                : (options.trainAllScenarios ? 17U : 16U))
            : options.diverseMaps
                ? (options.trainAllScenarios ? 15U : 14U)
                : (options.trainAllScenarios ? 13U : 12U);
        std::uint32_t warmupStart = 1U;
        std::uint32_t warmupGenerations = options.curriculumGenerations > 0U
            ? options.curriculumGenerations : options.generations;
        if (options.resumePopulation) {
            fullResume = loadTrainingPopulation(*options.resumePopulation);
            if (!fullResume || fullResume->archetype != options.archetype
                || fullResume->contract != options.brainContract
                || fullResume->population.size() != options.population
                || fullResume->curriculum != curriculum
                || (options.seedExplicit && seed != fullResume->seed)
                || (options.curriculumGenerations > 0U
                    && options.curriculumGenerations != fullResume->warmupGenerations)
                || fullResume->generation > std::numeric_limits<std::uint32_t>::max()
                    - options.generations) {
                result.error = "invalid or incompatible training population checkpoint";
                return result;
            }
            seed = fullResume->seed;
            firstGeneration = fullResume->generation + 1U;
            warmupStart = fullResume->warmupStart;
            warmupGenerations = fullResume->warmupGenerations;
        }
        if (options.resumeBrain) {
            // A standalone brain has no curriculum metadata. Cooperative
            // resumes must use the complete, versioned population snapshot.
            if (cooperative) {
                result.error = "cooperative --resume is unsupported; use --resume-population";
                return result;
            }
            resume = persistence::loadEnemyBrain(*options.resumeBrain);
            if (!resume || resume->archetype != options.archetype) {
                result.error = "cannot load a matching enemy brain for resume";
                return result;
            }
            if (resume->contract != options.brainContract) {
                result.error = "brain contract does not match training mode";
                return result;
            }
            if (options.seedExplicit && seed != resume->trainingSeed) {
                result.error = "resume brain seed does not match --seed";
                return result;
            }
            seed = resume->trainingSeed;
            if (resume->genome.generation
                > std::numeric_limits<std::uint32_t>::max() - options.generations) {
                result.error = "generation number would overflow";
                return result;
            }
            firstGeneration = resume->genome.generation + 1U;
            warmupStart = firstGeneration;
            warmupGenerations = 0U;
        }
        if (options.saveDirectory
            && !prepareOutputDirectory(*options.saveDirectory)) {
            result.error = "save directory must be new or empty";
            return result;
        }
        const wkc::core::Seed populationSeed = wkc::core::deriveSeed(
            seed, evolutionDomain,
            static_cast<std::uint64_t>(options.archetype));
        std::vector<wkc::ai::Genome> population;
        population.reserve(options.population);
        if (fullResume) {
            population = nextPopulation(fullResume->population, populationSeed,
                firstGeneration, options.brainContract);
        }
        for (std::uint32_t index = 0; !fullResume && index < options.population; ++index) {
            if (resume && index == 0U) {
                wkc::ai::Genome seeded = resume->genome;
                seeded.generation = firstGeneration;
                seeded.id = index;
                seeded.fitness = 0.0F;
                population.push_back(std::move(seeded));
            } else if (resume) {
                wkc::ai::Genome seeded = resume->genome;
                wkc::ai::mutate(seeded,
                    wkc::core::deriveSeed(populationSeed, initialBrainDomain,
                        (static_cast<std::uint64_t>(firstGeneration) << 32U) | index),
                    0.12F, 0.18F);
                seeded.generation = firstGeneration;
                seeded.id = index;
                population.push_back(std::move(seeded));
            } else if (index == 0U || (options.population >= 4U
                    && index < options.population / 4U)) {
                wkc::ai::Genome seeded = curriculumGenome(
                    options.archetype, firstGeneration, index,
                    options.brainContract);
                wkc::ai::mutate(seeded,
                    wkc::core::deriveSeed(populationSeed,
                        initialBrainDomain, index),
                    index == 0U ? 0.05F : 0.12F,
                    index == 0U ? 0.05F : 0.18F);
                population.push_back(std::move(seeded));
            } else {
                population.push_back(freshGenome(
                    wkc::core::deriveSeed(populationSeed, initialBrainDomain, index),
                    firstGeneration, index, options.brainContract));
            }
        }
        const wkc::core::Seed commonScenarios = wkc::core::deriveSeed(
            seed, trainingScenarioDomain,
            static_cast<std::uint64_t>(options.archetype));
        output << "AI_TRAINING archetype=" << archetypeName(options.archetype)
               << " reward_rules=" << (options.brainContract
                    == gameplay::EnemyBrainContract::Cooperative ? 6 : 5)
               << " brain_contract=" << static_cast<unsigned>(options.brainContract)
               << " curriculum=" << curriculum
               << " resume_population=" << static_cast<bool>(fullResume)
               << " stage_immigrants=2"
               << " runtime_control_all=1"
               << " seed=" << seed
               << " population=" << options.population
               << " generations=" << options.generations
               << " cooldown_ms=" << options.cooldownMilliseconds
               << " start_generation=" << firstGeneration
               << " stages=" << scenarioCount
               << " scenarios=" << scenarioCount
               << " holdout_scenarios=" << holdoutScenarioCount
               << " repetitions=" << (options.trainAllScenarios
                    ? scenarioRepetitions * 2U : scenarioRepetitions) << '\n';
        for (std::uint32_t offset = 0; offset < options.generations; ++offset) {
            const std::uint32_t generation = firstGeneration + offset;
            // Every candidate in a generation sees the same fixtures, while
            // successive generations sample new deterministic maps and player
            // trajectories. Holdout seeds remain independent and unchanged.
            const wkc::core::Seed generationScenarios
                = wkc::core::deriveSeed(commonScenarios,
                    trainingScenarioDomain, generation);
            const std::size_t warmupOffset = generation - warmupStart;
            const std::size_t unlockedStages = warmupGenerations == 0U ? scenarioCount
                : std::min(scenarioCount, std::size_t{1}
                    + (warmupOffset * scenarioCount) / warmupGenerations);
            result.completedStages = static_cast<std::uint32_t>(unlockedStages);
            if (warmupGenerations > 0U && warmupOffset > 0U && population.size() >= 4U) {
                const std::size_t previousStages = std::min(scenarioCount,
                    std::size_t{1}
                        + ((warmupOffset - 1U) * scenarioCount) / warmupGenerations);
                if (unlockedStages > previousStages) {
                    for (std::size_t immigrant = 0U; immigrant < 2U;
                         ++immigrant) {
                        const std::size_t index
                            = population.size() - 2U + immigrant;
                        wkc::ai::Genome seeded = curriculumGenome(
                            options.archetype, generation,
                            static_cast<std::uint32_t>(index),
                            options.brainContract);
                        wkc::ai::mutate(seeded, wkc::core::deriveSeed(
                                populationSeed, initialBrainDomain,
                                (static_cast<std::uint64_t>(generation) << 8U)
                                    | immigrant),
                            0.04F, 0.06F);
                        population[index] = std::move(seeded);
                    }
                }
            }
            float total = 0.0F;
            for (wkc::ai::Genome& genome : population) {
                genome.fitness = evaluateGenome(
                    genome, generationScenarios, options.archetype,
                    false, options.trainAllScenarios, unlockedStages,
                    options.diverseMaps, options.brainContract).score;
                total += genome.fitness;
            }
            std::sort(population.begin(), population.end(),
                [](const wkc::ai::Genome& left, const wkc::ai::Genome& right) {
                    return left.fitness > right.fitness
                        || (left.fitness == right.fitness && left.id < right.id);
                });
            const float best = population.front().fitness;
            if (offset == 0U) {
                result.initialBest = best;
            }
            result.finalBest = best;
            result.lastGeneration = generation;
            result.champion = population.front();
            const WorkerEvaluation bestEvaluation
                = evaluateGenome(*result.champion, generationScenarios,
                    options.archetype, false, options.trainAllScenarios, scenarioCount,
                    options.diverseMaps, options.brainContract);
            const WorkerFitnessMetrics& bestMetrics = bestEvaluation.metrics;
            output << "AI_GENERATION number=" << generation
                   << " scenario_seed=" << generationScenarios
                   << " stage=" << unlockedStages << '/' << scenarioCount
                   << " best=" << std::fixed << std::setprecision(3) << best
                   << " full_score=" << bestEvaluation.score
                   << " mean=" << total / static_cast<float>(population.size())
                   << " worst_scenario=" << bestEvaluation.worstScenario
                   << " combat_hit_episodes="
                   << bestEvaluation.combatEpisodesWithHit
                   << " combat_competent=" << bestEvaluation.combatCompetent
                   << " damage_dealt=" << bestMetrics.damageDealt;
            appendStageDiagnostics(output, bestEvaluation);
            output << " damage_received=" << bestMetrics.damageReceived
                   << " fallback_damage_dealt=" << bestMetrics.fallbackDamageDealt
                   << " fallback_damage_received=" << bestMetrics.fallbackDamageReceived
                   << " hits=" << bestMetrics.attacksLanded
                   << " shots=" << bestMetrics.shotsFired
                   << " blocked_shots=" << bestMetrics.shotsBlocked
                   << " dodged_shots=" << bestMetrics.shotsDodged
                   << " reloads=" << bestMetrics.reloadsStarted
                   << " premature_reloads=" << bestMetrics.prematureReloads
                   << " dodge_opportunities=" << bestMetrics.dodgeOpportunities
                   << " dodges=" << bestMetrics.dodgesSucceeded
                   << " incoming_hits=" << bestMetrics.incomingProjectileHits
                   << " dodge_lateral_m=" << bestMetrics.dodgeLateralMeters
                   << " cover_entries=" << bestMetrics.coverEntries
                   << " cover_s=" << bestMetrics.coverSeconds
                   << " exposed_s=" << bestMetrics.exposedSeconds
                   << " mean_distance=" << (bestMetrics.distanceSamples > 0U
                        ? bestMetrics.distanceSum
                            / static_cast<float>(bestMetrics.distanceSamples)
                        : 0.0F)
                   << " range_error_s=" << bestMetrics.rangeErrorSeconds
                   << " real_map_episodes=" << bestMetrics.realMapEpisodes
                   << " large_room_episodes=" << bestMetrics.largeRoomEpisodes
                   << " corridor_spawns=" << bestMetrics.corridorSpawns
                   << " cover_map_episodes=" << bestMetrics.coverMapEpisodes
                   << " peek_map_episodes=" << bestMetrics.peekMapEpisodes
                   << " height_map_episodes=" << bestMetrics.heightMapEpisodes
                   << " long_map_episodes=" << bestMetrics.longMapEpisodes
                   << " boss_helper_episodes=" << bestMetrics.bossHelperEpisodes
                   << " fallback_hits=" << bestMetrics.fallbackAttacksLanded
                   << " fallback_shots=" << bestMetrics.fallbackShotsFired
                   << " range_progress=" << bestMetrics.tacticalProgressMeters
                   << " proximity_s=" << bestMetrics.proximitySeconds
                   << " contact_s=" << bestMetrics.contactSeconds
                   << " aim_s=" << bestMetrics.aimSeconds
                   << " boss_long_s=" << bestMetrics.bossLongRangeSeconds
                   << " boss_pressure_s=" << bestMetrics.bossPressureSeconds
                   << " boss_pursuit_s=" << bestMetrics.bossPursuitSeconds
                   << " survival_s=" << bestMetrics.survivalSeconds
                   << " idle_s=" << bestMetrics.idleSeconds
                   << " blocked_s=" << bestMetrics.blockedSeconds
                   << " wasted_attack_s=" << bestMetrics.wastedAttackSeconds
                   << " forward_credit_s=" << bestMetrics.forwardCreditSeconds
                   << " retreat_credit_s=" << bestMetrics.retreatCreditSeconds
                   << " strafe_credit_s=" << bestMetrics.strafeCreditSeconds
                   << " reversals=" << bestMetrics.unproductiveReversals
                   << " neural_ticks=" << bestMetrics.neuralTicks
                   << " fallback_ticks=" << bestMetrics.fallbackTicks
                   << " player_jumps=" << bestMetrics.playerJumps
                   << " player_peak_speed=" << bestMetrics.peakPlayerSpeed
                   << " kills=" << bestMetrics.playerEliminations
                   << " fallback_kills=" << bestMetrics.fallbackPlayerEliminations
                   << " deaths=" << bestMetrics.deaths
                   << " support=" << bestMetrics.supportActions;
            if (options.brainContract == gameplay::EnemyBrainContract::Cooperative) {
                output << " group_episodes=" << bestMetrics.groupEpisodes
                       << " friendly_hits=" << bestMetrics.friendlyHits
                       << " friendly_damage=" << bestMetrics.friendlyDamage
                       << " friendly_fire_holds="
                       << bestMetrics.friendlyFireHolds;
            }
            output << '\n';
            if (options.saveDirectory) {
                const std::filesystem::path path
                    = checkpointPath(*options.saveDirectory, generation,
                        options.archetype);
                if (!persistence::saveEnemyBrain(path,
                        {*result.champion, seed, options.archetype,
                            options.brainContract})) {
                    result.error = "failed to save enemy brain checkpoint";
                    return result;
                }
                result.lastCheckpoint = path;
                auto populationPath = path;
                populationPath.replace_extension(".population");
                if (!saveTrainingPopulation(populationPath,
                    {population, seed, options.archetype, generation, curriculum,
                        warmupStart, warmupGenerations,
                        options.brainContract})) {
                    result.error = "failed to save complete training population";
                    return result;
                }
                result.lastPopulation = populationPath;
            }
            output.flush();
            if (!output.good()) {
                result.error = "failed to write training progress";
                return result;
            }
            if (offset + 1U < options.generations) {
                population = nextPopulation(
                    population, populationSeed, generation + 1U,
                    options.brainContract);
                if (options.cooldownMilliseconds > 0U) {
                    std::this_thread::sleep_for(std::chrono::milliseconds{
                        options.cooldownMilliseconds});
                }
            }
        }
        const wkc::core::Seed holdoutSeed
            = wkc::core::deriveSeed(seed, holdoutScenarioDomain, 0U);
        const WorkerEvaluation holdout = evaluateGenome(
            *result.champion, holdoutSeed, options.archetype, true,
            options.trainAllScenarios, scenarioCount, options.diverseMaps,
            options.brainContract);
        result.holdoutFitness = holdout.score;
        output << "AI_RESULT generation=" << result.lastGeneration
               << " reward_rules=" << (options.brainContract
                    == gameplay::EnemyBrainContract::Cooperative ? 6 : 5)
               << " brain_contract=" << static_cast<unsigned>(options.brainContract)
               << " runtime_control_all=1"
               << " stages_completed=" << result.completedStages
               << " train_best=" << result.finalBest
               << " holdout=" << result.holdoutFitness
               << " holdout_combat_hit_episodes="
               << holdout.combatEpisodesWithHit
               << " holdout_combat_competent=" << holdout.combatCompetent
               << " holdout_hits=" << holdout.metrics.attacksLanded
               << " holdout_premature_reloads="
               << holdout.metrics.prematureReloads
               << " holdout_dodges=" << holdout.metrics.dodgesSucceeded
               << " holdout_dodge_opportunities="
               << holdout.metrics.dodgeOpportunities
               << " holdout_dodge_lateral_m="
               << holdout.metrics.dodgeLateralMeters;
        if (options.brainContract == gameplay::EnemyBrainContract::Cooperative) {
            output << " holdout_group_episodes=" << holdout.metrics.groupEpisodes
                   << " holdout_friendly_hits=" << holdout.metrics.friendlyHits
                   << " holdout_friendly_damage=" << holdout.metrics.friendlyDamage
                   << " holdout_friendly_fire_holds="
                   << holdout.metrics.friendlyFireHolds;
        }
        appendStageDiagnostics(output, holdout);
        if (result.lastCheckpoint) {
            output << " brain=" << result.lastCheckpoint->string();
        }
        output << '\n';
        result.succeeded = true;
    } catch (const std::exception& error) {
        result.error = error.what();
    }
    return result;
}

} // namespace gdp::game::ai
