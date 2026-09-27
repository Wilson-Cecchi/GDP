#include "ai/NeuralNetwork.hpp"

#include "core/Random.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace wkc::ai {

NeuralNetwork::NeuralNetwork(
    const std::size_t inputs,
    const std::size_t hidden,
    const std::size_t outputs)
    : inputs_(inputs), hidden_(hidden), outputs_(outputs)
{
    // The initial use case is a small enemy brain, not an arbitrary tensor API.
    if (inputs == 0 || hidden == 0 || outputs == 0
        || inputs > 128 || hidden > 128 || outputs > 128) {
        throw std::invalid_argument("neural network dimensions must be in [1, 128]");
    }
    parameters_.resize(hidden * (inputs + 1) + outputs * (hidden + 1));
}

void NeuralNetwork::randomize(const wkc::core::Seed seed, const float magnitude)
{
    if (!std::isfinite(magnitude) || magnitude < 0.0F || magnitude > 1.0F) {
        throw std::invalid_argument("random weight magnitude must be in [0, 1]");
    }
    wkc::core::RandomStream random{seed};
    for (float& parameter : parameters_) {
        parameter = random.rangeFloat(-magnitude, magnitude);
    }
}

std::vector<float> NeuralNetwork::forward(const std::span<const float> inputs) const
{
    if (inputs.size() != inputs_) {
        throw std::invalid_argument("neural network input count mismatch");
    }
    if (!std::all_of(inputs.begin(), inputs.end(), [](const float value) {
            return std::isfinite(value) && value >= -1.0F && value <= 1.0F;
        })) {
        throw std::invalid_argument("neural network inputs must be finite and normalized");
    }

    std::vector<float> hiddenValues(hidden_);
    std::size_t offset = 0;
    for (float& value : hiddenValues) {
        float sum = parameters_[offset + inputs_];
        for (std::size_t input = 0; input < inputs_; ++input) {
            sum += inputs[input] * parameters_[offset + input];
        }
        value = std::tanh(sum);
        offset += inputs_ + 1;
    }

    std::vector<float> result(outputs_);
    for (float& value : result) {
        float sum = parameters_[offset + hidden_];
        for (std::size_t neuron = 0; neuron < hidden_; ++neuron) {
            sum += hiddenValues[neuron] * parameters_[offset + neuron];
        }
        value = std::tanh(sum);
        offset += hidden_ + 1;
    }
    return result;
}

} // namespace wkc::ai
