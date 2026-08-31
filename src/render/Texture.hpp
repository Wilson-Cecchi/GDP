#pragma once

#include "render/GlApi.hpp"

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
    void reset() noexcept;

    [[nodiscard]] bool isValid() const noexcept;

private:
    const GlApi* gl_{nullptr};
    GLuint texture_{0};
};

} // namespace wkc::render
