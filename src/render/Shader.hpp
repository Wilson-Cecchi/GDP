#pragma once

#include "core/Math.hpp"
#include "render/GlApi.hpp"

#include <string_view>

namespace wkc::render {

class Shader {
public:
    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    [[nodiscard]] bool compile(
        const GlApi& gl,
        std::string_view vertexSource,
        std::string_view fragmentSource);
    void use(const GlApi& gl) const noexcept;
    void setMat4(const GlApi& gl, const char* name, const gdp::core::Mat4& value) const noexcept;
    void setVec2(const GlApi& gl, const char* name, gdp::core::Vec2 value) const noexcept;
    void setVec3(const GlApi& gl, const char* name, gdp::core::Vec3 value) const noexcept;
    void setInt(const GlApi& gl, const char* name, int value) const noexcept;
    void setFloat(const GlApi& gl, const char* name, float value) const noexcept;
    void reset() noexcept;

    [[nodiscard]] bool isValid() const noexcept;

private:
    const GlApi* gl_{nullptr};
    GLuint program_{0};
};

} // namespace wkc::render
