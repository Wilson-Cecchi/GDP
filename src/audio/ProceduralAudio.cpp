#include "audio/ProceduralAudio.hpp"

#include "core/Hash.hpp"
#include "core/Random.hpp"
#include "game/SeedDomains.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <numbers>

namespace gdp::audio {
namespace {

std::uint64_t addFloat(const std::uint64_t hash, const float value) noexcept
{
    return gdp::core::hashCombine(hash, std::bit_cast<std::uint32_t>(value));
}

void configureRecipe(AudioRecipe& recipe) noexcept
{
    switch (recipe.kind) {
    case AudioEventKind::WeaponFire:
        recipe.durationSeconds = 0.18F;
        recipe.baseFrequency = 92.0F;
        recipe.secondaryFrequency = 54.0F;
        recipe.attackSeconds = 0.002F;
        recipe.releaseSeconds = 0.17F;
        recipe.noiseMix = 0.72F;
        recipe.gain = 0.58F;
        break;
    case AudioEventKind::DryFire:
        recipe.durationSeconds = 0.065F;
        recipe.baseFrequency = 920.0F;
        recipe.secondaryFrequency = 510.0F;
        recipe.attackSeconds = 0.001F;
        recipe.releaseSeconds = 0.055F;
        recipe.noiseMix = 0.18F;
        recipe.gain = 0.24F;
        break;
    case AudioEventKind::Reload:
        recipe.durationSeconds = 0.16F;
        recipe.baseFrequency = 380.0F;
        recipe.secondaryFrequency = 690.0F;
        recipe.attackSeconds = 0.003F;
        recipe.releaseSeconds = 0.11F;
        recipe.noiseMix = 0.30F;
        recipe.pulseRate = 11.0F;
        recipe.gain = 0.28F;
        break;
    case AudioEventKind::EnemyHit:
        recipe.durationSeconds = 0.11F;
        recipe.baseFrequency = 155.0F;
        recipe.secondaryFrequency = 84.0F;
        recipe.attackSeconds = 0.002F;
        recipe.releaseSeconds = 0.10F;
        recipe.noiseMix = 0.48F;
        recipe.gain = 0.36F;
        break;
    case AudioEventKind::EnemyDeath:
        recipe.durationSeconds = 0.34F;
        recipe.baseFrequency = 128.0F;
        recipe.secondaryFrequency = 42.0F;
        recipe.attackSeconds = 0.004F;
        recipe.releaseSeconds = 0.31F;
        recipe.noiseMix = 0.42F;
        recipe.gain = 0.42F;
        break;
    case AudioEventKind::PropImpact:
        recipe.durationSeconds = 0.095F;
        recipe.baseFrequency = 230.0F;
        recipe.secondaryFrequency = 72.0F;
        recipe.attackSeconds = 0.001F;
        recipe.releaseSeconds = 0.09F;
        recipe.noiseMix = 0.64F;
        recipe.gain = 0.34F;
        break;
    case AudioEventKind::PropDestroyed:
        recipe.durationSeconds = 0.27F;
        recipe.baseFrequency = 112.0F;
        recipe.secondaryFrequency = 38.0F;
        recipe.attackSeconds = 0.002F;
        recipe.releaseSeconds = 0.25F;
        recipe.noiseMix = 0.78F;
        recipe.gain = 0.44F;
        break;
    case AudioEventKind::Pickup:
        recipe.durationSeconds = 0.20F;
        recipe.baseFrequency = 520.0F;
        recipe.secondaryFrequency = 980.0F;
        recipe.attackSeconds = 0.006F;
        recipe.releaseSeconds = 0.15F;
        recipe.noiseMix = 0.03F;
        recipe.gain = 0.25F;
        break;
    case AudioEventKind::EventStart:
        recipe.durationSeconds = 0.42F;
        recipe.baseFrequency = 210.0F;
        recipe.secondaryFrequency = 108.0F;
        recipe.attackSeconds = 0.012F;
        recipe.releaseSeconds = 0.30F;
        recipe.noiseMix = 0.08F;
        recipe.pulseRate = 5.5F;
        recipe.gain = 0.34F;
        break;
    case AudioEventKind::EventComplete:
        recipe.durationSeconds = 0.36F;
        recipe.baseFrequency = 360.0F;
        recipe.secondaryFrequency = 760.0F;
        recipe.attackSeconds = 0.008F;
        recipe.releaseSeconds = 0.25F;
        recipe.noiseMix = 0.02F;
        recipe.gain = 0.27F;
        break;
    case AudioEventKind::PlayerHurt:
        recipe.durationSeconds = 0.14F;
        recipe.baseFrequency = 118.0F;
        recipe.secondaryFrequency = 58.0F;
        recipe.attackSeconds = 0.002F;
        recipe.releaseSeconds = 0.13F;
        recipe.noiseMix = 0.36F;
        recipe.gain = 0.38F;
        break;
    case AudioEventKind::Ambient:
        recipe.durationSeconds = 2.0F;
        recipe.baseFrequency = 48.0F;
        recipe.secondaryFrequency = 56.0F;
        recipe.attackSeconds = 0.18F;
        recipe.releaseSeconds = 0.18F;
        recipe.noiseMix = 0.06F;
        recipe.pulseRate = 0.42F;
        recipe.gain = 0.12F;
        break;
    case AudioEventKind::Count:
        break;
    }
}

} // namespace

bool AudioValidationResult::isValid() const noexcept
{
    return errors.empty();
}

AudioRecipe generateAudioRecipe(
    const gdp::core::Seed sourceSeed,
    const AudioEventKind kind,
    const std::uint64_t eventCounter)
{
    AudioRecipe recipe;
    recipe.kind = kind;
    const std::uint64_t localId = (static_cast<std::uint64_t>(kind) << 56U)
        ^ eventCounter;
    recipe.seed = gdp::core::deriveSeed(sourceSeed, gdp::game::SeedDomain::Audio, localId);
    configureRecipe(recipe);

    if (kind == AudioEventKind::Ambient) {
        constexpr std::array baseFrequencies{
            48.0F, 42.0F, 60.0F, 72.0F, 38.0F, 54.0F, 64.0F,
        };
        constexpr std::array secondaryFrequencies{
            56.0F, 51.0F, 68.0F, 96.0F, 47.0F, 63.0F, 80.0F,
        };
        constexpr std::array pulseRates{
            0.42F, 0.31F, 0.18F, 0.25F, 0.55F, 0.80F, 0.20F,
        };
        constexpr std::array noiseMixes{
            0.06F, 0.08F, 0.03F, 0.02F, 0.11F, 0.05F, 0.025F,
        };
        const std::size_t profile = static_cast<std::size_t>(
            eventCounter % baseFrequencies.size());
        recipe.baseFrequency = baseFrequencies[profile];
        recipe.secondaryFrequency = secondaryFrequencies[profile];
        recipe.pulseRate = pulseRates[profile];
        recipe.noiseMix = noiseMixes[profile];
    }

    if (kind < AudioEventKind::Count) {
        gdp::core::RandomStream random{gdp::core::deriveSeed(
            recipe.seed, gdp::game::SeedDomain::Audio, 1U)};
        const float pitchVariation = random.rangeFloat(0.94F, 1.06F);
        recipe.baseFrequency *= pitchVariation;
        recipe.secondaryFrequency *= pitchVariation;
        recipe.durationSeconds *= random.rangeFloat(0.96F, 1.04F);
        recipe.noiseMix = std::clamp(
            recipe.noiseMix + random.rangeFloat(-0.018F, 0.018F), 0.0F, 1.0F);
    }
    recipe.recipeHash = audioRecipeHash(recipe);
    return recipe;
}

AudioValidationResult validateAudioRecipe(const AudioRecipe& recipe)
{
    AudioValidationResult result;
    if (recipe.kind >= AudioEventKind::Count) {
        result.errors.push_back(AudioValidationError::InvalidKind);
    }
    if (recipe.seed == 0U) {
        result.errors.push_back(AudioValidationError::InvalidSeed);
    }
    if (!std::isfinite(recipe.durationSeconds)
        || recipe.durationSeconds < 0.03F || recipe.durationSeconds > 3.0F) {
        result.errors.push_back(AudioValidationError::InvalidDuration);
    }
    if (!std::isfinite(recipe.baseFrequency)
        || !std::isfinite(recipe.secondaryFrequency)
        || recipe.baseFrequency < 20.0F || recipe.baseFrequency > 4'000.0F
        || recipe.secondaryFrequency < 20.0F || recipe.secondaryFrequency > 4'000.0F) {
        result.errors.push_back(AudioValidationError::InvalidFrequency);
    }
    if (!std::isfinite(recipe.attackSeconds) || !std::isfinite(recipe.releaseSeconds)
        || recipe.attackSeconds < 0.0F || recipe.releaseSeconds < 0.0F
        || recipe.attackSeconds > recipe.durationSeconds
        || recipe.releaseSeconds > recipe.durationSeconds) {
        result.errors.push_back(AudioValidationError::InvalidEnvelope);
    }
    if (!std::isfinite(recipe.noiseMix) || !std::isfinite(recipe.pulseRate)
        || recipe.noiseMix < 0.0F || recipe.noiseMix > 1.0F
        || recipe.pulseRate < 0.0F || recipe.pulseRate > 30.0F) {
        result.errors.push_back(AudioValidationError::InvalidMix);
    }
    if (!std::isfinite(recipe.gain) || recipe.gain <= 0.0F || recipe.gain > 1.0F) {
        result.errors.push_back(AudioValidationError::InvalidGain);
    }
    if (recipe.recipeHash == 0U || recipe.recipeHash != audioRecipeHash(recipe)) {
        result.errors.push_back(AudioValidationError::InvalidHash);
    }
    return result;
}

AudioClip synthesizeAudioClip(const AudioRecipe& recipe, const int sampleRate)
{
    AudioClip clip;
    clip.sampleRate = sampleRate;
    clip.recipeHash = recipe.recipeHash;
    if (!validateAudioRecipe(recipe).isValid() || sampleRate < 8'000 || sampleRate > 192'000) {
        return clip;
    }

    const std::size_t sampleCount = static_cast<std::size_t>(std::max(
        1.0F, std::round(recipe.durationSeconds * static_cast<float>(sampleRate))));
    clip.samples.resize(sampleCount);
    gdp::core::RandomStream noise{gdp::core::deriveSeed(
        recipe.seed, gdp::game::SeedDomain::Audio, 2U)};
    constexpr float twoPi = 2.0F * std::numbers::pi_v<float>;
    for (std::size_t index = 0; index < sampleCount; ++index) {
        const float time = static_cast<float>(index) / static_cast<float>(sampleRate);
        const float progress = time / recipe.durationSeconds;
        const float sweepPhase = twoPi * ((recipe.baseFrequency * time)
            + (0.5F * (recipe.secondaryFrequency - recipe.baseFrequency)
                * time * progress));
        const float tone = (std::sin(sweepPhase)
            + (0.34F * std::sin((sweepPhase * 0.5F) + 0.37F))) / 1.34F;
        const float randomSample = (noise.uniform01() * 2.0F) - 1.0F;
        float envelope = 1.0F;
        if (recipe.attackSeconds > 0.0F && time < recipe.attackSeconds) {
            envelope *= time / recipe.attackSeconds;
        }
        const float releaseStart = recipe.durationSeconds - recipe.releaseSeconds;
        if (recipe.releaseSeconds > 0.0F && time > releaseStart) {
            envelope *= std::max(
                0.0F, (recipe.durationSeconds - time) / recipe.releaseSeconds);
        }
        const float pulse = recipe.pulseRate <= 0.0F
            ? 1.0F
            : 0.68F + (0.32F * std::sin(twoPi * recipe.pulseRate * time));
        const float mixed = (tone * (1.0F - recipe.noiseMix))
            + (randomSample * recipe.noiseMix);
        clip.samples[index] = std::clamp(
            mixed * envelope * pulse * recipe.gain, -1.0F, 1.0F);
    }
    return clip;
}

AudioValidationResult validateAudioClip(
    const AudioClip& clip,
    const AudioRecipe& recipe)
{
    AudioValidationResult result = validateAudioRecipe(recipe);
    const std::size_t expected = clip.sampleRate > 0
        ? static_cast<std::size_t>(std::max(1.0F,
            std::round(recipe.durationSeconds * static_cast<float>(clip.sampleRate))))
        : 0U;
    if (clip.sampleRate < 8'000 || clip.sampleRate > 192'000
        || clip.samples.size() != expected || clip.recipeHash != recipe.recipeHash) {
        result.errors.push_back(AudioValidationError::InvalidClip);
    }
    for (const float sample : clip.samples) {
        if (!std::isfinite(sample) || sample < -1.0F || sample > 1.0F) {
            result.errors.push_back(AudioValidationError::ClippedSample);
            break;
        }
    }
    return result;
}

std::uint64_t audioRecipeHash(const AudioRecipe& recipe) noexcept
{
    std::uint64_t hash = gdp::core::hashCombine(0x415544494F5F5243ULL, recipe.seed);
    hash = gdp::core::hashCombine(hash, static_cast<std::uint8_t>(recipe.kind));
    hash = addFloat(hash, recipe.durationSeconds);
    hash = addFloat(hash, recipe.baseFrequency);
    hash = addFloat(hash, recipe.secondaryFrequency);
    hash = addFloat(hash, recipe.attackSeconds);
    hash = addFloat(hash, recipe.releaseSeconds);
    hash = addFloat(hash, recipe.noiseMix);
    hash = addFloat(hash, recipe.pulseRate);
    return addFloat(hash, recipe.gain);
}

} // namespace gdp::audio
