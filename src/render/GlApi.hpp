#pragma once

#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>

namespace wkc::render {

struct GlApi {
    PFNGLGENVERTEXARRAYSPROC genVertexArrays{};
    PFNGLBINDVERTEXARRAYPROC bindVertexArray{};
    PFNGLDELETEVERTEXARRAYSPROC deleteVertexArrays{};
    PFNGLGENBUFFERSPROC genBuffers{};
    PFNGLBINDBUFFERPROC bindBuffer{};
    PFNGLBUFFERDATAPROC bufferData{};
    PFNGLDELETEBUFFERSPROC deleteBuffers{};
    PFNGLENABLEVERTEXATTRIBARRAYPROC enableVertexAttribArray{};
    PFNGLVERTEXATTRIBPOINTERPROC vertexAttribPointer{};
    PFNGLVERTEXATTRIBDIVISORPROC vertexAttribDivisor{};
    PFNGLDRAWELEMENTSINSTANCEDPROC drawElementsInstanced{};

    PFNGLCREATESHADERPROC createShader{};
    PFNGLSHADERSOURCEPROC shaderSource{};
    PFNGLCOMPILESHADERPROC compileShader{};
    PFNGLGETSHADERIVPROC getShaderiv{};
    PFNGLGETSHADERINFOLOGPROC getShaderInfoLog{};
    PFNGLDELETESHADERPROC deleteShader{};
    PFNGLCREATEPROGRAMPROC createProgram{};
    PFNGLATTACHSHADERPROC attachShader{};
    PFNGLLINKPROGRAMPROC linkProgram{};
    PFNGLGETPROGRAMIVPROC getProgramiv{};
    PFNGLGETPROGRAMINFOLOGPROC getProgramInfoLog{};
    PFNGLDELETEPROGRAMPROC deleteProgram{};
    PFNGLUSEPROGRAMPROC useProgram{};
    PFNGLGETUNIFORMLOCATIONPROC getUniformLocation{};
    PFNGLUNIFORMMATRIX4FVPROC uniformMatrix4fv{};
    PFNGLUNIFORM2FPROC uniform2f{};
    PFNGLUNIFORM3FPROC uniform3f{};
    PFNGLUNIFORM1FPROC uniform1f{};
    PFNGLUNIFORM1IPROC uniform1i{};

    PFNGLACTIVETEXTUREPROC activeTexture{};
    PFNGLGENERATEMIPMAPPROC generateMipmap{};

    PFNGLGENQUERIESPROC genQueries{};
    PFNGLDELETEQUERIESPROC deleteQueries{};
    PFNGLBEGINQUERYPROC beginQuery{};
    PFNGLENDQUERYPROC endQuery{};
    PFNGLGETQUERYOBJECTUIVPROC getQueryObjectuiv{};
    PFNGLGETQUERYOBJECTUI64VPROC getQueryObjectui64v{};

    PFNGLGENFRAMEBUFFERSPROC genFramebuffers{};
    PFNGLBINDFRAMEBUFFERPROC bindFramebuffer{};
    PFNGLFRAMEBUFFERTEXTURE2DPROC framebufferTexture2D{};
    PFNGLCHECKFRAMEBUFFERSTATUSPROC checkFramebufferStatus{};
    PFNGLDELETEFRAMEBUFFERSPROC deleteFramebuffers{};
    PFNGLBLITFRAMEBUFFERPROC blitFramebuffer{};
    PFNGLGENRENDERBUFFERSPROC genRenderbuffers{};
    PFNGLBINDRENDERBUFFERPROC bindRenderbuffer{};
    PFNGLRENDERBUFFERSTORAGEPROC renderbufferStorage{};
    PFNGLFRAMEBUFFERRENDERBUFFERPROC framebufferRenderbuffer{};
    PFNGLDELETERENDERBUFFERSPROC deleteRenderbuffers{};
};

[[nodiscard]] bool loadGlApi(GlApi& api) noexcept;

} // namespace wkc::render
