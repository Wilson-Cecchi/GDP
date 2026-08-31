#include "render/Camera.hpp"

#include <algorithm>
#include <numbers>

namespace wkc::render {

void Camera::setPerspective(
    const float verticalFieldOfViewDegrees,
    const float nearPlane,
    const float farPlane) noexcept
{
    fieldOfViewDegrees_ = std::clamp(verticalFieldOfViewDegrees, 20.0F, 120.0F);
    nearPlane_ = std::max(nearPlane, 0.001F);
    farPlane_ = std::max(farPlane, nearPlane_ + 0.001F);
}

void Camera::lookAt(const gdp::core::Vec3 position, const gdp::core::Vec3 target) noexcept
{
    position_ = position;
    target_ = target;
}

gdp::core::Mat4 Camera::viewMatrix() const noexcept
{
    return gdp::core::lookAtMatrix(position_, target_, {0.0F, 1.0F, 0.0F});
}

gdp::core::Mat4 Camera::projectionMatrix(const float aspectRatio) const noexcept
{
    constexpr float degreesToRadians = std::numbers::pi_v<float> / 180.0F;
    return gdp::core::perspectiveMatrix(
        fieldOfViewDegrees_ * degreesToRadians,
        aspectRatio,
        nearPlane_,
        farPlane_);
}

gdp::core::Vec3 Camera::position() const noexcept
{
    return position_;
}

} // namespace wkc::render
