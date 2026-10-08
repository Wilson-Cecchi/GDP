#pragma once

#include "ai/Genome.hpp"
#include "entities/EntityManager.hpp"
#include "gameplay/EnemyBrainContract.hpp"

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>

namespace gdp::game::ai {

enum class TrainingStage : std::uint8_t {
    Locomotion = 1,
    Aim,
    FireControl,
    Dodge,
    Duel,
    Positioning,
    RealMaps,
};

struct WorkerFitnessMetrics {
    float damageDealt{0.0F};
    float damageReceived{0.0F};
    float fallbackDamageDealt{0.0F};
    float fallbackDamageReceived{0.0F};
    float survivalSeconds{0.0F};
    float tacticalProgressMeters{0.0F};
    float proximitySeconds{0.0F};
    float idleSeconds{0.0F};
    float contactSeconds{0.0F};
    float aimSeconds{0.0F};
    float exposedSeconds{0.0F};
    float coverSeconds{0.0F};
    float rangeErrorSeconds{0.0F};
    float distanceSum{0.0F};
    float bossLongRangeSeconds{0.0F};
    float bossPressureSeconds{0.0F};
    float bossPursuitSeconds{0.0F};
    float blockedSeconds{0.0F};
    float wastedAttackSeconds{0.0F};
    float dodgeLateralMeters{0.0F};
    float forwardCreditSeconds{0.0F};
    float retreatCreditSeconds{0.0F};
    float strafeCreditSeconds{0.0F};
    float peakPlayerSpeed{0.0F};
    std::uint32_t attacksLanded{0};
    std::uint32_t shotsFired{0};
    std::uint32_t shotsBlocked{0};
    std::uint32_t shotsDodged{0};
    std::uint32_t reloadsStarted{0};
    std::uint32_t prematureReloads{0};
    std::uint32_t dodgeOpportunities{0};
    std::uint32_t dodgesSucceeded{0};
    std::uint32_t incomingProjectileHits{0};
    std::uint32_t coverEntries{0};
    std::uint32_t distanceSamples{0};
    std::uint32_t realMapEpisodes{0};
    std::uint32_t corridorSpawns{0};
    std::uint32_t coverMapEpisodes{0};
    std::uint32_t peekMapEpisodes{0};
    std::uint32_t heightMapEpisodes{0};
    std::uint32_t fallbackAttacksLanded{0};
    std::uint32_t fallbackShotsFired{0};
    std::uint32_t playerEliminations{0};
    std::uint32_t fallbackPlayerEliminations{0};
    std::uint32_t deaths{0};
    std::uint32_t supportActions{0};
    std::uint32_t unproductiveReversals{0};
    std::uint32_t neuralTicks{0};
    std::uint32_t fallbackTicks{0};
    std::uint32_t playerJumps{0};
    std::uint32_t groupEpisodes{0};
    std::uint32_t friendlyHits{0};
    std::uint32_t friendlyFireHolds{0};
    float friendlyDamage{0.0F};
};

struct DirectionalCredit {
    float forward{0.0F};
    float retreat{0.0F};
    float strafe{0.0F};
};

[[nodiscard]] float scoreWorkerFitness(const WorkerFitnessMetrics& metrics) noexcept;
[[nodiscard]] float scoreEnemyFitness(
    const WorkerFitnessMetrics& metrics,
    gdp::entities::EnemyArchetype archetype) noexcept;
[[nodiscard]] float scoreTrainingStage(
    const WorkerFitnessMetrics& metrics,
    gdp::entities::EnemyArchetype archetype,
    TrainingStage stage) noexcept;
[[nodiscard]] float scoreCooperativeTrainingStage(
    const WorkerFitnessMetrics& metrics,
    gdp::entities::EnemyArchetype archetype,
    TrainingStage stage) noexcept;
[[nodiscard]] float creditedTacticalProgress(
    float neuralProgress, float totalProgress) noexcept;
[[nodiscard]] DirectionalCredit directionalMovementCredit(
    float forward, float strafe, float durationSeconds) noexcept;
[[nodiscard]] float evaluateWorkerGenome(
    const wkc::ai::Genome& genome,
    wkc::core::Seed scenarioSeed);

struct WorkerTrainingOptions {
    gdp::entities::EnemyArchetype archetype{gdp::entities::EnemyArchetype::Worker};
    wkc::core::Seed seed{12'345U};
    bool seedExplicit{false};
    bool trainAllScenarios{false};
    bool diverseMaps{false};
    gameplay::EnemyBrainContract brainContract{
        gameplay::EnemyBrainContract::Tactical};
    std::uint32_t curriculumGenerations{0U}; // Zero uses first run length.
    std::uint32_t population{16U};
    std::uint32_t generations{10U}; // Additional generations when resuming.
    std::uint32_t cooldownMilliseconds{0U};
    std::optional<std::filesystem::path> saveDirectory;
    std::optional<std::filesystem::path> resumeBrain;
    std::optional<std::filesystem::path> resumePopulation;
};

struct WorkerTrainingResult {
    bool succeeded{false};
    std::string error;
    std::optional<wkc::ai::Genome> champion;
    float initialBest{0.0F};
    float finalBest{0.0F};
    float holdoutFitness{0.0F};
    std::uint32_t completedStages{0U};
    std::optional<std::filesystem::path> lastCheckpoint;
    std::uint32_t lastGeneration{0U};
    std::optional<std::filesystem::path> lastPopulation;
};

[[nodiscard]] WorkerTrainingResult runWorkerTraining(
    const WorkerTrainingOptions& options,
    std::ostream& output);

[[nodiscard]] WorkerTrainingResult runEnemyTraining(
    const WorkerTrainingOptions& options,
    std::ostream& output);

} // namespace gdp::game::ai
