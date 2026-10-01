#pragma once

#include "gameplay/SettingsMenu.hpp"

#include <cstdint>
#include <filesystem>

namespace gdp::persistence {

inline constexpr std::uint32_t currentSettingsVersion = 7U;

enum class SettingsLoadStatus : std::uint8_t {
    Loaded,
    Migrated,
    LoadedWithRecovery,
    Missing,
    Invalid,
    UnsupportedVersion,
    IoError,
};

struct SettingsLoadResult {
    gameplay::RuntimeSettings settings{};
    SettingsLoadStatus status{SettingsLoadStatus::Missing};
};

[[nodiscard]] SettingsLoadResult loadRuntimeSettings(
    const std::filesystem::path& path) noexcept;
[[nodiscard]] bool saveRuntimeSettings(
    const std::filesystem::path& path,
    const gameplay::RuntimeSettings& settings) noexcept;

} // namespace gdp::persistence
