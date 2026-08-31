#pragma once

#include "core/Seed.hpp"
#include "entities/EntityManager.hpp"
#include "world/RoomModule.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace gdp::world {

struct PropSpawn {
    gdp::entities::PropType type{gdp::entities::PropType::Crate};
    gdp::entities::PropState state{gdp::entities::PropState::Intact};
    wkc::core::Vec3 position{};
    wkc::core::Vec3 scale{1.0F, 1.0F, 1.0F};
    bool solid{true};
    float health{1.0F};
    float maximumHealth{1.0F};
    gdp::entities::ItemType dropType{gdp::entities::ItemType::Ammo};
    std::uint32_t dropQuantity{0};
    wkc::core::Seed visualSeed{0};

    constexpr bool operator==(const PropSpawn&) const noexcept = default;
};

struct PropLayout {
    wkc::core::Seed seed{0};
    RoomRole role{RoomRole::Normal};
    RoomTheme theme{RoomTheme::Industrial};
    std::uint32_t depth{0};
    std::uint32_t budget{0};
    std::uint32_t spent{0};
    std::vector<PropSpawn> props;
    std::uint64_t layoutHash{0};
};

enum class PropValidationError : std::uint8_t {
    InvalidRole,
    InvalidTheme,
    MissingZone,
    InvalidProp,
    OutsideZone,
    OverlappingProps,
    BudgetMismatch,
    InvalidLayoutHash,
};

struct PropValidationResult {
    std::vector<PropValidationError> errors;

    [[nodiscard]] bool isValid() const noexcept;
};

[[nodiscard]] PropLayout generatePropLayout(
    wkc::core::Seed roomSeed,
    RoomRole role,
    RoomTheme theme,
    std::uint32_t depth,
    std::span<const SpawnZone> propZones,
    float difficulty = 1.0F);

[[nodiscard]] PropValidationResult validatePropLayout(
    const PropLayout& layout,
    std::span<const SpawnZone> propZones);

[[nodiscard]] std::uint64_t propLayoutHash(const PropLayout& layout) noexcept;

} // namespace gdp::world
