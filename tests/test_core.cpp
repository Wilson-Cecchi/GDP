#include "core/Hash.hpp"
#include "core/Math.hpp"
#include "core/Random.hpp"
#include "core/Seed.hpp"
#include "core/Time.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string_view>

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void testHashAndSeed()
{
    using namespace gdp::core;

    expect(fnv1a64("") == 0xCBF29CE484222325ULL, "FNV-1a empty-string golden value");
    expect(fnv1a64("GDP") == 0xEA7B99199804A402ULL, "FNV-1a GDP golden value");
    expect(fnv1a64("GDP") != fnv1a64("gdp"), "string hashes distinguish content");

    constexpr Seed root = 0x123456789ABCDEF0ULL;
    const Seed roomA = deriveSeed(root, SeedDomain::Room, 42);
    const Seed roomARepeat = deriveSeed(root, SeedDomain::Room, 42);
    const Seed roomB = deriveSeed(root, SeedDomain::Room, 43);
    const Seed lootA = deriveSeed(root, SeedDomain::Loot, 42);

    expect(roomA == roomARepeat, "derived seeds are repeatable");
    expect(roomA == 0x75247D92702F490EULL, "derived seed golden value is stable");
    expect(roomA != roomB, "local IDs create independent seeds");
    expect(roomA != lootA, "domains create independent seeds");
    expect(
        roomA != deriveSeed(root, SeedDomain::Room, 42, GenerationVersion{2, 0}),
        "generation versions change derived seeds");
}

void testRandom()
{
    using namespace gdp::core;

    RandomStream golden{0};
    expect(golden.nextU64() == 0xE220A8397B1DCDAFULL, "SplitMix64 first golden value");
    expect(golden.nextU64() == 0x6E789E6AA1B965F4ULL, "SplitMix64 second golden value");
    expect(golden.nextU64() == 0x06C45D188009454FULL, "SplitMix64 third golden value");

    RandomStream first{987654321};
    RandomStream second{987654321};
    for (int index = 0; index < 128; ++index) {
        expect(first.nextU64() == second.nextU64(), "equal seeds produce equal sequences");
    }

    RandomStream ranges{123};
    for (int index = 0; index < 1'000; ++index) {
        const int integer = ranges.rangeInt(-7, 11);
        const float real = ranges.rangeFloat(-2.0F, 3.0F);
        expect(integer >= -7 && integer <= 11, "integer random value stays in range");
        expect(real >= -2.0F && real <= 3.0F, "float random value stays in range");
    }
    static_cast<void>(ranges.rangeInt(
        std::numeric_limits<int>::min(),
        std::numeric_limits<int>::max()));
    expect(!ranges.chance(0.0F), "zero probability never succeeds");
    expect(ranges.chance(1.0F), "unit probability always succeeds");
}

std::uint64_t simulateTenSeconds(const int renderRate)
{
    gdp::core::FixedStepClock clock;
    for (int frame = 0; frame < renderRate * 10; ++frame) {
        clock.advanceBy(1.0 / static_cast<double>(renderRate));
        while (clock.consumeFixedStep()) {
        }
    }
    return clock.totalSteps();
}

void testTime()
{
    using gdp::core::FixedStepClock;

    expect(simulateTenSeconds(30) == 600, "30 FPS rendering preserves 60 Hz simulation");
    expect(simulateTenSeconds(60) == 600, "60 FPS rendering preserves 60 Hz simulation");
    expect(simulateTenSeconds(144) == 600, "144 FPS rendering preserves 60 Hz simulation");

    FixedStepClock stalled;
    stalled.advanceBy(10.0);
    while (stalled.consumeFixedStep()) {
    }
    expect(stalled.totalSteps() == 15, "long frames are clamped to avoid a death spiral");

    FixedStepClock interpolation;
    interpolation.advanceBy(interpolation.fixedStepSeconds() * 0.5);
    expect(
        interpolation.interpolationAlpha() > 0.49
            && interpolation.interpolationAlpha() < 0.51,
        "interpolation alpha exposes the remaining fraction");
}

void testMath()
{
    using namespace gdp::core;

    constexpr std::array positions{
        GridPos{0, 0, 0},
        GridPos{1, -2, 3},
        GridPos{-17, 4, 29},
    };
    for (const GridPos position : positions) {
        expect(worldToGrid(gridToWorld(position, 2.5F), 2.5F) == position,
            "grid/world conversion round-trips");
    }

    constexpr Vec3 connector{2.0F, 1.0F, 3.0F};
    expect(rotateQuarterTurns(connector, QuarterTurn::Degrees0) == Vec3{2.0F, 1.0F, 3.0F},
        "zero-degree module rotation");
    expect(rotateQuarterTurns(connector, QuarterTurn::Degrees90) == Vec3{3.0F, 1.0F, -2.0F},
        "90-degree module rotation");
    expect(rotateQuarterTurns(connector, QuarterTurn::Degrees180) == Vec3{-2.0F, 1.0F, -3.0F},
        "180-degree module rotation");
    expect(rotateQuarterTurns(connector, QuarterTurn::Degrees270) == Vec3{-3.0F, 1.0F, 2.0F},
        "270-degree module rotation");

    constexpr Aabb bounds{{-1.0F, 0.0F, -1.0F}, {1.0F, 2.0F, 1.0F}};
    expect(bounds.isValid(), "ordered AABB is valid");
    expect(bounds.contains({0.0F, 1.0F, 0.0F}), "AABB contains an interior point");
    expect(!bounds.contains({2.0F, 1.0F, 0.0F}), "AABB rejects an exterior point");
    expect(translated(bounds, {4.0F, 0.0F, 0.0F}).contains({4.0F, 1.0F, 0.0F}),
        "translated AABB follows its module");
}

} // namespace

int main()
{
    testHashAndSeed();
    testRandom();
    testTime();
    testMath();
    if (failures != 0) {
        std::cerr << failures << " core test(s) failed\n";
        return 1;
    }

    std::cout << "All GDP core tests passed\n";
    return 0;
}
