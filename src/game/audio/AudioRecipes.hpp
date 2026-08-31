#pragma once

#include "audio/AudioFoundation.hpp"

#include <cstdint>
#include <vector>

namespace gdp::audio {

enum class AudioEventKind : std::uint8_t {
    WeaponFire,
    DryFire,
    Reload,
    EnemyHit,
    EnemyDeath,
    PropImpact,
    PropDestroyed,
    Pickup,
    EventStart,
    EventComplete,
    PlayerHurt,
    Ambient,
    Count,
};

[[nodiscard]] wkc::audio::AudioRecipe generateAudioRecipe(
    gdp::core::Seed sourceSeed,
    AudioEventKind kind,
    std::uint64_t eventCounter = 0U);

} // namespace gdp::audio
