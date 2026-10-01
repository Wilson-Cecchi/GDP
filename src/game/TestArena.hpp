#pragma once
#include "world/WorldEntities.hpp"
namespace gdp::game {
[[nodiscard]] world::World makeTestArena(wkc::core::Seed seed);
[[nodiscard]] world::InitialEntitySet populateTestArena(world::World&, entities::EntityManager&, entities::PlayerCreateInfo);
}
