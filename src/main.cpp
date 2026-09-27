#include "game/Application.hpp"

#include "core/Hash.hpp"
#include "game/LaunchSeed.hpp"
#include "game/ai/WorkerTraining.hpp"

#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <random>
#include <string_view>

namespace {

void printUsage(const std::string_view executable)
{
    std::cout << "Usage: " << executable
              << " [--headless] [--benchmark PROFILE] [--frames N] [--seed N]"
                 " [--experimental-neural-worker] [--neural-brain FILE] [--help]\n"
              << "       " << executable
              << " --ai-training worker [--population N] [--generations N]"
                 " [--seed N] [--save-dir DIR] [--resume FILE]\n"
              << "  --headless           Run without creating a window or OpenGL context.\n"
              << "  --benchmark PROFILE  Measure POTATO, LOW, MEDIUM, HIGH, or ULTRA at 1280x720.\n"
              << "  --frames N           Stop after exactly N rendered loop iterations.\n"
              << "  --seed N             Generate a deterministic world from unsigned integer N.\n"
              << "  --experimental-neural-worker  Test one seeded neural Worker per run.\n"
              << "  --neural-brain FILE  Load a saved brain for that experimental Worker.\n"
              << "  --ai-training worker  Train Worker brains headlessly; no normal run.\n"
              << "  --population N      Training population (2..128, default 16).\n"
              << "  --generations N     Training generations (1..10000, default 10).\n"
              << "  --save-dir DIR      Save one best-brain checkpoint per generation.\n"
              << "  --resume FILE       Seed a new training run from a checkpoint.\n"
              << "  --help               Show this message.\n";
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
    gdp::game::ApplicationConfig config;
    gdp::game::ai::WorkerTrainingOptions trainingOptions;
    bool trainingMode = false;
    bool trainingOnlyArgumentSeen = false;
    std::optional<wkc::core::Seed> requestedSeed;

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};

        if (argument == "--help") {
            printUsage(argv[0]);
            return 0;
        }

        if (argument == "--headless") {
            config.headless = true;
            continue;
        }

        if (argument == "--experimental-neural-worker") {
            config.experimentalNeuralWorker = true;
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

        if (argument == "--ai-training") {
            if (index + 1 >= argc || std::string_view{argv[++index]} != "worker") {
                std::cerr << "--ai-training currently supports only worker\n";
                return 2;
            }
            trainingMode = true;
            continue;
        }

        if (argument == "--population" || argument == "--generations") {
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
            } else {
                trainingOptions.generations = *value;
            }
            trainingOnlyArgumentSeen = true;
            continue;
        }

        if (argument == "--save-dir" || argument == "--resume") {
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
            || config.experimentalNeuralWorker || config.neuralBrainPath) {
            std::cerr << "Training cannot be combined with gameplay or benchmark options\n";
            return 2;
        }
        trainingOptions.seed = requestedSeed.value_or(12'345U);
        trainingOptions.seedExplicit = requestedSeed.has_value();
        const auto result = gdp::game::ai::runWorkerTraining(trainingOptions, std::cout);
        if (!result.succeeded) {
            std::cerr << "AI training failed: " << result.error << '\n';
            return 1;
        }
        return 0;
    }
    if (trainingOnlyArgumentSeen) {
        std::cerr << "--population, --generations, --save-dir and --resume need --ai-training\n";
        return 2;
    }
    if (config.neuralBrainPath && !config.experimentalNeuralWorker) {
        std::cerr << "--neural-brain needs --experimental-neural-worker\n";
        return 2;
    }
    if (config.headless && config.benchmarkQuality) {
        std::cerr << "--headless and --benchmark cannot be used together\n";
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
