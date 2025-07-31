#pragma once

#include "pch.hpp"

namespace scpp {

static std::string_view kFillImageRedKernelSource = R"CLC(
__kernel void fill_red(write_only image2d_t dest) {
    int2 pos = (int2)(get_global_id(0), get_global_id(1));
    write_imagef(dest, pos, (float4)(1.0f, 0.0f, 0.0f, 1.0f));
}
)CLC"sv;

class OpenCLRenderer {
private:
    cl::Device       m_device;
    cl::Context      m_context;
    cl::CommandQueue m_commandQueue;
    cl::Program      m_program;
    cl::Kernel       m_fillRedKernel;

    GLuint m_textureID{0};

    constexpr static uint32_t kImageWidth  = 3840;
    constexpr static uint32_t kImageHeight = 2160;

    cl::ImageGL             m_clImageGL;
    std::vector<cl::Memory> m_glObjects;

    const float kFactComputeUnits   = 40.0f;
    const float kFactClockFrequency = 2000.0f;
    const float kFactGlobalMemory   = 8.0f * 0x1p30;
    const float kFactMaxMemAlloc    = 8.0f * 0x1p30 * 2.f;
    const float kFactLocalMemory    = 48.0f * 1024;
    const float kFactMaxWorkGroup   = 256.0f;
    const float kFactMaxIm2DWidth   = 8192.0f;
    const float kFactMaxSamplers    = 16.0f;

    [[nodiscard]]
    std::optional<cl::Device> SelectDevice() const;

    [[nodiscard]]
    float GetDeviceScore(const cl::Device& device) const;

public:
    OpenCLRenderer();

    void ExecuteKernel();

    [[nodiscard]]
    GLuint GetTextureID() const {
        return m_textureID;
    }

    void ImGuiImageRender() const;
};

} // namespace scpp