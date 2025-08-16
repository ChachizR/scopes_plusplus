#pragma once

#include "pch.hpp"
#include "math_utils.hpp"

namespace scpp {
struct CLGLTextureRGBA {
    std::string_view description{};
    GLuint           glTextureID;
    cl::ImageGL      clImageGL;

    Dims2D size;

    CLGLTextureRGBA(Dims2D size, const cl::Context& context, std::string_view desc = ""sv);
    CLGLTextureRGBA(std::string_view desc, Dims2D size, const cl::Context& context);

    ~CLGLTextureRGBA() {
        if (glTextureID == 0)
            return;
        glDeleteTextures(1, &glTextureID);
        std::println("Deleted OpenGL texture {} '{}'\t ", glTextureID, description);
    }

    CLGLTextureRGBA(const CLGLTextureRGBA&)            = delete;
    CLGLTextureRGBA& operator=(const CLGLTextureRGBA&) = delete;
    CLGLTextureRGBA(CLGLTextureRGBA&& rhs) noexcept;
    CLGLTextureRGBA& operator=(CLGLTextureRGBA&& rhs) noexcept;

    // should be called on the main gl thread
    void Resize(Dims2D newSize, const cl::Context& context);
};
} // namespace scpp