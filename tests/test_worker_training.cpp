#include "game/ai/WorkerTraining.hpp"

#include "core/Hash.hpp"
#include "persistence/WorkerBrainStore.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void testFitness()
{
    using namespace gdp::game::ai;
    const WorkerFitnessMetrics baseline{
        .damageDealt = 10.0F,
        .damageReceived = 5.0F,
        .survivalSeconds = 3.0F,
        .approachMeters = 2.0F,
        .proximitySeconds = 1.0F,
        .idleSeconds = 1.0F,
        .attacksLanded = 1U,
    };
    WorkerFitnessMetrics better = baseline;
    better.damageDealt += 10.0F;
    better.attacksLanded += 1U;
    expect(scoreWorkerFitness(better) > scoreWorkerFitness(baseline),
        "damage and landed attacks increase fitness");
    WorkerFitnessMetrics worse = baseline;
    worse.damageReceived += 10.0F;
    expect(scoreWorkerFitness(worse) < scoreWorkerFitness(baseline),
        "damage received reduces fitness");
    worse = baseline;
    worse.idleSeconds += 2.0F;
    expect(scoreWorkerFitness(worse) < scoreWorkerFitness(baseline),
        "idling reduces fitness");
    worse = baseline;
    worse.blockedSeconds += 1.0F;
    expect(scoreWorkerFitness(worse) < scoreWorkerFitness(baseline),
        "blocked movement reduces fitness");
    worse = baseline;
    worse.wastedAttackSeconds += 1.0F;
    expect(scoreWorkerFitness(worse) < scoreWorkerFitness(baseline),
        "attacking without a chance to hit reduces fitness");
    worse = baseline;
    worse.deaths = 1U;
    expect(scoreWorkerFitness(worse) < scoreWorkerFitness(baseline),
        "death reduces fitness");
    WorkerFitnessMetrics contact = baseline;
    contact.contactSeconds += 2.0F;
    expect(scoreWorkerFitness(contact) > scoreWorkerFitness(baseline),
        "useful visual contact increases fitness");
    WorkerFitnessMetrics capped = contact;
    capped.contactSeconds = 100.0F;
    expect(scoreWorkerFitness(capped) - scoreWorkerFitness(contact) <= 2.0F,
        "contact reward is bounded so camping cannot dominate combat");
    const auto boss = gdp::entities::EnemyArchetype::IndustrialRobot;
    WorkerFitnessMetrics robotHit = baseline;
    robotHit.damageDealt += 10.0F;
    expect(scoreEnemyFitness(robotHit, boss) - scoreEnemyFitness(baseline, boss)
            < scoreWorkerFitness(robotHit) - scoreWorkerFitness(baseline),
        "boss damage is scaled to avoid dominating its own fitness");
    WorkerFitnessMetrics robotDeath = baseline;
    robotDeath.deaths = 1U;
    expect(scoreEnemyFitness(baseline, boss)
            - scoreEnemyFitness(robotDeath, boss) > 8.0F,
        "boss death has a stronger penalty");
}

void testTrainingAndPersistence()
{
    namespace fs = std::filesystem;
    using namespace gdp::game::ai;
    const fs::path root = fs::temp_directory_path()
        / ("gdp_worker_training_test_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code error;
    if (!fs::create_directory(root, error) || error) {
        expect(false, "test owns a unique temporary directory");
        return;
    }
    const fs::path firstDirectory = root / "first";
    WorkerTrainingOptions options{
        .seed = 42U,
        .seedExplicit = true,
        .population = 4U,
        .generations = 2U,
        .saveDirectory = firstDirectory,
        .resumeBrain = std::nullopt,
    };
    std::ostringstream output;
    const WorkerTrainingResult first = runWorkerTraining(options, output);
    expect(first.succeeded && first.champion && first.lastCheckpoint
            && first.lastGeneration == 2U,
        "automated training produces a champion and final checkpoint");
    expect(output.str().find("scenarios=4") != std::string::npos
            && output.str().find("repetitions=2") != std::string::npos
            && output.str().find("worst_scenario=") != std::string::npos
            && output.str().find("blocked_s=") != std::string::npos
            && output.str().find("wasted_attack_s=") != std::string::npos,
        "training reports scenario count and reward-penalty evidence");
    if (first.succeeded && first.champion && first.lastCheckpoint) {
        expect(first.finalBest >= first.initialBest - 1e-4F,
            "elitism preserves the best score on fixed scenarios");
        const auto saved = gdp::persistence::loadWorkerBrain(*first.lastCheckpoint);
        expect(saved && saved->trainingSeed == options.seed
                && saved->genome.generation == 2U,
            "checkpoint loads with seed and generation");
        if (saved) {
            expect(std::equal(saved->genome.network.parameters().begin(),
                    saved->genome.network.parameters().end(),
                    first.champion->network.parameters().begin()),
                "checkpoint preserves every weight exactly");
        }
        std::ostringstream legacyBody;
        legacyBody << "GDP_WORKER_BRAIN 1\nseed " << options.seed
                   << "\ngeneration " << first.champion->generation
                   << "\nid " << first.champion->id
                   << "\nfitness_bits "
                   << std::bit_cast<std::uint32_t>(first.champion->fitness)
                   << "\nweights "
                   << first.champion->network.parameters().size() << '\n';
        for (const float weight : first.champion->network.parameters()) {
            legacyBody << std::bit_cast<std::uint32_t>(weight) << '\n';
        }
        const fs::path legacyPath = root / "legacy_worker.brain";
        {
            std::ofstream legacyFile(legacyPath, std::ios::binary);
            legacyFile << legacyBody.str() << "checksum "
                       << wkc::core::fnv1a64(legacyBody.str()) << '\n';
        }
        const auto legacy = gdp::persistence::loadWorkerBrain(legacyPath);
        expect(legacy && legacy->archetype
                == gdp::entities::EnemyArchetype::Worker,
            "version-one Worker checkpoints remain loadable");
        expect(!gdp::persistence::saveWorkerBrain(
                *first.lastCheckpoint, {*first.champion, options.seed}),
            "saving never overwrites an existing checkpoint");
        wkc::ai::Genome invalidGenome = *first.champion;
        invalidGenome.network.parameters()[0] = 2.0F;
        expect(!gdp::persistence::saveWorkerBrain(
                root / "invalid.brain", {invalidGenome, options.seed}),
            "out-of-range weights cannot be saved");
        const fs::path danglingLink = root / "dangling.brain";
        fs::create_symlink(root / "absent.brain", danglingLink, error);
        if (!error) {
            expect(!gdp::persistence::saveWorkerBrain(
                    danglingLink, {*first.champion, options.seed})
                    && fs::is_symlink(danglingLink),
                "saving never replaces a dangling symbolic link");
        }
        error.clear();

        WorkerTrainingOptions repeated = options;
        repeated.saveDirectory.reset();
        std::ostringstream repeatedOutput;
        const WorkerTrainingResult same = runWorkerTraining(repeated, repeatedOutput);
        expect(same.succeeded && same.champion
                && same.initialBest == first.initialBest
                && same.finalBest == first.finalBest
                && same.holdoutFitness == first.holdoutFitness,
            "fixed seed repeats training and holdout scores");
        if (same.champion) {
            expect(std::equal(same.champion->network.parameters().begin(),
                    same.champion->network.parameters().end(),
                    first.champion->network.parameters().begin()),
                "fixed seed repeats the champion weights");
        }
        WorkerTrainingOptions different = repeated;
        different.seed = 43U;
        std::ostringstream differentOutput;
        const WorkerTrainingResult other = runWorkerTraining(
            different, differentOutput);
        expect(other.succeeded && other.champion
                && !std::equal(other.champion->network.parameters().begin(),
                    other.champion->network.parameters().end(),
                    first.champion->network.parameters().begin()),
            "a different seed changes the trained champion");

        WorkerTrainingOptions resume = options;
        resume.generations = 1U;
        resume.seedExplicit = false;
        resume.resumeBrain = *first.lastCheckpoint;
        resume.saveDirectory = root / "resumed";
        std::ostringstream resumeOutput;
        const WorkerTrainingResult continued = runWorkerTraining(resume, resumeOutput);
        expect(continued.succeeded && continued.lastGeneration == 3U
                && continued.lastCheckpoint,
            "resume starts from the next generation in a new directory");

        const fs::path corrupt = root / "corrupt.brain";
        fs::copy_file(*first.lastCheckpoint, corrupt, fs::copy_options::none, error);
        if (!error) {
            std::ofstream appended(corrupt, std::ios::app);
            appended << "tampered\n";
            appended.close();
            expect(!gdp::persistence::loadWorkerBrain(corrupt),
                "checksum or trailing-data corruption invalidates a brain");
        } else {
            expect(false, "test can create a corrupt copy");
        }

        std::ostringstream conflictOutput;
        const WorkerTrainingResult conflict = runWorkerTraining(options, conflictOutput);
        expect(!conflict.succeeded,
            "training refuses to write into a nonempty save directory");
    }
    fs::remove_all(root, error);
    expect(!error, "temporary training files are cleaned up");
}

void testOtherArchetypes()
{
    namespace fs = std::filesystem;
    using namespace gdp::game::ai;
    using gdp::entities::EnemyArchetype;
    const fs::path root = fs::temp_directory_path()
        / ("gdp_all_enemy_training_test_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code error;
    if (!fs::create_directory(root, error) || error) {
        expect(false, "all-archetype test owns a unique temporary directory");
        return;
    }
    for (const EnemyArchetype archetype : {
             EnemyArchetype::Guard, EnemyArchetype::Officer,
             EnemyArchetype::Engineer, EnemyArchetype::IndustrialRobot}) {
        WorkerTrainingOptions options;
        options.archetype = archetype;
        options.seed = 42U;
        options.population = 3U;
        options.generations = 2U;
        options.saveDirectory = root / std::to_string(
            static_cast<unsigned>(archetype));
        std::ostringstream output;
        const WorkerTrainingResult trained = runEnemyTraining(options, output);
        expect(trained.succeeded && trained.lastCheckpoint
                && trained.champion && trained.lastGeneration == 2U,
            "each enemy archetype trains and saves a brain");
        if (!trained.lastCheckpoint) continue;
        const auto saved = gdp::persistence::loadEnemyBrain(*trained.lastCheckpoint);
        expect(saved && saved->archetype == archetype
                && saved->trainingSeed == options.seed,
            "checkpoint identifies its enemy archetype and seed");
        expect(!gdp::persistence::loadWorkerBrain(*trained.lastCheckpoint),
            "Worker-only loader rejects another archetype");
        WorkerTrainingOptions mismatched = options;
        mismatched.archetype = EnemyArchetype::Worker;
        mismatched.resumeBrain = *trained.lastCheckpoint;
        mismatched.saveDirectory.reset();
        std::ostringstream rejectedOutput;
        expect(!runEnemyTraining(mismatched, rejectedOutput).succeeded,
            "resume refuses a brain belonging to another archetype");
        if (archetype == EnemyArchetype::Guard) {
            WorkerTrainingOptions resumed = options;
            resumed.generations = 1U;
            resumed.resumeBrain = *trained.lastCheckpoint;
            resumed.saveDirectory = root / "guard_resumed";
            std::ostringstream resumeOutput;
            const WorkerTrainingResult continuation
                = runEnemyTraining(resumed, resumeOutput);
            expect(continuation.succeeded && continuation.lastGeneration == 3U,
                "a non-Worker archetype can continue from its checkpoint");
        }
        WorkerTrainingOptions repeated = options;
        repeated.saveDirectory.reset();
        std::ostringstream repeatedOutput;
        const WorkerTrainingResult again
            = runEnemyTraining(repeated, repeatedOutput);
        expect(again.succeeded && again.finalBest == trained.finalBest
                && again.holdoutFitness == trained.holdoutFitness,
            "each archetype training run is deterministic");
    }
    fs::remove_all(root, error);
    expect(!error, "all-archetype temporary checkpoints are cleaned up");
}

} // namespace

int main()
{
    testFitness();
    testTrainingAndPersistence();
    testOtherArchetypes();
    return failures == 0 ? 0 : 1;
}
