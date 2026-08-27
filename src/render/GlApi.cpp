#include "render/GlApi.hpp"

#include "core/Log.hpp"

#include <SDL3/SDL_video.h>

#include <string>

namespace gdp::render {
namespace {

template<typename Function>
bool loadFunction(Function& destination, const char* const name) noexcept
{
    destination = reinterpret_cast<Function>(SDL_GL_GetProcAddress(name));
    if (destination == nullptr) {
        gdp::core::log(gdp::core::LogLevel::Error, std::string{"Missing OpenGL function: "} + name);
        return false;
    }
    return true;
}

} // namespace

bool loadGlApi(GlApi& api) noexcept
{
    bool loaded = true;
#define GDP_LOAD_GL(member, name) loaded = loadFunction(api.member, name) && loaded
    GDP_LOAD_GL(genVertexArrays, "glGenVertexArrays");
    GDP_LOAD_GL(bindVertexArray, "glBindVertexArray");
    GDP_LOAD_GL(deleteVertexArrays, "glDeleteVertexArrays");
    GDP_LOAD_GL(genBuffers, "glGenBuffers");
    GDP_LOAD_GL(bindBuffer, "glBindBuffer");
    GDP_LOAD_GL(bufferData, "glBufferData");
    GDP_LOAD_GL(deleteBuffers, "glDeleteBuffers");
    GDP_LOAD_GL(enableVertexAttribArray, "glEnableVertexAttribArray");
    GDP_LOAD_GL(vertexAttribPointer, "glVertexAttribPointer");
    GDP_LOAD_GL(createShader, "glCreateShader");
    GDP_LOAD_GL(shaderSource, "glShaderSource");
    GDP_LOAD_GL(compileShader, "glCompileShader");
    GDP_LOAD_GL(getShaderiv, "glGetShaderiv");
    GDP_LOAD_GL(getShaderInfoLog, "glGetShaderInfoLog");
    GDP_LOAD_GL(deleteShader, "glDeleteShader");
    GDP_LOAD_GL(createProgram, "glCreateProgram");
    GDP_LOAD_GL(attachShader, "glAttachShader");
    GDP_LOAD_GL(linkProgram, "glLinkProgram");
    GDP_LOAD_GL(getProgramiv, "glGetProgramiv");
    GDP_LOAD_GL(getProgramInfoLog, "glGetProgramInfoLog");
    GDP_LOAD_GL(deleteProgram, "glDeleteProgram");
    GDP_LOAD_GL(useProgram, "glUseProgram");
    GDP_LOAD_GL(getUniformLocation, "glGetUniformLocation");
    GDP_LOAD_GL(uniformMatrix4fv, "glUniformMatrix4fv");
    GDP_LOAD_GL(uniform3f, "glUniform3f");
    GDP_LOAD_GL(uniform1f, "glUniform1f");
    GDP_LOAD_GL(uniform1i, "glUniform1i");
    GDP_LOAD_GL(activeTexture, "glActiveTexture");
    GDP_LOAD_GL(generateMipmap, "glGenerateMipmap");
    GDP_LOAD_GL(genFramebuffers, "glGenFramebuffers");
    GDP_LOAD_GL(bindFramebuffer, "glBindFramebuffer");
    GDP_LOAD_GL(framebufferTexture2D, "glFramebufferTexture2D");
    GDP_LOAD_GL(checkFramebufferStatus, "glCheckFramebufferStatus");
    GDP_LOAD_GL(deleteFramebuffers, "glDeleteFramebuffers");
    GDP_LOAD_GL(blitFramebuffer, "glBlitFramebuffer");
    GDP_LOAD_GL(genRenderbuffers, "glGenRenderbuffers");
    GDP_LOAD_GL(bindRenderbuffer, "glBindRenderbuffer");
    GDP_LOAD_GL(renderbufferStorage, "glRenderbufferStorage");
    GDP_LOAD_GL(framebufferRenderbuffer, "glFramebufferRenderbuffer");
    GDP_LOAD_GL(deleteRenderbuffers, "glDeleteRenderbuffers");
#undef GDP_LOAD_GL
    return loaded;
}

} // namespace gdp::render
