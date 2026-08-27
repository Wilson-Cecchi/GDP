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

struct EnemyUpdateSummary {
    std::size_t idle{0};
    std::size_t chasing{0};
    std::size_t repositioning{0};
    std::size_t attacking{0};
    std::size_t attacksLanded{0};
    std::size_t commandsIssued{0};
    std::size_t alliesSupported{0};
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

} // namespace gdp::gameplay
