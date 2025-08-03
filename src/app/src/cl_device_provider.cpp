#include "cl_device_provider.hpp"

#define CHECK_KERNEL_ERROR(res, name)                                     \
    if (res != CL_SUCCESS) {                                              \
        std::println("Failed to create OpenCL kernel: {} {}", name, res); \
        return std::unexpected(ErrorCode::KernelCreateKernelFailed);      \
    }

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

    if (selectedDevice) {
        std::println("Selected OpenCL device: {}", selectedDevice->getInfo<CL_DEVICE_NAME>());
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

auto OpenCLDeviceProvider::LoadProgramFromFile(const std::filesystem::path& path) const -> std::expected<cl::Program, ErrorCode> {
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
    cl_int res = CL_SUCCESS;

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
    return program;
}

auto OpenCLDeviceProvider::CreateKernels() const -> std::expected<RenderPipelineKernels, ErrorCode> {
    auto convertProgramPath = std::filesystem::path{c_KernelConvertSourcePath};

    auto convertProgramEx = LoadProgramFromFile(convertProgramPath);

    if (!convertProgramEx) {
        std::println("Failed to load OpenCL program from file: {}", convertProgramPath.string());
        return std::unexpected(convertProgramEx.error());
    }

    cl_int res = CL_SUCCESS;

    const auto convertProgram = *convertProgramEx;

    RenderPipelineKernels kernels;

    kernels.convertSource_RGBA_8888 =
        cl::Kernel(convertProgram, c_KernelName_convertSource_RGBA_8888.data(), &res);

    CHECK_KERNEL_ERROR(res, c_KernelName_convertSource_RGBA_8888);

    kernels.convertSource_RGBX_8888 =
        cl::Kernel(convertProgram, c_KernelName_convertSource_RGBX_8888.data(), &res);

    CHECK_KERNEL_ERROR(res, c_KernelName_convertSource_RGBX_8888);

    kernels.convertSource_BGRA_8888 =
        cl::Kernel(convertProgram, c_KernelName_convertSource_BGRA_8888.data(), &res);

    CHECK_KERNEL_ERROR(res, c_KernelName_convertSource_BGRA_8888);

    kernels.convertSource_BGRX_8888 =
        cl::Kernel(convertProgram, c_KernelName_convertSource_BGRX_8888.data(), &res);

    CHECK_KERNEL_ERROR(res, c_KernelName_convertSource_BGRX_8888);

    kernels.convertSource_ARGB_8888 =
        cl::Kernel(convertProgram, c_KernelName_convertSource_ARGB_8888.data(), &res);

    CHECK_KERNEL_ERROR(res, c_KernelName_convertSource_ARGB_8888);

    kernels.convertSource_RGB_888 =
        cl::Kernel(convertProgram, c_KernelName_convertSource_RGB_888.data(), &res);

    CHECK_KERNEL_ERROR(res, c_KernelName_convertSource_RGB_888);

    kernels.convertSource_BGR_888_InvY =
        cl::Kernel(convertProgram, c_KernelName_convertSource_BGR_888_InvY.data(), &res);

    CHECK_KERNEL_ERROR(res, c_KernelName_convertSource_BGR_888_InvY);

    kernels.convertSource_UYVY_422 =
        cl::Kernel(convertProgram, c_KernelName_convertSource_UYVY_422.data(), &res);

    CHECK_KERNEL_ERROR(res, c_KernelName_convertSource_UYVY_422);

    kernels.convertSource_YUYV_422 =
        cl::Kernel(convertProgram, c_KernelName_convertSource_YUYV_422.data(), &res);

    CHECK_KERNEL_ERROR(res, c_KernelName_convertSource_YUYV_422);

    kernels.convertSource_NV12 =
        cl::Kernel(convertProgram, c_KernelName_convertSource_NV12.data(), &res);

    CHECK_KERNEL_ERROR(res, c_KernelName_convertSource_NV12);

    const auto accumulateProgramPath = std::filesystem::path{c_KernelAccumulateSourcePath};

    auto accumulateProgramEx = LoadProgramFromFile(accumulateProgramPath);

    if (!accumulateProgramEx) {
        std::println("Failed to load OpenCL program from file: {}", accumulateProgramPath.string());
        return std::unexpected(accumulateProgramEx.error());
    }

    const auto accumulateProgram = *accumulateProgramEx;

    kernels.accumulateWaveforms =
        cl::Kernel(accumulateProgram, c_KernelName_accumulateWaveforms.data(), &res);

    CHECK_KERNEL_ERROR(res, c_KernelName_accumulateWaveforms);

    const auto createImagesProgramPath = std::filesystem::path{c_KernelCreateImagesSourcePath};

    auto createImagesProgramEx = LoadProgramFromFile(createImagesProgramPath);

    if (!createImagesProgramEx) {
        std::println("Failed to load OpenCL program from file: {}", createImagesProgramPath.string());
        return std::unexpected(createImagesProgramEx.error());
    }

    const auto createImagesProgram = *createImagesProgramEx;

    kernels.createWaveformImages =
        cl::Kernel(createImagesProgram, c_KernelName_createWaveformImages.data(), &res);

    CHECK_KERNEL_ERROR(res, c_KernelName_createWaveformImages);

    kernels.initialized = true;

    return kernels;
}
} // namespace scpp