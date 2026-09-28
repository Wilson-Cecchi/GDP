#pragma once

#include "persistence/WorkerBrainStore.hpp"

#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>

namespace gdp::game::ai {

constexpr std::size_t activeEnemyBrainCount = 5U;
using ActiveEnemyBrainCatalog = std::array<
    std::optional<persistence::EnemyBrainRecord>, activeEnemyBrainCount>;

[[nodiscard]] ActiveEnemyBrainCatalog loadActiveEnemyBrains(
    const std::filesystem::path& directory);

} // namespace gdp::game::ai
