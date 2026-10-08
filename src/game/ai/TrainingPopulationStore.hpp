#pragma once

#include "ai/Genome.hpp"
#include "entities/EntityManager.hpp"
#include "gameplay/EnemyBrainContract.hpp"
#include <filesystem>
#include <optional>
#include <vector>

namespace gdp::game::ai {
// Ranked, evaluated population. Random operations use seed + generation domains,
// so no live RNG state is needed. Warmup progress survives process boundaries.
struct TrainingPopulationRecord {
    std::vector<wkc::ai::Genome> population;
    wkc::core::Seed seed{0};
    entities::EnemyArchetype archetype{entities::EnemyArchetype::Worker};
    std::uint32_t generation{0};
    std::uint32_t curriculum{13};
    std::uint32_t warmupStart{1};
    std::uint32_t warmupGenerations{0}; // Zero means all stages unlocked.
    gameplay::EnemyBrainContract contract{gameplay::EnemyBrainContract::Tactical};
};
[[nodiscard]] std::optional<TrainingPopulationRecord> loadTrainingPopulation(
    const std::filesystem::path& path) noexcept;
// Writes members and checksummed metadata to a private directory, validates,
// then publishes the complete directory with one rename. Never overwrites.
[[nodiscard]] bool saveTrainingPopulation(const std::filesystem::path& path,
    const TrainingPopulationRecord& record) noexcept;
} // namespace gdp::game::ai
