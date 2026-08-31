#include "world/EventGenerator.hpp"

#include "core/Hash.hpp"
#include "core/Random.hpp"
#include "game/SeedDomains.hpp"

#include <algorithm>
#include <array>
#include <bit>

namespace gdp::world {
namespace {

bool containsPlanar(const SpawnZone& zone, const wkc::core::Vec3 position) noexcept
{
    return position.x >= zone.bounds.minimum.x && position.x <= zone.bounds.maximum.x
        && position.z >= zone.bounds.minimum.z && position.z <= zone.bounds.maximum.z
        && wkc::core::nearlyEqual(position.y, zone.bounds.minimum.y, 1.0e-4F);
}

wkc::core::Vec3 choosePosition(
    const wkc::core::Seed seed,
    const std::uint64_t ordinal,
    const std::span<const SpawnZone> zones)
{
    wkc::core::RandomStream random{wkc::core::deriveSeed(
        seed, gdp::game::SeedDomain::Events, 100U + ordinal)};
    const SpawnZone& zone = zones[static_cast<std::size_t>(random.rangeInt(
        0, static_cast<int>(zones.size() - 1U)))];
    constexpr std::array anchors{
        wkc::core::Vec2{0.18F, 0.72F},
        wkc::core::Vec2{0.82F, 0.28F},
        wkc::core::Vec2{0.28F, 0.22F},
        wkc::core::Vec2{0.72F, 0.78F},
    };
    const wkc::core::Vec2 anchor = anchors[ordinal % anchors.size()];
    const float normalizedX = std::clamp(
        anchor.x + random.rangeFloat(-0.035F, 0.035F), 0.10F, 0.90F);
    const float normalizedZ = std::clamp(
        anchor.y + random.rangeFloat(-0.035F, 0.035F), 0.10F, 0.90F);
    return {
        zone.bounds.minimum.x
            + ((zone.bounds.maximum.x - zone.bounds.minimum.x) * normalizedX),
        zone.bounds.minimum.y,
        zone.bounds.minimum.z
            + ((zone.bounds.maximum.z - zone.bounds.minimum.z) * normalizedZ),
    };
}

void addSpawn(
    RoomEventPlan& plan,
    const gdp::entities::EnemyArchetype archetype,
    const std::uint32_t wave,
    const std::span<const SpawnZone> zones,
    const float difficulty)
{
    if (zones.empty()) {
        return;
    }
    const std::uint64_t ordinal = plan.spawns.size();
    const float baseHealth = archetype == gdp::entities::EnemyArchetype::Guard
        ? 68.0F : 48.0F;
    plan.spawns.push_back({
        archetype,
        choosePosition(plan.seed, ordinal, zones),
        baseHealth * difficulty,
        wkc::core::deriveSeed(
            plan.seed, gdp::game::SeedDomain::EnemyVisual, 500U + ordinal),
        wave,
    });
}

void configureReward(
    RoomEventPlan& plan,
    const gdp::entities::ItemType type,
    const std::uint32_t quantity,
    const std::span<const SpawnZone> zones)
{
    plan.rewardType = type;
    plan.rewardQuantity = quantity;
    if (!zones.empty()) {
        plan.rewardPosition = choosePosition(plan.seed, 90U, zones);
    }
}

std::uint64_t addFloat(const std::uint64_t hash, const float value) noexcept
{
    return wkc::core::hashCombine(hash, std::bit_cast<std::uint32_t>(value));
}

} // namespace

bool EventValidationResult::isValid() const noexcept
{
    return errors.empty();
}

RoomEventPlan generateRoomEventPlan(
    const wkc::core::Seed roomSeed,
    const RoomRole role,
    const RoomTheme theme,
    const std::uint32_t depth,
    const std::span<const SpawnZone> eventZones,
    const float difficulty)
{
    using gdp::entities::EnemyArchetype;
    using gdp::entities::ItemType;
    RoomEventPlan plan;
    plan.seed = wkc::core::deriveSeed(roomSeed, gdp::game::SeedDomain::Events, 0);
    plan.role = role;
    plan.theme = theme;
    plan.depth = depth;
    const float safeDifficulty = std::clamp(difficulty, 0.5F, 2.0F);
    wkc::core::RandomStream selection{wkc::core::deriveSeed(
        plan.seed, gdp::game::SeedDomain::Events, 1)};

    switch (role) {
    case RoomRole::Start:
        plan.type = RoomEventType::TerminalAmbush;
        plan.trigger = RoomEventTrigger::TerminalUse;
        plan.locksRoom = true;
        plan.requiresRoomClear = true;
        plan.waveCount = 1U;
        addSpawn(plan, EnemyArchetype::Worker, 0U, eventZones, safeDifficulty);
        configureReward(plan, ItemType::Ammo, 10U, eventZones);
        break;
    case RoomRole::Normal:
        if (selection.chance(0.58F)) {
            plan.type = RoomEventType::Reinforcements;
            plan.waveCount = depth >= 3U && selection.chance(0.45F) ? 2U : 1U;
            plan.interWaveDelay = 0.75F;
            for (std::uint32_t wave = 0; wave < plan.waveCount; ++wave) {
                const bool guard = depth >= 3U && selection.chance(0.34F);
                addSpawn(plan,
                    guard ? EnemyArchetype::Guard : EnemyArchetype::Worker,
                    wave, eventZones, safeDifficulty);
            }
            configureReward(plan,
                selection.chance(0.35F) ? ItemType::Health : ItemType::Ammo,
                8U, eventZones);
        }
        break;
    case RoomRole::Hub:
    case RoomRole::Secret:
        plan.type = RoomEventType::SupplyDrop;
        configureReward(plan,
            role == RoomRole::Hub ? ItemType::Health : ItemType::Ammo,
            role == RoomRole::Hub ? 18U : 12U, eventZones);
        break;
    case RoomRole::Arena:
        plan.type = RoomEventType::Lockdown;
        plan.locksRoom = true;
        plan.requiresRoomClear = true;
        plan.waveCount = 1U;
        addSpawn(plan,
            depth >= 3U ? EnemyArchetype::Guard : EnemyArchetype::Worker,
            0U, eventZones, safeDifficulty);
        configureReward(plan, ItemType::Ammo, 14U, eventZones);
        break;
    case RoomRole::DeadEnd:
        if (selection.chance(0.55F)) {
            plan.type = RoomEventType::SupplyDrop;
            configureReward(plan, ItemType::Health, 15U, eventZones);
        } else {
            plan.type = RoomEventType::Reinforcements;
            plan.waveCount = 1U;
            addSpawn(plan, EnemyArchetype::Worker, 0U, eventZones, safeDifficulty);
            configureReward(plan, ItemType::Ammo, 10U, eventZones);
        }
        break;
    case RoomRole::Boss:
        plan.type = RoomEventType::Lockdown;
        plan.locksRoom = true;
        plan.requiresRoomClear = true;
        configureReward(plan, ItemType::Health, 30U, eventZones);
        break;
    case RoomRole::Count:
        break;
    }
    plan.planHash = roomEventPlanHash(plan);
    return plan;
}

EventValidationResult validateRoomEventPlan(
    const RoomEventPlan& plan,
    const std::span<const SpawnZone> eventZones)
{
    EventValidationResult result;
    if (plan.role >= RoomRole::Count) {
        result.errors.push_back(EventValidationError::InvalidRole);
    }
    if (plan.theme > RoomTheme::Cafeteria) {
        result.errors.push_back(EventValidationError::InvalidTheme);
    }
    if (plan.type >= RoomEventType::Count) {
        result.errors.push_back(EventValidationError::InvalidType);
    }
    const bool needsZone = plan.type != RoomEventType::None;
    if (needsZone && eventZones.empty()) {
        result.errors.push_back(EventValidationError::MissingZone);
    }
    if ((plan.type == RoomEventType::TerminalAmbush
            && plan.trigger != RoomEventTrigger::TerminalUse)
        || (plan.type != RoomEventType::TerminalAmbush
            && plan.trigger != RoomEventTrigger::RoomEntry)) {
        result.errors.push_back(EventValidationError::InvalidTrigger);
    }
    if ((plan.waveCount == 0U && !plan.spawns.empty())
        || (plan.waveCount > 0U && plan.spawns.empty())) {
        result.errors.push_back(EventValidationError::InvalidWave);
    }
    std::vector<std::uint32_t> spawnsPerWave(plan.waveCount, 0U);
    for (const EventEnemySpawn& spawn : plan.spawns) {
        if (spawn.wave >= plan.waveCount || spawn.health <= 0.0F
            || spawn.visualSeed == 0U
            || (spawn.archetype != gdp::entities::EnemyArchetype::Worker
                && spawn.archetype != gdp::entities::EnemyArchetype::Guard)) {
            result.errors.push_back(EventValidationError::InvalidSpawn);
        }
        if (spawn.wave < spawnsPerWave.size()) {
            ++spawnsPerWave[spawn.wave];
        }
        if (std::none_of(eventZones.begin(), eventZones.end(),
                [&spawn](const SpawnZone& zone) {
                    return containsPlanar(zone, spawn.position);
                })) {
            result.errors.push_back(EventValidationError::OutsideZone);
        }
    }
    if (std::any_of(spawnsPerWave.begin(), spawnsPerWave.end(),
            [](const std::uint32_t count) { return count == 0U; })) {
        result.errors.push_back(EventValidationError::InvalidWave);
    }
    const bool invalidTypeContract
        = (plan.type == RoomEventType::None
            && (plan.locksRoom || plan.requiresRoomClear || plan.waveCount != 0U))
        || (plan.type == RoomEventType::SupplyDrop
            && (plan.locksRoom || plan.requiresRoomClear || plan.waveCount != 0U))
        || (plan.type == RoomEventType::Reinforcements
            && (plan.locksRoom || plan.requiresRoomClear || plan.waveCount == 0U))
        || ((plan.type == RoomEventType::TerminalAmbush
                || plan.type == RoomEventType::Lockdown)
            && (!plan.locksRoom || !plan.requiresRoomClear));
    if (invalidTypeContract || plan.interWaveDelay < 0.0F) {
        result.errors.push_back(EventValidationError::InvalidWave);
    }
    if ((plan.type == RoomEventType::None && plan.rewardQuantity != 0U)
        || (plan.type != RoomEventType::None && plan.rewardQuantity == 0U)
        || plan.rewardType > gdp::entities::ItemType::Key) {
        result.errors.push_back(EventValidationError::InvalidReward);
    }
    if (plan.type != RoomEventType::None
        && std::none_of(eventZones.begin(), eventZones.end(),
            [&plan](const SpawnZone& zone) {
                return containsPlanar(zone, plan.rewardPosition);
            })) {
        result.errors.push_back(EventValidationError::OutsideZone);
    }
    if (plan.planHash == 0U || plan.planHash != roomEventPlanHash(plan)) {
        result.errors.push_back(EventValidationError::InvalidPlanHash);
    }
    return result;
}

std::uint64_t roomEventPlanHash(const RoomEventPlan& plan) noexcept
{
    std::uint64_t hash = wkc::core::hashCombine(0x4556454E545F504CULL, plan.seed);
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(plan.role));
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(plan.theme));
    hash = wkc::core::hashCombine(hash, plan.depth);
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(plan.type));
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(plan.trigger));
    hash = wkc::core::hashCombine(hash, plan.locksRoom ? 1U : 0U);
    hash = wkc::core::hashCombine(hash, plan.requiresRoomClear ? 1U : 0U);
    hash = wkc::core::hashCombine(hash, plan.waveCount);
    hash = addFloat(hash, plan.interWaveDelay);
    hash = wkc::core::hashCombine(hash, plan.spawns.size());
    for (const EventEnemySpawn& spawn : plan.spawns) {
        hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(spawn.archetype));
        hash = addFloat(hash, spawn.position.x);
        hash = addFloat(hash, spawn.position.y);
        hash = addFloat(hash, spawn.position.z);
        hash = addFloat(hash, spawn.health);
        hash = wkc::core::hashCombine(hash, spawn.visualSeed);
        hash = wkc::core::hashCombine(hash, spawn.wave);
    }
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(plan.rewardType));
    hash = wkc::core::hashCombine(hash, plan.rewardQuantity);
    hash = addFloat(hash, plan.rewardPosition.x);
    hash = addFloat(hash, plan.rewardPosition.y);
    hash = addFloat(hash, plan.rewardPosition.z);
    return hash;
}

} // namespace gdp::world
