#pragma once

#include "core/Seed.hpp"

#include <cstdint>
#include <vector>

namespace gdp::audio {

inline constexpr int proceduralAudioSampleRate = 48'000;

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

struct AudioRecipe {
    AudioEventKind kind{AudioEventKind::WeaponFire};
    gdp::core::Seed seed{0};
    float durationSeconds{0.1F};
    float baseFrequency{220.0F};
    float secondaryFrequency{220.0F};
    float attackSeconds{0.005F};
    float releaseSeconds{0.08F};
    float noiseMix{0.0F};
    float pulseRate{0.0F};
    float gain{0.5F};
    std::uint64_t recipeHash{0};
};

struct AudioClip {
    int sampleRate{proceduralAudioSampleRate};
    std::vector<float> samples;
    std::uint64_t recipeHash{0};
};

enum class AudioValidationError : std::uint8_t {
    InvalidKind,
    InvalidSeed,
    InvalidDuration,
    InvalidFrequency,
    InvalidEnvelope,
    InvalidMix,
    InvalidGain,
    InvalidHash,
    InvalidClip,
    ClippedSample,
};

struct AudioValidationResult {
    std::vector<AudioValidationError> errors;

    [[nodiscard]] bool isValid() const noexcept;
};

[[nodiscard]] AudioRecipe generateAudioRecipe(
    gdp::core::Seed sourceSeed,
    AudioEventKind kind,
    std::uint64_t eventCounter = 0U);

[[nodiscard]] AudioValidationResult validateAudioRecipe(
    const AudioRecipe& recipe);

[[nodiscard]] AudioClip synthesizeAudioClip(
    const AudioRecipe& recipe,
    int sampleRate = proceduralAudioSampleRate);

[[nodiscard]] AudioValidationResult validateAudioClip(
    const AudioClip& clip,
    const AudioRecipe& recipe);

[[nodiscard]] std::uint64_t audioRecipeHash(
    const AudioRecipe& recipe) noexcept;

} // namespace gdp::audio
