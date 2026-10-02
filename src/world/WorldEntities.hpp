#pragma once

#include "entities/EntityManager.hpp"
#include "world/World.hpp"

namespace gdp::world {

struct InitialEntitySet {
    gdp::entities::EntityId player{};
    gdp::entities::EntityId terminal{};
    gdp::entities::EntityId exitTerminal{};

    [[nodiscard]] bool isValid() const noexcept;
};

[[nodiscard]] InitialEntitySet populateInitialEntities(
    World& world,
    gdp::entities::EntityManager& manager,
    gdp::entities::PlayerCreateInfo player,
    float difficulty = 1.0F,
    bool campaignWeapons = false);

} // namespace gdp::world
