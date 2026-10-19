#include "game/visual/GdpRenderer.hpp"
#include "game/MenuFlow.hpp"
#include "game/visual/MenuBackdrop.hpp"
#include "gameplay/PlayerGrenade.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "WeaponReloadValidation.hpp"
#include "PlayerHandsValidation.hpp"
#include "PlayerBodyValidation.hpp"

namespace {

struct InstancingPixelComparison {
    unsigned maximumDelta{0U};
    std::size_t changedChannels{0U};
    bool alphaMatches{true};
    bool equivalent{false};
};

InstancingPixelComparison compareInstancingPixels(
    const std::vector<std::uint8_t>& actual, const std::vector<std::uint8_t>& reference)
{
    InstancingPixelComparison result;
    if (actual.empty() || actual.size() != reference.size() || actual.size() % 4U != 0U)
        return result;
    for (std::size_t i = 0U; i < actual.size(); ++i) {
        const auto delta = static_cast<unsigned>(std::abs(int(actual[i]) - int(reference[i])));
        if (i % 4U == 3U) result.alphaMatches &= delta == 0U;
        else {
            result.maximumDelta = std::max(result.maximumDelta, delta);
            result.changedChannels += delta != 0U;
        }
    }
    // llvmpipe: 11/192,000 RGB channels differ, maximum 7/255. NVIDIA is exact.
    // Permit <=8/255 only in <=1/8192 RGB channels; alpha remains exact.
    result.equivalent = result.alphaMatches && result.maximumDelta <= 8U
        && result.changedChannels <= (actual.size() / 4U * 3U) / 8192U + 1U;
    return result;
}

bool validateInstancingTolerance()
{
    std::vector<std::uint8_t> reference(320U * 200U * 4U, 100U);
    auto actual = reference;
    bool passed = compareInstancingPixels(actual, reference).equivalent;
    actual[0] += 7U;
    passed &= compareInstancingPixels(actual, reference).equivalent;
    actual[0] = 109U;
    passed &= !compareInstancingPixels(actual, reference).equivalent;
    actual = reference; actual[3] = 101U;
    passed &= !compareInstancingPixels(actual, reference).equivalent;
    actual = reference;
    for (std::size_t i = 0U; i < 400U; i += 4U) ++actual[i];
    passed &= !compareInstancingPixels(actual, reference).equivalent;
    actual.pop_back();
    passed &= !compareInstancingPixels(actual, reference).equivalent;
    if (!passed) std::cerr << "Instancing tolerance accepts a meaningful pixel regression\n";
    return passed;
}

// Manual renderer capture of the approved Worker limb proposal. These frames
// support technical review; final appearance is judged in a played build.
bool captureWorkerLimbVariation(gdp::game::visual::GdpRenderer& renderer)
{
    using namespace gdp::game::visual;
    const char* directory = std::getenv("GDP_WORKER_LIMB_CAPTURE_DIR");
    if (!directory || !*directory) {
        std::cerr << "Set GDP_WORKER_LIMB_CAPTURE_DIR for Worker captures\n";
        return false;
    }
    std::filesystem::create_directories(directory);
    constexpr int width = 1280;
    constexpr int height = 720;
    RoomRenderData room;
    room.origin = {0.0F, 0.0F, -20.0F};
    room.worldBounds = {{-5.0F, 0.0F, -4.0F}, {5.0F, 4.0F, 9.0F}};
    RoomLightingRenderData lighting;
    lighting.ambientColor = {1.0F, 1.0F, 1.0F};
    const bool darkEyeCapture = std::getenv("GDP_WORKER_EYE_DARK_CAPTURE") != nullptr;
    lighting.ambientIntensity = darkEyeCapture ? 0.12F : 0.48F;
    lighting.lights[0] = {
        {0.0F, 3.3F, 2.0F}, {1.0F, 1.0F, 1.0F}, 1.25F, 8.0F};
    if (darkEyeCapture) lighting.lightCount = 0U;
    bool passed = true;
    for (const bool potato : {false, true}) {
        HudRenderData hud;
        // Suppress the central reticle so it cannot cover the eye in the
        // manual Worker review captures.
        hud.weapon = HudWeaponRole::Sniper;
        hud.graphicsQuality = potato
            ? HudGraphicsQuality::Potato : HudGraphicsQuality::Medium;
        hud.detailedMaterials = !potato;
        hud.worldRenderScalePercent = potato ? 50U : 100U;
        for (const wkc::core::Seed seed : {101U, 202U, 303U}) {
            passed &= renderer.prepareWorkerVisual(seed);
            for (const bool near : {true, false}) {
                for (const bool side : {false, true}) {
                    if (side && !near) continue;
                    CameraData camera;
                    camera.position = side
                        ? wkc::core::Vec3{3.3F, 1.55F, 0.0F}
                        : wkc::core::Vec3{0.0F, 1.55F, near ? 2.7F : 7.5F};
                    camera.forward = side
                        ? wkc::core::Vec3{-1.0F, 0.0F, 0.0F}
                        : wkc::core::Vec3{0.0F, 0.0F, -1.0F};
                    for (const EnemyAnimationState state : {
                             EnemyAnimationState::Idle, EnemyAnimationState::Walk,
                             EnemyAnimationState::Attack}) {
                        if (!near && state != EnemyAnimationState::Idle) continue;
                        if (side && state == EnemyAnimationState::Walk) continue;
                        if (potato && state != EnemyAnimationState::Idle) continue;
                        EnemyRenderData worker{{0.0F, 0.0F, 0.0F}, true, seed};
                        worker.animationState = state;
                        worker.animationTick = 15U;
                        const std::array enemies{worker};
                        passed &= renderer.render(width, height, camera, enemies,
                            {}, {}, lighting, room, {}, {}, {}, hud);
                        glFinish();
                        std::vector<std::uint8_t> pixels(
                            static_cast<std::size_t>(width * height * 4));
                        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                            pixels.data());
                        passed &= glGetError() == GL_NO_ERROR;
                        std::vector<std::uint8_t> topDown(pixels.size());
                        const std::size_t stride = static_cast<std::size_t>(width * 4);
                        for (int row = 0; row < height; ++row) {
                            std::copy_n(pixels.data()
                                    + static_cast<std::size_t>(height - row - 1) * stride,
                                stride, topDown.data() + static_cast<std::size_t>(row) * stride);
                        }
                        const char* phase = state == EnemyAnimationState::Walk ? "walk"
                            : state == EnemyAnimationState::Attack ? "attack" : "idle";
                        const auto path = std::filesystem::path{directory}
                            / (std::string{potato ? "potato" : "medium"}
                                + "_worker_seed" + std::to_string(seed)
                                + (side ? "_side_" : near ? "_near_" : "_far_")
                                + phase + ".bmp");
                        SDL_Surface* surface = SDL_CreateSurfaceFrom(width, height,
                            SDL_PIXELFORMAT_RGBA32, topDown.data(), width * 4);
                        passed &= surface && SDL_SaveBMP(surface, path.string().c_str());
                        if (surface) SDL_DestroySurface(surface);
                    }
                }
            }
        }
    }
    return passed;
}

// Review-only orthographic comparisons for the reconstructed enemy models.
bool captureEnemyReferenceModels(gdp::game::visual::GdpRenderer& renderer)
{
    using namespace gdp::game::visual;
    const char* const directory = std::getenv("GDP_ENEMY_REFERENCE_CAPTURE_DIR");
    const char* const enabled = std::getenv("GDP_ENEMY_REFERENCE_MODELS");
    if (directory == nullptr || *directory == '\0' || enabled == nullptr
        || std::string_view{enabled} != "1") {
        std::cerr << "Set GDP_ENEMY_REFERENCE_MODELS=1 and "
                     "GDP_ENEMY_REFERENCE_CAPTURE_DIR for model captures\n";
        return false;
    }
    std::filesystem::create_directories(directory);
    constexpr int width = 1280;
    constexpr int height = 720;
    RoomRenderData room;
    room.origin = {0.0F, 0.0F, -20.0F};
    room.worldBounds = {{-5.0F, 0.0F, -4.0F}, {5.0F, 4.0F, 9.0F}};
    RoomLightingRenderData lighting;
    lighting.ambientColor = {1.0F, 1.0F, 1.0F};
    lighting.ambientIntensity = 0.34F;
    lighting.lights[0] = {{0.0F, 3.3F, 2.0F},
        {1.0F, 0.95F, 0.85F}, 0.85F, 8.0F};
    lighting.lightCount = 1U;
    constexpr std::array roles{
        EnemyVisualRole::Worker, EnemyVisualRole::Guard,
        EnemyVisualRole::Officer, EnemyVisualRole::Engineer,
        EnemyVisualRole::IndustrialRobot};
    constexpr std::array names{"worker", "guard", "officer", "engineer", "robot"};
    constexpr std::array angles{
        "front", "left", "back", "right", "oblique_right"};
    constexpr std::array positions{
        wkc::core::Vec3{0.0F, 1.55F, 2.9F},
        wkc::core::Vec3{-2.9F, 1.55F, 0.0F},
        wkc::core::Vec3{0.0F, 1.55F, -2.9F},
        wkc::core::Vec3{2.9F, 1.55F, 0.0F},
        wkc::core::Vec3{2.3F, 1.55F, 2.3F}};
    bool passed = true;
    for (std::size_t roleIndex = 0U; roleIndex < roles.size(); ++roleIndex) {
        for (const bool potato : {false, true}) {
            HudRenderData hud;
            hud.weapon = HudWeaponRole::Sniper;
            hud.graphicsQuality = potato
                ? HudGraphicsQuality::Potato : HudGraphicsQuality::Medium;
            hud.detailedMaterials = !potato;
            hud.worldRenderScalePercent = potato ? 50U : 100U;
            for (std::size_t angle = 0U; angle <= angles.size(); ++angle) {
                if (potato && angle != 0U) continue;
                CameraData camera;
                const bool detailFront = angle == angles.size();
                camera.position = positions[detailFront ? 0U : angle];
                camera.forward = wkc::core::Vec3{} - camera.position;
                camera.forward.y = 0.0F;
                if (detailFront || angle == 4U) {
                    camera.verticalFieldOfViewDegrees = 48.0F;
                }
                const int variants = detailFront && !potato
                    ? roles[roleIndex] == EnemyVisualRole::IndustrialRobot ? 3 : 2
                    : !potato && roles[roleIndex] == EnemyVisualRole::Guard
                        && (angle == 1U || angle == 3U || angle == 4U) ? 2 : 1;
                for (int variant = 0; variant < variants; ++variant) {
                    EnemyRenderData enemy{{0.0F, 0.0F, 0.0F}, true,
                        static_cast<wkc::core::Seed>(101U + roleIndex), roles[roleIndex]};
                    if (variant != 0) {
                        enemy.animationState = EnemyAnimationState::Attack;
                        enemy.previousAnimationState = EnemyAnimationState::Attack;
                        enemy.attackVisualKind = variant == 2
                                || roles[roleIndex] == EnemyVisualRole::Worker
                            ? EnemyAttackVisualKind::Melee
                            : EnemyAttackVisualKind::Ranged;
                        enemy.attackPulse = 1.0F;
                    }
                    const std::array enemies{enemy};
                    passed &= renderer.render(width, height, camera, enemies,
                        {}, {}, lighting, room, {}, {}, {}, hud);
                    glFinish();
                    std::vector<std::uint8_t> pixels(
                        static_cast<std::size_t>(width * height * 4));
                    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                        pixels.data());
                    passed &= glGetError() == GL_NO_ERROR;
                    std::vector<std::uint8_t> topDown(pixels.size());
                    const std::size_t stride = static_cast<std::size_t>(width * 4);
                    for (int row = 0; row < height; ++row) {
                        std::copy_n(pixels.data()
                                + static_cast<std::size_t>(height - row - 1) * stride,
                            stride, topDown.data() + static_cast<std::size_t>(row) * stride);
                    }
                    const std::string view = variant == 2 ? "melee_front"
                        : variant == 1 ? (detailFront ? "attack_front"
                            : std::string{"attack_"} + angles[angle])
                        : potato ? "potato_front" : detailFront
                            ? "detail_front" : angles[angle];
                    const auto path = std::filesystem::path{directory}
                        / (std::string{names[roleIndex]} + "_" + view + ".bmp");
                    SDL_Surface* surface = SDL_CreateSurfaceFrom(width, height,
                        SDL_PIXELFORMAT_RGBA32, topDown.data(), width * 4);
                    passed &= surface && SDL_SaveBMP(surface, path.string().c_str());
                    if (surface) SDL_DestroySurface(surface);
                }
            }
        }
    }
    return passed;
}

// Manual offscreen capture; excluded from the normal renderer pixel suite.
bool captureGrenadeSmoke(gdp::game::visual::GdpRenderer& renderer)
{
    using namespace gdp::game::visual;
    constexpr int width = 1280;
    constexpr int height = 720;
    const char* directory = std::getenv("GDP_GRENADE_SMOKE_CAPTURE_DIR");
    if (!directory || !*directory) {
        std::cerr << "Set GDP_GRENADE_SMOKE_CAPTURE_DIR for manual grenade capture\n";
        return false;
    }
    std::filesystem::create_directories(directory);
    bool passed = true;
    for (const bool potato : {false, true}) {
        HudRenderData hud;
        hud.graphicsQuality = potato
            ? HudGraphicsQuality::Potato : HudGraphicsQuality::Medium;
        hud.detailedMaterials = !potato;
        hud.worldRenderScalePercent = potato ? 50U : 100U;
        for (const float fov : {60.0F, 90.0F}) {
            CameraData camera;
            camera.position.x = 1.5F;
            camera.verticalFieldOfViewDegrees = fov;
            for (const float distance : {1.2F, 4.0F}) {
                const bool near = distance < 2.0F;
                const auto position = camera.position + camera.forward * distance;
                const std::string prefix = std::string{potato ? "potato" : "medium"}
                    + "_fov" + std::to_string(static_cast<int>(fov))
                    + (near ? "_near" : "_far");
                const auto capture = [&](const std::string& phase,
                                         const std::span<const GrenadeRenderData> grenades,
                                         const std::span<const GrenadeBlastRenderData> blasts) {
                    passed &= renderer.render(width, height, camera, {}, {}, {}, {},
                        {}, {}, {}, {}, hud, {}, {}, {}, {}, grenades, blasts);
                    const auto stats = renderer.lastFrameStats();
                    glFinish();
                    std::vector<std::uint8_t> pixels(
                        static_cast<std::size_t>(width * height * 4));
                    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                        pixels.data());
                    passed &= glGetError() == GL_NO_ERROR;
                    std::vector<std::uint8_t> topDown(pixels.size());
                    const std::size_t stride = static_cast<std::size_t>(width * 4);
                    for (int row = 0; row < height; ++row)
                        std::copy_n(pixels.data()
                            + static_cast<std::size_t>(height - row - 1) * stride,
                            stride, topDown.data() + static_cast<std::size_t>(row) * stride);
                    const auto path = std::filesystem::path{directory}
                        / (prefix + "_" + phase + ".bmp");
                    SDL_Surface* surface = SDL_CreateSurfaceFrom(width, height,
                        SDL_PIXELFORMAT_RGBA32, topDown.data(), width * 4);
                    passed &= surface && SDL_SaveBMP(surface, path.string().c_str());
                    if (surface) SDL_DestroySurface(surface);
                    std::cout << "GRENADE_SMOKE_FRAME quality="
                        << (potato ? "POTATO" : "MEDIUM")
                        << " fov=" << fov << " distance=" << distance
                        << " phase=" << phase << " draws=" << stats.drawCalls
                        << " triangles=" << stats.triangles << '\n';
                    return pixels;
                };
                const auto changedPixels = [](const std::vector<std::uint8_t>& a,
                                              const std::vector<std::uint8_t>& b) {
                    std::size_t count = 0U;
                    for (std::size_t pixel = 0U; pixel < a.size(); pixel += 4U)
                        count += a[pixel] != b[pixel]
                            || a[pixel + 1U] != b[pixel + 1U]
                            || a[pixel + 2U] != b[pixel + 2U];
                    return count;
                };
                const auto baseline = capture("baseline", {}, {});
                const std::array grenade{GrenadeRenderData{position}};
                const auto projectile = capture("in_flight", grenade, {});
                const auto projectileChanged = changedPixels(projectile, baseline);
                passed &= projectileChanged > 0U;
                for (const auto& [phase, strength] : {
                         std::pair{"blast_start", 1.0F},
                         std::pair{"blast_mid", 0.5F},
                         std::pair{"blast_end", 0.1F}}) {
                    const std::array blast{GrenadeBlastRenderData{position, strength}};
                    const auto effect = capture(phase, {}, blast);
                    const auto changed = changedPixels(effect, baseline);
                    passed &= changed > 0U;
                    std::cout << "GRENADE_SMOKE quality="
                        << (potato ? "POTATO" : "MEDIUM")
                        << " fov=" << fov << " distance=" << distance
                        << " phase=" << phase << " changed_pixels=" << changed << '\n';
                }
                std::cout << "GRENADE_SMOKE quality="
                    << (potato ? "POTATO" : "MEDIUM")
                    << " fov=" << fov << " distance=" << distance
                    << " phase=in_flight changed_pixels=" << projectileChanged << '\n';
            }
        }
    }
    return passed;
}

bool profileGrenadeBlast(gdp::game::visual::GdpRenderer& renderer)
{
    using namespace gdp::game::visual;
    constexpr int width = 1280;
    constexpr int height = 720;
    constexpr unsigned int warmup = 8U;
    constexpr unsigned int samples = 32U;
    const CameraData camera{};
    const auto median = [](std::vector<double> values) {
        std::sort(values.begin(), values.end());
        return values[values.size() / 2U];
    };
    bool passed = true;
    for (const bool potato : {false, true}) {
        HudRenderData hud;
        hud.graphicsQuality = potato
            ? HudGraphicsQuality::Potato : HudGraphicsQuality::Medium;
        hud.detailedMaterials = !potato;
        hud.worldRenderScalePercent = potato ? 50U : 100U;
        const auto render = [&](const std::span<const GrenadeBlastRenderData> blasts) {
            return renderer.render(width, height, camera, {}, {}, {}, {}, {},
                {}, {}, {}, hud, {}, {}, {}, {}, {}, blasts);
        };
        const auto pixels = [&](const std::span<const GrenadeBlastRenderData> blasts) {
            passed &= render(blasts);
            glFinish();
            std::vector<std::uint8_t> result(
                static_cast<std::size_t>(width * height * 4));
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                result.data());
            passed &= glGetError() == GL_NO_ERROR;
            return result;
        };
        const auto baselinePixels = pixels({});
        struct Case {
            const char* name;
            bool enabled;
            wkc::core::Vec3 position;
            float strength;
        };
        const std::array cases{
            Case{"none", false, {}, 0.0F},
            Case{"near_start", true, camera.position + camera.forward * .25F, 1.0F},
            Case{"near_mid", true, camera.position + camera.forward * .25F, .50F},
            Case{"near_late", true, camera.position + camera.forward * .25F, .10F},
            Case{"far_late", true, camera.position + camera.forward * 3.0F, .10F},
            Case{"none_after", false, {}, 0.0F},
        };
        for (const auto& entry : cases) {
            const std::array blast{
                GrenadeBlastRenderData{entry.position, entry.strength}};
            const std::span<const GrenadeBlastRenderData> span = entry.enabled
                ? std::span<const GrenadeBlastRenderData>{blast}
                : std::span<const GrenadeBlastRenderData>{};
            const auto effectPixels = pixels(span);
            std::size_t changed = 0U;
            for (std::size_t i = 0; i < effectPixels.size(); i += 4U) {
                const int delta = std::abs(int(effectPixels[i]) - int(baselinePixels[i]))
                    + std::abs(int(effectPixels[i + 1U]) - int(baselinePixels[i + 1U]))
                    + std::abs(int(effectPixels[i + 2U]) - int(baselinePixels[i + 2U]));
                changed += delta > 9;
            }
            std::vector<double> renderMilliseconds;
            std::vector<double> finishMilliseconds;
            std::vector<double> gpuMilliseconds;
            RendererFrameStats stats{};
            for (unsigned int frame = 0U; frame < warmup + samples; ++frame) {
                const auto begin = std::chrono::steady_clock::now();
                passed &= render(span);
                const auto submitted = std::chrono::steady_clock::now();
                glFinish();
                const auto finished = std::chrono::steady_clock::now();
                passed &= glGetError() == GL_NO_ERROR;
                if (frame < warmup) continue;
                stats = renderer.lastFrameStats();
                renderMilliseconds.push_back(
                    std::chrono::duration<double, std::milli>(submitted - begin).count());
                finishMilliseconds.push_back(
                    std::chrono::duration<double, std::milli>(finished - submitted).count());
                if (stats.gpuTimingAvailable)
                    gpuMilliseconds.push_back(stats.gpuMilliseconds);
            }
            std::cout << "GRENADE_BLAST_PROFILE quality="
                << (potato ? "POTATO" : "MEDIUM")
                << " case=" << entry.name
                << " render_ms=" << median(renderMilliseconds)
                << " finish_ms=" << median(finishMilliseconds)
                << " gpu_ms="
                << (gpuMilliseconds.empty() ? -1.0 : median(gpuMilliseconds))
                << " gpu_samples=" << gpuMilliseconds.size()
                << " draws=" << stats.drawCalls
                << " triangles=" << stats.triangles
                << " changed_pixels=" << changed
                << " total_pixels=" << width * height << '\n';
        }
    }
    return passed;
}

bool validateFrontendMenus(gdp::game::visual::GdpRenderer& renderer)
{
    using namespace gdp::game;
    using namespace gdp::game::visual;
    bool passed = true;
    const char* captureDirectory = std::getenv("GDP_MENU_CAPTURE_DIR");
    std::array<CampaignSlotInfo, 3> slots{};
    gdp::gameplay::CampaignCheckpoint checkpoint;
    checkpoint.floor = 7U; checkpoint.difficulty = gdp::gameplay::Difficulty::Hard;
    slots[1] = {checkpoint, true};
    for (const auto language : {gdp::gameplay::Language::English, gdp::gameplay::Language::PortugueseBrazil}) {
    for (auto dimensions : {std::array{1280, 720}, std::array{960, 540}, std::array{640, 360}, std::array{320, 200}}) {
        const int width = dimensions[0], height = dimensions[1];
        for (auto page : {MenuPage::Root, MenuPage::CampaignSlots, MenuPage::CampaignSlot,
                 MenuPage::Difficulty, MenuPage::Overwrite, MenuPage::Speedrun,
                 MenuPage::Leaderboard, MenuPage::Settings, MenuPage::Display,
                 MenuPage::Audio, MenuPage::Controls, MenuPage::Bindings, MenuPage::MoreBindings}) {
            MenuFlowState state; state.page = page;
            gdp::gameplay::RuntimeSettings settings; settings.language = language;
            const auto view = makeMenuView(state, slots, {}, settings, true);
            HudRenderData hud;
            hud.language = language;
            hud.listMenu.active = true; hud.listMenu.title = view.title; hud.listMenu.subtitle = view.subtitle;
            hud.listMenu.titleScreen = page == MenuPage::Root;
            for (const auto& row : view.rows) {
                hud.listMenu.rows.push_back(row.text); hud.listMenu.enabled.push_back(row.enabled);
                hud.listMenu.adjustable.push_back(row.setting.has_value());
            }
            const auto renderBackdrop = [&](const double time) {
                const auto scene = sampleMenuBackdrop(time);
                return renderer.render(width, height, scene.camera, scene.enemies, {}, scene.props,
                    scene.lighting, scene.room, {}, {}, {}, hud);
            };
            passed &= renderBackdrop(0.0);
            std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height * 4));
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            if (captureDirectory) {
                std::vector<std::uint8_t> topDown(pixels.size());
                const auto stride = static_cast<std::size_t>(width * 4);
                for (int row = 0; row < height; ++row) std::copy_n(pixels.data() + static_cast<std::size_t>(height - row - 1) * stride,
                    stride, topDown.data() + static_cast<std::size_t>(row) * stride);
                std::filesystem::create_directories(captureDirectory);
                const auto path = std::filesystem::path{captureDirectory} / (std::string{gdp::gameplay::languageName(language)} + "-menu-" + std::to_string(static_cast<unsigned>(page))
                    + "-" + std::to_string(width) + "x" + std::to_string(height) + ".bmp");
                auto* surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, topDown.data(), width * 4);
                passed &= surface && SDL_SaveBMP(surface, path.string().c_str());
                SDL_DestroySurface(surface);
            }
            hud.listMenu.selection = view.rows.size() - 1U;
            passed &= renderBackdrop(0.0);
            std::vector<std::uint8_t> changed(pixels.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, changed.data());
            passed &= pixels != changed;
            if (page == MenuPage::Root) {
                passed &= renderBackdrop(8.0);
                std::vector<std::uint8_t> animated(pixels.size());
                glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, animated.data());
                std::size_t differences = 0U;
                for (int y = 0; y < height; ++y) for (int x = width * 3 / 4; x < width; ++x) {
                    const auto index = static_cast<std::size_t>((y * width + x) * 4);
                    differences += animated[index] != changed[index];
                }
                passed &= differences > 100U;
            }
        }
    }
    }
    if (!passed) std::cerr << "Frontend menu pixel render failed\n";
    return passed;
}

bool validateTacticalHud(gdp::game::visual::GdpRenderer& renderer)
{
    using namespace gdp::game::visual;
    bool passed = true;
    const char* capture = std::getenv("GDP_TACTICAL_CAPTURE_DIR");
    for (const auto language : {gdp::gameplay::Language::English, gdp::gameplay::Language::PortugueseBrazil}) {
    for (auto dimensions : {std::array{1280, 720}, std::array{960, 540},
             std::array{960, 600}, std::array{640, 360}, std::array{320, 200}}) {
        const int width = dimensions[0], height = dimensions[1];
        HudRenderData hud;
        hud.language = language;
        hud.visible = true;
        hud.health = 100.0F; hud.maximumHealth = 115.0F;
        hud.armor = 40.0F; hud.magazineAmmo = 24U; hud.reserveAmmo = 96U;
        hud.weapon = HudWeaponRole::Carbine; hud.level = 3U;
        hud.experience = 80U; hud.experienceForNextLevel = 175U;
        const auto pixelsFor = [&](const HudRenderData& state) {
            passed &= renderer.render(width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, state);
            std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height * 4));
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            return pixels;
        };
        const auto original = pixelsFor(hud);
        hud.health = 12.0F;
        const auto damaged = pixelsFor(hud);
        hud.health = 100.0F; hud.magazineAmmo = 0U;
        const auto empty = pixelsFor(hud);
        const auto differences = [&](const auto& other, int firstX, int lastX) {
            std::size_t count = 0;
            for (int y = 0; y < height; ++y) for (int x = firstX; x < lastX; ++x) {
                const auto offset = static_cast<std::size_t>((y * width + x) * 4);
                if (!std::equal(original.begin() + static_cast<std::ptrdiff_t>(offset),
                        original.begin() + static_cast<std::ptrdiff_t>(offset + 3U),
                        other.begin() + static_cast<std::ptrdiff_t>(offset))) ++count;
            }
            return count;
        };
        passed &= differences(damaged, 0, width / 2) > 0U
            && differences(damaged, width / 2, width) == 0U;
        passed &= differences(empty, 0, width / 2) == 0U
            && differences(empty, width / 2, width) > 0U;
        hud.weapon = HudWeaponRole::Knife;
        passed &= pixelsFor(hud) != empty;
        for (const auto role : {HudWeaponRole::Sniper, HudWeaponRole::Shotgun, HudWeaponRole::SubmachineGun}) {
            hud.weapon = role;
            passed &= pixelsFor(hud) != empty;
            // All approved firearms have their own mesh.
            WeaponViewmodelRenderData model;
            model.visible = true; model.role = role;
            passed &= renderer.render(width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, hud, {}, model);
        }
        hud.grenadeSelected = true;
        hud.grenadePriming = true;
        hud.grenades = 1U;
        hud.grenadeFuseRemainingSeconds = gdp::gameplay::grenadeFuseSeconds;
        const auto freshFuse = pixelsFor(hud);
        hud.grenadeFuseRemainingSeconds = 0.75F;
        const auto urgentFuse = pixelsFor(hud);
        hud.grenadePriming = false;
        const auto releasedFuse = pixelsFor(hud);
        passed &= freshFuse != urgentFuse && urgentFuse != releasedFuse;
        if (capture) {
            std::filesystem::create_directories(capture);
            const auto save = [&](const auto& pixels, const std::string_view state) {
                std::vector<std::uint8_t> topDown(pixels.size());
                const auto stride = static_cast<std::size_t>(width * 4);
                for (int y = 0; y < height; ++y) std::copy_n(pixels.data()
                    + static_cast<std::size_t>(height - y - 1) * stride, stride,
                    topDown.data() + static_cast<std::size_t>(y) * stride);
                const auto path = std::filesystem::path{capture}
                    / (std::string{gdp::gameplay::languageName(language)}
                        + "-tactical-hud-" + std::string{state} + "-"
                        + std::to_string(width) + "x" + std::to_string(height) + ".bmp");
                auto* surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32,
                    topDown.data(), width * 4);
                const bool saved = surface && SDL_SaveBMP(surface, path.string().c_str());
                SDL_DestroySurface(surface);
                return saved;
            };
            passed &= save(original, "normal") && save(freshFuse, "fuse-full")
                && save(urgentFuse, "fuse-urgent");
        }
    }
    }
    if (!passed) std::cerr << "Tactical HUD resource positioning/feedback failed\n";
    return passed;
}

bool validateBossHud(gdp::game::visual::GdpRenderer& renderer)
{
    using namespace gdp::game::visual;
    const char* const captureDirectory = std::getenv("GDP_BOSS_HUD_CAPTURE_DIR");
    bool passed = true;
    for (const auto language : {gdp::gameplay::Language::English, gdp::gameplay::Language::PortugueseBrazil}) {
    for (const auto dimensions : {std::array{1280, 720},
             std::array{960, 540}, std::array{640, 360}, std::array{320, 200}}) {
        const int width = dimensions[0];
        const int height = dimensions[1];
        HudRenderData hud;
        hud.language = language;
        hud.visible = true;
        hud.objective = ObjectiveRenderState::DefeatBoss;
        hud.magazineAmmo = 8U;
        hud.reserveAmmo = 36U;
        hud.experienceForNextLevel = 100U;
        hud.runTimerVisible = true;
        hud.elapsedRunTicks = 4300U;
        hud.event = HudEventState::Active;
        hud.eventLocked = true;
        hud.map.visible = true;
        hud.map.roomCount = 2U;
        hud.map.rooms[0] = {1U, {0.0F, 0.0F}, true, true, true,
            {-4.0F, -4.0F}, {4.0F, 4.0F}};
        hud.map.rooms[1] = {2U, {0.0F, -12.0F}, true, true, false,
            {-5.0F, -17.0F}, {5.0F, -7.0F}};
        hud.map.linkCount = 1U;
        hud.map.links[0].first = 0U;
        hud.map.links[0].second = 1U;
        hud.map.links[0].known = true;
        hud.map.links[0].pointCount = 2U;
        hud.map.links[0].points[0] = {0.0F, -4.0F};
        hud.map.links[0].points[1] = {0.0F, -7.0F};
        constexpr std::array boss{
            EnemyRenderData{{1.5F, 0.0F, -0.5F}, true, 666U,
                EnemyVisualRole::IndustrialRobot}};
        const auto renderPixels = [&](const HudRenderData& snapshot) {
            passed &= renderer.render(width, height, {}, boss, {}, {}, {},
                {}, {}, {}, {}, snapshot);
            std::vector<std::uint8_t> pixels(
                static_cast<std::size_t>(width * height * 4));
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            return pixels;
        };
        const auto baseline = renderPixels(hud);
        hud.boss = {true, 200.0F, 200.0F, HudBossPhase::LongRange};
        const auto full = renderPixels(hud);
        hud.boss.health = 100.0F;
        const auto half = renderPixels(hud);
        const auto longestWarningRun = [&](const std::vector<std::uint8_t>& pixels) {
            std::array<int, 3> longest{0, 0, 0};
            for (int y = height * 3 / 4; y < height; ++y) {
                int run = 0;
                for (int x = 0; x < width; ++x) {
                    const auto offset = static_cast<std::size_t>((y * width + x) * 4);
                    const bool warning = pixels[offset] > 200U
                        && pixels[offset + 1U] > 80U && pixels[offset + 1U] < 200U
                        && pixels[offset + 2U] < 80U;
                    run = warning ? run + 1 : 0;
                    if (run > longest[0]) {
                        longest = {run, x - run + 1, x};
                    }
                }
            }
            return longest;
        };
        const auto fullBar = longestWarningRun(full);
        const auto halfBar = longestWarningRun(half);
        passed &= fullBar[0] > 40 && std::abs(halfBar[0] * 2 - fullBar[0]) <= 1
            && std::abs(fullBar[1] + fullBar[2] + 1 - width) <= 1;
        // Long Campaign timer text and F3 must not cover or move the central bar.
        HudRenderData campaignHud = hud;
        campaignHud.boss.health = 200.0F;
        campaignHud.campaignMode = true;
        campaignHud.campaignFloor = 2U;
        campaignHud.campaignFloorCount = 3U;
        campaignHud.campaignThreatPercent = 110U;
        passed &= longestWarningRun(renderPixels(campaignHud)) == fullBar;
        campaignHud.performanceOverlay = true;
        passed &= longestWarningRun(renderPixels(campaignHud)) == fullBar;
        hud.boss.phase = HudBossPhase::Pressure;
        const auto pressure = renderPixels(hud);
        hud.boss.health = 32.0F;
        hud.boss.phase = HudBossPhase::Pursuit;
        const auto pursuit = renderPixels(hud);
        passed &= full != baseline && half != full && pressure != half
            && pursuit != pressure;

        if (captureDirectory != nullptr) {
            const auto saveCapture = [&](const std::vector<std::uint8_t>& pixels,
                const std::string& phase) {
                std::vector<std::uint8_t> topDown(pixels.size());
                const std::size_t stride = static_cast<std::size_t>(width * 4);
                for (int row = 0; row < height; ++row) {
                    std::copy_n(pixels.data() + static_cast<std::size_t>(height - row - 1)
                        * stride, stride, topDown.data() + static_cast<std::size_t>(row)
                        * stride);
                }
                const std::filesystem::path directory{captureDirectory};
                std::filesystem::create_directories(directory);
                const auto path = directory / (std::string{gdp::gameplay::languageName(language)} + "-boss-hud-" + std::to_string(width)
                    + "x" + std::to_string(height) + "-" + phase + ".bmp");
                SDL_Surface* const surface = SDL_CreateSurfaceFrom(width, height,
                    SDL_PIXELFORMAT_RGBA32, topDown.data(), width * 4);
                const bool saved = surface != nullptr
                    && SDL_SaveBMP(surface, path.string().c_str());
                SDL_DestroySurface(surface);
                return saved;
            };
            passed &= saveCapture(full, "range") && saveCapture(pressure, "pressure")
                && saveCapture(pursuit, "pursuit");
        }
        hud.boss.visible = false;
        passed &= renderPixels(hud) == baseline;
        hud.objective = ObjectiveRenderState::Victory;
        const auto victory = renderPixels(hud);
        hud.boss.visible = true;
        passed &= renderPixels(hud) == victory;
    }
    }
    if (!passed) {
        std::cerr << "Boss HUD health/phase/visibility rendering failed\n";
    }
    return passed;
}

bool validateCampaignExitHud(gdp::game::visual::GdpRenderer& renderer)
{
    using namespace gdp::game::visual;
    bool passed = true;
    const char* capture = std::getenv("GDP_CAMPAIGN_EXIT_CAPTURE_DIR");
    for (const auto language : {gdp::gameplay::Language::English, gdp::gameplay::Language::PortugueseBrazil}) {
        for (const auto dimensions : {std::array{1280, 720}, std::array{960, 540},
                 std::array{640, 360}, std::array{320, 200}}) {
            const int width = dimensions[0], height = dimensions[1];
            HudRenderData hud;
            hud.language = language;
            hud.visible = true;
            hud.campaignMode = true;
            hud.campaignFloor = 1U;
            hud.campaignFloorCount = 10U;
            hud.runTimerVisible = true;
            hud.objective = ObjectiveRenderState::DefeatBoss;
            hud.prompt = HudPrompt::ExitTerminalLocked;
            hud.map.visible = true;
            hud.map.roomCount = 2U;
            hud.map.rooms[0] = {1U, {0.0F, 0.0F}, true, true, true,
                {-4.0F, -4.0F}, {4.0F, 4.0F}};
            hud.map.rooms[1] = {2U, {0.0F, -12.0F}, true, false, false,
                {-4.0F, -16.0F}, {4.0F, -8.0F}};
            hud.map.linkCount = 1U;
            hud.map.links[0] = {0U, 1U, true};
            hud.map.links[0].pointCount = 2U;
            hud.map.links[0].points[0] = {0.0F, -4.0F};
            hud.map.links[0].points[1] = {0.0F, -8.0F};
            const auto pixelsFor = [&] {
                passed &= renderer.render(width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, hud);
                std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height * 4));
                glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                return pixels;
            };
            const auto locked = pixelsFor();
            hud.objective = ObjectiveRenderState::SecureArena;
            hud.prompt = HudPrompt::CampaignMissionRequired;
            hud.map.rooms[1].marker = HudMapMarker::Mission;
            const auto arenaMission = pixelsFor();
            hud.objective = ObjectiveRenderState::RecoverKey;
            const auto keyMission = pixelsFor();
            hud.objective = ObjectiveRenderState::ActivateTerminal;
            const auto terminalActivation = pixelsFor();
            hud.objective = ObjectiveRenderState::ClearAmbush;
            const auto terminalClear = pixelsFor();
            hud.objective = ObjectiveRenderState::ActivateExit;
            hud.prompt = HudPrompt::NextFloorTerminal;
            hud.map.rooms[1].marker = HudMapMarker::Exit;
            const auto ready = pixelsFor();
            hud.campaignFloor = 10U;
            hud.prompt = HudPrompt::FinishCampaignTerminal;
            const auto final = pixelsFor();
            hud.infoOverlay = true;
            hud.campaignFloor = 2U;
            hud.campaignMission = ObjectiveRenderState::ActivateTerminal;
            hud.objective = ObjectiveRenderState::ActivateTerminal;
            hud.prompt = HudPrompt::CampaignMissionRequired;
            hud.map.rooms[1].marker = HudMapMarker::Mission;
            const auto missionPending = pixelsFor();
            hud.campaignRouteMission = true;
            const auto routeNone = pixelsFor();
            hud.infoOverlay = false;
            const auto routeHiddenNone = pixelsFor();
            hud.campaignKeyRecovered = true;
            hud.campaignArenaSecured = true;
            const auto routeHiddenComplete = pixelsFor();
            hud.infoOverlay = true;
            hud.campaignKeyRecovered = false;
            hud.campaignArenaSecured = false;
            hud.campaignKeyRecovered = true;
            const auto routeKeyFirst = pixelsFor();
            hud.campaignKeyRecovered = false;
            hud.campaignArenaSecured = true;
            const auto routeArenaFirst = pixelsFor();
            hud.campaignKeyRecovered = true;
            hud.campaignMissionCompleted = true;
            const auto routeComplete = pixelsFor();
            hud.campaignRouteMission = false;
            hud.campaignKeyRecovered = false;
            hud.campaignArenaSecured = false;
            hud.campaignMissionCompleted = false;
            hud.campaignMission = ObjectiveRenderState::ClearAmbush;
            hud.objective = ObjectiveRenderState::ClearAmbush;
            const auto ambushPending = pixelsFor();
            hud.campaignBossDefeated = true;
            const auto bossFirst = pixelsFor();
            hud.campaignBossDefeated = false;
            hud.campaignMissionCompleted = true;
            const auto missionFirst = pixelsFor();
            hud.campaignBossDefeated = true;
            hud.campaignExitReady = true;
            hud.objective = ObjectiveRenderState::ActivateExit;
            hud.prompt = HudPrompt::NextFloorTerminal;
            hud.map.rooms[1].marker = HudMapMarker::Exit;
            const auto exitChecklist = pixelsFor();
            passed &= locked != arenaMission && arenaMission != keyMission
                && keyMission != terminalActivation
                && terminalActivation != terminalClear
                && terminalClear != ready && ready != final
                && final != missionPending && missionPending != routeNone
                && routeNone != routeKeyFirst && routeNone != routeArenaFirst
                && routeNone != routeComplete && routeKeyFirst != routeArenaFirst
                && routeKeyFirst != routeComplete && routeArenaFirst != routeComplete
                && routeHiddenNone == routeHiddenComplete
                && routeComplete != ambushPending
                && ambushPending != bossFirst && bossFirst != missionFirst
                && missionFirst != exitChecklist;
            if (capture) {
                const auto save = [&](const auto& pixels, const std::string& state) {
                    std::vector<std::uint8_t> topDown(pixels.size());
                    const auto stride = static_cast<std::size_t>(width * 4);
                    for (int row = 0; row < height; ++row) std::copy_n(pixels.data()
                        + static_cast<std::size_t>(height - row - 1) * stride, stride,
                        topDown.data() + static_cast<std::size_t>(row) * stride);
                    std::filesystem::create_directories(capture);
                    const auto path = std::filesystem::path{capture}
                        / (std::string{gdp::gameplay::languageName(language)} + "-exit-" + state
                            + "-" + std::to_string(width) + "x" + std::to_string(height) + ".bmp");
                    auto* surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32,
                        topDown.data(), width * 4);
                    const bool saved = surface && SDL_SaveBMP(surface, path.string().c_str());
                    SDL_DestroySurface(surface);
                    return saved;
                };
                passed &= save(locked, "locked") && save(arenaMission, "arena-mission")
                    && save(keyMission, "key-mission")
                    && save(terminalActivation, "terminal-activation")
                    && save(terminalClear, "terminal-clear") && save(ready, "ready")
                    && save(final, "final")
                    && save(missionPending, "mission-pending-tab")
                    && save(routeNone, "route-0-of-2-tab")
                    && save(routeKeyFirst, "route-key-1-of-2-tab")
                    && save(routeArenaFirst, "route-arena-1-of-2-tab")
                    && save(routeComplete, "route-2-of-2-tab")
                    && save(ambushPending, "ambush-pending-tab")
                    && save(bossFirst, "boss-first-tab")
                    && save(missionFirst, "mission-first-tab")
                    && save(exitChecklist, "exit-ready-tab");
            }
        }
    }
    if (!passed) std::cerr << "Campaign exit HUD rendering failed\n";
    return passed;
}

bool validateNewWeaponVisuals(gdp::game::visual::GdpRenderer& renderer)
{
    using namespace gdp::game::visual;
    const char* capture = std::getenv("GDP_NEW_WEAPON_CAPTURE_DIR");
    const int width = capture ? 1280 : 320;
    const int height = capture ? 720 : 200;
    bool passed = true;
    const auto readFrame = [&] {
        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height * 4));
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        return pixels;
    };
    const auto save = [&](const std::vector<std::uint8_t>& pixels, const std::string& name) {
        if (!capture) return true;
        std::filesystem::create_directories(capture);
        std::vector<std::uint8_t> topDown(pixels.size());
        const auto stride = static_cast<std::size_t>(width * 4);
        for (int y = 0; y < height; ++y) std::copy_n(
            pixels.data() + static_cast<std::size_t>(height - y - 1) * stride,
            stride, topDown.data() + static_cast<std::size_t>(y) * stride);
        auto* surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32,
            topDown.data(), width * 4);
        const auto path = std::filesystem::path{capture} / (name + ".bmp");
        const bool saved = surface && SDL_SaveBMP(surface, path.string().c_str());
        SDL_DestroySurface(surface);
        return saved;
    };
    for (const auto role : {HudWeaponRole::Sniper, HudWeaponRole::Shotgun,
             HudWeaponRole::SubmachineGun}) {
        const std::string name = role == HudWeaponRole::Sniper ? "sniper"
            : role == HudWeaponRole::Shotgun ? "shotgun" : "smg";
        CameraData camera;
        WeaponViewmodelRenderData model{.visible = true, .role = role, .visualSeed = 1717U};
        for (const bool potato : {false, true}) {
            HudRenderData quality;
            quality.detailedMaterials = !potato;
            quality.worldRenderScalePercent = potato ? 50U : 100U;
            quality.graphicsQuality = potato ? HudGraphicsQuality::Potato : HudGraphicsQuality::Medium;
            passed &= renderer.render(width, height, camera, {}, {}, {}, {}, {}, {}, {}, {}, quality);
            const auto background = readFrame();
            passed &= renderer.render(width, height, camera, {}, {}, {}, {}, {}, {}, {}, {}, quality, {}, model);
            const auto actual = readFrame();
            const auto stats = renderer.lastFrameStats();
            passed &= actual != background && stats.drawCalls > 0U && stats.lowSpecShader == potato;
            passed &= save(actual, name + (potato ? "-potato" : "-gameplay"));
            model.role = HudWeaponRole::Carbine;
            passed &= renderer.render(width, height, camera, {}, {}, {}, {}, {}, {}, {}, {}, quality, {}, model);
            passed &= actual != readFrame();
            model.role = role;
        }
        const std::array pickup{PickupRenderData{{0.0F, 0.0F, 0.0F},
            role == HudWeaponRole::Sniper ? PickupVisualRole::DroppedSniper
                : role == HudWeaponRole::Shotgun ? PickupVisualRole::DroppedShotgun
                    : PickupVisualRole::DroppedSubmachineGun,
            123U, 1.0F, role == HudWeaponRole::SubmachineGun ? 120.0F : 30.0F}};
        camera.position = {1.2F, 1.0F, 1.8F};
        camera.forward = wkc::core::normalized(wkc::core::Vec3{-1.2F, -0.9F, -1.8F});
        passed &= renderer.render(width, height, camera);
        const auto floor = readFrame();
        passed &= renderer.render(width, height, camera, {}, pickup);
        const auto drop = readFrame();
        passed &= floor != drop && save(drop, name + "-ground");
    }
    if (!passed) std::cerr << "Approved new weapon silhouettes failed pixel validation\n";
    return passed;
}

bool validateSniperScope(gdp::game::visual::GdpRenderer& renderer)
{
    using namespace gdp::game::visual;
    const char* capture = std::getenv("GDP_SNIPER_SCOPE_CAPTURE_DIR");
    bool passed = true;
    const std::vector<std::array<int, 2>> sizes = capture
        ? std::vector<std::array<int, 2>>{{1280, 720}, {960, 600}, {320, 200}}
        : std::vector<std::array<int, 2>>{{320, 200}, {384, 240}, {480, 200}, {200, 320}};
    for (const auto dimensions : sizes) for (const bool potato : {false, true})
    for (const float fov : {60.0F, 100.0F}) {
        const int width = dimensions[0], height = dimensions[1];
        const int cx = width / 2, cy = height / 2;
        const float radius = 0.44F * static_cast<float>(std::min(width, height));
        HudRenderData hud;
        hud.worldRenderScalePercent = potato ? 50U : 100U;
        hud.detailedMaterials = !potato;
        hud.graphicsQuality = potato ? HudGraphicsQuality::Potato : HudGraphicsQuality::Medium;
        CameraData camera;
        camera.verticalFieldOfViewDegrees = fov * 0.35F;
        camera.position = {1.8F, 1.62F, 4.0F};
        camera.forward = wkc::core::normalized(wkc::core::Vec3{0.0F, -0.05F, -1.0F});
        WeaponViewmodelRenderData model{.visible = true, .role = HudWeaponRole::Sniper,
            .visualSeed = 1717U, .aimStrength = 1.0F};
        const auto read = [&] {
            std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height * 4));
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            return pixels;
        };
        const auto frame = [&](const WeaponViewmodelRenderData& weapon, const HudRenderData& ui) {
            passed &= renderer.render(width, height, camera, {}, {}, {}, {}, {}, {}, {}, {}, ui, {}, weapon);
            return read();
        };
        const auto reference = frame({}, hud);
        const auto worldStats = renderer.lastFrameStats();
        const auto scope = frame(model, hud);
        const auto scopedStats = renderer.lastFrameStats();
        // No additional world rendering; viewmodel is absent at complete aim.
        passed &= scopedStats.drawCalls == worldStats.drawCalls
            && scopedStats.triangles == worldStats.triangles;
        const auto offset = [&](int x, int y) { return static_cast<std::size_t>((y * width + x) * 4); };
        const auto isDark = [&](int x, int y) {
            const auto i = offset(x, y);
            return scope[i] <= 3U && scope[i + 1U] <= 3U && scope[i + 2U] <= 3U;
        };
        passed &= isDark(2, 2) && isDark(width - 3, height - 3);
        // Equal radius along both axes, including portrait and ultrawide fixtures.
        const int inner = static_cast<int>(radius) - 10;
        const int outer = static_cast<int>(radius) + 5;
        for (const auto xy : {std::array{cx + inner, cy + 8}, std::array{cx + 8, cy + inner},
                 std::array{cx - inner, cy - 8}, std::array{cx - 8, cy - inner}}) {
            const auto i = offset(xy[0], xy[1]);
            passed &= std::equal(scope.begin() + static_cast<std::ptrdiff_t>(i),
                scope.begin() + static_cast<std::ptrdiff_t>(i + 3U),
                reference.begin() + static_cast<std::ptrdiff_t>(i));
        }
        passed &= isDark(cx + outer, cy + 4) && isDark(cx + 4, cy + outer);
        // White standard crosshair is not also painted over the optical reticle.
        passed &= isDark(cx + 8, cy) && isDark(cx, cy + 8);
        const auto center = offset(cx, cy);
        passed &= scope[center] > 160U && scope[center + 1U] > 160U;

        model.aimStrength = 0.5F;
        const auto partial = frame(model, hud);
        passed &= partial != scope && partial != reference;
        model.aimStrength = 0.0F;
        const auto hip = frame(model, hud);
        // Hip-fire sniper has no standard crosshair. Compare the center to
        // the same world with no viewmodel and an explicitly selected sniper.
        auto sniperHud = hud;
        sniperHud.weapon = HudWeaponRole::Sniper;
        const auto noHipReticle = frame({}, sniperHud);
        const auto sameCenter = [&](const std::vector<std::uint8_t>& a,
                                    const std::vector<std::uint8_t>& b) {
            for (int y = cy - 8; y < cy + 8; ++y) for (int x = cx - 8; x < cx + 8; ++x) {
                const auto i = offset(x, y);
                if (!std::equal(a.begin() + static_cast<std::ptrdiff_t>(i),
                        a.begin() + static_cast<std::ptrdiff_t>(i + 3U),
                        b.begin() + static_cast<std::ptrdiff_t>(i))) return false;
            }
            return true;
        };
        passed &= sameCenter(hip, noHipReticle) && !sameCenter(hip, reference);
        model.aimStrength = 1.0F;
        model.reloadProgress = 0.4F;
        const auto reload = frame(model, hud);
        model.aimStrength = 0.0F;
        passed &= reload == frame(model, hud);
        model.reloadProgress = 0.0F;
        model.aimStrength = 1.0F;
        model.equipStrength = 0.4F;
        const auto equip = frame(model, hud);
        model.aimStrength = 0.0F;
        passed &= equip == frame(model, hud);
        model.equipStrength = 0.0F;
        model.aimStrength = 1.0F;
        for (int menu = 0; menu < 7; ++menu) {
            auto ui = hud;
            if (menu == 0) ui.paused = true;
            if (menu == 1) ui.mainMenu = true;
            if (menu == 2) ui.settingsMenu = true;
            if (menu == 3) ui.controlsMenu = true;
            if (menu == 4) ui.listMenu.active = true;
            if (menu == 5) ui.objective = ObjectiveRenderState::Victory;
            if (menu == 6) ui.objective = ObjectiveRenderState::Defeat;
            const auto disabled = frame(model, ui);
            model.aimStrength = 0.0F;
            passed &= disabled == frame(model, ui);
            model.aimStrength = 1.0F;
        }
        for (const auto role : {HudWeaponRole::Pistol, HudWeaponRole::Carbine,
                 HudWeaponRole::SubmachineGun, HudWeaponRole::Shotgun, HudWeaponRole::Knife}) {
            model.role = role;
            model.aimStrength = 1.0F;
            const auto other = frame(model, hud);
            model.aimStrength = 0.0F;
            passed &= other == frame(model, hud) && sameCenter(other, reference);
        }
        model.role = HudWeaponRole::Sniper;
        // Return to aim, then release: no stale shader mode left on UI or next frame.
        model.aimStrength = 1.0F;
        passed &= scope == frame(model, hud);
        model.aimStrength = 0.0F;
        passed &= hip == frame(model, hud);
        if (capture) {
            std::filesystem::create_directories(capture);
            const auto save = [&](const std::vector<std::uint8_t>& pixels, const std::string& suffix) {
                std::vector<std::uint8_t> topDown(pixels.size());
                const auto stride = static_cast<std::size_t>(width * 4);
                for (int y = 0; y < height; ++y) std::copy_n(pixels.data()
                    + static_cast<std::size_t>(height - y - 1) * stride, stride,
                    topDown.data() + static_cast<std::size_t>(y) * stride);
                auto* surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32,
                    topDown.data(), width * 4);
                const auto path = std::filesystem::path{capture}
                    / ("sniper-scope-v2-" + std::to_string(width) + "x" + std::to_string(height)
                        + (potato ? "-potato" : "-medium") + "-fov" + std::to_string(int(fov))
                        + "-" + suffix + ".bmp");
                const bool saved = surface && SDL_SaveBMP(surface, path.string().c_str());
                SDL_DestroySurface(surface);
                return saved;
            };
            passed &= save(scope, "aim") && save(hip, "hip");
            hud.visible = true; hud.weapon = HudWeaponRole::Sniper;
            hud.magazineAmmo = 5U; hud.reserveAmmo = 3U;
            model.aimStrength = 1.0F;
            passed &= save(frame(model, hud), "hud");
        }
        if (!passed) {
            std::cerr << "Sniper scope failed: " << width << 'x' << height
                << " potato=" << potato << " fov=" << fov << '\n';
            return false;
        }
    }
    return passed;
}

} // namespace

int main(const int argc, char** argv)
{
    const bool reloadOnly = argc == 2 && std::string{argv[1]} == "--reload-only";
    const bool scopeOnly = argc == 2 && std::string{argv[1]} == "--scope-only";
    const bool bodyOnly = argc == 2 && std::string{argv[1]} == "--player-body-only";
    const bool handsOnly = argc == 2 && std::string{argv[1]} == "--player-hands-only";
    const bool campaignHudOnly = argc == 2
        && std::string{argv[1]} == "--campaign-hud-only";
    const bool tacticalHudOnly = argc == 2
        && std::string{argv[1]} == "--tactical-hud-only";
    const bool grenadeBlastProfile = argc == 2
        && std::string{argv[1]} == "--grenade-blast-profile";
    const bool grenadeSmokeCapture = argc == 2
        && std::string{argv[1]} == "--grenade-smoke-capture";
    const bool workerLimbCapture = argc == 2
        && std::string{argv[1]} == "--worker-limb-capture";
    const bool enemyReferenceCapture = argc == 2
        && std::string{argv[1]} == "--enemy-reference-capture";
    // Smallest supported interface fixture: enough room for map and corner resources.
    // A 160x100 buffer intentionally collapses the minimap to avoid covering the HUD.
    constexpr int width = 320;
    constexpr int height = 200;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }

    if (!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3)
        || !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3)
        || !SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE)
        || !SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24)) {
        std::cerr << "SDL_GL_SetAttribute failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    SDL_Window* const window = SDL_CreateWindow(
        "GDP renderer test",
        1280,
        720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (window == nullptr) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (context == nullptr || !SDL_GL_MakeCurrent(window, context)) {
        std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << '\n';
        if (context != nullptr) {
            SDL_GL_DestroyContext(context);
        }
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool passed = false;
    {
        gdp::game::visual::GdpRenderer renderer;
        constexpr std::array enemies{
            gdp::game::visual::EnemyRenderData{{1.5F, 0.0F, -0.5F}, true, 111},
            gdp::game::visual::EnemyRenderData{{-1.5F, 0.0F, -1.0F}, false, 222},
            gdp::game::visual::EnemyRenderData{
                {0.0F, 0.0F, -2.0F}, true, 333, gdp::game::visual::EnemyVisualRole::Guard},
            gdp::game::visual::EnemyRenderData{
                {0.0F, 0.0F, 0.5F}, true, 444, gdp::game::visual::EnemyVisualRole::Officer},
            gdp::game::visual::EnemyRenderData{
                {-0.5F, 0.0F, 1.0F}, true, 555, gdp::game::visual::EnemyVisualRole::Engineer},
            gdp::game::visual::EnemyRenderData{
                {0.75F, 0.0F, 1.0F}, true, 666,
                gdp::game::visual::EnemyVisualRole::IndustrialRobot},
            gdp::game::visual::EnemyRenderData{
                {-0.75F, 0.0F, 0.5F}, true, 111,
                gdp::game::visual::EnemyVisualRole::Worker, false},
        };
        constexpr std::array workerOnly{
            gdp::game::visual::EnemyRenderData{{1.5F, 0.0F, -0.5F}, true, 111},
        };
        constexpr std::array animatedWorkerOnly{
            gdp::game::visual::EnemyRenderData{
                {1.5F, 0.0F, -0.5F}, true, 111,
                gdp::game::visual::EnemyVisualRole::Worker, true,
                gdp::game::visual::EnemyAnimationState::Attack, 15U},
        };
        constexpr std::array hitWorkerOnly{
            gdp::game::visual::EnemyRenderData{
                {1.5F, 0.0F, -0.5F}, true, 111,
                gdp::game::visual::EnemyVisualRole::Worker, true,
                gdp::game::visual::EnemyAnimationState::Hit, 15U, 0.0F, 1.0F},
        };
        constexpr std::array fallenWorkerOnly{
            gdp::game::visual::EnemyRenderData{
                {1.5F, 0.0F, -0.5F}, false, 111,
                gdp::game::visual::EnemyVisualRole::Worker, true,
                gdp::game::visual::EnemyAnimationState::Dead, 15U},
        };
        constexpr std::array fallingWorkerOnly{
            gdp::game::visual::EnemyRenderData{
                {1.5F, 0.0F, -0.5F}, false, 111,
                gdp::game::visual::EnemyVisualRole::Worker, true,
                gdp::game::visual::EnemyAnimationState::Dead, 15U, 0.0F, 0.0F,
                gdp::game::visual::EnemyAnimationState::Attack, 0.5F},
        };
        constexpr std::array rotatedWorkerOnly{
            gdp::game::visual::EnemyRenderData{
                {1.5F, 0.0F, -0.5F}, true, 111,
                gdp::game::visual::EnemyVisualRole::Worker, true,
                gdp::game::visual::EnemyAnimationState::Idle, 0U, 90.0F},
        };
        constexpr std::array guardOnly{
            gdp::game::visual::EnemyRenderData{
                {1.5F, 0.0F, -0.5F}, true, 333, gdp::game::visual::EnemyVisualRole::Guard},
        };
        constexpr std::array officerOnly{
            gdp::game::visual::EnemyRenderData{
                {1.5F, 0.0F, -0.5F}, true, 444, gdp::game::visual::EnemyVisualRole::Officer},
        };
        constexpr std::array engineerOnly{
            gdp::game::visual::EnemyRenderData{
                {1.5F, 0.0F, -0.5F}, true, 555, gdp::game::visual::EnemyVisualRole::Engineer},
        };
        constexpr std::array industrialRobotOnly{
            gdp::game::visual::EnemyRenderData{
                {1.5F, 0.0F, -0.5F}, true, 666,
                gdp::game::visual::EnemyVisualRole::IndustrialRobot},
        };
        constexpr std::array pickups{
            gdp::game::visual::PickupRenderData{
                {-0.60F, 0.0F, -0.5F}, gdp::game::visual::PickupVisualRole::Health},
            gdp::game::visual::PickupRenderData{
                {0.0F, 0.0F, -0.5F}, gdp::game::visual::PickupVisualRole::Ammo},
            gdp::game::visual::PickupRenderData{
                {0.60F, 0.0F, -0.5F}, gdp::game::visual::PickupVisualRole::Key},
            gdp::game::visual::PickupRenderData{
                {1.20F, 0.0F, -0.5F}, gdp::game::visual::PickupVisualRole::Armor},
            gdp::game::visual::PickupRenderData{
                {1.80F, 0.0F, -0.5F}, gdp::game::visual::PickupVisualRole::Weapon},
            gdp::game::visual::PickupRenderData{
                {2.30F, 0.0F, -0.5F}, gdp::game::visual::PickupVisualRole::Special},
        };
        constexpr std::array props{
            gdp::game::visual::PropRenderData{
                {-1.35F, 0.0F, -1.2F}, {0.78F, 0.72F, 0.78F},
                gdp::game::visual::PropVisualRole::Crate,
                gdp::game::visual::PropVisualState::Intact, 7001},
            gdp::game::visual::PropRenderData{
                {0.0F, 0.0F, -1.2F}, {0.46F, 1.02F, 0.46F},
                gdp::game::visual::PropVisualRole::Canister,
                gdp::game::visual::PropVisualState::Damaged, 7002},
            gdp::game::visual::PropRenderData{
                {1.35F, 0.0F, -1.2F}, {1.16F, 0.92F, 0.34F},
                gdp::game::visual::PropVisualRole::Barricade,
                gdp::game::visual::PropVisualState::Destroyed, 7003},
        };
        gdp::game::visual::RoomLightingRenderData proceduralLighting;
        proceduralLighting.ambientColor = {0.52F, 0.68F, 0.82F};
        proceduralLighting.ambientIntensity = 0.18F;
        proceduralLighting.lightCount = 8U;
        proceduralLighting.fixtureEmissive = 0.84F;
        proceduralLighting.lights[0] = {
            {-2.0F, 3.82F, -2.0F}, {0.48F, 0.74F, 1.0F}, 1.05F, 5.4F};
        proceduralLighting.lights[1] = {
            {2.0F, 3.82F, -2.0F}, {0.60F, 0.82F, 1.0F}, 0.95F, 5.8F};
        proceduralLighting.lights[2] = {
            {0.0F, 3.82F, 2.0F}, {0.42F, 0.68F, 0.94F}, 1.10F, 5.2F};
        proceduralLighting.lights[3] = {
            {-3.0F, 3.82F, 2.0F}, {0.72F, 0.80F, 1.0F}, 0.72F, 4.8F};
        proceduralLighting.lights[4] = {
            {3.0F, 3.82F, 2.0F}, {0.72F, 0.80F, 1.0F}, 0.72F, 4.8F};
        proceduralLighting.lights[5] = {
            {-2.5F, 3.82F, -5.0F}, {0.55F, 0.72F, 0.96F}, 0.82F, 5.1F};
        proceduralLighting.lights[6] = {
            {2.5F, 3.82F, -5.0F}, {0.55F, 0.72F, 0.96F}, 0.82F, 5.1F};
        proceduralLighting.lights[7] = {
            {0.0F, 3.82F, -7.0F}, {0.48F, 0.66F, 0.92F}, 0.88F, 5.4F};
        if (!renderer.initialize(width, height) || !renderer.render(width, height)) {
            std::cerr << "Renderer failed to produce a frame\n";
        } else if (grenadeBlastProfile) {
            passed = profileGrenadeBlast(renderer);
        } else if (grenadeSmokeCapture) {
            passed = captureGrenadeSmoke(renderer);
        } else if (workerLimbCapture) {
            passed = captureWorkerLimbVariation(renderer);
        } else if (enemyReferenceCapture) {
            passed = captureEnemyReferenceModels(renderer);
        } else if (bodyOnly) {
            passed = validatePlayerBody(renderer);
        } else if (handsOnly) {
            passed = validatePlayerHands(renderer);
        } else if (campaignHudOnly) {
            passed = validateCampaignExitHud(renderer);
        } else if (tacticalHudOnly) {
            passed = validateTacticalHud(renderer);
        } else if (reloadOnly) {
            passed = validateWeaponReloads(renderer);
        } else if (scopeOnly) {
            passed = validateSniperScope(renderer) && validateNewWeaponVisuals(renderer);
        } else {
            std::vector<std::uint8_t> baseline(
                static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U);
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, baseline.data());

            const bool renderedRoomVariant = renderer.prepareIndustrialRoomMaterials(12345)
                && renderer.render(width, height);
            std::vector<std::uint8_t> roomPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, roomPixels.data());
            const bool repeatedRoom = renderer.prepareIndustrialRoomMaterials(12345)
                && renderer.render(width, height);
            std::vector<std::uint8_t> repeatedRoomPixels(baseline.size());
            glReadPixels(
                0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, repeatedRoomPixels.data());

            const bool renderedStorageVariant = renderer.prepareStorageRoomMaterials(24680)
                && renderer.render(width, height);
            const std::uint64_t storageMaterialKey = renderer.activeRoomMaterialKey();
            std::vector<std::uint8_t> storagePixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, storagePixels.data());

            const bool renderedCombatVariant = renderer.prepareCombatRoomMaterials(13579)
                && renderer.render(width, height);
            std::vector<std::uint8_t> combatPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, combatPixels.data());

            const bool renderedCafeteriaVariant
                = renderer.prepareCafeteriaRoomMaterials(97531)
                && renderer.render(width, height);
            std::vector<std::uint8_t> cafeteriaPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, cafeteriaPixels.data());

            const bool renderedLaboratoryVariant
                = renderer.prepareLaboratoryRoomMaterials(86420)
                && renderer.render(width, height);
            std::vector<std::uint8_t> laboratoryPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, laboratoryPixels.data());

            const bool renderedTechnicalVariant
                = renderer.prepareTechnicalRoomMaterials(112233)
                && renderer.render(width, height);
            std::vector<std::uint8_t> technicalPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, technicalPixels.data());

            const bool renderedSecurityVariant
                = renderer.prepareSecurityRoomMaterials(778899)
                && renderer.render(width, height);
            std::vector<std::uint8_t> securityPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, securityPixels.data());

            const bool renderedCentralHubVariant
                = renderer.prepareCentralHubRoomMaterials(424242)
                && renderer.render(width, height);
            const std::uint64_t centralHubMaterialKey = renderer.activeRoomMaterialKey();
            std::vector<std::uint8_t> centralHubPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, centralHubPixels.data());

            const bool renderedProceduralLighting = renderer.render(
                width, height, {}, {}, {}, {}, proceduralLighting);
            std::vector<std::uint8_t> lightingPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                lightingPixels.data());
            gdp::game::visual::RoomLightingRenderData flashlightLighting
                = proceduralLighting;
            flashlightLighting.flashlightEnabled = true;
            flashlightLighting.flashlightPosition = {0.0F, 1.62F, 4.0F};
            flashlightLighting.flashlightDirection = {0.0F, 0.0F, -1.0F};
            const bool renderedFlashlight = renderer.render(
                width, height, {}, {}, {}, {}, flashlightLighting);
            std::vector<std::uint8_t> flashlightPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                flashlightPixels.data());
            gdp::game::visual::RoomLightingRenderData darkLighting
                = proceduralLighting;
            darkLighting.lightCount = 0U;
            darkLighting.ambientIntensity = 0.05F;
            const bool renderedDarkReference = renderer.render(
                width, height, {}, {}, {}, {}, darkLighting);
            std::vector<std::uint8_t> darkPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                darkPixels.data());
            darkLighting.flashlightEnabled = true;
            darkLighting.flashlightPosition = {0.0F, 1.62F, 4.0F};
            darkLighting.flashlightDirection = {0.0F, 0.0F, -1.0F};
            const bool renderedDarkFlashlight = renderer.render(
                width, height, {}, {}, {}, {}, darkLighting);
            std::vector<std::uint8_t> darkFlashlightPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                darkFlashlightPixels.data());
            std::size_t darkBeamPixels = 0U;
            for (std::size_t index = 0U; index < darkPixels.size(); index += 4U) {
                if (static_cast<int>(darkFlashlightPixels[index])
                        - static_cast<int>(darkPixels[index]) >= 20) {
                    ++darkBeamPixels;
                }
            }

            gdp::game::visual::HudRenderData potatoQuality;
            potatoQuality.graphicsQuality
                = gdp::game::visual::HudGraphicsQuality::Potato;
            potatoQuality.worldRenderScalePercent = 50U;
            potatoQuality.detailedMaterials = false;
            gdp::game::visual::RoomLightingRenderData potatoLighting
                = proceduralLighting;
            potatoLighting.lightCount = 2U;
            potatoLighting.shadowLightCount = 0U;
            potatoLighting.shadowOccluderCount = 0U;
            const bool renderedPotatoQuality = renderer.render(
                width, height, {}, {}, {}, {}, potatoLighting,
                {}, {}, {}, {}, potatoQuality);
            const gdp::game::visual::RendererFrameStats potatoFrameStats
                = renderer.lastFrameStats();
            std::vector<std::uint8_t> potatoQualityPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                potatoQualityPixels.data());
            gdp::game::visual::RoomLightingRenderData potatoFlashlightLighting
                = potatoLighting;
            potatoFlashlightLighting.flashlightEnabled = true;
            potatoFlashlightLighting.flashlightPosition = {0.0F, 1.62F, 4.0F};
            potatoFlashlightLighting.flashlightDirection = {0.0F, 0.0F, -1.0F};
            const bool renderedPotatoFlashlight = renderer.render(
                width, height, {}, {}, {}, {}, potatoFlashlightLighting,
                {}, {}, {}, {}, potatoQuality);
            std::vector<std::uint8_t> potatoFlashlightPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                potatoFlashlightPixels.data());
            gdp::game::visual::RoomLightingRenderData potatoReferenceLighting
                = potatoLighting;
            potatoReferenceLighting.shadowOccluderCount = 1U;
            const bool renderedPotatoReference = renderer.render(
                width, height, {}, {}, {}, {}, potatoReferenceLighting,
                {}, {}, {}, {}, potatoQuality);
            const gdp::game::visual::RendererFrameStats potatoReferenceStats
                = renderer.lastFrameStats();
            std::vector<std::uint8_t> potatoReferencePixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                potatoReferencePixels.data());

            gdp::game::visual::RoomLightingRenderData damagedFixtureLighting
                = proceduralLighting;
            damagedFixtureLighting.lights[0].intensity *= 0.45F;
            damagedFixtureLighting.lights[0].fixtureState
                = gdp::game::visual::PropVisualState::Damaged;
            damagedFixtureLighting.lights[0].fixtureEmissionScale = 0.45F;
            const bool renderedDamagedFixture = renderer.render(
                width, height, {}, {}, {}, {}, damagedFixtureLighting);
            std::vector<std::uint8_t> damagedFixturePixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                damagedFixturePixels.data());

            gdp::game::visual::RoomLightingRenderData destroyedFixtureLighting
                = damagedFixtureLighting;
            destroyedFixtureLighting.lights[0].intensity = 0.0F;
            destroyedFixtureLighting.lights[0].fixtureState
                = gdp::game::visual::PropVisualState::Destroyed;
            destroyedFixtureLighting.lights[0].fixtureEmissionScale = 0.0F;
            const bool renderedDestroyedFixture = renderer.render(
                width, height, {}, {}, {}, {}, destroyedFixtureLighting);
            std::vector<std::uint8_t> destroyedFixturePixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                destroyedFixturePixels.data());

            gdp::game::visual::RoomLightingRenderData shadowBaselineLighting
                = proceduralLighting;
            shadowBaselineLighting.shadowLightCount = 0U;
            const bool renderedShadowBaseline = renderer.render(
                width, height, {}, {}, {}, {}, shadowBaselineLighting);
            std::vector<std::uint8_t> shadowBaselinePixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                shadowBaselinePixels.data());

            gdp::game::visual::RoomLightingRenderData shadowedLighting
                = shadowBaselineLighting;
            shadowedLighting.shadowLightCount = shadowedLighting.lightCount;
            shadowedLighting.shadowOccluderCount = 1U;
            shadowedLighting.shadowOccluders[0].bounds = {
                {-1.10F, 0.0F, -2.80F}, {1.10F, 0.70F, -1.60F}};
            renderer.setShadowShaderSpecializationEnabled(false);
            const bool renderedFullShadowReference = renderer.render(
                width, height, {}, {}, {}, {}, shadowedLighting);
            const gdp::game::visual::RendererFrameStats fullShadowReferenceStats
                = renderer.lastFrameStats();
            std::vector<std::uint8_t> fullShadowReferencePixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                fullShadowReferencePixels.data());
            renderer.setShadowShaderSpecializationEnabled(true);
            const bool renderedDynamicShadows = renderer.render(
                width, height, {}, {}, {}, {}, shadowedLighting);
            const gdp::game::visual::RendererFrameStats specializedShadowStats
                = renderer.lastFrameStats();
            std::vector<std::uint8_t> shadowedPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                shadowedPixels.data());
            const bool shadowSpecializationPreservedPixels
                = shadowedPixels == fullShadowReferencePixels
                && fullShadowReferenceStats.shadowShaderOccluderCapacity == 8U
                && specializedShadowStats.shadowShaderOccluderCapacity == 3U;

            gdp::game::visual::RoomLightingRenderData clampedShadowLighting
                = shadowedLighting;
            clampedShadowLighting.shadowLightCount
                = static_cast<std::uint32_t>(
                    gdp::game::visual::maxShadowCastingLights);
            const bool renderedClampedShadowBudget = renderer.render(
                width, height, {}, {}, {}, {}, clampedShadowLighting);
            std::vector<std::uint8_t> clampedShadowPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                clampedShadowPixels.data());

            gdp::game::visual::RoomLightingRenderData shadow6Lighting
                = shadowedLighting;
            shadow6Lighting.shadowOccluderCount = 4U;
            for (std::size_t index = 1U; index < 4U; ++index) {
                const float offset = static_cast<float>(index) * 0.25F;
                shadow6Lighting.shadowOccluders[index].bounds = {
                    {-1.10F + offset, 0.0F, -2.80F},
                    {1.10F + offset, 0.70F, -1.60F}};
            }
            const bool renderedShadow6Variant = renderer.render(
                width, height, {}, {}, {}, {}, shadow6Lighting);
            const gdp::game::visual::RendererFrameStats shadow6Stats
                = renderer.lastFrameStats();

            gdp::game::visual::RoomLightingRenderData shadow8Lighting
                = shadow6Lighting;
            shadow8Lighting.shadowOccluderCount = 7U;
            for (std::size_t index = 4U; index < 7U; ++index) {
                const float offset = static_cast<float>(index) * 0.25F;
                shadow8Lighting.shadowOccluders[index].bounds = {
                    {-1.10F + offset, 0.0F, -2.80F},
                    {1.10F + offset, 0.70F, -1.60F}};
            }
            const bool renderedShadow8Variant = renderer.render(
                width, height, {}, {}, {}, {}, shadow8Lighting);
            const gdp::game::visual::RendererFrameStats shadow8Stats
                = renderer.lastFrameStats();

            proceduralLighting.alert = true;
            const bool renderedAlertLighting = renderer.render(
                width, height, {}, {}, {}, {}, proceduralLighting);
            std::vector<std::uint8_t> alertLightingPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                alertLightingPixels.data());

            gdp::game::visual::RoomLightingRenderData translatedLighting;
            translatedLighting.lightCount = 1U;
            translatedLighting.lights[0] = {
                {16.0F, 3.82F, -14.0F}, {0.52F, 0.78F, 1.0F}, 1.10F, 5.6F};
            constexpr gdp::game::visual::CameraData translatedCamera{
                {18.0F, 1.62F, -14.0F}, {1.0F, 0.0F, 0.0F}, 68.0F};
            constexpr gdp::game::visual::RoomRenderData translatedRoom{
                {14.0F, 0.0F, -14.0F}, wkc::core::QuarterTurn::Degrees90, 0U,
                {{9.15F, 0.0F, -17.85F}, {18.85F, 4.0F, -10.15F}}};
            std::array translatedDoors{
                gdp::game::visual::DoorRenderData{
                    {18.85F, 0.0F, -14.0F}, {1.0F, 0.0F, 0.0F},
                    1.2F, 2.2F, false},
            };
            const bool renderedTranslatedDoor = renderer.render(
                width, height, translatedCamera, {}, {}, {}, translatedLighting,
                translatedRoom, translatedDoors);
            std::vector<std::uint8_t> translatedDoorPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                translatedDoorPixels.data());
            translatedDoors.front().locked = true;
            translatedDoors.front().openFraction = 0.0F;
            const bool renderedLockedDoor = renderer.render(
                width, height, translatedCamera, {}, {}, {}, translatedLighting,
                translatedRoom, translatedDoors);
            std::vector<std::uint8_t> lockedDoorPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                lockedDoorPixels.data());
            translatedDoors.front().locked = false;
            translatedDoors.front().openFraction = 0.45F;
            const bool renderedMovingDoor = renderer.render(
                width, height, translatedCamera, {}, {}, {}, translatedLighting,
                translatedRoom, translatedDoors);
            std::vector<std::uint8_t> movingDoorPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                movingDoorPixels.data());

            constexpr std::array corridorDoors{
                gdp::game::visual::DoorRenderData{
                    {0.0F, 0.0F, -4.85F}, {0.0F, 0.0F, -1.0F},
                    1.2F, 2.2F, false},
                gdp::game::visual::DoorRenderData{
                    {0.0F, 0.0F, -10.0F}, {0.0F, 0.0F, 1.0F},
                    1.2F, 2.2F, false},
            };
            constexpr gdp::game::visual::CameraData corridorCamera{
                {0.0F, 1.62F, -7.0F}, {0.0F, 0.0F, -1.0F}, 68.0F};
            const bool renderedOpenDoor = renderer.render(
                width, height, corridorCamera, {}, {}, {}, {}, {}, corridorDoors);
            std::vector<std::uint8_t> openDoorPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                openDoorPixels.data());
            constexpr std::array corridorPoints{
                wkc::core::Vec3{0.0F, 0.0F, -4.85F},
                wkc::core::Vec3{0.0F, 0.0F, -10.0F},
            };
            const std::array corridorGeometry{
                gdp::game::visual::CorridorRenderData{corridorPoints, 1.2F, 2.2F},
            };
            const std::array connectedRoomGeometry{
                gdp::game::visual::RoomRenderData{
                    {0.0F, 0.0F, -14.85F}, wkc::core::QuarterTurn::Degrees0,
                    storageMaterialKey,
                    {{-5.85F, 0.0F, -21.70F}, {5.85F, 5.0F, -10.0F}}},
            };
            renderer.setSurfaceInstancingEnabled(false);
            const bool renderedCorridorReference = renderer.render(
                width, height, corridorCamera, {}, {}, {}, {}, {}, corridorDoors,
                corridorGeometry, connectedRoomGeometry);
            const gdp::game::visual::RendererFrameStats corridorReferenceStats
                = renderer.lastFrameStats();
            std::vector<std::uint8_t> corridorReferencePixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                corridorReferencePixels.data());
            renderer.setSurfaceInstancingEnabled(true);
            const bool renderedCorridor = renderer.render(
                width, height, corridorCamera, {}, {}, {}, {}, {}, corridorDoors,
                corridorGeometry, connectedRoomGeometry);
            const gdp::game::visual::RendererFrameStats corridorInstancedStats
                = renderer.lastFrameStats();
            std::vector<std::uint8_t> corridorPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                corridorPixels.data());
            const auto instancingComparison = compareInstancingPixels(corridorPixels, corridorReferencePixels);
            const bool corridorInstancingPreservedPixels = instancingComparison.equivalent;
            const bool corridorInstancingReducedDraws
                = corridorInstancedStats.drawCalls < corridorReferenceStats.drawCalls
                && corridorInstancedStats.triangles
                    == corridorReferenceStats.triangles
                && corridorInstancedStats.instancedDrawCalls > 0U
                && corridorInstancedStats.renderedInstances
                    > corridorInstancedStats.instancedDrawCalls
                && corridorInstancedStats.avoidedDrawCalls
                    == corridorReferenceStats.drawCalls
                        - corridorInstancedStats.drawCalls;
            const std::array connectedRoomCentralHub{
                gdp::game::visual::RoomRenderData{
                    {0.0F, 0.0F, -14.85F}, wkc::core::QuarterTurn::Degrees0,
                    centralHubMaterialKey,
                    {{-5.85F, 0.0F, -21.70F}, {5.85F, 5.0F, -10.0F}}},
            };
            const bool renderedConnectedRoomMaterial = renderer.render(
                width, height, corridorCamera, {}, {}, {}, {}, {}, corridorDoors,
                corridorGeometry, connectedRoomCentralHub);
            std::vector<std::uint8_t> connectedRoomMaterialPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                connectedRoomMaterialPixels.data());

            constexpr std::array cornerPoints{
                wkc::core::Vec3{0.0F, 0.0F, 0.0F},
                wkc::core::Vec3{4.0F, 0.0F, 0.0F},
                wkc::core::Vec3{4.0F, 0.0F, -5.0F},
            };
            const std::array cornerCorridor{
                gdp::game::visual::CorridorRenderData{
                    cornerPoints, 1.2F, 2.2F, centralHubMaterialKey},
            };
            constexpr std::array cornerDoors{
                gdp::game::visual::DoorRenderData{
                    {0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F},
                    1.2F, 2.2F, false, 1.0F},
                gdp::game::visual::DoorRenderData{
                    {4.0F, 0.0F, -5.0F}, {0.0F, 0.0F, 1.0F},
                    1.2F, 2.2F, false, 1.0F},
            };
            const std::array cornerConnectedRoom{
                gdp::game::visual::RoomRenderData{
                    {4.0F, 0.0F, -9.85F}, wkc::core::QuarterTurn::Degrees0,
                    storageMaterialKey,
                    {{0.15F, 0.0F, -14.70F}, {7.85F, 4.0F, -5.0F}}},
            };
            const gdp::game::visual::RoomRenderData cornerRoom{
                {-4.85F, 0.0F, 0.0F}, wkc::core::QuarterTurn::Degrees90,
                centralHubMaterialKey,
                {{-9.70F, 0.0F, -3.85F}, {0.0F, 4.0F, 3.85F}}};
            constexpr gdp::game::visual::CameraData cornerCamera{
                {-0.35F, 1.62F, 0.0F}, {0.94F, 0.0F, -0.34F}, 68.0F};
            gdp::game::visual::RoomLightingRenderData cornerLighting;
            cornerLighting.ambientColor = {0.62F, 0.72F, 0.82F};
            cornerLighting.ambientIntensity = 0.30F;
            const bool renderedCornerCorridor = renderer.render(
                width, height, cornerCamera, {}, {}, {}, cornerLighting,
                cornerRoom, cornerDoors, cornerCorridor, cornerConnectedRoom);
            std::vector<std::uint8_t> cornerCorridorPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                cornerCorridorPixels.data());

            const bool renderedHud = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {},
                gdp::game::visual::HudRenderData{gdp::game::visual::ObjectiveRenderState::Victory});
            std::vector<std::uint8_t> hudPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                hudPixels.data());

            gdp::game::visual::HudRenderData resultReplayHud{
                gdp::game::visual::ObjectiveRenderState::Victory};
            resultReplayHud.resultMenuSelection
                = gdp::game::visual::HudResultMenuSelection::ReplaySeed;
            const bool renderedResultReplay = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, resultReplayHud);
            std::vector<std::uint8_t> resultReplayPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                resultReplayPixels.data());

            gdp::game::visual::HudRenderData campaignResultHud{
                gdp::game::visual::ObjectiveRenderState::Victory};
            campaignResultHud.campaignMode = true;
            campaignResultHud.campaignFloor = 1U;
            campaignResultHud.campaignFloorCount = 3U;
            campaignResultHud.campaignAdvanceAvailable = true;
            const bool renderedCampaignResult = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {},
                campaignResultHud);
            std::vector<std::uint8_t> campaignResultPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                campaignResultPixels.data());

            gdp::game::visual::HudRenderData resultExitHud{
                gdp::game::visual::ObjectiveRenderState::Victory};
            resultExitHud.resultMenuSelection
                = gdp::game::visual::HudResultMenuSelection::ExitGame;
            const bool renderedResultExit = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, resultExitHud);
            std::vector<std::uint8_t> resultExitPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                resultExitPixels.data());

            const gdp::game::visual::HudRenderData functionalHud{
                .objective = gdp::game::visual::ObjectiveRenderState::ReachBoss,
                .visible = true,
                .health = 24.0F,
                .maximumHealth = 115.0F,
                .armor = 18.0F,
                .maximumArmor = 60.0F,
                .magazineAmmo = 0U,
                .reserveAmmo = 28U,
                .weapon = gdp::game::visual::HudWeaponRole::Carbine,
                .weaponOverdrive = true,
                .keys = 2U,
                .level = 4U,
                .experience = 35U,
                .experienceForNextLevel = 80U,
                .event = gdp::game::visual::HudEventState::Active,
                .eventLocked = true,
                .prompt = gdp::game::visual::HudPrompt::DoorLocked,
                .damageFeedback = 1.0F,
                .hitFeedback = gdp::game::visual::HudHitFeedback::Kill,
                .hitFeedbackStrength = 1.0F,
                .notice = gdp::game::visual::HudNotice::LevelUp,
                .noticeFeedbackStrength = 1.0F,
            };
            const bool renderedFunctionalHud = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, functionalHud);
            std::vector<std::uint8_t> functionalHudPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                functionalHudPixels.data());

            gdp::game::visual::HudRenderData mapReferenceHud = functionalHud;
            mapReferenceHud.objective
                = gdp::game::visual::ObjectiveRenderState::None;
            const bool renderedMapReference = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, mapReferenceHud);
            std::vector<std::uint8_t> mapReferencePixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                mapReferencePixels.data());

            gdp::game::visual::HudRenderData mapHud = mapReferenceHud;
            mapHud.map.visible = true;
            mapHud.map.roomCount = 3U;
            mapHud.map.rooms[0] = {1U, {0.0F, 0.0F}, true, true, true,
                {-4.0F, -4.0F}, {4.0F, 4.0F}};
            mapHud.map.rooms[1] = {2U, {0.0F, -12.0F}, true, false, false,
                {-4.0F, -16.0F}, {4.0F, -8.0F}};
            mapHud.map.rooms[2] = {3U, {12.0F, -12.0F}, false, false, false,
                {8.0F, -16.0F}, {16.0F, -8.0F}};
            mapHud.map.linkCount = 2U;
            mapHud.map.links[0] = {0U, 1U, true};
            mapHud.map.links[0].points[0] = {0.0F, -2.0F};
            mapHud.map.links[0].points[1] = {0.0F, -10.0F};
            mapHud.map.links[0].pointCount = 2U;
            mapHud.map.links[1] = {1U, 2U, false};
            const bool renderedMapHud = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, mapHud);
            std::vector<std::uint8_t> mapHudPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                mapHudPixels.data());

            const auto renderMapMarker = [&](const std::size_t roomIndex,
                                             const gdp::game::visual::HudMapMarker marker) {
                auto marked = mapHud;
                marked.map.rooms[roomIndex].marker = marker;
                const bool rendered = renderer.render(width, height, {}, {}, {},
                    {}, {}, {}, {}, {}, {}, marked);
                std::vector<std::uint8_t> pixels(baseline.size());
                glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                    pixels.data());
                return std::pair{rendered, std::move(pixels)};
            };
            const auto [renderedMissionMap, missionMapPixels]
                = renderMapMarker(1U, gdp::game::visual::HudMapMarker::Mission);
            const auto [renderedBossMap, bossMapPixels]
                = renderMapMarker(1U, gdp::game::visual::HudMapMarker::Boss);
            const auto [renderedExitMap, exitMapPixels]
                = renderMapMarker(1U, gdp::game::visual::HudMapMarker::Exit);
            const auto [renderedHiddenMarkerMap, hiddenMarkerMapPixels]
                = renderMapMarker(2U, gdp::game::visual::HudMapMarker::Mission);
            const bool mapMarkersDistinct = renderedMissionMap && renderedBossMap
                && renderedExitMap && renderedHiddenMarkerMap
                && missionMapPixels != mapHudPixels
                && bossMapPixels != missionMapPixels
                && exitMapPixels != bossMapPixels
                && hiddenMarkerMapPixels == mapHudPixels;

            gdp::game::visual::HudRenderData narrowRoomMapHud = mapHud;
            narrowRoomMapHud.map.rooms[0].minimum.x = -2.0F;
            narrowRoomMapHud.map.rooms[0].maximum.x = 2.0F;
            const bool renderedNarrowRoomMapHud = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, narrowRoomMapHud);
            std::vector<std::uint8_t> narrowRoomMapHudPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                narrowRoomMapHudPixels.data());

            gdp::game::visual::HudRenderData bentMapHud = mapHud;
            bentMapHud.map.links[0].points[1] = {4.0F, -2.0F};
            bentMapHud.map.links[0].points[2] = {4.0F, -10.0F};
            bentMapHud.map.links[0].points[3] = {0.0F, -10.0F};
            bentMapHud.map.links[0].pointCount = 4U;
            const bool renderedBentMapHud = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, bentMapHud);
            std::vector<std::uint8_t> bentMapHudPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                bentMapHudPixels.data());

            mapHud.map.rooms[1].visited = true;
            mapHud.map.rooms[2].known = true;
            mapHud.map.links[1].known = true;
            const bool renderedExploredMapHud = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, mapHud);
            std::vector<std::uint8_t> exploredMapHudPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                exploredMapHudPixels.data());

            gdp::game::visual::HudRenderData infoHud = functionalHud;
            infoHud.infoOverlay = true;
            infoHud.runSeed = 12345U;
            infoHud.score = 2300U;
            infoHud.enemiesKilled = 7U;
            infoHud.eventsCompleted = 2U;
            infoHud.lootCollected = 3U;
            const bool renderedInfoHud = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, infoHud);
            std::vector<std::uint8_t> infoHudPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                infoHudPixels.data());

            infoHud.runSeed = 54321U;
            const bool renderedAlternateSeed = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, infoHud);
            std::vector<std::uint8_t> alternateSeedPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                alternateSeedPixels.data());

            gdp::game::visual::HudRenderData spreadHud = functionalHud;
            spreadHud.weaponSpread = 1.0F;
            const bool renderedSpreadHud = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, spreadHud);
            std::vector<std::uint8_t> spreadHudPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                spreadHudPixels.data());

            gdp::game::visual::HudRenderData weakPointHud = functionalHud;
            weakPointHud.hitFeedback
                = gdp::game::visual::HudHitFeedback::WeakKill;
            const bool renderedWeakPointHud = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, weakPointHud);
            std::vector<std::uint8_t> weakPointHudPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                weakPointHudPixels.data());

            gdp::game::visual::HudRenderData performanceHud = functionalHud;
            performanceHud.performanceOverlay = true;
            performanceHud.graphicsQuality
                = gdp::game::visual::HudGraphicsQuality::Potato;
            performanceHud.worldRenderScalePercent = 50U;
            performanceHud.detailedMaterials = false;
            performanceHud.performanceFramesPerSecond = 58U;
            performanceHud.averageFrameMilliseconds = 17U;
            performanceHud.percentile95FrameMilliseconds = 22U;
            performanceHud.worstFrameMilliseconds = 31U;
            performanceHud.averageUpdateMilliseconds = 2U;
            performanceHud.averageRenderMilliseconds = 7U;
            performanceHud.averagePresentMilliseconds = 8U;
            performanceHud.averageGpuMilliseconds = 6U;
            performanceHud.gpuTimingAvailable = true;
            performanceHud.framesOverBudgetPercent = 15U;
            performanceHud.worldGenerationMilliseconds = 12U;
            performanceHud.assetPreparationMilliseconds = 84U;
            performanceHud.performanceDrawCalls = 93U;
            performanceHud.performanceTriangles = 1'116U;
            performanceHud.performanceVisibleEnemies = 6U;
            performanceHud.performanceVisibleProps = 4U;
            performanceHud.performanceVisibleRooms = 3U;
            performanceHud.performanceActiveLights = 8U;
            performanceHud.performanceShadowLights = 2U;
            performanceHud.performanceShadowOccluders = 8U;
            performanceHud.performanceMeshCacheEntries = 34U;
            performanceHud.performanceTextureCacheEntries = 27U;
            performanceHud.performanceUniformUploads = 180U;
            performanceHud.performanceUniformSkips = 420U;
            performanceHud.performanceTextureUnitActivations = 15U;
            performanceHud.performanceTextureBinds = 28U;
            performanceHud.performanceTextureBindSkips = 90U;
            performanceHud.performanceMeshBinds = 12U;
            performanceHud.performanceMeshBindSkips = 81U;
            performanceHud.performanceLowSpecShader = true;
            performanceHud.performanceBackfaceCulledDraws = 72U;
            performanceHud.performanceCullStateChanges = 4U;
            performanceHud.performanceInstancedDrawCalls = 9U;
            performanceHud.performanceRenderedInstances = 42U;
            performanceHud.performanceAvoidedDrawCalls = 33U;
            performanceHud.performanceShadowShaderOccluderCapacity = 3U;
            const bool renderedPerformanceOverlay = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, performanceHud);
            const gdp::game::visual::RendererFrameStats performanceFrameStats
                = renderer.lastFrameStats();
            const bool measuredGpuFrame
                = performanceFrameStats.gpuTimingAvailable
                && std::isfinite(performanceFrameStats.gpuMilliseconds)
                && performanceFrameStats.gpuMilliseconds >= 0.0;
            std::vector<std::uint8_t> performanceOverlayPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                performanceOverlayPixels.data());

            gdp::game::visual::HudRenderData timedHud = functionalHud;
            timedHud.runTimerVisible = true;
            timedHud.elapsedRunTicks = 4'567U;
            const bool renderedRunTimer = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, timedHud);
            std::vector<std::uint8_t> runTimerPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                runTimerPixels.data());

            gdp::game::visual::HudRenderData campaignTimerHud = timedHud;
            campaignTimerHud.campaignMode = true;
            campaignTimerHud.campaignFloor = 2U;
            campaignTimerHud.campaignFloorCount = 3U;
            campaignTimerHud.campaignThreatPercent = 110U;
            const bool renderedCampaignTimer = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {},
                campaignTimerHud);
            std::vector<std::uint8_t> campaignTimerPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                campaignTimerPixels.data());

            gdp::game::visual::HudRenderData speedrunRecordHud = timedHud;
            speedrunRecordHud.speedrunMode = true;
            speedrunRecordHud.speedrunRecordVisible = true;
            speedrunRecordHud.bestSpeedrunTicks = 4'200U;
            const bool renderedSpeedrunRecord = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, speedrunRecordHud);
            std::vector<std::uint8_t> speedrunRecordPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                speedrunRecordPixels.data());

            gdp::game::visual::HudRenderData pausedHud = functionalHud;
            pausedHud.paused = true;
            pausedHud.pauseSelection = gdp::game::visual::HudPauseSelection::RestartRun;
            const bool renderedPauseMenu = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, pausedHud);
            std::vector<std::uint8_t> pauseMenuPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                pauseMenuPixels.data());

            gdp::game::visual::HudRenderData mainMenuHud;
            mainMenuHud.mainMenu = true;
            const bool renderedMainMenu = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, mainMenuHud);
            std::vector<std::uint8_t> mainMenuPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                mainMenuPixels.data());

            gdp::game::visual::HudRenderData controlsHud;
            controlsHud.controlsMenu = true;
            const bool renderedControls = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, controlsHud);
            std::vector<std::uint8_t> controlsPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                controlsPixels.data());

            gdp::game::visual::HudRenderData mainMenuContinueHud = mainMenuHud;
            mainMenuContinueHud.mainMenuSelection
                = gdp::game::visual::HudMainMenuSelection::ContinueCampaign;
            mainMenuContinueHud.campaignContinueAvailable = true;
            mainMenuContinueHud.campaignContinueFloor = 2U;
            const bool renderedMainMenuContinue = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {},
                mainMenuContinueHud);
            std::vector<std::uint8_t> mainMenuContinuePixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                mainMenuContinuePixels.data());

            gdp::game::visual::HudRenderData mainMenuCampaignHud = mainMenuHud;
            mainMenuCampaignHud.mainMenuSelection
                = gdp::game::visual::HudMainMenuSelection::Campaign;
            const bool renderedMainMenuCampaign = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {},
                mainMenuCampaignHud);
            std::vector<std::uint8_t> mainMenuCampaignPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                mainMenuCampaignPixels.data());

            gdp::game::visual::HudRenderData mainMenuBestSeedHud = mainMenuHud;
            mainMenuBestSeedHud.mainMenuSelection
                = gdp::game::visual::HudMainMenuSelection::BestSeed;
            mainMenuBestSeedHud.bestSeedAvailable = true;
            mainMenuBestSeedHud.bestSpeedrunTicks = 4'200U;
            const bool renderedMainMenuBestSeed = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {},
                mainMenuBestSeedHud);
            std::vector<std::uint8_t> mainMenuBestSeedPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                mainMenuBestSeedPixels.data());

            mainMenuHud.mainMenuSelection
                = gdp::game::visual::HudMainMenuSelection::ExitGame;
            const bool renderedMainMenuExit = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, mainMenuHud);
            std::vector<std::uint8_t> mainMenuExitPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                mainMenuExitPixels.data());

            mainMenuHud.exitConfirmation = true;
            const bool renderedExitConfirmation = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, mainMenuHud);
            std::vector<std::uint8_t> exitConfirmationPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                exitConfirmationPixels.data());

            mainMenuHud.exitConfirmationSelection
                = gdp::game::visual::HudExitConfirmationSelection::ExitGame;
            const bool renderedExitConfirmationSelected = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, mainMenuHud);
            std::vector<std::uint8_t> exitConfirmationSelectedPixels(
                baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                exitConfirmationSelectedPixels.data());

            gdp::game::visual::HudRenderData settingsHud;
            settingsHud.settingsMenu = true;
            const bool renderedSettings = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, settingsHud);
            std::vector<std::uint8_t> settingsPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                settingsPixels.data());

            settingsHud.settingsSelection
                = gdp::game::visual::HudSettingsSelection::VerticalSync;
            settingsHud.verticalSync = false;
            const bool renderedSettingsAdjusted = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, settingsHud);
            std::vector<std::uint8_t> settingsAdjustedPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                settingsAdjustedPixels.data());

            const bool renderedWorker = renderer.prepareWorkerVisual(111)
                && renderer.render(width, height, {}, workerOnly);
            std::vector<std::uint8_t> workerPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, workerPixels.data());

            const bool renderedAnimatedWorker
                = renderer.render(width, height, {}, animatedWorkerOnly);
            std::vector<std::uint8_t> animatedWorkerPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                animatedWorkerPixels.data());

            const bool renderedHitWorker
                = renderer.render(width, height, {}, hitWorkerOnly);
            std::vector<std::uint8_t> hitWorkerPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                hitWorkerPixels.data());

            const bool renderedFallenWorker
                = renderer.render(width, height, {}, fallenWorkerOnly);
            std::vector<std::uint8_t> fallenWorkerPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                fallenWorkerPixels.data());

            const bool renderedFallingWorker
                = renderer.render(width, height, {}, fallingWorkerOnly);
            std::vector<std::uint8_t> fallingWorkerPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                fallingWorkerPixels.data());

            const bool renderedRotatedWorker
                = renderer.render(width, height, {}, rotatedWorkerOnly);
            std::vector<std::uint8_t> rotatedWorkerPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                rotatedWorkerPixels.data());

            const bool renderedGuard = renderer.prepareGuardVisual(333)
                && renderer.render(width, height, {}, guardOnly);
            std::vector<std::uint8_t> guardPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, guardPixels.data());

            const bool renderedOfficer = renderer.prepareOfficerVisual(444)
                && renderer.render(width, height, {}, officerOnly);
            std::vector<std::uint8_t> officerPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, officerPixels.data());

            const bool renderedEngineer = renderer.prepareEngineerVisual(555)
                && renderer.render(width, height, {}, engineerOnly);
            std::vector<std::uint8_t> engineerPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, engineerPixels.data());

            const bool renderedIndustrialRobot
                = renderer.prepareIndustrialRobotVisual(666)
                && renderer.render(width, height, {}, industrialRobotOnly);
            std::vector<std::uint8_t> industrialRobotPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                industrialRobotPixels.data());

            const bool renderedPickups = renderer.render(width, height, {}, {}, pickups);
            std::vector<std::uint8_t> pickupPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                pickupPixels.data());

            const bool renderedProps = renderer.render(width, height, {}, {}, {}, props);
            std::vector<std::uint8_t> propPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                propPixels.data());

            const bool renderedImpactBaseline = renderer.render(width, height);
            std::vector<std::uint8_t> impactBaselinePixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                impactBaselinePixels.data());
            constexpr std::array impacts{
                gdp::game::visual::ImpactRenderData{
                    {-0.22F, 1.62F, 3.0F}, {0.0F, 0.0F, 1.0F},
                    gdp::game::visual::ImpactVisualMaterial::Metal, 1.0F, 81U},
                gdp::game::visual::ImpactRenderData{
                    {0.0F, 1.62F, 3.0F}, {0.0F, 0.0F, 1.0F},
                    gdp::game::visual::ImpactVisualMaterial::Composite, 1.0F, 82U},
                gdp::game::visual::ImpactRenderData{
                    {0.22F, 1.62F, 3.0F}, {0.0F, 0.0F, 1.0F},
                    gdp::game::visual::ImpactVisualMaterial::Flesh, 1.0F, 83U},
            };
            const bool renderedImpacts = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, impacts);
            std::vector<std::uint8_t> impactPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                impactPixels.data());

            constexpr gdp::game::visual::WeaponViewmodelRenderData pistolViewmodel{
                .visible = true,
                .role = gdp::game::visual::HudWeaponRole::Pistol,
                .visualSeed = 1717U,
            };
            const bool renderedPistolViewmodel = renderer.render(
                width, height,
                {}, // camera
                {}, // enemies
                {}, // pickups
                {}, // props
                {}, // lighting
                {}, // room
                {}, // doors
                {}, // corridors
                {}, // connected rooms
                {}, // HUD
                {}, // impacts
                pistolViewmodel);
            std::vector<std::uint8_t> pistolViewmodelPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                pistolViewmodelPixels.data());

            constexpr gdp::game::visual::WeaponViewmodelRenderData carbineViewmodel{
                .visible = true,
                .role = gdp::game::visual::HudWeaponRole::Carbine,
                .visualSeed = 1717U,
            };
            const bool renderedCarbineViewmodel = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
                carbineViewmodel);
            std::vector<std::uint8_t> carbineViewmodelPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                carbineViewmodelPixels.data());

            constexpr gdp::game::visual::WeaponViewmodelRenderData knifeViewmodel{
                .visible = true,
                .role = gdp::game::visual::HudWeaponRole::Knife,
                .meleeStrength = 0.5F,
                .visualSeed = 1717U,
            };
            const bool renderedKnifeViewmodel = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
                knifeViewmodel);
            std::vector<std::uint8_t> knifeViewmodelPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                knifeViewmodelPixels.data());

            struct WeaponFootprint {
                std::size_t pixels{0U};
                int minX{width};
                int minY{height};
                int maxX{0};
                int maxY{0};
            };
            const auto weaponFootprint = [&](const float fieldOfView,
                const gdp::game::visual::HudWeaponRole role,
                WeaponFootprint& footprint) {
                gdp::game::visual::CameraData fovCamera;
                fovCamera.verticalFieldOfViewDegrees = fieldOfView;
                const gdp::game::visual::WeaponViewmodelRenderData weapon{
                    .visible = true, .role = role, .visualSeed = 1717U};
                if (!renderer.render(width, height, fovCamera)) {
                    return false;
                }
                std::vector<std::uint8_t> background(baseline.size());
                glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                    background.data());
                if (!renderer.render(width, height, fovCamera, {}, {}, {}, {},
                        {}, {}, {}, {}, {}, {}, weapon)) {
                    return false;
                }
                std::vector<std::uint8_t> frame(baseline.size());
                glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                    frame.data());
                for (int y = 0; y < height; ++y) {
                    for (int x = 0; x < width; ++x) {
                        const std::size_t pixel = static_cast<std::size_t>(
                            (y * width + x) * 4);
                        bool changed = false;
                        for (std::size_t channel = 0U; channel < 3U; ++channel) {
                            changed |= std::abs(static_cast<int>(frame[pixel + channel])
                                - static_cast<int>(background[pixel + channel])) > 3;
                        }
                        if (changed) {
                            ++footprint.pixels;
                            footprint.minX = std::min(footprint.minX, x);
                            footprint.minY = std::min(footprint.minY, y);
                            footprint.maxX = std::max(footprint.maxX, x);
                            footprint.maxY = std::max(footprint.maxY, y);
                        }
                    }
                }
                return true;
            };
            bool renderedFovViewmodels = true;
            bool fovFootprintsStable = true;
            for (const auto role : {gdp::game::visual::HudWeaponRole::Pistol,
                    gdp::game::visual::HudWeaponRole::Carbine,
                    gdp::game::visual::HudWeaponRole::Knife,
                    gdp::game::visual::HudWeaponRole::Sniper,
                    gdp::game::visual::HudWeaponRole::Shotgun,
                    gdp::game::visual::HudWeaponRole::SubmachineGun}) {
                WeaponFootprint narrow;
                WeaponFootprint wide;
                renderedFovViewmodels &= weaponFootprint(60.0F, role, narrow)
                    && weaponFootprint(100.0F, role, wide);
                const bool stable = narrow.pixels > 20U
                    && wide.pixels > 20U
                    && std::abs(static_cast<int>(narrow.pixels)
                        - static_cast<int>(wide.pixels))
                        <= static_cast<int>(narrow.pixels / 10U + 2U)
                    && std::abs(narrow.minX - wide.minX) <= 3
                    && std::abs(narrow.minY - wide.minY) <= 3
                    && std::abs(narrow.maxX - wide.maxX) <= 3
                    && std::abs(narrow.maxY - wide.maxY) <= 3;
                fovFootprintsStable &= stable;
                if (!stable) {
                    std::cerr << "Viewmodel FOV footprint changed: role="
                        << static_cast<int>(role) << " pixels=" << narrow.pixels
                        << '/' << wide.pixels << " bounds=" << narrow.minX << ','
                        << narrow.minY << ',' << narrow.maxX << ',' << narrow.maxY
                        << '/' << wide.minX << ',' << wide.minY << ',' << wide.maxX
                        << ',' << wide.maxY << '\n';
                }
            }

            constexpr gdp::game::visual::WeaponViewmodelRenderData firingViewmodel{
                .visible = true,
                .role = gdp::game::visual::HudWeaponRole::Pistol,
                .recoilStrength = 1.0F,
                .muzzleFlashStrength = 1.0F,
                .overdrive = true,
                .visualSeed = 1717U,
            };
            const bool renderedFiringViewmodel = renderer.render(
                width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
                firingViewmodel);
            std::vector<std::uint8_t> firingViewmodelPixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                firingViewmodelPixels.data());

            const bool renderedEnemies = renderer.prepareWorkerVisual(111)
                && renderer.prepareWorkerVisual(222)
                && renderer.prepareGuardVisual(333)
                && renderer.prepareOfficerVisual(444)
                && renderer.prepareEngineerVisual(555)
                && renderer.prepareIndustrialRobotVisual(666)
                && renderer.render(width, height, {}, enemies, pickups, props);
            if (!renderedEnemies) {
                std::cerr << "Renderer failed to draw enemy placeholders\n";
            }
            std::vector<std::uint8_t> pixels(baseline.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

            std::uint8_t darkest = 255U;
            std::uint8_t brightest = 0U;
            std::size_t roomChangedChannels = 0;
            std::size_t themeChangedChannels = 0;
            std::size_t combatChangedChannels = 0;
            std::size_t cafeteriaChangedChannels = 0;
            std::size_t laboratoryChangedChannels = 0;
            std::size_t technicalChangedChannels = 0;
            std::size_t securityChangedChannels = 0;
            std::size_t centralHubChangedChannels = 0;
            std::size_t lightingChangedChannels = 0;
            std::size_t flashlightChangedChannels = 0;
            std::size_t potatoQualityChangedChannels = 0;
            std::size_t damagedFixtureChangedChannels = 0;
            std::size_t destroyedFixtureChangedChannels = 0;
            std::size_t dynamicShadowChangedChannels = 0;
            std::size_t alertLightingChangedChannels = 0;
            std::size_t doorStateChangedChannels = 0;
            std::size_t doorMotionChangedChannels = 0;
            std::size_t corridorChangedChannels = 0;
            std::size_t connectedRoomMaterialChangedChannels = 0;
            std::size_t cornerCyanPixels = 0;
            std::size_t hudChangedChannels = 0;
            std::size_t resultReplayChangedChannels = 0;
            std::size_t campaignResultChangedChannels = 0;
            std::size_t resultSelectionChangedChannels = 0;
            std::size_t functionalHudChangedChannels = 0;
            std::size_t mapHudChangedChannels = 0;
            std::size_t mapRouteChangedChannels = 0;
            std::size_t mapFootprintChangedChannels = 0;
            std::size_t mapExplorationChangedChannels = 0;
            std::size_t spreadHudChangedChannels = 0;
            std::size_t weakPointHudChangedChannels = 0;
            std::size_t performanceOverlayChangedChannels = 0;
            std::size_t runTimerChangedChannels = 0;
            std::size_t campaignTimerChangedChannels = 0;
            std::size_t speedrunRecordChangedChannels = 0;
            std::size_t pauseMenuChangedChannels = 0;
            std::size_t mainMenuChangedChannels = 0;
            std::size_t mainMenuContinueChangedChannels = 0;
            std::size_t mainMenuCampaignChangedChannels = 0;
            std::size_t mainMenuBestSeedChangedChannels = 0;
            std::size_t mainMenuSelectionChangedChannels = 0;
            std::size_t exitConfirmationChangedChannels = 0;
            std::size_t exitConfirmationSelectionChangedChannels = 0;
            std::size_t settingsMenuChangedChannels = 0;
            std::size_t settingsSelectionChangedChannels = 0;
            std::size_t workerChangedChannels = 0;
            std::size_t enemyAnimationChangedChannels = 0;
            std::size_t enemyHitChangedChannels = 0;
            std::size_t enemyDeathTransitionChangedChannels = 0;
            std::size_t enemyFacingChangedChannels = 0;
            std::size_t guardChangedChannels = 0;
            std::size_t officerChangedChannels = 0;
            std::size_t engineerChangedChannels = 0;
            std::size_t industrialRobotChangedChannels = 0;
            std::size_t pickupChangedChannels = 0;
            std::size_t propChangedChannels = 0;
            std::size_t impactChangedChannels = 0;
            std::size_t pistolViewmodelChangedChannels = 0;
            std::size_t carbineViewmodelChangedChannels = 0;
            std::size_t knifeViewmodelChangedChannels = 0;
            std::size_t firingViewmodelChangedChannels = 0;
            std::size_t enemyChangedChannels = 0;
            for (std::size_t index = 0; index < pixels.size(); index += 4U) {
                darkest = std::min(darkest, pixels[index]);
                brightest = std::max(brightest, pixels[index]);
                if (cornerCorridorPixels[index + 1U]
                        > cornerCorridorPixels[index] + 8U
                    && cornerCorridorPixels[index + 2U]
                        > cornerCorridorPixels[index] + 8U
                    && cornerCorridorPixels[index + 2U] > 24U) {
                    ++cornerCyanPixels;
                }
                for (std::size_t channel = 0; channel < 3U; ++channel) {
                    if (roomPixels[index + channel] != baseline[index + channel]) {
                        ++roomChangedChannels;
                    }
                    if (storagePixels[index + channel] != roomPixels[index + channel]) {
                        ++themeChangedChannels;
                    }
                    if (combatPixels[index + channel] != storagePixels[index + channel]) {
                        ++combatChangedChannels;
                    }
                    if (cafeteriaPixels[index + channel] != combatPixels[index + channel]) {
                        ++cafeteriaChangedChannels;
                    }
                    if (laboratoryPixels[index + channel] != cafeteriaPixels[index + channel]) {
                        ++laboratoryChangedChannels;
                    }
                    if (technicalPixels[index + channel] != laboratoryPixels[index + channel]) {
                        ++technicalChangedChannels;
                    }
                    if (securityPixels[index + channel] != technicalPixels[index + channel]) {
                        ++securityChangedChannels;
                    }
                    if (centralHubPixels[index + channel] != securityPixels[index + channel]) {
                        ++centralHubChangedChannels;
                    }
                    if (lightingPixels[index + channel]
                        != centralHubPixels[index + channel]) {
                        ++lightingChangedChannels;
                    }
                    if (flashlightPixels[index + channel]
                        > lightingPixels[index + channel]) {
                        ++flashlightChangedChannels;
                    }
                    if (potatoQualityPixels[index + channel]
                        != lightingPixels[index + channel]) {
                        ++potatoQualityChangedChannels;
                    }
                    if (damagedFixturePixels[index + channel]
                        != lightingPixels[index + channel]) {
                        ++damagedFixtureChangedChannels;
                    }
                    if (destroyedFixturePixels[index + channel]
                        != damagedFixturePixels[index + channel]) {
                        ++destroyedFixtureChangedChannels;
                    }
                    if (shadowedPixels[index + channel]
                        != shadowBaselinePixels[index + channel]) {
                        ++dynamicShadowChangedChannels;
                    }
                    if (alertLightingPixels[index + channel]
                        != lightingPixels[index + channel]) {
                        ++alertLightingChangedChannels;
                    }
                    if (lockedDoorPixels[index + channel]
                        != translatedDoorPixels[index + channel]) {
                        ++doorStateChangedChannels;
                    }
                    if (movingDoorPixels[index + channel]
                        != translatedDoorPixels[index + channel]) {
                        ++doorMotionChangedChannels;
                    }
                    if (corridorPixels[index + channel]
                        != openDoorPixels[index + channel]) {
                        ++corridorChangedChannels;
                    }
                    if (connectedRoomMaterialPixels[index + channel]
                        != corridorPixels[index + channel]) {
                        ++connectedRoomMaterialChangedChannels;
                    }
                    if (hudPixels[index + channel]
                        != centralHubPixels[index + channel]) {
                        ++hudChangedChannels;
                    }
                    if (resultExitPixels[index + channel]
                        != hudPixels[index + channel]) {
                        ++resultSelectionChangedChannels;
                    }
                    if (resultReplayPixels[index + channel]
                        != hudPixels[index + channel]) {
                        ++resultReplayChangedChannels;
                    }
                    if (campaignResultPixels[index + channel]
                        != hudPixels[index + channel]) {
                        ++campaignResultChangedChannels;
                    }
                    if (functionalHudPixels[index + channel]
                        != centralHubPixels[index + channel]) {
                        ++functionalHudChangedChannels;
                    }
                    if (mapHudPixels[index + channel]
                        != mapReferencePixels[index + channel]) {
                        ++mapHudChangedChannels;
                    }
                    if (bentMapHudPixels[index + channel]
                        != mapHudPixels[index + channel]) {
                        ++mapRouteChangedChannels;
                    }
                    if (narrowRoomMapHudPixels[index + channel]
                        != mapHudPixels[index + channel]) {
                        ++mapFootprintChangedChannels;
                    }
                    if (exploredMapHudPixels[index + channel]
                        != mapHudPixels[index + channel]) {
                        ++mapExplorationChangedChannels;
                    }
                    if (spreadHudPixels[index + channel]
                        != functionalHudPixels[index + channel]) {
                        ++spreadHudChangedChannels;
                    }
                    if (weakPointHudPixels[index + channel]
                        != functionalHudPixels[index + channel]) {
                        ++weakPointHudChangedChannels;
                    }
                    if (performanceOverlayPixels[index + channel]
                        != functionalHudPixels[index + channel]) {
                        ++performanceOverlayChangedChannels;
                    }
                    if (runTimerPixels[index + channel]
                        != functionalHudPixels[index + channel]) {
                        ++runTimerChangedChannels;
                    }
                    if (campaignTimerPixels[index + channel]
                        != runTimerPixels[index + channel]) {
                        ++campaignTimerChangedChannels;
                    }
                    if (speedrunRecordPixels[index + channel]
                        != runTimerPixels[index + channel]) {
                        ++speedrunRecordChangedChannels;
                    }
                    if (pauseMenuPixels[index + channel]
                        != functionalHudPixels[index + channel]) {
                        ++pauseMenuChangedChannels;
                    }
                    if (mainMenuPixels[index + channel] != baseline[index + channel]) {
                        ++mainMenuChangedChannels;
                    }
                    if (mainMenuContinuePixels[index + channel]
                        != mainMenuPixels[index + channel]) {
                        ++mainMenuContinueChangedChannels;
                    }
                    if (mainMenuCampaignPixels[index + channel]
                        != mainMenuPixels[index + channel]) {
                        ++mainMenuCampaignChangedChannels;
                    }
                    if (mainMenuBestSeedPixels[index + channel]
                        != mainMenuPixels[index + channel]) {
                        ++mainMenuBestSeedChangedChannels;
                    }
                    if (mainMenuExitPixels[index + channel]
                        != mainMenuPixels[index + channel]) {
                        ++mainMenuSelectionChangedChannels;
                    }
                    if (exitConfirmationPixels[index + channel]
                        != mainMenuExitPixels[index + channel]) {
                        ++exitConfirmationChangedChannels;
                    }
                    if (exitConfirmationSelectedPixels[index + channel]
                        != exitConfirmationPixels[index + channel]) {
                        ++exitConfirmationSelectionChangedChannels;
                    }
                    if (settingsPixels[index + channel]
                        != baseline[index + channel]) {
                        ++settingsMenuChangedChannels;
                    }
                    if (settingsAdjustedPixels[index + channel]
                        != settingsPixels[index + channel]) {
                        ++settingsSelectionChangedChannels;
                    }
                    if (workerPixels[index + channel] != centralHubPixels[index + channel]) {
                        ++workerChangedChannels;
                    }
                    if (animatedWorkerPixels[index + channel]
                        != workerPixels[index + channel]) {
                        ++enemyAnimationChangedChannels;
                    }
                    if (hitWorkerPixels[index + channel]
                        != workerPixels[index + channel]) {
                        ++enemyHitChangedChannels;
                    }
                    if (fallingWorkerPixels[index + channel]
                        != fallenWorkerPixels[index + channel]) {
                        ++enemyDeathTransitionChangedChannels;
                    }
                    if (rotatedWorkerPixels[index + channel]
                        != workerPixels[index + channel]) {
                        ++enemyFacingChangedChannels;
                    }
                    if (guardPixels[index + channel] != workerPixels[index + channel]) {
                        ++guardChangedChannels;
                    }
                    if (officerPixels[index + channel] != guardPixels[index + channel]) {
                        ++officerChangedChannels;
                    }
                    if (engineerPixels[index + channel] != officerPixels[index + channel]) {
                        ++engineerChangedChannels;
                    }
                    if (industrialRobotPixels[index + channel]
                        != engineerPixels[index + channel]) {
                        ++industrialRobotChangedChannels;
                    }
                    if (pickupPixels[index + channel]
                        != industrialRobotPixels[index + channel]) {
                        ++pickupChangedChannels;
                    }
                    if (propPixels[index + channel] != pickupPixels[index + channel]) {
                        ++propChangedChannels;
                    }
                    if (impactPixels[index + channel]
                        != impactBaselinePixels[index + channel]) {
                        ++impactChangedChannels;
                    }
                    if (pistolViewmodelPixels[index + channel]
                        != impactBaselinePixels[index + channel]) {
                        ++pistolViewmodelChangedChannels;
                    }
                    if (carbineViewmodelPixels[index + channel]
                        != pistolViewmodelPixels[index + channel]) {
                        ++carbineViewmodelChangedChannels;
                    }
                    if (knifeViewmodelPixels[index + channel]
                        != pistolViewmodelPixels[index + channel]) {
                        ++knifeViewmodelChangedChannels;
                    }
                    if (firingViewmodelPixels[index + channel]
                        != pistolViewmodelPixels[index + channel]) {
                        ++firingViewmodelChangedChannels;
                    }
                    if (pixels[index + channel] != propPixels[index + channel]) {
                        ++enemyChangedChannels;
                    }
                }
            }
            passed = !renderer.gpuVendor().empty()
                && !renderer.gpuRenderer().empty()
                && !renderer.gpuVersion().empty()
                && renderedRoomVariant && repeatedRoom
                && roomPixels == repeatedRoomPixels && roomChangedChannels > 100U
                && renderedStorageVariant && themeChangedChannels > 100U
                && renderedCombatVariant && combatChangedChannels > 100U
                && renderedCafeteriaVariant && cafeteriaChangedChannels > 100U
                && renderedLaboratoryVariant && laboratoryChangedChannels > 100U
                && renderedTechnicalVariant && technicalChangedChannels > 100U
                && renderedSecurityVariant && securityChangedChannels > 100U
                && renderedCentralHubVariant && centralHubChangedChannels > 100U
                && renderedProceduralLighting && lightingChangedChannels > 100U
                && renderedFlashlight && flashlightChangedChannels > 30U
                && renderedDarkReference && renderedDarkFlashlight
                && darkBeamPixels > 500U
                && renderedInfoHud && infoHudPixels != functionalHudPixels
                && renderedPotatoQuality && potatoQualityChangedChannels > 100U
                && renderedPotatoFlashlight
                && potatoFlashlightPixels != potatoQualityPixels
                && potatoFrameStats.lowSpecShader
                && renderedPotatoReference
                && !potatoReferenceStats.lowSpecShader
                && potatoReferencePixels == potatoQualityPixels
                && renderedDamagedFixture
                && damagedFixtureChangedChannels > 100U
                && renderedDestroyedFixture
                && destroyedFixtureChangedChannels > 100U
                && renderedShadowBaseline && renderedFullShadowReference
                && renderedDynamicShadows && shadowSpecializationPreservedPixels
                && dynamicShadowChangedChannels > 64U
                && renderedClampedShadowBudget
                && clampedShadowPixels == shadowedPixels
                && renderedShadow6Variant
                && shadow6Stats.shadowShaderOccluderCapacity == 6U
                && renderedShadow8Variant
                && shadow8Stats.shadowShaderOccluderCapacity == 8U
                && renderedAlertLighting && alertLightingChangedChannels > 100U
                && renderedTranslatedDoor && renderedLockedDoor && renderedMovingDoor
                && doorStateChangedChannels > 100U
                && doorMotionChangedChannels > 100U
                && renderedOpenDoor && renderedCorridorReference
                && renderedCorridor && corridorInstancingPreservedPixels
                && corridorInstancingReducedDraws
                && corridorChangedChannels > 100U
                && renderedConnectedRoomMaterial
                && connectedRoomMaterialChangedChannels > 100U
                && renderedCornerCorridor && cornerCyanPixels > 20U
                && renderedHud && hudChangedChannels > 100U
                && renderedResultReplay
                && resultReplayChangedChannels > 100U
                && renderedCampaignResult
                && campaignResultChangedChannels > 100U
                && renderedResultExit && resultSelectionChangedChannels > 100U
                && renderedFunctionalHud && functionalHudChangedChannels > 500U
                && renderedMapReference && renderedMapHud
                && mapMarkersDistinct
                && renderedBentMapHud && mapRouteChangedChannels > 5U
                && renderedNarrowRoomMapHud && mapFootprintChangedChannels > 5U
                && renderedExploredMapHud && mapHudChangedChannels > 100U
                && mapExplorationChangedChannels > 5U
                && renderedSpreadHud && spreadHudChangedChannels > 20U
                && renderedWeakPointHud && weakPointHudChangedChannels > 20U
                && renderedPerformanceOverlay
                && performanceOverlayChangedChannels > 500U
                && performanceFrameStats.drawCalls > 0U
                && performanceFrameStats.triangles
                    >= performanceFrameStats.drawCalls
                // Reference enemy assets are prebuilt GPU meshes and bypass
                // the procedural mesh cache; mesh binds and enemy pixel gates
                // below still verify that geometry rendered.
                && (performanceFrameStats.meshCacheEntries > 0U
                    || (std::getenv("GDP_ENEMY_REFERENCE_MODELS") == nullptr
                        || std::string_view{std::getenv("GDP_ENEMY_REFERENCE_MODELS")}
                            != "0"))
                && performanceFrameStats.textureCacheEntries > 0U
                && performanceFrameStats.uniformUploads > 0U
                && performanceFrameStats.uniformSkips > 0U
                && performanceFrameStats.textureUnitActivations > 0U
                && performanceFrameStats.textureBinds > 0U
                && performanceFrameStats.textureBindSkips > 0U
                && performanceFrameStats.meshBinds > 0U
                && performanceFrameStats.meshBindSkips > 0U
                && performanceFrameStats.backfaceCulledDraws > 0U
                && performanceFrameStats.cullStateChanges > 0U
                && performanceFrameStats.instancedDrawCalls > 0U
                && performanceFrameStats.renderedInstances
                    > performanceFrameStats.instancedDrawCalls
                && performanceFrameStats.avoidedDrawCalls > 0U
                && performanceFrameStats.shadowShaderOccluderCapacity == 0U
                && measuredGpuFrame
                && renderedRunTimer && runTimerChangedChannels > 50U
                && renderedCampaignTimer && campaignTimerChangedChannels > 50U
                && renderedSpeedrunRecord
                && speedrunRecordChangedChannels > 50U
                && renderedPauseMenu && pauseMenuChangedChannels > 500U
                && renderedMainMenu && mainMenuChangedChannels > 500U
                && renderedControls && controlsPixels != mainMenuPixels
                && renderedAlternateSeed && alternateSeedPixels != infoHudPixels
                && renderedMainMenuContinue
                && mainMenuContinueChangedChannels > 100U
                && renderedMainMenuCampaign
                && mainMenuCampaignChangedChannels > 100U
                && renderedMainMenuBestSeed
                && mainMenuBestSeedChangedChannels > 100U
                && renderedMainMenuExit && mainMenuSelectionChangedChannels > 100U
                && renderedExitConfirmation
                && exitConfirmationChangedChannels > 500U
                && renderedExitConfirmationSelected
                && exitConfirmationSelectionChangedChannels > 100U
                && renderedSettings && settingsMenuChangedChannels > 500U
                && renderedSettingsAdjusted
                && settingsSelectionChangedChannels > 100U
                && renderedWorker && workerChangedChannels > 100U
                && renderedAnimatedWorker && enemyAnimationChangedChannels > 100U
                && renderedHitWorker && enemyHitChangedChannels > 100U
                && renderedFallenWorker && renderedFallingWorker
                && enemyDeathTransitionChangedChannels > 100U
                && renderedRotatedWorker && enemyFacingChangedChannels > 100U
                && renderedGuard && guardChangedChannels > 100U
                && renderedOfficer && officerChangedChannels > 100U
                && renderedEngineer && engineerChangedChannels > 100U
                && renderedIndustrialRobot && industrialRobotChangedChannels > 100U
                && renderedPickups && pickupChangedChannels > 100U
                && renderedProps && propChangedChannels > 100U
                && renderedImpactBaseline && renderedImpacts
                && impactChangedChannels > 10U
                && renderedPistolViewmodel
                && pistolViewmodelChangedChannels > 100U
                && renderedCarbineViewmodel
                && carbineViewmodelChangedChannels > 100U
                && renderedKnifeViewmodel
                && knifeViewmodelChangedChannels > 100U
                && renderedFovViewmodels && fovFootprintsStable
                && renderedFiringViewmodel
                && firingViewmodelChangedChannels > 100U
                && renderedEnemies && brightest > darkest + 8U && enemyChangedChannels > 100U
                && glGetError() == GL_NO_ERROR;
            if (!passed) {
                std::cerr << "Framebuffer lacks expected scene or enemy contrast"
                          << " | corridor rendered=" << renderedCorridor
                          << " openDoor=" << renderedOpenDoor
                          << " changed=" << corridorChangedChannels
                          << " cornerCyan=" << cornerCyanPixels
                          << " doorState=" << doorStateChangedChannels
                          << " fixture=" << damagedFixtureChangedChannels
                          << '/' << destroyedFixtureChangedChannels
                          << " shadow=" << dynamicShadowChangedChannels
                          << " shadowRef=" << renderedFullShadowReference
                          << " shadowExact=" << shadowSpecializationPreservedPixels
                          << " shadowCap="
                          << fullShadowReferenceStats.shadowShaderOccluderCapacity
                          << '/' << specializedShadowStats.shadowShaderOccluderCapacity
                          << " impact=" << impactChangedChannels
                          << " pistol=" << pistolViewmodelChangedChannels
                          << " carbine=" << carbineViewmodelChangedChannels
                          << " knife=" << knifeViewmodelChangedChannels
                          << " fovStable=" << fovFootprintsStable
                          << " firing=" << firingViewmodelChangedChannels
                          << " impactRendered=" << renderedImpacts
                          << " enemyContrast=" << unsigned(brightest) - unsigned(darkest)
                          << " enemyChanged=" << enemyChangedChannels
                          << " worker=" << workerChangedChannels
                          << " guard=" << guardChangedChannels
                          << " officer=" << officerChangedChannels
                          << " engineer=" << engineerChangedChannels
                          << " robot=" << industrialRobotChangedChannels << '\n';
                std::cerr << "UI gates: map=" << mapHudChangedChannels
                          << " route=" << mapRouteChangedChannels << " footprint=" << mapFootprintChangedChannels
                          << " exploration=" << mapExplorationChangedChannels
                          << " timer=" << runTimerChangedChannels << " campaign=" << campaignTimerChangedChannels
                          << " record=" << speedrunRecordChangedChannels
                          << " main=" << mainMenuChangedChannels << " settings=" << settingsMenuChangedChannels
                          << " selection=" << settingsSelectionChangedChannels
                          << " hud=" << functionalHudChangedChannels << '\n';
                std::cerr << "Renderer invariants: roomExact=" << (roomPixels == repeatedRoomPixels)
                          << " potatoExact=" << (potatoReferencePixels == potatoQualityPixels)
                          << " shadowClampExact=" << (clampedShadowPixels == shadowedPixels)
                          << " instancingEquivalent=" << corridorInstancingPreservedPixels
                          << " instancingReduced=" << corridorInstancingReducedDraws
                          << " instancingDelta=" << instancingComparison.maximumDelta
                          << " instancingChanged=" << instancingComparison.changedChannels
                          << " gpuTiming=" << measuredGpuFrame
                          << " gpuSamples=" << performanceFrameStats.gpuTimingAvailable
                          << " uniformSkips=" << performanceFrameStats.uniformSkips
                          << " textureSkips=" << performanceFrameStats.textureBindSkips
                          << " meshSkips=" << performanceFrameStats.meshBindSkips
                          << " darkBeam=" << darkBeamPixels << '\n';
            }
            // Exercise the gameplay HUD at real drawable sizes after the small
            // fixed-scene checks; these snapshots do not change combat state.
            passed = validateBossHud(renderer) && passed;
            passed = validateCampaignExitHud(renderer) && passed;
            passed = validateFrontendMenus(renderer) && passed;
            passed = validateTacticalHud(renderer) && passed;
            passed = validateNewWeaponVisuals(renderer) && passed;
            passed = validateSniperScope(renderer) && passed;
            passed = validatePlayerBody(renderer) && passed;
            passed = validateInstancingTolerance() && passed;
        }
    }

    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    if (!passed) {
        return 1;
    }
    std::cout << "OpenGL framebuffer contains the fixed-room scene\n";
    return 0;
}
