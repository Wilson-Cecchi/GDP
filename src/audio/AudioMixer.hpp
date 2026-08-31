#pragma once

#include "audio/AudioFoundation.hpp"
#include "core/Math.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

struct SDL_AudioStream;

namespace wkc::audio {

inline constexpr std::size_t maximumAudioVoices = 16U;

class AudioMixer {
public:
    AudioMixer() = default;
    ~AudioMixer();

    AudioMixer(const AudioMixer&) = delete;
    AudioMixer& operator=(const AudioMixer&) = delete;
    AudioMixer(AudioMixer&&) = delete;
    AudioMixer& operator=(AudioMixer&&) = delete;

    [[nodiscard]] bool initialize(bool enableOutput);
    void shutdown() noexcept;

    void play(const AudioRecipe& recipe);
    void playAt(
        const AudioRecipe& recipe,
        gdp::core::Vec3 sourcePosition,
        gdp::core::Vec3 listenerPosition,
        gdp::core::Vec3 listenerForward);
    void setAmbience(const AudioRecipe& recipe);
    void setAmbienceBlend(
        const AudioRecipe& firstRecipe,
        const AudioRecipe& secondRecipe,
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

} // namespace wkc::audio
