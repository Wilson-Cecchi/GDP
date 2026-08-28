#pragma once

#include <cstdint>

namespace gdp::gameplay {

inline constexpr float damageFeedbackSeconds = 0.28F;
inline constexpr float hitFeedbackSeconds = 0.16F;
inline constexpr float noticeFeedbackSeconds = 1.80F;

enum class HitFeedbackKind : std::uint8_t {
    None,
    Hit,
    Kill,
};

enum class NoticeFeedbackKind : std::uint8_t {
    None,
    Pickup,
    EventStart,
    Wave,
    EventClear,
    LevelUp,
};

struct FeedbackState {
    float damageRemaining{0.0F};
    float hitRemaining{0.0F};
    HitFeedbackKind hit{HitFeedbackKind::None};
    float noticeRemaining{0.0F};
    NoticeFeedbackKind notice{NoticeFeedbackKind::None};

    constexpr bool operator==(const FeedbackState&) const noexcept = default;
};

void updateFeedback(FeedbackState& state, float fixedStepSeconds) noexcept;
void signalPlayerDamage(FeedbackState& state) noexcept;
void signalHit(FeedbackState& state, bool killed) noexcept;
void signalNotice(FeedbackState& state, NoticeFeedbackKind notice) noexcept;
void clearFeedback(FeedbackState& state) noexcept;

[[nodiscard]] float damageFeedbackStrength(const FeedbackState& state) noexcept;
[[nodiscard]] float hitFeedbackStrength(const FeedbackState& state) noexcept;
[[nodiscard]] float noticeFeedbackStrength(const FeedbackState& state) noexcept;

} // namespace gdp::gameplay
