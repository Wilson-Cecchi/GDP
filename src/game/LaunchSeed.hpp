#pragma once

#include "core/Seed.hpp"

#include <optional>

namespace gdp::game {

inline constexpr wkc::core::Seed deterministicDefaultWorldSeed
    = 0x4744505F574F524CULL;

[[nodiscard]] wkc::core::Seed selectLaunchSeed(
    bool headless,
    std::optional<wkc::core::Seed> requestedSeed,
    wkc::core::Seed launchEntropy) noexcept;

} // namespace gdp::game
