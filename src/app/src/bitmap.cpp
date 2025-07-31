#include "pch.hpp"
#include "bitmap.hpp"

namespace scpp {

GPUBitmap::GPUBitmap(BitmapData&& bitmapData)
    : m_bitmapData{std::move(bitmapData)} {
    if (m_textureID != 0) {
        glDeleteTextures(1, &m_textureID);
    }

    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    GLuint internalFormat = [&]() {
        switch (m_bitmapData.format) {
        case BitmapFormat::RGBA8:
            return GL_RGBA8;
        case BitmapFormat::RGB8:
            return GL_RGB8;
        }
        return GL_RGBA8;
    }();

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat,
                 m_bitmapData.width, m_bitmapData.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, m_bitmapData.data.get());
}

void GPUBitmap::ImGuiImageRender() const {
    ImVec2 avail = ImGui::GetContentRegionAvail();

    float imgW   = static_cast<float>(m_bitmapData.width);
    float imgH   = static_cast<float>(m_bitmapData.height);
    float scaleX = avail.x / imgW;
    float scaleY = avail.y / imgH;
    float scale  = (std::min)((std::min)(scaleX, scaleY), 1.0f);

    ImGui::Image(
        (void*)(intptr_t)m_textureID,
        ImVec2(imgW * scale, imgH * scale));
}
} // namespace scpp
