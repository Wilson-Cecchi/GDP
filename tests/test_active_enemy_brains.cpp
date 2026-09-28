#include "game/ai/ActiveEnemyBrains.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
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
    fs::copy_file(bundled / "worker.brain", temporary / "guard.brain", error);
    expect(!error, "test copies a mismatched brain under the guard filename");
    const auto mismatched = gdp::game::ai::loadActiveEnemyBrains(temporary);
    expect(!mismatched[1U], "wrong archetype brain is rejected per enemy type");
    fs::remove_all(temporary, error);
    expect(!error, "test removes only its own temporary directory");
    return failures == 0 ? 0 : 1;
}
