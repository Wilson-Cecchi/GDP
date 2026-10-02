#pragma once

#include "game/visual/GdpRenderer.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {
bool validateWeaponReloads(gdp::game::visual::GdpRenderer& renderer)
{
    using namespace gdp::game::visual;
    const char* capture = std::getenv("GDP_RELOAD_CAPTURE_DIR");
    const int width = capture ? 640 : 320;
    const int height = capture ? 360 : 200;
    const std::array roles{HudWeaponRole::Pistol, HudWeaponRole::Carbine,
        HudWeaponRole::SubmachineGun, HudWeaponRole::Sniper, HudWeaponRole::Shotgun};
    const std::array names{"pistol", "carbine", "smg", "sniper", "shotgun"};
    bool passed = true;
    std::vector<std::uint8_t> carbineMid;
    for (std::size_t r = 0; r < roles.size(); ++r) {
        for (const bool potato : {false, true}) {
            HudRenderData hud;
            hud.detailedMaterials = !potato;
            hud.worldRenderScalePercent = potato ? 50U : 100U;
            hud.graphicsQuality = potato ? HudGraphicsQuality::Potato : HudGraphicsQuality::Medium;
            WeaponViewmodelRenderData model{.visible = true, .role = roles[r], .visualSeed = 1717U};
            const auto frame = [&](const float progress) {
                model.reloadProgress = progress;
                passed &= renderer.render(width, height, {}, {}, {}, {}, {}, {}, {}, {}, {}, hud, {}, model);
                std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height * 4));
                glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                passed &= glGetError() == GL_NO_ERROR;
                return pixels;
            };
            const auto idle = frame(0.0F);
            const auto mid = frame(0.45F);
            passed &= idle != mid && frame(1.0F) == idle && frame(0.0F) == idle;
            passed &= frame(0.85F) != idle;
            if (roles[r] == HudWeaponRole::Shotgun) {
                const auto loading = frame(0.45F);
                const auto draws = renderer.lastFrameStats().drawCalls;
                frame(0.0F);
                passed &= draws > renderer.lastFrameStats().drawCalls;
                model.reloadRoundCount = 1U;
                passed &= frame(0.45F) != loading;
                model.reloadRoundCount = 6U;
            }
            if (!potato && roles[r] == HudWeaponRole::Carbine) carbineMid = mid;
            if (!potato && roles[r] == HudWeaponRole::SubmachineGun) passed &= mid != carbineMid;
            if (capture && !potato) {
                std::filesystem::create_directories(capture);
                for (int i = 0; i <= 30; ++i) {
                    const auto pixels = frame(float(i) / 30.0F);
                    std::vector<std::uint8_t> flipped(pixels.size());
                    const auto stride = static_cast<std::size_t>(width * 4);
                    for (int y = 0; y < height; ++y) std::copy_n(
                        pixels.data() + static_cast<std::size_t>(height - y - 1) * stride,
                        stride, flipped.data() + static_cast<std::size_t>(y) * stride);
                    auto* surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32,
                        flipped.data(), width * 4);
                    const std::string number = (i < 10 ? "0" : "") + std::to_string(i);
                    const auto path = std::filesystem::path{capture}
                        / (std::string(names[r]) + "-" + number + ".bmp");
                    passed &= surface && SDL_SaveBMP(surface, path.string().c_str());
                    SDL_DestroySurface(surface);
                }
            }
        }
    }
    if (!passed) std::cerr << "Per-weapon reload animation failed pixel/return/low-spec checks\n";
    return passed;
}
} // namespace
