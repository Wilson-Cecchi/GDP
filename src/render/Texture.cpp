#include "render/Texture.hpp"

#include "core/Log.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace gdp::render {

ImageData makeIndustrialCheckerImage(const int width, const int height)
{
    ImageData image;
    image.width = std::max(width, 1);
    image.height = std::max(height, 1);
    const auto pixelCount = static_cast<std::size_t>(image.width)
        * static_cast<std::size_t>(image.height);
    image.rgba.resize(pixelCount * 4U);

    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const bool alternate = ((x / 2) + (y / 2)) % 2 != 0;
            const std::uint8_t base = alternate ? 82U : 112U;
            const auto pixel = (static_cast<std::size_t>(y)
                * static_cast<std::size_t>(image.width)
                + static_cast<std::size_t>(x)) * 4U;
            image.rgba[pixel] = base;
            image.rgba[pixel + 1U] = static_cast<std::uint8_t>(base + 5U);
            image.rgba[pixel + 2U] = static_cast<std::uint8_t>(base + 7U);
            image.rgba[pixel + 3U] = 255U;
        }
    }
    return image;
}

bool validateImageData(const ImageData& image) noexcept
{
    if (image.width <= 0 || image.height <= 0) {
        return false;
    }
    const auto expectedSize = static_cast<std::size_t>(image.width)
        * static_cast<std::size_t>(image.height) * 4U;
    return image.rgba.size() == expectedSize;
}

Texture::~Texture()
{
    reset();
}

Texture::Texture(Texture&& other) noexcept
    : gl_(std::exchange(other.gl_, nullptr))
    , texture_(std::exchange(other.texture_, 0))
{
}

Texture& Texture::operator=(Texture&& other) noexcept
{
    if (this != &other) {
        reset();
        gl_ = std::exchange(other.gl_, nullptr);
        texture_ = std::exchange(other.texture_, 0);
    }
    return *this;
}

bool Texture::upload(
    const GlApi& gl,
    const ImageData& image,
    const TextureSampling sampling)
{
    if (!validateImageData(image)) {
        gdp::core::log(gdp::core::LogLevel::Error, "Texture upload rejected invalid image data");
        return false;
    }

    reset();
    gl_ = &gl;
    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    const bool smooth = sampling == TextureSampling::Trilinear;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
        smooth ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
        smooth ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        image.width,
        image.height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        image.rgba.data());
    gl.generateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture_ != 0;
}

void Texture::bind(const GlApi& gl, const unsigned int unit) const noexcept
{
    gl.activeTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
    glBindTexture(GL_TEXTURE_2D, texture_);
}

void Texture::reset() noexcept
{
    if (texture_ != 0) {
        glDeleteTextures(1, &texture_);
    }
    gl_ = nullptr;
    texture_ = 0;
}

bool Texture::isValid() const noexcept
{
    return texture_ != 0;
}

} // namespace gdp::render
