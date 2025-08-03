#include "cl_renderer.hpp"

#define CHECK_CL_ERROR_RET(res, msg)                     \
    if (res != CL_SUCCESS) [[unlikely]] {                \
        std::println("OpenCL Error: {} - {}", msg, res); \
        return;                                          \
    }

namespace scpp {

inline constexpr auto OpenCLRenderer::SelectConvertKernel(SourceFormat sourceFormat) -> cl::Kernel& {
    switch (sourceFormat) {
    case SourceFormat::RGBA_8888:
        return m_kernels.convertSource_RGBA_8888;
    case SourceFormat::RGBX_8888:
        return m_kernels.convertSource_RGBX_8888;
    case SourceFormat::UYVY_422:
        return m_kernels.convertSource_UYVY_422;
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

    m_commandQueue = cl::CommandQueue(m_context, m_device, cl::QueueProperties(), &res);

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
        CLGLTextureRGBA("YUV Parade"sv, c_WaveformSize, m_context));

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
    m_rgbaSizeBytes   = m_sourceDims.Area() * 4;
    m_yuvSizeBytes    = m_sourceDims.Area() * 3;

    m_bufSource     = cl::Buffer(m_context, CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY, m_sourceSizeBytes);
    m_bufIntermRGBA = cl::Buffer(m_context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, m_rgbaSizeBytes);
    m_bufIntermYUV  = cl::Buffer(m_context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, m_yuvSizeBytes);

    std::println("Resized OpenCL buffers: {}x{} "
                 "Source: {} bytes, "
                 "Interm RGBA: {} bytes, "
                 "Interm YUV: {} bytes",
                 m_sourceDims.width, m_sourceDims.height,
                 m_sourceSizeBytes, m_rgbaSizeBytes, m_yuvSizeBytes);

    if (!m_buffersInitialized) [[unlikely]] {
        cl::size_type wfAccSize = c_WaveformSize.Area() * sizeof(cl_uint) * 3;

        m_bufAccRGB = cl::Buffer(m_context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, wfAccSize);
        m_bufAccYUV = cl::Buffer(m_context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, wfAccSize);

        cl::size_type scopeAccSize = c_ScopeSize.Area() * sizeof(cl_uint) * 4;

        m_bufAcc2D_UV_RGBA  = cl::Buffer(m_context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, scopeAccSize);
        m_bufAcc2D_XYZ_RGBA = cl::Buffer(m_context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, scopeAccSize);
        m_bufAcc2D_DIA_RGBA = cl::Buffer(m_context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, scopeAccSize);
    }

    m_buffersInitialized = true;
}

void OpenCLRenderer::ExecutePipeline(const uint8_t* sourceData, Dims2D sourceDims, SourceFormat sourceFormat) {

    if (!sourceData) [[unlikely]]
        return;

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
        // TODO: don't discard this frame, but wait for the main gl thread to resize the textures
        return;
    }

    // wait for the main gl thread to resize the textures
    if (needsResizeFlag_mainThread) [[unlikely]]
        return;

    if (m_sourceSizeBytes == 0) [[unlikely]]
        return;

    /// CONVERT KERNEL

    cl::Kernel& convert_kernel = SelectConvertKernel(m_sourceFormat);

    if (!convert_kernel()) [[unlikely]] {
        std::println("Failed to select OpenCL kernel for source format: {}", static_cast<int>(m_sourceFormat));
        return;
    }

    cl_int res = CL_SUCCESS;

    res += convert_kernel.setArg<cl::Buffer>(0, m_bufSource);
    res += convert_kernel.setArg<cl::ImageGL>(1, m_targetTextures->sourcePreview.clImageGL);
    res += convert_kernel.setArg<cl::Buffer>(2, m_bufIntermRGBA);
    res += convert_kernel.setArg<cl::Buffer>(3, m_bufIntermYUV);
    res += convert_kernel.setArg<cl_uint>(4, m_sourceDims.width);
    res += convert_kernel.setArg<cl_uint>(5, m_sourceDims.height);
    res += convert_kernel.setArg<cl_int>(6, static_cast<int>(YUVColorSpace::BT709));

    CHECK_CL_ERROR_RET(res, "Failed to set OpenCL convert kernel arguments");

    cl::NDRange ndrGlobalConvert(m_sourceDims.width * m_sourceDims.height);

    /// ACCUMULATE KERNEL

    res += m_kernels.accumulateWaveforms.setArg<cl::Buffer>(0, m_bufIntermRGBA);
    res += m_kernels.accumulateWaveforms.setArg<cl::Buffer>(1, m_bufIntermYUV);
    res += m_kernels.accumulateWaveforms.setArg<cl::Buffer>(2, m_bufAccRGB);
    res += m_kernels.accumulateWaveforms.setArg<cl::Buffer>(3, m_bufAccYUV);
    res += m_kernels.accumulateWaveforms.setArg<cl_uint>(4, m_sourceDims.width);
    res += m_kernels.accumulateWaveforms.setArg<cl_uint>(5, m_sourceDims.height);
    res += m_kernels.accumulateWaveforms.setArg<cl_uchar>(6, 0);
    res += m_kernels.accumulateWaveforms.setArg<CLRect2D>(7, CLRect2D{0, 0, 0, 0});

    CHECK_CL_ERROR_RET(res, "Failed to set OpenCL accumulate kernel arguments");

    static constexpr size_t localHistKernelY = c_WaveformSize.height;

    size_t globalHeight = ((sourceDims.height + localHistKernelY - 1) / localHistKernelY) * localHistKernelY;

    cl::NDRange ndrGlobalAccWF((std::max)(sourceDims.width, c_WaveformSize.width), globalHeight);
    cl::NDRange ndrLocalAccWF(1, localHistKernelY);

    /// CREATE IMAGE KERNEL

    float sampleCount = std::ceil(static_cast<float>(sourceDims.width) / static_cast<float>(c_WaveformSize.width));

    float brightness = (1.f + 8.f * (1080.f / (float)sourceDims.height)) / sampleCount;

    res += m_kernels.createWaveformImages.setArg<cl::Buffer>(0, m_bufAccRGB);
    res += m_kernels.createWaveformImages.setArg<cl::Buffer>(1, m_bufAccYUV);
    res += m_kernels.createWaveformImages.setArg<cl::ImageGL>(2, m_targetTextures->wfLuma.clImageGL);
    res += m_kernels.createWaveformImages.setArg<cl::ImageGL>(3, m_targetTextures->wfRGB.clImageGL);
    res += m_kernels.createWaveformImages.setArg<cl::ImageGL>(4, m_targetTextures->wfRGBParade.clImageGL);
    res += m_kernels.createWaveformImages.setArg<cl::ImageGL>(5, m_targetTextures->wfRGBBlacks.clImageGL);
    res += m_kernels.createWaveformImages.setArg<cl::ImageGL>(6, m_targetTextures->wfYUVParade.clImageGL);
    res += m_kernels.createWaveformImages.setArg<cl_uint>(7, sourceDims.width);
    res += m_kernels.createWaveformImages.setArg<cl_int>(8, static_cast<cl_int>(YUVColorSpace::BT709));
    res += m_kernels.createWaveformImages.setArg<cl_float>(9, brightness);

    CHECK_CL_ERROR_RET(res, "Failed to set OpenCL create waveform images kernel arguments");

    cl::NDRange ndrGlobalCreateWF(c_WaveformSize.width, c_WaveformSize.height);
    cl::NDRange ndrLocalCreateWF(1, c_WaveformSize.height);

    /// UPLOAD SOURCE DATA

    {
        res = m_commandQueue.enqueueWriteBuffer(
            m_bufSource, CL_FALSE, 0, m_sourceSizeBytes, sourceData);

        CHECK_CL_ERROR_RET(res, "Failed to write source data to OpenCL buffer");
    }

    /// RESET ACC BUFFERS

    {
        res = m_commandQueue.enqueueFillBuffer(m_bufAccRGB, 0, 0, c_WaveformSize.Area() * sizeof(cl_uint) * 3);
        CHECK_CL_ERROR_RET(res, "Failed to fill OpenCL buffer for RGB accumulation");
        res = m_commandQueue.enqueueFillBuffer(m_bufAccYUV, 0, 0, c_WaveformSize.Area() * sizeof(cl_uint) * 3);
        CHECK_CL_ERROR_RET(res, "Failed to fill OpenCL buffer for YUV accumulation");
        res = m_commandQueue.enqueueFillBuffer(m_bufAcc2D_UV_RGBA, 0, 0, c_ScopeSize.Area() * sizeof(cl_uint) * 4);
        CHECK_CL_ERROR_RET(res, "Failed to fill OpenCL buffer for UV scope accumulation");
        res = m_commandQueue.enqueueFillBuffer(m_bufAcc2D_XYZ_RGBA, 0, 0, c_ScopeSize.Area() * sizeof(cl_uint) * 4);
        CHECK_CL_ERROR_RET(res, "Failed to fill OpenCL buffer for XYZ scope accumulation");
        res = m_commandQueue.enqueueFillBuffer(m_bufAcc2D_DIA_RGBA, 0, 0, c_ScopeSize.Area() * sizeof(cl_uint) * 4);
        CHECK_CL_ERROR_RET(res, "Failed to fill OpenCL buffer for DIA scope accumulation");
    }

    /// EXECUTE KERNELS

    res = m_commandQueue.enqueueAcquireGLObjects(&m_glObjects);

    CHECK_CL_ERROR_RET(res, "Failed to acquire OpenCL GL objects");

    res = m_commandQueue.enqueueNDRangeKernel(
        convert_kernel,
        cl::NullRange,
        ndrGlobalConvert,
        cl::NullRange);

    CHECK_CL_ERROR_RET(res, "Failed to enqueue OpenCL convert kernel");

    res = m_commandQueue.enqueueNDRangeKernel(
        m_kernels.accumulateWaveforms,
        cl::NullRange,
        ndrGlobalAccWF,
        ndrLocalAccWF);

    CHECK_CL_ERROR_RET(res, "Failed to enqueue OpenCL accumulate waveforms kernel");

    res = m_commandQueue.enqueueNDRangeKernel(
        m_kernels.createWaveformImages,
        cl::NullRange,
        ndrGlobalCreateWF,
        ndrLocalCreateWF);

    CHECK_CL_ERROR_RET(res, "Failed to enqueue OpenCL create waveform images kernel");

    res = m_commandQueue.enqueueReleaseGLObjects(&m_glObjects);

    CHECK_CL_ERROR_RET(res, "Failed to release OpenCL GL objects");

    res = m_commandQueue.finish();

    CHECK_CL_ERROR_RET(res, "Failed to finish OpenCL command queue");
}
} // namespace scpp