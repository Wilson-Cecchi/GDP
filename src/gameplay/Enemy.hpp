#pragma once

#include "ai/Genome.hpp"
#include "entities/EntityManager.hpp"
#include "gameplay/Collision.hpp"
#include "gameplay/EnemyBrainContract.hpp"
#include "gameplay/EnemyProjectile.hpp"
#include "gameplay/Player.hpp"
#include "gameplay/Session.hpp"

#include <cstddef>
#include <cstdint>
#include <array>
#include <optional>
#include <unordered_map>

namespace gdp::gameplay {

struct PatrolConfig {
    float radius{2.0F};
    float speedScale{0.42F};
    float waypointTimeout{4.0F};
    float pauseDuration{0.55F};
    float arrivalDistance{0.28F};
};

struct InvestigationConfig {
    float duration{3.5F};
    float speedScale{0.78F};
    float arrivalDistance{0.35F};
    float hearingRange{10.0F};
    float alertDuration{0.18F};
    float recoverDuration{0.65F};
    float visionHalfAngleDegrees{65.0F};
    float searchRadius{0.0F};
    float maximumTravelBonus{0.0F};
};

struct WorkerConfig {
    float detectionRange{12.0F};
    float attackRange{1.15F};
    float moveSpeed{1.35F};
    float attackDamage{10.0F};
    float attackInterval{1.0F};
    PatrolConfig patrol{};
    InvestigationConfig investigation{
        3.5F, 0.78F, 0.35F, 10.0F, 0.18F, 2.8F, 65.0F, 0.9F, 2.5F};
};

struct GuardConfig {
    float detectionRange{16.0F};
    float preferredMinimumRange{4.0F};
    float preferredMaximumRange{8.0F};
    float moveSpeed{1.10F};
    float attackDamage{8.0F};
    float attackInterval{0.80F};
    PatrolConfig patrol{2.2F, 0.38F, 4.5F, 0.70F, 0.30F};
    InvestigationConfig investigation{
        4.5F, 0.72F, 0.38F, 15.0F, 0.24F, 3.2F, 75.0F, 1.1F, 3.0F};
};

struct OfficerConfig {
    float detectionRange{18.0F};
    float preferredMinimumRange{6.0F};
    float preferredMaximumRange{10.0F};
    float moveSpeed{0.95F};
    float attackDamage{6.0F};
    float attackInterval{1.20F};
    float supportRange{10.0F};
    float supportInterval{2.50F};
    float allyCooldownReduction{0.35F};
    PatrolConfig patrol{1.8F, 0.32F, 5.0F, 0.85F, 0.28F};
    InvestigationConfig investigation{
        5.0F, 0.65F, 0.40F, 17.0F, 0.20F, 3.4F, 80.0F, 1.2F, 3.5F};
};

struct EngineerConfig {
    float detectionRange{14.0F};
    float preferredMinimumRange{3.0F};
    float preferredMaximumRange{6.0F};
    float moveSpeed{1.05F};
    float attackDamage{5.0F};
    float attackInterval{1.0F};
    float repairRange{7.0F};
    float repairInterval{2.0F};
    float repairAmount{12.0F};
    PatrolConfig patrol{2.0F, 0.36F, 4.5F, 0.65F, 0.28F};
    InvestigationConfig investigation{
        4.0F, 0.68F, 0.38F, 13.0F, 0.28F, 3.0F, 70.0F, 1.0F, 3.0F};
};

enum class IndustrialRobotPhase : std::uint8_t {
    LongRange,
    Pressure,
    Pursuit,
};

struct IndustrialRobotConfig {
    float detectionRange{24.0F};
    float phaseTwoHealthRatio{0.66F};
    float phaseThreeHealthRatio{0.33F};
    float longRangeMinimum{8.0F};
    float longRangeMaximum{13.0F};
    float pressureMinimum{4.0F};
    float pressureMaximum{8.0F};
    float pursuitAttackRange{2.25F};
    float longRangeMoveSpeed{0.75F};
    float pressureMoveSpeed{1.0F};
    float pursuitMoveSpeed{1.35F};
    float longRangeDamage{12.0F};
    float pressureDamage{10.0F};
    float pursuitDamage{16.0F};
    float longRangeAttackInterval{1.40F};
    float pressureAttackInterval{0.90F};
    float pursuitAttackInterval{0.65F};
    PatrolConfig patrol{1.6F, 0.25F, 5.5F, 1.0F, 0.35F};
    InvestigationConfig investigation{
        6.0F, 0.58F, 0.48F, 22.0F, 0.34F, 4.0F, 85.0F, 1.4F, 5.0F};
};

struct EnemyDifficultyConfigs {
    WorkerConfig worker{};
    GuardConfig guard{};
    OfficerConfig officer{};
    EngineerConfig engineer{};
    IndustrialRobotConfig industrialRobot{};
};

[[nodiscard]] EnemyDifficultyConfigs makeEnemyDifficultyConfigs(
    float multiplier) noexcept;

struct EnemyNoiseStimulus {
    wkc::core::Vec3 position{};
    float radius{18.0F};
};

struct EnemyPerception {
    bool playerFlashlightOn{false};
    std::vector<EnemyProjectile>* projectiles{nullptr};
    bool allowAttacks{true};
    Difficulty difficulty{Difficulty::Normal};
};

struct NeuralEnemyMotorState {
    float decisionSecondsRemaining{0.0F};
    std::array<float, 5> outputs{};
    float ineffectiveSeconds{0.0F};
    float scriptedSecondsRemaining{0.0F};
};

// One trained genome can control every enemy of an archetype, with independent
// decision timing/output per entity. Evolution occurs only in headless training.
struct NeuralEnemyExperiment {
    explicit NeuralEnemyExperiment(wkc::core::Seed seed,
        gdp::entities::EnemyArchetype archetype
            = gdp::entities::EnemyArchetype::Worker,
        EnemyBrainContract contract = EnemyBrainContract::Legacy);

    wkc::ai::Genome genome;
    gdp::entities::EnemyArchetype archetype{gdp::entities::EnemyArchetype::Worker};
    EnemyBrainContract contract{EnemyBrainContract::Legacy};
    bool controlAll{false};
    bool logSelection{true};
    std::optional<gdp::entities::EntityId> selected;
    std::uint64_t controlledTicks{0U};
    float decisionSecondsRemaining{0.0F};
    std::array<float, 5> outputs{};
    std::unordered_map<std::uint64_t, NeuralEnemyMotorState> motors;
};

using NeuralWorkerExperiment = NeuralEnemyExperiment;

[[nodiscard]] std::size_t alertEnemiesToNoise(
    gdp::entities::EntityManager& manager,
    std::uint32_t roomId,
    EnemyNoiseStimulus stimulus,
    EnemyDifficultyConfigs configs = {}) noexcept;

struct EnemyUpdateSummary {
    std::size_t idle{0};
    std::size_t patrolling{0};
    std::size_t alerting{0};
    std::size_t investigating{0};
    std::size_t recovering{0};
    std::size_t chasing{0};
    std::size_t repositioning{0};
    std::size_t attacking{0};
    std::size_t attacksLanded{0};
    std::size_t shotsFired{0};
    std::size_t friendlyFireHolds{0};
    std::size_t reloadsStarted{0};
    std::size_t commandsIssued{0};
    std::size_t alliesSupported{0};
    std::size_t repairsPerformed{0};
    std::size_t alliesRepaired{0};
    std::size_t propsRestored{0};
    std::size_t bossLongRange{0};
    std::size_t bossPressure{0};
    std::size_t bossPursuit{0};
};

[[nodiscard]] EnemyUpdateSummary updateWorkers(
    gdp::entities::EntityManager& manager,
    Player& player,
    std::uint32_t roomId,
    const CollisionWorld& collisionWorld,
    float fixedStepSeconds,
    WorkerConfig config = {},
    EnemyPerception perception = {},
    NeuralEnemyExperiment* neuralExperiment = nullptr) noexcept;

[[nodiscard]] EnemyUpdateSummary updateGuards(
    gdp::entities::EntityManager& manager,
    Player& player,
    std::uint32_t roomId,
    const CollisionWorld& collisionWorld,
    float fixedStepSeconds,
    GuardConfig config = {},
    EnemyPerception perception = {},
    NeuralEnemyExperiment* neuralExperiment = nullptr) noexcept;

[[nodiscard]] EnemyUpdateSummary updateOfficers(
    gdp::entities::EntityManager& manager,
    Player& player,
    std::uint32_t roomId,
    const CollisionWorld& collisionWorld,
    float fixedStepSeconds,
    OfficerConfig config = {},
    EnemyPerception perception = {},
    NeuralEnemyExperiment* neuralExperiment = nullptr) noexcept;

[[nodiscard]] EnemyUpdateSummary updateEngineers(
    gdp::entities::EntityManager& manager,
    Player& player,
    std::uint32_t roomId,
    const CollisionWorld& collisionWorld,
    float fixedStepSeconds,
    EngineerConfig config = {},
    EnemyPerception perception = {},
    NeuralEnemyExperiment* neuralExperiment = nullptr) noexcept;

[[nodiscard]] IndustrialRobotPhase industrialRobotPhase(
    float health,
    float maximumHealth,
    IndustrialRobotConfig config = {}) noexcept;

[[nodiscard]] EnemyUpdateSummary updateIndustrialRobots(
    gdp::entities::EntityManager& manager,
    Player& player,
    std::uint32_t roomId,
    const CollisionWorld& collisionWorld,
    float fixedStepSeconds,
    IndustrialRobotConfig config = {},
    EnemyPerception perception = {},
    NeuralEnemyExperiment* neuralExperiment = nullptr) noexcept;

} // namespace gdp::gameplay
