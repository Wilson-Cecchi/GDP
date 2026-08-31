#pragma once

#include <cstdint>

namespace wkc::core {

class FixedStepClock {
public:
    explicit FixedStepClock(
        double fixedStepSeconds = 1.0 / 60.0,
        double maximumFrameDeltaSeconds = 0.25) noexcept;

    void reset() noexcept;
    void advanceBy(double elapsedSeconds) noexcept;
    [[nodiscard]] bool consumeFixedStep() noexcept;

    [[nodiscard]] double fixedStepSeconds() const noexcept;
    [[nodiscard]] double interpolationAlpha() const noexcept;
    [[nodiscard]] std::uint64_t totalSteps() const noexcept;

private:
    double fixedStepSeconds_;
    double maximumFrameDeltaSeconds_;
    double accumulatorSeconds_{0.0};
    std::uint64_t totalSteps_{0};
};

} // namespace wkc::core
