#pragma once

#include "core/Math.hpp"

namespace gdp::render {

class Camera {
public:
    void setPerspective(float verticalFieldOfViewDegrees, float nearPlane, float farPlane) noexcept;
    void lookAt(gdp::core::Vec3 position, gdp::core::Vec3 target) noexcept;

    [[nodiscard]] gdp::core::Mat4 viewMatrix() const noexcept;
    [[nodiscard]] gdp::core::Mat4 projectionMatrix(float aspectRatio) const noexcept;
    [[nodiscard]] gdp::core::Vec3 position() const noexcept;

private:
    gdp::core::Vec3 position_{0.0F, 1.7F, 6.5F};
    gdp::core::Vec3 target_{0.0F, 1.5F, 0.0F};
    float fieldOfViewDegrees_{68.0F};
    float nearPlane_{0.05F};
    float farPlane_{100.0F};
};

} // namespace gdp::render
