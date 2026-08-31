#include "world/PropGenerator.hpp"

#include "core/Hash.hpp"
#include "core/Random.hpp"
#include "game/SeedDomains.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>

namespace gdp::world {
namespace {

std::uint32_t baseBudget(const RoomRole role, const std::uint32_t depth) noexcept
{
    switch (role) {
    case RoomRole::Start:
    case RoomRole::Hub:
    case RoomRole::Secret:
        return 1U;
    case RoomRole::Normal:
        return 2U + std::min(depth / 3U, 1U);
    case RoomRole::Arena:
    case RoomRole::Boss:
        return 2U;
    case RoomRole::DeadEnd:
        return 3U;
    case RoomRole::Count:
        return 0U;
    }
    return 0U;
}

gdp::entities::PropType selectType(
    const RoomTheme theme,
    wkc::core::RandomStream& random) noexcept
{
    using gdp::entities::PropType;
    const float roll = random.uniform01();
    switch (theme) {
    case RoomTheme::Storage:
    case RoomTheme::Cafeteria:
        return roll < 0.68F ? PropType::Crate
            : roll < 0.88F ? PropType::Barricade : PropType::Canister;
    case RoomTheme::Security:
        return roll < 0.64F ? PropType::Barricade
            : roll < 0.82F ? PropType::Crate : PropType::Canister;
    case RoomTheme::Laboratory:
    case RoomTheme::Maintenance:
        return roll < 0.58F ? PropType::Canister
            : roll < 0.82F ? PropType::Barricade : PropType::Crate;
    case RoomTheme::Industrial:
        return roll < 0.46F ? PropType::Canister
            : roll < 0.76F ? PropType::Crate : PropType::Barricade;
    case RoomTheme::Office:
        return roll < 0.52F ? PropType::Barricade
            : roll < 0.86F ? PropType::Crate : PropType::Canister;
    }
    return PropType::Crate;
}

wkc::core::Vec3 scaleFor(const gdp::entities::PropType type) noexcept
{
    using gdp::entities::PropType;
    switch (type) {
    case PropType::Crate:
        return {0.78F, 0.72F, 0.78F};
    case PropType::Canister:
        return {0.46F, 1.02F, 0.46F};
    case PropType::Barricade:
        return {1.16F, 0.92F, 0.34F};
    case PropType::Terminal:
    case PropType::Door:
    case PropType::Decoration:
        return {0.8F, 1.0F, 0.8F};
    }
    return {0.8F, 0.8F, 0.8F};
}

float healthFor(const gdp::entities::PropType type) noexcept
{
    using gdp::entities::PropType;
    switch (type) {
    case PropType::Crate:
        return 60.0F;
    case PropType::Canister:
        return 35.0F;
    case PropType::Barricade:
        return 90.0F;
    case PropType::Terminal:
    case PropType::Door:
    case PropType::Decoration:
        return 0.0F;
    }
    return 0.0F;
}

wkc::core::Vec3 choosePosition(
    const wkc::core::Seed seed,
    const std::uint64_t ordinal,
    const wkc::core::Vec3 scale,
    const std::span<const SpawnZone> zones)
{
    wkc::core::RandomStream random{wkc::core::deriveSeed(
        seed, gdp::game::SeedDomain::Props, 200U + ordinal)};
    const SpawnZone& zone = zones[static_cast<std::size_t>(random.rangeInt(
        0, static_cast<int>(zones.size() - 1U)))];
    constexpr std::array<float, 3> lanes{0.06F, 0.50F, 0.94F};
    const float lane = lanes[ordinal % lanes.size()];
    const bool placeAlongX
        = (zone.bounds.maximum.x - zone.bounds.minimum.x)
        >= (zone.bounds.maximum.z - zone.bounds.minimum.z);
    const wkc::core::Vec2 anchor{
        placeAlongX ? lane : 0.50F,
        placeAlongX ? 0.50F : lane,
    };
    const float halfX = scale.x * 0.5F;
    const float halfZ = scale.z * 0.5F;
    const float minimumX = zone.bounds.minimum.x + halfX;
    const float maximumX = zone.bounds.maximum.x - halfX;
    const float minimumZ = zone.bounds.minimum.z + halfZ;
    const float maximumZ = zone.bounds.maximum.z - halfZ;
    const float xJitter = placeAlongX ? 0.0F : random.rangeFloat(-0.025F, 0.025F);
    const float zJitter = placeAlongX ? random.rangeFloat(-0.025F, 0.025F) : 0.0F;
    const float x = minimumX + ((std::max(maximumX, minimumX) - minimumX)
        * std::clamp(anchor.x + xJitter, 0.0F, 1.0F));
    const float z = minimumZ + ((std::max(maximumZ, minimumZ) - minimumZ)
        * std::clamp(anchor.y + zJitter, 0.0F, 1.0F));
    return {x, zone.bounds.minimum.y, z};
}

bool footprintInside(const PropSpawn& prop, const SpawnZone& zone) noexcept
{
    const float halfX = prop.scale.x * 0.5F;
    const float halfZ = prop.scale.z * 0.5F;
    return prop.position.x - halfX >= zone.bounds.minimum.x
        && prop.position.x + halfX <= zone.bounds.maximum.x
        && prop.position.z - halfZ >= zone.bounds.minimum.z
        && prop.position.z + halfZ <= zone.bounds.maximum.z
        && wkc::core::nearlyEqual(prop.position.y, zone.bounds.minimum.y, 1.0e-4F);
}

bool footprintsOverlap(const PropSpawn& first, const PropSpawn& second) noexcept
{
    constexpr float clearance = 0.10F;
    return std::abs(first.position.x - second.position.x)
            < ((first.scale.x + second.scale.x) * 0.5F) + clearance
        && std::abs(first.position.z - second.position.z)
            < ((first.scale.z + second.scale.z) * 0.5F) + clearance;
}

void addProp(
    PropLayout& layout,
    const gdp::entities::PropType type,
    const std::span<const SpawnZone> zones)
{
    using gdp::entities::ItemType;
    using gdp::entities::PropState;
    using gdp::entities::PropType;
    if (zones.empty() || layout.props.size() >= 3U || layout.spent >= layout.budget) {
        return;
    }
    const std::uint64_t ordinal = layout.props.size();
    const wkc::core::Vec3 scale = scaleFor(type);
    wkc::core::RandomStream stateRandom{wkc::core::deriveSeed(
        layout.seed, gdp::game::SeedDomain::Props, 100U + ordinal)};
    const float maximumHealth = healthFor(type);
    const bool initiallyDamaged = layout.depth >= 2U && stateRandom.chance(0.18F);
    const PropState state = initiallyDamaged ? PropState::Damaged : PropState::Intact;
    ItemType dropType = ItemType::Ammo;
    std::uint32_t dropQuantity = 0U;
    if (type == PropType::Crate) {
        dropType = stateRandom.chance(0.38F) ? ItemType::Health : ItemType::Ammo;
        dropQuantity = dropType == ItemType::Health ? 20U : 8U;
    }
    layout.props.push_back({
        type,
        state,
        choosePosition(layout.seed, ordinal, scale, zones),
        scale,
        true,
        initiallyDamaged ? maximumHealth * 0.5F : maximumHealth,
        maximumHealth,
        dropType,
        dropQuantity,
        wkc::core::deriveSeed(
            layout.seed, gdp::game::SeedDomain::Props, 300U + ordinal),
    });
    ++layout.spent;
}

std::uint64_t addFloat(const std::uint64_t hash, const float value) noexcept
{
    return wkc::core::hashCombine(hash, std::bit_cast<std::uint32_t>(value));
}

} // namespace

bool PropValidationResult::isValid() const noexcept
{
    return errors.empty();
}

PropLayout generatePropLayout(
    const wkc::core::Seed roomSeed,
    const RoomRole role,
    const RoomTheme theme,
    const std::uint32_t depth,
    const std::span<const SpawnZone> propZones,
    const float difficulty)
{
    using gdp::entities::PropType;
    PropLayout layout;
    layout.seed = wkc::core::deriveSeed(roomSeed, gdp::game::SeedDomain::Props, 0);
    layout.role = role;
    layout.theme = theme;
    layout.depth = depth;
    const float safeDifficulty = std::clamp(difficulty, 0.5F, 2.0F);
    const std::uint32_t rawBudget = baseBudget(role, depth);
    layout.budget = rawBudget == 0U ? 0U : std::clamp(
        static_cast<std::uint32_t>(std::lround(
            static_cast<float>(rawBudget) / safeDifficulty)), 1U, 3U);
    if (layout.budget == 0U || propZones.empty()) {
        layout.layoutHash = propLayoutHash(layout);
        return layout;
    }

    if (role == RoomRole::Start || role == RoomRole::DeadEnd
        || role == RoomRole::Secret) {
        addProp(layout, PropType::Crate, propZones);
    } else if (role == RoomRole::Boss) {
        addProp(layout, PropType::Barricade, propZones);
    }

    wkc::core::RandomStream selection{wkc::core::deriveSeed(
        layout.seed, gdp::game::SeedDomain::Props, 1)};
    while (layout.spent < layout.budget && layout.props.size() < 3U) {
        addProp(layout, selectType(theme, selection), propZones);
    }
    layout.layoutHash = propLayoutHash(layout);
    return layout;
}

PropValidationResult validatePropLayout(
    const PropLayout& layout,
    const std::span<const SpawnZone> propZones)
{
    using gdp::entities::PropState;
    using gdp::entities::PropType;
    PropValidationResult result;
    if (layout.role >= RoomRole::Count) {
        result.errors.push_back(PropValidationError::InvalidRole);
    }
    if (layout.theme > RoomTheme::Cafeteria) {
        result.errors.push_back(PropValidationError::InvalidTheme);
    }
    if (layout.budget > 0U && propZones.empty()) {
        result.errors.push_back(PropValidationError::MissingZone);
    }
    if (layout.spent != layout.props.size() || layout.spent > layout.budget) {
        result.errors.push_back(PropValidationError::BudgetMismatch);
    }
    for (std::size_t index = 0; index < layout.props.size(); ++index) {
        const PropSpawn& prop = layout.props[index];
        const bool generatedType = prop.type == PropType::Crate
            || prop.type == PropType::Canister || prop.type == PropType::Barricade;
        if (!generatedType
            || prop.state == PropState::Destroyed || !prop.solid
            || prop.health <= 0.0F || prop.maximumHealth <= 0.0F
            || prop.health > prop.maximumHealth || prop.scale.x <= 0.0F
            || prop.scale.y <= 0.0F || prop.scale.z <= 0.0F
            || prop.visualSeed == 0U
            || (prop.type != PropType::Crate && prop.dropQuantity != 0U)) {
            result.errors.push_back(PropValidationError::InvalidProp);
        }
        if (std::none_of(propZones.begin(), propZones.end(),
                [&prop](const SpawnZone& zone) { return footprintInside(prop, zone); })) {
            result.errors.push_back(PropValidationError::OutsideZone);
        }
        for (std::size_t other = index + 1U; other < layout.props.size(); ++other) {
            if (footprintsOverlap(prop, layout.props[other])) {
                result.errors.push_back(PropValidationError::OverlappingProps);
            }
        }
    }
    if (layout.layoutHash == 0U || layout.layoutHash != propLayoutHash(layout)) {
        result.errors.push_back(PropValidationError::InvalidLayoutHash);
    }
    return result;
}

std::uint64_t propLayoutHash(const PropLayout& layout) noexcept
{
    std::uint64_t hash = wkc::core::hashCombine(0x50524F505F4C4159ULL, layout.seed);
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(layout.role));
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(layout.theme));
    hash = wkc::core::hashCombine(hash, layout.depth);
    hash = wkc::core::hashCombine(hash, layout.budget);
    hash = wkc::core::hashCombine(hash, layout.spent);
    hash = wkc::core::hashCombine(hash, layout.props.size());
    for (const PropSpawn& prop : layout.props) {
        hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(prop.type));
        hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(prop.state));
        hash = addFloat(hash, prop.position.x);
        hash = addFloat(hash, prop.position.y);
        hash = addFloat(hash, prop.position.z);
        hash = addFloat(hash, prop.scale.x);
        hash = addFloat(hash, prop.scale.y);
        hash = addFloat(hash, prop.scale.z);
        hash = addFloat(hash, prop.health);
        hash = addFloat(hash, prop.maximumHealth);
        hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(prop.dropType));
        hash = wkc::core::hashCombine(hash, prop.dropQuantity);
        hash = wkc::core::hashCombine(hash, prop.visualSeed);
    }
    return hash;
}

} // namespace gdp::world
