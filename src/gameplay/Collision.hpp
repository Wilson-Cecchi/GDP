#pragma once

#include "core/Math.hpp"

#include <vector>

namespace gdp::gameplay {

struct CharacterCollider {
    float radius{0.3F};
    float height{1.8F};
};

struct CharacterMotionResult {
    wkc::core::Vec3 position{};
    wkc::core::Vec3 velocity{};
    bool grounded{false};
    bool collided{false};
};

class CollisionWorld {
public:
    CollisionWorld(wkc::core::Aabb roomBounds, std::vector<wkc::core::Aabb> solids = {});
    [[nodiscard]] static CollisionWorld fromWalkableVolumes(
        std::vector<wkc::core::Aabb> walkableVolumes,
        std::vector<wkc::core::Aabb> solids = {});

    [[nodiscard]] static CollisionWorld fixedRoom(
        wkc::core::Vec3 worldOrigin = {},
        wkc::core::QuarterTurn rotation = wkc::core::QuarterTurn::Degrees0);
    [[nodiscard]] CharacterMotionResult moveCharacter(
        wkc::core::Vec3 position,
        wkc::core::Vec3 velocity,
        float fixedStepSeconds,
        CharacterCollider collider) const noexcept;

    [[nodiscard]] const wkc::core::Aabb& roomBounds() const noexcept;
    [[nodiscard]] const std::vector<wkc::core::Aabb>& walkableVolumes() const noexcept;
    [[nodiscard]] const std::vector<wkc::core::Aabb>& solids() const noexcept;
    [[nodiscard]] bool canOccupy(
        wkc::core::Vec3 feetPosition,
        CharacterCollider collider = {}) const noexcept;

private:
    struct WalkableVolumesTag {
    };

    CollisionWorld(
        WalkableVolumesTag,
        std::vector<wkc::core::Aabb> walkableVolumes,
        std::vector<wkc::core::Aabb> solids);

    [[nodiscard]] bool insideWalkable(
        wkc::core::Vec3 feetPosition,
        CharacterCollider collider) const noexcept;
    [[nodiscard]] bool overlapsSolid(
        wkc::core::Vec3 feetPosition,
        CharacterCollider collider) const noexcept;

    wkc::core::Aabb roomBounds_;
    std::vector<wkc::core::Aabb> walkableVolumes_;
    std::vector<wkc::core::Aabb> solids_;
};

} // namespace gdp::gameplay
