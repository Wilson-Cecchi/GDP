#include "game/ai/WorkerTraining.hpp"

#include <algorithm>
#include <cmath>

namespace gdp::game::ai {
namespace {

struct RewardProfile {
    float maximumHealth;
    float damage;
    float hit;
    float progress;
    float range;
    float contact;
    float aim;
    float received;
    float idle;
    float blocked;
    float wastedAttack;
    float reversal;
};

RewardProfile rewardProfile(const gdp::entities::EnemyArchetype archetype) noexcept
{
    using gdp::entities::EnemyArchetype;
    switch (archetype) {
    case EnemyArchetype::Worker:
        return {50.0F, 3.2F, 0.20F, 0.55F, 0.12F, 0.08F, 0.0F,
            2.5F, 0.12F, 0.30F, 0.15F, 0.20F};
    case EnemyArchetype::Guard:
        return {75.0F, 3.0F, 0.20F, 0.50F, 0.12F, 0.06F, 0.20F,
            2.7F, 0.12F, 0.30F, 0.18F, 0.20F};
    case EnemyArchetype::Officer:
        return {70.0F, 2.9F, 0.18F, 0.48F, 0.14F, 0.06F, 0.18F,
            2.8F, 0.12F, 0.30F, 0.18F, 0.20F};
    case EnemyArchetype::Engineer:
        return {70.0F, 2.7F, 0.18F, 0.48F, 0.12F, 0.05F, 0.16F,
            3.0F, 0.12F, 0.30F, 0.18F, 0.20F};
    case EnemyArchetype::IndustrialRobot:
        return {220.0F, 3.1F, 0.16F, 0.52F, 0.15F, 0.06F, 0.20F,
            2.2F, 0.12F, 0.32F, 0.20F, 0.20F};
    }
    return {};
}

} // namespace

float creditedTacticalProgress(
    const float neuralProgress, const float totalProgress) noexcept
{
    // Fallback can undo an earlier approach, but the network must not earn
    // the same approach over and over by recovering from that regression.
    return neuralProgress > 0.0F
        ? std::min(neuralProgress, std::max(totalProgress, 0.0F))
        : neuralProgress;
}

DirectionalCredit directionalMovementCredit(
    const float forward,
    const float strafe,
    const float durationSeconds) noexcept
{
    const float forwardMagnitude = std::abs(std::clamp(forward, -1.0F, 1.0F));
    const float strafeMagnitude = std::abs(std::clamp(strafe, -1.0F, 1.0F));
    const float total = forwardMagnitude + strafeMagnitude;
    if (total <= 1.0e-5F || durationSeconds <= 0.0F) {
        return {};
    }
    const float forwardShare = forwardMagnitude / total * durationSeconds;
    return {
        forward > 0.0F ? forwardShare : 0.0F,
        forward < 0.0F ? forwardShare : 0.0F,
        strafeMagnitude / total * durationSeconds,
    };
}

float scoreWorkerFitness(const WorkerFitnessMetrics& metrics) noexcept
{
    return scoreEnemyFitness(metrics, gdp::entities::EnemyArchetype::Worker);
}

float scoreEnemyFitness(
    const WorkerFitnessMetrics& metrics,
    const gdp::entities::EnemyArchetype archetype) noexcept
{
    const RewardProfile profile = rewardProfile(archetype);
    if (profile.maximumHealth <= 0.0F) {
        return 0.0F;
    }
    const float damageFraction = std::clamp(
        metrics.damageDealt / 100.0F, 0.0F, 1.0F);
    const float receivedFraction = std::clamp(
        (metrics.damageReceived + metrics.fallbackDamageReceived)
            / profile.maximumHealth, 0.0F, 1.0F);
    const std::uint32_t misses = metrics.shotsFired > metrics.attacksLanded
        ? metrics.shotsFired - metrics.attacksLanded : 0U;
    // Offensive credit is neural-authored. Damage taken is a whole-episode
    // consequence, even when a fallback tick delivers the final hit.
    // Survival alone, pressing forward and automatic support give no points.
    const float auxiliary = damageFraction * profile.damage
        + static_cast<float>(std::min(metrics.attacksLanded, 4U)) * profile.hit
        + std::clamp(metrics.tacticalProgressMeters, -4.0F, 4.0F)
            * profile.progress
        + std::min(metrics.proximitySeconds, 8.0F) * profile.range
        + std::min(metrics.contactSeconds, 8.0F) * profile.contact
        + std::min(metrics.aimSeconds, 4.0F) * profile.aim
        - receivedFraction * profile.received
        - std::min(metrics.idleSeconds, 8.0F) * profile.idle
        - std::min(metrics.blockedSeconds, 8.0F) * profile.blocked
        - std::min(metrics.wastedAttackSeconds, 8.0F) * profile.wastedAttack
        - static_cast<float>(std::min(misses, 8U)) * 0.06F
        - static_cast<float>(std::min(metrics.shotsBlocked, 8U)) * 0.14F
        - static_cast<float>(std::min(metrics.prematureReloads, 6U)) * 0.55F
        - static_cast<float>(std::min(metrics.unproductiveReversals, 8U))
            * profile.reversal;
    // An entire episode of shaping cannot outweigh either terminal event.
    const float terminal = 10.0F
        * static_cast<float>(std::min(metrics.playerEliminations, 1U))
        - 10.0F * static_cast<float>(std::min(metrics.deaths, 1U));
    return terminal + std::clamp(auxiliary, -4.0F, 4.0F);
}

float scoreTrainingStage(const WorkerFitnessMetrics& metrics,
    const gdp::entities::EnemyArchetype archetype,
    const TrainingStage stage) noexcept
{
    const float progress = std::clamp(metrics.tacticalProgressMeters, -4.0F, 4.0F);
    const float stuck = std::min(metrics.blockedSeconds, 8.0F);
    switch (stage) {
    case TrainingStage::Locomotion:
        return std::clamp(progress * 0.75F
            - stuck * 0.3F - std::min(metrics.idleSeconds, 8.0F) * 0.1F,
            -4.0F, 4.0F);
    case TrainingStage::Aim:
        return std::clamp(std::min(metrics.aimSeconds, 8.0F) * 0.35F
            + std::min(metrics.contactSeconds, 8.0F) * 0.08F
            - stuck * 0.25F, -4.0F, 4.0F);
    case TrainingStage::FireControl: {
        const auto misses = metrics.shotsFired > metrics.attacksLanded
            ? metrics.shotsFired - metrics.attacksLanded : 0U;
        return std::clamp(std::min(metrics.damageDealt / 100.0F, 1.0F) * 3.0F
            + static_cast<float>(std::min(metrics.attacksLanded, 4U)) * 0.22F
            - static_cast<float>(std::min(misses, 8U)) * 0.08F
            - static_cast<float>(std::min(metrics.shotsBlocked, 8U)) * 0.18F
            - static_cast<float>(std::min(metrics.prematureReloads, 6U)) * 0.55F
            - std::min(metrics.wastedAttackSeconds, 8.0F) * 0.08F,
            -4.0F, 4.0F);
    }
    case TrainingStage::Dodge: {
        // A failed but lateral near-miss supplies a bounded learning gradient.
        // Successful avoidance remains worth more than any single hit, and
        // movement without a projectile hit cannot earn this partial credit.
        const float nearMiss = metrics.incomingProjectileHits > 0U
            ? std::min(metrics.dodgeLateralMeters
                    / static_cast<float>(metrics.incomingProjectileHits), 0.6F)
                / 0.6F * 0.7F
            : 0.0F;
        return std::clamp(
            static_cast<float>(std::min(metrics.dodgesSucceeded, 6U)) * 0.55F
            - static_cast<float>(std::min(metrics.incomingProjectileHits, 6U))
                * 0.45F
            + nearMiss
            - static_cast<float>(std::min(metrics.unproductiveReversals, 8U))
                * 0.08F, -4.0F, 4.0F);
    }
    case TrainingStage::Duel:
    case TrainingStage::RealMaps:
        // Survival and proximity are not a substitute for doing the job of a
        // melee attacker or a ranged support unit in a combat episode.
        return (archetype == gdp::entities::EnemyArchetype::Worker
                || archetype == gdp::entities::EnemyArchetype::Engineer)
                && metrics.attacksLanded == 0U
            ? std::min(scoreEnemyFitness(metrics, archetype), -1.5F)
            : scoreEnemyFitness(metrics, archetype);
    case TrainingStage::Positioning: {
        const float combat = scoreEnemyFitness(metrics, archetype)
            + (metrics.attacksLanded > 0U
                ? static_cast<float>(std::min(metrics.coverEntries, 1U))
                    * 0.35F : 0.0F);
        return (archetype == gdp::entities::EnemyArchetype::Worker
                || archetype == gdp::entities::EnemyArchetype::Engineer)
                && metrics.attacksLanded == 0U
            ? std::min(combat, -1.5F) : combat;
    }
    }
    return 0.0F;
}

} // namespace gdp::game::ai
