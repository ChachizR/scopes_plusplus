#pragma once

#include "pch.hpp"

#include "utils.hpp"
#include "video_source.hpp"
#include "cl_renderer.hpp"
#include "imgui_util.hpp"
#include "source_provider.hpp"

namespace scpp {

class Application {
private:
#if defined(__APPLE__)
    constexpr static auto m_glslVersion = "#version 150"sv;
#else
    constexpr static auto m_glslVersion = "#version 330"sv;
#endif

    float m_mainScale{};

    GLFWwindow* m_window{nullptr};

    ImVec4 m_clearColor{0.05f, 0.05f, 0.05f, 1.00f};

    ImFont* m_fontRoboto{nullptr};

    bool m_showSourcePreview{true};
    bool m_showFalseColor{true};
    bool m_showWFLuma{true};
    bool m_showWFRgb{true};
    bool m_showWFRgbParade{true};
    bool m_showWFRgbBlacks{true};
    bool m_showWFYuvParade{true};
    bool m_showSCUV{true};
    bool m_showSCXYZ{true};
    bool m_showSCDia{true};

    std::unique_ptr<scpp::OpenCLDeviceProvider> m_openclDeviceProvider = nullptr;

    std::unique_ptr<SourceProvider> m_sourceProvider = nullptr;
    std::unique_ptr<VideoSource>    m_source         = nullptr;

public:
    Application();

    ~Application();

    void Run();

private:
    auto InitGLFW() -> bool;

    auto InitImGui() -> bool;

    void SetImGuiStyle();

    auto LoadFonts() -> bool;

    void ShutdownImGui();

    void ShutdownGLFW();

    static inline void GLFWErrorCallback(int error, const char* description) {
        std::println("GLFW Error {}: {}", error, description);
    }

    // UI

    constexpr static auto c_uiWFAspect = WindowAspectData{
        .targetAspectRatio = 580.f / 256.f,
        .offset            = ImVec2(0.f, 32.f) // Account for title bar height
    };

    constexpr static auto c_uiSCWindowSizeOffset = ImVec2(0.f, 32.f);

    constexpr static auto c_uiMinWFSize = ImVec2(100, 32 + 50);
    constexpr static auto c_uiMinSCSize = ImVec2(100, 32 + 100);
    constexpr static auto c_uiMaxSize   = ImVec2(FLT_MAX, FLT_MAX);

    void UI_Main() noexcept;
    void UI_MainMenuBar() noexcept;
    void UI_Settings() const noexcept;
    void SyncRenderFeaturesFromUI() noexcept;
    void UI_Sources() noexcept;
    void UI_SourceStats() noexcept;
    void UI_RenderSettings() noexcept;
    void UI_SourcePreview(const scpp::TargetTextures* sourceTextures) noexcept;
    void UI_FalseColor(const scpp::TargetTextures* sourceTextures) noexcept;
    void UI_Waveforms(const scpp::TargetTextures* sourceTextures) noexcept;
    void UI_Scopes(const scpp::TargetTextures* sourceTextures) noexcept;
    void UI_ActiveSource() noexcept;
};

} // namespace scpp
