#pragma once

#include "pch.hpp"

#include "utils.hpp"
#include "video_source.hpp"
#include "cl_renderer.hpp"
#include "ndi_source_provider.hpp"

namespace scpp {

class Application {
private:
    constexpr static auto m_glslVersion = "#version 330"sv;

    float m_mainScale{};

    GLFWwindow* m_window{nullptr};

    ImVec4 m_clearColor{0.05f, 0.05f, 0.05f, 1.00f};

    ImFont* m_fontRoboto{nullptr};

    std::unique_ptr<scpp::OpenCLDeviceProvider> m_openclDeviceProvider = nullptr;

    std::unique_ptr<NDISourceProvider> m_ndiSourceProvider = nullptr;

public:
    Application();

    ~Application();

    void Run();

private:
    [[nodiscard]]
    bool InitGLFW();

    [[nodiscard]]
    bool InitImGui();

    [[nodiscard]]
    bool LoadFonts();

    void ShutdownImGui();

    void ShutdownGLFW();


    static inline void GLFWErrorCallback(int error, const char* description) {
        std::println("GLFW Error {}: {}", error, description);
    }

    // UI

    void UI_Main(VideoSource* source) const noexcept;
    void UI_MainMenuBar() const noexcept;
    void UI_Settings() const noexcept;
    void UI_NDISources() const noexcept;
};

} // namespace scpp