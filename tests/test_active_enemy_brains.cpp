#include "game/ai/ActiveEnemyBrains.hpp"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
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

bool copyWithCrLf(const std::filesystem::path& sourcePath,
    const std::filesystem::path& destinationPath)
{
    std::ifstream source(sourcePath, std::ios::binary);
    if (!source) {
        return false;
    }
    const std::string original{std::istreambuf_iterator<char>{source},
        std::istreambuf_iterator<char>{}};
    if (source.bad()) {
        return false;
    }
    std::ofstream destination(destinationPath,
        std::ios::binary | std::ios::trunc);
    if (!destination) {
        return false;
    }
    for (const char character : original) {
        if (character == '\n') {
            destination.put('\r');
        }
        destination.put(character);
    }
    destination.close();
    return destination.good();
}

} // namespace

int main()
{
    namespace fs = std::filesystem;
    const fs::path bundled = fs::path{GDP_SOURCE_DIRECTORY} / "assets" / "ai";
    const auto brains = gdp::game::ai::loadActiveEnemyBrains(bundled);
    for (std::size_t index = 0U; index < brains.size(); ++index) {
        expect(brains[index]
                && static_cast<std::size_t>(brains[index]->archetype) == index
                && brains[index]->genome.generation == 200U
                && brains[index]->trainingSeed == 42U,
            "every bundled brain has the expected type and training snapshot");
    }

    const fs::path missing = bundled / "does-not-exist";
    const auto absent = gdp::game::ai::loadActiveEnemyBrains(missing);
    for (const auto& brain : absent) {
        expect(!brain, "missing brain falls back to scripted AI");
    }

    const fs::path temporary = fs::temp_directory_path()
        / ("gdp_active_brains_test_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code error;
    if (!fs::create_directory(temporary, error) || error) {
        expect(false, "test creates a unique temporary directory");
        return 1;
    }
    constexpr std::array<std::string_view, 5> names{
        "worker", "guard", "officer", "engineer", "robot"};
    for (const std::string_view name : names) {
        const std::string filename = std::string{name} + ".brain";
        expect(copyWithCrLf(bundled / filename, temporary / filename),
            "test creates a CRLF copy of each bundled brain");
    }
    const auto windowsLineEndings = gdp::game::ai::loadActiveEnemyBrains(temporary);
    for (std::size_t index = 0U; index < windowsLineEndings.size(); ++index) {
        expect(windowsLineEndings[index]
                && static_cast<std::size_t>(
                    windowsLineEndings[index]->archetype) == index
                && windowsLineEndings[index]->genome.generation == 200U
                && windowsLineEndings[index]->trainingSeed == 42U,
            "CRLF transport preserves each checked brain");
    }

    fs::copy_file(bundled / "worker.brain", temporary / "guard.brain",
        fs::copy_options::overwrite_existing, error);
    expect(!error, "test copies a mismatched brain under the guard filename");
    const auto mismatched = gdp::game::ai::loadActiveEnemyBrains(temporary);
    expect(!mismatched[1U], "wrong archetype brain is rejected per enemy type");
    fs::remove_all(temporary, error);
    expect(!error, "test removes only its own temporary directory");
    return failures == 0 ? 0 : 1;
}
