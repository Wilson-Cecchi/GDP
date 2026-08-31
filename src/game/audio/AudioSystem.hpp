#pragma once

#include "core/Math.hpp"
#include "game/audio/AudioRecipes.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

struct SDL_AudioStream;

namespace gdp::audio {

inline constexpr std::size_t maximumAudioVoices = 16U;

class AudioSystem {
public:
    AudioSystem() = default;
    ~AudioSystem();

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
    struct Voice {
        std::shared_ptr<const AudioClip> clip;
        std::size_t cursor{0};
        float leftGain{1.0F};
        float rightGain{1.0F};
        bool loop{false};
        bool ambient{false};
    };

    [[nodiscard]] std::shared_ptr<const AudioClip> acquireClip(
        const AudioRecipe& recipe);
    void addVoice(
        std::shared_ptr<const AudioClip> clip,
        float leftGain,
        float rightGain,
        bool loop,
        bool ambient);
    void disableOutput() noexcept;

    std::unordered_map<std::uint64_t, std::shared_ptr<const AudioClip>> clips_;
    std::vector<Voice> voices_;
    SDL_AudioStream* stream_{nullptr};
    bool initialized_{false};
    bool audioSubsystemInitialized_{false};
    bool outputActive_{false};
    float masterVolume_{0.85F};
    float effectsVolume_{0.90F};
    float ambientVolume_{0.55F};
    float lastMixPeak_{0.0F};
    float lastLeftEnergy_{0.0F};
    float lastRightEnergy_{0.0F};
};

} // namespace gdp::audio
