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
    scene.camera.position = {3.8F + 0.5F * std::sin(phase), 1.75F, 7.0F};
    scene.camera.forward = wkc::core::normalized(
        wkc::core::Vec3{0.8F, 1.45F, -5.0F} - scene.camera.position);
    scene.camera.verticalFieldOfViewDegrees = 58.0F;
    scene.lighting.ambientIntensity = 0.38F;
    scene.lighting.lightCount = 2U;
    scene.lighting.lights[0] = {{2.5F, 3.4F, -3.0F}, {0.78F, 0.88F, 1.0F}, 1.2F, 9.0F};
    scene.lighting.lights[1] = {{-3.5F, 3.4F, -5.5F}, {0.85F, 0.88F, 0.90F}, 1.1F, 8.0F};
    for (std::size_t i = 0U; i < scene.enemies.size(); ++i) {
        auto& enemy = scene.enemies[i];
        const float angle = phase * 10.0F + static_cast<float>(i) * std::numbers::pi_v<float>;
        const float centerX = i == 0U ? 4.0F : -4.0F;
        enemy.position = {centerX + 1.4F * std::cos(angle), 0.0F, -6.0F + 1.4F * std::sin(angle)};
        enemy.role = i == 0U ? EnemyVisualRole::Guard : EnemyVisualRole::Worker;
        enemy.animationState = EnemyAnimationState::Walk;
        enemy.animationTick = static_cast<std::uint64_t>(time * 60.0);
        enemy.yawDegrees = std::atan2(-std::sin(angle), std::cos(angle))
            * 180.0F / std::numbers::pi_v<float>;
    }
    scene.props = {{
        {{6.0F, 0.5F, -2.0F}, {1.0F, 1.0F, 1.0F}, PropVisualRole::Crate},
        {{5.8F, 0.6F, -3.3F}, {0.7F, 1.2F, 0.7F}, PropVisualRole::Canister},
        {{-5.8F, 0.6F, -2.0F}, {1.5F, 1.2F, 0.4F}, PropVisualRole::Barricade},
    }};
    return scene;
}

} // namespace gdp::game::visual
