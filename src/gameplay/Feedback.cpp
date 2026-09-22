#include "gameplay/Feedback.hpp"

#include <algorithm>
#include <cmath>

namespace gdp::gameplay {
namespace {

float advanceTimer(const float remaining, const float fixedStepSeconds) noexcept
{
    if (!std::isfinite(remaining) || remaining <= 0.0F) {
        return 0.0F;
    }
    if (!std::isfinite(fixedStepSeconds) || fixedStepSeconds <= 0.0F) {
        return remaining;
    }
    return std::max(0.0F, remaining - fixedStepSeconds);
}

float strength(const float remaining, const float duration) noexcept
{
    if (!std::isfinite(remaining) || !std::isfinite(duration) || duration <= 0.0F) {
        return 0.0F;
    }
    return std::clamp(remaining / duration, 0.0F, 1.0F);
}

} // namespace

void updateFeedback(FeedbackState& state, const float fixedStepSeconds) noexcept
{
    state.damageRemaining = advanceTimer(state.damageRemaining, fixedStepSeconds);
    state.hitRemaining = advanceTimer(state.hitRemaining, fixedStepSeconds);
    if (state.hitRemaining <= 0.0F) {
        state.hit = HitFeedbackKind::None;
    }
    state.noticeRemaining = advanceTimer(state.noticeRemaining, fixedStepSeconds);
    if (state.noticeRemaining <= 0.0F) {
        state.notice = NoticeFeedbackKind::None;
    }
}

void signalPlayerDamage(FeedbackState& state) noexcept
{
    state.damageRemaining = damageFeedbackSeconds;
}

void signalHit(
    FeedbackState& state,
    const bool killed,
    const bool weakPoint) noexcept
{
    state.hitRemaining = hitFeedbackSeconds;
    state.hit = weakPoint
        ? killed ? HitFeedbackKind::WeakKill : HitFeedbackKind::WeakHit
        : killed ? HitFeedbackKind::Kill : HitFeedbackKind::Hit;
}

void signalNotice(FeedbackState& state, const NoticeFeedbackKind notice) noexcept
{
    if (notice == NoticeFeedbackKind::None) {
        return;
    }
    state.noticeRemaining = noticeFeedbackSeconds;
    state.notice = notice;
}

void clearFeedback(FeedbackState& state) noexcept
{
    state = {};
}

float damageFeedbackStrength(const FeedbackState& state) noexcept
{
    return strength(state.damageRemaining, damageFeedbackSeconds);
}

float hitFeedbackStrength(const FeedbackState& state) noexcept
{
    return strength(state.hitRemaining, hitFeedbackSeconds);
}

float noticeFeedbackStrength(const FeedbackState& state) noexcept
{
    return strength(state.noticeRemaining, noticeFeedbackSeconds);
}

} // namespace gdp::gameplay
