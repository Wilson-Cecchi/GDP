#pragma once

#include "core/Seed.hpp"
#include "entities/EntityManager.hpp"
#include "world/RoomModule.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace gdp::world {

struct LootDrop {
    gdp::entities::ItemType type{gdp::entities::ItemType::Health};
    std::uint32_t quantity{1};
    gdp::core::Vec3 position{};

    constexpr bool operator==(const LootDrop&) const noexcept = default;
};

struct LootLayout {
    gdp::core::Seed seed{0};
    RoomRole role{RoomRole::Normal};
    RoomTheme theme{RoomTheme::Industrial};
    std::uint32_t depth{0};
    std::uint32_t budget{0};
    std::uint32_t spent{0};
    std::vector<LootDrop> drops;
    std::uint64_t layoutHash{0};
};

enum class LootValidationError : std::uint8_t {
    InvalidRole,
    InvalidTheme,
    MissingZone,
    InvalidDrop,
    OutsideZone,
    OverlappingDrops,
    BudgetExceeded,
    InvalidKeyDistribution,
    InvalidLayoutHash,
};

struct LootValidationResult {
    std::vector<LootValidationError> errors;

    [[nodiscard]] bool isValid() const noexcept;
};

[[nodiscard]] LootLayout generateLootLayout(
    gdp::core::Seed roomSeed,
    RoomRole role,
    RoomTheme theme,
    std::uint32_t depth,
    std::span<const SpawnZone> lootZones,
    float difficulty = 1.0F);

[[nodiscard]] LootValidationResult validateLootLayout(
    const LootLayout& layout,
    std::span<const SpawnZone> lootZones);

[[nodiscard]] std::uint64_t lootLayoutHash(const LootLayout& layout) noexcept;

} // namespace gdp::world
