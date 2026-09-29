#include "game/ai/WorkerTraining.hpp"

#include "core/Random.hpp"
#include "entities/EntityManager.hpp"
#include "gameplay/Combat.hpp"
#include "gameplay/Enemy.hpp"
#include "gameplay/Player.hpp"
#include "persistence/WorkerBrainStore.hpp"

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
#include <thread>
#include <utility>
#include <vector>

namespace gdp::game::ai {
namespace {

constexpr std::uint64_t initialBrainDomain = 0x4149425241494E01ULL;
constexpr std::uint64_t evolutionDomain = 0x414945564F4C5645ULL;
constexpr std::uint64_t trainingScenarioDomain = 0x41495343454E4554ULL;
constexpr std::uint64_t holdoutScenarioDomain = 0x41495343454E4548ULL;
constexpr float fixedStep = 1.0F / 60.0F;
constexpr int episodeSteps = 480;
constexpr int bossEpisodeSteps = 720;
constexpr std::size_t scenarioCount = 5U;
constexpr std::size_t holdoutScenarioCount = 2U;
constexpr std::size_t scenarioRepetitions = 2U;
constexpr std::size_t inputCount = 7;
constexpr std::size_t hiddenCount = 8;
constexpr std::size_t outputCount = 4;

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
    {PlayerPattern::Bhop, CoverPattern::Open, -20.0F, 0.52F, 1.4F},
    {PlayerPattern::Retreat, CoverPattern::Center, -50.0F, 0.52F, 1.1F},
    {PlayerPattern::Advance, CoverPattern::Right, 55.0F, 0.25F, 1.1F},
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

TrainingProfile profileFor(const EnemyArchetype archetype) noexcept
{
    switch (archetype) {
    case EnemyArchetype::Worker: return {3.4F, 4.6F, 50.0F, 0.0F, 2.0F, 5.0F};
    case EnemyArchetype::Guard: return {6.0F, 8.0F, 75.0F, 4.0F, 8.0F, 10.0F};
    case EnemyArchetype::Officer: return {7.0F, 9.0F, 70.0F, 6.0F, 10.0F, 11.0F};
    case EnemyArchetype::Engineer: return {5.0F, 7.0F, 70.0F, 3.0F, 6.0F, 10.0F};
    case EnemyArchetype::IndustrialRobot:
        return {9.0F, 11.0F, 220.0F, 4.0F, 13.0F, 14.0F};
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

gameplay::EnemyUpdateSummary updateTrainingEnemy(
    const EnemyArchetype archetype,
    entities::EntityManager& manager,
    gameplay::Player& player,
    const std::uint32_t roomId,
    const gameplay::CollisionWorld& arena,
    gameplay::NeuralEnemyExperiment& brain)
{
    switch (archetype) {
    case EnemyArchetype::Worker:
        return gameplay::updateWorkers(manager, player, roomId, arena,
            fixedStep, {}, {}, &brain);
    case EnemyArchetype::Guard:
        return gameplay::updateGuards(manager, player, roomId, arena,
            fixedStep, {}, {}, &brain);
    case EnemyArchetype::Officer:
        return gameplay::updateOfficers(manager, player, roomId, arena,
            fixedStep, {}, {}, &brain);
    case EnemyArchetype::Engineer:
        return gameplay::updateEngineers(manager, player, roomId, arena,
            fixedStep, {}, {}, &brain);
    case EnemyArchetype::IndustrialRobot:
        return gameplay::updateIndustrialRobots(manager, player, roomId, arena,
            fixedStep, {}, {}, &brain);
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

WorkerFitnessMetrics simulateEpisode(
    const wkc::ai::Genome& genome,
    const wkc::core::Seed seed,
    const std::size_t scenario,
    const EnemyArchetype archetype)
{
    const TrainingProfile profile = profileFor(archetype);
    const TrainingScenario& plan = scenarioFor(archetype, scenario);
    wkc::core::RandomStream random{seed};
    const float workerX = random.rangeFloat(-0.8F, 0.8F);
    const float workerZ = random.rangeFloat(profile.spawnMin, profile.spawnMax);
    const float sweep = random.rangeFloat(0.5F, 1.8F);
    const float direction = random.chance(0.5F) ? 1.0F : -1.0F;
    const float phase = random.rangeFloat(0.0F, 3.0F);
    const gameplay::CollisionWorld arena{
        {{-16.0F, 0.0F, -16.0F}, {16.0F, 4.0F, 16.0F}}, coverFor(plan.cover)};
    gameplay::Player player;
    player.spawn({0.0F, 0.0F, 0.0F});
    entities::EntityManager manager;
    constexpr std::uint32_t roomId = 1U;
    const entities::EntityId worker = manager.createEnemy({
        .transform = {.position = {workerX, 0.0F, workerZ},
            .rotationDegrees = {0.0F,
                180.0F + plan.facingOffsetDegrees, 0.0F}},
        .roomId = roomId,
        .archetype = archetype,
        .health = profile.health,
        .visualSeed = seed,
    });
    manager.findData<entities::EnemyData>(worker)->health
        = profile.health * plan.initialHealthRatio;
    std::optional<entities::EntityId> supportAlly;
    if (archetype == EnemyArchetype::Officer) {
        const auto ally = manager.createEnemy({
            .transform = {.position = {
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
            .transform = {.position = {
                scenario % 2U == 0U ? 2.0F : -3.0F, 0.0F,
                scenario % 3U == 0U ? 2.0F : 4.0F}},
            .roomId = roomId,
            .archetype = EnemyArchetype::Guard,
            .health = 35.0F,
            .visualSeed = seed,
        });
        manager.findData<entities::EnemyData>(ally)->health = 25.0F;
    }
    gameplay::NeuralEnemyExperiment brain{seed, archetype};
    brain.genome = genome;
    brain.logSelection = false;
    WorkerFitnessMetrics metrics;
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
        if (supportAlly) {
            entities::EnemyData* ally
                = manager.findData<entities::EnemyData>(*supportAlly);
            ally->attackCooldown = std::max(
                0.0F, ally->attackCooldown - fixedStep);
        }
        if (plan.player == PlayerPattern::Bhop) {
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
            const wkc::core::Vec3 nextPlayerPosition = playerPosition(
                plan.player, time, direction, sweep, phase);
            if (arena.canOccupy(nextPlayerPosition,
                    {.radius = 0.35F, .height = 1.8F})) {
                (void)player.relocate(nextPlayerPosition, -90.0F);
            }
        }
        const wkc::core::Vec3 before = entity->transform.position;
        const float previousAlignment = facingAlignment(before,
            entity->transform.rotationDegrees.y, player.state().position);
        const float playerHealthBefore = player.state().health;
        const std::uint64_t controlledBefore = brain.controlledTicks;
        const auto summary = updateTrainingEnemy(
            archetype, manager, player, roomId, arena, brain);
        const bool controlled = brain.controlledTicks != controlledBefore;
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
        totalTacticalProgress += tacticalProgress;
        if (controlled) {
            metrics.tacticalProgressMeters += tacticalProgress;
            metrics.damageDealt += std::max(
                0.0F, playerHealthBefore - player.state().health);
            metrics.attacksLanded += static_cast<std::uint32_t>(summary.attacksLanded);
        } else {
            metrics.fallbackDamageDealt += std::max(
                0.0F, playerHealthBefore - player.state().health);
            metrics.fallbackAttacksLanded += static_cast<std::uint32_t>(
                summary.attacksLanded);
        }
        metrics.supportActions += static_cast<std::uint32_t>(
            summary.commandsIssued + summary.repairsPerformed);
        metrics.bossLongRangeSeconds += static_cast<float>(
            summary.bossLongRange) * fixedStep;
        metrics.bossPressureSeconds += static_cast<float>(
            summary.bossPressure) * fixedStep;
        metrics.bossPursuitSeconds += static_cast<float>(
            summary.bossPursuit) * fixedStep;
        metrics.survivalSeconds += fixedStep;
        const bool playerVisible = gameplay::hasLineOfSight(
                entity->transform.position + wkc::core::Vec3{0.0F, 1.0F, 0.0F},
                player.cameraPosition(), arena);
        if (controlled) {
            const float forward = std::clamp(brain.outputs[0], -1.0F, 1.0F);
            const float strafe = std::clamp(brain.outputs[1], -1.0F, 1.0F);
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
            const bool underThreat = playerVisible
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
        if (controlled && distance <= currentProfile.preferredMaximum + 2.0F
            && playerVisible) {
            metrics.contactSeconds += fixedStep;
            if (archetype != EnemyArchetype::Worker && distance > 1.0e-5F) {
                constexpr float radiansPerDegree = 0.01745329251994329577F;
                constexpr float aimCosine = 0.9781476007F; // 12 degrees.
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
            brain.outputs[0], brain.outputs[1]);
        if (controlled && movement < 0.002F
            && rangeError(distance, currentProfile) > 0.5F
            && requestedMovement < 0.1F) {
            metrics.idleSeconds += fixedStep;
        }
        if (controlled && movement < 0.002F && requestedMovement > 0.3F) {
            metrics.blockedSeconds += fixedStep;
        }
        if (controlled && brain.outputs[3] > 0.65F
            && summary.attacking == 0U) {
            metrics.wastedAttackSeconds += fixedStep;
        }
        if (!player.state().alive) {
            if (controlled) {
                metrics.playerEliminations = 1U;
            } else {
                metrics.fallbackPlayerEliminations = 1U;
            }
            break;
        }
        shotCooldown -= fixedStep;
        if (shotCooldown <= 0.0F && distance <= profile.playerAttackRange
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
};

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
    total.bossLongRangeSeconds += sample.bossLongRangeSeconds;
    total.bossPressureSeconds += sample.bossPressureSeconds;
    total.bossPursuitSeconds += sample.bossPursuitSeconds;
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
    total.fallbackAttacksLanded += sample.fallbackAttacksLanded;
    total.playerEliminations += sample.playerEliminations;
    total.fallbackPlayerEliminations += sample.fallbackPlayerEliminations;
    total.deaths += sample.deaths;
    total.supportActions += sample.supportActions;
    total.unproductiveReversals += sample.unproductiveReversals;
    total.neuralTicks += sample.neuralTicks;
    total.fallbackTicks += sample.fallbackTicks;
    total.playerJumps += sample.playerJumps;
}

WorkerEvaluation evaluateGenome(
    const wkc::ai::Genome& genome,
    const wkc::core::Seed scenarioSeed,
    const EnemyArchetype archetype,
    const bool holdout = false,
    const bool trainAllScenarios = false)
{
    WorkerEvaluation evaluation;
    float worst = std::numeric_limits<float>::infinity();
    const std::size_t firstScenario
        = holdout && !trainAllScenarios ? scenarioCount : 0U;
    const std::size_t count = trainAllScenarios
        ? scenarioCount + holdoutScenarioCount
        : holdout ? holdoutScenarioCount : scenarioCount;
    for (std::size_t offset = 0U; offset < count; ++offset) {
        const std::size_t scenario = firstScenario + offset;
        for (std::size_t repetition = 0U;
             repetition < scenarioRepetitions; ++repetition) {
            const WorkerFitnessMetrics metrics = simulateEpisode(genome,
                wkc::core::deriveSeed(scenarioSeed, trainingScenarioDomain,
                    scenario * scenarioRepetitions + repetition), scenario,
                archetype);
            const float scenarioScore = scoreEnemyFitness(metrics, archetype);
            evaluation.score += scenarioScore;
            worst = std::min(worst, scenarioScore);
            addMetrics(evaluation.metrics, metrics);
        }
    }
    evaluation.worstScenario = worst;
    evaluation.score = 0.75F
        * (evaluation.score
            / static_cast<float>(count * scenarioRepetitions))
        + 0.25F * worst;
    return evaluation;
}

wkc::ai::Genome freshGenome(
    const wkc::core::Seed seed,
    const std::uint32_t generation,
    const std::uint32_t id)
{
    wkc::ai::Genome genome{
        wkc::ai::NeuralNetwork{inputCount, hiddenCount, outputCount},
        0.0F, generation, id};
    genome.network.randomize(seed, 0.5F);
    return genome;
}

wkc::ai::Genome curriculumGenome(
    const EnemyArchetype archetype,
    const std::uint32_t generation,
    const std::uint32_t id)
{
    wkc::ai::Genome genome{
        wkc::ai::NeuralNetwork{inputCount, hiddenCount, outputCount},
        0.0F, generation, id};
    auto parameters = genome.network.parameters();
    // Hidden 0 reacts to visibility, 1 turns toward signed target angle,
    // and 2 approaches when the target is far. The remaining neurons start
    // free for evolution; this is a baseline, not a scripted final policy.
    constexpr std::size_t hiddenStride = inputCount + 1U;
    constexpr std::size_t outputStride = hiddenCount + 1U;
    constexpr std::size_t outputBase = hiddenCount * hiddenStride;
    parameters[2] = 1.0F;
    parameters[hiddenStride + 1U] = -1.0F;
    parameters[2U * hiddenStride] = -1.0F;
    if (archetype == EnemyArchetype::Worker) {
        parameters[outputBase + hiddenCount] = 0.7F;
    } else {
        parameters[outputBase + 2U] = 0.8F;
    }
    parameters[outputBase + 2U * outputStride + 1U] = 1.0F;
    parameters[outputBase + 3U * outputStride] = 1.0F;
    parameters[outputBase + 3U * outputStride + hiddenCount] = 0.25F;
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
    const std::uint32_t generation)
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
                    static_cast<std::uint32_t>(index))
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
        || options.population < 2U || options.population > 128U
        || options.generations == 0U || options.generations > 10'000U
        || options.cooldownMilliseconds > 10'000U) {
        result.error = "population must be 2..128, generations 1..10000, "
            "and cooldown 0..10000 ms";
        return result;
    }
    try {
        wkc::core::Seed seed = options.seed;
        std::optional<persistence::EnemyBrainRecord> resume;
        std::uint32_t firstGeneration = 1U;
        if (options.resumeBrain) {
            resume = persistence::loadEnemyBrain(*options.resumeBrain);
            if (!resume || resume->archetype != options.archetype) {
                result.error = "cannot load a matching enemy brain for resume";
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
        for (std::uint32_t index = 0; index < options.population; ++index) {
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
                    options.archetype, firstGeneration, index);
                wkc::ai::mutate(seeded,
                    wkc::core::deriveSeed(populationSeed,
                        initialBrainDomain, index),
                    index == 0U ? 0.05F : 0.12F,
                    index == 0U ? 0.05F : 0.18F);
                population.push_back(std::move(seeded));
            } else {
                population.push_back(freshGenome(
                    wkc::core::deriveSeed(populationSeed, initialBrainDomain, index),
                    firstGeneration, index));
            }
        }
        const wkc::core::Seed commonScenarios = wkc::core::deriveSeed(
            seed, trainingScenarioDomain,
            static_cast<std::uint64_t>(options.archetype));
        output << "AI_TRAINING archetype=" << archetypeName(options.archetype)
               << " reward_rules=2"
               << " curriculum=" << (options.trainAllScenarios ? 3 : 2)
               << " seed=" << seed
               << " population=" << options.population
               << " generations=" << options.generations
               << " cooldown_ms=" << options.cooldownMilliseconds
               << " start_generation=" << firstGeneration
               << " scenarios=" << (options.trainAllScenarios
                    ? scenarioCount + holdoutScenarioCount : scenarioCount)
               << " holdout_scenarios=" << (options.trainAllScenarios
                    ? scenarioCount + holdoutScenarioCount : holdoutScenarioCount)
               << " repetitions=" << scenarioRepetitions << '\n';
        for (std::uint32_t offset = 0; offset < options.generations; ++offset) {
            const std::uint32_t generation = firstGeneration + offset;
            float total = 0.0F;
            for (wkc::ai::Genome& genome : population) {
                genome.fitness = evaluateGenome(
                    genome, commonScenarios, options.archetype,
                    false, options.trainAllScenarios).score;
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
                = evaluateGenome(*result.champion, commonScenarios,
                    options.archetype, false, options.trainAllScenarios);
            const WorkerFitnessMetrics& bestMetrics = bestEvaluation.metrics;
            output << "AI_GENERATION number=" << generation
                   << " best=" << std::fixed << std::setprecision(3) << best
                   << " mean=" << total / static_cast<float>(population.size())
                   << " worst_scenario=" << bestEvaluation.worstScenario
                   << " damage_dealt=" << bestMetrics.damageDealt
                   << " damage_received=" << bestMetrics.damageReceived
                   << " fallback_damage_dealt=" << bestMetrics.fallbackDamageDealt
                   << " fallback_damage_received=" << bestMetrics.fallbackDamageReceived
                   << " hits=" << bestMetrics.attacksLanded
                   << " fallback_hits=" << bestMetrics.fallbackAttacksLanded
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
                   << " support=" << bestMetrics.supportActions
                   << '\n';
            if (options.saveDirectory) {
                const std::filesystem::path path
                    = checkpointPath(*options.saveDirectory, generation,
                        options.archetype);
                if (!persistence::saveEnemyBrain(path,
                        {*result.champion, seed, options.archetype})) {
                    result.error = "failed to save enemy brain checkpoint";
                    return result;
                }
                result.lastCheckpoint = path;
            }
            output.flush();
            if (!output.good()) {
                result.error = "failed to write training progress";
                return result;
            }
            if (offset + 1U < options.generations) {
                population = nextPopulation(
                    population, populationSeed, generation + 1U);
                if (options.cooldownMilliseconds > 0U) {
                    std::this_thread::sleep_for(std::chrono::milliseconds{
                        options.cooldownMilliseconds});
                }
            }
        }
        const wkc::core::Seed holdoutSeed
            = wkc::core::deriveSeed(seed, holdoutScenarioDomain, 0U);
        result.holdoutFitness = evaluateGenome(
            *result.champion, holdoutSeed, options.archetype, true,
            options.trainAllScenarios).score;
        output << "AI_RESULT generation=" << result.lastGeneration
               << " reward_rules=2"
               << " train_best=" << result.finalBest
               << " holdout=" << result.holdoutFitness;
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
