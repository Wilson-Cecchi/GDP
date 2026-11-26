#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>

namespace gdp::game {

// One source/row/direction at a time. The caller supplies the current physical
// held state, so focus loss, pointer-up, page changes and selection changes stop
// repetition without relying on platform key repeat events.
class MenuAdjustmentRepeat {
public:
    using Clock = std::chrono::steady_clock;

    void reset() noexcept { active_ = false; }

    [[nodiscard]] int advance(const std::uint64_t token,
        const Clock::time_point now, const bool pressed,
        const bool held) noexcept
    {
        if (!held) { reset(); return 0; }
        if (pressed) {
            active_ = true;
            token_ = token;
            next_ = now + std::chrono::milliseconds(320);
            return 1;
        }
        if (!active_ || token_ != token || now < next_) {
            if (active_ && token_ != token) reset();
            return 0;
        }
        const auto overdue = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - next_).count();
        const int steps = static_cast<int>(std::min<std::int64_t>(
            64, 1 + overdue / 55));
        next_ += std::chrono::milliseconds(55 * steps);
        return steps;
    }

private:
    bool active_{false};
    std::uint64_t token_{0U};
    Clock::time_point next_{};
};

} // namespace gdp::game
