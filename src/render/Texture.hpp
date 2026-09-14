#pragma once

#include "render/GlApi.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace wkc::render {

struct ImageData {
    int width{0};
    int height{0};
    std::vector<std::uint8_t> rgba;
};

[[nodiscard]] bool validateImageData(const ImageData& image) noexcept;

enum class TextureSampling : std::uint8_t {
    Pixelated,
    Trilinear,
};

inline constexpr std::size_t trackedTextureUnitCount = 16U;

struct TextureBindingDecision {
    bool activateUnit{false};
    bool bindTexture{false};

    constexpr bool operator==(const TextureBindingDecision&) const noexcept
        = default;
};

struct TextureBindingCacheStats {
    std::uint64_t requests{0U};
    std::uint64_t unitActivations{0U};
    std::uint64_t textureBinds{0U};
    std::uint64_t skippedTextureBinds{0U};

    constexpr bool operator==(const TextureBindingCacheStats&) const noexcept
        = default;
};

class TextureBindingCache {
public:
    [[nodiscard]] TextureBindingDecision prepare(
        unsigned int unit, GLuint texture) noexcept;
    void beginFrame() noexcept;
    [[nodiscard]] TextureBindingCacheStats stats() const noexcept;

private:
    std::array<GLuint, trackedTextureUnitCount> textures_{};
    std::array<bool, trackedTextureUnitCount> knownTextures_{};
    unsigned int activeUnit_{0U};
    bool activeUnitKnown_{false};
    TextureBindingCacheStats stats_{};
};

class Texture {
public:
    Texture() = default;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    [[nodiscard]] bool upload(
        const GlApi& gl,
        const ImageData& image,
        TextureSampling sampling = TextureSampling::Pixelated);
    void bind(const GlApi& gl, unsigned int unit = 0) const noexcept;
    void bind(
        const GlApi& gl,
        TextureBindingCache& cache,
        unsigned int unit = 0) const noexcept;
    void reset() noexcept;

    [[nodiscard]] bool isValid() const noexcept;

private:
    const GlApi* gl_{nullptr};
    GLuint texture_{0};
};

} // namespace wkc::render
