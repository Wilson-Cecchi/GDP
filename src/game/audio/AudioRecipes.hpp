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
    DoorOpen,
    DoorClose,
    DoorLock,
    Footstep,
    Count,
};

[[nodiscard]] wkc::audio::AudioRecipe generateAudioRecipe(
    wkc::core::Seed sourceSeed,
    AudioEventKind kind,
    std::uint64_t eventCounter = 0U);

} // namespace gdp::audio
