#include "audio/AudioFoundation.hpp"

#include "core/Hash.hpp"
#include "core/Random.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <numbers>

namespace wkc::audio {
namespace {

// Versioned synthesis substream. Its numeric value preserves existing
// waveforms while keeping the reusable foundation independent of game domains.
inline constexpr gdp::core::SeedDomainId audioNoiseSeedDomain = 16U;

std::uint64_t addFloat(const std::uint64_t hash, const float value) noexcept
{
    return wkc::core::hashCombine(hash, std::bit_cast<std::uint32_t>(value));
}

} // namespace

bool AudioValidationResult::isValid() const noexcept
{
    return errors.empty();
}

AudioValidationResult validateAudioRecipe(const AudioRecipe& recipe)
{
    AudioValidationResult result;
    if (recipe.kindCount == 0U || recipe.kind >= recipe.kindCount) {
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
        recipe.seed, audioNoiseSeedDomain, 2U)};
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
    std::uint64_t hash = wkc::core::hashCombine(0x415544494F5F5243ULL, recipe.seed);
    hash = wkc::core::hashCombine(hash, recipe.kind);
    hash = addFloat(hash, recipe.durationSeconds);
    hash = addFloat(hash, recipe.baseFrequency);
    hash = addFloat(hash, recipe.secondaryFrequency);
    hash = addFloat(hash, recipe.attackSeconds);
    hash = addFloat(hash, recipe.releaseSeconds);
    hash = addFloat(hash, recipe.noiseMix);
    hash = addFloat(hash, recipe.pulseRate);
    return addFloat(hash, recipe.gain);
}

} // namespace wkc::audio
