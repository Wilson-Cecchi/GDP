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
#include <numbers>
#include <string>
#include <vector>

namespace {
bool validatePlayerBody(gdp::game::visual::GdpRenderer& renderer)
{
    using namespace gdp::game::visual;
    const char* capture = std::getenv("GDP_PLAYER_BODY_CAPTURE_DIR");
    const std::array sizes = capture ? std::array{std::array{1280,720},std::array{960,600},std::array{320,200}}
        : std::array{std::array{320,200},std::array{384,240},std::array{480,200}};
    bool passed = true;
    for (const auto size : sizes) for (const bool potato : {false,true})
    for (const float fov : {60.0F,100.0F}) {
        const int width = size[0], height = size[1];
        HudRenderData hud;
        hud.detailedMaterials = !potato;
        hud.worldRenderScalePercent = potato ? 50U : 100U;
        hud.graphicsQuality = potato ? HudGraphicsQuality::Potato : HudGraphicsQuality::Medium;
        RoomLightingRenderData lighting;
        lighting.ambientIntensity = .45F;
        lighting.ambientColor = {.82F,.87F,.95F};
        CameraData camera{{0,1.62F,0},wkc::core::normalized({0,-1,-.25F}),fov};
        PlayerBodyRenderData body{.visible=true};
        const auto frame = [&](const std::span<const PropRenderData> props = {},
                               const WeaponViewmodelRenderData weapon = {}) {
            passed &= renderer.render(width,height,camera,{},{},props,lighting,{},{},{},{},hud,{},weapon,{},body);
            passed &= glGetError() == GL_NO_ERROR;
            std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width*height*4));
            glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
            return pixels;
        };
        body.visible = false;
        const auto baseline = frame();
        const auto baselineStats = renderer.lastFrameStats();
        body.visible = true;
        const auto standing = frame();
        const auto bodyStats = renderer.lastFrameStats();
        passed &= standing != baseline && frame() == standing;
        passed &= bodyStats.drawCalls == baselineStats.drawCalls+4U
            && bodyStats.triangles == baselineStats.triangles+2096U;
        const auto save = [&](const std::string& name, const std::vector<std::uint8_t>& pixels) {
            if (!capture) return;
            std::filesystem::create_directories(capture);
            std::vector<std::uint8_t> flipped(pixels.size());
            const auto stride = static_cast<std::size_t>(width*4);
            for (int y = 0; y < height; ++y) std::copy_n(pixels.data()+static_cast<std::size_t>(height-y-1)*stride,
                stride,flipped.data()+static_cast<std::size_t>(y)*stride);
            auto* surface = SDL_CreateSurfaceFrom(width,height,SDL_PIXELFORMAT_RGBA32,flipped.data(),width*4);
            const auto path = std::filesystem::path{capture} / (name+"_"+std::to_string(width)+"x"
                +std::to_string(height)+"_fov"+std::to_string(static_cast<int>(fov))
                +(potato ? "_potato.bmp" : "_medium.bmp"));
            passed &= surface && SDL_SaveBMP(surface,path.string().c_str());
            SDL_DestroySurface(surface);
        };
        save("standing",standing);
        body.gaitWeight = 1; body.gaitPhase = 1.1F; body.forwardMovement = 1;
        const auto walking = frame();
        passed &= walking != standing;
        save("walking",walking);
        if (capture && width == 960 && !potato && fov == 60.0F) {
            for (int i = 0; i < 24; ++i) {
                body.gaitPhase = static_cast<float>(i)/24.0F*2.0F*std::numbers::pi_v<float>;
                save("gait_"+std::string(i < 10 ? "0" : "")+std::to_string(i),frame());
            }
            body.gaitPhase = 1.1F;
        }
        body.forwardMovement = -1;
        passed &= frame() != walking;
        body.forwardMovement = 0; body.rightMovement = 1;
        passed &= frame() != walking;
        body.gaitWeight = 0; body.rightMovement = 0;
        body.crouchAmount = 1; camera.position.y = 1.02F;
        const auto crouched = frame();
        body.visible = false;
        passed &= crouched != frame();
        body.visible = true;
        save("crouched",crouched);
        body.crouchAmount = 0; body.airborneStrength = 1;
        body.feetPosition.y = .7F; camera.position.y = 2.32F;
        const auto airborne = frame();
        body.visible = false;
        passed &= airborne != frame();
        body.visible = true;
        save("airborne",airborne);
        body = {.visible=true}; camera.position.y = 1.62F;
        // A solid crate above the body must occlude it, unlike the near weapon pass.
        const std::array cover{PropRenderData{{0,0,0},{2,1.45F,2},PropVisualRole::Crate}};
        const auto covered = frame(cover);
        body.visible = false;
        passed &= covered == frame(cover);
        body.visible = true; camera.forward = {0,0,-1};
        const auto level = frame();
        body.visible = false;
        passed &= level == frame(); // No own head/neck or floating legs on horizon.
        body.visible = true; camera.forward = wkc::core::normalized({0,-1,-.25F});
        WeaponViewmodelRenderData weapon{.visible=true,.role=HudWeaponRole::Carbine};
        const auto armed = frame({},weapon);
        passed &= armed != standing;
        save("armed",armed);
        weapon.role = HudWeaponRole::Sniper; weapon.aimStrength = 1;
        passed &= frame({},weapon) != armed;
        hud.mainMenu = true;
        const auto menu = frame();
        body.visible = false;
        passed &= menu == frame();
    }
    if (!passed) std::cerr << "Player body failed pose/occlusion/FOV/low-spec pixel checks\n";
    else std::cout << "Player body: four draws, 2096 triangles, world-depth and pose pixels checked\n";
    return passed;
}
} // namespace
