#include "ai/Genome.hpp"
#include "ai/NeuralNetwork.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

template<typename Callback>
void expectInvalid(Callback callback, const std::string_view message)
{
    try {
        callback();
        expect(false, message);
    } catch (const std::invalid_argument&) {
    }
}

void testNetwork()
{
    using wkc::ai::NeuralNetwork;
    NeuralNetwork first{3, 4, 2};
    expect(first.parameters().size() == 26, "network includes weights and biases");
    first.randomize(42);
    NeuralNetwork repeated{3, 4, 2};
    repeated.randomize(42);
    expect(std::ranges::equal(first.parameters(), repeated.parameters()),
        "same seed gives identical parameters");
    repeated.randomize(43);
    expect(!std::ranges::equal(first.parameters(), repeated.parameters()),
        "different seed changes parameters");
    expect(std::ranges::all_of(first.parameters(), [](const float value) {
        return value >= -1.0F && value <= 1.0F;
    }), "random weights stay bounded");

    const std::array<float, 3> inputs{0.25F, -0.75F, 1.0F};
    const auto output = first.forward(inputs);
    expect(output == first.forward(inputs), "forward pass is deterministic");
    expect(output.size() == 2, "forward pass returns all outputs");
    expect(std::ranges::all_of(output, [](const float value) {
        return std::isfinite(value) && value >= -1.0F && value <= 1.0F;
    }), "tanh outputs stay normalized");

    NeuralNetwork handSet{2, 1, 1};
    const std::array<float, 5> knownWeights{2.0F, -1.0F, 0.5F, 1.5F, -0.25F};
    std::ranges::copy(knownWeights, handSet.parameters().begin());
    const auto knownOutput = handSet.forward(std::array<float, 2>{1.0F, 0.0F});
    const float expected = std::tanh(1.5F * std::tanh(2.5F) - 0.25F);
    expect(std::abs(knownOutput[0] - expected) < 0.00001F,
        "forward pass applies weights and biases in documented order");

    NeuralNetwork clone = first;
    expect(clone.forward(inputs) == output, "clone produces the same output");
    clone.parameters()[0] += 0.25F;
    expect(clone.parameters()[0] != first.parameters()[0], "clone owns independent weights");

    expectInvalid([] { NeuralNetwork invalid{0, 2, 1}; }, "zero dimension is rejected");
    expectInvalid([] { NeuralNetwork invalid{129, 2, 1}; }, "oversized dimension is rejected");
    expectInvalid([&] { static_cast<void>(first.forward(std::array<float, 2>{0.0F, 0.0F})); },
        "wrong input size is rejected");
    expectInvalid([&] { static_cast<void>(first.forward(std::array<float, 3>{
        0.0F, std::numeric_limits<float>::quiet_NaN(), 0.0F})); },
        "non-finite input is rejected");
    expectInvalid([&] { static_cast<void>(first.forward(std::array<float, 3>{
        0.0F, 1.1F, 0.0F})); },
        "non-normalized input is rejected");
    expectInvalid([&] { first.randomize(42, -0.1F); },
        "invalid initialization magnitude is rejected");
}

void testGenome()
{
    using wkc::ai::Genome;
    Genome parent{wkc::ai::NeuralNetwork{3, 4, 2}, 12.0F, 2, 7};
    parent.network.randomize(100);
    Genome clone = parent;
    expect(std::ranges::equal(clone.network.parameters(), parent.network.parameters()),
        "genome clone copies all parameters");
    expect(clone.fitness == parent.fitness && clone.id == parent.id,
        "genome clone copies metadata");

    wkc::ai::mutate(clone, 50, 0.0F, 1.0F);
    expect(std::ranges::equal(clone.network.parameters(), parent.network.parameters()),
        "zero-probability mutation preserves weights");
    expect(clone.fitness == 0.0F, "mutation invalidates previous fitness");
    wkc::ai::mutate(clone, 51, 1.0F, 0.5F);
    expect(!std::ranges::equal(clone.network.parameters(), parent.network.parameters()),
        "mutation changes weights");
    expect(std::ranges::all_of(clone.network.parameters(), [](const float value) {
        return value >= -1.0F && value <= 1.0F;
    }), "mutation clamps weights");
    Genome repeated = parent;
    wkc::ai::mutate(repeated, 51, 1.0F, 0.5F);
    expect(std::ranges::equal(clone.network.parameters(), repeated.network.parameters()),
        "mutation is seeded and deterministic");
    expect(!std::ranges::equal(parent.network.parameters(), clone.network.parameters()),
        "mutation does not change the parent copy");

    Genome second{wkc::ai::NeuralNetwork{3, 4, 2}, 8.0F, 4, 9};
    second.network.randomize(200);
    const Genome child = wkc::ai::crossover(parent, second, 123, 11);
    const Genome sameChild = wkc::ai::crossover(parent, second, 123, 11);
    expect(child.generation == 5 && child.id == 11 && child.fitness == 0.0F,
        "crossover sets child metadata");
    expect(std::ranges::equal(child.network.parameters(), sameChild.network.parameters()),
        "crossover is seeded and deterministic");
    for (std::size_t index = 0; index < child.network.parameters().size(); ++index) {
        expect(child.network.parameters()[index] == parent.network.parameters()[index]
            || child.network.parameters()[index] == second.network.parameters()[index],
            "each inherited weight comes from a parent");
    }

    expectInvalid([&] { wkc::ai::mutate(parent, 1, -0.1F, 0.1F); },
        "invalid mutation probability is rejected");
    expectInvalid([&] { wkc::ai::mutate(parent, 1, 0.5F,
        std::numeric_limits<float>::infinity()); },
        "non-finite mutation strength is rejected");
    const Genome incompatible{wkc::ai::NeuralNetwork{3, 5, 2}};
    expectInvalid([&] { static_cast<void>(wkc::ai::crossover(parent, incompatible, 1, 1)); },
        "crossover rejects incompatible shapes");
}

} // namespace

int main()
{
    testNetwork();
    testGenome();
    if (failures != 0) {
        std::cerr << failures << " AI foundation tests failed\n";
    }
    return failures == 0 ? 0 : 1;
}
