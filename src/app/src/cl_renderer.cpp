#include "cl_renderer.hpp"

namespace scpp {

OpenCLDeviceProvider::OpenCLDeviceProvider() {
    const auto deviceOpt = SelectDevice();

    if (!deviceOpt) {

        m_initialized = false;
        std::println("No suitable OpenCL device found.");
        return;
    }

    m_device = *deviceOpt;

    const auto contextOpt = CreateContext();

    if (!contextOpt) {
        std::println("Failed to create OpenCL context.");
        m_initialized = false;
        return;
    }

    m_context = *contextOpt;

    auto kernelsResult = CreateKernels();

    if (!kernelsResult) {
        std::println("Failed to create OpenCL kernels: {}", kernelsResult.error());
        m_initialized = false;
        return;
    }

    m_kernels = *kernelsResult;

    m_initialized = true;
}

auto OpenCLDeviceProvider::GetDeviceScore(const cl::Device& device) const noexcept -> float {
    const auto platform = device.getInfo<CL_DEVICE_PLATFORM>();

    const auto platform_vendor  = platform.getInfo<CL_PLATFORM_VENDOR>();
    const auto platform_name    = platform.getInfo<CL_PLATFORM_NAME>();
    const auto platform_version = platform.getInfo<CL_PLATFORM_VERSION>();

    const auto name    = device.getInfo<CL_DEVICE_NAME>();
    const auto vendor  = device.getInfo<CL_DEVICE_VENDOR>();
    const auto profile = device.getInfo<CL_DEVICE_PROFILE>();
    const auto version = device.getInfo<CL_DEVICE_VERSION>();

    const auto computeUnits      = device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>();
    const auto scoreComputeUnits = (float)computeUnits / c_FactComputeUnits;

    float score = 1.f;
    score *= scoreComputeUnits;

    const auto clockFrequency      = device.getInfo<CL_DEVICE_MAX_CLOCK_FREQUENCY>();
    const auto scoreClockFrequency = (float)clockFrequency / c_FactClockFrequency;

    score *= scoreClockFrequency;

    const auto globalMemory      = device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>();
    const auto scoreGlobalMemory = (float)globalMemory / c_FactGlobalMemory;

    score *= scoreGlobalMemory;

    const auto memAllocSize     = device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>();
    const auto scoreMaxMemAlloc = (float)memAllocSize / c_FactMaxMemAlloc;

    score *= scoreMaxMemAlloc;

    // const auto localMemory      = device.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>();
    // const auto scoreLocalMemory = (float)localMemory / c_FactLocalMemory;
    //  score *= scoreLocalMemory;

    const auto maxWorkGroupSize  = device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>();
    const auto scoreMaxWorkGroup = (float)maxWorkGroupSize / c_FactMaxWorkGroup;
    score *= scoreMaxWorkGroup;

    const auto maxIm2DWidth = device.getInfo<CL_DEVICE_IMAGE2D_MAX_WIDTH>();
    // const auto maxIm2DHeight = device.getInfo<CL_DEVICE_IMAGE2D_MAX_HEIGHT>();

    const auto scoreIm2DWidth = (float)maxIm2DWidth / c_FactMaxIm2DWidth;

    score *= scoreIm2DWidth;

    const auto maxSamplers = device.getInfo<CL_DEVICE_MAX_SAMPLERS>();

    const auto scoreMaxSamplers = (float)maxSamplers / c_FactMaxSamplers;

    score *= scoreMaxSamplers;

    if (maxWorkGroupSize < 256) {
        score = 0;
    }

    return score;
}

auto OpenCLDeviceProvider::SelectDevice() const -> std::optional<cl::Device> {
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);

    std::optional<cl::Device> selectedDevice;

    float bestScore = 0.0f;

    for (const auto& platform : platforms) {
        std::vector<cl::Device> devices;
        platform.getDevices(CL_DEVICE_TYPE_GPU, &devices);

        for (const auto& device : devices) {
            auto deviceType = device.getInfo<CL_DEVICE_TYPE>();
            if (deviceType != CL_DEVICE_TYPE_GPU) {
                continue; // Skip non-GPU devices
            }

            float score = GetDeviceScore(device);

            if (score > bestScore) {
                bestScore      = score;
                selectedDevice = device;
            }
        }
    }

    return selectedDevice;
}

auto OpenCLDeviceProvider::CreateContext() -> std::optional<cl::Context> {
    cl_int res = CL_SUCCESS;

    auto platform = m_device.getInfo<CL_DEVICE_PLATFORM>(&res);

    if (res != CL_SUCCESS) {
        std::println("Failed to get OpenCL platform: {}", res);
        return std::nullopt;
    }

#ifdef _WIN32
    HGLRC                 nativeGLCtx = wglGetCurrentContext();
    HDC                   nativeDC    = wglGetCurrentDC();
    cl_context_properties props[]     = {
        CL_GL_CONTEXT_KHR, (cl_context_properties)nativeGLCtx,
        CL_WGL_HDC_KHR, (cl_context_properties)nativeDC,
        CL_CONTEXT_PLATFORM, (cl_context_properties)(platform()),
        0};
#elif defined(__APPLE__)
    CGLContextObj         cglCtx   = CGLGetCurrentContext();
    CGLShareGroupObj      shareGrp = CGLGetShareGroup(cglCtx);
    cl_context_properties props[]  = {
        CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE,
        (cl_context_properties)shareGrp,
        CL_CONTEXT_PLATFORM, (cl_context_properties)(platform()),
        0};
#else
    GLXContext            glxCtx  = glXGetCurrentContext();
    Display*              dpy     = glXGetCurrentDisplay();
    cl_context_properties props[] = {
        CL_GL_CONTEXT_KHR, (cl_context_properties)glxCtx,
        CL_GLX_DISPLAY_KHR, (cl_context_properties)dpy,
        CL_CONTEXT_PLATFORM, (cl_context_properties)(platform()),
        0};
#endif

    cl::Context context(
        std::vector<cl::Device>{m_device},
        props,
        nullptr,
        nullptr,
        &res);

    if (res != CL_SUCCESS) {
        std::println("Failed to create OpenCL context: {}", res);
        return std::nullopt;
    }

    return context;
}

auto OpenCLDeviceProvider::CreateKernels() const -> std::expected<RenderPipelineKernels, ErrorCode> {
    const auto path = std::filesystem::path{c_KernelSourcePath};

    if (!std::filesystem::exists(path)) {
        std::println("OpenCL kernel source file not found: {}", path.string());
        return std::unexpected(ErrorCode::KernelSourceNotFound);
    }

    std::ifstream kernelFile(path);
    if (!kernelFile.is_open()) {
        std::println("Failed to open OpenCL kernel source file: {}", path.string());
        return std::unexpected(ErrorCode::FSNotFound);
    }

    std::string kernelSource((std::istreambuf_iterator<char>(kernelFile)),
                             std::istreambuf_iterator<char>());

    kernelFile.close();

    cl_int      res = CL_SUCCESS;
    cl::Program program(m_context, kernelSource, false, &res);

    if (res != CL_SUCCESS) {
        std::println("Failed to create OpenCL program: {}", res);
        return std::unexpected(ErrorCode::KernelCreateProgramFailed);
    }

    res = program.build({m_device});

    if (res != CL_SUCCESS) {
        std::println("Failed to build OpenCL program: {}", res);

        cl_build_status status = program.getBuildInfo<CL_PROGRAM_BUILD_STATUS>(m_device, &res);

        if (res != CL_SUCCESS) {
            std::println("Failed to get build status: {}", res);
            return std::unexpected(ErrorCode::KernelProgramBuildFailed);
        }

        std::string log = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(m_device, &res);

        if (res != CL_SUCCESS) {
            std::println("Failed to get build log: {}", res);
            return std::unexpected(ErrorCode::KernelProgramBuildFailed);
        }

        std::println("OpenCL build status: {}", status);
        std::println("OpenCL build log:\n{}", log);

        return std::unexpected(ErrorCode::KernelProgramBuildFailed);
    }

    RenderPipelineKernels kernels;

    kernels.convertSource_RGBA_8888 =
        cl::Kernel(program, c_KernelName_convertSource_RGBA_8888.data(), &res);

    if (res != CL_SUCCESS) {
        std::println("Failed to create OpenCL kernel: {} {}", c_KernelName_convertSource_RGBA_8888, res);
        return std::unexpected(ErrorCode::KernelCreateKernelFailed);
    }

    kernels.convertSource_RGBX_8888 =
        cl::Kernel(program, c_KernelName_convertSource_RGBX_8888.data(), &res);

    if (res != CL_SUCCESS) {
        std::println("Failed to create OpenCL kernel: {} {}", c_KernelName_convertSource_RGBX_8888, res);
        return std::unexpected(ErrorCode::KernelCreateKernelFailed);
    }

    kernels.initialized = true;

    return kernels;
}

CLGLTextureRGBA::CLGLTextureRGBA(Dims2D size, const cl::Context& context)
    : size{size} {
    glGenTextures(1, &glTextureID);
    glBindTexture(GL_TEXTURE_2D, glTextureID);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA8,
        size.width, size.height, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

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
    glBindTexture(GL_TEXTURE_2D, glTextureID);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA8,
        size.width, size.height, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
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

    std::println("Resized OpenCL texture {}: {} to {}x{}", glTextureID, description, size.width, size.height);
}

inline constexpr auto OpenCLRenderer::SelectConvertKernel(SourceFormat sourceFormat) -> cl::Kernel& {
    switch (sourceFormat) {
    case SourceFormat::RGBA_8888:
        return m_kernels.convertSource_RGBA_8888;
    case SourceFormat::RGBX_8888:
        return m_kernels.convertSource_RGBX_8888;
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
        CLGLTextureRGBA("Source Preview"sv, kDefaultSourceSize, m_context),
        CLGLTextureRGBA("False Color"sv, kDefaultSourceSize, m_context),
        CLGLTextureRGBA("Luminance Waveform"sv, kWaveformSize, m_context));

    m_glObjects = {m_targetTextures->sourcePreview.clImageGL,
                   m_targetTextures->falseColor.clImageGL,
                   m_targetTextures->wfLuma.clImageGL};

    m_sourceFormat = SourceFormat::unknown;
    m_kernels      = m_deviceProviderRef.GetKernels();

    if (!m_kernels.initialized) {
        std::println("OpenCL kernels are not initialized.");
        return;
    }
}

void OpenCLRenderer::ResizeSourceTextures() {
    if (!m_targetTextures) {
        return;
    }

    m_targetTextures->sourcePreview.Resize(m_sourceDims, m_context);
    m_targetTextures->falseColor.Resize(m_sourceDims, m_context);

    m_glObjects = {
        m_targetTextures->sourcePreview.clImageGL,
        m_targetTextures->falseColor.clImageGL,
        m_targetTextures->wfLuma.clImageGL};
}

void OpenCLRenderer::ResizeBuffers() {

    m_sourceSizeBytes = GetSourceSize(m_sourceFormat, m_sourceDims);
    m_rgbaSizeBytes   = m_sourceDims.Area() * 4;
    m_yuvSizeBytes    = m_sourceDims.Area() * 3;

    m_bufSource     = cl::Buffer(m_context, CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY, m_sourceSizeBytes);
    m_bufIntermRGBA = cl::Buffer(m_context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, m_rgbaSizeBytes);
    m_bufIntermYUV  = cl::Buffer(m_context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, m_yuvSizeBytes);

    m_buffersInitialized = true;

    std::println("Resized OpenCL buffers: {}x{} "
                 "Source: {} bytes, "
                 "Interm RGBA: {} bytes, "
                 "Interm YUV: {} bytes",
                 m_sourceDims.width, m_sourceDims.height,
                 m_sourceSizeBytes, m_rgbaSizeBytes, m_yuvSizeBytes);
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

    if (res != CL_SUCCESS) [[unlikely]] {
        std::println("Failed to set OpenCL kernel arguments");
        return;
    }

    cl::NDRange ndrGlobalConvert(m_sourceDims.width * m_sourceDims.height);

    res = m_commandQueue.enqueueWriteBuffer(
        m_bufSource, CL_TRUE, 0, m_sourceSizeBytes, sourceData);

    if (res != CL_SUCCESS) [[unlikely]] {
        std::println("Failed to write source data to OpenCL buffer: {}", res);
        return;
    }

    res = m_commandQueue.enqueueAcquireGLObjects(&m_glObjects);

    if (res != CL_SUCCESS) [[unlikely]] {
        std::println("Failed to acquire OpenCL GL objects: {}", res);
        return;
    }

    res = m_commandQueue.enqueueNDRangeKernel(
        convert_kernel,
        cl::NullRange,
        ndrGlobalConvert,
        cl::NullRange);

    if (res != CL_SUCCESS) [[unlikely]] {
        std::println("Failed to enqueue OpenCL kernel: {}", res);
        return;
    }

    res = m_commandQueue.enqueueReleaseGLObjects(&m_glObjects);

    if (res != CL_SUCCESS) [[unlikely]] {
        std::println("Failed to release OpenCL GL objects: {}", res);
        return;
    }

    res = m_commandQueue.finish();

    if (res != CL_SUCCESS) [[unlikely]] {
        std::println("Failed to finish OpenCL command queue: {}", res);
        return;
    }
}
} // namespace scpp