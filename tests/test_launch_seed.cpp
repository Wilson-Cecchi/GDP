#include "game/LaunchSeed.hpp"
#include "world/World.hpp"
#include "world/WorldGenerator.hpp"

#include <iostream>
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

void testRequestedSeedAlwaysWins()
{
    constexpr wkc::core::Seed requested = 12345U;
    expect(gdp::game::selectLaunchSeed(false, requested, 111U) == requested
            && gdp::game::selectLaunchSeed(true, requested, 222U) == requested,
        "an explicit command-line seed wins in graphical and headless modes");
}

void testHeadlessDefaultRemainsDeterministic()
{
    expect(gdp::game::selectLaunchSeed(true, std::nullopt, 111U)
                == gdp::game::deterministicDefaultWorldSeed
            && gdp::game::selectLaunchSeed(true, std::nullopt, 222U)
                == gdp::game::deterministicDefaultWorldSeed,
        "headless execution without an explicit seed keeps the stable test seed");
}

void testGraphicalLaunchUsesEntropyDeterministically()
{
    const wkc::core::Seed first
        = gdp::game::selectLaunchSeed(false, std::nullopt, 111U);
    const wkc::core::Seed repeated
        = gdp::game::selectLaunchSeed(false, std::nullopt, 111U);
    const wkc::core::Seed next
        = gdp::game::selectLaunchSeed(false, std::nullopt, 222U);
    expect(first == repeated,
        "the same launch entropy produces the same initial world seed");
    expect(first != next,
        "different launch entropy produces a different initial world seed");

    const gdp::world::WorldGenerationResult firstWorld
        = gdp::world::generateWorld(first);
    const gdp::world::WorldGenerationResult nextWorld
        = gdp::world::generateWorld(next);
    expect(firstWorld.succeeded() && nextWorld.succeeded(),
        "automatically selected launch seeds both generate valid worlds");
    if (firstWorld.world && nextWorld.world) {
        expect(gdp::world::worldFingerprint(*firstWorld.world)
                != gdp::world::worldFingerprint(*nextWorld.world),
            "different graphical launches produce structurally different maps");
    }
}

} // namespace

int main()
{
    testRequestedSeedAlwaysWins();
    testHeadlessDefaultRemainsDeterministic();
    testGraphicalLaunchUsesEntropyDeterministically();
    if (failures != 0) {
        std::cerr << failures << " launch seed test(s) failed\n";
        return 1;
    }
    std::cout << "Launch seed tests passed\n";
    return 0;
}
