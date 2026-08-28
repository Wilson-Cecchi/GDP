#include "core/Math.hpp"
#include "gameplay/Feedback.hpp"

#include <iostream>
#include <limits>
#include <string_view>

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void testDamageAndHitTimers()
{
    using namespace gdp::gameplay;
    FeedbackState feedback;
    signalPlayerDamage(feedback);
    signalHit(feedback, false);
    expect(damageFeedbackStrength(feedback) == 1.0F
            && hitFeedbackStrength(feedback) == 1.0F
            && feedback.hit == HitFeedbackKind::Hit,
        "damage and hit signals start at full deterministic strength");

    updateFeedback(feedback, hitFeedbackSeconds * 0.5F);
    expect(gdp::core::nearlyEqual(hitFeedbackStrength(feedback), 0.5F)
            && feedback.hit == HitFeedbackKind::Hit,
        "hit feedback fades proportionally on the fixed timeline");
    signalHit(feedback, true);
    expect(feedback.hit == HitFeedbackKind::Kill
            && hitFeedbackStrength(feedback) == 1.0F,
        "kill confirmation replaces and refreshes an existing hit marker");

    updateFeedback(feedback, 10.0F);
    expect(feedback.hit == HitFeedbackKind::None
            && hitFeedbackStrength(feedback) == 0.0F
            && damageFeedbackStrength(feedback) == 0.0F,
        "expired combat feedback clears its semantic state");
}

void testNoticesAndInvalidSteps()
{
    using namespace gdp::gameplay;
    FeedbackState feedback;
    signalNotice(feedback, NoticeFeedbackKind::Pickup);
    updateFeedback(feedback, -1.0F);
    updateFeedback(feedback, std::numeric_limits<float>::quiet_NaN());
    expect(feedback.notice == NoticeFeedbackKind::Pickup
            && noticeFeedbackStrength(feedback) == 1.0F,
        "invalid time steps cannot consume a transient notice");

    signalNotice(feedback, NoticeFeedbackKind::EventClear);
    expect(feedback.notice == NoticeFeedbackKind::EventClear,
        "new gameplay notice atomically replaces the previous message");
    updateFeedback(feedback, noticeFeedbackSeconds);
    expect(feedback.notice == NoticeFeedbackKind::None
            && noticeFeedbackStrength(feedback) == 0.0F,
        "notice expires exactly at its documented duration");

    signalNotice(feedback, NoticeFeedbackKind::LevelUp);
    clearFeedback(feedback);
    expect(feedback == FeedbackState{},
        "explicit clear resets all feedback when a run restarts");
}

} // namespace

int main()
{
    testDamageAndHitTimers();
    testNoticesAndInvalidSteps();
    if (failures != 0) {
        std::cerr << failures << " feedback test(s) failed\n";
        return 1;
    }
    std::cout << "All GDP HUD feedback tests passed\n";
    return 0;
}
