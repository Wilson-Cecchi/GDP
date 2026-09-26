#include "game/audio/AudioRecipes.hpp"

#include "core/Random.hpp"
#include "game/SeedDomains.hpp"

#include <algorithm>
#include <array>

namespace gdp::audio {
namespace {

void configureRecipe(
    wkc::audio::AudioRecipe& recipe,
    const AudioEventKind kind) noexcept
{
    switch (kind) {
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
    case AudioEventKind::DoorOpen:
        recipe.durationSeconds = 0.46F;
        recipe.baseFrequency = 86.0F;
        recipe.secondaryFrequency = 174.0F;
        recipe.attackSeconds = 0.008F;
        recipe.releaseSeconds = 0.36F;
        recipe.noiseMix = 0.20F;
        recipe.pulseRate = 6.5F;
        recipe.gain = 0.30F;
        break;
    case AudioEventKind::DoorClose:
        recipe.durationSeconds = 0.60F;
        recipe.baseFrequency = 118.0F;
        recipe.secondaryFrequency = 72.0F;
        recipe.attackSeconds = 0.006F;
        recipe.releaseSeconds = 0.50F;
        recipe.noiseMix = 0.28F;
        recipe.pulseRate = 5.0F;
        recipe.gain = 0.34F;
        break;
    case AudioEventKind::DoorLock:
        recipe.durationSeconds = 0.12F;
        recipe.baseFrequency = 310.0F;
        recipe.secondaryFrequency = 92.0F;
        recipe.attackSeconds = 0.001F;
        recipe.releaseSeconds = 0.10F;
        recipe.noiseMix = 0.22F;
        recipe.gain = 0.38F;
        break;
    case AudioEventKind::Footstep:
        recipe.durationSeconds = 0.115F;
        recipe.baseFrequency = 145.0F;
        recipe.secondaryFrequency = 58.0F;
        recipe.attackSeconds = 0.001F;
        recipe.releaseSeconds = 0.095F;
        recipe.noiseMix = 0.48F;
        recipe.pulseRate = 13.0F;
        recipe.gain = 0.26F;
        break;
    case AudioEventKind::UiNavigate:
        recipe.durationSeconds = 0.055F;
        recipe.baseFrequency = 780.0F;
        recipe.secondaryFrequency = 1'040.0F;
        recipe.attackSeconds = 0.001F;
        recipe.releaseSeconds = 0.045F;
        recipe.noiseMix = 0.02F;
        recipe.gain = 0.16F;
        break;
    case AudioEventKind::UiConfirm:
        recipe.durationSeconds = 0.12F;
        recipe.baseFrequency = 460.0F;
        recipe.secondaryFrequency = 920.0F;
        recipe.attackSeconds = 0.003F;
        recipe.releaseSeconds = 0.095F;
        recipe.noiseMix = 0.015F;
        recipe.gain = 0.20F;
        break;
    case AudioEventKind::UiBack:
        recipe.durationSeconds = 0.10F;
        recipe.baseFrequency = 620.0F;
        recipe.secondaryFrequency = 280.0F;
        recipe.attackSeconds = 0.002F;
        recipe.releaseSeconds = 0.08F;
        recipe.noiseMix = 0.03F;
        recipe.gain = 0.18F;
        break;
    case AudioEventKind::MeleeSwing:
        recipe.durationSeconds = 0.16F;
        recipe.baseFrequency = 260.0F;
        recipe.secondaryFrequency = 760.0F;
        recipe.attackSeconds = 0.002F;
        recipe.releaseSeconds = 0.13F;
        recipe.noiseMix = 0.36F;
        recipe.gain = 0.30F;
        break;
    case AudioEventKind::Count:
        break;
    }
}

} // namespace

wkc::audio::AudioRecipe generateAudioRecipe(
    const wkc::core::Seed sourceSeed,
    const AudioEventKind kind,
    const std::uint64_t eventCounter)
{
    wkc::audio::AudioRecipe recipe;
    recipe.kind = static_cast<std::uint8_t>(kind);
    recipe.kindCount = static_cast<std::uint8_t>(AudioEventKind::Count);
    const std::uint64_t localId = (static_cast<std::uint64_t>(kind) << 56U)
        ^ eventCounter;
    recipe.seed = wkc::core::deriveSeed(sourceSeed, gdp::game::SeedDomain::Audio, localId);
    configureRecipe(recipe, kind);

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
        wkc::core::RandomStream random{wkc::core::deriveSeed(
            recipe.seed, gdp::game::SeedDomain::Audio, 1U)};
        const float pitchVariation = random.rangeFloat(0.94F, 1.06F);
        recipe.baseFrequency *= pitchVariation;
        recipe.secondaryFrequency *= pitchVariation;
        recipe.durationSeconds *= random.rangeFloat(0.96F, 1.04F);
        recipe.noiseMix = std::clamp(
            recipe.noiseMix + random.rangeFloat(-0.018F, 0.018F), 0.0F, 1.0F);
    }
    recipe.recipeHash = wkc::audio::audioRecipeHash(recipe);
    return recipe;
}

} // namespace gdp::audio
