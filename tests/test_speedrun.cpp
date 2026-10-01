#include "gameplay/Speedrun.hpp"

#include <iostream>
#include <optional>
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

gdp::gameplay::SessionState completedSpeedrun(
    const std::uint64_t ticks,
    const wkc::core::Seed seed = 12345U)
{
    gdp::gameplay::SessionState state;
    state.phase = gdp::gameplay::SessionPhase::Victory;
    state.mode = gdp::gameplay::RunMode::Speedrun;
    state.currentSeed = seed;
    state.elapsedRunTicks = ticks;
    state.difficulty = gdp::gameplay::Difficulty::Hard;
    return state;
}

void testEligibility()
{
    using namespace gdp::gameplay;
    std::optional<SpeedrunRecord> record;

    SessionState standard = completedSpeedrun(600U);
    standard.mode = RunMode::Standard;
    expect(considerSpeedrunResult(standard, record)
                == SpeedrunRecordResult::Ineligible
            && !record,
        "a Standard victory cannot create a Speedrun record");

    SessionState defeat = completedSpeedrun(600U);
    defeat.phase = SessionPhase::Defeat;
    expect(considerSpeedrunResult(defeat, record)
                == SpeedrunRecordResult::Ineligible
            && !record,
        "a Speedrun defeat cannot create a record");

    expect(considerSpeedrunResult(completedSpeedrun(0U), record)
                == SpeedrunRecordResult::Ineligible
            && !record,
        "a zero-duration result is rejected as invalid timing data");
}

void testPersonalBestLifecycle()
{
    using namespace gdp::gameplay;
    std::optional<SpeedrunRecord> record;

    expect(considerSpeedrunResult(completedSpeedrun(900U, 11U), record)
                == SpeedrunRecordResult::FirstRecord
            && record == SpeedrunRecord{currentSpeedrunRulesVersion, 900U, 11U},
        "the first valid Speedrun victory establishes a personal best");
    expect(speedrunRecordChanged(SpeedrunRecordResult::FirstRecord)
            && speedrunRecordChanged(SpeedrunRecordResult::Improved)
            && !speedrunRecordChanged(SpeedrunRecordResult::Kept)
            && !speedrunRecordChanged(SpeedrunRecordResult::Ineligible),
        "only first and improved results require persistence");

    expect(considerSpeedrunResult(completedSpeedrun(900U, 22U), record)
                == SpeedrunRecordResult::Kept
            && record->bestTicks == 900U && record->seed == 11U,
        "an equal time preserves the original record and seed");
    expect(considerSpeedrunResult(completedSpeedrun(1'200U, 33U), record)
                == SpeedrunRecordResult::Kept
            && record->bestTicks == 900U && record->seed == 11U,
        "a slower victory never overwrites the personal best");
    expect(considerSpeedrunResult(completedSpeedrun(720U, 44U), record)
                == SpeedrunRecordResult::Improved
            && record == SpeedrunRecord{currentSpeedrunRulesVersion, 720U, 44U},
        "a faster victory replaces both time and seed");
}

void testRulesVersionIsolation()
{
    using namespace gdp::gameplay;
    std::optional<SpeedrunRecord> record
        = SpeedrunRecord{currentSpeedrunRulesVersion + 1U, 100U, 99U};
    expect(considerSpeedrunResult(completedSpeedrun(800U, 55U), record)
                == SpeedrunRecordResult::FirstRecord
            && record
            == SpeedrunRecord{currentSpeedrunRulesVersion, 800U, 55U},
        "a record from different rules cannot compete with the current ruleset");
}

} // namespace

int main()
{
    testEligibility();
    testPersonalBestLifecycle();
    testRulesVersionIsolation();
    if (failures != 0) {
        std::cerr << failures << " Speedrun test(s) failed\n";
        return 1;
    }
    std::cout << "All GDP Speedrun tests passed\n";
    return 0;
}
