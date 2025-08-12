#pragma once

#include "pch.hpp"
#include "utils.hpp"

namespace scpp {
struct RenderPipelineKernels {

    bool initialized{false};

    cl::Kernel convertSource_RGBA_8888;
    cl::Kernel convertSource_RGBX_8888;
    cl::Kernel convertSource_BGRA_8888;
    cl::Kernel convertSource_BGRX_8888;
    cl::Kernel convertSource_ARGB_8888;
    cl::Kernel convertSource_RGB_888;
    cl::Kernel convertSource_BGR_888_InvY;

    cl::Kernel convertSource_UYVY_422;
    cl::Kernel convertSource_UYVA_4224;
    cl::Kernel convertSource_YUYV_422;
    cl::Kernel convertSource_NV12;
    cl::Kernel convertSource_P216;

    cl::Kernel accumulateWaveforms;
    cl::Kernel accumulateUVScope;
    cl::Kernel accumulateXYZScope;
    cl::Kernel accumulateDiaScope;

    cl::Kernel createWaveformImages;
    cl::Kernel createScopeImages;
};

class OpenCLDeviceProvider {
private:
    cl::Device            m_device;
    cl::Context           m_context;
    RenderPipelineKernels m_kernels;

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

    [[nodiscard]]
    auto LoadProgramFromFile(const std::filesystem::path& path) const -> std::expected<cl::Program, ErrorCode>;

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

private:
    static constexpr float c_FactComputeUnits   = 40.0f;
    static constexpr float c_FactClockFrequency = 2000.0f;
    static constexpr float c_FactGlobalMemory   = 8.0f * 0x1p30;
    static constexpr float c_FactMaxMemAlloc    = 8.0f * 0x1p30 * 2.f;
    static constexpr float c_FactLocalMemory    = 48.0f * 1024;
    static constexpr float c_FactMaxWorkGroup   = 256.0f;
    static constexpr float c_FactMaxIm2DWidth   = 8192.0f;
    static constexpr float c_FactMaxSamplers    = 16.0f;

    static constexpr auto c_KernelConvertSourcePath = "./kernels/01_convert.cl"sv;

    static constexpr auto c_KernelName_convertSource_RGBA_8888    = "convertSource_RGBA_8888"sv;
    static constexpr auto c_KernelName_convertSource_RGBX_8888    = "convertSource_RGBX_8888"sv;
    static constexpr auto c_KernelName_convertSource_BGRA_8888    = "convertSource_BGRA_8888"sv;
    static constexpr auto c_KernelName_convertSource_BGRX_8888    = "convertSource_BGRX_8888"sv;
    static constexpr auto c_KernelName_convertSource_ARGB_8888    = "convertSource_ARGB_8888"sv;
    static constexpr auto c_KernelName_convertSource_RGB_888      = "convertSource_RGB_888"sv;
    static constexpr auto c_KernelName_convertSource_BGR_888_InvY = "convertSource_BGR_888_InvY"sv;
    static constexpr auto c_KernelName_convertSource_UYVY_422     = "convertSource_UYVY_422"sv;
    static constexpr auto c_KernelName_convertSource_UYVA_4224    = "convertSource_UYVA_4224"sv;
    static constexpr auto c_KernelName_convertSource_YUYV_422     = "convertSource_YUYV_422"sv;
    static constexpr auto c_KernelName_convertSource_NV12         = "convertSource_NV12"sv;
    static constexpr auto c_KernelName_convertSource_P216         = "convertSource_P216"sv;

    static constexpr auto c_KernelAccumulateSourcePath = "./kernels/02_accumulate.cl"sv;

    static constexpr auto c_KernelName_accumulateWaveforms = "accumulateWaveforms"sv;
    static constexpr auto c_KernelName_accumulateUVScope   = "accumulateUVScope"sv;
    static constexpr auto c_KernelName_accumulateXYZScope  = "accumulateXYZScope"sv;
    static constexpr auto c_KernelName_accumulateDiaScope  = "accumulateDiaScope"sv;

    static constexpr auto c_KernelCreateImagesSourcePath = "./kernels/03_create_images.cl"sv;

    static constexpr auto c_KernelName_createWaveformImages = "createWaveformImages"sv;
    static constexpr auto c_KernelName_createScopeImages    = "createScopeImages"sv;

    static constexpr auto c_KernelCommonSourcePath = "./kernels/common.cl"sv;
};

} // namespace scpp