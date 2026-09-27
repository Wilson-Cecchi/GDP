#pragma once

#include "ai/Genome.hpp"

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>

namespace gdp::game::ai {

struct WorkerFitnessMetrics {
    float damageDealt{0.0F};
    float damageReceived{0.0F};
    float survivalSeconds{0.0F};
    float approachMeters{0.0F};
    float proximitySeconds{0.0F};
    float idleSeconds{0.0F};
    float contactSeconds{0.0F};
    float blockedSeconds{0.0F};
    float wastedAttackSeconds{0.0F};
    std::uint32_t attacksLanded{0};
    std::uint32_t deaths{0};
};

[[nodiscard]] float scoreWorkerFitness(const WorkerFitnessMetrics& metrics) noexcept;
[[nodiscard]] float evaluateWorkerGenome(
    const wkc::ai::Genome& genome,
    wkc::core::Seed scenarioSeed);

struct WorkerTrainingOptions {
    wkc::core::Seed seed{12'345U};
    bool seedExplicit{false};
    std::uint32_t population{16U};
    std::uint32_t generations{10U}; // Additional generations when resuming.
    std::optional<std::filesystem::path> saveDirectory;
    std::optional<std::filesystem::path> resumeBrain;
};

struct WorkerTrainingResult {
    bool succeeded{false};
    std::string error;
    std::optional<wkc::ai::Genome> champion;
    float initialBest{0.0F};
    float finalBest{0.0F};
    float holdoutFitness{0.0F};
    std::optional<std::filesystem::path> lastCheckpoint;
    std::uint32_t lastGeneration{0U};
};

[[nodiscard]] WorkerTrainingResult runWorkerTraining(
    const WorkerTrainingOptions& options,
    std::ostream& output);

} // namespace gdp::game::ai
