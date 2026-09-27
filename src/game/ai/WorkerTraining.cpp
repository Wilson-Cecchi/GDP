#include "game/ai/WorkerTraining.hpp"

#include "core/Random.hpp"
#include "entities/EntityManager.hpp"
#include "gameplay/Combat.hpp"
#include "gameplay/Enemy.hpp"
#include "gameplay/Player.hpp"
#include "persistence/WorkerBrainStore.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <limits>
#include <optional>
#include <ostream>
#include <sstream>
#include <utility>
#include <vector>

namespace gdp::game::ai {
namespace {

constexpr std::uint64_t initialBrainDomain = 0x4149425241494E01ULL;
constexpr std::uint64_t evolutionDomain = 0x414945564F4C5645ULL;
constexpr std::uint64_t trainingScenarioDomain = 0x41495343454E4554ULL;
constexpr std::uint64_t holdoutScenarioDomain = 0x41495343454E4548ULL;
constexpr float fixedStep = 1.0F / 60.0F;
constexpr int episodeSteps = 480;
constexpr std::size_t scenarioCount = 4U;
constexpr std::size_t scenarioRepetitions = 2U;
constexpr std::size_t inputCount = 7;
constexpr std::size_t hiddenCount = 8;
constexpr std::size_t outputCount = 4;

float planarDistance(
    const wkc::core::Vec3 first,
    const wkc::core::Vec3 second) noexcept
{
    const float x = first.x - second.x;
    const float z = first.z - second.z;
    return std::sqrt(x * x + z * z);
}

WorkerFitnessMetrics simulateEpisode(
    const wkc::ai::Genome& genome,
    const wkc::core::Seed seed,
    const std::size_t scenario)
{
    wkc::core::RandomStream random{seed};
    const float workerX = random.rangeFloat(-0.8F, 0.8F);
    const float workerZ = random.rangeFloat(3.4F, 4.6F);
    const float sweep = random.rangeFloat(0.5F, 1.8F);
    const float direction = random.chance(0.5F) ? 1.0F : -1.0F;
    const float phase = random.rangeFloat(0.0F, 3.0F);
    std::vector<wkc::core::Aabb> obstacles;
    if (scenario == 1U) {
        obstacles.push_back({{-1.5F, 0.0F, 1.4F}, {-0.45F, 2.4F, 2.7F}});
    } else if (scenario == 2U) {
        obstacles.push_back({{0.45F, 0.0F, 1.4F}, {1.5F, 2.4F, 2.7F}});
    }
    const gameplay::CollisionWorld arena{
        {{-8.0F, 0.0F, -8.0F}, {8.0F, 4.0F, 8.0F}}, std::move(obstacles)};
    gameplay::Player player;
    player.spawn({0.0F, 0.0F, 0.0F});
    entities::EntityManager manager;
    constexpr std::uint32_t roomId = 1U;
    const entities::EntityId worker = manager.createEnemy({
        .transform = {.position = {workerX, 0.0F, workerZ},
            .rotationDegrees = {0.0F, 180.0F, 0.0F}},
        .roomId = roomId,
        .archetype = entities::EnemyArchetype::Worker,
        .health = 50.0F,
        .visualSeed = seed,
    });
    gameplay::NeuralWorkerExperiment brain{seed};
    brain.genome = genome;
    brain.logSelection = false;
    WorkerFitnessMetrics metrics;
    const float initialDistance = planarDistance(
        manager.find(worker)->transform.position, player.state().position);
    float closestDistance = initialDistance;
    float shotCooldown = 1.0F;
    for (int tick = 0; tick < episodeSteps; ++tick) {
        entities::Entity* entity = manager.find(worker);
        entities::EnemyData* enemy = manager.findData<entities::EnemyData>(worker);
        if (entity == nullptr || enemy == nullptr
            || enemy->behavior == entities::EnemyBehaviorState::Dead
            || !player.state().alive) {
            break;
        }
        const float time = static_cast<float>(tick) * fixedStep;
        const float playerX = direction * sweep * std::sin(time * 0.7F + phase);
        const float playerZ = scenario == 3U
            ? -std::min(time * 0.25F, 1.8F) : 0.0F;
        (void)player.relocate({playerX, 0.0F, playerZ}, -90.0F);
        const wkc::core::Vec3 before = entity->transform.position;
        const float playerHealthBefore = player.state().health;
        const auto summary = gameplay::updateWorkers(
            manager, player, roomId, arena, fixedStep, {}, {}, &brain);
        const float movement = planarDistance(before, entity->transform.position);
        const float distance = planarDistance(
            entity->transform.position, player.state().position);
        closestDistance = std::min(closestDistance, distance);
        metrics.damageDealt += std::max(
            0.0F, playerHealthBefore - player.state().health);
        metrics.attacksLanded += static_cast<std::uint32_t>(summary.attacksLanded);
        metrics.survivalSeconds += fixedStep;
        const bool playerVisible = gameplay::hasLineOfSight(
                entity->transform.position + wkc::core::Vec3{0.0F, 1.0F, 0.0F},
                player.cameraPosition(), arena);
        if (distance <= 2.0F && playerVisible) {
            metrics.proximitySeconds += fixedStep;
        }
        if (distance <= 6.0F && playerVisible) {
            metrics.contactSeconds += fixedStep;
        }
        const float requestedMovement = std::hypot(
            brain.outputs[0], brain.outputs[1]);
        if (movement < 0.002F && distance > 1.5F
            && requestedMovement < 0.1F) {
            metrics.idleSeconds += fixedStep;
        }
        if (movement < 0.002F && requestedMovement > 0.3F) {
            metrics.blockedSeconds += fixedStep;
        }
        if (brain.outputs[3] > 0.65F && summary.attacking == 0U) {
            metrics.wastedAttackSeconds += fixedStep;
        }
        shotCooldown -= fixedStep;
        if (shotCooldown <= 0.0F && distance <= 5.0F
            && gameplay::hasLineOfSight(
                player.cameraPosition(),
                entity->transform.position
                    + wkc::core::Vec3{0.0F, 1.0F, 0.0F}, arena)) {
            const float healthBefore = enemy->health;
            (void)gameplay::applyDamage(manager, worker, 8.0F);
            metrics.damageReceived += healthBefore - enemy->health;
            shotCooldown = 1.4F;
        }
    }
    metrics.approachMeters = std::max(0.0F, initialDistance - closestDistance);
    if (const entities::EnemyData* enemy
        = manager.findData<entities::EnemyData>(worker);
        enemy != nullptr && enemy->behavior == entities::EnemyBehaviorState::Dead) {
        metrics.deaths = 1U;
    }
    return metrics;
}

struct WorkerEvaluation {
    float score{0.0F};
    float worstScenario{0.0F};
    WorkerFitnessMetrics metrics{};
};

void addMetrics(WorkerFitnessMetrics& total, const WorkerFitnessMetrics& sample)
{
    total.damageDealt += sample.damageDealt;
    total.damageReceived += sample.damageReceived;
    total.survivalSeconds += sample.survivalSeconds;
    total.approachMeters += sample.approachMeters;
    total.proximitySeconds += sample.proximitySeconds;
    total.idleSeconds += sample.idleSeconds;
    total.contactSeconds += sample.contactSeconds;
    total.blockedSeconds += sample.blockedSeconds;
    total.wastedAttackSeconds += sample.wastedAttackSeconds;
    total.attacksLanded += sample.attacksLanded;
    total.deaths += sample.deaths;
}

WorkerEvaluation evaluateGenome(
    const wkc::ai::Genome& genome,
    const wkc::core::Seed scenarioSeed)
{
    WorkerEvaluation evaluation;
    float worst = std::numeric_limits<float>::infinity();
    for (std::size_t scenario = 0U; scenario < scenarioCount; ++scenario) {
        for (std::size_t repetition = 0U;
             repetition < scenarioRepetitions; ++repetition) {
            const WorkerFitnessMetrics metrics = simulateEpisode(genome,
                wkc::core::deriveSeed(scenarioSeed, trainingScenarioDomain,
                    scenario * scenarioRepetitions + repetition), scenario);
            const float scenarioScore = scoreWorkerFitness(metrics);
            evaluation.score += scenarioScore;
            worst = std::min(worst, scenarioScore);
            addMetrics(evaluation.metrics, metrics);
        }
    }
    evaluation.worstScenario = worst;
    evaluation.score = 0.75F
        * (evaluation.score
            / static_cast<float>(scenarioCount * scenarioRepetitions))
        + 0.25F * worst;
    return evaluation;
}

wkc::ai::Genome freshGenome(
    const wkc::core::Seed seed,
    const std::uint32_t generation,
    const std::uint32_t id)
{
    wkc::ai::Genome genome{
        wkc::ai::NeuralNetwork{inputCount, hiddenCount, outputCount},
        0.0F, generation, id};
    genome.network.randomize(seed, 0.5F);
    return genome;
}

std::size_t tournamentWinner(
    const std::vector<wkc::ai::Genome>& population,
    wkc::core::RandomStream& random)
{
    std::size_t winner = static_cast<std::size_t>(
        random.rangeInt(0, static_cast<int>(population.size()) - 1));
    for (int contestant = 1; contestant < 3; ++contestant) {
        const std::size_t candidate = static_cast<std::size_t>(
            random.rangeInt(0, static_cast<int>(population.size()) - 1));
        if (population[candidate].fitness > population[winner].fitness) {
            winner = candidate;
        }
    }
    return winner;
}

std::vector<wkc::ai::Genome> nextPopulation(
    const std::vector<wkc::ai::Genome>& ranked,
    const wkc::core::Seed seed,
    const std::uint32_t generation)
{
    wkc::core::RandomStream random{
        wkc::core::deriveSeed(seed, evolutionDomain, generation)};
    std::vector<wkc::ai::Genome> next;
    next.reserve(ranked.size());
    const std::size_t eliteCount = std::max<std::size_t>(1, ranked.size() / 10U);
    for (std::size_t index = 0; index < ranked.size(); ++index) {
        wkc::ai::Genome child = index < eliteCount
            ? ranked[index]
            : random.chance(0.10F)
                ? freshGenome(random.nextU64(), generation,
                    static_cast<std::uint32_t>(index))
                : ranked[tournamentWinner(ranked, random)];
        if (index >= eliteCount && child.generation != generation) {
            wkc::ai::mutate(child, random.nextU64(), 0.12F, 0.18F);
        }
        child.generation = generation;
        child.id = static_cast<std::uint32_t>(index);
        child.fitness = 0.0F;
        next.push_back(std::move(child));
    }
    return next;
}

std::filesystem::path checkpointPath(
    const std::filesystem::path& directory,
    const std::uint32_t generation)
{
    std::ostringstream name;
    name << "worker_generation_" << std::setw(6) << std::setfill('0')
         << generation << ".brain";
    return directory / name.str();
}

bool prepareOutputDirectory(const std::filesystem::path& directory)
{
    std::error_code error;
    if (directory.empty()) {
        return false;
    }
    if (std::filesystem::exists(directory, error)) {
        return !error && std::filesystem::is_directory(directory, error)
            && !error && std::filesystem::is_empty(directory, error) && !error;
    }
    return !error && std::filesystem::create_directories(directory, error) && !error;
}

} // namespace

float scoreWorkerFitness(const WorkerFitnessMetrics& metrics) noexcept
{
    // Combat is the primary objective; bounded shaping helps early evolution
    // without making waiting, wall-running or blind attacks profitable.
    return metrics.damageDealt * 4.0F
        + static_cast<float>(metrics.attacksLanded) * 2.0F
        + std::min(metrics.approachMeters, 6.0F) * 1.5F
        + std::min(metrics.proximitySeconds, 8.0F) * 0.5F
        + std::min(metrics.contactSeconds, 8.0F) * 0.25F
        + metrics.survivalSeconds * 0.1F
        - metrics.damageReceived * 0.35F
        - metrics.idleSeconds * 0.5F
        - metrics.blockedSeconds * 1.5F
        - metrics.wastedAttackSeconds * 0.6F
        - static_cast<float>(metrics.deaths) * 8.0F;
}

float evaluateWorkerGenome(
    const wkc::ai::Genome& genome,
    const wkc::core::Seed scenarioSeed)
{
    return evaluateGenome(genome, scenarioSeed).score;
}

WorkerTrainingResult runWorkerTraining(
    const WorkerTrainingOptions& options,
    std::ostream& output)
{
    WorkerTrainingResult result;
    if (options.population < 2U || options.population > 128U
        || options.generations == 0U || options.generations > 10'000U) {
        result.error = "population must be 2..128 and generations must be 1..10000";
        return result;
    }
    try {
        wkc::core::Seed seed = options.seed;
        std::optional<persistence::WorkerBrainRecord> resume;
        std::uint32_t firstGeneration = 1U;
        if (options.resumeBrain) {
            resume = persistence::loadWorkerBrain(*options.resumeBrain);
            if (!resume) {
                result.error = "cannot load a valid Worker brain for resume";
                return result;
            }
            if (options.seedExplicit && seed != resume->trainingSeed) {
                result.error = "resume brain seed does not match --seed";
                return result;
            }
            seed = resume->trainingSeed;
            if (resume->genome.generation
                > std::numeric_limits<std::uint32_t>::max() - options.generations) {
                result.error = "generation number would overflow";
                return result;
            }
            firstGeneration = resume->genome.generation + 1U;
        }
        if (options.saveDirectory
            && !prepareOutputDirectory(*options.saveDirectory)) {
            result.error = "save directory must be new or empty";
            return result;
        }
        std::vector<wkc::ai::Genome> population;
        population.reserve(options.population);
        for (std::uint32_t index = 0; index < options.population; ++index) {
            if (resume && index == 0U) {
                wkc::ai::Genome seeded = resume->genome;
                seeded.generation = firstGeneration;
                seeded.id = index;
                seeded.fitness = 0.0F;
                population.push_back(std::move(seeded));
            } else if (resume) {
                wkc::ai::Genome seeded = resume->genome;
                wkc::ai::mutate(seeded,
                    wkc::core::deriveSeed(seed, initialBrainDomain,
                        (static_cast<std::uint64_t>(firstGeneration) << 32U) | index),
                    0.12F, 0.18F);
                seeded.generation = firstGeneration;
                seeded.id = index;
                population.push_back(std::move(seeded));
            } else {
                population.push_back(freshGenome(
                    wkc::core::deriveSeed(seed, initialBrainDomain, index),
                    firstGeneration, index));
            }
        }
        const wkc::core::Seed commonScenarios
            = wkc::core::deriveSeed(seed, trainingScenarioDomain, 0U);
        output << "AI_TRAINING archetype=WORKER seed=" << seed
               << " population=" << options.population
               << " generations=" << options.generations
               << " start_generation=" << firstGeneration
               << " scenarios=" << scenarioCount
               << " repetitions=" << scenarioRepetitions << '\n';
        for (std::uint32_t offset = 0; offset < options.generations; ++offset) {
            const std::uint32_t generation = firstGeneration + offset;
            float total = 0.0F;
            for (wkc::ai::Genome& genome : population) {
                genome.fitness = evaluateWorkerGenome(genome, commonScenarios);
                total += genome.fitness;
            }
            std::sort(population.begin(), population.end(),
                [](const wkc::ai::Genome& left, const wkc::ai::Genome& right) {
                    return left.fitness > right.fitness
                        || (left.fitness == right.fitness && left.id < right.id);
                });
            const float best = population.front().fitness;
            if (offset == 0U) {
                result.initialBest = best;
            }
            result.finalBest = best;
            result.lastGeneration = generation;
            result.champion = population.front();
            const WorkerEvaluation bestEvaluation
                = evaluateGenome(*result.champion, commonScenarios);
            const WorkerFitnessMetrics& bestMetrics = bestEvaluation.metrics;
            output << "AI_GENERATION number=" << generation
                   << " best=" << std::fixed << std::setprecision(3) << best
                   << " mean=" << total / static_cast<float>(population.size())
                   << " worst_scenario=" << bestEvaluation.worstScenario
                   << " damage_dealt=" << bestMetrics.damageDealt
                   << " damage_received=" << bestMetrics.damageReceived
                   << " hits=" << bestMetrics.attacksLanded
                   << " approach=" << bestMetrics.approachMeters
                   << " proximity_s=" << bestMetrics.proximitySeconds
                   << " contact_s=" << bestMetrics.contactSeconds
                   << " survival_s=" << bestMetrics.survivalSeconds
                   << " idle_s=" << bestMetrics.idleSeconds
                   << " blocked_s=" << bestMetrics.blockedSeconds
                   << " wasted_attack_s=" << bestMetrics.wastedAttackSeconds
                   << " deaths=" << bestMetrics.deaths
                   << '\n';
            if (options.saveDirectory) {
                const std::filesystem::path path
                    = checkpointPath(*options.saveDirectory, generation);
                if (!persistence::saveWorkerBrain(path,
                        {*result.champion, seed})) {
                    result.error = "failed to save Worker brain checkpoint";
                    return result;
                }
                result.lastCheckpoint = path;
            }
            output.flush();
            if (!output.good()) {
                result.error = "failed to write training progress";
                return result;
            }
            if (offset + 1U < options.generations) {
                population = nextPopulation(population, seed, generation + 1U);
            }
        }
        const wkc::core::Seed holdoutSeed
            = wkc::core::deriveSeed(seed, holdoutScenarioDomain, 0U);
        result.holdoutFitness = evaluateWorkerGenome(*result.champion, holdoutSeed);
        output << "AI_RESULT generation=" << result.lastGeneration
               << " train_best=" << result.finalBest
               << " holdout=" << result.holdoutFitness;
        if (result.lastCheckpoint) {
            output << " brain=" << result.lastCheckpoint->string();
        }
        output << '\n';
        result.succeeded = true;
    } catch (const std::exception& error) {
        result.error = error.what();
    }
    return result;
}

} // namespace gdp::game::ai
