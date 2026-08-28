#include "audio/AudioSystem.hpp"
#include "audio/ProceduralAudio.hpp"
#include "core/Seed.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <unordered_set>

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

bool containsError(
    const gdp::audio::AudioValidationResult& result,
    const gdp::audio::AudioValidationError error)
{
    return std::find(result.errors.begin(), result.errors.end(), error)
        != result.errors.end();
}

void testDeterministicRecipeAndClip()
{
    using namespace gdp::audio;
    constexpr gdp::core::Seed seed = 0x415544494F5F5631ULL;
    const AudioRecipe first = generateAudioRecipe(seed, AudioEventKind::WeaponFire, 7U);
    const AudioRecipe repeated = generateAudioRecipe(seed, AudioEventKind::WeaponFire, 7U);
    const AudioClip firstClip = synthesizeAudioClip(first);
    const AudioClip repeatedClip = synthesizeAudioClip(repeated);
    expect(first.recipeHash == repeated.recipeHash && firstClip.samples == repeatedClip.samples,
        "same seed, kind and counter reproduce the complete waveform");
    expect(first.seed == gdp::core::deriveSeed(seed, gdp::core::SeedDomain::Audio,
               (static_cast<std::uint64_t>(AudioEventKind::WeaponFire) << 56U) ^ 7U),
        "audio recipes start from the isolated Audio seed domain");
    expect(validateAudioRecipe(first).isValid()
            && validateAudioClip(firstClip, first).isValid(),
        "representative weapon recipe and clip validate");
    constexpr std::uint64_t goldenHash = 0xEAD1C1A48BF9FF7FULL;
    expect(first.recipeHash == goldenHash,
        "representative audio recipe keeps its golden hash");
}

void testCueCoverageAndClipping()
{
    using namespace gdp::audio;
    std::array<bool, static_cast<std::size_t>(AudioEventKind::Count)> covered{};
    for (std::size_t index = 0; index < covered.size(); ++index) {
        const AudioEventKind kind = static_cast<AudioEventKind>(index);
        const AudioRecipe recipe = generateAudioRecipe(1000U + index, kind, index);
        const AudioClip clip = synthesizeAudioClip(recipe);
        expect(validateAudioRecipe(recipe).isValid(),
            "every audio event kind produces a valid recipe");
        expect(validateAudioClip(clip, recipe).isValid() && !clip.samples.empty(),
            "every audio event kind produces a finite unclipped clip");
        covered[index] = !clip.samples.empty();
    }
    expect(std::all_of(covered.begin(), covered.end(), [](const bool value) { return value; }),
        "synthesis covers all functional and ambient cue categories");

    std::unordered_set<std::uint32_t> ambientFrequencies;
    for (std::uint64_t profile = 0; profile < 7U; ++profile) {
        const AudioRecipe ambience = generateAudioRecipe(
            12345U, AudioEventKind::Ambient, profile);
        ambientFrequencies.insert(static_cast<std::uint32_t>(
            std::lround(ambience.baseFrequency * 1'000.0F)));
    }
    expect(ambientFrequencies.size() == 7U,
        "all seven room-theme profiles receive distinct ambient foundations");

    std::unordered_set<std::uint64_t> hashes;
    for (std::uint64_t index = 0; index < 1'000U; ++index) {
        const AudioEventKind kind = static_cast<AudioEventKind>(
            index % static_cast<std::uint64_t>(AudioEventKind::Count));
        hashes.insert(generateAudioRecipe(12345U, kind, index).recipeHash);
    }
    expect(hashes.size() == 1'000U,
        "1,000 event counters produce 1,000 distinct deterministic recipes");
}

void testValidationFailures()
{
    using namespace gdp::audio;
    AudioRecipe invalid = generateAudioRecipe(77U, AudioEventKind::Pickup, 2U);
    invalid.durationSeconds = 0.0F;
    invalid.noiseMix = 2.0F;
    const AudioValidationResult result = validateAudioRecipe(invalid);
    expect(containsError(result, AudioValidationError::InvalidDuration),
        "validator rejects invalid cue duration");
    expect(containsError(result, AudioValidationError::InvalidMix),
        "validator rejects invalid noise mix");
    expect(containsError(result, AudioValidationError::InvalidHash),
        "validator detects mutated audio recipes");
}

void testSilentMixerAndSpatialization()
{
    using namespace gdp::audio;
    AudioSystem audio;
    expect(audio.initialize(false) && audio.isInitialized() && !audio.isOutputActive(),
        "audio system initializes silently without opening a device");

    audio.playAt(AudioEventKind::EnemyHit, 91U, 0U,
        {3.0F, 0.0F, 0.0F}, {}, {0.0F, 0.0F, -1.0F});
    audio.update(1.0F / 60.0F);
    expect(audio.lastRightEnergy() > audio.lastLeftEnergy() * 4.0F,
        "sound to the listener right is panned to the right channel");

    audio.clearVoices();
    audio.playAt(AudioEventKind::EnemyHit, 91U, 1U,
        {-3.0F, 0.0F, 0.0F}, {}, {0.0F, 0.0F, -1.0F});
    audio.update(1.0F / 60.0F);
    expect(audio.lastLeftEnergy() > audio.lastRightEnergy() * 4.0F,
        "sound to the listener left is panned to the left channel");

    audio.clearVoices();
    audio.playAt(AudioEventKind::PropImpact, 92U, 0U,
        {0.0F, 0.0F, -1.0F}, {}, {0.0F, 0.0F, -1.0F});
    audio.update(1.0F / 120.0F);
    const float nearPeak = audio.lastMixPeak();
    audio.clearVoices();
    audio.playAt(AudioEventKind::PropImpact, 92U, 0U,
        {0.0F, 0.0F, -20.0F}, {}, {0.0F, 0.0F, -1.0F});
    audio.update(1.0F / 120.0F);
    expect(nearPeak > audio.lastMixPeak() * 8.0F,
        "distance attenuation makes far impacts substantially quieter");

    audio.clearVoices();
    for (std::uint64_t index = 0; index < 40U; ++index) {
        audio.play(AudioEventKind::WeaponFire, 100U, index);
    }
    audio.update(1.0F / 60.0F);
    expect(audio.activeVoiceCount() == maximumAudioVoices
            && audio.lastMixPeak() <= 1.0F,
        "voice budget is enforced and the final stereo mix cannot clip");

    audio.clearVoices();
    audio.setAmbience(12345U, 2U);
    audio.update(2.2F);
    expect(audio.activeVoiceCount() == 1U && audio.ambientVoiceCount() == 1U
            && audio.lastMixPeak() > 0.0F,
        "ambient voice loops in silent/headless mixer mode");
    audio.setAmbienceBlend(12345U, 2U, 67890U, 4U, 0.5F);
    audio.update(1.0F / 60.0F);
    expect(audio.activeVoiceCount() == 2U && audio.ambientVoiceCount() == 2U
            && audio.lastMixPeak() > 0.0F,
        "corridor crossfade mixes two cached ambient loops without silence");
    audio.setAmbienceBlend(12345U, 2U, 67890U, 4U, 0.75F);
    expect(audio.ambientVoiceCount() == 2U,
        "updating a crossfade reuses both ambient voices instead of restarting them");
    audio.setAmbience(67890U, 4U);
    expect(audio.activeVoiceCount() == 1U && audio.ambientVoiceCount() == 1U,
        "finishing a corridor blend preserves only the destination ambience");
    audio.stopAmbience();
    expect(audio.activeVoiceCount() == 0U,
        "ambient loop can be replaced or stopped without stale voices");

    audio.play(AudioEventKind::DryFire, 55U, 1U);
    const std::size_t cached = audio.cachedClipCount();
    audio.play(AudioEventKind::DryFire, 55U, 1U);
    expect(audio.cachedClipCount() == cached,
        "identical cues reuse their synthesized clip cache");
    audio.update(0.2F);
    expect(audio.activeVoiceCount() == 0U,
        "finished one-shot voices leave the mixer automatically");
}

} // namespace

int main()
{
    testDeterministicRecipeAndClip();
    testCueCoverageAndClipping();
    testValidationFailures();
    testSilentMixerAndSpatialization();

    if (failures != 0) {
        std::cerr << failures << " audio test(s) failed\n";
        return 1;
    }
    std::cout << "Procedural audio and silent mixer tests passed\n";
    return 0;
}
