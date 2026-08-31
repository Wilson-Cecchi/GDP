#include "core/Hash.hpp"

namespace wkc::core {

std::uint64_t mix64(std::uint64_t value) noexcept
{
    value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31U);
}

std::uint64_t hashCombine(const std::uint64_t seed, const std::uint64_t value) noexcept
{
    constexpr std::uint64_t goldenRatio = 0x9E3779B97F4A7C15ULL;
    return mix64(seed ^ (value + goldenRatio + (seed << 6U) + (seed >> 2U)));
}

std::uint64_t fnv1a64(const std::string_view text) noexcept
{
    std::uint64_t hash = 0xCBF29CE484222325ULL;
    for (const unsigned char character : text) {
        hash ^= character;
        hash *= 0x100000001B3ULL;
    }
    return hash;
}

} // namespace wkc::core
