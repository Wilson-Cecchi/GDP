#pragma once

#include "gameplay/Speedrun.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>

namespace gdp::persistence {

inline constexpr std::uint32_t currentSpeedrunRecordFileVersion = 1U;

enum class SpeedrunRecordLoadStatus : std::uint8_t {
    Loaded,
    Missing,
    Invalid,
    UnsupportedVersion,
    IncompatibleRules,
    IoError,
};

struct SpeedrunRecordLoadResult {
    std::optional<gameplay::SpeedrunRecord> record;
    SpeedrunRecordLoadStatus status{SpeedrunRecordLoadStatus::Missing};
};

[[nodiscard]] SpeedrunRecordLoadResult loadSpeedrunRecord(
    const std::filesystem::path& path) noexcept;
[[nodiscard]] bool saveSpeedrunRecord(
    const std::filesystem::path& path,
    const gameplay::SpeedrunRecord& record) noexcept;

} // namespace gdp::persistence
