#include "render/Shader.hpp"

#include "core/Log.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace wkc::render {
namespace {

GLuint compileStage(
    const GlApi& gl,
    const GLenum type,
    const std::string_view source,
    const char* const label)
{
    const GLuint shader = gl.createShader(type);
    const char* sourcePointer = source.data();
    const GLint sourceLength = static_cast<GLint>(source.size());
    gl.shaderSource(shader, 1, &sourcePointer, &sourceLength);
    gl.compileShader(shader);

    GLint compiled = GL_FALSE;
    gl.getShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) {
        return shader;
    }

    GLint logLength = 0;
    gl.getShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    std::vector<char> buffer(static_cast<std::size_t>(std::max(logLength, 1)));
    gl.getShaderInfoLog(shader, logLength, nullptr, buffer.data());
    wkc::core::log(
        wkc::core::LogLevel::Error,
        std::string{label} + " shader compilation failed: " + buffer.data());
    gl.deleteShader(shader);
    return 0;
}

} // namespace

Shader::~Shader()
{
    reset();
}

Shader::Shader(Shader&& other) noexcept
    : gl_(std::exchange(other.gl_, nullptr))
    , program_(std::exchange(other.program_, 0))
{
}

Shader& Shader::operator=(Shader&& other) noexcept
{
    if (this != &other) {
        reset();
        gl_ = std::exchange(other.gl_, nullptr);
        program_ = std::exchange(other.program_, 0);
    }
    return *this;
}

bool Shader::compile(
    const GlApi& gl,
    const std::string_view vertexSource,
    const std::string_view fragmentSource)
{
    reset();
    const GLuint vertex = compileStage(gl, GL_VERTEX_SHADER, vertexSource, "Vertex");
    if (vertex == 0) {
        return false;
    }
    const GLuint fragment = compileStage(gl, GL_FRAGMENT_SHADER, fragmentSource, "Fragment");
    if (fragment == 0) {
        gl.deleteShader(vertex);
        return false;
    }

    const GLuint program = gl.createProgram();
    gl.attachShader(program, vertex);
    gl.attachShader(program, fragment);
    gl.linkProgram(program);
    gl.deleteShader(vertex);
    gl.deleteShader(fragment);

    GLint linked = GL_FALSE;
    gl.getProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        GLint logLength = 0;
        gl.getProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> buffer(static_cast<std::size_t>(std::max(logLength, 1)));
        gl.getProgramInfoLog(program, logLength, nullptr, buffer.data());
        wkc::core::log(
            wkc::core::LogLevel::Error,
            std::string{"Shader link failed: "} + buffer.data());
        gl.deleteProgram(program);
        return false;
    }

    gl_ = &gl;
    program_ = program;
    return true;
}

void Shader::use(const GlApi& gl) const noexcept
{
    gl.useProgram(program_);
}

void Shader::setMat4(
    const GlApi& gl,
    const char* const name,
    const gdp::core::Mat4& value) const noexcept
{
    const GLint location = gl.getUniformLocation(program_, name);
    if (location >= 0) {
        gl.uniformMatrix4fv(location, 1, GL_FALSE, value.data());
    }
}

void Shader::setVec3(
    const GlApi& gl,
    const char* const name,
    const gdp::core::Vec3 value) const noexcept
{
    const GLint location = gl.getUniformLocation(program_, name);
    if (location >= 0) {
        gl.uniform3f(location, value.x, value.y, value.z);
    }
}

void Shader::setVec2(
    const GlApi& gl,
    const char* const name,
    const gdp::core::Vec2 value) const noexcept
{
    const GLint location = gl.getUniformLocation(program_, name);
    if (location >= 0) {
        gl.uniform2f(location, value.x, value.y);
    }
}

void Shader::setInt(
    const GlApi& gl,
    const char* const name,
    const int value) const noexcept
{
    const GLint location = gl.getUniformLocation(program_, name);
    if (location >= 0) {
        gl.uniform1i(location, value);
    }
}

void Shader::setFloat(
    const GlApi& gl,
    const char* const name,
    const float value) const noexcept
{
    const GLint location = gl.getUniformLocation(program_, name);
    if (location >= 0) {
        gl.uniform1f(location, value);
    }
}

void Shader::reset() noexcept
{
    if (gl_ != nullptr && program_ != 0) {
        gl_->deleteProgram(program_);
    }
    gl_ = nullptr;
    program_ = 0;
}

bool Shader::isValid() const noexcept
{
    return program_ != 0;
}

} // namespace wkc::render
