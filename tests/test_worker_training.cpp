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
#include <string>
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

float reportedMetric(const std::string& output, const std::string_view key)
{
    const std::size_t start = output.find(key);
    if (start == std::string::npos) return -1.0F;
    std::istringstream input{output.substr(start + key.size())};
    float value = -1.0F;
    input >> value;
    return value;
}

void testFitness()
{
    using namespace gdp::game::ai;
    expect(creditedTacticalProgress(3.0F, 0.0F) == 0.0F
            && creditedTacticalProgress(3.0F, 1.0F) == 1.0F
            && creditedTacticalProgress(-2.0F, 3.0F) == -2.0F,
        "fallback regressions cannot be farmed as neural tactical progress");
    const DirectionalCredit diagonal = directionalMovementCredit(1.0F, 1.0F, 1.0F);
    const DirectionalCredit reverse = directionalMovementCredit(-1.0F, 1.0F, 1.0F);
    expect(diagonal.forward == 0.5F && diagonal.strafe == 0.5F
            && diagonal.retreat == 0.0F
            && reverse.retreat == 0.5F && reverse.strafe == 0.5F,
        "directional credit is normalized without changing physical movement");
    const WorkerFitnessMetrics baseline{
        .damageDealt = 10.0F,
        .damageReceived = 5.0F,
        .survivalSeconds = 3.0F,
        .tacticalProgressMeters = 2.0F,
        .proximitySeconds = 1.0F,
        .idleSeconds = 1.0F,
        .attacksLanded = 1U,
    };
    WorkerFitnessMetrics better = baseline;
    better.damageDealt += 10.0F;
    better.attacksLanded += 1U;
    expect(scoreWorkerFitness(better) > scoreWorkerFitness(baseline),
        "damage and landed attacks increase fitness");
    WorkerFitnessMetrics wastefulShooter = baseline;
    wastefulShooter.shotsFired = 8U;
    wastefulShooter.shotsBlocked = 2U;
    expect(scoreEnemyFitness(wastefulShooter,
            gdp::entities::EnemyArchetype::Guard)
            < scoreEnemyFitness(baseline,
                gdp::entities::EnemyArchetype::Guard),
        "missed and obstructed shots cost bounded reward");
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
    WorkerFitnessMetrics progress = baseline;
    progress.tacticalProgressMeters += 1.0F;
    expect(scoreWorkerFitness(progress) > scoreWorkerFitness(baseline),
        "net progress toward tactical range is rewarded");
    progress.tacticalProgressMeters -= 3.0F;
    expect(scoreWorkerFitness(progress) < scoreWorkerFitness(baseline),
        "losing tactical range removes earlier credit");
    WorkerFitnessMetrics oscillating = baseline;
    oscillating.unproductiveReversals = 2U;
    expect(scoreWorkerFitness(oscillating) < scoreWorkerFitness(baseline),
        "unproductive command reversals are penalized");
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
    WorkerFitnessMetrics aimed = baseline;
    aimed.aimSeconds += 2.0F;
    expect(scoreEnemyFitness(aimed, gdp::entities::EnemyArchetype::Guard)
            > scoreEnemyFitness(baseline, gdp::entities::EnemyArchetype::Guard)
            && scoreWorkerFitness(aimed) == scoreWorkerFitness(baseline),
        "ranged roles reward aligned aim but melee Worker does not");
    WorkerFitnessMetrics supported = baseline;
    supported.supportActions += 2U;
    expect(scoreEnemyFitness(supported, gdp::entities::EnemyArchetype::Engineer)
            == scoreEnemyFitness(baseline, gdp::entities::EnemyArchetype::Engineer),
        "automatic support actions do not reward the neural policy");
    WorkerFitnessMetrics fallback = baseline;
    fallback.fallbackDamageDealt = 100.0F;
    fallback.fallbackAttacksLanded = 10U;
    fallback.fallbackPlayerEliminations = 1U;
    expect(scoreWorkerFitness(fallback) == scoreWorkerFitness(baseline),
        "scripted fallback combat does not reward the neural policy");
    fallback.fallbackDamageReceived = 10.0F;
    expect(scoreWorkerFitness(fallback) < scoreWorkerFitness(baseline),
        "damage received during fallback still counts against survival");
    WorkerFitnessMetrics movementCommand = baseline;
    movementCommand.forwardCreditSeconds = 8.0F;
    movementCommand.strafeCreditSeconds = 8.0F;
    expect(scoreWorkerFitness(movementCommand) == scoreWorkerFitness(baseline),
        "movement commands have diagnostic credit, not automatic reward");
    const WorkerFitnessMetrics shapingOnly{
        .damageDealt = 1000.0F,
        .tacticalProgressMeters = 100.0F,
        .proximitySeconds = 100.0F,
        .contactSeconds = 100.0F,
        .attacksLanded = 100U,
    };
    WorkerFitnessMetrics winner;
    winner.playerEliminations = 1U;
    expect(scoreWorkerFitness(winner) > scoreWorkerFitness(shapingOnly),
        "eliminating the player outweighs all auxiliary reward");
    WorkerFitnessMetrics dead = shapingOnly;
    dead.deaths = 1U;
    expect(scoreWorkerFitness(dead) < 0.0F,
        "death outweighs even maximum auxiliary reward");
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
    expect(output.str().find("reward_rules=3") != std::string::npos
            && output.str().find("brain_contract=2") != std::string::npos
            && output.str().find("curriculum=2") != std::string::npos
            && output.str().find("scenarios=5") != std::string::npos
            && output.str().find("holdout_scenarios=2") != std::string::npos
            && output.str().find("repetitions=2") != std::string::npos
            && output.str().find("worst_scenario=") != std::string::npos
            && output.str().find("blocked_s=") != std::string::npos
            && output.str().find("wasted_attack_s=") != std::string::npos
            && output.str().find("aim_s=") != std::string::npos
            && output.str().find("range_progress=") != std::string::npos
            && output.str().find("fallback_damage_dealt=") != std::string::npos
            && output.str().find("reversals=") != std::string::npos
            && output.str().find("neural_ticks=") != std::string::npos
            && reportedMetric(output.str(), "player_jumps=") >= 2.0F
            && reportedMetric(output.str(), "player_peak_speed=") > 4.5F,
        "training reports scenario count and reward-penalty evidence");
    if (first.succeeded && first.champion && first.lastCheckpoint) {
        expect(first.finalBest >= first.initialBest - 1e-4F,
            "elitism preserves the best score on fixed scenarios");
        const auto saved = gdp::persistence::loadWorkerBrain(*first.lastCheckpoint);
        expect(saved && saved->trainingSeed == options.seed
                && saved->genome.generation == 2U
                && saved->contract
                    == gdp::gameplay::EnemyBrainContract::Tactical,
            "checkpoint loads with seed and generation");
        if (saved) {
            expect(std::equal(saved->genome.network.parameters().begin(),
                    saved->genome.network.parameters().end(),
                    first.champion->network.parameters().begin()),
                "checkpoint preserves every weight exactly");
        }
        wkc::ai::Genome legacyGenome{
            wkc::ai::NeuralNetwork{7U, 8U, 4U},
            first.champion->fitness,
            first.champion->generation,
            first.champion->id};
        legacyGenome.network.randomize(42U, 0.5F);
        std::ostringstream legacyBody;
        legacyBody << "GDP_WORKER_BRAIN 1\nseed " << options.seed
                   << "\ngeneration " << legacyGenome.generation
                   << "\nid " << legacyGenome.id
                   << "\nfitness_bits "
                   << std::bit_cast<std::uint32_t>(legacyGenome.fitness)
                   << "\nweights "
                   << legacyGenome.network.parameters().size() << '\n';
        for (const float weight : legacyGenome.network.parameters()) {
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
                == gdp::entities::EnemyArchetype::Worker
                && legacy->contract
                    == gdp::gameplay::EnemyBrainContract::Legacy,
            "version-one Worker checkpoints remain loadable");
        WorkerTrainingOptions legacyResume = options;
        legacyResume.resumeBrain = legacyPath;
        legacyResume.saveDirectory.reset();
        std::ostringstream legacyResumeOutput;
        expect(!runWorkerTraining(legacyResume, legacyResumeOutput).succeeded,
            "old motor contract cannot silently resume tactical training");
        expect(!gdp::persistence::saveWorkerBrain(
                *first.lastCheckpoint,
                {*first.champion, options.seed,
                    gdp::entities::EnemyArchetype::Worker,
                    gdp::gameplay::EnemyBrainContract::Tactical}),
            "saving never overwrites an existing checkpoint");
        wkc::ai::Genome invalidGenome = *first.champion;
        invalidGenome.network.parameters()[0] = 2.0F;
        expect(!gdp::persistence::saveWorkerBrain(
                root / "invalid.brain", {invalidGenome, options.seed,
                    gdp::entities::EnemyArchetype::Worker,
                    gdp::gameplay::EnemyBrainContract::Tactical}),
            "out-of-range weights cannot be saved");
        const fs::path danglingLink = root / "dangling.brain";
        fs::create_symlink(root / "absent.brain", danglingLink, error);
        if (!error) {
            expect(!gdp::persistence::saveWorkerBrain(
                    danglingLink, {*first.champion, options.seed,
                        gdp::entities::EnemyArchetype::Worker,
                        gdp::gameplay::EnemyBrainContract::Tactical})
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

        WorkerTrainingOptions allScenarios = repeated;
        allScenarios.generations = 1U;
        allScenarios.trainAllScenarios = true;
        std::ostringstream allScenariosOutput;
        const WorkerTrainingResult broad = runWorkerTraining(
            allScenarios, allScenariosOutput);
        expect(broad.succeeded && broad.champion
                && allScenariosOutput.str().find("curriculum=3")
                    != std::string::npos
                && allScenariosOutput.str().find("scenarios=7")
                    != std::string::npos
                && allScenariosOutput.str().find("holdout_scenarios=7")
                    != std::string::npos,
            "all-scenarios curriculum trains and holds out all seven situations");
        std::ostringstream repeatBroadOutput;
        const WorkerTrainingResult repeatBroad = runWorkerTraining(
            allScenarios, repeatBroadOutput);
        expect(repeatBroad.succeeded
                && repeatBroad.finalBest == broad.finalBest
                && repeatBroad.holdoutFitness == broad.holdoutFitness,
            "all-scenarios curriculum remains deterministic for a fixed seed");

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
        expect(reportedMetric(output.str(), "player_jumps=") >= 2.0F
                && reportedMetric(output.str(), "player_peak_speed=") > 4.5F,
            "every archetype trains against a physics-based Bhop scenario");
        if (archetype == EnemyArchetype::IndustrialRobot) {
            expect(reportedMetric(output.str(), "boss_long_s=") > 0.0F
                    && reportedMetric(output.str(), "boss_pressure_s=") > 0.0F
                    && reportedMetric(output.str(), "boss_pursuit_s=") > 0.0F,
                "boss curriculum exercises all three health phases");
        }
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
