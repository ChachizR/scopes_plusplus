#pragma once

#include "pch.hpp"
#include "error_codes.hpp"

namespace scpp {
struct RenderPipelineKernels {
    bool       initialized{false};
    cl::Kernel convertSource_RGBA_8888;
    cl::Kernel convertSource_RGBX_8888;
};

class OpenCLDeviceProvider {
private:
    cl::Device            m_device;
    cl::Context           m_context;
    RenderPipelineKernels m_kernels;

    static constexpr float c_FactComputeUnits   = 40.0f;
    static constexpr float c_FactClockFrequency = 2000.0f;
    static constexpr float c_FactGlobalMemory   = 8.0f * 0x1p30;
    static constexpr float c_FactMaxMemAlloc    = 8.0f * 0x1p30 * 2.f;
    static constexpr float c_FactLocalMemory    = 48.0f * 1024;
    static constexpr float c_FactMaxWorkGroup   = 256.0f;
    static constexpr float c_FactMaxIm2DWidth   = 8192.0f;
    static constexpr float c_FactMaxSamplers    = 16.0f;

    static constexpr auto c_KernelSourcePath                   = "./kernels/renderpipeline.cl"sv;
    static constexpr auto c_KernelName_convertSource_RGBA_8888 = "convertSource_RGBA_8888_to_RGBA_YUV"sv;
    static constexpr auto c_KernelName_convertSource_RGBX_8888 = "convertSource_RGBX_8888_to_RGBA_YUV"sv;

    bool m_initialized{false};

public:
    OpenCLDeviceProvider();

private:
    [[nodiscard]]
    auto GetDeviceScore(const cl::Device& device) const noexcept -> float;

    [[nodiscard]]
    auto SelectDevice() const -> std::optional<cl::Device>;

    [[nodiscard]]
    auto CreateContext() -> std::optional<cl::Context>;

    auto CreateKernels() const -> std::expected<RenderPipelineKernels, ErrorCode>;

public:
    [[nodiscard]]
    constexpr bool IsInitialized() const noexcept {
        return m_initialized;
    }

    [[nodiscard]]
    constexpr const cl::Device* GetDevice() const noexcept {
        return m_initialized ? &m_device : nullptr;
    }

    [[nodiscard]]
    constexpr const cl::Context* GetContext() const noexcept {
        return m_initialized ? &m_context : nullptr;
    }

    [[nodiscard]]
    const RenderPipelineKernels& GetKernels() const noexcept {
        return m_kernels;
    }
};

struct Dims2D {
    uint32_t width;
    uint32_t height;

    constexpr Dims2D(uint32_t w, uint32_t h)
        : width{w}
        , height{h} {}

    constexpr bool operator==(const Dims2D& other) const noexcept {
        return width == other.width && height == other.height;
    }

    constexpr uint64_t Area() const noexcept {
        return static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
    }
};

struct CLGLTextureRGBA {
    std::string_view description{};
    GLuint           glTextureID;
    cl::ImageGL      clImageGL;

    Dims2D size;

    CLGLTextureRGBA(Dims2D size, const cl::Context& context);
    CLGLTextureRGBA(std::string_view description, Dims2D size, const cl::Context& context);

    ~CLGLTextureRGBA() {
        if (glTextureID == 0)
            return;
        glDeleteTextures(1, &glTextureID);
        std::println("Deleted OpenGL texture: {}", glTextureID);
    }

    CLGLTextureRGBA(const CLGLTextureRGBA&)            = delete;
    CLGLTextureRGBA& operator=(const CLGLTextureRGBA&) = delete;
    CLGLTextureRGBA(CLGLTextureRGBA&& rhs) noexcept;
    CLGLTextureRGBA& operator=(CLGLTextureRGBA&& rhs) noexcept;

    // should be called on the main gl thread
    void Resize(Dims2D newSize, const cl::Context& context);
};

static inline void ImGuiImageRender(
    const CLGLTextureRGBA& texture) {
    ImVec2 avail = ImGui::GetContentRegionAvail();

    float imgW   = static_cast<float>(texture.size.width);
    float imgH   = static_cast<float>(texture.size.height);
    float scaleX = avail.x / imgW;
    float scaleY = avail.y / imgH;
    float scale  = (std::min)((std::min)(scaleX, scaleY), 1.0f);

    const auto imTextureID = static_cast<ImTextureID>(texture.glTextureID);

    ImGui::Image(
        imTextureID,
        ImVec2(imgW * scale, imgH * scale),
        ImVec2(0, 0), ImVec2(1, 1));
}

struct TargetTextures {
    CLGLTextureRGBA sourcePreview;
    CLGLTextureRGBA falseColor;
    CLGLTextureRGBA wfLuma;

    TargetTextures(CLGLTextureRGBA&& sourcePreview,
                   CLGLTextureRGBA&& falseColor,
                   CLGLTextureRGBA&& wfLuma)
        : sourcePreview{std::move(sourcePreview)}
        , falseColor{std::move(falseColor)}
        , wfLuma{std::move(wfLuma)} {}
};

enum class SourceFormat {
    unknown = 0,
    RGBA_8888,
    RGBX_8888,
    BGRA_8888,
    ARGB_8888,
    BGRX_8888,
    BGR_888_InvY,
    RGB_888,
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

    static constexpr Dims2D kDefaultSourceSize{1920, 1080};
    static constexpr Dims2D kWaveformSize{580, 256};
    static constexpr Dims2D kScopeSize{256, 256};

    std::unique_ptr<TargetTextures> m_targetTextures = nullptr;

    RenderPipelineKernels m_kernels;

    uint64_t m_sourceSizeBytes{0};
    uint64_t m_rgbaSizeBytes{0};
    uint64_t m_yuvSizeBytes{0};

    cl::Buffer m_bufSource;
    cl::Buffer m_bufIntermRGBA;
    cl::Buffer m_bufIntermYUV;

    bool m_buffersInitialized{false};

    std::vector<cl::Memory> m_glObjects;

    Dims2D       m_sourceDims{kDefaultSourceSize};
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