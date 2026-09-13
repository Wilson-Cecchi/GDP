#pragma once

#include "core/Math.hpp"
#include "render/GlApi.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace wkc::render {

struct ShaderUniformCacheStats {
    std::uint64_t requests{0U};
    std::uint64_t hits{0U};
    std::uint64_t driverLookups{0U};
    std::size_t entries{0U};

    constexpr bool operator==(const ShaderUniformCacheStats&) const noexcept
        = default;
};

class ShaderUniformLocationCache {
public:
    [[nodiscard]] GLint resolve(
        const GlApi& gl, GLuint program, const char* name) noexcept;
    void reset() noexcept;
    [[nodiscard]] ShaderUniformCacheStats stats() const noexcept;

private:
    struct TransparentStringHash {
        using is_transparent = void;

        [[nodiscard]] std::size_t operator()(std::string_view value) const noexcept
        {
            return std::hash<std::string_view>{}(value);
        }
    };

    std::unordered_map<std::string, GLint,
        TransparentStringHash, std::equal_to<>> locations_{};
    std::uint64_t requests_{0U};
    std::uint64_t hits_{0U};
    std::uint64_t driverLookups_{0U};
};

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
    void setMat4(const GlApi& gl, const char* name, const wkc::core::Mat4& value) const noexcept;
    void setVec2(const GlApi& gl, const char* name, wkc::core::Vec2 value) const noexcept;
    void setVec3(const GlApi& gl, const char* name, wkc::core::Vec3 value) const noexcept;
    void setInt(const GlApi& gl, const char* name, int value) const noexcept;
    void setFloat(const GlApi& gl, const char* name, float value) const noexcept;
    void reset() noexcept;

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] ShaderUniformCacheStats uniformCacheStats() const noexcept
    {
        return uniformLocations_.stats();
    }

private:
    const GlApi* gl_{nullptr};
    GLuint program_{0};
    mutable ShaderUniformLocationCache uniformLocations_{};
};

} // namespace wkc::render
