#pragma once

#include "gameplay/Player.hpp"
#include "gameplay/Weapon.hpp"

#include <cstdint>
#include <vector>

namespace gdp::gameplay {

inline constexpr std::uint32_t maximumProgressionLevel = 10U;

enum class ProgressionEvent : std::uint8_t {
    WorkerKilled,
    GuardKilled,
    OfficerKilled,
    EngineerKilled,
    IndustrialRobotKilled,
    PropDestroyed,
    LootCollected,
    RoomEventCompleted,
    Count,
};

enum class ProgressionReward : std::uint8_t {
    Vitality,
    Firepower,
    Capacity,
};

struct ProgressionState {
    std::uint32_t level{1U};
    std::uint32_t experienceIntoLevel{0U};
    std::uint64_t totalExperience{0U};
    std::uint64_t score{0U};
    std::uint32_t enemiesKilled{0U};
    std::uint32_t propsDestroyed{0U};
    std::uint32_t lootCollected{0U};
    std::uint32_t eventsCompleted{0U};
    std::uint32_t vitalityRank{0U};
    std::uint32_t firepowerRank{0U};
    std::uint32_t capacityRank{0U};

    constexpr bool operator==(const ProgressionState&) const noexcept = default;
};

struct ProgressionUpdate {
    ProgressionEvent event{ProgressionEvent::WorkerKilled};
    std::uint32_t count{0U};
    std::uint32_t experienceAwarded{0U};
    std::uint64_t scoreAwarded{0U};
    std::uint32_t levelsGained{0U};
    std::vector<ProgressionReward> rewards;
};

class ProgressionSystem {
public:
    [[nodiscard]] ProgressionUpdate record(
        ProgressionEvent event,
        std::uint32_t count,
        Player& player,
        Weapon& weapon);

    [[nodiscard]] const ProgressionState& state() const noexcept;
    [[nodiscard]] bool restoreCampaignProgress(
        const ProgressionState& savedState) noexcept;
    void reset() noexcept;

private:
    ProgressionState state_{};
};

[[nodiscard]] std::uint32_t experienceForNextLevel(
    std::uint32_t level) noexcept;
[[nodiscard]] ProgressionReward rewardForLevel(
    std::uint32_t level) noexcept;
[[nodiscard]] bool validateProgressionState(
    const ProgressionState& state) noexcept;

} // namespace gdp::gameplay
