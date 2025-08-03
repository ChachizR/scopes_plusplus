#pragma once

#include "pch.hpp"

#include "utils.hpp"
#include "cl_device_provider.hpp"

namespace scpp {

enum class ScaleBehavior {
    DoNotScale,
    OnlyScaleDown,
    ScaleToFit,
};

enum class FlipBehavior {
    DoNotFlip,
    FlipHorizontally,
    FlipVertically,
    FlipBoth,
};

static inline void ImGuiImageRender(
    const CLGLTextureRGBA& texture,
    ScaleBehavior          scaleBehavior = ScaleBehavior::OnlyScaleDown,
    FlipBehavior           flipBehavior  = FlipBehavior::DoNotFlip) {
    ImVec2 avail = ImGui::GetContentRegionAvail();

    float imgW   = static_cast<float>(texture.size.width);
    float imgH   = static_cast<float>(texture.size.height);
    float scaleX = avail.x / imgW;
    float scaleY = avail.y / imgH;
    float scale  = 1.f;

    switch (scaleBehavior) {
    case ScaleBehavior::DoNotScale:
        break;
    case ScaleBehavior::OnlyScaleDown:
        scale = (std::min)((std::min)(scaleX, scaleY), 1.f);
        break;
    case ScaleBehavior::ScaleToFit:
        scale = (std::min)(scaleX, scaleY);
        break;
    }

    const auto [uv0, uv1] = [&]() -> std::pair<ImVec2, ImVec2> {
        if (flipBehavior == FlipBehavior::DoNotFlip) {
            return {ImVec2(0, 0), ImVec2(1, 1)};
        } else if (flipBehavior == FlipBehavior::FlipHorizontally) {
            return {ImVec2(1, 0), ImVec2(0, 1)};
        } else if (flipBehavior == FlipBehavior::FlipVertically) {
            return {ImVec2(0, 1), ImVec2(1, 0)};
        } else { // FlipBoth
            return {ImVec2(1, 1), ImVec2(0, 0)};
        }
    }();

    const auto imTextureID = static_cast<ImTextureID>(texture.glTextureID);

    ImGui::ImageWithBg(imTextureID, ImVec2(imgW * scale, imgH * scale), uv0, uv1,ImVec4(0.f,0.f,0.f,1.f));
}

struct TargetTextures {
    CLGLTextureRGBA sourcePreview;
    CLGLTextureRGBA falseColor;
    CLGLTextureRGBA wfLuma;
    CLGLTextureRGBA wfRGB;
    CLGLTextureRGBA wfRGBParade;
    CLGLTextureRGBA wfRGBBlacks;
    CLGLTextureRGBA wfYUVParade;

    TargetTextures(CLGLTextureRGBA&& sourcePreview,
                   CLGLTextureRGBA&& falseColor,
                   CLGLTextureRGBA&& wfLuma,
                   CLGLTextureRGBA&& wfRGB,
                   CLGLTextureRGBA&& wfRGBParade,
                   CLGLTextureRGBA&& wfRGBBlacks,
                   CLGLTextureRGBA&& wfYUVParade)

        : sourcePreview{std::move(sourcePreview)}
        , falseColor{std::move(falseColor)}
        , wfLuma{std::move(wfLuma)}
        , wfRGB{std::move(wfRGB)}
        , wfRGBParade{std::move(wfRGBParade)}
        , wfRGBBlacks{std::move(wfRGBBlacks)}
        , wfYUVParade{std::move(wfYUVParade)} {}
};

enum class SourceFormat {
    unknown = 0,
    RGBA_8888,
    RGBX_8888,
    BGRA_8888,
    BGRX_8888,
    ARGB_8888,
    RGB_888,
    BGR_888_InvY,
    UYVY_422,
    YUYV_422,
    NV12,
    UYVY10_422,
};

[[nodiscard]]
static inline constexpr auto SourceFormatFromNDIFourCC(NDIlib_FourCC_video_type_e fourcc) noexcept -> SourceFormat {
    switch (fourcc) {
    case NDIlib_FourCC_type_RGBA:
        return SourceFormat::RGBA_8888;
    case NDIlib_FourCC_type_BGRA:
        return SourceFormat::BGRA_8888;
    case NDIlib_FourCC_type_BGRX:
        return SourceFormat::BGRX_8888;
    case NDIlib_FourCC_type_RGBX:
        return SourceFormat::RGBX_8888;
    case NDIlib_FourCC_type_NV12:
        return SourceFormat::NV12;
    case NDIlib_FourCC_type_UYVY:
        return SourceFormat::UYVY_422;
    }
    return SourceFormat::unknown;
}

enum class YUVColorSpace : int32_t {
    unknown = 0,
    BT601,
    BT709,
    BT2020,
};

[[nodiscard]]
static inline constexpr auto GetSourceSize(SourceFormat format, Dims2D size) noexcept -> uint64_t {
    switch (format) {
    case SourceFormat::unknown:
        return 0;
    case SourceFormat::RGBA_8888:
    case SourceFormat::RGBX_8888:
    case SourceFormat::BGRA_8888:
    case SourceFormat::ARGB_8888:
    case SourceFormat::BGRX_8888:
        return 4ull * size.width * size.height;
    case SourceFormat::BGR_888_InvY:
    case SourceFormat::RGB_888:
        return 3ull * size.width * size.height;
    case SourceFormat::UYVY_422:
    case SourceFormat::YUYV_422:
        return 2ull * size.width * size.height;
    case SourceFormat::NV12:
        return static_cast<uint64_t>(ceilf(1.5f * size.width * size.height));
    case SourceFormat::UYVY10_422:
        return static_cast<uint64_t>((47 + size.width / 48) * 128 * size.height);
    }
    return 0;
}

class OpenCLRenderer {
private:
    const OpenCLDeviceProvider& m_deviceProviderRef;

    cl::Device       m_device;
    cl::Context      m_context;
    cl::CommandQueue m_commandQueue;

    bool m_initialized{false};

    static constexpr Dims2D c_DefaultSourceSize{1920, 1080};
    static constexpr Dims2D c_WaveformSize{580, 256};
    static constexpr Dims2D c_ScopeSize{256, 256};

    std::unique_ptr<TargetTextures> m_targetTextures = nullptr;

    RenderPipelineKernels m_kernels;

    uint64_t m_sourceSizeBytes{0};
    uint64_t m_rgbaSizeBytes{0};
    uint64_t m_yuvSizeBytes{0};

    cl::Buffer m_bufSource;
    cl::Buffer m_bufIntermRGBA;
    cl::Buffer m_bufIntermYUV;

    cl::Buffer m_bufAccRGB, m_bufAccYUV;
    cl::Buffer m_bufAcc2D_UV_RGBA, m_bufAcc2D_XYZ_RGBA, m_bufAcc2D_DIA_RGBA;

    bool m_buffersInitialized{false};

    std::vector<cl::Memory> m_glObjects;

    Dims2D       m_sourceDims{c_DefaultSourceSize};
    SourceFormat m_sourceFormat{SourceFormat::RGBA_8888};

    [[nodiscard]]
    constexpr inline auto SelectConvertKernel(SourceFormat sourceFormat) -> cl::Kernel&;

public:
    // should be called on the main gl thread
    OpenCLRenderer(const OpenCLDeviceProvider& deviceProviderRef);

    OpenCLRenderer(const OpenCLRenderer&)            = delete;
    OpenCLRenderer& operator=(const OpenCLRenderer&) = delete;
    OpenCLRenderer(OpenCLRenderer&&)                 = delete;
    OpenCLRenderer& operator=(OpenCLRenderer&&)      = delete;

    // checked by the main gl thread
    bool needsResizeFlag_mainThread{false};

    // should be called on the main gl thread
    void ResizeGLTextures();

private:
    void UpdateGLObjects() {
        m_glObjects = {
            m_targetTextures->sourcePreview.clImageGL,
            m_targetTextures->falseColor.clImageGL,
            m_targetTextures->wfLuma.clImageGL,
            m_targetTextures->wfRGB.clImageGL,
            m_targetTextures->wfRGBParade.clImageGL,
            m_targetTextures->wfRGBBlacks.clImageGL,
            m_targetTextures->wfYUVParade.clImageGL};
    }

    void ResizeBuffers();

public:
    [[nodiscard]]
    auto GetTargetTextures() const noexcept -> const TargetTextures* const {
        return m_targetTextures.get();
    }

    void ExecutePipeline(
        const uint8_t* sourceData,
        Dims2D         sourceSize,
        SourceFormat   sourceFormat);
};

} // namespace scpp