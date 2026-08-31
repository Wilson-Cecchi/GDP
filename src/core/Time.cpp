#include "core/Time.hpp"

#include <algorithm>
#include <cmath>

namespace wkc::core {

FixedStepClock::FixedStepClock(
    const double fixedStepSeconds,
    const double maximumFrameDeltaSeconds) noexcept
    : fixedStepSeconds_(fixedStepSeconds > 0.0 ? fixedStepSeconds : 1.0 / 60.0)
    , maximumFrameDeltaSeconds_(maximumFrameDeltaSeconds > 0.0
              ? maximumFrameDeltaSeconds
              : 0.25)
{
}

void FixedStepClock::reset() noexcept
{
    accumulatorSeconds_ = 0.0;
    totalSteps_ = 0;
}

void FixedStepClock::advanceBy(const double elapsedSeconds) noexcept
{
    accumulatorSeconds_ += std::clamp(elapsedSeconds, 0.0, maximumFrameDeltaSeconds_);
}

bool FixedStepClock::consumeFixedStep() noexcept
{
    constexpr double toleranceScale = 1.0e-10;
    const double tolerance = fixedStepSeconds_ * toleranceScale;
    if (accumulatorSeconds_ + tolerance < fixedStepSeconds_) {
        return false;
    }

    accumulatorSeconds_ -= fixedStepSeconds_;
    if (std::abs(accumulatorSeconds_) <= tolerance) {
        accumulatorSeconds_ = 0.0;
    }
    ++totalSteps_;
    return true;
}

double FixedStepClock::fixedStepSeconds() const noexcept
{
    return fixedStepSeconds_;
}

double FixedStepClock::interpolationAlpha() const noexcept
{
    return std::clamp(accumulatorSeconds_ / fixedStepSeconds_, 0.0, 1.0);
}

std::uint64_t FixedStepClock::totalSteps() const noexcept
{
    return totalSteps_;
}

} // namespace wkc::core
