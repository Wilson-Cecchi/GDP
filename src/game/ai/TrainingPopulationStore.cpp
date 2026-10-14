#include "game/ai/TrainingPopulationStore.hpp"
#include "core/Hash.hpp"
#include "persistence/WorkerBrainStore.hpp"
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>

namespace gdp::game::ai {
namespace {
std::filesystem::path memberPath(const std::filesystem::path& path, std::size_t index)
{
    std::ostringstream name;
    name << "member-" << std::setw(3) << std::setfill('0') << index << ".brain";
    return path / name.str();
}
}
std::optional<TrainingPopulationRecord> loadTrainingPopulation(
    const std::filesystem::path& path) noexcept
{
    try {
        std::ifstream file(path / "state.txt", std::ios::binary);
        if (!file) return std::nullopt;
        file.seekg(0, std::ios::end);
        const auto size = file.tellg();
        if (size < 0 || size > std::streampos{4096}) return std::nullopt;
        file.seekg(0);
        std::string content(static_cast<std::size_t>(size), '\0');
        file.read(content.data(), static_cast<std::streamsize>(content.size()));
        if (!file) return std::nullopt;
        const auto checksumAt = content.rfind("checksum ");
        if (checksumAt == std::string::npos || checksumAt == 0
            || content[checksumAt - 1] != '\n') return std::nullopt;
        const std::string body = content.substr(0, checksumAt);
        std::istringstream checksumInput{content.substr(checksumAt)};
        std::string key, extra;
        std::uint64_t checksum = 0;
        if (!(checksumInput >> key >> checksum) || key != "checksum"
            || (checksumInput >> extra) || checksum != wkc::core::fnv1a64(body))
            return std::nullopt;
        std::istringstream input{body};
        TrainingPopulationRecord record;
        unsigned version = 0, archetype = 0, count = 0, rules = 0;
        if (!(input >> key >> version) || key != "GDP_TRAINING_POPULATION"
            || (version != 1 && version != 2)
            || !(input >> key >> archetype) || key != "archetype" || archetype > 4
            || !(input >> key >> record.seed) || key != "seed"
            || !(input >> key >> record.generation) || key != "generation" || record.generation == 0
            || !(input >> key >> count) || key != "population" || count < 2 || count > 128
            || !(input >> key >> rules) || key != "reward_rules"
                || rules != (version == 2 ? 6U : 5U))
            return std::nullopt;
        if (version == 2) {
            unsigned contract = 0;
            if (!(input >> key >> contract) || key != "contract"
                || contract != static_cast<unsigned>(
                    gameplay::EnemyBrainContract::Cooperative))
                return std::nullopt;
            record.contract = gameplay::EnemyBrainContract::Cooperative;
        }
        if (!(input >> key >> record.curriculum) || key != "curriculum"
                || record.curriculum < 12 || record.curriculum > 19
            || !(input >> key >> record.warmupStart) || key != "warmup_start"
                || record.warmupStart == 0 || record.warmupStart > record.generation
            || !(input >> key >> record.warmupGenerations) || key != "warmup_generations"
                || record.warmupGenerations > 10000 || (input >> extra))
            return std::nullopt;
        record.archetype = static_cast<entities::EnemyArchetype>(archetype);
        std::set<std::uint32_t> ids;
        for (unsigned index = 0; index < count; ++index) {
            auto member = persistence::loadEnemyBrain(memberPath(path, index));
            if (!member || member->trainingSeed != record.seed
                || member->archetype != record.archetype
                || member->contract != record.contract
                || member->genome.generation != record.generation
                || member->genome.id >= count || !ids.insert(member->genome.id).second)
                return std::nullopt;
            record.population.push_back(std::move(member->genome));
        }
        // Preserved order is the selection order used by the next generation.
        for (std::size_t index = 1; index < record.population.size(); ++index) {
            const auto& before = record.population[index - 1];
            const auto& after = record.population[index];
            if (before.fitness < after.fitness
                || (before.fitness == after.fitness && before.id > after.id))
                return std::nullopt;
        }
        return record;
    } catch (...) { return std::nullopt; }
}
bool saveTrainingPopulation(const std::filesystem::path& path,
    const TrainingPopulationRecord& record) noexcept
{
    auto temporary = path;
    temporary += ".tmp";
    bool ownsTemporary = false;
    try {
        if (path.empty() || (record.contract != gameplay::EnemyBrainContract::Tactical
                && record.contract != gameplay::EnemyBrainContract::Cooperative)
            || std::filesystem::exists(path)
            || !std::filesystem::create_directory(temporary)) return false;
        ownsTemporary = true;
        for (std::size_t index = 0; index < record.population.size(); ++index) {
            if (!persistence::saveEnemyBrain(memberPath(temporary, index),
                {record.population[index], record.seed, record.archetype,
                    record.contract})) {
                std::filesystem::remove_all(temporary);
                return false;
            }
        }
        std::ostringstream body;
        const bool cooperative
            = record.contract == gameplay::EnemyBrainContract::Cooperative;
        body << "GDP_TRAINING_POPULATION " << (cooperative ? 2 : 1)
            << "\narchetype " << static_cast<unsigned>(record.archetype)
            << "\nseed " << record.seed << "\ngeneration " << record.generation
            << "\npopulation " << record.population.size()
            << "\nreward_rules " << (cooperative ? 6 : 5) << '\n';
        if (cooperative) {
            body << "contract " << static_cast<unsigned>(record.contract) << '\n';
        }
        body << "curriculum " << record.curriculum
            << "\nwarmup_start " << record.warmupStart
            << "\nwarmup_generations " << record.warmupGenerations << '\n';
        {
            std::ofstream output(temporary / "state.txt", std::ios::binary);
            output << body.str() << "checksum " << wkc::core::fnv1a64(body.str()) << '\n';
            output.flush();
            if (!output.good()) {
                output.close(); std::filesystem::remove_all(temporary); return false;
            }
        }
        if (!loadTrainingPopulation(temporary)) {
            std::filesystem::remove_all(temporary); return false;
        }
        std::filesystem::rename(temporary, path);
        return true;
    } catch (...) {
        std::error_code error;
        if (ownsTemporary) std::filesystem::remove_all(temporary, error);
        return false;
    }
}
} // namespace gdp::game::ai
