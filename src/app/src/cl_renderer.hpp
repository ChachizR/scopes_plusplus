#pragma once

#include "pch.hpp"

#include "utils.hpp"
#include "cl_device_provider.hpp"
#include "false_color.hpp"

namespace scpp {

struct TargetTextures {
    CLGLTextureRGBA sourcePreview;
    CLGLTextureRGBA falseColor;
    CLGLTextureRGBA wfLuma;
    CLGLTextureRGBA wfRGB;
    CLGLTextureRGBA wfRGBParade;
    CLGLTextureRGBA wfRGBBlacks;
    CLGLTextureRGBA wfYUVParade;
    CLGLTextureRGBA scUV;
    CLGLTextureRGBA scXYZ;
    CLGLTextureRGBA scDia;

    TargetTextures(CLGLTextureRGBA&& sourcePreview,
                   CLGLTextureRGBA&& falseColor,
                   CLGLTextureRGBA&& wfLuma,
                   CLGLTextureRGBA&& wfRGB,
                   CLGLTextureRGBA&& wfRGBParade,
                   CLGLTextureRGBA&& wfRGBBlacks,
                   CLGLTextureRGBA&& wfYUVParade,
                   CLGLTextureRGBA&& scUV,
                   CLGLTextureRGBA&& scXYZ,
                   CLGLTextureRGBA&& scDia)

        : sourcePreview{std::move(sourcePreview)}
        , falseColor{std::move(falseColor)}
        , wfLuma{std::move(wfLuma)}
        , wfRGB{std::move(wfRGB)}
        , wfRGBParade{std::move(wfRGBParade)}
        , wfRGBBlacks{std::move(wfRGBBlacks)}
        , wfYUVParade{std::move(wfYUVParade)}
        , scUV{std::move(scUV)}
        , scXYZ{std::move(scXYZ)}
        , scDia{std::move(scDia)} {}
};

enum class SourceColorSpace {
    Linear_RGB = 0,
    BT601_525  = 1,
    BT601_625  = 2,
    BT709      = 3,
    BT2020     = 4,
    SRGB       = 5,
    max // only for counting
};

[[nodiscard]]
static inline constexpr auto SourceColorSpaceToString(SourceColorSpace colorSpace) noexcept -> std::string_view {
    switch (colorSpace) {
    case SourceColorSpace::Linear_RGB:
        return "Linear RGB";
    case SourceColorSpace::BT601_525:
        return "BT.601 525";
    case SourceColorSpace::BT601_625:
        return "BT.601 625";
    case SourceColorSpace::BT709:
        return "BT.709";
    case SourceColorSpace::BT2020:
        return "BT.2020";
    case SourceColorSpace::SRGB:
        return "sRGB";
    }
    return "Unknown Color Space";
}

enum class SourceYUVRange {
    Limited = 0, // 16-235 for Y, 16-240 for UV
    Full    = 1, // 0-255 for Y, 0-255 for UV
    max          // only for counting
};

[[nodiscard]]
static inline constexpr auto SourceYUVRangeToString(SourceYUVRange yuvRange) noexcept -> std::string_view {
    switch (yuvRange) {
    case SourceYUVRange::Limited:
        return "Limited Range";
    case SourceYUVRange::Full:
        return "Full Range";
    }
    return "Unknown YUV Range";
}

using RenderFeatureFlags = cl_uint;

enum RenderFeature : RenderFeatureFlags {
    None        = 0,
    FalseColor  = 1 << 0,
    WFLuma      = 1 << 1,
    WFRgb       = 1 << 2,
    WFRgbParade = 1 << 3,
    WFRgbBlacks = 1 << 4,
    WFYuvParade = 1 << 5,
    SCUV        = 1 << 6,
    SCXYZ       = 1 << 7,
    SCDia       = 1 << 8,
    AnyWFRgb    = WFRgb | WFRgbParade | WFRgbBlacks,
    AnyWFYUV    = WFLuma | WFYuvParade,
    AnyWF       = AnyWFRgb | AnyWFYUV,
    AnySC       = SCUV | SCXYZ | SCDia,
    All         = 0xFFFFFFFF
};

struct RenderSettings {
    SourceColorSpace   colorSpace{SourceColorSpace::BT709};
    SourceYUVRange     yuvRange{SourceYUVRange::Limited};
    RenderFeatureFlags enabledFeatures{RenderFeature::All};
};

constexpr static inline void SetRenderFeatureFlag(RenderFeatureFlags& flags, RenderFeature feature, bool enabled = true) noexcept {
    if (enabled && (~flags & feature)) {
        flags |= feature;
    } else if (!enabled && (flags & feature)) {
        flags &= ~feature;
    }
}

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
    UYVA_4224,
    NV12,
    UYVY10_422,
    P216
};

#if defined(SCPP_ENABLE_NDI)
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
    case NDIlib_FourCC_type_UYVY:
        return SourceFormat::UYVY_422;
    case NDIlib_FourCC_type_UYVA:
        return SourceFormat::UYVA_4224;
    case NDIlib_FourCC_type_P216:
        return SourceFormat::P216;
    case NDIlib_FourCC_type_NV12:
        return SourceFormat::NV12;
    }
    return SourceFormat::unknown;
}
#endif

[[nodiscard]]
static inline constexpr auto SourceFormatToString(SourceFormat format) noexcept -> std::string_view {
    switch (format) {
    case SourceFormat::unknown:
        return "Unknown";
    case SourceFormat::RGBA_8888:
        return "RGBA 8888";
    case SourceFormat::RGBX_8888:
        return "RGBX 8888";
    case SourceFormat::BGRA_8888:
        return "BGRA 8888";
    case SourceFormat::BGRX_8888:
        return "BGRX 8888";
    case SourceFormat::ARGB_8888:
        return "ARGB 8888";
    case SourceFormat::RGB_888:
        return "RGB 888";
    case SourceFormat::BGR_888_InvY:
        return "BGR 888 (Inverted Y)";
    case SourceFormat::UYVY_422:
        return "UYVY 422";
    case SourceFormat::YUYV_422:
        return "YUYV 422";
    case SourceFormat::UYVA_4224:
        return "UYVA 4224";
    case SourceFormat::NV12:
        return "NV12";
    case SourceFormat::UYVY10_422:
        return "UYVY10 422";
    case SourceFormat::P216:
        return "P216";
    }
    return "Unknown Format";
}

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
        return 4ull * size.Area();
    case SourceFormat::BGR_888_InvY:
    case SourceFormat::RGB_888:
        return 3ull * size.Area();
    case SourceFormat::UYVY_422:
    case SourceFormat::YUYV_422:
        return 2ull * size.Area();
    case SourceFormat::UYVA_4224:
        return 3ull * size.Area();
    case SourceFormat::P216:
        return 2ull * size.Area() * sizeof(uint16_t);
    case SourceFormat::NV12:
        return static_cast<uint64_t>(ceilf(1.5f * size.Area()));
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

    static constexpr Dims2D c_DefaultSourceSize{1920u, 1080u};
    static constexpr Dims2D c_WaveformSize{580u, 256u};
    static constexpr Dims2D c_ScopeSize{256u, 256u};

    std::unique_ptr<TargetTextures> m_targetTextures = nullptr;

    RenderPipelineKernels m_kernels;

    uint64_t m_sourceSizeBytes{0};
    uint64_t m_rgbaSizeBytes{0};
    uint64_t m_yuvSizeBytes{0};

    cl::Buffer m_bufSource;
    cl::Buffer m_bufIntermRGBA;
    cl::Buffer m_bufIntermYUV;
    cl::Buffer m_bufIntermXYZ;

    cl::Buffer m_bufAccRGB, m_bufAccYUV;
    cl::Buffer m_bufAcc2D_UV_RGBA, m_bufAcc2D_XYZ_RGBA, m_bufAcc2D_DIA_RGBA;

    FalseColorMap    m_falseColorMap{c_falseColorMaps[0].second};
    bool             m_falseColorMapChanged = true;
    cl::Buffer       m_bufFalseColorMapFull;
    cl::Buffer       m_bufFalseColorMapLimited;
    std::string_view m_selectedFalseColorMapName{c_falseColorMaps[0].first};

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

    [[nodiscard]]
    auto GetFalseColorMap() const noexcept -> const FalseColorMap& { return m_falseColorMap; }

    [[nodiscard]]
    auto GetSelectedFalseColorMapName() const noexcept -> std::string_view { return m_selectedFalseColorMapName; }

    void SetFalseColorMap(FalseColorMapDataRef map, std::string_view name = ""sv) {
        m_falseColorMap             = FalseColorMap(map);
        m_selectedFalseColorMapName = name;
        m_falseColorMapChanged      = true;
    }

private:
    void UpdateGLObjects() {
        m_glObjects = {
            m_targetTextures->sourcePreview.clImageGL,
            m_targetTextures->falseColor.clImageGL,
            m_targetTextures->wfLuma.clImageGL,
            m_targetTextures->wfRGB.clImageGL,
            m_targetTextures->wfRGBParade.clImageGL,
            m_targetTextures->wfRGBBlacks.clImageGL,
            m_targetTextures->wfYUVParade.clImageGL,
            m_targetTextures->scUV.clImageGL,
            m_targetTextures->scXYZ.clImageGL,
            m_targetTextures->scDia.clImageGL};
    }

    void ResizeBuffers();

public:
    [[nodiscard]]
    auto GetTargetTextures() const noexcept -> const TargetTextures* const {
        return m_targetTextures.get();
    }

    void ExecutePipeline(const uint8_t* sourceData, Dims2D sourceSize, SourceFormat sourceFormat, uint32_t lineStrideBytes, RenderSettings renderSettings);
};

} // namespace scpp

template<>
struct std::formatter<scpp::SourceFormat, char> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }
    template<typename FormatContext>
    auto format(const scpp::SourceFormat& format, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "{}", scpp::SourceFormatToString(format));
    }
};

template<>
struct std::formatter<scpp::SourceColorSpace, char> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }
    template<typename FormatContext>
    auto format(const scpp::SourceColorSpace& colorSpace, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "{}", scpp::SourceColorSpaceToString(colorSpace));
    }
};

template<>
struct std::formatter<scpp::SourceYUVRange, char> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }
    template<typename FormatContext>
    auto format(const scpp::SourceYUVRange& yuvRange, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "{}", scpp::SourceYUVRangeToString(yuvRange));
    }
};
