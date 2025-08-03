#include "cl_gl_utils.hpp"

namespace scpp {
void CreateNewGLRGBATexture(GLuint& textureID, Dims2D dims) {
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA8,
        dims.width, dims.height, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void DeleteGLRGBATexture(GLuint& textureID) {
    if (textureID != 0) {
        glDeleteTextures(1, &textureID);
        textureID = 0; // Reset to 0 after deletion
    }
}

CLGLTextureRGBA::CLGLTextureRGBA(Dims2D size, const cl::Context& context)
    : size{size} {
    CreateNewGLRGBATexture(glTextureID, size);

    cl_int res = CL_SUCCESS;

    clImageGL = cl::ImageGL(
        context,
        CL_MEM_WRITE_ONLY,
        GL_TEXTURE_2D,
        0,
        glTextureID,
        &res);

    if (res != CL_SUCCESS) {
        std::println("Failed to create OpenCL ImageGL: {}", res);
    }

    std::println("Created OpenCL texture {}: {} with size {}x{}", glTextureID, description, size.width, size.height);
}

CLGLTextureRGBA::CLGLTextureRGBA(std::string_view description, Dims2D size, const cl::Context& context)
    : CLGLTextureRGBA(size, context) {
    this->description = description;
}

CLGLTextureRGBA::CLGLTextureRGBA(CLGLTextureRGBA&& rhs) noexcept
    : description{rhs.description}
    , glTextureID{rhs.glTextureID}
    , clImageGL{std::move(rhs.clImageGL)}
    , size{rhs.size} {
    rhs.glTextureID = 0; // Transfer ownership explicitly
}

// Move assignment operator
CLGLTextureRGBA& CLGLTextureRGBA::operator=(CLGLTextureRGBA&& rhs) noexcept {
    if (this != &rhs) {
        if (glTextureID) {
            glDeleteTextures(1, &glTextureID);
            std::println("Deleted OpenGL texture (move-assignment): {}", glTextureID);
        }

        description = rhs.description;
        glTextureID = rhs.glTextureID;
        clImageGL   = std::move(rhs.clImageGL);
        size        = rhs.size;

        rhs.glTextureID = 0; // Transfer ownership explicitly
    }
    return *this;
}

void CLGLTextureRGBA::Resize(Dims2D newSize, const cl::Context& context) {
    if (newSize.width == size.width && newSize.height == size.height) {
        return; // No resize needed
    }

    size = newSize;
    DeleteGLRGBATexture(glTextureID);
    CreateNewGLRGBATexture(glTextureID, size);

    // Recreate the OpenCL image
    cl_int res = CL_SUCCESS;

    clImageGL = cl::ImageGL(
        context,
        CL_MEM_WRITE_ONLY,
        GL_TEXTURE_2D,
        0,
        glTextureID,
        &res);

    if (res != CL_SUCCESS) {
        std::println("Failed to recreate OpenCL ImageGL: {}", res);
    }

    const auto width  = clImageGL.getImageInfo<CL_IMAGE_WIDTH>(&res);
    const auto height = clImageGL.getImageInfo<CL_IMAGE_HEIGHT>(&res);
    if (res != CL_SUCCESS) {
        std::println("Failed to get OpenCL ImageGL size: {}", res);
        return;
    }

    std::println("Resized OpenCL texture {}: {} to {}x{}", glTextureID, description, size.width, size.height);
    std::println("OpenCL ImageGL size: {}x{}", width, height);
}
} // namespace scpp