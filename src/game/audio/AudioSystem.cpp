#include "game/audio/AudioSystem.hpp"

namespace gdp::audio {

bool AudioSystem::initialize(const bool enableOutput)
{
    return mixer_.initialize(enableOutput);
}

void AudioSystem::shutdown() noexcept
{
    mixer_.shutdown();
}

void AudioSystem::play(
    const AudioEventKind kind,
    const gdp::core::Seed sourceSeed,
    const std::uint64_t eventCounter)
{
    if (kind >= AudioEventKind::Count || kind == AudioEventKind::Ambient) {
        return;
    }
    mixer_.play(generateAudioRecipe(sourceSeed, kind, eventCounter));
}

void AudioSystem::playAt(
    const AudioEventKind kind,
    const gdp::core::Seed sourceSeed,
    const std::uint64_t eventCounter,
    const gdp::core::Vec3 sourcePosition,
    const gdp::core::Vec3 listenerPosition,
    const gdp::core::Vec3 listenerForward)
{
    if (kind >= AudioEventKind::Count || kind == AudioEventKind::Ambient) {
        return;
    }
    mixer_.playAt(generateAudioRecipe(sourceSeed, kind, eventCounter),
        sourcePosition, listenerPosition, listenerForward);
}

void AudioSystem::setAmbience(
    const gdp::core::Seed roomSeed,
    const std::uint64_t profileId)
{
    mixer_.setAmbience(generateAudioRecipe(
        roomSeed, AudioEventKind::Ambient, profileId));
}

void AudioSystem::setAmbienceBlend(
    const gdp::core::Seed firstRoomSeed,
    const std::uint64_t firstProfileId,
    const gdp::core::Seed secondRoomSeed,
    const std::uint64_t secondProfileId,
    const float secondWeight)
{
    mixer_.setAmbienceBlend(
        generateAudioRecipe(firstRoomSeed, AudioEventKind::Ambient, firstProfileId),
        generateAudioRecipe(secondRoomSeed, AudioEventKind::Ambient, secondProfileId),
        secondWeight);
}

void AudioSystem::stopAmbience() noexcept
{
    mixer_.stopAmbience();
}

void AudioSystem::clearVoices() noexcept
{
    mixer_.clearVoices();
}

void AudioSystem::update(const float fixedStepSeconds)
{
    mixer_.update(fixedStepSeconds);
}

void AudioSystem::setMasterVolume(const float volume) noexcept
{
    mixer_.setMasterVolume(volume);
}

void AudioSystem::setEffectsVolume(const float volume) noexcept
{
    mixer_.setEffectsVolume(volume);
}

void AudioSystem::setAmbientVolume(const float volume) noexcept
{
    mixer_.setAmbientVolume(volume);
}

bool AudioSystem::isInitialized() const noexcept
{
    return mixer_.isInitialized();
}

bool AudioSystem::isOutputActive() const noexcept
{
    return mixer_.isOutputActive();
}

std::size_t AudioSystem::activeVoiceCount() const noexcept
{
    return mixer_.activeVoiceCount();
}

std::size_t AudioSystem::ambientVoiceCount() const noexcept
{
    return mixer_.ambientVoiceCount();
}

std::size_t AudioSystem::cachedClipCount() const noexcept
{
    return mixer_.cachedClipCount();
}

float AudioSystem::lastMixPeak() const noexcept
{
    return mixer_.lastMixPeak();
}

float AudioSystem::lastLeftEnergy() const noexcept
{
    return mixer_.lastLeftEnergy();
}

float AudioSystem::lastRightEnergy() const noexcept
{
    return mixer_.lastRightEnergy();
}

} // namespace gdp::audio
