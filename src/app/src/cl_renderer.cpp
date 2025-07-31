#include "cl_renderer.hpp"

namespace scpp {
std::optional<cl::Device> OpenCLRenderer::SelectDevice() const {
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

float OpenCLRenderer::GetDeviceScore(const cl::Device& device) const {
    const auto platform = device.getInfo<CL_DEVICE_PLATFORM>();

    const auto platform_vendor  = platform.getInfo<CL_PLATFORM_VENDOR>();
    const auto platform_name    = platform.getInfo<CL_PLATFORM_NAME>();
    const auto platform_version = platform.getInfo<CL_PLATFORM_VERSION>();

    const auto name    = device.getInfo<CL_DEVICE_NAME>();
    const auto vendor  = device.getInfo<CL_DEVICE_VENDOR>();
    const auto profile = device.getInfo<CL_DEVICE_PROFILE>();
    const auto version = device.getInfo<CL_DEVICE_VERSION>();

    const auto computeUnits      = device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>();
    const auto scoreComputeUnits = (float)computeUnits / kFactComputeUnits;

    float score = 1.f;
    score *= scoreComputeUnits;

    const auto clockFrequency      = device.getInfo<CL_DEVICE_MAX_CLOCK_FREQUENCY>();
    const auto scoreClockFrequency = (float)clockFrequency / kFactClockFrequency;

    score *= scoreClockFrequency;

    const auto globalMemory      = device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>();
    const auto scoreGlobalMemory = (float)globalMemory / kFactGlobalMemory;

    score *= scoreGlobalMemory;

    const auto memAllocSize     = device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>();
    const auto scoreMaxMemAlloc = (float)memAllocSize / kFactMaxMemAlloc;

    score *= scoreMaxMemAlloc;

    // const auto localMemory      = device.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>();
    // const auto scoreLocalMemory = (float)localMemory / kFactLocalMemory;
    //  score *= scoreLocalMemory;

    const auto maxWorkGroupSize  = device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>();
    const auto scoreMaxWorkGroup = (float)maxWorkGroupSize / kFactMaxWorkGroup;
    score *= scoreMaxWorkGroup;

    const auto maxIm2DWidth = device.getInfo<CL_DEVICE_IMAGE2D_MAX_WIDTH>();
    // const auto maxIm2DHeight = device.getInfo<CL_DEVICE_IMAGE2D_MAX_HEIGHT>();

    const auto scoreIm2DWidth = (float)maxIm2DWidth / kFactMaxIm2DWidth;

    score *= scoreIm2DWidth;

    const auto maxSamplers = device.getInfo<CL_DEVICE_MAX_SAMPLERS>();

    const auto scoreMaxSamplers = (float)maxSamplers / kFactMaxSamplers;

    score *= scoreMaxSamplers;

    if (maxWorkGroupSize < 256) {
        score = 0;
    }

    return score;
}

OpenCLRenderer::OpenCLRenderer() {

    cl_int res = CL_SUCCESS;

    auto deviceOpt = SelectDevice();

    if (!deviceOpt) {
        std::println("No suitable OpenCL device found.");
        return;
    }

    m_device = *deviceOpt;

    auto platform = m_device.getInfo<CL_DEVICE_PLATFORM>(&res);

    if (res != CL_SUCCESS) {
        std::println("Failed to get OpenCL platform: {}", res);
        return;
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

    m_context = cl::Context(
        std::vector<cl::Device>{m_device},
        props);

    m_commandQueue = cl::CommandQueue(m_context, m_device, cl::QueueProperties(), &res);

    if (res != CL_SUCCESS) {
        std::println("Failed to create OpenCL command queue: {}", res);
        return;
    }

    // GL

    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kImageWidth, kImageHeight, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_clImageGL = cl::ImageGL(
        m_context,
        CL_MEM_WRITE_ONLY,
        GL_TEXTURE_2D,
        0,
        m_textureID);

    m_program = cl::Program(m_context, kFillImageRedKernelSource.data(), false, &res);

    if (res != CL_SUCCESS) {
        std::println("Failed to create OpenCL program: {}", res);
        return;
    }

    res = m_program.build({m_device});

    if (res != CL_SUCCESS) {
        std::println("Failed to build OpenCL program: {}", res);

        cl_build_status status = m_program.getBuildInfo<CL_PROGRAM_BUILD_STATUS>(m_device, &res);

        if (res != CL_SUCCESS) {
            std::println("Failed to get build status: {}", res);
            return;
        }

        std::string log = m_program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(m_device, &res);

        if (res != CL_SUCCESS) {
            std::println("Failed to get build log: {}", res);
            return;
        }

        std::println("OpenCL build status: {}", status);
        std::println("OpenCL build log:\n{}", log);
        return;
    }

    m_fillRedKernel = cl::Kernel(m_program, "fill_red", &res);
    if (res != CL_SUCCESS) {
        std::println("Failed to create OpenCL kernel: {}", res);
        return;
    }

    m_fillRedKernel.setArg(0, m_clImageGL);

    m_glObjects = {m_clImageGL};
}

void OpenCLRenderer::ExecuteKernel() {
    m_commandQueue.enqueueAcquireGLObjects(&m_glObjects);
    m_commandQueue.enqueueNDRangeKernel(
        m_fillRedKernel,
        cl::NullRange,
        cl::NDRange(kImageWidth, kImageHeight),
        cl::NullRange);

    m_commandQueue.enqueueReleaseGLObjects(&m_glObjects);
    m_commandQueue.finish();
}

void OpenCLRenderer::ImGuiImageRender() const {
    ImVec2 avail = ImGui::GetContentRegionAvail();

    float imgW   = static_cast<float>(kImageWidth);
    float imgH   = static_cast<float>(kImageHeight);
    float scaleX = avail.x / imgW;
    float scaleY = avail.y / imgH;
    float scale  = (std::min)((std::min)(scaleX, scaleY), 1.0f);

    ImGui::Image(
        (void*)(intptr_t)m_textureID,
        ImVec2(imgW * scale, imgH * scale));
}
} // namespace scpp
