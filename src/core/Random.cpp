#include "core/Random.hpp"

#include "core/Hash.hpp"

#include <algorithm>
#include <cstdint>

namespace wkc::core {

RandomStream::RandomStream(const Seed seed) noexcept
    : state_(seed)
{
}

std::uint64_t RandomStream::nextU64() noexcept
{
    state_ += 0x9E3779B97F4A7C15ULL;
    return wkc::core::mix64(state_);
}

std::uint32_t RandomStream::nextU32() noexcept
{
    return static_cast<std::uint32_t>(nextU64() >> 32U);
}

float RandomStream::uniform01() noexcept
{
    constexpr float inverseRange = 1.0F / 16'777'216.0F;
    return static_cast<float>(nextU32() >> 8U) * inverseRange;
}

int RandomStream::rangeInt(int minimum, int maximum) noexcept
{
    if (minimum > maximum) {
        std::swap(minimum, maximum);
    }

    const auto signedSpan = static_cast<std::int64_t>(maximum)
        - static_cast<std::int64_t>(minimum) + 1;
    const auto span = static_cast<std::uint64_t>(signedSpan);
    const auto rejectionThreshold = (0ULL - span) % span;

    std::uint64_t sample = 0;
    do {
        sample = nextU64();
    } while (sample < rejectionThreshold);

    const auto offset = static_cast<std::int64_t>(sample % span);
    return static_cast<int>(static_cast<std::int64_t>(minimum) + offset);
}

float RandomStream::rangeFloat(float minimum, float maximum) noexcept
{
    if (minimum > maximum) {
        std::swap(minimum, maximum);
    }
    return minimum + ((maximum - minimum) * uniform01());
}

bool RandomStream::chance(const float probability) noexcept
{
    if (probability <= 0.0F) {
        return false;
    }
    if (probability >= 1.0F) {
        return true;
    }
    return uniform01() < probability;
}

} // namespace wkc::core
