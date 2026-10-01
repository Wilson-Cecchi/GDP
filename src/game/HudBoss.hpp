#pragma once

#include "entities/EntityManager.hpp"
#include "game/visual/GdpRenderData.hpp"
#include "gameplay/Session.hpp"

namespace gdp::game {

[[nodiscard]] visual::HudBossData buildBossHud(
    const entities::EntityManager& entities,
    const gameplay::SessionState& session) noexcept;

} // namespace gdp::game
