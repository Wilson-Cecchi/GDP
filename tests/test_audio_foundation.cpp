#include "audio/AudioFoundation.hpp"

#include <iostream>
#include <string_view>

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

gdp::audio::AudioRecipe makeGenericRecipe()
{
    gdp::audio::AudioRecipe recipe;
    recipe.kind = 2U;
    recipe.kindCount = 4U;
    recipe.seed = 0x574B435F41554449ULL;
    recipe.durationSeconds = 0.12F;
    recipe.baseFrequency = 180.0F;
    recipe.secondaryFrequency = 360.0F;
    recipe.attackSeconds = 0.004F;
    recipe.releaseSeconds = 0.09F;
    recipe.noiseMix = 0.16F;
    recipe.pulseRate = 2.5F;
    recipe.gain = 0.4F;
    recipe.recipeHash = gdp::audio::audioRecipeHash(recipe);
    return recipe;
}

} // namespace

int main()
{
    using namespace gdp::audio;

    const AudioRecipe recipe = makeGenericRecipe();
    expect(validateAudioRecipe(recipe).isValid(),
        "generic recipe validates without GDP event definitions");

    const AudioClip first = synthesizeAudioClip(recipe);
    const AudioClip repeated = synthesizeAudioClip(recipe);
    expect(validateAudioClip(first, recipe).isValid() && !first.samples.empty(),
        "generic recipe synthesizes a valid clip");
    expect(first.samples == repeated.samples,
        "generic synthesis remains deterministic");

    AudioRecipe invalid = recipe;
    invalid.kind = invalid.kindCount;
    expect(!validateAudioRecipe(invalid).isValid(),
        "generic category bounds are validated without a GDP enum");

    if (failures != 0) {
        std::cerr << failures << " audio foundation test(s) failed\n";
        return 1;
    }
    std::cout << "WKC Forge audio foundation tests passed\n";
    return 0;
}
