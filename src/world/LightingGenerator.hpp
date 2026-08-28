#pragma once

#include "core/Math.hpp"
#include "core/Seed.hpp"
#include "world/RoomModule.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace gdp::world {

inline constexpr std::size_t maxRoomLights = 4U;

enum class RoomLightingMood : std::uint8_t {
    Neutral,
    Warm,
    Cool,
    Clinical,
    Dim,
    Ominous,
    Count,
};

struct RoomPointLight {
    gdp::core::Vec3 position{};
    gdp::core::Vec3 color{1.0F, 1.0F, 1.0F};
    float intensity{1.0F};
    float range{6.0F};

    constexpr bool operator==(const RoomPointLight&) const noexcept = default;
};

struct RoomLightingPlan {
    gdp::core::Seed seed{0};
    RoomRole role{RoomRole::Normal};
    RoomTheme theme{RoomTheme::Industrial};
    RoomLightingMood mood{RoomLightingMood::Neutral};
    gdp::core::Vec3 ambientColor{0.70F, 0.78F, 0.88F};
    float ambientIntensity{0.18F};
    float fixtureEmissive{0.75F};
    bool supportsAlert{false};
    std::vector<RoomPointLight> lights;
    std::uint64_t planHash{0};
};

enum class LightingValidationError : std::uint8_t {
    InvalidRole,
    InvalidTheme,
    InvalidMood,
    InvalidBounds,
    InvalidAmbient,
    InvalidFixture,
    InvalidLightCount,
    InvalidLight,
    LightOutsideRoom,
    InvalidPlanHash,
};

struct LightingValidationResult {
    std::vector<LightingValidationError> errors;

    [[nodiscard]] bool isValid() const noexcept;
};

[[nodiscard]] RoomLightingPlan generateRoomLightingPlan(
    gdp::core::Seed roomSeed,
    RoomRole role,
    RoomTheme theme,
    gdp::core::Aabb roomBounds);

[[nodiscard]] LightingValidationResult validateRoomLightingPlan(
    const RoomLightingPlan& plan,
    gdp::core::Aabb roomBounds);

[[nodiscard]] std::uint64_t roomLightingPlanHash(
    const RoomLightingPlan& plan) noexcept;

} // namespace gdp::world
