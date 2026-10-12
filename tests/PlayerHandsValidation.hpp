#pragma once

#include "game/visual/GdpRenderer.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {
bool validatePlayerHands(gdp::game::visual::GdpRenderer& renderer)
{
    using namespace gdp::game::visual;
    const char* capture=std::getenv("GDP_HANDS_CAPTURE_DIR");
    constexpr std::array roles{HudWeaponRole::Pistol,HudWeaponRole::Carbine,HudWeaponRole::Knife,
        HudWeaponRole::Sniper,HudWeaponRole::Shotgun,HudWeaponRole::SubmachineGun};
    constexpr std::array names{"pistol","carbine","knife","sniper","shotgun","smg"};
    const std::array sizes=capture ? std::array{std::array{1280,720},std::array{960,600},std::array{320,200}}
        : std::array{std::array{320,200},std::array{384,240},std::array{480,200}};
    bool passed=true;
    for(const auto size:sizes) for(const bool potato:{false,true}) {
        const int width=size[0],height=size[1];
        HudRenderData hud;
        hud.detailedMaterials=!potato;
        hud.worldRenderScalePercent=potato ? 50U : 100U;
        hud.graphicsQuality=potato ? HudGraphicsQuality::Potato : HudGraphicsQuality::Medium;
        CameraData camera;
        const auto save=[&](const std::string& name,const std::vector<std::uint8_t>& pixels) {
            if(!capture) return;
            std::filesystem::create_directories(capture);
            std::vector<std::uint8_t> flipped(pixels.size());
            const auto stride=static_cast<std::size_t>(width*4);
            for(int y=0;y<height;++y) std::copy_n(pixels.data()+static_cast<std::size_t>(height-y-1)*stride,
                stride,flipped.data()+static_cast<std::size_t>(y)*stride);
            auto* surface=SDL_CreateSurfaceFrom(width,height,SDL_PIXELFORMAT_RGBA32,flipped.data(),width*4);
            const auto path=std::filesystem::path{capture}/(name+"_"+std::to_string(width)+"x"
                +std::to_string(height)+(potato ? "_potato.bmp" : "_medium.bmp"));
            passed &= surface && SDL_SaveBMP(surface,path.string().c_str());
            SDL_DestroySurface(surface);
        };
        for(std::size_t r=0;r<roles.size();++r) {
            WeaponViewmodelRenderData weapon{.visible=true,.role=roles[r],.visualSeed=1717U};
            const auto frame=[&]() {
                passed &= renderer.render(width,height,camera,{},{},{},{},{},{},{},{},hud,{},weapon);
                std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width*height*4));
                glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
                passed &= glGetError()==GL_NO_ERROR;
                return pixels;
            };
            camera.verticalFieldOfViewDegrees=60;
            weapon.handsVisible=false;
            const auto baseline=frame(); const auto stats=renderer.lastFrameStats();
            weapon.handsVisible=true;
            const auto idle=frame(); const auto withHands=renderer.lastFrameStats();
            passed &= baseline!=idle && frame()==idle;
            if(capture && width==1280 && !potato) save(std::string(names[r])+"_idle_fov60",idle);
            const unsigned int handTriangles=roles[r]==HudWeaponRole::Knife ? 2008U : 4016U;
            passed &= withHands.drawCalls==stats.drawCalls+3U && withHands.triangles==stats.triangles+handTriangles;
            // Contacts stay peripheral: no new silhouette covering aim at rest.
            for(int y=height/2-8;y<height/2+8;++y) for(int x=width/2-8;x<width/2+8;++x)
                for(int channel=0;channel<4;++channel) {
                    const auto i=static_cast<std::size_t>((y*width+x)*4+channel);
                    passed &= idle[i]==baseline[i];
                }
            std::vector<bool> footprint(static_cast<std::size_t>(width*height));
            for(std::size_t i=0;i<footprint.size();++i)
                footprint[i]=!std::equal(idle.begin()+static_cast<std::ptrdiff_t>(i*4),
                    idle.begin()+static_cast<std::ptrdiff_t>(i*4+3),baseline.begin()+static_cast<std::ptrdiff_t>(i*4));
            camera.verticalFieldOfViewDegrees=100;
            weapon.handsVisible=false; const auto wideBaseline=frame();
            weapon.handsVisible=true; const auto wide=frame();
            std::size_t mismatches=0;
            for(std::size_t i=0;i<footprint.size();++i) {
                const bool changed=!std::equal(wide.begin()+static_cast<std::ptrdiff_t>(i*4),
                    wide.begin()+static_cast<std::ptrdiff_t>(i*4+3),wideBaseline.begin()+static_cast<std::ptrdiff_t>(i*4));
                if(changed!=footprint[i]) ++mismatches;
            }
            passed &= mismatches<static_cast<std::size_t>(width*height)/100U;
            save(std::string(names[r])+"_idle",wide);
            camera.verticalFieldOfViewDegrees=68;
            for(const float progress:{.16F,.31F,.40F,.60F,.84F,.88F,.96F}) {
                weapon.reloadProgress=progress;
                const auto reloading=frame();
                weapon.handsVisible=false;
                passed &= frame()!=reloading;
                weapon.handsVisible=true;
                passed &= frame()==reloading;
                if(capture && width==1280 && !potato)
                    save(std::string(names[r])+"_reload_"+std::to_string(int(progress*100)),reloading);
            }
            weapon.reloadProgress=0;
            const auto original=frame();
            const auto savePose=[&](const std::string& pose,const std::vector<std::uint8_t>& pixels) {
                if(capture && width==1280 && !potato) save(std::string(names[r])+"_"+pose,pixels);
            };
            weapon.reloadProgress=1;
            passed &= frame()==original;
            weapon.reloadProgress=0;
            weapon.recoilStrength=1; const auto recoil=frame(); passed &= recoil!=original;
            savePose("recoil",recoil);
            weapon.recoilStrength=0;
            weapon.crouchStrength=1; const auto crouch=frame(); passed &= crouch!=original; savePose("crouch",crouch);
            weapon.crouchStrength=0; weapon.airborneStrength=1;
            const auto airborne=frame(); passed &= airborne!=original; savePose("airborne",airborne);
            weapon.airborneStrength=0; weapon.equipStrength=1;
            const auto equip=frame(); passed &= equip!=original; savePose("equip",equip);
            weapon.equipStrength=0;
            if(roles[r]==HudWeaponRole::Knife) {
                weapon.grenadeSelected=true;
                std::vector<bool> narrowHeldFootprint;
                std::vector<bool> narrowChargedFootprint;
                for(const int fov:{60,100}) {
                    camera.verticalFieldOfViewDegrees=static_cast<float>(fov);
                    weapon.grenadeChargeStrength=0.0F;
                    weapon.handsVisible=false;
                    const auto grenadeOnly=frame();
                    weapon.handsVisible=true;
                    const auto grenadeHeld=frame();
                    passed &= grenadeHeld!=grenadeOnly && frame()==grenadeHeld;
                    weapon.grenadeChargeStrength=1.0F;
                    weapon.handsVisible=false;
                    const auto grenadeChargedOnly=frame();
                    weapon.handsVisible=true;
                    const auto grenadeCharged=frame();
                    passed &= grenadeCharged!=grenadeHeld
                        && grenadeCharged!=grenadeChargedOnly && frame()==grenadeCharged;
                    if(capture && width==1280) {
                        save("grenade_held_fov"+std::to_string(fov),grenadeHeld);
                        save("grenade_charged_fov"+std::to_string(fov),grenadeCharged);
                    }
                    const auto grenadeFootprint=[&](const std::vector<std::uint8_t>& withHandPixels,
                                                    const std::vector<std::uint8_t>& withoutHandPixels) {
                        std::vector<bool> result(static_cast<std::size_t>(width*height));
                        for(std::size_t i=0;i<result.size();++i)
                            result[i]=!std::equal(withHandPixels.begin()+static_cast<std::ptrdiff_t>(i*4),
                                withHandPixels.begin()+static_cast<std::ptrdiff_t>(i*4+3),
                                withoutHandPixels.begin()+static_cast<std::ptrdiff_t>(i*4));
                        return result;
                    };
                    const auto heldFootprint=grenadeFootprint(grenadeHeld,grenadeOnly);
                    const auto chargedFootprint=grenadeFootprint(grenadeCharged,grenadeChargedOnly);
                    if(fov==60) {
                        narrowHeldFootprint=heldFootprint;
                        narrowChargedFootprint=chargedFootprint;
                    } else {
                        const auto grenadeMismatches=[&](const std::vector<bool>& narrowMask,
                                                        const std::vector<bool>& wideMask) {
                            std::size_t count=0;
                            for(std::size_t i=0;i<narrowMask.size();++i)
                                if(narrowMask[i]!=wideMask[i]) ++count;
                            return count;
                        };
                        passed &= grenadeMismatches(narrowHeldFootprint,heldFootprint)
                                <static_cast<std::size_t>(width*height)/100U;
                        passed &= grenadeMismatches(narrowChargedFootprint,chargedFootprint)
                                <static_cast<std::size_t>(width*height)/100U;
                    }
                }
                camera.verticalFieldOfViewDegrees=68;
                weapon.grenadeSelected=false;
                weapon.grenadeChargeStrength=0.0F;
                for(const float phase:{.25F,.50F,.75F,1.0F}) {
                    weapon.meleeStrength=phase; const auto melee=frame();
                    passed &= phase<1.0F ? melee!=original : melee==original;
                    savePose("attack_"+std::to_string(int(phase*100)),melee);
                }
                weapon.meleeStrength=0;
            }
            weapon.aimStrength=.5F; savePose("aim_half",frame()); weapon.aimStrength=0;
            if(roles[r]==HudWeaponRole::Sniper) {
                weapon.aimStrength=1; const auto scope=frame();
                weapon.handsVisible=false; passed &= frame()==scope;
                weapon.handsVisible=true; weapon.aimStrength=0;
            }
            for(const auto objective:{ObjectiveRenderState::Victory,ObjectiveRenderState::Defeat}) {
                hud.objective=objective;
                const auto result=frame(); weapon.handsVisible=false;
                passed &= frame()==result; weapon.handsVisible=true;
            }
            hud.objective=ObjectiveRenderState::ReachBoss; hud.mainMenu=true;
            const auto menu=frame(); weapon.handsVisible=false;
            passed &= frame()==menu; hud.mainMenu=false; weapon.handsVisible=true;
            weapon.visible=false; const auto empty=frame(); weapon.handsVisible=false;
            passed &= frame()==empty;
        }
    }
    if(!passed) std::cerr<<"Player hands failed contact/FOV/reset/low-spec pixel checks\n";
    else std::cout<<"Player hands correction: three draws, 4016/2008 triangles, six roles and reload poses checked\n";
    return passed;
}
} // namespace
