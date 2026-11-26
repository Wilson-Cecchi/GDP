#include "core/Seed.hpp"
#include "game/SeedDomains.hpp"
#include "world/LightingGenerator.hpp"
#include "world/World.hpp"
#include "world/WorldGenerator.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <unordered_set>

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

bool containsError(
    const gdp::world::LightingValidationResult& result,
    const gdp::world::LightingValidationError error)
{
    return std::find(result.errors.begin(), result.errors.end(), error)
        != result.errors.end();
}

constexpr wkc::core::Aabb roomBounds{
    {-4.0F, 0.0F, -5.0F},
    {4.0F, 4.0F, 5.0F},
};

void testDeterministicPlan()
{
    using namespace gdp::world;
    constexpr wkc::core::Seed seed = 0x4C494748545F5631ULL;
    const RoomLightingPlan first = generateRoomLightingPlan(
        seed, RoomRole::Start, RoomTheme::Industrial, roomBounds);
    const RoomLightingPlan repeated = generateRoomLightingPlan(
        seed, RoomRole::Start, RoomTheme::Industrial, roomBounds);
    expect(first.planHash == repeated.planHash && first.lights == repeated.lights,
        "same room seed reproduces the complete lighting plan");
    expect(first.seed == wkc::core::deriveSeed(seed, gdp::game::SeedDomain::Lighting, 0U),
        "lighting plan starts from the isolated Lighting seed domain");
    expect(!first.supportsAlert && first.lights.size() == 4U,
        "safe START lighting is readable without lockdown alert");
    expect(validateRoomLightingPlan(first, roomBounds).isValid(),
        "representative START lighting plan validates");

    const RoomLightingPlan terminal = generateRoomLightingPlan(
        seed, RoomRole::Terminal, RoomTheme::Security, roomBounds);
    expect(terminal.supportsAlert && terminal.lights.size() == 5U
            && validateRoomLightingPlan(terminal, roomBounds).isValid(),
        "dedicated Terminal room supports lockdown lighting");

    constexpr std::uint64_t goldenHash = 0x9B931860A415C85DULL;
    expect(first.planHash == goldenHash,
        "representative lighting plan keeps its golden hash");
}

void testRoleAndThemeContracts()
{
    using namespace gdp::world;
    const RoomLightingPlan secret = generateRoomLightingPlan(
        10, RoomRole::Secret, RoomTheme::Maintenance, roomBounds);
    const RoomLightingPlan boss = generateRoomLightingPlan(
        11, RoomRole::Boss, RoomTheme::Security, roomBounds);
    const RoomLightingPlan laboratory = generateRoomLightingPlan(
        12, RoomRole::Normal, RoomTheme::Laboratory, roomBounds);
    const RoomLightingPlan cafeteria = generateRoomLightingPlan(
        13, RoomRole::Normal, RoomTheme::Cafeteria, roomBounds);
    const RoomLightingPlan hub = generateRoomLightingPlan(
        14, RoomRole::Hub, RoomTheme::Office, roomBounds);

    expect(secret.mood == RoomLightingMood::Dim && secret.lights.size() == 2U
            && secret.ambientIntensity >= 0.10F,
        "secret room remains dim without becoming unreadable");
    expect(boss.mood == RoomLightingMood::Ominous && boss.lights.size() == 5U
            && boss.supportsAlert,
        "boss room receives a full ominous alert-capable rig");
    expect(laboratory.mood == RoomLightingMood::Clinical,
        "laboratory theme receives clinical lighting");
    expect(cafeteria.mood == RoomLightingMood::Warm
            && cafeteria.ambientColor == wkc::core::Vec3{0.84F, 0.81F, 0.75F}
            && cafeteria.lights.front().color.y > 0.85F
            && cafeteria.ambientIntensity >= 0.218F,
        "cafeteria receives the approved softer neutral-warm light");
    expect(hub.lights.size() == 5U,
        "hub receives a fifth physical fill fixture");
}

void testSpaciousRoomLighting()
{
    using namespace gdp::world;
    constexpr wkc::core::Aabb bossBounds{
        {-13.85F, 0.0F, -15.85F}, {13.85F, 7.0F, 15.85F}};
    constexpr wkc::core::Aabb normalBounds{
        {-8.85F, 0.0F, -10.85F}, {8.85F, 6.0F, 10.85F}};
    constexpr wkc::core::Aabb smallerNormalBounds{
        {-7.85F, 0.0F, -8.85F}, {7.85F, 5.0F, 8.85F}};
    constexpr wkc::core::Aabb cafeteriaPreviewBounds{
        {-9.0F, 0.0F, -10.0F}, {9.0F, 6.0F, 10.0F}};
    constexpr wkc::core::Aabb hubBounds{
        {-10.0F, 0.0F, -10.0F}, {10.0F, 6.0F, 10.0F}};
    const auto checkCoverage = [](const RoomLightingPlan& plan,
                                   const wkc::core::Aabb bounds,
                                   const float maximumNormalizedDistance) {
        for (const float x : {bounds.minimum.x + 0.5F, bounds.maximum.x - 0.5F}) {
            for (const float z : {bounds.minimum.z + 0.5F, bounds.maximum.z - 0.5F}) {
                float nearest = 100.0F;
                for (const RoomPointLight& light : plan.lights) {
                    const wkc::core::Vec3 offset
                        = light.position - wkc::core::Vec3{x, 1.7F, z};
                    nearest = std::min(nearest,
                        wkc::core::length(offset) / light.range);
                }
                if (nearest > maximumNormalizedDistance) {
                    return false;
                }
            }
        }
        return true;
    };

    for (wkc::core::Seed seed = 1U; seed <= 1'000U; ++seed) {
        const RoomLightingPlan boss = generateRoomLightingPlan(
            seed, RoomRole::Boss, RoomTheme::Security, bossBounds);
        const RoomLightingPlan normal = generateRoomLightingPlan(
            seed, RoomRole::Normal, RoomTheme::Laboratory, normalBounds);
        const RoomLightingPlan cafeteria = generateRoomLightingPlan(
            seed, RoomRole::Start, RoomTheme::Cafeteria,
            cafeteriaPreviewBounds);
        const RoomLightingPlan hub = generateRoomLightingPlan(
            seed, RoomRole::Hub, RoomTheme::Industrial, hubBounds);
        expect(validateRoomLightingPlan(boss, bossBounds).isValid()
                && validateRoomLightingPlan(normal, normalBounds).isValid()
                && validateRoomLightingPlan(cafeteria,
                    cafeteriaPreviewBounds).isValid()
                && validateRoomLightingPlan(hub, hubBounds).isValid(),
            "spacious room lighting validates across 1,000 seeds");
        expect(boss.lights.size() == 5U && normal.lights.size() == 5U
                && cafeteria.lights.size() == 5U && hub.lights.size() == 5U,
            "spacious rooms add a weaker fifth fixture to the four-light layout");
        expect(boss.ambientIntensity >= 0.198F
                && boss.ambientIntensity <= 0.222F,
            "spacious Boss keeps its dark palette with readable ambient fill");
        expect(checkCoverage(boss, bossBounds, 1.20F)
                && checkCoverage(normal, normalBounds, 1.20F)
                && checkCoverage(cafeteria, cafeteriaPreviewBounds, 1.20F)
                && checkCoverage(hub, hubBounds, 1.20F),
            "room lighting plans reach every corner at camera height");
        expect(cafeteria.ambientIntensity >= 0.218F
                && cafeteria.ambientIntensity <= 0.242F
                && cafeteria.fixtureEmissive >= 0.74F
                && cafeteria.fixtureEmissive <= 0.82F,
            "Cafeteria keeps the approved comfortable fill and fixture range");
        for (std::size_t index = 0U; index < cafeteria.lights.size(); ++index) {
            const RoomPointLight& light = cafeteria.lights[index];
            expect(index == 4U
                    ? light.range >= 4.5F && light.range <= 8.0F
                        && light.intensity == 0.55F
                    : light.range >= 8.8F && light.range <= 9.4F
                        && light.intensity >= 0.96F && light.intensity <= 1.08F,
                "Cafeteria fixtures spread light evenly without hotspots");
        }
        for (std::size_t index = 0U; index < boss.lights.size(); ++index) {
            const RoomPointLight& light = boss.lights[index];
            expect((index == 4U
                    ? light.range >= 4.5F && light.range <= 8.0F
                        && light.intensity == 0.55F
                    : light.range >= 11.0F && light.range <= 11.8F
                        && light.intensity >= 1.05F && light.intensity <= 1.25F)
                    && light.position.x > bossBounds.minimum.x + 5.0F
                    && light.position.x < bossBounds.maximum.x - 5.0F
                    && light.position.z > bossBounds.minimum.z + 5.0F
                    && light.position.z < bossBounds.maximum.z - 5.0F,
                "Boss fixtures keep controlled reach away from room boundaries");
        }
        for (std::size_t index = 0U; index < normal.lights.size(); ++index) {
            const RoomPointLight& light = normal.lights[index];
            expect(index == 4U
                    ? light.range >= 4.5F && light.range <= 8.0F
                        && light.intensity == 0.55F
                    : light.range >= 8.5F && light.range <= 9.3F
                        && light.intensity >= 0.96F && light.intensity <= 1.14F,
                "spacious Normal uses a shorter local lighting reach");
        }
        for (std::size_t index = 0U; index < hub.lights.size(); ++index) {
            const RoomPointLight& light = hub.lights[index];
            expect(index == 4U
                    ? light.range == 6.5F && light.intensity == 0.55F
                    : light.range >= 8.5F && light.range <= 9.3F
                        && light.intensity >= 0.96F
                        && light.intensity <= 1.14F,
                "Hub fill fixture remains weaker than the four main lights");
            expect(std::abs(light.position.y - 5.52F) < 0.001F,
                "Hub fixtures remain suspended at the ceiling height");
            if (index == 4U) {
                expect(std::abs(light.position.x - 5.0F) < 0.001F
                        && std::abs(light.position.z) < 0.001F,
                    "fifth Hub fixture fills the barricade side");
            } else {
                expect(std::abs(std::abs(light.position.x) - 5.0F) < 0.001F
                        && std::abs(std::abs(light.position.z) - 5.0F) < 0.001F,
                    "original Hub fixtures keep their four quadrants");
            }
        }
    }
    const RoomLightingPlan smallerNormal = generateRoomLightingPlan(
        77U, RoomRole::Normal, RoomTheme::Industrial, smallerNormalBounds);
    expect(smallerNormal.lights.size() == 5U
            && validateRoomLightingPlan(smallerNormal, smallerNormalBounds).isValid(),
        "a 16 by 18 module retains the spacious lighting plan after wall inset");
}

void testValidationFailures()
{
    using namespace gdp::world;
    RoomLightingPlan invalid = generateRoomLightingPlan(
        22, RoomRole::Arena, RoomTheme::Security, roomBounds);
    invalid.lights.front().position.x = roomBounds.maximum.x + 1.0F;
    invalid.ambientIntensity = 0.0F;
    const LightingValidationResult result = validateRoomLightingPlan(invalid, roomBounds);
    expect(containsError(result, LightingValidationError::InvalidAmbient),
        "validator rejects unreadable ambient light");
    expect(containsError(result, LightingValidationError::LightOutsideRoom),
        "validator rejects fixtures outside room bounds");
    expect(containsError(result, LightingValidationError::InvalidPlanHash),
        "validator detects mutated plans through their hash");

    const wkc::core::Aabb flatBounds{{0.0F, 0.0F, 0.0F}, {1.0F, 0.2F, 1.0F}};
    const RoomLightingPlan flat = generateRoomLightingPlan(
        23, RoomRole::Normal, RoomTheme::Office, flatBounds);
    expect(containsError(validateRoomLightingPlan(flat, flatBounds),
               LightingValidationError::InvalidBounds),
        "validator rejects rooms too flat to host ceiling lights");
}

void testSeedCoverageAndWorldIsolation()
{
    using namespace gdp::world;
    std::unordered_set<std::uint64_t> hashes;
    std::array<bool, static_cast<std::size_t>(RoomLightingMood::Count)> moods{};
    for (std::uint64_t index = 1; index <= 1'000U; ++index) {
        const RoomRole role = static_cast<RoomRole>(index % static_cast<std::uint64_t>(RoomRole::Count));
        const RoomTheme theme = static_cast<RoomTheme>(index % 7U);
        const RoomLightingPlan plan = generateRoomLightingPlan(
            index * 7919U, role, theme, roomBounds);
        expect(validateRoomLightingPlan(plan, roomBounds).isValid(),
            "lighting plan validates across deterministic seed coverage");
        hashes.insert(plan.planHash);
        moods[static_cast<std::size_t>(plan.mood)] = true;
    }
    expect(hashes.size() == 1'000U,
        "1,000 room seeds produce 1,000 distinct lighting plans");
    expect(std::all_of(moods.begin(), moods.end(), [](const bool seen) { return seen; }),
        "seed and role coverage reaches every lighting mood");

    for (const wkc::core::Seed worldSeed : {wkc::core::Seed{12345},
             wkc::core::Seed{0x4744505F574F524CULL}}) {
        const WorldGenerationResult generated = generateWorld(worldSeed);
        bool valid = generated.world.has_value();
        if (generated.world) {
            const std::uint64_t structuralHash = worldFingerprint(*generated.world);
            for (const RoomInstance& room : generated.world->rooms) {
                const RoomLightingPlan plan = generateRoomLightingPlan(
                    room.seed, room.role, room.theme, room.worldBounds);
                valid = valid && validateRoomLightingPlan(plan, room.worldBounds).isValid();
            }
            valid = valid && worldFingerprint(*generated.world) == structuralHash;
        }
        expect(valid,
            "documented worlds validate lighting without changing structural fingerprints");
    }
}

} // namespace

int main()
{
    testDeterministicPlan();
    testRoleAndThemeContracts();
    testSpaciousRoomLighting();
    testValidationFailures();
    testSeedCoverageAndWorldIsolation();

    if (failures != 0) {
        std::cerr << failures << " lighting test(s) failed\n";
        return 1;
    }
    std::cout << "Procedural room lighting tests passed\n";
    return 0;
}
