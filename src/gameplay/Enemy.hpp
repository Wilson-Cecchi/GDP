#pragma once

#include "entities/EntityManager.hpp"
#include "gameplay/Collision.hpp"
#include "gameplay/Player.hpp"

#include <cstddef>
#include <cstdint>

namespace gdp::gameplay {

struct WorkerConfig {
    float detectionRange{12.0F};
    float attackRange{1.15F};
    float moveSpeed{1.35F};
    float attackDamage{10.0F};
    float attackInterval{1.0F};
};

struct GuardConfig {
    float detectionRange{16.0F};
    float preferredMinimumRange{4.0F};
    float preferredMaximumRange{8.0F};
    float moveSpeed{1.10F};
    float attackDamage{8.0F};
    float attackInterval{0.80F};
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

struct EnemyUpdateSummary {
    std::size_t idle{0};
    std::size_t chasing{0};
    std::size_t repositioning{0};
    std::size_t attacking{0};
    std::size_t attacksLanded{0};
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
    WorkerConfig config = {}) noexcept;

[[nodiscard]] EnemyUpdateSummary updateGuards(
    gdp::entities::EntityManager& manager,
    Player& player,
    std::uint32_t roomId,
    const CollisionWorld& collisionWorld,
    float fixedStepSeconds,
    GuardConfig config = {}) noexcept;

[[nodiscard]] EnemyUpdateSummary updateOfficers(
    gdp::entities::EntityManager& manager,
    Player& player,
    std::uint32_t roomId,
    const CollisionWorld& collisionWorld,
    float fixedStepSeconds,
    OfficerConfig config = {}) noexcept;

[[nodiscard]] EnemyUpdateSummary updateEngineers(
    gdp::entities::EntityManager& manager,
    Player& player,
    std::uint32_t roomId,
    const CollisionWorld& collisionWorld,
    float fixedStepSeconds,
    EngineerConfig config = {}) noexcept;

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
    IndustrialRobotConfig config = {}) noexcept;

} // namespace gdp::gameplay
