#pragma once

#include "pch.hpp"

namespace scpp {

enum class BitmapFormat {
    RGBA8,
    RGB8,
};

struct BitmapData {
    uint32_t                   width{0};
    uint32_t                   height{0};
    std::unique_ptr<uint8_t[]> data{nullptr};
    BitmapFormat               format{BitmapFormat::RGBA8};
};

class GPUBitmap {
private:
    BitmapData m_bitmapData;
    GLuint     m_textureID{0};

public:
    GPUBitmap(BitmapData&& bitmapData);

    GLuint GetTextureID() const {
        return m_textureID;
    }

    uint32_t GetWidth() const {
        return m_bitmapData.width;
    }

    uint32_t GetHeight() const {
        return m_bitmapData.height;
    }

    void ImGuiImageRender() const;
};

} // namespace scpp