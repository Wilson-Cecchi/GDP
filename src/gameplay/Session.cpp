#include "gameplay/Session.hpp"

#include "game/SeedDomains.hpp"

#include <limits>

namespace gdp::gameplay {

SessionSystem::SessionSystem(const wkc::core::Seed initialSeed) noexcept
{
    state_.initialSeed = initialSeed;
    state_.currentSeed = initialSeed;
}

SessionUpdate SessionSystem::update(
    const bool playerAlive,
    const gdp::world::RoomRole currentRoomRole,
    const bool bossExists,
    const bool bossAlive) noexcept
{
    SessionUpdate update{state_.phase, state_.phase, false};
    if (state_.phase != SessionPhase::Playing) {
        return update;
    }
    if (!playerAlive) {
        state_.phase = SessionPhase::Defeat;
    } else if (currentRoomRole == gdp::world::RoomRole::Boss && bossExists) {
        state_.bossEngaged = state_.bossEngaged || bossAlive;
        if (state_.bossEngaged && !bossAlive) {
            state_.phase = SessionPhase::Victory;
        }
    }
    update.phase = state_.phase;
    update.changed = update.phase != update.previousPhase;
    return update;
}

std::optional<wkc::core::Seed> SessionSystem::restart() noexcept
{
    if (state_.phase == SessionPhase::Playing) {
        return std::nullopt;
    }
    ++state_.runIndex;
    state_.currentSeed = seedForRun(state_.initialSeed, state_.runIndex);
    state_.phase = SessionPhase::Playing;
    state_.bossEngaged = false;
    state_.elapsedRunTicks = 0U;
    return state_.currentSeed;
}

bool SessionSystem::selectRunMode(const RunMode mode) noexcept
{
    if (mode >= RunMode::Count || state_.phase != SessionPhase::Playing
        || state_.elapsedRunTicks != 0U || state_.bossEngaged) {
        return false;
    }
    state_.mode = mode;
    return true;
}

void SessionSystem::advanceRunTimer(const bool gameplayActive) noexcept
{
    if (state_.phase == SessionPhase::Playing
        && (gameplayActive || state_.mode == RunMode::Speedrun)
        && state_.elapsedRunTicks < std::numeric_limits<std::uint64_t>::max()) {
        ++state_.elapsedRunTicks;
    }
}

const SessionState& SessionSystem::state() const noexcept
{
    return state_;
}

wkc::core::Seed seedForRun(
    const wkc::core::Seed initialSeed,
    const std::uint32_t runIndex) noexcept
{
    if (runIndex == 0U) {
        return initialSeed;
    }
    return wkc::core::deriveSeed(
        initialSeed, gdp::game::SeedDomain::World, runIndex);
}

const char* sessionPhaseName(const SessionPhase phase) noexcept
{
    switch (phase) {
    case SessionPhase::Playing:
        return "Playing";
    case SessionPhase::Victory:
        return "Victory";
    case SessionPhase::Defeat:
        return "Defeat";
    }
    return "Unknown";
}

const char* runModeName(const RunMode mode) noexcept
{
    switch (mode) {
    case RunMode::Standard:
        return "Standard";
    case RunMode::Speedrun:
        return "Speedrun";
    case RunMode::Count:
        break;
    }
    return "Unknown";
}

BossStatus bossStatus(
    const gdp::entities::EntityManager& entities,
    const gdp::world::RoomId roomId) noexcept
{
    BossStatus status;
    for (const gdp::entities::EntityId id
        : entities.query(gdp::entities::EntityKind::Enemy)) {
        const gdp::entities::Entity* entity = entities.find(id);
        const gdp::entities::EnemyData* enemy
            = entities.findData<gdp::entities::EnemyData>(id);
        if (entity == nullptr || enemy == nullptr || entity->roomId != roomId
            || enemy->archetype
                != gdp::entities::EnemyArchetype::IndustrialRobot) {
            continue;
        }
        status.exists = true;
        status.alive = status.alive
            || enemy->behavior != gdp::entities::EnemyBehaviorState::Dead;
    }
    return status;
}

} // namespace gdp::gameplay
