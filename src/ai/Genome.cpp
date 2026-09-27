#include "ai/Genome.hpp"

#include "core/Random.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace wkc::ai {

void mutate(
    Genome& genome,
    const wkc::core::Seed seed,
    const float probability,
    const float strength)
{
    if (!std::isfinite(probability) || probability < 0.0F || probability > 1.0F
        || !std::isfinite(strength) || strength < 0.0F || strength > 1.0F) {
        throw std::invalid_argument("mutation probability and strength must be in [0, 1]");
    }
    wkc::core::RandomStream random{seed};
    for (float& parameter : genome.network.parameters()) {
        if (random.chance(probability)) {
            parameter = std::clamp(
                parameter + random.rangeFloat(-strength, strength), -1.0F, 1.0F);
        }
    }
    genome.fitness = 0.0F;
}

Genome crossover(
    const Genome& first,
    const Genome& second,
    const wkc::core::Seed seed,
    const std::uint32_t childId)
{
    if (first.network.inputCount() != second.network.inputCount()
        || first.network.hiddenCount() != second.network.hiddenCount()
        || first.network.outputCount() != second.network.outputCount()) {
        throw std::invalid_argument("crossover requires matching network dimensions");
    }

    Genome child{first.network, 0.0F,
        std::max(first.generation, second.generation) + 1, childId};
    wkc::core::RandomStream random{seed};
    const auto secondParameters = second.network.parameters();
    auto childParameters = child.network.parameters();
    for (std::size_t index = 0; index < childParameters.size(); ++index) {
        if (random.chance(0.5F)) {
            childParameters[index] = secondParameters[index];
        }
    }
    return child;
}

} // namespace wkc::ai
