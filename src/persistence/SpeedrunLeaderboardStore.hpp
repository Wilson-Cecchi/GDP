#pragma once
#include "gameplay/Speedrun.hpp"
#include <filesystem>
#include <vector>
namespace gdp::persistence {
using SpeedrunLeaderboard = std::vector<gameplay::SpeedrunRecord>;
[[nodiscard]] bool addSpeedrunLeaderboardResult(SpeedrunLeaderboard&, const gameplay::SessionState&);
[[nodiscard]] SpeedrunLeaderboard loadSpeedrunLeaderboard(const std::filesystem::path&) noexcept;
[[nodiscard]] bool saveSpeedrunLeaderboard(const std::filesystem::path&, const SpeedrunLeaderboard&) noexcept;
}
