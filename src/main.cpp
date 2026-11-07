#include "game/Application.hpp"

#include "core/Hash.hpp"
#include "game/LaunchSeed.hpp"
#include "game/ai/WorkerTraining.hpp"

#include <SDL3/SDL.h>
#if defined(_WIN32) && defined(GDP_WINDOWS_GUI_EXECUTABLE)
#include <SDL3/SDL_main.h>
#endif

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <string_view>

namespace {

void printUsage(const std::string_view executable)
{
    std::cout << "Usage: " << executable
              << " [--headless] [--benchmark PROFILE] [--frames N] [--seed N]"
                 " [--scripted-enemies] [--experimental-neural-enemy TYPE]"
                 " [--neural-brain FILE] [--test-brain-dir DIR] [--help] [--version]\n"
              << "       " << executable
              << " --ai-training TYPE [--population N] [--generations N]"
                 " [--seed N] [--all-scenarios] [--cooldown-ms N]"
                 " [--diverse-maps] [--hard-case-curriculum]"
                 " [--hard-case-curriculum-v2]"
                 " [--perception-curriculum]"
                 " [--combat-pressure-curriculum]"
                 " [--curriculum-generations N]"
                 " [--cooperative-brain]"
                 " [--save-dir DIR] [--resume FILE | --resume-population DIR]\n"
              << "  --headless           Run without creating a window or OpenGL context.\n"
#if GDP_ENABLE_TEST_MODE
              << "  --test-mode          Open the dedicated development arena directly.\n"
              << "  --storage-room-preview  Open the Storage room layout preview directly.\n"
              << "  --combat-room-preview  Open the Combat room layout preview directly.\n"
              << "  --boss-room-preview  Open the Boss arena with its encounter directly.\n"
              << "  --cafeteria-preview  Open the Cafeteria layout preview directly.\n"
              << "  --cafeteria-long-preview  Open the elongated Cafeteria preview directly.\n"
              << "  --laboratory-preview  Open the Laboratory layout preview directly.\n"
              << "  --technical-preview  Open the Technical/Maintenance layout preview directly.\n"
              << "  --technical-l-preview  Open the L-shaped Maintenance layout preview directly.\n"
              << "  --security-preview  Open the Security layout preview directly.\n"
              << "  --security-t-preview  Open the T-shaped Security layout preview directly.\n"
              << "  --security-indicator-demo  Cycle the Security T alarm, reboot, and ready indicators.\n"
              << "  --central-hub-preview  Open the Central Hub layout preview directly.\n"
              << "  --central-hub-shadow-preview  Test geometry shadows in the Central Hub.\n"
              << "  --central-hub-shadow-legacy-preview  Compare the same scene with prior shadows.\n"
              << "  --legacy-shadows  Compare a preview with the previous shadow system.\n"
              << "  --industrial-corridor-preview  Open an Industrial Corridor traversal preview directly.\n"
              << "  --fixture-preview NAME  Compare a ceiling fixture in the clear Security room.\n"
              << "      NAME: industrial, storage, maintenance, laboratory, cafeteria,\n"
              << "            security, office, boss, or hub.\n"
#endif
              << "  --benchmark PROFILE  Measure POTATO, LOW, MEDIUM, HIGH, or ULTRA at 1280x720.\n"
              << "  --frames N           Stop after exactly N rendered loop iterations.\n"
              << "  --seed N             Generate a deterministic world from unsigned integer N.\n"
              << "  --scripted-enemies   Use the original scripted AI instead of bundled brains.\n"
              << "  --experimental-neural-enemy TYPE  Test one neural enemy of TYPE.\n"
              << "  --experimental-neural-worker  Alias for Worker playtesting.\n"
              << "  --neural-brain FILE  Load a matching saved experimental brain.\n"
              << "  --test-brain-dir DIR  Test all five TYPE.brain files without replacing bundled AI.\n"
              << "  --ai-training TYPE  Train worker, guard, officer, engineer, robot, or all.\n"
              << "  --population N      Training population (2..128, default 16).\n"
              << "  --generations N     Training generations (1..10000, default 10).\n"
              << "  --all-scenarios     Train all seven scenarios per type; test on separate seeds.\n"
              << "  --cooldown-ms N     Sleep 1..10000 ms between generations to limit heat.\n"
              << "  --save-dir DIR      Save one best-brain checkpoint per generation.\n"
              << "  --resume FILE       Seed a new training run from a checkpoint.\n"
              << "  --resume-population DIR  Continue an entire saved population exactly.\n"
              << "  --diverse-maps      Train varied cover, peek, height and boss-phase encounters.\n"
              << "  --hard-case-curriculum  Pilot extra cooperative cover and team encounters.\n"
              << "  --hard-case-curriculum-v2  Revised isolated hard-case pilot.\n"
              << "  --perception-curriculum  Train finite-age ally reports and lost-vision investigation.\n"
              << "  --combat-pressure-curriculum  Pilot Guard/Engineer combat and perception curriculum.\n"
              << "  --cooperative-brain  Train a versioned brain with ally and firing-lane inputs.\n"
              << "  --curriculum-generations N  Initial curriculum length, preserved on full resume.\n"
              << "  --help               Show this message.\n"
              << "  --version            Show the packaged game version.\n";
}

int confirmUpdate(const std::string_view version)
{
    if (version.empty() || version.size() > 32U
        || version.find_first_not_of("0123456789.") != std::string_view::npos
        || version.front() == '.' || version.back() == '.'
        || std::count(version.begin(), version.end(), '.') != 2
        || version.find("..") != std::string_view::npos) {
        std::cerr << "Invalid update version\n";
        return 2;
    }
    const std::string message = "Iron Seed " + std::string{version}
        + " está disponível. Instalar antes de iniciar o jogo?\n\n"
          "Iron Seed " + std::string{version} + " is available. Install before playing?";
    const SDL_MessageBoxButtonData buttons[] = {
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Mais tarde / Later"},
        {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Instalar / Install"},
    };
    const SDL_MessageBoxData box{
        SDL_MESSAGEBOX_INFORMATION, nullptr, "Iron Seed", message.c_str(), 2, buttons, nullptr};
    int selection = -1;
    if (!SDL_ShowMessageBox(&box, &selection)) {
        std::cerr << "Cannot show update confirmation: " << SDL_GetError() << '\n';
        return 1;
    }
    return selection == 1 ? 0 : 1;
}

std::optional<gdp::entities::EnemyArchetype> parseEnemyArchetype(
    const std::string_view value)
{
    using gdp::entities::EnemyArchetype;
    if (value == "worker") return EnemyArchetype::Worker;
    if (value == "guard") return EnemyArchetype::Guard;
    if (value == "officer") return EnemyArchetype::Officer;
    if (value == "engineer") return EnemyArchetype::Engineer;
    if (value == "robot") return EnemyArchetype::IndustrialRobot;
    return std::nullopt;
}

std::optional<std::uint32_t> parsePositiveU32(const std::string_view value)
{
    std::uint32_t result = 0;
    const auto parsed = std::from_chars(
        value.data(), value.data() + value.size(), result);
    if (value.empty() || parsed.ec != std::errc{}
        || parsed.ptr != value.data() + value.size() || result == 0U) {
        return std::nullopt;
    }
    return result;
}

std::optional<std::uint64_t> parseSeed(const std::string_view value)
{
    std::uint64_t result = 0;
    const auto* const begin = value.data();
    const auto* const end = value.data() + value.size();
    const auto parsed = std::from_chars(begin, end, result);
    if (parsed.ec != std::errc{} || parsed.ptr != end) {
        return std::nullopt;
    }
    return result;
}

std::optional<std::uint64_t> parseFrameLimit(const std::string_view value)
{
    std::uint64_t result = 0;
    const auto* const begin = value.data();
    const auto* const end = value.data() + value.size();
    const auto parsed = std::from_chars(begin, end, result);

    if (parsed.ec != std::errc{} || parsed.ptr != end || result == 0) {
        return std::nullopt;
    }

    return result;
}

wkc::core::Seed collectLaunchEntropy()
{
    const auto wallTime = std::chrono::system_clock::now().time_since_epoch().count();
    const auto monotonicTime = std::chrono::steady_clock::now().time_since_epoch().count();
    wkc::core::Seed entropy = wkc::core::hashCombine(
        static_cast<wkc::core::Seed>(wallTime),
        static_cast<wkc::core::Seed>(monotonicTime));
    try {
        std::random_device source;
        entropy = wkc::core::hashCombine(
            entropy, static_cast<wkc::core::Seed>(source()));
        entropy = wkc::core::hashCombine(
            entropy, static_cast<wkc::core::Seed>(source()));
    } catch (...) {
        // Clocks still provide a changing launch seed when system entropy is unavailable.
    }
    return entropy;
}

} // namespace

int main(const int argc, char* argv[])
{
    if (argc >= 2 && std::string_view{argv[1]} == "--confirm-update") {
        return argc == 3 ? confirmUpdate(argv[2]) : 2;
    }
    gdp::game::ApplicationConfig config;
    gdp::game::ai::WorkerTrainingOptions trainingOptions;
    bool trainingMode = false;
    bool trainAll = false;
    bool trainingOnlyArgumentSeen = false;
    std::optional<wkc::core::Seed> requestedSeed;

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};

        if (argument == "--help") {
            printUsage(argv[0]);
            return 0;
        }

        if (argument == "--version") {
            std::cout << "Iron Seed " << GDP_VERSION_STRING << '\n';
            return 0;
        }

        if (argument == "--headless") {
            config.headless = true;
            continue;
        }
#if GDP_ENABLE_TEST_MODE
        if (argument == "--test-mode") { config.testMode = true; continue; }
        if (argument == "--storage-room-preview") {
            config.storageRoomPreview = true;
            continue;
        }
        if (argument == "--combat-room-preview") {
            config.combatRoomPreview = true;
            continue;
        }
        if (argument == "--boss-room-preview") {
            config.bossRoomPreview = true;
            continue;
        }
        if (argument == "--cafeteria-preview") {
            config.cafeteriaPreview = true;
            continue;
        }
        if (argument == "--cafeteria-long-preview") {
            config.cafeteriaLongPreview = true;
            continue;
        }
        if (argument == "--laboratory-preview") {
            config.laboratoryPreview = true;
            continue;
        }
        if (argument == "--technical-preview") {
            config.technicalPreview = true;
            continue;
        }
        if (argument == "--technical-l-preview") {
            config.technicalLPreview = true;
            continue;
        }
        if (argument == "--security-preview") {
            config.securityPreview = true;
            continue;
        }
        if (argument == "--security-t-preview") {
            config.securityTPreview = true;
            continue;
        }
        if (argument == "--security-indicator-demo") {
            config.securityIndicatorDemo = true;
            continue;
        }
        if (argument == "--central-hub-preview") {
            config.centralHubPreview = true;
            continue;
        }
        if (argument == "--central-hub-shadow-preview") {
            config.centralHubPreview = true;
            config.centralHubShadowPreview = true;
            continue;
        }
        if (argument == "--central-hub-shadow-legacy-preview") {
            config.centralHubPreview = true;
            config.centralHubShadowPreview = true;
            config.legacyGeometryShadows = true;
            continue;
        }
        if (argument == "--legacy-shadows") {
            config.legacyGeometryShadows = true;
            continue;
        }
        if (argument == "--industrial-corridor-preview") {
            config.industrialCorridorPreview = true;
            continue;
        }
        if (argument == "--fixture-preview") {
            if (index + 1 >= argc) {
                std::cerr << "--fixture-preview needs a fixture name\n";
                return 2;
            }
            const std::string_view name{argv[++index]};
            using Fixture = gdp::game::visual::CeilingFixtureVariant;
            if (name == "industrial") config.fixturePreviewVariant = Fixture::Industrial;
            else if (name == "storage") config.fixturePreviewVariant = Fixture::Storage;
            else if (name == "maintenance") config.fixturePreviewVariant = Fixture::Maintenance;
            else if (name == "laboratory") config.fixturePreviewVariant = Fixture::Laboratory;
            else if (name == "cafeteria") config.fixturePreviewVariant = Fixture::Cafeteria;
            else if (name == "security") config.fixturePreviewVariant = Fixture::Security;
            else if (name == "office") config.fixturePreviewVariant = Fixture::Office;
            else if (name == "boss") config.fixturePreviewVariant = Fixture::BossArena;
            else if (name == "hub") config.fixturePreviewVariant = Fixture::CentralHub;
            else {
                std::cerr << "Unknown fixture preview: " << name << '\n';
                return 2;
            }
            config.securityPreview = true;
            continue;
        }
#endif

        if (argument == "--scripted-enemies") {
            config.scriptedEnemies = true;
            continue;
        }

        if (argument == "--experimental-neural-worker") {
            config.experimentalNeuralEnemy = gdp::entities::EnemyArchetype::Worker;
            continue;
        }

        if (argument == "--experimental-neural-enemy") {
            if (index + 1 >= argc
                || !(config.experimentalNeuralEnemy
                    = parseEnemyArchetype(argv[++index]))) {
                std::cerr << "Expected worker, guard, officer, engineer or robot\n";
                return 2;
            }
            continue;
        }

        if (argument == "--neural-brain") {
            if (index + 1 >= argc) {
                std::cerr << "Missing value after --neural-brain\n";
                return 2;
            }
            config.neuralBrainPath = std::filesystem::path{argv[++index]};
            continue;
        }

        if (argument == "--test-brain-dir") {
            if (index + 1 >= argc) {
                std::cerr << "Missing value after --test-brain-dir\n";
                return 2;
            }
            config.testBrainDirectory = std::filesystem::path{argv[++index]};
            if (config.testBrainDirectory->empty()) {
                std::cerr << "Empty test brain directory\n";
                return 2;
            }
            continue;
        }

        if (argument == "--ai-training") {
            if (index + 1 >= argc) {
                std::cerr << "Missing archetype after --ai-training\n";
                return 2;
            }
            const std::string_view selected{argv[++index]};
            trainAll = selected == "all";
            const auto archetype = parseEnemyArchetype(selected);
            if (!trainAll && !archetype) {
                std::cerr << "Expected worker, guard, officer, engineer, robot or all\n";
                return 2;
            }
            if (archetype) trainingOptions.archetype = *archetype;
            trainingMode = true;
            continue;
        }

        if (argument == "--all-scenarios" || argument == "--diverse-maps"
            || argument == "--hard-case-curriculum"
            || argument == "--hard-case-curriculum-v2"
            || argument == "--perception-curriculum"
            || argument == "--combat-pressure-curriculum") {
            if (argument == "--all-scenarios") trainingOptions.trainAllScenarios = true;
            else if (argument == "--diverse-maps") trainingOptions.diverseMaps = true;
            else if (argument == "--hard-case-curriculum")
                trainingOptions.hardCaseCurriculum = true;
            else if (argument == "--hard-case-curriculum-v2")
                trainingOptions.hardCaseCurriculumV2 = true;
            else if (argument == "--perception-curriculum")
                trainingOptions.perceptionCurriculum = true;
            else trainingOptions.combatPressureCurriculum = true;
            trainingOnlyArgumentSeen = true;
            continue;
        }

        if (argument == "--cooperative-brain") {
            trainingOptions.brainContract
                = gdp::gameplay::EnemyBrainContract::Cooperative;
            trainingOnlyArgumentSeen = true;
            continue;
        }

        if (argument == "--population" || argument == "--generations"
            || argument == "--cooldown-ms" || argument == "--curriculum-generations") {
            if (index + 1 >= argc) {
                std::cerr << "Missing value after " << argument << '\n';
                return 2;
            }
            const auto value = parsePositiveU32(argv[++index]);
            if (!value) {
                std::cerr << "Invalid value for " << argument << '\n';
                return 2;
            }
            if (argument == "--population") {
                trainingOptions.population = *value;
            } else if (argument == "--generations") {
                trainingOptions.generations = *value;
            } else if (argument == "--curriculum-generations") {
                trainingOptions.curriculumGenerations = *value;
            } else {
                trainingOptions.cooldownMilliseconds = *value;
            }
            trainingOnlyArgumentSeen = true;
            continue;
        }

        if (argument == "--save-dir" || argument == "--resume"
            || argument == "--resume-population") {
            if (index + 1 >= argc) {
                std::cerr << "Missing value after " << argument << '\n';
                return 2;
            }
            const std::filesystem::path value{argv[++index]};
            if (value.empty()) {
                std::cerr << "Empty path for " << argument << '\n';
                return 2;
            }
            if (argument == "--save-dir") {
                trainingOptions.saveDirectory = value;
            } else if (argument == "--resume-population") {
                trainingOptions.resumePopulation = value;
            } else {
                trainingOptions.resumeBrain = value;
            }
            trainingOnlyArgumentSeen = true;
            continue;
        }

        if (argument == "--benchmark") {
            if (index + 1 >= argc) {
                std::cerr << "Missing value after --benchmark\n";
                return 2;
            }
            const auto quality
                = gdp::gameplay::graphicsQualityForName(argv[++index]);
            if (!quality) {
                std::cerr << "Invalid benchmark profile: " << argv[index]
                          << " (expected POTATO, LOW, MEDIUM, HIGH, or ULTRA)\n";
                return 2;
            }
            config.benchmarkQuality = *quality;
            continue;
        }

        if (argument == "--frames") {
            if (index + 1 >= argc) {
                std::cerr << "Missing value after --frames\n";
                return 2;
            }

            const auto frameLimit = parseFrameLimit(argv[++index]);
            if (!frameLimit) {
                std::cerr << "Invalid frame limit: " << argv[index] << '\n';
                return 2;
            }

            config.frameLimit = frameLimit;
            continue;
        }

        if (argument == "--seed") {
            if (index + 1 >= argc) {
                std::cerr << "Missing value after --seed\n";
                return 2;
            }
            const auto seed = parseSeed(argv[++index]);
            if (!seed) {
                std::cerr << "Invalid world seed: " << argv[index] << '\n';
                return 2;
            }
            requestedSeed = *seed;
            continue;
        }

        std::cerr << "Unknown argument: " << argument << '\n';
        printUsage(argv[0]);
        return 2;
    }

    if (trainingMode) {
        if (config.benchmarkQuality || config.frameLimit
            || config.testMode || config.storageRoomPreview
            || config.combatRoomPreview
            || config.bossRoomPreview
            || config.cafeteriaPreview
            || config.cafeteriaLongPreview
            || config.laboratoryPreview
            || config.technicalPreview
            || config.technicalLPreview
            || config.securityPreview
            || config.securityTPreview
            || config.securityIndicatorDemo
            || config.centralHubPreview
            || config.industrialCorridorPreview
            || config.scriptedEnemies || config.experimentalNeuralEnemy
            || config.neuralBrainPath || config.testBrainDirectory) {
            std::cerr << "Training cannot be combined with gameplay or benchmark options\n";
            return 2;
        }
        if (trainAll && (trainingOptions.resumeBrain || trainingOptions.resumePopulation)) {
            std::cerr << "resume requires a single training archetype\n";
            return 2;
        }
        if (trainAll && trainingOptions.combatPressureCurriculum) {
            std::cerr << "combat-pressure curriculum requires Guard or Engineer\n";
            return 2;
        }
        trainingOptions.seed = requestedSeed.value_or(12'345U);
        trainingOptions.seedExplicit = requestedSeed.has_value();
        if (trainAll) {
            const auto root = trainingOptions.saveDirectory;
            if (root) {
                std::error_code error;
                if (std::filesystem::exists(*root, error)
                    ? error || !std::filesystem::is_directory(*root, error)
                        || !std::filesystem::is_empty(*root, error)
                    : error || !std::filesystem::create_directories(*root, error)) {
                    std::cerr << "AI training failed: save directory must be new or empty\n";
                    return 1;
                }
            }
            constexpr std::string_view names[]{
                "worker", "guard", "officer", "engineer", "robot"};
            for (std::size_t type = 0U; type < 5U; ++type) {
                trainingOptions.archetype
                    = static_cast<gdp::entities::EnemyArchetype>(type);
                if (root) trainingOptions.saveDirectory = *root / names[type];
                const auto result = gdp::game::ai::runEnemyTraining(
                    trainingOptions, std::cout);
                if (!result.succeeded) {
                    std::cerr << "AI training failed for " << names[type]
                              << ": " << result.error << '\n';
                    return 1;
                }
            }
        } else {
            const auto result = gdp::game::ai::runEnemyTraining(
                trainingOptions, std::cout);
            if (!result.succeeded) {
                std::cerr << "AI training failed: " << result.error << '\n';
                return 1;
            }
        }
        return 0;
    }
    if (trainingOnlyArgumentSeen) {
        std::cerr << "Training options need --ai-training\n";
        return 2;
    }
    if (config.neuralBrainPath && !config.experimentalNeuralEnemy) {
        std::cerr << "--neural-brain needs --experimental-neural-enemy TYPE\n";
        return 2;
    }
    if (config.scriptedEnemies && config.experimentalNeuralEnemy) {
        std::cerr << "--scripted-enemies cannot be combined with experimental neural AI\n";
        return 2;
    }
    if (config.testBrainDirectory && (config.scriptedEnemies
            || config.experimentalNeuralEnemy || config.neuralBrainPath)) {
        std::cerr << "--test-brain-dir cannot be combined with other enemy AI modes\n";
        return 2;
    }
    if (config.headless && config.benchmarkQuality) {
        std::cerr << "--headless and --benchmark cannot be used together\n";
        return 2;
    }
    if (config.securityIndicatorDemo && !config.securityTPreview) {
        std::cerr << "--security-indicator-demo needs --security-t-preview\n";
        return 2;
    }
    if (static_cast<int>(config.storageRoomPreview)
            + static_cast<int>(config.combatRoomPreview)
            + static_cast<int>(config.bossRoomPreview)
            + static_cast<int>(config.cafeteriaPreview)
            + static_cast<int>(config.cafeteriaLongPreview)
            + static_cast<int>(config.laboratoryPreview)
            + static_cast<int>(config.technicalPreview)
            + static_cast<int>(config.technicalLPreview)
            + static_cast<int>(config.securityPreview)
            + static_cast<int>(config.securityTPreview)
            + static_cast<int>(config.centralHubPreview)
            + static_cast<int>(config.industrialCorridorPreview)
            + static_cast<int>(config.testMode) > 1) {
        std::cerr << "Select one room preview or --test-mode\n";
        return 2;
    }
    if (config.benchmarkQuality && !config.frameLimit) {
        config.frameLimit = gdp::game::performanceSampleWindow * 2U;
    }

    const bool deterministicLaunch
        = config.headless || config.benchmarkQuality.has_value();
    const wkc::core::Seed launchEntropy
        = !deterministicLaunch && !requestedSeed ? collectLaunchEntropy() : 0U;
    config.worldSeed = gdp::game::selectLaunchSeed(
        deterministicLaunch, requestedSeed, launchEntropy);

    gdp::game::Application application{config};
    return application.run();
}
