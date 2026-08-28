#pragma once

#include "core/Math.hpp"

#include <vector>

namespace gdp::gameplay {

struct CharacterCollider {
    float radius{0.3F};
    float height{1.8F};
};

struct CharacterMotionResult {
    gdp::core::Vec3 position{};
    gdp::core::Vec3 velocity{};
    bool grounded{false};
    bool collided{false};
};

class CollisionWorld {
public:
    CollisionWorld(gdp::core::Aabb roomBounds, std::vector<gdp::core::Aabb> solids = {});
    [[nodiscard]] static CollisionWorld fromWalkableVolumes(
        std::vector<gdp::core::Aabb> walkableVolumes,
        std::vector<gdp::core::Aabb> solids = {});

    [[nodiscard]] static CollisionWorld fixedRoom(
        gdp::core::Vec3 worldOrigin = {},
        gdp::core::QuarterTurn rotation = gdp::core::QuarterTurn::Degrees0);
    [[nodiscard]] CharacterMotionResult moveCharacter(
        gdp::core::Vec3 position,
        gdp::core::Vec3 velocity,
        float fixedStepSeconds,
        CharacterCollider collider) const noexcept;

    [[nodiscard]] const gdp::core::Aabb& roomBounds() const noexcept;
    [[nodiscard]] const std::vector<gdp::core::Aabb>& walkableVolumes() const noexcept;
    [[nodiscard]] const std::vector<gdp::core::Aabb>& solids() const noexcept;
    [[nodiscard]] bool canOccupy(
        gdp::core::Vec3 feetPosition,
        CharacterCollider collider = {}) const noexcept;

private:
    struct WalkableVolumesTag {
    };

    CollisionWorld(
        WalkableVolumesTag,
        std::vector<gdp::core::Aabb> walkableVolumes,
        std::vector<gdp::core::Aabb> solids);

    [[nodiscard]] bool insideWalkable(
        gdp::core::Vec3 feetPosition,
        CharacterCollider collider) const noexcept;
    [[nodiscard]] bool overlapsSolid(
        gdp::core::Vec3 feetPosition,
        CharacterCollider collider) const noexcept;

    gdp::core::Aabb roomBounds_;
    std::vector<gdp::core::Aabb> walkableVolumes_;
    std::vector<gdp::core::Aabb> solids_;
};

} // namespace gdp::gameplay
