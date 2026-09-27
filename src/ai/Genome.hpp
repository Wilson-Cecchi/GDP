#pragma once

#include "ai/NeuralNetwork.hpp"

#include <cstdint>

namespace wkc::ai {

// Value-owned brain parameters and evolutionary bookkeeping. Copying this
// type clones the full network; no live enemy/gameplay state is stored here.
struct Genome {
    NeuralNetwork network;
    float fitness{0.0F};
    std::uint32_t generation{0};
    std::uint32_t id{0};
};

// Mutates selected parameters by a uniform delta, clamped to [-1, 1].
// The caller supplies an independent seed for each evolutionary operation.
void mutate(Genome& genome, wkc::core::Seed seed, float probability, float strength);

// Each child parameter is chosen independently from either compatible parent.
[[nodiscard]] Genome crossover(
    const Genome& first,
    const Genome& second,
    wkc::core::Seed seed,
    std::uint32_t childId);

} // namespace wkc::ai
