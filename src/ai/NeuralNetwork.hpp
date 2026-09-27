#pragma once

#include "core/Seed.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace wkc::ai {

// A single-hidden-layer, fully connected network. Parameters are laid out as
// hidden neurons (input weights, bias), then output neurons (hidden weights,
// bias). Inputs and outputs are normalized to [-1, 1].
class NeuralNetwork {
public:
    NeuralNetwork(std::size_t inputs, std::size_t hidden, std::size_t outputs);

    [[nodiscard]] std::size_t inputCount() const noexcept { return inputs_; }
    [[nodiscard]] std::size_t hiddenCount() const noexcept { return hidden_; }
    [[nodiscard]] std::size_t outputCount() const noexcept { return outputs_; }
    [[nodiscard]] std::span<const float> parameters() const noexcept { return parameters_; }
    [[nodiscard]] std::span<float> parameters() noexcept { return parameters_; }

    // Randomization never consumes the game's structural random stream.
    void randomize(wkc::core::Seed seed, float magnitude = 1.0F);
    [[nodiscard]] std::vector<float> forward(std::span<const float> inputs) const;

private:
    std::size_t inputs_;
    std::size_t hidden_;
    std::size_t outputs_;
    std::vector<float> parameters_;
};

} // namespace wkc::ai
