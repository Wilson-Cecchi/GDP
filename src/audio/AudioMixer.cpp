#include "audio/AudioMixer.hpp"

#include "core/Log.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace gdp::audio {

AudioMixer::~AudioMixer()
{
    shutdown();
}

bool AudioMixer::initialize(const bool enableOutput)
{
    shutdown();
    initialized_ = true;
    if (!enableOutput) {
        return true;
    }

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        gdp::core::log(gdp::core::LogLevel::Warning,
            std::string{"Audio subsystem unavailable; continuing silently: "}
                + SDL_GetError());
        return true;
    }
    audioSubsystemInitialized_ = true;

    const SDL_AudioSpec specification{SDL_AUDIO_F32, 2, proceduralAudioSampleRate};
    stream_ = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &specification, nullptr, nullptr);
    if (stream_ == nullptr) {
        gdp::core::log(gdp::core::LogLevel::Warning,
            std::string{"Audio device unavailable; continuing silently: "}
                + SDL_GetError());
        disableOutput();
        return true;
    }
    if (!SDL_ResumeAudioStreamDevice(stream_)) {
        gdp::core::log(gdp::core::LogLevel::Warning,
            std::string{"Audio device could not start; continuing silently: "}
                + SDL_GetError());
        disableOutput();
        return true;
    }
    outputActive_ = true;
    gdp::core::log(gdp::core::LogLevel::Info,
        "Procedural audio initialized at 48 kHz stereo");
    return true;
}

void AudioMixer::shutdown() noexcept
{
    voices_.clear();
    clips_.clear();
    disableOutput();
    initialized_ = false;
    lastMixPeak_ = 0.0F;
    lastLeftEnergy_ = 0.0F;
    lastRightEnergy_ = 0.0F;
}

void AudioMixer::play(
    const AudioRecipe& recipe)
{
    if (!initialized_) {
        return;
    }
    addVoice(acquireClip(recipe), 1.0F, 1.0F, false, false);
}

void AudioMixer::playAt(
    const AudioRecipe& recipe,
    const gdp::core::Vec3 sourcePosition,
    const gdp::core::Vec3 listenerPosition,
    const gdp::core::Vec3 listenerForward)
{
    if (!initialized_) {
        return;
    }
    const gdp::core::Vec3 offset = sourcePosition - listenerPosition;
    const float distance = gdp::core::length(offset);
    const gdp::core::Vec3 direction = distance > 1.0e-5F
        ? offset * (1.0F / distance) : gdp::core::Vec3{};
    gdp::core::Vec3 flatForward{listenerForward.x, 0.0F, listenerForward.z};
    flatForward = gdp::core::normalized(flatForward);
    if (gdp::core::length(flatForward) <= 1.0e-5F) {
        flatForward = {0.0F, 0.0F, -1.0F};
    }
    const gdp::core::Vec3 right{-flatForward.z, 0.0F, flatForward.x};
    const float pan = std::clamp(gdp::core::dot(direction, right), -1.0F, 1.0F);
    const float attenuation = 1.0F / (1.0F + (0.075F * distance * distance));
    const float left = std::sqrt(0.5F * (1.0F - pan));
    const float rightGain = std::sqrt(0.5F * (1.0F + pan));
    addVoice(acquireClip(recipe),
        attenuation * left,
        attenuation * rightGain,
        false,
        false);
}

void AudioMixer::setAmbience(const AudioRecipe& recipe)
{
    if (!initialized_) {
        return;
    }
    const std::shared_ptr<const AudioClip> clip = acquireClip(recipe);
    if (!clip) {
        return;
    }
    voices_.erase(std::remove_if(voices_.begin(), voices_.end(),
        [&clip](const Voice& voice) {
            return voice.ambient && (!voice.clip
                || voice.clip->recipeHash != clip->recipeHash);
        }), voices_.end());
    const auto existing = std::find_if(voices_.begin(), voices_.end(),
        [&clip](const Voice& voice) {
            return voice.ambient && voice.clip
                && voice.clip->recipeHash == clip->recipeHash;
        });
    if (existing != voices_.end()) {
        existing->leftGain = 1.0F;
        existing->rightGain = 1.0F;
        return;
    }
    addVoice(clip, 1.0F, 1.0F, true, true);
}

void AudioMixer::setAmbienceBlend(
    const AudioRecipe& firstRecipe,
    const AudioRecipe& secondRecipe,
    const float secondWeight)
{
    if (!initialized_ || !std::isfinite(secondWeight)) {
        return;
    }
    const float weight = std::clamp(secondWeight, 0.0F, 1.0F);
    if (weight <= 1.0e-4F) {
        setAmbience(firstRecipe);
        return;
    }
    if (weight >= 1.0F - 1.0e-4F) {
        setAmbience(secondRecipe);
        return;
    }

    if (firstRecipe.recipeHash == secondRecipe.recipeHash) {
        setAmbience(firstRecipe);
        return;
    }
    const std::shared_ptr<const AudioClip> firstClip = acquireClip(firstRecipe);
    const std::shared_ptr<const AudioClip> secondClip = acquireClip(secondRecipe);
    if (!firstClip || !secondClip) {
        return;
    }
    voices_.erase(std::remove_if(voices_.begin(), voices_.end(),
        [&firstClip, &secondClip](const Voice& voice) {
            return voice.ambient && (!voice.clip
                || (voice.clip->recipeHash != firstClip->recipeHash
                    && voice.clip->recipeHash != secondClip->recipeHash));
        }), voices_.end());
    const auto ensureAmbient = [this](
        const std::shared_ptr<const AudioClip>& clip,
        const float gain) {
        const auto existing = std::find_if(voices_.begin(), voices_.end(),
            [&clip](const Voice& voice) {
                return voice.ambient && voice.clip
                    && voice.clip->recipeHash == clip->recipeHash;
            });
        if (existing != voices_.end()) {
            existing->leftGain = gain;
            existing->rightGain = gain;
        } else {
            addVoice(clip, gain, gain, true, true);
        }
    };
    ensureAmbient(firstClip, std::sqrt(1.0F - weight));
    ensureAmbient(secondClip, std::sqrt(weight));
}

void AudioMixer::stopAmbience() noexcept
{
    voices_.erase(std::remove_if(voices_.begin(), voices_.end(),
        [](const Voice& voice) { return voice.ambient; }), voices_.end());
}

void AudioMixer::clearVoices() noexcept
{
    voices_.clear();
}

void AudioMixer::update(const float fixedStepSeconds)
{
    if (!initialized_ || !std::isfinite(fixedStepSeconds) || fixedStepSeconds <= 0.0F) {
        return;
    }
    const int requestedFrames = static_cast<int>(std::lround(
        fixedStepSeconds * static_cast<float>(proceduralAudioSampleRate)));
    const int frameCount = std::clamp(requestedFrames, 1, 4'096);
    std::vector<float> mix(static_cast<std::size_t>(frameCount) * 2U, 0.0F);

    for (Voice& voice : voices_) {
        if (!voice.clip || voice.clip->samples.empty()) {
            continue;
        }
        const float busVolume = voice.ambient ? ambientVolume_ : effectsVolume_;
        for (int frame = 0; frame < frameCount; ++frame) {
            if (voice.cursor >= voice.clip->samples.size()) {
                if (!voice.loop) {
                    break;
                }
                voice.cursor = 0U;
            }
            const float sample = voice.clip->samples[voice.cursor++]
                * busVolume * masterVolume_;
            const std::size_t output = static_cast<std::size_t>(frame) * 2U;
            mix[output] += sample * voice.leftGain;
            mix[output + 1U] += sample * voice.rightGain;
        }
    }
    voices_.erase(std::remove_if(voices_.begin(), voices_.end(),
        [](const Voice& voice) {
            return !voice.loop && (!voice.clip || voice.cursor >= voice.clip->samples.size());
        }), voices_.end());

    float leftEnergy = 0.0F;
    float rightEnergy = 0.0F;
    lastMixPeak_ = 0.0F;
    for (int frame = 0; frame < frameCount; ++frame) {
        const std::size_t output = static_cast<std::size_t>(frame) * 2U;
        mix[output] = std::clamp(mix[output], -1.0F, 1.0F);
        mix[output + 1U] = std::clamp(mix[output + 1U], -1.0F, 1.0F);
        leftEnergy += std::abs(mix[output]);
        rightEnergy += std::abs(mix[output + 1U]);
        lastMixPeak_ = std::max(lastMixPeak_,
            std::max(std::abs(mix[output]), std::abs(mix[output + 1U])));
    }
    lastLeftEnergy_ = leftEnergy / static_cast<float>(frameCount);
    lastRightEnergy_ = rightEnergy / static_cast<float>(frameCount);

    if (outputActive_ && stream_ != nullptr) {
        const int queuedBytes = SDL_GetAudioStreamQueued(stream_);
        constexpr int maximumQueuedBytes = proceduralAudioSampleRate * 2
            * static_cast<int>(sizeof(float)) / 5;
        const std::size_t byteCount = mix.size() * sizeof(float);
        if (queuedBytes >= 0 && queuedBytes < maximumQueuedBytes
            && byteCount <= static_cast<std::size_t>(std::numeric_limits<int>::max())
            && !SDL_PutAudioStreamData(
                stream_, mix.data(), static_cast<int>(byteCount))) {
            gdp::core::log(gdp::core::LogLevel::Warning,
                std::string{"Audio stream failed; continuing silently: "} + SDL_GetError());
            disableOutput();
        }
    }
}

void AudioMixer::setMasterVolume(const float volume) noexcept
{
    masterVolume_ = std::clamp(volume, 0.0F, 1.0F);
}

void AudioMixer::setEffectsVolume(const float volume) noexcept
{
    effectsVolume_ = std::clamp(volume, 0.0F, 1.0F);
}

void AudioMixer::setAmbientVolume(const float volume) noexcept
{
    ambientVolume_ = std::clamp(volume, 0.0F, 1.0F);
}

bool AudioMixer::isInitialized() const noexcept
{
    return initialized_;
}

bool AudioMixer::isOutputActive() const noexcept
{
    return outputActive_;
}

std::size_t AudioMixer::activeVoiceCount() const noexcept
{
    return voices_.size();
}

std::size_t AudioMixer::ambientVoiceCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(
        voices_.begin(), voices_.end(),
        [](const Voice& voice) { return voice.ambient; }));
}

std::size_t AudioMixer::cachedClipCount() const noexcept
{
    return clips_.size();
}

float AudioMixer::lastMixPeak() const noexcept
{
    return lastMixPeak_;
}

float AudioMixer::lastLeftEnergy() const noexcept
{
    return lastLeftEnergy_;
}

float AudioMixer::lastRightEnergy() const noexcept
{
    return lastRightEnergy_;
}

std::shared_ptr<const AudioClip> AudioMixer::acquireClip(const AudioRecipe& recipe)
{
    const auto existing = clips_.find(recipe.recipeHash);
    if (existing != clips_.end()) {
        return existing->second;
    }
    auto clip = std::make_shared<AudioClip>(synthesizeAudioClip(recipe));
    if (!validateAudioClip(*clip, recipe).isValid()) {
        return {};
    }
    clips_.emplace(recipe.recipeHash, clip);
    return clip;
}

void AudioMixer::addVoice(
    std::shared_ptr<const AudioClip> clip,
    const float leftGain,
    const float rightGain,
    const bool loop,
    const bool ambient)
{
    if (!clip || clip->samples.empty()) {
        return;
    }
    if (voices_.size() >= maximumAudioVoices) {
        const auto removable = std::find_if(voices_.begin(), voices_.end(),
            [](const Voice& voice) { return !voice.ambient; });
        if (removable != voices_.end()) {
            voices_.erase(removable);
        } else if (!ambient) {
            return;
        } else {
            voices_.erase(voices_.begin());
        }
    }
    voices_.push_back({
        std::move(clip),
        0U,
        std::clamp(leftGain, 0.0F, 1.0F),
        std::clamp(rightGain, 0.0F, 1.0F),
        loop,
        ambient,
    });
}

void AudioMixer::disableOutput() noexcept
{
    outputActive_ = false;
    if (stream_ != nullptr) {
        SDL_DestroyAudioStream(stream_);
        stream_ = nullptr;
    }
    if (audioSubsystemInitialized_) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        audioSubsystemInitialized_ = false;
    }
}

} // namespace gdp::audio
