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
// 6/7: Boss-sized long firing lane, open or with central cover;
// 8: long common-room lane with cover and a lateral peek.
[[nodiscard]] TrainingEncounter makeTrainingEncounter(wkc::core::Aabb bounds,
    wkc::core::Seed seed, unsigned variant);
}
