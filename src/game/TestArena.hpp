#pragma once
#include "world/WorldEntities.hpp"
namespace gdp::game {
[[nodiscard]] world::World makeTestArena(wkc::core::Seed seed);
[[nodiscard]] world::InitialEntitySet populateTestArena(world::World&, entities::EntityManager&, entities::PlayerCreateInfo);
[[nodiscard]] world::World makeStorageRoomPreview(wkc::core::Seed seed);
[[nodiscard]] world::InitialEntitySet populateStorageRoomPreview(
    world::World&, entities::EntityManager&, entities::PlayerCreateInfo);
[[nodiscard]] world::World makeCombatRoomPreview(wkc::core::Seed seed);
[[nodiscard]] world::InitialEntitySet populateCombatRoomPreview(
    world::World&, entities::EntityManager&, entities::PlayerCreateInfo);
[[nodiscard]] world::World makeBossRoomPreview(wkc::core::Seed seed);
[[nodiscard]] world::InitialEntitySet populateBossRoomPreview(
    world::World&, entities::EntityManager&, entities::PlayerCreateInfo);
[[nodiscard]] world::World makeCafeteriaPreview(wkc::core::Seed seed);
[[nodiscard]] world::World makeCafeteriaLongPreview(wkc::core::Seed seed);
[[nodiscard]] world::InitialEntitySet populateCafeteriaPreview(
    world::World&, entities::EntityManager&, entities::PlayerCreateInfo);
[[nodiscard]] world::World makeLaboratoryPreview(wkc::core::Seed seed);
[[nodiscard]] world::InitialEntitySet populateLaboratoryPreview(
    world::World&, entities::EntityManager&, entities::PlayerCreateInfo);
[[nodiscard]] world::World makeTechnicalPreview(wkc::core::Seed seed);
[[nodiscard]] world::World makeTechnicalLPreview(wkc::core::Seed seed);
[[nodiscard]] world::InitialEntitySet populateTechnicalPreview(
    world::World&, entities::EntityManager&, entities::PlayerCreateInfo);
[[nodiscard]] world::World makeSecurityPreview(wkc::core::Seed seed);
[[nodiscard]] world::World makeSecurityTPreview(wkc::core::Seed seed);
[[nodiscard]] world::InitialEntitySet populateSecurityPreview(
    world::World&, entities::EntityManager&, entities::PlayerCreateInfo);
[[nodiscard]] world::World makeCentralHubPreview(wkc::core::Seed seed);
[[nodiscard]] world::InitialEntitySet populateCentralHubPreview(
    world::World&, entities::EntityManager&, entities::PlayerCreateInfo,
    bool shadowPilot = false);
[[nodiscard]] world::World makeIndustrialCorridorPreview(wkc::core::Seed seed);
[[nodiscard]] world::InitialEntitySet populateIndustrialCorridorPreview(
    world::World&, entities::EntityManager&, entities::PlayerCreateInfo);
}
