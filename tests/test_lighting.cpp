#include "core/Seed.hpp"
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

constexpr gdp::core::Aabb roomBounds{
    {-4.0F, 0.0F, -5.0F},
    {4.0F, 4.0F, 5.0F},
};

void testDeterministicPlan()
{
    using namespace gdp::world;
    constexpr gdp::core::Seed seed = 0x4C494748545F5631ULL;
    const RoomLightingPlan first = generateRoomLightingPlan(
        seed, RoomRole::Start, RoomTheme::Industrial, roomBounds);
    const RoomLightingPlan repeated = generateRoomLightingPlan(
        seed, RoomRole::Start, RoomTheme::Industrial, roomBounds);
    expect(first.planHash == repeated.planHash && first.lights == repeated.lights,
        "same room seed reproduces the complete lighting plan");
    expect(first.seed == gdp::core::deriveSeed(seed, gdp::core::SeedDomain::Lighting, 0U),
        "lighting plan starts from the isolated Lighting seed domain");
    expect(first.supportsAlert && first.lights.size() == 3U,
        "START lighting is readable and can react to lockdown");
    expect(validateRoomLightingPlan(first, roomBounds).isValid(),
        "representative START lighting plan validates");

    constexpr std::uint64_t goldenHash = 0x2D7AF8E8762A807FULL;
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

    expect(secret.mood == RoomLightingMood::Dim && secret.lights.size() == 1U
            && secret.ambientIntensity >= 0.10F,
        "secret room remains dim without becoming unreadable");
    expect(boss.mood == RoomLightingMood::Ominous && boss.lights.size() == 4U
            && boss.supportsAlert,
        "boss room receives a full ominous alert-capable rig");
    expect(laboratory.mood == RoomLightingMood::Clinical,
        "laboratory theme receives clinical lighting");
    expect(cafeteria.mood == RoomLightingMood::Warm,
        "cafeteria theme receives warm lighting");
    expect(hub.lights.size() == 4U,
        "hub receives four fixtures for navigation readability");
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

    const gdp::core::Aabb flatBounds{{0.0F, 0.0F, 0.0F}, {1.0F, 0.2F, 1.0F}};
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

    for (const gdp::core::Seed worldSeed : {gdp::core::Seed{12345},
             gdp::core::Seed{0x4744505F574F524CULL}}) {
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
    testValidationFailures();
    testSeedCoverageAndWorldIsolation();

    if (failures != 0) {
        std::cerr << failures << " lighting test(s) failed\n";
        return 1;
    }
    std::cout << "Procedural room lighting tests passed\n";
    return 0;
}
