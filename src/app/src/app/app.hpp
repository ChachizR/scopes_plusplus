#pragma once

#include "pch.hpp"

#include "../ui/ui.hpp"

namespace scpp {

class Application {
private:
    constexpr static auto m_glslVersion = "#version 330"sv;

    float m_mainScale{};

    GLFWwindow* m_window{nullptr};

    ImVec4 m_clearColor{0.05f, 0.05f, 0.05f, 1.00f};

    ImFont* m_fontRoboto{nullptr};

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

    void Render();

    static inline void GLFWErrorCallback(int error, const char* description) {
        std::println("GLFW Error {}: {}", error, description);
    }
};

} // namespace scpp