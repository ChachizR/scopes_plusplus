#include "cl_renderer.hpp"

#define CHECK_CL_ERROR_RET(res, msg)                                      \
    if (res != CL_SUCCESS) [[unlikely]] {                                 \
        m_lastPipelineStatus = std::format("{} ({})", msg, res);          \
        std::println("OpenCL Error: {} - {}", msg, res);                  \
        return;                                                           \
    }

namespace scpp {

inline constexpr auto OpenCLRenderer::SelectConvertKernel(SourceFormat sourceFormat) -> cl::Kernel& {
    switch (sourceFormat) {
    case SourceFormat::RGBA_8888:
        return m_kernels.convertSource_RGBA_8888;
    case SourceFormat::RGBX_8888:
        return m_kernels.convertSource_RGBX_8888;
    case SourceFormat::BGRA_8888:
        return m_kernels.convertSource_BGRA_8888;
    case SourceFormat::BGRX_8888:
        return m_kernels.convertSource_BGRX_8888;
    case SourceFormat::ARGB_8888:
        return m_kernels.convertSource_ARGB_8888;
    case SourceFormat::RGB_888:
        return m_kernels.convertSource_RGB_888;
    case SourceFormat::BGR_888_InvY:
        return m_kernels.convertSource_BGR_888_InvY;
    case SourceFormat::UYVY_422:
        return m_kernels.convertSource_UYVY_422;
    case SourceFormat::UYVA_4224:
        return m_kernels.convertSource_UYVA_4224;
    case SourceFormat::YUYV_422:
        return m_kernels.convertSource_YUYV_422;
    case SourceFormat::NV12:
        return m_kernels.convertSource_NV12;
    case SourceFormat::P216:
        return m_kernels.convertSource_P216;
    }
    return m_kernels.convertSource_RGBA_8888; // Default to RGBA_8888 if no match found
}

OpenCLRenderer::OpenCLRenderer(const OpenCLDeviceProvider& deviceProviderRef)
    : m_deviceProviderRef{deviceProviderRef} {

    if (!m_deviceProviderRef.IsInitialized()) {
        std::println("OpenCL device provider is not initialized.");
        return;
    }

    m_device  = *m_deviceProviderRef.GetDevice();
    m_context = *m_deviceProviderRef.GetContext();

    cl_int res = CL_SUCCESS;

    m_commandQueue = cl::CommandQueue(m_context, m_device, cl::QueueProperties::Profiling, &res);

    if (res != CL_SUCCESS) {
        std::println("Failed to create OpenCL command queue: {}", res);
        return;
    }

    m_targetTextures = std::make_unique<TargetTextures>(
        CLGLTextureRGBA("Source Preview"sv, c_DefaultSourceSize, m_context),
        CLGLTextureRGBA("False Color"sv, c_DefaultSourceSize, m_context),
        CLGLTextureRGBA("Luminance Waveform"sv, c_WaveformSize, m_context),
        CLGLTextureRGBA("RGB Waveform"sv, c_WaveformSize, m_context),
        CLGLTextureRGBA("RGB Parade"sv, c_WaveformSize, m_context),
        CLGLTextureRGBA("RGB Blacklevel"sv, c_WaveformSize, m_context),
        CLGLTextureRGBA("YUV Parade"sv, c_WaveformSize, m_context),
        CLGLTextureRGBA("UV Vectorscope"sv, c_ScopeSize, m_context),
        CLGLTextureRGBA("CIE 1931 Chromaticity"sv, c_ScopeSize, m_context),
        CLGLTextureRGBA("Double Diamond"sv, c_ScopeSize, m_context));

    UpdateGLObjects();

    m_sourceFormat = SourceFormat::unknown;
    m_kernels      = m_deviceProviderRef.GetKernels();

    if (!m_kernels.initialized) {
        std::println("OpenCL kernels are not initialized.");
        return;
    }
}

void OpenCLRenderer::ResizeGLTextures() {
    if (!needsResizeFlag_mainThread) {
        std::println("The textures don't need resizing!");
        return;
    }

    if (!m_targetTextures) {
        std::println("Target textures are not initialized.");
        return;
    }

    m_targetTextures->sourcePreview.Resize(m_sourceDims, m_context);
    m_targetTextures->falseColor.Resize(m_sourceDims, m_context);

    UpdateGLObjects();

    needsResizeFlag_mainThread = false;
}

void OpenCLRenderer::ResizeBuffers() {

    m_sourceSizeBytes = GetSourceSize(m_sourceFormat, m_sourceDims);
    m_rgbaSizeBytes   = m_sourceDims.Area() * 4 * sizeof(cl_float);
    m_yuvSizeBytes    = m_sourceDims.Area() * 3 * sizeof(cl_float);

    m_bufSource     = cl::Buffer(m_context, CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY, m_sourceSizeBytes);
    m_bufIntermRGBA = cl::Buffer(m_context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, m_rgbaSizeBytes);
    m_bufIntermYUV  = cl::Buffer(m_context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, m_yuvSizeBytes);
    m_bufIntermXYZ  = cl::Buffer(m_context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, m_yuvSizeBytes);

    std::println("Resized OpenCL buffers: {}x{} "
                 "Source: {} bytes, "
                 "Interm RGBA: {} bytes, "
                 "Interm YUV: {} bytes",
                 m_sourceDims.width, m_sourceDims.height,
                 m_sourceSizeBytes, m_rgbaSizeBytes, m_yuvSizeBytes);

    if (!m_buffersInitialized) [[unlikely]] {
        cl::size_type wfAccSize = c_WaveformSize.Area() * sizeof(cl_float) * 3;

        m_bufAccRGB = cl::Buffer(m_context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, wfAccSize);
        m_bufAccYUV = cl::Buffer(m_context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, wfAccSize);

        cl::size_type scopeAccSize = c_ScopeSize.Area() * sizeof(cl_float) * 4;

        m_bufAcc2D_UV_RGBA  = cl::Buffer(m_context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, scopeAccSize);
        m_bufAcc2D_XYZ_RGBA = cl::Buffer(m_context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, scopeAccSize);
        m_bufAcc2D_DIA_RGBA = cl::Buffer(m_context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, scopeAccSize);

        m_bufFalseColorMapFull    = cl::Buffer(m_context, CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY, 256 * sizeof(cl_uchar4));
        m_bufFalseColorMapLimited = cl::Buffer(m_context, CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY, 256 * sizeof(cl_uchar4));
    }

    m_buffersInitialized = true;
}

void OpenCLRenderer::ExecutePipeline(
    const uint8_t* sourceData,
    Dims2D         sourceDims,
    SourceFormat   sourceFormat,
    uint32_t       lineStrideBytes,
    RenderSettings renderSettings) {

    if (!sourceData) [[unlikely]] {
        m_lastPipelineStatus = "No source frame data";
        return;
    }

    bool formatChanged     = (sourceFormat != m_sourceFormat);
    bool sourceDimsChanged = (sourceDims != m_sourceDims);

    if (formatChanged || sourceDimsChanged) [[unlikely]] {
        // If the source format has changed, we need to resize the textures and buffers.
        m_sourceFormat = sourceFormat;
        m_sourceDims   = sourceDims;
        ResizeBuffers();
    } else if (!m_buffersInitialized) [[unlikely]] {
        // If the buffers are not initialized, we need to initialize them.
        ResizeBuffers();
    }

    // Resizing the Gl textures can only be done on the main gl thread.
    // thus we set a flag that the textures need to be resized.

    if (sourceDimsChanged && !needsResizeFlag_mainThread) [[unlikely]] {
        needsResizeFlag_mainThread = true;
        m_lastPipelineStatus       = std::format("Resizing textures to {}", sourceDims);
        // TODO: don't discard this frame, but wait for the main gl thread to resize the textures
        return;
    }

    // wait for the main gl thread to resize the textures
    if (needsResizeFlag_mainThread) [[unlikely]] {
        m_lastPipelineStatus = std::format("Waiting for texture resize to {}", sourceDims);
        return;
    }

    if (m_sourceSizeBytes == 0) [[unlikely]] {
        m_lastPipelineStatus = std::format("Unsupported source format {}", m_sourceFormat);
        return;
    }

    /// CONVERT KERNEL

    cl::Kernel& convert_kernel = SelectConvertKernel(m_sourceFormat);

    if (!convert_kernel()) [[unlikely]] {
        m_lastPipelineStatus = std::format("Missing convert kernel for {}", m_sourceFormat);
        std::println("Failed to select OpenCL kernel for source format: {}", m_sourceFormat);
        return;
    }

    cl_int res = CL_SUCCESS;
    const auto features = renderSettings.enabledFeatures;
    const auto analysisStep = (std::max)(1u, renderSettings.analysisResolutionDivisor);
    const Dims2D analysisDims{
        static_cast<uint32_t>((m_sourceDims.width + analysisStep - 1u) / analysisStep),
        static_cast<uint32_t>((m_sourceDims.height + analysisStep - 1u) / analysisStep)};
    m_lastTimingStats.Reset();

    enum class TimingBucket {
        Upload,
        Reset,
        AcquireGL,
        Convert,
        WaveformAccum,
        WaveformImage,
        ScopeUV,
        ScopeXYZ,
        ScopeDia,
        ScopeImage,
        FalseColor,
        ReleaseGL
    };

    struct TimedEvent {
        TimingBucket bucket{};
        cl::Event    event{};
    };

    std::vector<TimedEvent> timedEvents;
    timedEvents.reserve(24u);

    auto appendTimedEvent = [&](TimingBucket bucket) -> cl::Event* {
        auto& entry = timedEvents.emplace_back();
        entry.bucket = bucket;
        return &entry.event;
    };

    auto addEventDuration = [this](TimingBucket bucket, float durationMS) {
        switch (bucket) {
        case TimingBucket::Upload:
            m_lastTimingStats.uploadMS += durationMS;
            break;
        case TimingBucket::Reset:
            m_lastTimingStats.resetMS += durationMS;
            break;
        case TimingBucket::AcquireGL:
            m_lastTimingStats.acquireGLMS += durationMS;
            break;
        case TimingBucket::Convert:
            m_lastTimingStats.convertMS += durationMS;
            break;
        case TimingBucket::WaveformAccum:
            m_lastTimingStats.waveformAccumMS += durationMS;
            break;
        case TimingBucket::WaveformImage:
            m_lastTimingStats.waveformImageMS += durationMS;
            break;
        case TimingBucket::ScopeUV:
            m_lastTimingStats.scopeUVMS += durationMS;
            break;
        case TimingBucket::ScopeXYZ:
            m_lastTimingStats.scopeXYZMS += durationMS;
            break;
        case TimingBucket::ScopeDia:
            m_lastTimingStats.scopeDiaMS += durationMS;
            break;
        case TimingBucket::ScopeImage:
            m_lastTimingStats.scopeImageMS += durationMS;
            break;
        case TimingBucket::FalseColor:
            m_lastTimingStats.falseColorMS += durationMS;
            break;
        case TimingBucket::ReleaseGL:
            m_lastTimingStats.releaseGLMS += durationMS;
            break;
        }
    };

    auto requireGLImage = [this](const CLGLTextureRGBA& texture) -> bool {
        if (texture.IsValid()) {
            return true;
        }

        m_lastPipelineStatus = std::format("OpenCL GL image '{}' is invalid", texture.description);
        std::println("OpenCL Error: OpenCL GL image '{}' is invalid", texture.description);
        return false;
    };

    auto setKernelArg = [this](cl::Kernel& kernel, cl_uint index, const auto& value, std::string_view label) -> bool {
        const cl_int argRes = kernel.setArg(index, value);
        if (argRes == CL_SUCCESS) {
            return true;
        }

        m_lastPipelineStatus = std::format("Failed to set OpenCL {} argument {} ({})", label, index, argRes);
        std::println("OpenCL Error: Failed to set OpenCL {} argument {} - {}", label, index, argRes);
        return false;
    };

    if (!requireGLImage(m_targetTextures->sourcePreview)) {
        return;
    }
    if ((features & RenderFeature::FalseColor) && !requireGLImage(m_targetTextures->falseColor)) {
        return;
    }
    if (features & RenderFeature::AnyWF) {
        if (!requireGLImage(m_targetTextures->wfLuma)
            || !requireGLImage(m_targetTextures->wfRGB)
            || !requireGLImage(m_targetTextures->wfRGBParade)
            || !requireGLImage(m_targetTextures->wfRGBBlacks)
            || !requireGLImage(m_targetTextures->wfYUVParade)) {
            return;
        }
    }
    if (features & RenderFeature::AnySC) {
        if (!requireGLImage(m_targetTextures->scUV)
            || !requireGLImage(m_targetTextures->scXYZ)
            || !requireGLImage(m_targetTextures->scDia)) {
            return;
        }
    }

    if (!setKernelArg(convert_kernel, 0, m_bufSource, "convert input buffer")
        || !setKernelArg(convert_kernel, 1, m_targetTextures->sourcePreview.clImageGL, "convert source preview image")
        || !setKernelArg(convert_kernel, 2, m_bufIntermRGBA, "convert RGBA buffer")
        || !setKernelArg(convert_kernel, 3, m_bufIntermYUV, "convert YUV buffer")
        || !setKernelArg(convert_kernel, 4, m_bufIntermXYZ, "convert XYZ buffer")
        || !setKernelArg(convert_kernel, 5, m_sourceDims.width, "convert width")
        || !setKernelArg(convert_kernel, 6, m_sourceDims.height, "convert height")
        || !setKernelArg(convert_kernel, 7, lineStrideBytes, "convert stride")
        || !setKernelArg(convert_kernel, 8, static_cast<cl_int>(renderSettings.colorSpace), "convert source colorspace")
        || !setKernelArg(convert_kernel, 9, static_cast<cl_int>(renderSettings.yuvRange), "convert YUV range")
        || !setKernelArg(convert_kernel, 10, renderSettings.enabledFeatures, "convert enabled features")
        || !setKernelArg(convert_kernel, 11, analysisStep, "convert analysis step")) {
        return;
    }

    cl::NDRange ndrGlobalConvert(m_sourceDims.width * m_sourceDims.height);

    /// ACCUMULATE KERNEL
    if (features & RenderFeature::AnyWF) {
        res += m_kernels.accumulateWaveforms.setArg<cl::Buffer>(0, m_bufIntermRGBA);
        res += m_kernels.accumulateWaveforms.setArg<cl::Buffer>(1, m_bufIntermYUV);
        res += m_kernels.accumulateWaveforms.setArg<cl::Buffer>(2, m_bufAccRGB);
        res += m_kernels.accumulateWaveforms.setArg<cl::Buffer>(3, m_bufAccYUV);
        res += m_kernels.accumulateWaveforms.setArg<cl_uint>(4, m_sourceDims.width);
        res += m_kernels.accumulateWaveforms.setArg<cl_uint>(5, m_sourceDims.height);
        res += m_kernels.accumulateWaveforms.setArg<cl_uchar>(6, 0);
        res += m_kernels.accumulateWaveforms.setArg<CLRect2D>(7, CLRect2D{0, 0, 0, 0});
        res += m_kernels.accumulateWaveforms.setArg<RenderFeatureFlags>(8, renderSettings.enabledFeatures);
        res += m_kernels.accumulateWaveforms.setArg<cl_uint>(9, analysisStep);

        CHECK_CL_ERROR_RET(res, "Failed to set OpenCL accumulate kernel arguments");
    }

    static constexpr size_t localHistKernelY = c_WaveformSize.height;

    size_t globalHeight = ((analysisDims.height + localHistKernelY - 1) / localHistKernelY) * localHistKernelY;

    cl::NDRange ndrGlobalAccWF((std::max)(analysisDims.width, c_WaveformSize.width), globalHeight);
    cl::NDRange ndrLocalAccWF(1, localHistKernelY);

    /// CREATE IMAGE KERNEL

    float sampleCount = std::ceil(static_cast<float>(analysisDims.width) / static_cast<float>(c_WaveformSize.width));
    float brightness  = (1.f + 8.f * (1080.f / (float)analysisDims.height)) / sampleCount;

    if (features & RenderFeature::AnyWF) {
        res += m_kernels.createWaveformImages.setArg<cl::Buffer>(0, m_bufAccRGB);
        res += m_kernels.createWaveformImages.setArg<cl::Buffer>(1, m_bufAccYUV);
        res += m_kernels.createWaveformImages.setArg<cl::ImageGL>(2, m_targetTextures->wfLuma.clImageGL);
        res += m_kernels.createWaveformImages.setArg<cl::ImageGL>(3, m_targetTextures->wfRGB.clImageGL);
        res += m_kernels.createWaveformImages.setArg<cl::ImageGL>(4, m_targetTextures->wfRGBParade.clImageGL);
        res += m_kernels.createWaveformImages.setArg<cl::ImageGL>(5, m_targetTextures->wfRGBBlacks.clImageGL);
        res += m_kernels.createWaveformImages.setArg<cl::ImageGL>(6, m_targetTextures->wfYUVParade.clImageGL);
        res += m_kernels.createWaveformImages.setArg<cl_uint>(7, sourceDims.width);
        res += m_kernels.createWaveformImages.setArg<cl_int>(8, static_cast<cl_int>(renderSettings.colorSpace));
        res += m_kernels.createWaveformImages.setArg<cl_float>(9, brightness);
        res += m_kernels.createWaveformImages.setArg<RenderFeatureFlags>(10, renderSettings.enabledFeatures);

        CHECK_CL_ERROR_RET(res, "Failed to set OpenCL create waveform images kernel arguments");
    }

    cl::NDRange ndrGlobalCreateWF(c_WaveformSize.width, c_WaveformSize.height);
    cl::NDRange ndrLocalCreateWF(1, c_WaveformSize.height);

    /// ACCUMULATE SCOPE KERNELS
    if (features & RenderFeature::SCUV) {
        res += m_kernels.accumulateUVScope.setArg<cl::Buffer>(0, m_bufIntermRGBA);
        res += m_kernels.accumulateUVScope.setArg<cl::Buffer>(1, m_bufIntermYUV);
        res += m_kernels.accumulateUVScope.setArg<cl::Buffer>(2, m_bufAcc2D_UV_RGBA);
        res += m_kernels.accumulateUVScope.setArg<cl_uint>(3, m_sourceDims.width);
        res += m_kernels.accumulateUVScope.setArg<cl_uint>(4, m_sourceDims.height);
        res += m_kernels.accumulateUVScope.setArg<cl_uchar>(5, 0);
        res += m_kernels.accumulateUVScope.setArg<CLRect2D>(6, CLRect2D{0, 0, 0, 0});
        res += m_kernels.accumulateUVScope.setArg<cl_uint>(7, analysisStep);

        CHECK_CL_ERROR_RET(res, "Failed to set OpenCL accumulate UV scope kernel arguments");
    }
    if (features & RenderFeature::SCXYZ) {
        res += m_kernels.accumulateXYZScope.setArg<cl::Buffer>(0, m_bufIntermXYZ);
        res += m_kernels.accumulateXYZScope.setArg<cl::Buffer>(1, m_bufAcc2D_XYZ_RGBA);
        res += m_kernels.accumulateXYZScope.setArg<cl_uint>(2, m_sourceDims.width);
        res += m_kernels.accumulateXYZScope.setArg<cl_uint>(3, m_sourceDims.height);
        res += m_kernels.accumulateXYZScope.setArg<cl_uchar>(4, 0);
        res += m_kernels.accumulateXYZScope.setArg<CLRect2D>(5, CLRect2D{0, 0, 0, 0});
        res += m_kernels.accumulateXYZScope.setArg<cl_int>(6, static_cast<cl_int>(renderSettings.colorSpace));
        res += m_kernels.accumulateXYZScope.setArg<cl_uint>(7, analysisStep);

        CHECK_CL_ERROR_RET(res, "Failed to set OpenCL accumulate XYZ scope kernel arguments");
    }
    if (features & RenderFeature::SCDia) {
        res += m_kernels.accumulateDiaScope.setArg<cl::Buffer>(0, m_bufIntermRGBA);
        res += m_kernels.accumulateDiaScope.setArg<cl::Buffer>(1, m_bufAcc2D_DIA_RGBA);
        res += m_kernels.accumulateDiaScope.setArg<cl_uint>(2, m_sourceDims.width);
        res += m_kernels.accumulateDiaScope.setArg<cl_uint>(3, m_sourceDims.height);
        res += m_kernels.accumulateDiaScope.setArg<cl_uchar>(4, 0);
        res += m_kernels.accumulateDiaScope.setArg<CLRect2D>(5, CLRect2D{0, 0, 0, 0});
        res += m_kernels.accumulateDiaScope.setArg<cl_uint>(6, analysisStep);

        CHECK_CL_ERROR_RET(res, "Failed to set OpenCL accumulate DIA scope kernel arguments");
    }

    cl::NDRange ndrGlobalAccScope(analysisDims.width, analysisDims.height);

    /// CREATE SCOPE IMAGES
    if (features & RenderFeature::AnySC) {
        res += m_kernels.createScopeImages.setArg<cl::Buffer>(0, m_bufAcc2D_UV_RGBA);
        res += m_kernels.createScopeImages.setArg<cl::Buffer>(1, m_bufAcc2D_XYZ_RGBA);
        res += m_kernels.createScopeImages.setArg<cl::Buffer>(2, m_bufAcc2D_DIA_RGBA);
        res += m_kernels.createScopeImages.setArg<cl::ImageGL>(3, m_targetTextures->scUV.clImageGL);
        res += m_kernels.createScopeImages.setArg<cl::ImageGL>(4, m_targetTextures->scXYZ.clImageGL);
        res += m_kernels.createScopeImages.setArg<cl::ImageGL>(5, m_targetTextures->scDia.clImageGL);
        res += m_kernels.createScopeImages.setArg<cl_uint>(6, m_sourceDims.width);
        res += m_kernels.createScopeImages.setArg<cl_float>(7, brightness);
        res += m_kernels.createScopeImages.setArg<RenderFeatureFlags>(8, renderSettings.enabledFeatures);

        CHECK_CL_ERROR_RET(res, "Failed to set OpenCL create scope images kernel arguments");
    }

    cl::NDRange ndrGlobalCreateScope(c_ScopeSize.width, c_ScopeSize.height);

    /// POST FX
    if (features & RenderFeature::FalseColor) {
        res += m_kernels.createFalseColorImage.setArg<cl::Buffer>(0, m_bufIntermYUV);
        res += m_kernels.createFalseColorImage.setArg<cl_uint>(1, m_sourceDims.width);
        res += m_kernels.createFalseColorImage.setArg<cl_uint>(2, m_sourceDims.height);
        res += m_kernels.createFalseColorImage.setArg<cl::Buffer>(3, (renderSettings.yuvRange == SourceYUVRange::Full) ? m_bufFalseColorMapFull : m_bufFalseColorMapLimited);
        res += m_kernels.createFalseColorImage.setArg<cl::ImageGL>(4, m_targetTextures->falseColor.clImageGL);

        CHECK_CL_ERROR_RET(res, "Failed to set OpenCL false color image kernel arguments");
    }

    cl::NDRange ndrGlobalFalseColor(m_sourceDims.width * m_sourceDims.height);

    /// UPLOAD SOURCE DATA

    {
        res = m_commandQueue.enqueueWriteBuffer(
            m_bufSource, CL_FALSE, 0, m_sourceSizeBytes, sourceData, nullptr, appendTimedEvent(TimingBucket::Upload));

        CHECK_CL_ERROR_RET(res, "Failed to write source data to OpenCL buffer");

        if (m_falseColorMapChanged) [[unlikely]] {
            res = m_commandQueue.enqueueWriteBuffer(
                m_bufFalseColorMapFull, CL_FALSE, 0, 256 * sizeof(cl_uchar4), m_falseColorMap.GetDataFullRange().data(), nullptr, appendTimedEvent(TimingBucket::Upload));

            CHECK_CL_ERROR_RET(res, "Failed to write false color map (full range) to OpenCL buffer");

            res = m_commandQueue.enqueueWriteBuffer(
                m_bufFalseColorMapLimited, CL_FALSE, 0, 256 * sizeof(cl_uchar4), m_falseColorMap.GetDataLimitedRange().data(), nullptr, appendTimedEvent(TimingBucket::Upload));

            CHECK_CL_ERROR_RET(res, "Failed to write false color map (limited range) to OpenCL buffer");

            m_falseColorMapChanged = false;
        }
    }

    /// RESET ACC BUFFERS

    {
        if (features & RenderFeature::AnyWFRgb) {
            res = m_commandQueue.enqueueFillBuffer(m_bufAccRGB, 0, 0, c_WaveformSize.Area() * sizeof(cl_uint) * 3, nullptr, appendTimedEvent(TimingBucket::Reset));
            CHECK_CL_ERROR_RET(res, "Failed to fill OpenCL buffer for RGB accumulation");
        }
        if (features & RenderFeature::AnyWFYUV) {
            res = m_commandQueue.enqueueFillBuffer(m_bufAccYUV, 0, 0, c_WaveformSize.Area() * sizeof(cl_uint) * 3, nullptr, appendTimedEvent(TimingBucket::Reset));
            CHECK_CL_ERROR_RET(res, "Failed to fill OpenCL buffer for YUV accumulation");
        }
        if (features & RenderFeature::SCUV) {
            res = m_commandQueue.enqueueFillBuffer(m_bufAcc2D_UV_RGBA, 0, 0, c_ScopeSize.Area() * sizeof(cl_uint) * 4, nullptr, appendTimedEvent(TimingBucket::Reset));
            CHECK_CL_ERROR_RET(res, "Failed to fill OpenCL buffer for UV scope accumulation");
        }
        if (features & RenderFeature::SCXYZ) {
            res = m_commandQueue.enqueueFillBuffer(m_bufAcc2D_XYZ_RGBA, 0, 0, c_ScopeSize.Area() * sizeof(cl_uint) * 4, nullptr, appendTimedEvent(TimingBucket::Reset));
            CHECK_CL_ERROR_RET(res, "Failed to fill OpenCL buffer for XYZ scope accumulation");
        }
        if (features & RenderFeature::SCDia) {
            res = m_commandQueue.enqueueFillBuffer(m_bufAcc2D_DIA_RGBA, 0, 0, c_ScopeSize.Area() * sizeof(cl_uint) * 4, nullptr, appendTimedEvent(TimingBucket::Reset));
            CHECK_CL_ERROR_RET(res, "Failed to fill OpenCL buffer for DIA scope accumulation");
        }
    }

    /// EXECUTE KERNELS

    res = m_commandQueue.enqueueAcquireGLObjects(&m_glObjects, nullptr, appendTimedEvent(TimingBucket::AcquireGL));

    CHECK_CL_ERROR_RET(res, "Failed to acquire OpenCL GL objects");

    res = m_commandQueue.enqueueNDRangeKernel(
        convert_kernel,
        cl::NullRange,
        ndrGlobalConvert,
        cl::NullRange,
        nullptr,
        appendTimedEvent(TimingBucket::Convert));

    CHECK_CL_ERROR_RET(res, "Failed to enqueue OpenCL convert kernel");

    if (features & RenderFeature::AnyWF) {
        res = m_commandQueue.enqueueNDRangeKernel(
            m_kernels.accumulateWaveforms,
            cl::NullRange,
            ndrGlobalAccWF,
            ndrLocalAccWF,
            nullptr,
            appendTimedEvent(TimingBucket::WaveformAccum));

        CHECK_CL_ERROR_RET(res, "Failed to enqueue OpenCL accumulate waveforms kernel");
        res = m_commandQueue.enqueueNDRangeKernel(
            m_kernels.createWaveformImages,
            cl::NullRange,
            ndrGlobalCreateWF,
            ndrLocalCreateWF,
            nullptr,
            appendTimedEvent(TimingBucket::WaveformImage));

        CHECK_CL_ERROR_RET(res, "Failed to enqueue OpenCL create waveform images kernel");
    }

    if (features & RenderFeature::SCUV) {
        res = m_commandQueue.enqueueNDRangeKernel(
            m_kernels.accumulateUVScope,
            cl::NullRange,
            ndrGlobalAccScope,
            cl::NullRange,
            nullptr,
            appendTimedEvent(TimingBucket::ScopeUV));

        CHECK_CL_ERROR_RET(res, "Failed to enqueue OpenCL accumulate UV scope kernel");
    }

    if (features & RenderFeature::SCXYZ) {
        res = m_commandQueue.enqueueNDRangeKernel(
            m_kernels.accumulateXYZScope,
            cl::NullRange,
            ndrGlobalAccScope,
            cl::NullRange,
            nullptr,
            appendTimedEvent(TimingBucket::ScopeXYZ));

        CHECK_CL_ERROR_RET(res, "Failed to enqueue OpenCL accumulate XYZ scope kernel");
    }

    if (features & RenderFeature::SCDia) {
        res = m_commandQueue.enqueueNDRangeKernel(
            m_kernels.accumulateDiaScope,
            cl::NullRange,
            ndrGlobalAccScope,
            cl::NullRange,
            nullptr,
            appendTimedEvent(TimingBucket::ScopeDia));

        CHECK_CL_ERROR_RET(res, "Failed to enqueue OpenCL accumulate DIA scope kernel");
    }

    if (features & RenderFeature::AnySC) {
        res = m_commandQueue.enqueueNDRangeKernel(
            m_kernels.createScopeImages,
            cl::NullRange,
            ndrGlobalCreateScope,
            cl::NullRange,
            nullptr,
            appendTimedEvent(TimingBucket::ScopeImage));

        CHECK_CL_ERROR_RET(res, "Failed to enqueue OpenCL create scope images kernel");
    }

    if (features & RenderFeature::FalseColor) {
        res = m_commandQueue.enqueueNDRangeKernel(
            m_kernels.createFalseColorImage,
            cl::NullRange,
            ndrGlobalFalseColor,
            cl::NullRange,
            nullptr,
            appendTimedEvent(TimingBucket::FalseColor));

        CHECK_CL_ERROR_RET(res, "Failed to enqueue OpenCL false color image kernel");
    }

    res = m_commandQueue.enqueueReleaseGLObjects(&m_glObjects, nullptr, appendTimedEvent(TimingBucket::ReleaseGL));

    CHECK_CL_ERROR_RET(res, "Failed to release OpenCL GL objects");

    const auto finishStart = Clock::now();
    res = m_commandQueue.finish();
    const auto finishEnd = Clock::now();

    CHECK_CL_ERROR_RET(res, "Failed to finish OpenCL command queue");

    m_lastTimingStats.finishWaitMS = static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(finishEnd - finishStart).count()) / 1e3f;

    for (const auto& timedEvent : timedEvents) {
        cl_int startRes = CL_SUCCESS;
        cl_int endRes   = CL_SUCCESS;
        const auto startNS = timedEvent.event.getProfilingInfo<CL_PROFILING_COMMAND_START>(&startRes);
        const auto endNS   = timedEvent.event.getProfilingInfo<CL_PROFILING_COMMAND_END>(&endRes);
        if (startRes != CL_SUCCESS || endRes != CL_SUCCESS || endNS < startNS) {
            continue;
        }

        const float durationMS = static_cast<float>(endNS - startNS) / 1e6f;
        m_lastTimingStats.totalGPUCommandMS += durationMS;
        addEventDuration(timedEvent.bucket, durationMS);
    }

    ++m_submittedFrameCount;
    m_lastPipelineStatus = std::format("Rendered frame {} at {}", m_submittedFrameCount, m_sourceDims);
}
} // namespace scpp
