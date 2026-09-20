#include "game/Application.hpp"

#include "core/Hash.hpp"
#include "game/LaunchSeed.hpp"

#include <charconv>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <optional>
#include <random>
#include <string_view>

namespace {

void printUsage(const std::string_view executable)
{
    std::cout << "Usage: " << executable
              << " [--headless] [--benchmark PROFILE] [--frames N] [--seed N] [--help]\n"
              << "  --headless           Run without creating a window or OpenGL context.\n"
              << "  --benchmark PROFILE  Measure POTATO, LOW, MEDIUM, HIGH, or ULTRA at 1280x720.\n"
              << "  --frames N           Stop after exactly N rendered loop iterations.\n"
              << "  --seed N             Generate a deterministic world from unsigned integer N.\n"
              << "  --help               Show this message.\n";
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
