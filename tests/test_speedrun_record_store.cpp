#include "persistence/SpeedrunRecordStore.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

class TemporaryDirectory {
public:
    TemporaryDirectory()
    {
        const auto suffix = std::chrono::steady_clock::now()
                                .time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path()
            / ("gdp_speedrun_record_" + std::to_string(suffix));
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept
    {
        return path_;
    }

private:
    std::filesystem::path path_;
};

void writeText(const std::filesystem::path& path, const std::string_view text)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
}

void testMissingAndRoundTrip()
{
    using namespace gdp;
    TemporaryDirectory directory;
    const std::filesystem::path path
        = directory.path() / "speedrun_record.toml";

    const persistence::SpeedrunRecordLoadResult missing
        = persistence::loadSpeedrunRecord(path);
    expect(missing.status == persistence::SpeedrunRecordLoadStatus::Missing
            && !missing.record,
        "a missing record file is a valid empty state");

    gameplay::SpeedrunRecord record{
        gameplay::currentSpeedrunRulesVersion, 7'654U, 123'456U};
    expect(persistence::saveSpeedrunRecord(path, record),
        "a valid record is written through a validated temporary file");
    const persistence::SpeedrunRecordLoadResult loaded
        = persistence::loadSpeedrunRecord(path);
    expect(loaded.status == persistence::SpeedrunRecordLoadStatus::Loaded
            && loaded.record == record,
        "record time, seed and rules survive a disk round trip");
    expect(!std::filesystem::exists(path.string() + ".tmp"),
        "successful save leaves no temporary record file");

    record.bestTicks = 6'000U;
    record.seed = 999U;
    expect(persistence::saveSpeedrunRecord(path, record)
            && persistence::loadSpeedrunRecord(path).record == record,
        "atomic replacement updates an existing personal best");
}

void testInvalidAndVersionedData()
{
    using namespace gdp;
    TemporaryDirectory directory;
    const std::filesystem::path path
        = directory.path() / "speedrun_record.toml";

    writeText(path,
        "version = 1\n"
        "rules_version = 18\n"
        "best_ticks = 0\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::Invalid,
        "zero-duration records are rejected");

    writeText(path,
        "version = 99\n"
        "rules_version = 1\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::UnsupportedVersion,
        "unknown file schemas are not interpreted as current data");

    writeText(path,
        "version = 1\n"
        "rules_version = 99\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from another ruleset remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 1\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before procedural Armor remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 2\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before the Carbine remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 3\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before Arena Overdrive remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 4\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before deterministic obstacle avoidance remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 5\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before deterministic enemy separation remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 6\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before hard enemy contact volumes remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 7\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before deterministic patrol remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 8\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before enemy investigation remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 9\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before the START validation cover remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 10\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before combat-noise perception remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 11\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before the dedicated Alert state remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 12\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before the Recover state remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 13\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before global enemy pathfinding remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 14\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before directional enemy vision remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 15\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before group route diversity remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 16\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before material impact feedback remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 17\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before synchronized door motion remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 18\n"
        "best_ticks = six hundred\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::Invalid,
        "malformed timing data is rejected safely");

    writeText(path,
        "version = 1\n"
        "rules_version = 19\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before weak points remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 20\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before deterministic weapon spread remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 21\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before automatic Carbine fire remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 22\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before timed reloads remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 23\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before two-slot weapon loadouts remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 24\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before authoritative weapon switching remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 25\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before responsive weapon selection remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 26\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before the permanent Knife fallback remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 27\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before crouch and jump movement remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 28\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before encounter variation remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 29\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before route layout variation remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 30\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before corridor combat remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 31\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before cross-room enemy combat remain isolated");

    writeText(path,
        "version = 1\n"
        "rules_version = 32\n"
        "best_ticks = 600\n"
        "seed = 123\n");
    expect(persistence::loadSpeedrunRecord(path).status
            == persistence::SpeedrunRecordLoadStatus::IncompatibleRules,
        "records from before moving enemy searches remain isolated");

    const gameplay::SpeedrunRecord invalid{
        gameplay::currentSpeedrunRulesVersion, 0U, 123U};
    expect(!persistence::saveSpeedrunRecord(path, invalid),
        "invalid in-memory timing data is never persisted");
}

void testCommentsAndFutureFields()
{
    using namespace gdp;
    TemporaryDirectory directory;
    const std::filesystem::path path
        = directory.path() / "speedrun_record.toml";
    writeText(path,
        "# local record\nversion = 1\nrules_version = "
            + std::to_string(gameplay::currentSpeedrunRulesVersion)
            + "\nbest_ticks = 321 # fixed ticks\nseed = 987\n"
              "future_field = preserved_by_future_versions\n");
    const persistence::SpeedrunRecordLoadResult loaded
        = persistence::loadSpeedrunRecord(path);
    expect(loaded.status == persistence::SpeedrunRecordLoadStatus::Loaded
            && loaded.record
            == gameplay::SpeedrunRecord{
                gameplay::currentSpeedrunRulesVersion, 321U, 987U},
        "comments and unknown future fields remain forward compatible");
}

} // namespace

int main()
{
    testMissingAndRoundTrip();
    testInvalidAndVersionedData();
    testCommentsAndFutureFields();
    if (failures != 0) {
        std::cerr << failures << " Speedrun record persistence test(s) failed\n";
        return 1;
    }
    std::cout << "All GDP Speedrun record persistence tests passed\n";
    return 0;
}
