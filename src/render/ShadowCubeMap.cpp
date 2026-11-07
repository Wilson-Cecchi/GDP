#include "render/ShadowCubeMap.hpp"

#include "core/Log.hpp"

namespace wkc::render {

ShadowCubeMap::~ShadowCubeMap() { reset(); }

bool ShadowCubeMap::resize(const GlApi& gl, const int size)
{
    if (size <= 0) return false;
    if (isValid() && size_ == size) return true;
    reset();
    gl_ = &gl;
    size_ = size;
    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture_);
    for (unsigned int face = 0; face < 6U; ++face) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0,
            GL_DEPTH_COMPONENT24, size, size, 0, GL_DEPTH_COMPONENT,
            GL_UNSIGNED_INT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    gl.genFramebuffers(1, &framebuffer_);
    gl.bindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    gl.framebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
        GL_TEXTURE_CUBE_MAP_POSITIVE_X, texture_, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    const bool complete = gl.checkFramebufferStatus(GL_FRAMEBUFFER)
        == GL_FRAMEBUFFER_COMPLETE;
    gl.bindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    if (!complete) {
        wkc::core::log(wkc::core::LogLevel::Error,
            "OpenGL shadow cubemap framebuffer is incomplete");
        reset();
    }
    return complete;
}

void ShadowCubeMap::bindFace(const GlApi& gl, const unsigned int face) const noexcept
{
    gl.bindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    gl.framebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
        GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, texture_, 0);
}

void ShadowCubeMap::reset() noexcept
{
    if (gl_ && framebuffer_) gl_->deleteFramebuffers(1, &framebuffer_);
    if (texture_) glDeleteTextures(1, &texture_);
    framebuffer_ = texture_ = 0;
    size_ = 0;
    gl_ = nullptr;
}

} // namespace wkc::render
