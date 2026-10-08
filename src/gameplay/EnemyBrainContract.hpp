#pragma once

#include <cstddef>
#include <cstdint>

namespace gdp::gameplay {

enum class EnemyBrainContract : std::uint8_t {
    Legacy = 1,
    Tactical = 2,
    Cooperative = 3,
};

struct EnemyBrainShape {
    std::size_t inputs;
    std::size_t hidden;
    std::size_t outputs;
};

[[nodiscard]] constexpr EnemyBrainShape enemyBrainShape(
    const EnemyBrainContract contract) noexcept
{
    switch (contract) {
    case EnemyBrainContract::Legacy: return {7U, 8U, 4U};
    case EnemyBrainContract::Tactical: return {12U, 12U, 5U};
    case EnemyBrainContract::Cooperative: return {19U, 12U, 5U};
    }
    return {};
}

} // namespace gdp::gameplay
