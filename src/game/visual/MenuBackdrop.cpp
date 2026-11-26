#include "game/visual/MenuBackdrop.hpp"
#include "game/visual/RoomMaterialGenerator.hpp"

#include <cmath>
#include <numbers>

namespace gdp::game::visual {

MenuBackdropSnapshot sampleMenuBackdrop(const double seconds) noexcept
{
    // Bound the animation tick conversion, including malformed/very long uptime.
    const double time = std::isfinite(seconds) && seconds >= 0.0
        ? std::fmod(seconds, 240.0) : 0.0;
    const float phase = static_cast<float>(time) * std::numbers::pi_v<float> / 120.0F;
    MenuBackdropSnapshot scene;
    // initialize() already caches this baseline; never inherit the last gameplay room.
    static const auto materialKey = generateIndustrialRoomMaterials(0U).materialHash;
    scene.room.materialKey = materialKey;
    scene.room.worldBounds = {{-8.0F, 0.0F, -10.0F}, {8.0F, 4.0F, 10.0F}};
    // A quiet industrial staging bay: keep the foreground clear for menu
    // labels and place the equipment against the far wall at floor height.
    scene.camera.position = {0.8F + 0.24F * std::sin(phase),
        1.72F, 6.8F + 0.10F * std::cos(phase)};
    scene.camera.forward = wkc::core::normalized(
        wkc::core::Vec3{1.5F, 1.55F, -5.7F} - scene.camera.position);
    scene.camera.verticalFieldOfViewDegrees = 60.0F;
    scene.lighting.ambientIntensity = 0.30F;
    scene.lighting.lightCount = 2U;
    scene.lighting.lights[0] = {{3.5F, 3.5F, -4.7F},
        {1.0F, 0.76F, 0.53F}, 1.20F, 8.5F};
    scene.lighting.lights[1] = {{-3.3F, 3.5F, -5.8F},
        {0.38F, 0.69F, 0.78F}, 0.85F, 7.5F};
    for (std::size_t i = 0U; i < scene.enemies.size(); ++i) {
        auto& enemy = scene.enemies[i];
        const float angle = phase * 5.0F + static_cast<float>(i) * std::numbers::pi_v<float>;
        const float centerX = i == 0U ? 1.7F : -2.5F;
        enemy.position = {centerX + 0.85F * std::cos(angle),
            0.0F, -6.1F + 0.55F * std::sin(angle)};
        enemy.role = i == 0U ? EnemyVisualRole::Guard : EnemyVisualRole::Worker;
        enemy.animationState = EnemyAnimationState::Walk;
        enemy.animationTick = static_cast<std::uint64_t>(time * 60.0);
        enemy.yawDegrees = std::atan2(-0.85F * std::sin(angle),
            0.55F * std::cos(angle))
            * 180.0F / std::numbers::pi_v<float>;
    }
    scene.props = {{
        {{5.5F, 0.0F, -7.0F}, {1.15F, 1.8F, 0.6F},
            PropVisualRole::Terminal, PropVisualState::Intact, 0x5A17U},
        {{4.2F, 0.0F, -4.5F}, {0.95F, 0.95F, 0.95F},
            PropVisualRole::Crate, PropVisualState::Intact, 0x5A18U},
        {{6.0F, 0.0F, -4.4F}, {0.46F, 1.02F, 0.46F},
            PropVisualRole::Canister, PropVisualState::Intact, 0x5A19U},
    }};
    return scene;
}

} // namespace gdp::game::visual
