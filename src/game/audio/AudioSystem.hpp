#pragma once

#include "audio/AudioMixer.hpp"
#include "game/audio/AudioRecipes.hpp"

#include <cstddef>
#include <cstdint>

namespace gdp::audio {

class AudioSystem {
public:
    AudioSystem() = default;
    ~AudioSystem() = default;

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;
    AudioSystem(AudioSystem&&) = delete;
    AudioSystem& operator=(AudioSystem&&) = delete;

    [[nodiscard]] bool initialize(bool enableOutput);
    void shutdown() noexcept;

    void play(
        AudioEventKind kind,
        gdp::core::Seed sourceSeed,
        std::uint64_t eventCounter = 0U);
    void playAt(
        AudioEventKind kind,
        gdp::core::Seed sourceSeed,
        std::uint64_t eventCounter,
        gdp::core::Vec3 sourcePosition,
        gdp::core::Vec3 listenerPosition,
        gdp::core::Vec3 listenerForward);
    void setAmbience(gdp::core::Seed roomSeed, std::uint64_t profileId);
    void setAmbienceBlend(
        gdp::core::Seed firstRoomSeed,
        std::uint64_t firstProfileId,
        gdp::core::Seed secondRoomSeed,
        std::uint64_t secondProfileId,
        float secondWeight);
    void stopAmbience() noexcept;
    void clearVoices() noexcept;
    void update(float fixedStepSeconds);

    void setMasterVolume(float volume) noexcept;
    void setEffectsVolume(float volume) noexcept;
    void setAmbientVolume(float volume) noexcept;

    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]] bool isOutputActive() const noexcept;
    [[nodiscard]] std::size_t activeVoiceCount() const noexcept;
    [[nodiscard]] std::size_t ambientVoiceCount() const noexcept;
    [[nodiscard]] std::size_t cachedClipCount() const noexcept;
    [[nodiscard]] float lastMixPeak() const noexcept;
    [[nodiscard]] float lastLeftEnergy() const noexcept;
    [[nodiscard]] float lastRightEnergy() const noexcept;

private:
    wkc::audio::AudioMixer mixer_{};
};

} // namespace gdp::audio
