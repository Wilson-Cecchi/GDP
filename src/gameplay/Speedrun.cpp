#include "gameplay/Speedrun.hpp"

namespace gdp::gameplay {

SpeedrunRecordResult considerSpeedrunResult(
    const SessionState& session,
    std::optional<SpeedrunRecord>& record) noexcept
{
    if (session.mode != RunMode::Speedrun
        || session.difficulty != Difficulty::Hard
        || session.phase != SessionPhase::Victory
        || session.elapsedRunTicks == 0U) {
        return SpeedrunRecordResult::Ineligible;
    }

    const SpeedrunRecord candidate{
        currentSpeedrunRulesVersion,
        session.elapsedRunTicks,
        session.currentSeed,
    };
    if (!record || record->rulesVersion != currentSpeedrunRulesVersion) {
        record = candidate;
        return SpeedrunRecordResult::FirstRecord;
    }
    if (candidate.bestTicks < record->bestTicks) {
        record = candidate;
        return SpeedrunRecordResult::Improved;
    }
    return SpeedrunRecordResult::Kept;
}

bool speedrunRecordChanged(const SpeedrunRecordResult result) noexcept
{
    return result == SpeedrunRecordResult::FirstRecord
        || result == SpeedrunRecordResult::Improved;
}

} // namespace gdp::gameplay
