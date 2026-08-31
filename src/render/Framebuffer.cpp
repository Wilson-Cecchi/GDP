#include "render/Framebuffer.hpp"

#include "core/Log.hpp"

#include <utility>

namespace wkc::render {

Framebuffer::~Framebuffer()
{
    reset();
}

Framebuffer::Framebuffer(Framebuffer&& other) noexcept
    : gl_(std::exchange(other.gl_, nullptr))
    , framebuffer_(std::exchange(other.framebuffer_, 0))
    , colorTexture_(std::exchange(other.colorTexture_, 0))
    , depthBuffer_(std::exchange(other.depthBuffer_, 0))
    , width_(std::exchange(other.width_, 0))
    , height_(std::exchange(other.height_, 0))
{
}

Framebuffer& Framebuffer::operator=(Framebuffer&& other) noexcept
{
    if (this != &other) {
        reset();
        gl_ = std::exchange(other.gl_, nullptr);
        framebuffer_ = std::exchange(other.framebuffer_, 0);
        colorTexture_ = std::exchange(other.colorTexture_, 0);
        depthBuffer_ = std::exchange(other.depthBuffer_, 0);
        width_ = std::exchange(other.width_, 0);
        height_ = std::exchange(other.height_, 0);
    }
    return *this;
}

bool Framebuffer::resize(const GlApi& gl, const int width, const int height)
{
    if (width <= 0 || height <= 0) {
        return false;
    }
    if (isValid() && width == width_ && height == height_) {
        return true;
    }

    reset();
    gl_ = &gl;
    width_ = width;
    height_ = height;

    gl.genFramebuffers(1, &framebuffer_);
    gl.bindFramebuffer(GL_FRAMEBUFFER, framebuffer_);

    glGenTextures(1, &colorTexture_);
    glBindTexture(GL_TEXTURE_2D, colorTexture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        width_,
        height_,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        nullptr);
    gl.framebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        colorTexture_,
        0);

    gl.genRenderbuffers(1, &depthBuffer_);
    gl.bindRenderbuffer(GL_RENDERBUFFER, depthBuffer_);
    gl.renderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width_, height_);
    gl.framebufferRenderbuffer(
        GL_FRAMEBUFFER,
        GL_DEPTH_ATTACHMENT,
        GL_RENDERBUFFER,
        depthBuffer_);

    const GLenum status = gl.checkFramebufferStatus(GL_FRAMEBUFFER);
    gl.bindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    gl.bindRenderbuffer(GL_RENDERBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        gdp::core::log(gdp::core::LogLevel::Error, "OpenGL framebuffer is incomplete");
        reset();
        return false;
    }
    return true;
}

void Framebuffer::bind(const GlApi& gl) const noexcept
{
    gl.bindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
}

void Framebuffer::presentToDefault(
    const GlApi& gl,
    const int targetWidth,
    const int targetHeight) const noexcept
{
    gl.bindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer_);
    gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    gl.blitFramebuffer(
        0,
        0,
        width_,
        height_,
        0,
        0,
        targetWidth,
        targetHeight,
        GL_COLOR_BUFFER_BIT,
        GL_NEAREST);
    gl.bindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::reset() noexcept
{
    if (gl_ != nullptr) {
        if (depthBuffer_ != 0) {
            gl_->deleteRenderbuffers(1, &depthBuffer_);
        }
        if (framebuffer_ != 0) {
            gl_->deleteFramebuffers(1, &framebuffer_);
        }
    }
    if (colorTexture_ != 0) {
        glDeleteTextures(1, &colorTexture_);
    }
    gl_ = nullptr;
    framebuffer_ = 0;
    colorTexture_ = 0;
    depthBuffer_ = 0;
    width_ = 0;
    height_ = 0;
}

bool Framebuffer::isValid() const noexcept
{
    return framebuffer_ != 0 && colorTexture_ != 0 && depthBuffer_ != 0;
}

int Framebuffer::width() const noexcept
{
    return width_;
}

int Framebuffer::height() const noexcept
{
    return height_;
}

} // namespace wkc::render
