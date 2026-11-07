#pragma once

#include "render/GlApi.hpp"

namespace wkc::render {

class ShadowCubeMap {
public:
    ShadowCubeMap() = default;
    ~ShadowCubeMap();
    ShadowCubeMap(const ShadowCubeMap&) = delete;
    ShadowCubeMap& operator=(const ShadowCubeMap&) = delete;

    [[nodiscard]] bool resize(const GlApi& gl, int size);
    void bindFace(const GlApi& gl, unsigned int face) const noexcept;
    void reset() noexcept;
    [[nodiscard]] bool isValid() const noexcept { return framebuffer_ != 0 && texture_ != 0; }
    [[nodiscard]] GLuint texture() const noexcept { return texture_; }
    [[nodiscard]] int size() const noexcept { return size_; }

private:
    const GlApi* gl_{nullptr};
    GLuint framebuffer_{0};
    GLuint texture_{0};
    int size_{0};
};

} // namespace wkc::render
