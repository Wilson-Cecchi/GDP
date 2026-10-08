#pragma once
#include "gameplay/Collision.hpp"
#include "core/Seed.hpp"
namespace gdp::game::ai {
struct TrainingEncounter {
    gameplay::CollisionWorld collision;
    wkc::core::Vec3 player;
    wkc::core::Vec3 enemy;
    float peekWidth{0};
    bool elevated{false};
};
// Variants 1/2: offset cover; 3/5: peek behind narrow/wide cover;
// 4: low, narrow platform reachable by melee from the floor.
[[nodiscard]] TrainingEncounter makeTrainingEncounter(wkc::core::Aabb bounds,
    wkc::core::Seed seed, unsigned variant);
}
