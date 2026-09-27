#include "persistence/WorkerBrainStore.hpp"

#include "core/Hash.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

namespace gdp::persistence {
namespace {

constexpr std::size_t inputCount = 7;
constexpr std::size_t hiddenCount = 8;
constexpr std::size_t outputCount = 4;
constexpr std::size_t parameterCount = hiddenCount * (inputCount + 1)
    + outputCount * (hiddenCount + 1);

bool pathOccupied(const std::filesystem::path& path) noexcept
{
    std::error_code error;
    const std::filesystem::file_status status
        = std::filesystem::symlink_status(path, error);
    if (error == std::errc::no_such_file_or_directory) {
        return false;
    }
    return error || status.type() != std::filesystem::file_type::not_found;
}

bool validRecord(const WorkerBrainRecord& record) noexcept
{
    const auto& network = record.genome.network;
    if (network.inputCount() != inputCount || network.hiddenCount() != hiddenCount
        || network.outputCount() != outputCount
        || !std::isfinite(record.genome.fitness)
        || record.archetype > gdp::entities::EnemyArchetype::IndustrialRobot) {
        return false;
    }
    for (const float weight : network.parameters()) {
        if (!std::isfinite(weight) || weight < -1.0F || weight > 1.0F) {
            return false;
        }
    }
    return true;
}

std::string serialize(const WorkerBrainRecord& record)
{
    std::ostringstream output;
    output << "GDP_ENEMY_BRAIN 2\n"
           << "archetype " << static_cast<unsigned>(record.archetype) << '\n'
           << "seed " << record.trainingSeed << '\n'
           << "generation " << record.genome.generation << '\n'
           << "id " << record.genome.id << '\n'
           << "fitness_bits " << std::bit_cast<std::uint32_t>(record.genome.fitness)
           << '\n'
           << "weights " << parameterCount << '\n';
    for (const float weight : record.genome.network.parameters()) {
        output << std::bit_cast<std::uint32_t>(weight) << '\n';
    }
    const std::string body = output.str();
    output << "checksum " << wkc::core::fnv1a64(body) << '\n';
    return output.str();
}

std::optional<WorkerBrainRecord> parse(const std::string& content)
{
    const std::size_t checksumStart = content.rfind("checksum ");
    if (checksumStart == std::string::npos || checksumStart == 0U
        || content[checksumStart - 1U] != '\n') {
        return std::nullopt;
    }
    const std::string body = content.substr(0U, checksumStart);
    std::istringstream checksumInput{content.substr(checksumStart)};
    std::string checksumKey;
    std::uint64_t storedChecksum = 0;
    std::string extra;
    if (!(checksumInput >> checksumKey >> storedChecksum)
        || checksumKey != "checksum" || (checksumInput >> extra)
        || wkc::core::fnv1a64(body) != storedChecksum) {
        return std::nullopt;
    }

    std::istringstream input{body};
    std::string key;
    std::string magic;
    std::uint32_t version = 0;
    unsigned archetypeNumber = 0U;
    std::uint64_t seed = 0;
    std::uint32_t generation = 0;
    std::uint32_t id = 0;
    std::uint32_t fitnessBits = 0;
    std::size_t weightCount = 0;
    if (!(input >> magic >> version)
        || !((magic == "GDP_WORKER_BRAIN" && version == 1U)
            || (magic == "GDP_ENEMY_BRAIN" && version == 2U))) {
        return std::nullopt;
    }
    if (version == 2U && (!(input >> key >> archetypeNumber)
            || key != "archetype"
            || archetypeNumber > static_cast<unsigned>(
                gdp::entities::EnemyArchetype::IndustrialRobot))) {
        return std::nullopt;
    }
    if (!(input >> key >> seed) || key != "seed"
        || !(input >> key >> generation) || key != "generation"
        || !(input >> key >> id) || key != "id"
        || !(input >> key >> fitnessBits) || key != "fitness_bits"
        || !(input >> key >> weightCount) || key != "weights"
        || weightCount != parameterCount) {
        return std::nullopt;
    }
    WorkerBrainRecord record{
        wkc::ai::Genome{wkc::ai::NeuralNetwork{inputCount, hiddenCount, outputCount},
            std::bit_cast<float>(fitnessBits), generation, id},
        seed,
        static_cast<gdp::entities::EnemyArchetype>(archetypeNumber)};
    for (float& weight : record.genome.network.parameters()) {
        std::uint32_t bits = 0;
        if (!(input >> bits)) {
            return std::nullopt;
        }
        weight = std::bit_cast<float>(bits);
    }
    if ((input >> extra) || !validRecord(record)) {
        return std::nullopt;
    }
    return record;
}

} // namespace

std::optional<EnemyBrainRecord> loadEnemyBrain(
    const std::filesystem::path& path) noexcept
{
    try {
        std::ifstream input(path, std::ios::binary);
        if (!input.is_open()) {
            return std::nullopt;
        }
        input.seekg(0, std::ios::end);
        const std::streampos size = input.tellg();
        if (size < 0 || size > std::streampos{16'384}) {
            return std::nullopt;
        }
        input.seekg(0, std::ios::beg);
        std::string content(static_cast<std::size_t>(size), '\0');
        input.read(content.data(), static_cast<std::streamsize>(content.size()));
        if (!input || input.gcount() != static_cast<std::streamsize>(content.size())) {
            return std::nullopt;
        }
        return parse(content);
    } catch (...) {
        return std::nullopt;
    }
}

bool saveEnemyBrain(
    const std::filesystem::path& path,
    const WorkerBrainRecord& record) noexcept
{
    if (!validRecord(record) || path.empty()) {
        return false;
    }
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    try {
        std::error_code error;
        if (pathOccupied(path) || pathOccupied(temporary)) {
            return false;
        }
        const std::filesystem::path parent = path.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent, error);
            if (error) {
                return false;
            }
        }
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output.is_open()) {
                return false;
            }
            output << serialize(record);
            output.flush();
            if (!output.good()) {
                output.close();
                std::filesystem::remove(temporary, error);
                return false;
            }
        }
        const auto validation = loadEnemyBrain(temporary);
        if (!validation || validation->trainingSeed != record.trainingSeed
            || validation->archetype != record.archetype
            || validation->genome.generation != record.genome.generation
            || validation->genome.id != record.genome.id
            || validation->genome.fitness != record.genome.fitness
            || !std::equal(validation->genome.network.parameters().begin(),
                validation->genome.network.parameters().end(),
                record.genome.network.parameters().begin())) {
            std::filesystem::remove(temporary, error);
            return false;
        }
        std::filesystem::rename(temporary, path, error);
        if (error) {
            std::filesystem::remove(temporary, error);
            return false;
        }
        return true;
    } catch (...) {
        std::error_code error;
        std::filesystem::remove(temporary, error);
        return false;
    }
}

std::optional<WorkerBrainRecord> loadWorkerBrain(
    const std::filesystem::path& path) noexcept
{
    auto record = loadEnemyBrain(path);
    if (record && record->archetype == gdp::entities::EnemyArchetype::Worker) {
        return record;
    }
    return std::nullopt;
}

bool saveWorkerBrain(
    const std::filesystem::path& path,
    const WorkerBrainRecord& record) noexcept
{
    return record.archetype == gdp::entities::EnemyArchetype::Worker
        && saveEnemyBrain(path, record);
}

} // namespace gdp::persistence
