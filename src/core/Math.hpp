#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace wkc::core {

struct Vec2 {
    float x{0.0F};
    float y{0.0F};

    constexpr bool operator==(const Vec2&) const noexcept = default;
};

struct Vec3 {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};

    constexpr bool operator==(const Vec3&) const noexcept = default;
};

[[nodiscard]] constexpr Vec2 operator+(const Vec2 left, const Vec2 right) noexcept
{
    return {left.x + right.x, left.y + right.y};
}

[[nodiscard]] constexpr Vec3 operator+(const Vec3 left, const Vec3 right) noexcept
{
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] constexpr Vec3 operator-(const Vec3 left, const Vec3 right) noexcept
{
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] constexpr Vec3 operator*(const Vec3 value, const float scalar) noexcept
{
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

[[nodiscard]] constexpr float dot(const Vec3 left, const Vec3 right) noexcept
{
    return (left.x * right.x) + (left.y * right.y) + (left.z * right.z);
}

[[nodiscard]] constexpr Vec3 cross(const Vec3 left, const Vec3 right) noexcept
{
    return {
        (left.y * right.z) - (left.z * right.y),
        (left.z * right.x) - (left.x * right.z),
        (left.x * right.y) - (left.y * right.x),
    };
}

[[nodiscard]] inline float length(const Vec3 value) noexcept
{
    return std::sqrt(dot(value, value));
}

[[nodiscard]] inline Vec3 normalized(const Vec3 value) noexcept
{
    const float magnitude = length(value);
    if (magnitude <= 1.0e-7F) {
        return {};
    }
    return value * (1.0F / magnitude);
}

struct Transform {
    Vec3 position{};
    Vec3 rotationDegrees{};
    Vec3 scale{1.0F, 1.0F, 1.0F};

    constexpr bool operator==(const Transform&) const noexcept = default;
};

struct GridPos {
    std::int32_t x{0};
    std::int32_t y{0};
    std::int32_t z{0};

    constexpr bool operator==(const GridPos&) const noexcept = default;
};

enum class QuarterTurn : std::uint8_t {
    Degrees0 = 0,
    Degrees90 = 1,
    Degrees180 = 2,
    Degrees270 = 3,
};

[[nodiscard]] constexpr Vec3 gridToWorld(
    const GridPos grid,
    const float cellSize = 1.0F) noexcept
{
    return {
        static_cast<float>(grid.x) * cellSize,
        static_cast<float>(grid.y) * cellSize,
        static_cast<float>(grid.z) * cellSize,
    };
}

[[nodiscard]] inline GridPos worldToGrid(
    const Vec3 world,
    const float cellSize = 1.0F) noexcept
{
    const float safeCellSize = cellSize > 0.0F ? cellSize : 1.0F;
    return {
        static_cast<std::int32_t>(std::lround(world.x / safeCellSize)),
        static_cast<std::int32_t>(std::lround(world.y / safeCellSize)),
        static_cast<std::int32_t>(std::lround(world.z / safeCellSize)),
    };
}

[[nodiscard]] constexpr Vec3 rotateQuarterTurns(
    const Vec3 value,
    const QuarterTurn rotation) noexcept
{
    switch (rotation) {
    case QuarterTurn::Degrees0:
        return value;
    case QuarterTurn::Degrees90:
        return {value.z, value.y, -value.x};
    case QuarterTurn::Degrees180:
        return {-value.x, value.y, -value.z};
    case QuarterTurn::Degrees270:
        return {-value.z, value.y, value.x};
    }
    return value;
}

[[nodiscard]] inline bool nearlyEqual(
    const float left,
    const float right,
    const float epsilon = 1.0e-5F) noexcept
{
    return std::abs(left - right) <= epsilon;
}

[[nodiscard]] inline bool nearlyEqual(
    const Vec3 left,
    const Vec3 right,
    const float epsilon = 1.0e-5F) noexcept
{
    return nearlyEqual(left.x, right.x, epsilon)
        && nearlyEqual(left.y, right.y, epsilon)
        && nearlyEqual(left.z, right.z, epsilon);
}

struct Aabb {
    Vec3 minimum{};
    Vec3 maximum{};

    constexpr bool operator==(const Aabb&) const noexcept = default;

    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return minimum.x <= maximum.x && minimum.y <= maximum.y
            && minimum.z <= maximum.z;
    }

    [[nodiscard]] constexpr bool contains(const Vec3 point) const noexcept
    {
        return point.x >= minimum.x && point.x <= maximum.x
            && point.y >= minimum.y && point.y <= maximum.y
            && point.z >= minimum.z && point.z <= maximum.z;
    }
};

[[nodiscard]] constexpr Aabb translated(const Aabb bounds, const Vec3 offset) noexcept
{
    return {bounds.minimum + offset, bounds.maximum + offset};
}

struct Mat4 {
    std::array<float, 16> elements{};

    [[nodiscard]] static constexpr Mat4 identity() noexcept
    {
        return {{
            1.0F, 0.0F, 0.0F, 0.0F,
            0.0F, 1.0F, 0.0F, 0.0F,
            0.0F, 0.0F, 1.0F, 0.0F,
            0.0F, 0.0F, 0.0F, 1.0F,
        }};
    }

    [[nodiscard]] constexpr const float* data() const noexcept
    {
        return elements.data();
    }
};

[[nodiscard]] constexpr Mat4 operator*(const Mat4& left, const Mat4& right) noexcept
{
    Mat4 result{};
    for (std::size_t column = 0; column < 4; ++column) {
        for (std::size_t row = 0; row < 4; ++row) {
            float value = 0.0F;
            for (std::size_t index = 0; index < 4; ++index) {
                value += left.elements[(index * 4) + row]
                    * right.elements[(column * 4) + index];
            }
            result.elements[(column * 4) + row] = value;
        }
    }
    return result;
}

[[nodiscard]] constexpr Mat4 translationMatrix(const Vec3 translation) noexcept
{
    Mat4 result = Mat4::identity();
    result.elements[12] = translation.x;
    result.elements[13] = translation.y;
    result.elements[14] = translation.z;
    return result;
}

[[nodiscard]] constexpr Mat4 scaleMatrix(const Vec3 scale) noexcept
{
    Mat4 result{};
    result.elements[0] = scale.x;
    result.elements[5] = scale.y;
    result.elements[10] = scale.z;
    result.elements[15] = 1.0F;
    return result;
}

[[nodiscard]] inline Mat4 rotationYMatrix(const float radians) noexcept
{
    if (!std::isfinite(radians)) {
        return Mat4::identity();
    }
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    Mat4 result = Mat4::identity();
    result.elements[0] = cosine;
    result.elements[2] = -sine;
    result.elements[8] = sine;
    result.elements[10] = cosine;
    return result;
}

[[nodiscard]] inline Mat4 perspectiveMatrix(
    const float verticalFieldOfViewRadians,
    const float aspectRatio,
    const float nearPlane,
    const float farPlane) noexcept
{
    if (aspectRatio <= 0.0F || nearPlane <= 0.0F || farPlane <= nearPlane) {
        return Mat4::identity();
    }

    const float tangent = std::tan(verticalFieldOfViewRadians * 0.5F);
    if (tangent <= 0.0F) {
        return Mat4::identity();
    }

    Mat4 result{};
    result.elements[0] = 1.0F / (aspectRatio * tangent);
    result.elements[5] = 1.0F / tangent;
    result.elements[10] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    result.elements[11] = -1.0F;
    result.elements[14] = -(2.0F * farPlane * nearPlane) / (farPlane - nearPlane);
    return result;
}

[[nodiscard]] inline Mat4 lookAtMatrix(
    const Vec3 eye,
    const Vec3 target,
    const Vec3 up) noexcept
{
    const Vec3 forward = normalized(target - eye);
    const Vec3 right = normalized(cross(forward, up));
    const Vec3 correctedUp = cross(right, forward);

    Mat4 result = Mat4::identity();
    result.elements[0] = right.x;
    result.elements[4] = right.y;
    result.elements[8] = right.z;
    result.elements[1] = correctedUp.x;
    result.elements[5] = correctedUp.y;
    result.elements[9] = correctedUp.z;
    result.elements[2] = -forward.x;
    result.elements[6] = -forward.y;
    result.elements[10] = -forward.z;
    result.elements[12] = -dot(right, eye);
    result.elements[13] = -dot(correctedUp, eye);
    result.elements[14] = dot(forward, eye);
    return result;
}

} // namespace wkc::core
