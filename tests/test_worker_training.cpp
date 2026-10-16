#include "game/ai/WorkerTraining.hpp"
#include "game/ai/TrainingPopulationStore.hpp"
#include "game/ai/TrainingEncounter.hpp"
#include "gameplay/Combat.hpp"

#include "core/Hash.hpp"
#include "gameplay/EnemyBrainContract.hpp"
#include "persistence/WorkerBrainStore.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
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

bool observedCleanHardCaseHit(const std::string& output)
{
    std::istringstream input{output};
    std::string line;
    while (std::getline(input, line)) {
        if (line.starts_with("AI_GENERATION")
            && reportedMetric(line, "hard_case_hit_episodes=") > 0.0F
            && reportedMetric(line, "hard_case_damage=") > 0.0F
            && reportedMetric(line, "hard_case_friendly_damage=") == 0.0F) {
            return true;
        }
    }
    return false;
}

bool observedDistinctMissScores(const std::string& output)
{
    std::istringstream input{output};
    std::string line;
    std::optional<float> firstScore;
    while (std::getline(input, line)) {
        if (!line.starts_with("AI_GENERATION")
            || reportedMetric(line, "hard_case_hit_episodes=") != 0.0F) {
            continue;
        }
        const float score = reportedMetric(line, "hard_case_score=");
        if (firstScore && score != *firstScore) return true;
        firstScore = score;
    }
    return false;
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
    WorkerFitnessMetrics prematureReload = baseline;
    prematureReload.prematureReloads = 2U;
    expect(scoreEnemyFitness(prematureReload,
            gdp::entities::EnemyArchetype::IndustrialRobot)
            < scoreEnemyFitness(baseline,
                gdp::entities::EnemyArchetype::IndustrialRobot)
            && scoreTrainingStage(prematureReload,
                gdp::entities::EnemyArchetype::IndustrialRobot,
                TrainingStage::FireControl)
                < scoreTrainingStage(baseline,
                    gdp::entities::EnemyArchetype::IndustrialRobot,
                    TrainingStage::FireControl),
        "premature reloads reduce both combat and fire-control reward");
    WorkerFitnessMetrics necessaryReload = baseline;
    necessaryReload.reloadsStarted = 2U;
    expect(scoreEnemyFitness(necessaryReload,
            gdp::entities::EnemyArchetype::IndustrialRobot)
            == scoreEnemyFitness(baseline,
                gdp::entities::EnemyArchetype::IndustrialRobot),
        "necessary reloads are not penalized");
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
    WorkerFitnessMetrics dodging = baseline;
    dodging.dodgeOpportunities = 1U;
    dodging.dodgesSucceeded = 1U;
    expect(scoreTrainingStage(dodging,
            gdp::entities::EnemyArchetype::Guard, TrainingStage::Dodge)
            > scoreTrainingStage(baseline,
                gdp::entities::EnemyArchetype::Guard, TrainingStage::Dodge),
        "dodge stage rewards a verified avoided projectile");
    WorkerFitnessMetrics strafing = baseline;
    strafing.strafeCreditSeconds = 20.0F;
    expect(scoreTrainingStage(strafing,
            gdp::entities::EnemyArchetype::Guard, TrainingStage::Dodge)
            == scoreTrainingStage(baseline,
                gdp::entities::EnemyArchetype::Guard, TrainingStage::Dodge),
        "dodge stage never rewards strafe without a real threat");
    WorkerFitnessMetrics directHit = baseline;
    directHit.incomingProjectileHits = 1U;
    WorkerFitnessMetrics partialDodge = directHit;
    partialDodge.dodgeLateralMeters = 0.3F;
    WorkerFitnessMetrics noThreat = baseline;
    noThreat.dodgeLateralMeters = 0.3F;
    const float directHitScore = scoreTrainingStage(directHit, boss,
        TrainingStage::Dodge);
    const float partialDodgeScore = scoreTrainingStage(partialDodge, boss,
        TrainingStage::Dodge);
    expect(partialDodgeScore > directHitScore
            && partialDodgeScore < scoreTrainingStage(dodging, boss,
                TrainingStage::Dodge),
        "real lateral displacement improves a failed dodge but not beyond avoidance");
    expect(scoreTrainingStage(noThreat, boss, TrainingStage::Dodge)
            == scoreTrainingStage(baseline, boss, TrainingStage::Dodge),
        "lateral movement without an incoming hit earns no partial dodge credit");
    WorkerFitnessMetrics covered = baseline;
    covered.coverEntries = 1U;
    const auto guard = gdp::entities::EnemyArchetype::Guard;
    expect(scoreTrainingStage(covered, guard, TrainingStage::Positioning)
            > scoreTrainingStage(baseline, guard, TrainingStage::Positioning),
        "positioning stage credits cover only when paired with a real hit");
    covered.attacksLanded = 0U;
    WorkerFitnessMetrics noHit = baseline;
    noHit.attacksLanded = 0U;
    expect(scoreTrainingStage(covered, guard, TrainingStage::Positioning)
            == scoreTrainingStage(noHit, guard, TrainingStage::Positioning),
        "passive cover cannot replace combat in positioning stage");
    covered.attacksLanded = 1U;
    covered.coverEntries = 20U;
    expect(scoreTrainingStage(covered, guard, TrainingStage::Positioning)
            - scoreTrainingStage(baseline, guard,
                TrainingStage::Positioning) < 0.5F,
        "repeated cover entry cannot farm unlimited reward");
    WorkerFitnessMetrics passiveCombat = baseline;
    passiveCombat.attacksLanded = 0U;
    passiveCombat.damageDealt = 0.0F;
    passiveCombat.tacticalProgressMeters = 4.0F;
    passiveCombat.proximitySeconds = 8.0F;
    passiveCombat.contactSeconds = 8.0F;
    for (const auto archetype : {gdp::entities::EnemyArchetype::Worker,
             gdp::entities::EnemyArchetype::Engineer}) {
        expect(scoreTrainingStage(passiveCombat, archetype, TrainingStage::Duel)
                <= -1.5F
                && scoreTrainingStage(passiveCombat, archetype,
                    TrainingStage::Positioning) <= -1.5F
                && scoreTrainingStage(passiveCombat, archetype,
                    TrainingStage::RealMaps) <= -1.5F,
            "passive combat cannot farm proximity and positioning reward");
    }
    const auto tacticalShape = gdp::gameplay::enemyBrainShape(
        gdp::gameplay::EnemyBrainContract::Tactical);
    const wkc::ai::Genome passiveWorker{
        wkc::ai::NeuralNetwork{tacticalShape.inputs, tacticalShape.hidden,
            tacticalShape.outputs}, 0.0F, 0U, 0U};
    expect(evaluateWorkerGenome(passiveWorker, 42U) < -19.0F,
        "a non-attacking Worker cannot win selection by merely surviving");
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
    WorkerFitnessMetrics careless = baseline;
    careless.friendlyHits = 2U;
    careless.friendlyDamage = 25.0F;
    careless.friendlyFireHolds = 120U;
    expect(scoreCooperativeTrainingStage(careless,
            gdp::entities::EnemyArchetype::Guard, TrainingStage::FireControl)
            < scoreCooperativeTrainingStage(baseline,
                gdp::entities::EnemyArchetype::Guard, TrainingStage::FireControl)
            && scoreCooperativeTrainingStage(careless,
                gdp::entities::EnemyArchetype::Guard, TrainingStage::Aim)
                == scoreTrainingStage(careless,
                    gdp::entities::EnemyArchetype::Guard, TrainingStage::Aim),
        "cooperative combat penalizes allied damage and prolonged fire holds");
}

void testCooperativeTraining()
{
    namespace fs = std::filesystem;
    using namespace gdp::game::ai;
    const fs::path root = fs::temp_directory_path()
        / ("gdp_cooperative_training_test_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code error;
    if (!fs::create_directory(root, error) || error) {
        expect(false, "test owns a unique cooperative training directory");
        return;
    }
    WorkerTrainingOptions options{
        .archetype = gdp::entities::EnemyArchetype::Guard,
        .seed = 42U,
        .seedExplicit = true,
        .brainContract = gdp::gameplay::EnemyBrainContract::Cooperative,
        .curriculumGenerations = 1U,
        .population = 2U,
        .generations = 1U,
        .saveDirectory = root / "first",
        .resumeBrain = std::nullopt,
        .resumePopulation = std::nullopt,
    };
    std::ostringstream log;
    const auto first = runEnemyTraining(options, log);
    expect(first.succeeded && first.lastCheckpoint && first.lastPopulation
            && log.str().find("reward_rules=6") != std::string::npos
            && log.str().find("curriculum=16") != std::string::npos
            && log.str().find("group_episodes=") != std::string::npos
            && reportedMetric(log.str(), "group_episodes=") > 0.0F
            && reportedMetric(log.str(), "large_room_episodes=") > 0.0F,
        "cooperative training runs allied encounters in larger rooms");
    if (first.lastCheckpoint && first.lastPopulation) {
        const auto brain = gdp::persistence::loadEnemyBrain(*first.lastCheckpoint);
        const auto population = loadTrainingPopulation(*first.lastPopulation);
        expect(brain && population && brain->contract
                == gdp::gameplay::EnemyBrainContract::Cooperative
                && population->contract
                    == gdp::gameplay::EnemyBrainContract::Cooperative,
            "versioned cooperative brain and full population reload");
        options.resumePopulation = first.lastPopulation;
        options.saveDirectory = root / "resumed";
        std::ostringstream resumeLog;
        const auto resumed = runEnemyTraining(options, resumeLog);
        expect(resumed.succeeded && resumed.lastGeneration == 2U,
            "cooperative population resumes at the next generation");
        options.trainAllScenarios = true;
        options.saveDirectory.reset();
        std::ostringstream changedRepetitionsLog;
        expect(!runEnemyTraining(options, changedRepetitionsLog).succeeded,
            "cooperative population refuses changed scenario repetitions");
        options.trainAllScenarios = false;
        if (population) {
            auto olderCurriculum = *population;
            olderCurriculum.curriculum = 12U;
            const auto oldPath = root / "older_curriculum.population";
            expect(saveTrainingPopulation(oldPath, olderCurriculum),
                "older cooperative curriculum fixture saves for rejection test");
            options.resumePopulation = oldPath;
            options.saveDirectory.reset();
            std::ostringstream oldLog;
            expect(!runEnemyTraining(options, oldLog).succeeded,
                "cooperative training refuses a pre-0.1.6 curriculum population");
        }
        options.resumePopulation.reset();
        options.resumeBrain = first.lastCheckpoint;
        std::ostringstream brainLog;
        const auto brainResume = runEnemyTraining(options, brainLog);
        expect(!brainResume.succeeded
                && brainResume.error.find("--resume-population") != std::string::npos,
            "cooperative brain-only resume is rejected without curriculum metadata");
        options.resumeBrain.reset();
        options.resumePopulation = first.lastPopulation;
        options.brainContract = gdp::gameplay::EnemyBrainContract::Tactical;
        options.saveDirectory.reset();
        std::ostringstream mismatchLog;
        expect(!runEnemyTraining(options, mismatchLog).succeeded,
            "tactical training refuses a cooperative population");
    }
    WorkerTrainingOptions bossOptions = options;
    bossOptions.archetype = gdp::entities::EnemyArchetype::IndustrialRobot;
    bossOptions.brainContract = gdp::gameplay::EnemyBrainContract::Cooperative;
    bossOptions.resumePopulation.reset();
    bossOptions.diverseMaps = true;
    bossOptions.saveDirectory.reset();
    std::ostringstream bossLog;
    const auto trainedBoss = runEnemyTraining(bossOptions, bossLog);
    expect(trainedBoss.succeeded
            && bossLog.str().find("curriculum=18") != std::string::npos
            && reportedMetric(bossLog.str(), "long_map_episodes=") > 0.0F
            && reportedMetric(bossLog.str(), "boss_helper_episodes=") > 0.0F,
        "cooperative Robot trains with long Boss room and two aides");
    WorkerTrainingOptions layoutRetry{
        .archetype = gdp::entities::EnemyArchetype::Guard,
        .seed = 20261014U,
        .seedExplicit = true,
        .trainAllScenarios = true,
        .diverseMaps = true,
        .brainContract = gdp::gameplay::EnemyBrainContract::Cooperative,
        .population = 2U,
        .generations = 10U,
        .saveDirectory = std::nullopt,
        .resumeBrain = std::nullopt,
        .resumePopulation = std::nullopt,
    };
    std::ostringstream layoutLog;
    const auto layoutResult = runEnemyTraining(layoutRetry, layoutLog);
    expect(layoutResult.succeeded && layoutResult.lastGeneration == 10U,
        "cooperative Guard retries a failed wide-room world seed deterministically");
    fs::remove_all(root);
}

void testHardCaseCurriculum()
{
    namespace fs = std::filesystem;
    using namespace gdp::game::ai;
    const fs::path root = fs::temp_directory_path()
        / ("gdp_hard_case_training_test_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code error;
    if (!fs::create_directory(root, error) || error) {
        expect(false, "hard-case test owns a unique output directory");
        return;
    }
    WorkerTrainingOptions options{
        .archetype = gdp::entities::EnemyArchetype::Guard,
        .seed = 20261014U,
        .seedExplicit = true,
        .trainAllScenarios = true,
        .diverseMaps = true,
        .hardCaseCurriculum = true,
        .brainContract = gdp::gameplay::EnemyBrainContract::Cooperative,
        .curriculumGenerations = 1U,
        .population = 2U,
        .generations = 2U,
        .saveDirectory = root / "continuous",
        .resumeBrain = std::nullopt,
        .resumePopulation = std::nullopt,
    };
    std::ostringstream continuousLog;
    const auto continuous = runEnemyTraining(options, continuousLog);
    expect(continuous.succeeded && continuous.lastPopulation
            && continuousLog.str().find("curriculum=20") != std::string::npos
            && reportedMetric(continuousLog.str(), "hard_case_episodes=") == 2.0F
            && reportedMetric(continuousLog.str(), "hard_case_group_episodes=")
                == 2.0F
            && reportedMetric(continuousLog.str(), "hard_case_player_jumps=")
                > 0.0F,
        "opt-in Guard pilot exercises two additional cover/group fixtures");
    if (continuous.lastPopulation) {
        const auto saved = loadTrainingPopulation(*continuous.lastPopulation);
        expect(saved && saved->curriculum == 20U,
            "hard-case population stores a distinct curriculum ID");
        if (saved) {
            auto legacy = *saved;
            legacy.curriculum = 19U;
            const fs::path legacyPath = root / "legacy.population";
            expect(saveTrainingPopulation(legacyPath, legacy),
                "curriculum 19 compatibility fixture saves");
            WorkerTrainingOptions incompatible = options;
            incompatible.generations = 1U;
            incompatible.resumePopulation = legacyPath;
            incompatible.saveDirectory.reset();
            std::ostringstream incompatibleLog;
            expect(!runEnemyTraining(incompatible, incompatibleLog).succeeded,
                "curriculum 20 cannot resume a curriculum 19 population");
        }
    }
    WorkerTrainingOptions split = options;
    split.generations = 1U;
    split.saveDirectory = root / "split";
    std::ostringstream firstLog;
    const auto first = runEnemyTraining(split, firstLog);
    expect(first.succeeded && first.lastPopulation,
        "hard-case pilot creates a resumable first generation");
    if (first.lastPopulation) {
        split.resumePopulation = first.lastPopulation;
        split.saveDirectory = root / "resumed";
        std::ostringstream resumedLog;
        const auto resumed = runEnemyTraining(split, resumedLog);
        expect(resumed.succeeded && continuous.succeeded
                && resumed.lastGeneration == 2U
                && resumed.finalBest == continuous.finalBest
                && resumed.holdoutFitness == continuous.holdoutFitness
                && resumed.champion && continuous.champion
                && std::equal(resumed.champion->network.parameters().begin(),
                    resumed.champion->network.parameters().end(),
                    continuous.champion->network.parameters().begin()),
            "hard-case split run exactly reproduces uninterrupted champion");
        split.hardCaseCurriculum = false;
        split.saveDirectory.reset();
        std::ostringstream incompatibleLog;
        expect(!runEnemyTraining(split, incompatibleLog).succeeded,
            "curriculum 19 cannot resume a curriculum 20 population");
    }
    options.generations = 1U;
    options.archetype = gdp::entities::EnemyArchetype::IndustrialRobot;
    options.saveDirectory.reset();
    std::ostringstream robotLog;
    const auto robot = runEnemyTraining(options, robotLog);
    expect(robot.succeeded && robotLog.str().find("curriculum=20")
            != std::string::npos
            && reportedMetric(robotLog.str(), "hard_case_episodes=") == 2.0F
            && reportedMetric(robotLog.str(), "hard_case_player_jumps=") > 0.0F
            && reportedMetric(robotLog.str(), "hard_case_cover_map_episodes=")
                > 0.0F
            && reportedMetric(robotLog.str(), "hard_case_boss_helper_episodes=")
                > 0.0F,
        "Robot pilot exercises Bhop with cover and Boss helpers");
    options.archetype = gdp::entities::EnemyArchetype::Worker;
    std::ostringstream invalidLog;
    expect(!runEnemyTraining(options, invalidLog).succeeded,
        "hard-case pilot is limited to Guard and Robot");
    fs::remove_all(root, error);
    expect(!error, "hard-case temporary checkpoints are cleaned up");
}

void testHardCaseCurriculumV2()
{
    namespace fs = std::filesystem;
    using namespace gdp::game::ai;
    const fs::path root = fs::temp_directory_path()
        / ("gdp_hard_case_v2_test_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code error;
    if (!fs::create_directory(root, error) || error) {
        expect(false, "v21 test owns a unique output directory");
        return;
    }
    WorkerTrainingOptions options{
        .archetype = gdp::entities::EnemyArchetype::Guard,
        .seed = 20261014U,
        .seedExplicit = true,
        .trainAllScenarios = true,
        .diverseMaps = true,
        .hardCaseCurriculumV2 = true,
        .brainContract = gdp::gameplay::EnemyBrainContract::Cooperative,
        .curriculumGenerations = 1U,
        .population = 8U,
        .generations = 2U,
        .saveDirectory = root / "continuous",
        .resumeBrain = std::nullopt,
        .resumePopulation = std::nullopt,
    };
    std::ostringstream continuousLog;
    const auto continuous = runEnemyTraining(options, continuousLog);
    expect(continuous.succeeded && continuous.lastPopulation
            && continuousLog.str().find("curriculum=21") != std::string::npos
            && observedCleanHardCaseHit(continuousLog.str())
            && reportedMetric(continuousLog.str(), "hard_case_group_episodes=")
                == 2.0F,
        "v21 Guard fixture allows a neural hit with no allied damage");
    if (continuous.lastPopulation) {
        const auto saved = loadTrainingPopulation(*continuous.lastPopulation);
        expect(saved && saved->curriculum == 21U,
            "v21 population carries an incompatible curriculum ID");
        if (saved) for (const std::uint32_t olderId : {19U, 20U}) {
            auto older = *saved;
            older.curriculum = olderId;
            const fs::path olderPath = root
                / ("curriculum_" + std::to_string(olderId) + ".population");
            expect(saveTrainingPopulation(olderPath, older),
                "older curriculum compatibility fixture saves");
            WorkerTrainingOptions incompatible = options;
            incompatible.generations = 1U;
            incompatible.resumePopulation = olderPath;
            incompatible.saveDirectory.reset();
            std::ostringstream incompatibleLog;
            expect(!runEnemyTraining(incompatible, incompatibleLog).succeeded,
                "v21 refuses curriculum 19 and 20 populations");
        }
    }
    WorkerTrainingOptions split = options;
    split.generations = 1U;
    split.saveDirectory = root / "split";
    std::ostringstream firstLog;
    const auto first = runEnemyTraining(split, firstLog);
    expect(first.succeeded && first.lastPopulation,
        "v21 saves a full first-generation population");
    if (first.lastPopulation) {
        split.resumePopulation = first.lastPopulation;
        split.saveDirectory = root / "resumed";
        std::ostringstream resumedLog;
        const auto resumed = runEnemyTraining(split, resumedLog);
        expect(resumed.succeeded && continuous.succeeded
                && resumed.lastGeneration == 2U
                && resumed.finalBest == continuous.finalBest
                && resumed.holdoutFitness == continuous.holdoutFitness
                && resumed.champion && continuous.champion
                && std::equal(resumed.champion->network.parameters().begin(),
                    resumed.champion->network.parameters().end(),
                    continuous.champion->network.parameters().begin()),
            "v21 split resume reproduces the uninterrupted champion");
        split.hardCaseCurriculumV2 = false;
        split.saveDirectory.reset();
        std::ostringstream incompatibleLog;
        expect(!runEnemyTraining(split, incompatibleLog).succeeded,
            "curriculum 19 refuses a v21 population");
        split.hardCaseCurriculum = true;
        expect(!runEnemyTraining(split, incompatibleLog).succeeded,
            "curriculum 20 refuses a v21 population");
    }
    options.saveDirectory.reset();
    options.generations = 6U;
    options.curriculumGenerations = 3U;
    options.archetype = gdp::entities::EnemyArchetype::IndustrialRobot;
    std::ostringstream robotLog;
    const auto robot = runEnemyTraining(options, robotLog);
    expect(robot.succeeded && observedCleanHardCaseHit(robotLog.str())
            && observedDistinctMissScores(robotLog.str())
            && reportedMetric(robotLog.str(), "hard_case_boss_helper_episodes=")
                == 2.0F
            && reportedMetric(robotLog.str(), "hard_case_player_jumps=") > 0.0F,
        "v21 Robot has feasible Boss/Bhop fixtures and graded misses");
    options.hardCaseCurriculum = true;
    std::ostringstream invalidLog;
    expect(!runEnemyTraining(options, invalidLog).succeeded,
        "v20 and v21 cannot be selected together");
    fs::remove_all(root, error);
    expect(!error, "v21 temporary checkpoints are cleaned up");
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
        .resumePopulation = std::nullopt,
    };
    std::ostringstream output;
    const WorkerTrainingResult first = runWorkerTraining(options, output);
    expect(first.succeeded && first.champion && first.lastCheckpoint
            && first.lastGeneration == 2U && first.completedStages == 4U,
        "automated training produces a champion and final checkpoint");
    expect(output.str().find("reward_rules=5") != std::string::npos
            && output.str().find("brain_contract=2") != std::string::npos
            && output.str().find("curriculum=12") != std::string::npos
            && output.str().find("stage_immigrants=2") != std::string::npos
            && output.str().find("runtime_control_all=1") != std::string::npos
            && output.str().find("scenarios=7") != std::string::npos
            && output.str().find("holdout_scenarios=7") != std::string::npos
            && output.str().find("stage=1/7") != std::string::npos
            && output.str().find("stage=4/7") != std::string::npos
            && output.str().find("scenario_seed=") != std::string::npos
            && output.str().find("stages_completed=4") != std::string::npos
            && output.str().find("repetitions=2") != std::string::npos
            && output.str().find("worst_scenario=") != std::string::npos
            && output.str().find("combat_hit_episodes=") != std::string::npos
            && output.str().find("dodge_lateral_m=") != std::string::npos
            && output.str().find("premature_reloads=") != std::string::npos
            && output.str().find("holdout_combat_competent=")
                != std::string::npos
            && output.str().find("stage_scores=") != std::string::npos
            && output.str().find("stage_hit_episodes=") != std::string::npos
            && output.str().find("stage_dodges=") != std::string::npos
            && output.str().find("blocked_s=") != std::string::npos
            && output.str().find("wasted_attack_s=") != std::string::npos
            && output.str().find("aim_s=") != std::string::npos
            && output.str().find("range_progress=") != std::string::npos
            && output.str().find("fallback_damage_dealt=") != std::string::npos
            && output.str().find("reversals=") != std::string::npos
            && output.str().find("neural_ticks=") != std::string::npos
            && reportedMetric(output.str(), "fallback_ticks=") > 0.0F
            && reportedMetric(output.str(), "player_jumps=") >= 2.0F
            && reportedMetric(output.str(), "player_peak_speed=") > 4.5F,
        "training reports scenario count and reward-penalty evidence");
    const std::string report = output.str();
    const std::string seedLabel = "scenario_seed=";
    const std::size_t firstSeed = report.find(seedLabel);
    const std::size_t secondSeed = firstSeed == std::string::npos
        ? std::string::npos
        : report.find(seedLabel, firstSeed + seedLabel.size());
    expect(firstSeed != std::string::npos
            && secondSeed != std::string::npos
            && report.substr(firstSeed, report.find(' ', firstSeed) - firstSeed)
                != report.substr(secondSeed,
                    report.find(' ', secondSeed) - secondSeed),
        "successive generations train on different deterministic fixtures");
    if (first.succeeded && first.champion && first.lastCheckpoint) {
        expect(std::isfinite(first.initialBest)
                && std::isfinite(first.finalBest),
            "scores remain finite across curriculum stage transitions");
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
                && allScenariosOutput.str().find("curriculum=13")
                    != std::string::npos
                && allScenariosOutput.str().find("scenarios=7")
                    != std::string::npos
                && allScenariosOutput.str().find("holdout_scenarios=7")
                    != std::string::npos
                && allScenariosOutput.str().find("repetitions=4")
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
        expect(reportedMetric(output.str(), "dodge_opportunities=") > 0.0F,
            "every archetype faces an actual incoming projectile in dodge stage");
        expect(reportedMetric(output.str(), "shots=")
                + reportedMetric(output.str(), "fallback_shots=") > 0.0F,
            "every ranged archetype exercises real projectiles in training");
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


void testFullPopulationAndEncounters()
{
    using namespace gdp::game::ai;
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / ("gdp_full_population_" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    WorkerTrainingOptions options;
    options.seed = 42;
    options.population = 4;
    options.generations = 4;
    options.curriculumGenerations = 3;
    options.trainAllScenarios = true;
    options.diverseMaps = true;
    options.saveDirectory = root / "continuous";
    std::ostringstream continuousLog;
    const auto continuous = runWorkerTraining(options, continuousLog);
    auto splitOptions = options;
    splitOptions.generations = 2;
    splitOptions.saveDirectory = root / "split";
    std::ostringstream firstLog;
    const auto first = runWorkerTraining(splitOptions, firstLog);
    expect(continuous.succeeded && first.succeeded && first.lastPopulation,
        "diverse curriculum saves full populations");
    if (continuous.succeeded && first.lastPopulation) {
        const auto saved = loadTrainingPopulation(*first.lastPopulation);
        expect(saved && saved->population.size() == 4 && saved->generation == 2,
            "population snapshot includes every ranked individual and progress");
        splitOptions.resumePopulation = first.lastPopulation;
        splitOptions.saveDirectory = root / "continued";
        std::ostringstream continuedLog;
        const auto continued = runWorkerTraining(splitOptions, continuedLog);
        expect(continued.succeeded && continued.lastGeneration == 4
            && continued.finalBest == continuous.finalBest
            && continued.holdoutFitness == continuous.holdoutFitness,
            "split run exactly matches uninterrupted fitness and holdout");
        if (continued.champion && continuous.champion) {
            expect(std::equal(continued.champion->network.parameters().begin(),
                continued.champion->network.parameters().end(),
                continuous.champion->network.parameters().begin()),
                "full resume reproduces champion weights exactly");
        }
        const auto resumedPopulation = continued.lastPopulation
            ? loadTrainingPopulation(*continued.lastPopulation) : std::nullopt;
        const auto fullPopulation = loadTrainingPopulation(*continuous.lastPopulation);
        bool identical = resumedPopulation && fullPopulation;
        if (identical) for (std::size_t i = 0; i < fullPopulation->population.size(); ++i) {
            const auto& a = fullPopulation->population[i];
            const auto& b = resumedPopulation->population[i];
            identical = identical && a.id == b.id && a.fitness == b.fitness
                && std::equal(a.network.parameters().begin(), a.network.parameters().end(),
                    b.network.parameters().begin());
        }
        expect(identical, "full resume reproduces the entire ranked population");
        splitOptions.population = 8;
        std::ostringstream mismatchLog;
        expect(!runWorkerTraining(splitOptions, mismatchLog).succeeded,
            "full resume refuses population-size mismatch");
        splitOptions.population = 4;
        splitOptions.diverseMaps = false;
        expect(!runWorkerTraining(splitOptions, mismatchLog).succeeded,
            "full resume refuses curriculum mismatch");
        expect(saved && !saveTrainingPopulation(*first.lastPopulation, *saved),
            "population snapshots never overwrite existing state");
        fs::remove(*first.lastPopulation / "member-001.brain");
        expect(!loadTrainingPopulation(*first.lastPopulation),
            "missing member invalidates entire population snapshot");
    }
    for (unsigned variant = 1; variant <= 5; ++variant) {
        for (unsigned seed = 1; seed <= 8; ++seed) {
            const auto encounter = makeTrainingEncounter({{-4,0,-4},{4,4,4}}, seed, variant);
            expect(encounter.collision.canOccupy(encounter.player, {.radius=0.35F,.height=1.8F})
                && encounter.collision.canOccupy(encounter.enemy, {.radius=0.52F,.height=1.9F}),
                "all varied encounters have physically valid spawns");
            if (variant == 4) {
                const auto approach = wkc::core::Vec3{encounter.player.x,0,encounter.player.z+0.9F};
                expect(encounter.collision.canOccupy(approach, {.radius=0.35F,.height=1.8F})
                    && gdp::gameplay::hasLineOfSight(approach+wkc::core::Vec3{0,1.3F,0},
                        encounter.player+wkc::core::Vec3{0,1.6F,0}, encounter.collision),
                    "height encounter has a reachable melee attack position without climbing");
            }
            if (variant == 3 || variant == 5) {
                const auto hidden = encounter.player + wkc::core::Vec3{0,1.6F,0};
                const auto peek = hidden + wkc::core::Vec3{encounter.peekWidth,0,0};
                const auto eye = encounter.enemy + wkc::core::Vec3{0,1.3F,0};
                expect(!gdp::gameplay::hasLineOfSight(eye,hidden,encounter.collision)
                    && gdp::gameplay::hasLineOfSight(eye,peek,encounter.collision),
                    "peek trajectory actually alternates blocked and visible positions");
            }
        }
    }
    for (unsigned variant = 6; variant <= 8; ++variant) {
        const wkc::core::Aabb bounds = variant == 8
            ? wkc::core::Aabb{{-9,0,-10},{9,6,10}}
            : wkc::core::Aabb{{-14,0,-16},{14,7,16}};
        for (unsigned seed = 1; seed <= 8; ++seed) {
            const auto encounter = makeTrainingEncounter(bounds, seed, variant);
            const auto distance = wkc::core::length(
                encounter.player - encounter.enemy);
            expect(distance >= (variant == 8 ? 10.0F : 20.0F)
                    && encounter.collision.canOccupy(encounter.player,
                        {.radius=0.35F,.height=1.8F})
                    && encounter.collision.canOccupy(encounter.enemy,
                        {.radius=0.52F,.height=1.9F}),
                "large-room encounters have valid long-range spawns");
            if (variant != 6) {
                const auto eye = encounter.enemy + wkc::core::Vec3{0,1.3F,0};
                const auto hidden = encounter.player + wkc::core::Vec3{0,1.6F,0};
                const auto peek = hidden
                    + wkc::core::Vec3{encounter.peekWidth,0,0};
                expect(!gdp::gameplay::hasLineOfSight(eye,hidden,encounter.collision)
                        && gdp::gameplay::hasLineOfSight(eye,peek,encounter.collision),
                    "long cover lane has blocked and clear peeking sightlines");
            }
        }
    }
    fs::remove_all(root);
}

} // namespace

int main(const int argc, char* argv[])
{
    const std::string_view selection = argc == 1 ? "--all"
        : argc == 2 ? std::string_view{argv[1]} : "";
    if (selection != "--all" && selection != "--fitness"
        && selection != "--cooperative" && selection != "--persistence"
        && selection != "--other-archetypes"
        && selection != "--population-encounters"
        && selection != "--curriculum-v20"
        && selection != "--curriculum-v21") {
        std::cerr << "Unknown worker-training test group\n";
        return 2;
    }
    if (selection == "--all" || selection == "--fitness") testFitness();
    if (selection == "--all" || selection == "--cooperative")
        testCooperativeTraining();
    if (selection == "--all" || selection == "--persistence")
        testTrainingAndPersistence();
    if (selection == "--all" || selection == "--other-archetypes")
        testOtherArchetypes();
    if (selection == "--all" || selection == "--population-encounters")
        testFullPopulationAndEncounters();
    if (selection == "--all" || selection == "--curriculum-v20")
        testHardCaseCurriculum();
    if (selection == "--all" || selection == "--curriculum-v21")
        testHardCaseCurriculumV2();
    return failures == 0 ? 0 : 1;
}
