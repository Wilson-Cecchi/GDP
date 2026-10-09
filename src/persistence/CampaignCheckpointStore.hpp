#pragma once

#include "gameplay/Campaign.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>

namespace gdp::persistence {

inline constexpr std::uint32_t currentCampaignCheckpointFileVersion = 8U;

enum class CampaignCheckpointLoadStatus : std::uint8_t {
    Loaded,
    Missing,
    Invalid,
    UnsupportedVersion,
    IncompatibleRules,
    IoError,
};

struct CampaignCheckpointLoadResult {
    std::optional<gameplay::CampaignCheckpoint> checkpoint;
    CampaignCheckpointLoadStatus status{CampaignCheckpointLoadStatus::Missing};
    bool migratedRules{false};
};

[[nodiscard]] CampaignCheckpointLoadResult loadCampaignCheckpoint(
    const std::filesystem::path& path) noexcept;
[[nodiscard]] bool saveCampaignCheckpoint(
    const std::filesystem::path& path,
    const gameplay::CampaignCheckpoint& checkpoint) noexcept;
[[nodiscard]] bool removeCampaignCheckpoint(
    const std::filesystem::path& path) noexcept;

} // namespace gdp::persistence
