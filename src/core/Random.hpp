#pragma once

#include "core/Seed.hpp"

#include <cstdint>

namespace wkc::core {

class RandomStream {
public:
    explicit RandomStream(Seed seed) noexcept;

    [[nodiscard]] std::uint32_t nextU32() noexcept;
    [[nodiscard]] std::uint64_t nextU64() noexcept;
    [[nodiscard]] float uniform01() noexcept;
    [[nodiscard]] int rangeInt(int minimum, int maximum) noexcept;
    [[nodiscard]] float rangeFloat(float minimum, float maximum) noexcept;
    [[nodiscard]] bool chance(float probability) noexcept;

private:
    std::uint64_t state_;
};

} // namespace wkc::core
