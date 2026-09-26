#include "game/visual/GdpRenderer.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

bool savePpm(
    const std::filesystem::path& path,
    const int width,
    const int height,
    const std::span<const std::uint8_t> rgba)
{
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        return false;
    }
    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (int y = height - 1; y >= 0; --y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t pixel = (static_cast<std::size_t>(y)
                * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4U;
            output.write(reinterpret_cast<const char*>(&rgba[pixel]), 3);
        }
    }
    return static_cast<bool>(output);
}

} // namespace

int main(const int argc, char* argv[])
{
    constexpr int width = 1280;
    constexpr int height = 720;
    const std::filesystem::path outputDirectory
        = argc > 1 ? std::filesystem::path{argv[1]} : std::filesystem::path{"."};
    std::filesystem::create_directories(outputDirectory);

    if (!SDL_Init(SDL_INIT_VIDEO)
        || !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3)
        || !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3)
        || !SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE)
        || !SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24)) {
        std::cerr << "SDL/OpenGL setup failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    SDL_Window* const window = SDL_CreateWindow(
        "GDP Industrial wall capture", width, height,
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

    bool passed = true;
    {
        gdp::game::visual::GdpRenderer renderer;
        if (!renderer.initialize(width, height)) {
            std::cerr << "Renderer initialization failed\n";
            passed = false;
        } else {
            constexpr std::array corridorPoints{
                wkc::core::Vec3{0.0F, 0.0F, 4.0F},
                wkc::core::Vec3{0.0F, 0.0F, -14.0F},
            };
            const std::array corridor{
                gdp::game::visual::CorridorRenderData{corridorPoints, 2.8F, 3.2F},
            };
            constexpr std::array doors{
                gdp::game::visual::DoorRenderData{
                    {0.0F, 0.0F, -14.0F}, {0.0F, 0.0F, 1.0F},
                    2.8F, 3.2F, false, 1.0F},
            };
            constexpr gdp::game::visual::RoomRenderData roomBehindCamera{
                {0.0F, 0.0F, 10.0F}, wkc::core::QuarterTurn::Degrees0, 0U,
                {{-4.0F, 0.0F, 5.0F}, {4.0F, 4.0F, 15.0F}}};
            constexpr std::array connectedRoom{
                gdp::game::visual::RoomRenderData{
                    {0.0F, 0.0F, -18.0F}, wkc::core::QuarterTurn::Degrees0, 0U,
                    {{-4.8F, 0.0F, -22.0F}, {4.8F, 4.2F, -14.0F}}},
            };
            constexpr gdp::game::visual::CameraData gameplayCamera{
                {0.0F, 1.58F, 3.25F}, {0.14F, -0.015F, -0.99F}, 70.0F};
            constexpr gdp::game::visual::CameraData closeCamera{
                {0.15F, 1.55F, 1.80F}, {0.94F, -0.08F, -0.33F}, 58.0F};
            constexpr gdp::game::visual::CameraData ceilingCamera{
                {0.0F, 1.50F, 2.50F}, {0.0F, 0.62F, -0.78F}, 68.0F};
            constexpr gdp::game::visual::CameraData ceilingCloseCamera{
                {0.35F, 1.68F, -2.2F}, {-0.10F, 0.90F, -0.42F}, 60.0F};
            gdp::game::visual::RoomLightingRenderData lighting;
            lighting.ambientColor = {0.78F, 0.82F, 0.86F};
            lighting.ambientIntensity = 0.22F;
            lighting.fixtureEmissive = 0.72F;
            lighting.lightCount = 4U;
            lighting.lights[0] = {
                {0.0F, 2.9F, 1.0F}, {0.92F, 0.96F, 1.0F}, 0.62F, 7.5F};
            lighting.lights[1] = {
                {0.0F, 2.9F, -4.0F}, {0.86F, 0.92F, 1.0F}, 0.58F, 7.5F};
            lighting.lights[2] = {
                {0.0F, 2.9F, -9.0F}, {0.86F, 0.92F, 1.0F}, 0.55F, 7.5F};
            lighting.lights[3] = {
                {0.0F, 3.2F, -17.0F}, {0.92F, 0.96F, 1.0F}, 0.62F, 8.5F};

            struct CaptureCase {
                wkc::core::Seed seed;
                const char* label;
            };
            constexpr std::array captures{
                CaptureCase{2U, "maintained_seed2"},
                CaptureCase{3U, "used_seed3"},
                CaptureCase{1U, "worn_seed1"},
            };
            std::vector<std::uint8_t> pixels(
                static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            for (const CaptureCase capture : captures) {
                if (!renderer.prepareIndustrialRoomMaterials(capture.seed)) {
                    std::cerr << "Material preparation failed for seed " << capture.seed << '\n';
                    passed = false;
                    continue;
                }
                const auto captureView = [&](const gdp::game::visual::CameraData view,
                                              const char* const unit,
                                              const char* const suffix,
                                              const gdp::game::visual::RoomLightingRenderData&
                                                  captureLighting) {
                    if (!renderer.render(width, height, view, {}, {}, {}, captureLighting,
                            roomBehindCamera, doors, corridor, connectedRoom)) {
                        std::cerr << "Render failed for seed " << capture.seed << '\n';
                        passed = false;
                        return;
                    }
                    glReadPixels(
                        0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                    const std::filesystem::path path = outputDirectory
                        / (std::string{unit} + '_'
                            + capture.label + '_' + suffix + ".ppm");
                    if (!savePpm(path, width, height, pixels)) {
                        std::cerr << "Could not save " << path << '\n';
                        passed = false;
                    } else {
                        std::cout << path.string() << '\n';
                    }
                };
                captureView(gameplayCamera, "industrial_corridor_wall_v2_hd",
                    "gameplay", lighting);
                captureView(closeCamera, "industrial_corridor_wall_v2_hd",
                    "close", lighting);
                gdp::game::visual::RoomLightingRenderData ceilingLighting = lighting;
                ceilingLighting.fixtureEmissive = 0.22F;
                captureView(ceilingCamera, "industrial_corridor_ceiling_v1_hd",
                    "gameplay", ceilingLighting);
                captureView(ceilingCloseCamera, "industrial_corridor_ceiling_v1_hd",
                    "close", ceilingLighting);
                if (capture.seed == 2U) {
                    const auto captureWeapon = [&](const char* const label,
                        const gdp::game::visual::HudWeaponRole role,
                        const gdp::game::visual::CameraData view,
                        const gdp::game::visual::HudRenderData hud) {
                        const gdp::game::visual::WeaponViewmodelRenderData weapon{
                            .visible = true, .role = role, .visualSeed = 1717U};
                        if (!renderer.render(width, height, view, {}, {}, {},
                                lighting, roomBehindCamera, doors, corridor,
                                connectedRoom, hud, {}, weapon)) {
                            std::cerr << "Weapon render failed for " << label << '\n';
                            passed = false;
                            return;
                        }
                        const auto stats = renderer.lastFrameStats();
                        std::cout << "WEAPON_CAPTURE " << label
                            << " draws=" << stats.drawCalls
                            << " triangles=" << stats.triangles << '\n';
                        glReadPixels(0, 0, width, height, GL_RGBA,
                            GL_UNSIGNED_BYTE, pixels.data());
                        const std::filesystem::path path = outputDirectory
                            / (std::string{"weapon_first_person_v1_used_"}
                                + label + ".ppm");
                        if (!savePpm(path, width, height, pixels)) {
                            std::cerr << "Could not save " << path << '\n';
                            passed = false;
                        } else {
                            std::cout << path.string() << '\n';
                        }
                    };
                    auto closeWeaponCamera = gameplayCamera;
                    closeWeaponCamera.verticalFieldOfViewDegrees = 55.0F;
                    captureWeapon("pistol_gameplay",
                        gdp::game::visual::HudWeaponRole::Pistol, gameplayCamera, {});
                    captureWeapon("pistol_close",
                        gdp::game::visual::HudWeaponRole::Pistol, closeWeaponCamera, {});
                    captureWeapon("carbine_gameplay",
                        gdp::game::visual::HudWeaponRole::Carbine, gameplayCamera, {});
                    captureWeapon("carbine_close",
                        gdp::game::visual::HudWeaponRole::Carbine, closeWeaponCamera, {});
                    captureWeapon("knife_gameplay",
                        gdp::game::visual::HudWeaponRole::Knife, gameplayCamera, {});
                    captureWeapon("knife_close",
                        gdp::game::visual::HudWeaponRole::Knife, closeWeaponCamera, {});
                    auto minimumFovCamera = gameplayCamera;
                    minimumFovCamera.verticalFieldOfViewDegrees = 60.0F;
                    auto maximumFovCamera = gameplayCamera;
                    maximumFovCamera.verticalFieldOfViewDegrees = 100.0F;
                    captureWeapon("pistol_fov60",
                        gdp::game::visual::HudWeaponRole::Pistol, minimumFovCamera, {});
                    captureWeapon("pistol_fov100",
                        gdp::game::visual::HudWeaponRole::Pistol, maximumFovCamera, {});
                    captureWeapon("carbine_fov60",
                        gdp::game::visual::HudWeaponRole::Carbine, minimumFovCamera, {});
                    captureWeapon("carbine_fov100",
                        gdp::game::visual::HudWeaponRole::Carbine, maximumFovCamera, {});
                    captureWeapon("knife_fov60",
                        gdp::game::visual::HudWeaponRole::Knife, minimumFovCamera, {});
                    captureWeapon("knife_fov100",
                        gdp::game::visual::HudWeaponRole::Knife, maximumFovCamera, {});
                    gdp::game::visual::HudRenderData potatoHud;
                    potatoHud.graphicsQuality
                        = gdp::game::visual::HudGraphicsQuality::Potato;
                    potatoHud.detailedMaterials = false;
                    captureWeapon("carbine_potato",
                        gdp::game::visual::HudWeaponRole::Carbine, gameplayCamera,
                        potatoHud);
                    captureView(ceilingCamera, "ceiling_fixture_v1_hd",
                        "on", lighting);
                    gdp::game::visual::RoomLightingRenderData offLighting = lighting;
                    offLighting.fixtureEmissive = 0.0F;
                    for (std::size_t index = 0; index < offLighting.lightCount; ++index) {
                        offLighting.lights[index].intensity = 0.0F;
                    }
                    captureView(ceilingCamera, "ceiling_fixture_v1_hd",
                        "off", offLighting);
                    gdp::game::visual::RoomLightingRenderData alertLighting = lighting;
                    alertLighting.alert = true;
                    captureView(ceilingCamera, "ceiling_fixture_v1_hd",
                        "alert", alertLighting);
                }
            }
        }
    }

    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return passed ? 0 : 1;
}
