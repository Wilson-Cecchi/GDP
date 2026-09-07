#include "game/LaunchSeed.hpp"

#include "core/Hash.hpp"

namespace gdp::game {

wkc::core::Seed selectLaunchSeed(
    const bool headless,
    const std::optional<wkc::core::Seed> requestedSeed,
    const wkc::core::Seed launchEntropy) noexcept
{
    if (requestedSeed) {
        return *requestedSeed;
    }
    if (headless) {
        return deterministicDefaultWorldSeed;
    }
    constexpr wkc::core::Seed launchSeedSalt = 0x4C41554E43485345ULL;
    return wkc::core::hashCombine(launchSeedSalt, launchEntropy);
}

} // namespace gdp::game
