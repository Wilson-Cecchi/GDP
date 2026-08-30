#include "render/Renderer.hpp"

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
        gdp::render::Renderer renderer;
        if (!renderer.initialize(width, height)) {
            std::cerr << "Renderer initialization failed\n";
            passed = false;
        } else {
            constexpr std::array corridorPoints{
                gdp::core::Vec3{0.0F, 0.0F, 4.0F},
                gdp::core::Vec3{0.0F, 0.0F, -14.0F},
            };
            const std::array corridor{
                gdp::render::CorridorRenderData{corridorPoints, 2.8F, 3.2F},
            };
            constexpr std::array doors{
                gdp::render::DoorRenderData{
                    {0.0F, 0.0F, -14.0F}, {0.0F, 0.0F, 1.0F},
                    2.8F, 3.2F, false, 1.0F},
            };
            constexpr gdp::render::RoomRenderData roomBehindCamera{
                {0.0F, 0.0F, 10.0F}, gdp::core::QuarterTurn::Degrees0, 0U,
                {{-4.0F, 0.0F, 5.0F}, {4.0F, 4.0F, 15.0F}}};
            constexpr std::array connectedRoom{
                gdp::render::RoomRenderData{
                    {0.0F, 0.0F, -18.0F}, gdp::core::QuarterTurn::Degrees0, 0U,
                    {{-4.8F, 0.0F, -22.0F}, {4.8F, 4.2F, -14.0F}}},
            };
            constexpr gdp::render::CameraData gameplayCamera{
                {0.0F, 1.58F, 3.25F}, {0.14F, -0.015F, -0.99F}, 70.0F};
            constexpr gdp::render::CameraData closeCamera{
                {0.15F, 1.55F, 1.80F}, {0.94F, -0.08F, -0.33F}, 58.0F};
            gdp::render::RoomLightingRenderData lighting;
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
                gdp::core::Seed seed;
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
                const auto captureView = [&](const gdp::render::CameraData view,
                                              const char* const suffix) {
                    if (!renderer.render(width, height, view, {}, {}, {}, lighting,
                            roomBehindCamera, doors, corridor, connectedRoom)) {
                        std::cerr << "Render failed for seed " << capture.seed << '\n';
                        passed = false;
                        return;
                    }
                    glReadPixels(
                        0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                    const std::filesystem::path path = outputDirectory
                        / (std::string{"industrial_corridor_wall_v2_hd_"}
                            + capture.label + '_' + suffix + ".ppm");
                    if (!savePpm(path, width, height, pixels)) {
                        std::cerr << "Could not save " << path << '\n';
                        passed = false;
                    } else {
                        std::cout << path.string() << '\n';
                    }
                };
                captureView(gameplayCamera, "gameplay");
                captureView(closeCamera, "close");
            }
        }
    }

    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return passed ? 0 : 1;
}
