#pragma once

#include "game/visual/GdpRenderData.hpp"

namespace gdp::game::visual {

// Presentation only: no entities, session, world RNG, brains, or save files.
struct MenuBackdropSnapshot {
    CameraData camera;
    RoomRenderData room;
    RoomLightingRenderData lighting;
    std::array<EnemyRenderData, 2> enemies;
    std::array<PropRenderData, 3> props;
};

[[nodiscard]] MenuBackdropSnapshot sampleMenuBackdrop(double seconds) noexcept;

} // namespace gdp::game::visual
