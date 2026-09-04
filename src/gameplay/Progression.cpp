#include "gameplay/Progression.hpp"

#include <algorithm>
#include <array>
#include <limits>

namespace gdp::gameplay {
namespace {

struct ProgressionValue {
    std::uint32_t experience;
    std::uint32_t score;
};

constexpr std::array progressionValues{
    ProgressionValue{25U, 250U},
    ProgressionValue{40U, 450U},
    ProgressionValue{55U, 650U},
    ProgressionValue{50U, 600U},
    ProgressionValue{200U, 3'000U},
    ProgressionValue{10U, 100U},
    ProgressionValue{5U, 50U},
    ProgressionValue{75U, 1'000U},
};

std::uint32_t saturatedAdd(
    const std::uint32_t value,
    const std::uint32_t amount) noexcept
{
    return amount > std::numeric_limits<std::uint32_t>::max() - value
        ? std::numeric_limits<std::uint32_t>::max() : value + amount;
}

std::uint64_t saturatedAdd(
    const std::uint64_t value,
    const std::uint64_t amount) noexcept
{
    return amount > std::numeric_limits<std::uint64_t>::max() - value
        ? std::numeric_limits<std::uint64_t>::max() : value + amount;
}

std::uint32_t saturatedMultiply(
    const std::uint32_t value,
    const std::uint32_t count) noexcept
{
    if (value != 0U && count > std::numeric_limits<std::uint32_t>::max() / value) {
        return std::numeric_limits<std::uint32_t>::max();
    }
    return value * count;
}

std::uint64_t saturatedMultiply64(
    const std::uint32_t value,
    const std::uint32_t count) noexcept
{
    return static_cast<std::uint64_t>(value) * static_cast<std::uint64_t>(count);
}

void applyReward(
    const ProgressionReward reward,
    ProgressionState& state,
    Player& player,
    Weapon& weapon) noexcept
{
    if (reward == ProgressionReward::Vitality) {
        ++state.vitalityRank;
        (void)player.increaseMaximumHealth(15.0F, 15.0F);
        (void)player.increaseMaximumArmor(10.0F, 10.0F);
    } else if (reward == ProgressionReward::Firepower) {
        ++state.firepowerRank;
        (void)weapon.increaseDamage(4.0F);
    } else {
        ++state.capacityRank;
        (void)weapon.increaseReserveCapacity(4U, 4U);
    }
}

std::uint32_t expectedRanks(
    const std::uint32_t level,
    const ProgressionReward reward) noexcept
{
    std::uint32_t count = 0U;
    for (std::uint32_t current = 2U; current <= level; ++current) {
        if (rewardForLevel(current) == reward) {
            ++count;
        }
    }
    return count;
}

} // namespace

ProgressionUpdate ProgressionSystem::record(
    const ProgressionEvent event,
    const std::uint32_t count,
    Player& player,
    Weapon& weapon)
{
    ProgressionUpdate update;
    update.event = event;
    update.count = count;
    if (event >= ProgressionEvent::Count || count == 0U) {
        return update;
    }

    const ProgressionValue value = progressionValues[static_cast<std::size_t>(event)];
    update.experienceAwarded = saturatedMultiply(value.experience, count);
    update.scoreAwarded = saturatedMultiply64(value.score, count);
    state_.totalExperience = saturatedAdd(
        state_.totalExperience, static_cast<std::uint64_t>(update.experienceAwarded));
    state_.score = saturatedAdd(state_.score, update.scoreAwarded);

    if (event <= ProgressionEvent::IndustrialRobotKilled) {
        state_.enemiesKilled = saturatedAdd(state_.enemiesKilled, count);
    } else if (event == ProgressionEvent::PropDestroyed) {
        state_.propsDestroyed = saturatedAdd(state_.propsDestroyed, count);
    } else if (event == ProgressionEvent::LootCollected) {
        state_.lootCollected = saturatedAdd(state_.lootCollected, count);
    } else if (event == ProgressionEvent::RoomEventCompleted) {
        state_.eventsCompleted = saturatedAdd(state_.eventsCompleted, count);
    }

    if (state_.level >= maximumProgressionLevel) {
        state_.experienceIntoLevel = 0U;
        return update;
    }
    state_.experienceIntoLevel = saturatedAdd(
        state_.experienceIntoLevel, update.experienceAwarded);
    while (state_.level < maximumProgressionLevel) {
        const std::uint32_t required = experienceForNextLevel(state_.level);
        if (required == 0U || state_.experienceIntoLevel < required) {
            break;
        }
        state_.experienceIntoLevel -= required;
        ++state_.level;
        ++update.levelsGained;
        const ProgressionReward reward = rewardForLevel(state_.level);
        update.rewards.push_back(reward);
        applyReward(reward, state_, player, weapon);
    }
    if (state_.level >= maximumProgressionLevel) {
        state_.experienceIntoLevel = 0U;
    }
    return update;
}

const ProgressionState& ProgressionSystem::state() const noexcept
{
    return state_;
}

bool ProgressionSystem::restoreCampaignProgress(
    const ProgressionState& savedState) noexcept
{
    if (!validateProgressionState(savedState)) {
        return false;
    }
    state_ = savedState;
    return true;
}

void ProgressionSystem::reset() noexcept
{
    state_ = {};
}

std::uint32_t experienceForNextLevel(const std::uint32_t level) noexcept
{
    if (level == 0U || level >= maximumProgressionLevel) {
        return 0U;
    }
    return 100U + ((level - 1U) * 75U);
}

ProgressionReward rewardForLevel(const std::uint32_t level) noexcept
{
    if (level % 3U == 0U) {
        return ProgressionReward::Firepower;
    }
    if (level % 3U == 1U) {
        return ProgressionReward::Capacity;
    }
    return ProgressionReward::Vitality;
}

bool validateProgressionState(const ProgressionState& state) noexcept
{
    if (state.level == 0U || state.level > maximumProgressionLevel) {
        return false;
    }
    const std::uint32_t required = experienceForNextLevel(state.level);
    if ((state.level < maximumProgressionLevel
            && state.experienceIntoLevel >= required)
        || (state.level == maximumProgressionLevel
            && state.experienceIntoLevel != 0U)) {
        return false;
    }
    return state.vitalityRank == expectedRanks(state.level, ProgressionReward::Vitality)
        && state.firepowerRank == expectedRanks(state.level, ProgressionReward::Firepower)
        && state.capacityRank == expectedRanks(state.level, ProgressionReward::Capacity);
}

} // namespace gdp::gameplay
