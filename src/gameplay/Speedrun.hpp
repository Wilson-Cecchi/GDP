#pragma once

#include "core/Seed.hpp"
#include "gameplay/Session.hpp"

#include <cstdint>
#include <optional>

namespace gdp::gameplay {

inline constexpr std::uint32_t currentSpeedrunRulesVersion = 36U;

struct SpeedrunRecord {
    std::uint32_t rulesVersion{currentSpeedrunRulesVersion};
    std::uint64_t bestTicks{0U};
    wkc::core::Seed seed{0U};

    constexpr bool operator==(const SpeedrunRecord&) const noexcept = default;
};

enum class SpeedrunRecordResult : std::uint8_t {
    Ineligible,
    FirstRecord,
    Improved,
    Kept,
};

[[nodiscard]] SpeedrunRecordResult considerSpeedrunResult(
    const SessionState& session,
    std::optional<SpeedrunRecord>& record) noexcept;
[[nodiscard]] bool speedrunRecordChanged(
    SpeedrunRecordResult result) noexcept;

} // namespace gdp::gameplay
