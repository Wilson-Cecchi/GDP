#include "world/LightingGenerator.hpp"

#include "core/Hash.hpp"
#include "core/Random.hpp"
#include "game/SeedDomains.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>

namespace gdp::world {
namespace {

struct LightingPalette {
    RoomLightingMood mood;
    wkc::core::Vec3 ambient;
    wkc::core::Vec3 light;
};

LightingPalette paletteFor(const RoomRole role, const RoomTheme theme) noexcept
{
    if (role == RoomRole::Secret) {
        return {RoomLightingMood::Dim, {0.42F, 0.50F, 0.58F}, {0.56F, 0.72F, 0.86F}};
    }
    if (role == RoomRole::Boss) {
        return {RoomLightingMood::Ominous, {0.54F, 0.32F, 0.28F}, {0.94F, 0.44F, 0.30F}};
    }
    switch (theme) {
    case RoomTheme::Storage:
    case RoomTheme::Cafeteria:
        return {RoomLightingMood::Warm, {0.82F, 0.70F, 0.52F}, {1.0F, 0.78F, 0.48F}};
    case RoomTheme::Laboratory:
        return {RoomLightingMood::Clinical, {0.68F, 0.84F, 0.88F}, {0.72F, 0.96F, 1.0F}};
    case RoomTheme::Security:
        return {RoomLightingMood::Cool, {0.50F, 0.66F, 0.82F}, {0.52F, 0.78F, 1.0F}};
    case RoomTheme::Office:
        return {RoomLightingMood::Neutral, {0.74F, 0.76F, 0.68F}, {0.90F, 0.88F, 0.70F}};
    case RoomTheme::Industrial:
    case RoomTheme::Maintenance:
        return {RoomLightingMood::Cool, {0.58F, 0.68F, 0.78F}, {0.70F, 0.84F, 1.0F}};
    }
    return {RoomLightingMood::Neutral, {0.70F, 0.78F, 0.88F}, {0.82F, 0.90F, 1.0F}};
}

std::size_t lightCountFor(const RoomRole role, const wkc::core::Aabb bounds) noexcept
{
    if (role == RoomRole::Secret) {
        return 1U;
    }
    if (role == RoomRole::Hub || role == RoomRole::Arena || role == RoomRole::Boss) {
        return maxRoomLights;
    }
    const float width = bounds.maximum.x - bounds.minimum.x;
    const float depth = bounds.maximum.z - bounds.minimum.z;
    return width * depth >= 72.0F ? 3U : 2U;
}

bool finiteVec3(const wkc::core::Vec3 value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool validColor(const wkc::core::Vec3 value) noexcept
{
    return finiteVec3(value)
        && value.x >= 0.0F && value.x <= 1.0F
        && value.y >= 0.0F && value.y <= 1.0F
        && value.z >= 0.0F && value.z <= 1.0F;
}

std::uint64_t addFloat(const std::uint64_t hash, const float value) noexcept
{
    return wkc::core::hashCombine(hash, std::bit_cast<std::uint32_t>(value));
}

std::uint64_t addVec3(std::uint64_t hash, const wkc::core::Vec3 value) noexcept
{
    hash = addFloat(hash, value.x);
    hash = addFloat(hash, value.y);
    return addFloat(hash, value.z);
}

} // namespace

bool LightingValidationResult::isValid() const noexcept
{
    return errors.empty();
}

RoomLightingPlan generateRoomLightingPlan(
    const wkc::core::Seed roomSeed,
    const RoomRole role,
    const RoomTheme theme,
    const wkc::core::Aabb roomBounds)
{
    RoomLightingPlan plan;
    plan.seed = wkc::core::deriveSeed(roomSeed, gdp::game::SeedDomain::Lighting, 0U);
    plan.role = role;
    plan.theme = theme;
    const LightingPalette palette = paletteFor(role, theme);
    plan.mood = palette.mood;
    plan.ambientColor = palette.ambient;
    plan.supportsAlert = role == RoomRole::Start || role == RoomRole::Arena
        || role == RoomRole::Boss || theme == RoomTheme::Security;

    wkc::core::RandomStream random{wkc::core::deriveSeed(
        plan.seed, gdp::game::SeedDomain::Lighting, 1U)};
    const float roleAmbient = role == RoomRole::Secret ? 0.115F
        : role == RoomRole::Boss ? 0.14F
        : role == RoomRole::Arena ? 0.17F : 0.19F;
    plan.ambientIntensity = roleAmbient + random.rangeFloat(-0.012F, 0.012F);
    plan.fixtureEmissive = random.rangeFloat(0.68F, 0.92F);

    if (roomBounds.isValid()) {
        constexpr std::array anchors{
            wkc::core::Vec2{0.28F, 0.30F},
            wkc::core::Vec2{0.72F, 0.70F},
            wkc::core::Vec2{0.72F, 0.30F},
            wkc::core::Vec2{0.28F, 0.70F},
        };
        const std::size_t count = lightCountFor(role, roomBounds);
        const float ceiling = roomBounds.maximum.y - 0.16F;
        for (std::size_t index = 0; index < count; ++index) {
            const float jitterX = random.rangeFloat(-0.025F, 0.025F);
            const float jitterZ = random.rangeFloat(-0.025F, 0.025F);
            const float normalizedX = std::clamp(anchors[index].x + jitterX, 0.15F, 0.85F);
            const float normalizedZ = std::clamp(anchors[index].y + jitterZ, 0.15F, 0.85F);
            wkc::core::Vec3 lightColor = palette.light;
            const float colorVariation = random.rangeFloat(-0.035F, 0.035F);
            lightColor.x = std::clamp(lightColor.x + colorVariation, 0.0F, 1.0F);
            lightColor.y = std::clamp(lightColor.y + colorVariation, 0.0F, 1.0F);
            lightColor.z = std::clamp(lightColor.z + colorVariation, 0.0F, 1.0F);
            plan.lights.push_back({
                {
                    roomBounds.minimum.x
                        + ((roomBounds.maximum.x - roomBounds.minimum.x) * normalizedX),
                    ceiling,
                    roomBounds.minimum.z
                        + ((roomBounds.maximum.z - roomBounds.minimum.z) * normalizedZ),
                },
                lightColor,
                random.rangeFloat(0.86F, 1.16F),
                random.rangeFloat(4.8F, 6.8F),
            });
        }
    }
    plan.planHash = roomLightingPlanHash(plan);
    return plan;
}

LightingValidationResult validateRoomLightingPlan(
    const RoomLightingPlan& plan,
    const wkc::core::Aabb roomBounds)
{
    LightingValidationResult result;
    if (plan.role >= RoomRole::Count) {
        result.errors.push_back(LightingValidationError::InvalidRole);
    }
    if (plan.theme > RoomTheme::Cafeteria) {
        result.errors.push_back(LightingValidationError::InvalidTheme);
    }
    if (plan.mood >= RoomLightingMood::Count) {
        result.errors.push_back(LightingValidationError::InvalidMood);
    }
    const float height = roomBounds.maximum.y - roomBounds.minimum.y;
    if (!roomBounds.isValid() || !finiteVec3(roomBounds.minimum)
        || !finiteVec3(roomBounds.maximum) || height < 1.0F) {
        result.errors.push_back(LightingValidationError::InvalidBounds);
    }
    if (!validColor(plan.ambientColor) || !std::isfinite(plan.ambientIntensity)
        || plan.ambientIntensity < 0.10F || plan.ambientIntensity > 0.35F) {
        result.errors.push_back(LightingValidationError::InvalidAmbient);
    }
    if (!std::isfinite(plan.fixtureEmissive)
        || plan.fixtureEmissive < 0.2F || plan.fixtureEmissive > 1.5F) {
        result.errors.push_back(LightingValidationError::InvalidFixture);
    }
    if (plan.lights.empty() || plan.lights.size() > maxRoomLights) {
        result.errors.push_back(LightingValidationError::InvalidLightCount);
    }
    for (const RoomPointLight& light : plan.lights) {
        if (!finiteVec3(light.position) || !validColor(light.color)
            || !std::isfinite(light.intensity) || !std::isfinite(light.range)
            || light.intensity < 0.5F || light.intensity > 2.0F
            || light.range < 2.0F || light.range > 12.0F) {
            result.errors.push_back(LightingValidationError::InvalidLight);
        }
        if (!roomBounds.contains(light.position)
            || light.position.y < roomBounds.maximum.y - 0.5F) {
            result.errors.push_back(LightingValidationError::LightOutsideRoom);
        }
    }
    if (plan.planHash == 0U || plan.planHash != roomLightingPlanHash(plan)) {
        result.errors.push_back(LightingValidationError::InvalidPlanHash);
    }
    return result;
}

std::uint64_t roomLightingPlanHash(const RoomLightingPlan& plan) noexcept
{
    std::uint64_t hash = wkc::core::hashCombine(0x4C494748545F504CULL, plan.seed);
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(plan.role));
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(plan.theme));
    hash = wkc::core::hashCombine(hash, static_cast<std::uint8_t>(plan.mood));
    hash = addVec3(hash, plan.ambientColor);
    hash = addFloat(hash, plan.ambientIntensity);
    hash = addFloat(hash, plan.fixtureEmissive);
    hash = wkc::core::hashCombine(hash, plan.supportsAlert ? 1U : 0U);
    hash = wkc::core::hashCombine(hash, plan.lights.size());
    for (const RoomPointLight& light : plan.lights) {
        hash = addVec3(hash, light.position);
        hash = addVec3(hash, light.color);
        hash = addFloat(hash, light.intensity);
        hash = addFloat(hash, light.range);
    }
    return hash;
}

} // namespace gdp::world
