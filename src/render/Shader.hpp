#pragma once

#include "core/Math.hpp"
#include "render/GlApi.hpp"

#include <array>
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

struct ShaderUniformValueCacheStats {
    std::uint64_t requests{0U};
    std::uint64_t uploads{0U};
    std::uint64_t skips{0U};
    std::size_t entries{0U};

    constexpr bool operator==(const ShaderUniformValueCacheStats&) const noexcept
        = default;
};

class ShaderUniformValueCache {
public:
    [[nodiscard]] bool needsUpload(
        GLint location, const wkc::core::Mat4& value) noexcept;
    [[nodiscard]] bool needsUpload(
        GLint location, wkc::core::Vec2 value) noexcept;
    [[nodiscard]] bool needsUpload(
        GLint location, wkc::core::Vec3 value) noexcept;
    [[nodiscard]] bool needsUpload(GLint location, int value) noexcept;
    [[nodiscard]] bool needsUpload(GLint location, float value) noexcept;
    void beginFrame() noexcept;
    void reset() noexcept;
    [[nodiscard]] ShaderUniformValueCacheStats stats() const noexcept;

private:
    enum class ValueKind : std::uint8_t {
        Mat4,
        Vec2,
        Vec3,
        Int,
        Float,
    };

    struct UniformValue {
        ValueKind kind{ValueKind::Float};
        std::array<float, 16U> components{};
        int integer{0};

        constexpr bool operator==(const UniformValue&) const noexcept = default;
    };

    [[nodiscard]] bool needsUpload(
        GLint location, const UniformValue& value) noexcept;

    std::unordered_map<GLint, UniformValue> values_{};
    std::uint64_t requests_{0U};
    std::uint64_t uploads_{0U};
    std::uint64_t skips_{0U};
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
    void beginUniformFrame() const noexcept
    {
        uniformValues_.beginFrame();
    }
    [[nodiscard]] ShaderUniformValueCacheStats uniformValueCacheStats() const noexcept
    {
        return uniformValues_.stats();
    }

private:
    const GlApi* gl_{nullptr};
    GLuint program_{0};
    mutable ShaderUniformLocationCache uniformLocations_{};
    mutable ShaderUniformValueCache uniformValues_{};
};

} // namespace wkc::render
