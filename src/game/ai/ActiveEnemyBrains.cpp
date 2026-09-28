#include "game/ai/ActiveEnemyBrains.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace gdp::game::ai {

static_assert(static_cast<std::size_t>(
    gdp::entities::EnemyArchetype::IndustrialRobot) + 1U
    == activeEnemyBrainCount);

ActiveEnemyBrainCatalog loadActiveEnemyBrains(
    const std::filesystem::path& directory)
{
    constexpr std::array<std::string_view, activeEnemyBrainCount> names{
        "worker", "guard", "officer", "engineer", "robot"};
    ActiveEnemyBrainCatalog brains;
    for (std::size_t index = 0U; index < names.size(); ++index) {
        const std::filesystem::path path = directory
            / (std::string{names[index]} + ".brain");
        auto record = persistence::loadEnemyBrain(path);
        if (record && static_cast<std::size_t>(record->archetype) == index) {
            brains[index] = std::move(record);
        }
    }
    return brains;
}

} // namespace gdp::game::ai
