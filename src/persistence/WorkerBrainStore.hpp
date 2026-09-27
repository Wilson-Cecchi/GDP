#pragma once

#include "ai/Genome.hpp"

#include <filesystem>
#include <optional>

namespace gdp::persistence {

// Versioned, checksummed snapshot of the best experimental Worker genome.
// A snapshot can seed another training run or be loaded explicitly for a run.
struct WorkerBrainRecord {
    wkc::ai::Genome genome;
    wkc::core::Seed trainingSeed{0};
};

[[nodiscard]] std::optional<WorkerBrainRecord> loadWorkerBrain(
    const std::filesystem::path& path) noexcept;

// Never overwrites an existing destination or temporary file.
[[nodiscard]] bool saveWorkerBrain(
    const std::filesystem::path& path,
    const WorkerBrainRecord& record) noexcept;

} // namespace gdp::persistence
