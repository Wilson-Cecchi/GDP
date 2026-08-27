#pragma once

#include "render/GlApi.hpp"

namespace gdp::render {

class Framebuffer {
public:
    Framebuffer() = default;
    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;
    Framebuffer(Framebuffer&& other) noexcept;
    Framebuffer& operator=(Framebuffer&& other) noexcept;

    [[nodiscard]] bool resize(const GlApi& gl, int width, int height);
    void bind(const GlApi& gl) const noexcept;
    void presentToDefault(const GlApi& gl, int targetWidth, int targetHeight) const noexcept;
    void reset() noexcept;

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] int width() const noexcept;
    [[nodiscard]] int height() const noexcept;

private:
    const GlApi* gl_{nullptr};
    GLuint framebuffer_{0};
    GLuint colorTexture_{0};
    GLuint depthBuffer_{0};
    int width_{0};
    int height_{0};
};

} // namespace gdp::render
