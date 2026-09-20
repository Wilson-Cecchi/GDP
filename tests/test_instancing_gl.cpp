#include "core/Math.hpp"
#include "render/GlApi.hpp"
#include "render/Mesh.hpp"
#include "render/Shader.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>

namespace {

bool pixelIsLit(const int x, const int y)
{
    std::array<std::uint8_t, 4U> pixel{};
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    return pixel[0] > 200U && pixel[1] > 200U && pixel[2] > 200U;
}

} // namespace

int main()
{
    constexpr int width = 96;
    constexpr int height = 64;
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }
    if (!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3)
        || !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3)
        || !SDL_GL_SetAttribute(
            SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE)) {
        std::cerr << "SDL_GL_SetAttribute failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    SDL_Window* const window = SDL_CreateWindow(
        "WKC Forge instancing test", width, height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (window == nullptr) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }
    SDL_GLContext const context = SDL_GL_CreateContext(window);
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
        wkc::render::GlApi gl;
        wkc::render::Mesh mesh;
        wkc::render::Shader shader;
        wkc::render::MeshBindingCache bindings;
        constexpr auto vertexSource = R"GLSL(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 4) in mat4 aInstanceModel;
void main()
{
    gl_Position = aInstanceModel * vec4(aPosition, 1.0);
}
)GLSL";
        constexpr auto fragmentSource = R"GLSL(#version 330 core
out vec4 fragmentColor;
void main()
{
    fragmentColor = vec4(1.0);
}
)GLSL";

        passed = wkc::render::loadGlApi(gl)
            && mesh.upload(gl, wkc::render::makeBoxMesh())
            && shader.compile(gl, vertexSource, fragmentSource);
        if (!passed) {
            std::cerr << "Instancing test resources failed to initialize\n";
        } else {
            const std::array transforms{
                wkc::core::translationMatrix({-0.55F, 0.0F, 0.0F})
                    * wkc::core::scaleMatrix({0.5F, 0.5F, 0.5F}),
                wkc::core::translationMatrix({0.55F, 0.0F, 0.0F})
                    * wkc::core::scaleMatrix({0.5F, 0.5F, 0.5F}),
            };
            passed = mesh.uploadInstanceTransforms(gl, transforms, bindings)
                && mesh.instanceCount() == transforms.size();

            auto invalidTransforms = transforms;
            invalidTransforms.front().elements[0]
                = std::numeric_limits<float>::quiet_NaN();
            passed = passed
                && !mesh.uploadInstanceTransforms(
                    gl, invalidTransforms, bindings)
                && mesh.instanceCount() == transforms.size();

            glViewport(0, 0, width, height);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
            glClear(GL_COLOR_BUFFER_BIT);
            shader.use(gl);
            mesh.drawInstanced(gl, bindings);
            const bool drewBothInstances = pixelIsLit(22, height / 2)
                && pixelIsLit(74, height / 2)
                && !pixelIsLit(width / 2, height / 2);
            const auto bindingStats = bindings.stats();
            passed = passed && drewBothInstances
                && bindingStats.requests == 1U
                && bindingStats.vertexArrayBinds == 1U;

            passed = passed
                && mesh.uploadInstanceTransforms(gl, {}, bindings)
                && mesh.instanceCount() == 0U;
            glClear(GL_COLOR_BUFFER_BIT);
            mesh.drawInstanced(gl, bindings);
            passed = passed && !pixelIsLit(22, height / 2)
                && glGetError() == GL_NO_ERROR;
        }
    }

    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (!passed) {
        std::cerr << "WKC Forge instancing regression failed\n";
        return 1;
    }
    std::cout << "WKC Forge instancing regression passed\n";
    return 0;
}
