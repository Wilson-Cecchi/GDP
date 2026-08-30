#include "render/Texture.hpp"

#include "core/Log.hpp"

#include <algorithm>
#include <array>
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

CeilingFixtureMaterialImages makeCeilingFixtureMaterialImages()
{
    constexpr int resolution = 1024;
    CeilingFixtureMaterialImages images;
    for (ImageData* const image
        : {&images.albedo, &images.normal, &images.properties, &images.emission}) {
        image->width = resolution;
        image->height = resolution;
        image->rgba.resize(
            static_cast<std::size_t>(resolution) * resolution * 4U);
    }
    const auto write = [](ImageData& image, const int x, const int y,
                           const std::array<std::uint8_t, 4> color) {
        const std::size_t pixel = (static_cast<std::size_t>(y)
            * static_cast<std::size_t>(image.width)
            + static_cast<std::size_t>(x)) * 4U;
        std::copy(color.begin(), color.end(), image.rgba.begin()
                + static_cast<std::ptrdiff_t>(pixel));
    };

    for (int y = 0; y < resolution; ++y) {
        for (int x = 0; x < resolution; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(resolution - 1);
            const float v = static_cast<float>(y) / static_cast<float>(resolution - 1);
            const int fine = ((x * 17) + (y * 31) + ((x ^ y) * 7)) & 15;
            std::array<std::uint8_t, 4> albedo{};
            std::array<std::uint8_t, 4> normal{128U, 128U, 255U, 255U};
            std::array<std::uint8_t, 4> properties{};
            std::array<std::uint8_t, 4> emission{0U, 0U, 0U, 255U};

            if (u < 0.50F) {
                const std::uint8_t grain = static_cast<std::uint8_t>(46 + fine / 2);
                albedo = {grain, static_cast<std::uint8_t>(grain + 4U),
                    static_cast<std::uint8_t>(grain + 6U), 255U};
                normal = {static_cast<std::uint8_t>(126 + (fine & 3)), 128U, 255U, 255U};
                properties = {148U, 82U, 242U, 255U};
            } else if (u < 0.68F) {
                const int brush = (y + (x / 7)) & 7;
                const std::uint8_t steel = static_cast<std::uint8_t>(136 + brush);
                albedo = {steel, static_cast<std::uint8_t>(steel + 3U),
                    static_cast<std::uint8_t>(steel + 4U), 255U};
                normal = {128U, static_cast<std::uint8_t>(125 + brush), 255U, 255U};
                properties = {86U, 232U, 246U, 255U};
            } else if (u < 0.75F) {
                const std::uint8_t gasket = static_cast<std::uint8_t>(20 + fine / 4);
                albedo = {gasket, static_cast<std::uint8_t>(gasket + 5U),
                    static_cast<std::uint8_t>(gasket + 7U), 255U};
                normal = {128U, 128U, 252U, 255U};
                properties = {204U, 10U, 174U, 255U};
            } else {
                const int weave = (((x / 3) + (y / 3)) & 1) == 0 ? 5 : 0;
                const std::uint8_t lens = static_cast<std::uint8_t>(218 + weave + fine / 5);
                albedo = {lens, static_cast<std::uint8_t>(std::min(255, lens + 7)),
                    static_cast<std::uint8_t>(std::min(255, lens + 9)), 255U};
                normal = {static_cast<std::uint8_t>(127 + (fine & 1)),
                    static_cast<std::uint8_t>(127 + ((fine >> 1) & 1)), 255U, 255U};
                properties = {166U, 5U, 250U, 255U};
                const float localU = (u - 0.75F) * 4.0F;
                const float edge = std::min({localU, 1.0F - localU, v, 1.0F - v});
                const std::uint8_t mask = edge <= 0.025F ? 0U
                    : static_cast<std::uint8_t>(
                          std::clamp((edge - 0.025F) * 5'100.0F, 0.0F, 255.0F));
                emission = {mask, mask, mask, 255U};
            }

            write(images.albedo, x, y, albedo);
            write(images.normal, x, y, normal);
            write(images.properties, x, y, properties);
            write(images.emission, x, y, emission);
        }
    }
    return images;
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
