#include "world/LootGenerator.hpp"

#include "core/Hash.hpp"
#include "core/Random.hpp"
#include "game/SeedDomains.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>

namespace gdp::world {
namespace {

constexpr float minimumDropSeparation = 0.42F;

std::uint32_t baseBudget(const RoomRole role, const std::uint32_t depth) noexcept
{
    switch (role) {
    case RoomRole::Start:
        return 1U;
    case RoomRole::Normal:
        return 1U + std::min(depth / 2U, 2U);
    case RoomRole::Hub:
        return 2U + std::min(depth / 3U, 1U);
    case RoomRole::Arena:
        return 2U + std::min(depth / 3U, 1U);
    case RoomRole::DeadEnd:
        return 3U;
    case RoomRole::Secret:
        return 3U;
    case RoomRole::Boss:
    case RoomRole::Count:
        return 0U;
    }
    return 0U;
}

std::uint32_t itemCost(const gdp::entities::ItemType type) noexcept
{
    switch (type) {
    case gdp::entities::ItemType::Health:
        return 2U;
    case gdp::entities::ItemType::Ammo:
        return 1U;
    case gdp::entities::ItemType::Key:
        return 0U;
    }
    return 0U;
}

float healthProbability(const RoomTheme theme) noexcept
{
    switch (theme) {
    case RoomTheme::Laboratory:
    case RoomTheme::Cafeteria:
        return 0.68F;
    case RoomTheme::Office:
        return 0.56F;
    case RoomTheme::Industrial:
        return 0.46F;
    case RoomTheme::Storage:
        return 0.28F;
    case RoomTheme::Maintenance:
    case RoomTheme::Security:
        return 0.32F;
    }
    return 0.46F;
}

std::uint32_t quantityFor(
    const gdp::entities::ItemType type,
    const gdp::core::Seed seed,
    const std::uint32_t depth,
    const std::uint64_t ordinal) noexcept
{
    gdp::core::RandomStream random{gdp::core::deriveSeed(
        seed, gdp::game::SeedDomain::Loot, 100U + ordinal)};
    switch (type) {
    case gdp::entities::ItemType::Health:
        return 15U + (5U * static_cast<std::uint32_t>(random.rangeInt(
            0, static_cast<int>(std::min(depth + 1U, 5U)))));
    case gdp::entities::ItemType::Ammo:
        return 6U + (2U * std::min(depth, 4U))
            + (2U * static_cast<std::uint32_t>(random.rangeInt(0, 2)));
    case gdp::entities::ItemType::Key:
        return 1U;
    }
    return 1U;
}

bool containsPlanar(const SpawnZone& zone, const gdp::core::Vec3 position) noexcept
{
    return position.x >= zone.bounds.minimum.x && position.x <= zone.bounds.maximum.x
        && position.z >= zone.bounds.minimum.z && position.z <= zone.bounds.maximum.z
        && gdp::core::nearlyEqual(position.y, zone.bounds.minimum.y, 1.0e-4F);
}

float planarDistance(
    const gdp::core::Vec3 first,
    const gdp::core::Vec3 second) noexcept
{
    const float x = first.x - second.x;
    const float z = first.z - second.z;
    return std::sqrt((x * x) + (z * z));
}

gdp::core::Vec3 choosePosition(
    const gdp::core::Seed seed,
    const std::uint64_t ordinal,
    const std::span<const SpawnZone> zones)
{
    gdp::core::RandomStream random{gdp::core::deriveSeed(
        seed, gdp::game::SeedDomain::Loot, 200U + ordinal)};
    const std::size_t zoneIndex = static_cast<std::size_t>(random.rangeInt(
        0, static_cast<int>(zones.size() - 1U)));
    const SpawnZone& zone = zones[zoneIndex];
    constexpr std::array anchors{
        gdp::core::Vec3{0.18F, 0.0F, 0.18F},
        gdp::core::Vec3{0.82F, 0.0F, 0.82F},
        gdp::core::Vec3{0.18F, 0.0F, 0.82F},
    };
    const gdp::core::Vec3 anchor
        = anchors[static_cast<std::size_t>(ordinal % anchors.size())];
    const float normalizedX = std::clamp(
        anchor.x + random.rangeFloat(-0.035F, 0.035F), 0.10F, 0.90F);
    const float normalizedZ = std::clamp(
        anchor.z + random.rangeFloat(-0.035F, 0.035F), 0.10F, 0.90F);
    return {
        zone.bounds.minimum.x
            + ((zone.bounds.maximum.x - zone.bounds.minimum.x) * normalizedX),
        zone.bounds.minimum.y,
        zone.bounds.minimum.z
            + ((zone.bounds.maximum.z - zone.bounds.minimum.z) * normalizedZ),
    };
}

void addDrop(
    LootLayout& layout,
    const gdp::entities::ItemType type,
    const std::span<const SpawnZone> zones)
{
    if (zones.empty() || layout.drops.size() >= 3U) {
        return;
    }
    const std::uint64_t ordinal = layout.drops.size();
    layout.drops.push_back({
        type,
        quantityFor(type, layout.seed, layout.depth, ordinal),
        choosePosition(layout.seed, ordinal, zones),
    });
    layout.spent += itemCost(type);
}

std::uint64_t addFloat(const std::uint64_t hash, const float value) noexcept
{
    return wkc::core::hashCombine(hash, std::bit_cast<std::uint32_t>(value));
}

} // namespace

bool LootValidationResult::isValid() const noexcept
{
    return errors.empty();
}

LootLayout generateLootLayout(
    const gdp::core::Seed roomSeed,
    const RoomRole role,
    const RoomTheme theme,
    const std::uint32_t depth,
    const std::span<const SpawnZone> lootZones,
    const float difficulty)
{
    LootLayout layout;
    layout.seed = gdp::core::deriveSeed(roomSeed, gdp::game::SeedDomain::Loot, 0);
    layout.role = role;
    layout.theme = theme;
    layout.depth = depth;
    const float safeDifficulty = std::clamp(difficulty, 0.5F, 2.0F);
    const std::uint32_t rawBudget = baseBudget(role, depth);
    layout.budget = rawBudget == 0U ? 0U : std::max(
        1U, static_cast<std::uint32_t>(std::lround(
            static_cast<float>(rawBudget) / safeDifficulty)));
    if (role == RoomRole::Hub || role == RoomRole::Secret) {
        layout.budget = std::max(layout.budget, 2U);
    }

    if (layout.budget == 0U || lootZones.empty()) {
        layout.layoutHash = lootLayoutHash(layout);
        return layout;
    }

    switch (role) {
    case RoomRole::Start:
    case RoomRole::Arena:
    case RoomRole::DeadEnd:
        addDrop(layout, gdp::entities::ItemType::Ammo, lootZones);
        break;
    case RoomRole::Hub:
        addDrop(layout, gdp::entities::ItemType::Health, lootZones);
        break;
    case RoomRole::Secret:
        addDrop(layout, gdp::entities::ItemType::Key, lootZones);
        addDrop(layout, gdp::entities::ItemType::Health, lootZones);
        break;
    case RoomRole::Normal:
    case RoomRole::Boss:
    case RoomRole::Count:
        break;
    }

    gdp::core::RandomStream selection{gdp::core::deriveSeed(
        layout.seed, gdp::game::SeedDomain::Loot, 1)};
    while (layout.drops.size() < 3U && layout.spent < layout.budget) {
        const std::uint32_t remaining = layout.budget - layout.spent;
        const bool chooseHealth = remaining >= itemCost(gdp::entities::ItemType::Health)
            && selection.chance(healthProbability(theme));
        addDrop(layout,
            chooseHealth ? gdp::entities::ItemType::Health
                         : gdp::entities::ItemType::Ammo,
            lootZones);
    }
    layout.layoutHash = lootLayoutHash(layout);
    return layout;
}

LootValidationResult validateLootLayout(
    const LootLayout& layout,
    const std::span<const SpawnZone> lootZones)
{
    LootValidationResult result;
    if (layout.role >= RoomRole::Count) {
        result.errors.push_back(LootValidationError::InvalidRole);
    }
    if (layout.theme > RoomTheme::Cafeteria) {
        result.errors.push_back(LootValidationError::InvalidTheme);
    }
    if (layout.budget > 0U && lootZones.empty()) {
        result.errors.push_back(LootValidationError::MissingZone);
    }
    if (layout.spent > layout.budget) {
        result.errors.push_back(LootValidationError::BudgetExceeded);
    }
    std::size_t keyCount = 0;
    for (std::size_t index = 0; index < layout.drops.size(); ++index) {
        const LootDrop& drop = layout.drops[index];
        if (drop.quantity == 0U || drop.type > gdp::entities::ItemType::Key) {
            result.errors.push_back(LootValidationError::InvalidDrop);
        }
        keyCount += drop.type == gdp::entities::ItemType::Key ? 1U : 0U;
        if (std::none_of(
                lootZones.begin(), lootZones.end(),
                [&drop](const SpawnZone& zone) { return containsPlanar(zone, drop.position); })) {
            result.errors.push_back(LootValidationError::OutsideZone);
        }
        for (std::size_t other = index + 1U; other < layout.drops.size(); ++other) {
            if (planarDistance(drop.position, layout.drops[other].position)
                < minimumDropSeparation - 1.0e-4F) {
                result.errors.push_back(LootValidationError::OverlappingDrops);
            }
        }
    }
    if ((layout.role == RoomRole::Secret && keyCount != 1U)
        || (layout.role != RoomRole::Secret && keyCount != 0U)) {
        result.errors.push_back(LootValidationError::InvalidKeyDistribution);
    }
    if (layout.layoutHash == 0U || layout.layoutHash != lootLayoutHash(layout)) {
        result.errors.push_back(LootValidationError::InvalidLayoutHash);
    }
    return result;
}

std::uint64_t lootLayoutHash(const LootLayout& layout) noexcept
{
    std::uint64_t hash = wkc::core::hashCombine(0x4C4F4F545F4C4159ULL, layout.seed);
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(layout.role));
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(layout.theme));
    hash = wkc::core::hashCombine(hash, layout.depth);
    hash = wkc::core::hashCombine(hash, layout.budget);
    hash = wkc::core::hashCombine(hash, layout.spent);
    hash = wkc::core::hashCombine(hash, layout.drops.size());
    for (const LootDrop& drop : layout.drops) {
        hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(drop.type));
        hash = wkc::core::hashCombine(hash, drop.quantity);
        hash = addFloat(hash, drop.position.x);
        hash = addFloat(hash, drop.position.y);
        hash = addFloat(hash, drop.position.z);
    }
    return hash;
}

} // namespace gdp::world
