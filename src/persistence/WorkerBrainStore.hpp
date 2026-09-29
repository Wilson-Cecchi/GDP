#pragma once

#include "ai/Genome.hpp"
#include "entities/EntityManager.hpp"
#include "gameplay/EnemyBrainContract.hpp"

#include <filesystem>
#include <optional>

namespace gdp::persistence {

// Versioned, checksummed snapshot of the best experimental enemy genome.
// A snapshot can seed another training run or be loaded explicitly for a run.
struct WorkerBrainRecord {
    wkc::ai::Genome genome;
    wkc::core::Seed trainingSeed{0};
    gdp::entities::EnemyArchetype archetype{gdp::entities::EnemyArchetype::Worker};
    gdp::gameplay::EnemyBrainContract contract{
        gdp::gameplay::EnemyBrainContract::Legacy};
};

using EnemyBrainRecord = WorkerBrainRecord;

[[nodiscard]] std::optional<EnemyBrainRecord> loadEnemyBrain(
    const std::filesystem::path& path) noexcept;
[[nodiscard]] bool saveEnemyBrain(
    const std::filesystem::path& path,
    const EnemyBrainRecord& record) noexcept;

[[nodiscard]] std::optional<WorkerBrainRecord> loadWorkerBrain(
    const std::filesystem::path& path) noexcept;

// Never overwrites an existing destination or temporary file.
[[nodiscard]] bool saveWorkerBrain(
    const std::filesystem::path& path,
    const WorkerBrainRecord& record) noexcept;

} // namespace gdp::persistence
