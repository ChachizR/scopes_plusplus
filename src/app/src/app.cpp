#include "app.hpp"

namespace scpp {
Application::Application() {
    if (!InitGLFW()) {
        std::println("Failed to initialize GLFW");
        throw std::runtime_error("GLFW initialization failed");
    }
    if (!InitImGui()) {
        std::println("Failed to initialize ImGui");
        glfwDestroyWindow(m_window);
        glfwTerminate();
        throw std::runtime_error("ImGui initialization failed");
    }

    SetImGuiStyle();

    m_openclDeviceProvider = std::make_unique<OpenCLDeviceProvider>();

    if (!m_openclDeviceProvider->IsInitialized()) {
        std::println("Failed to initialize OpenCL device provider");
        throw std::runtime_error("OpenCL device provider initialization failed");
    }

    m_ndiSourceProvider = std::make_unique<NDISourceProvider>(*m_openclDeviceProvider);
}

Application::~Application() {
    ShutdownImGui();
    ShutdownGLFW();
}

auto Application::InitGLFW() -> bool {
    glfwSetErrorCallback(GLFWErrorCallback);

    if (!glfwInit()) {
        std::println("Failed to initialize GLFW");
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    m_mainScale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());

    m_window = glfwCreateWindow((int32_t)(1280 * m_mainScale), (int32_t)(720 * m_mainScale), "Scopes++", nullptr, nullptr);

    if (!m_window) {
        std::println("Failed to create GLFW window");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);
    return true;
}

auto Application::InitImGui() -> bool {
    IMGUI_CHECKVERSION();

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    if (!LoadFonts())
        return false;

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-Viewport / Platform Windows
    io.ConfigViewportsNoDecoration       = false;
    io.ConfigViewportsNoTaskBarIcon      = false;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(m_mainScale);
    style.FontScaleDpi = m_mainScale;

    /*io.ConfigDpiScaleFonts     = true;
    io.ConfigDpiScaleViewports = true;*/

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding              = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init(m_glslVersion.data());

    return true;
}

void Application::SetImGuiStyle() {
    // ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowPadding    = ImVec2(8, 8); // padding within a window
    style.FramePadding     = ImVec2(8, 4); // space around text in buttons, inputs, etc.
    style.ItemSpacing      = ImVec2(8, 6); // space between items in a window
    style.ItemInnerSpacing = ImVec2(8, 8); // space between items in a same line
    style.IndentSpacing    = 24.f;
    style.ScrollbarSize    = 16.f; // width of the scrollbar
    style.GrabMinSize      = 12.f;

    style.WindowBorderSize = 1.f;
    style.ChildBorderSize  = 0.f;
    style.FrameBorderSize  = 0.f;
    style.PopupBorderSize  = 0.f;
    style.TabBorderSize    = 0.f;
    style.TabBarBorderSize = 1.f;

    style.WindowRounding    = 8.0f;
    style.ChildRounding     = 8.0f;
    style.FrameRounding     = 6.0f;
    style.PopupRounding     = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding      = 6.0f;
    style.TabRounding       = 6.0f;

    style.CellPadding = ImVec2(4, 4);

    style.WindowTitleAlign         = ImVec2(0.5f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_Right;

    style.DockingSeparatorSize = 1.f;

    auto rgba = [](float r, float g, float b, float a) { return ImVec4(r / 255.f, g / 255.f, b / 255.f, a); };

    auto withAlpha = [](ImVec4 col, float a) { return ImVec4{col.x, col.y, col.z, a}; };

    constexpr static auto c_bg0 = rgba(8, 8, 8, 1.f);
    constexpr static auto c_bg1 = rgba(16, 16, 16, 1.f);
    constexpr static auto c_bg2 = rgba(15, 38, 34, 1.f);

    constexpr static auto c_hl0 = rgba(64, 245, 205, 1.f);
    constexpr static auto c_hl1 = rgba(42, 173, 145, 1.f);

    ImVec4* colors = style.Colors;

    colors[ImGuiCol_WindowBg]       = c_bg0;
    colors[ImGuiCol_PopupBg]        = c_bg1;
    colors[ImGuiCol_ChildBg]        = c_bg1;
    colors[ImGuiCol_FrameBg]        = withAlpha(c_hl1, 0.25f);
    colors[ImGuiCol_FrameBgHovered] = withAlpha(c_hl1, 0.5f);
    colors[ImGuiCol_FrameBgActive]  = c_hl1;

    colors[ImGuiCol_Text]           = rgba(255, 255, 255, 1.f);
    colors[ImGuiCol_TextDisabled]   = rgba(128, 128, 128, 1.f);
    colors[ImGuiCol_TextLink]       = c_hl0;
    colors[ImGuiCol_TextSelectedBg] = withAlpha(c_hl0, 0.25f);

    colors[ImGuiCol_Border]        = rgba(28, 28, 28, 1.f);
    colors[ImGuiCol_TitleBg]       = c_bg0;
    colors[ImGuiCol_TitleBgActive] = c_bg2;

    colors[ImGuiCol_ScrollbarBg]          = withAlpha(c_bg0, 0.1f);
    colors[ImGuiCol_ScrollbarGrab]        = withAlpha(c_hl1, 0.25f);
    colors[ImGuiCol_ScrollbarGrabHovered] = withAlpha(c_hl1, 0.5f);
    colors[ImGuiCol_ScrollbarGrabActive]  = c_hl1;

    colors[ImGuiCol_CheckMark]        = c_hl0;
    colors[ImGuiCol_SliderGrab]       = withAlpha(c_hl0, 0.5f);
    colors[ImGuiCol_SliderGrabActive] = c_hl0;

    colors[ImGuiCol_Button]        = withAlpha(c_hl0, 0.25f);
    colors[ImGuiCol_ButtonHovered] = withAlpha(c_hl0, 0.5f);
    colors[ImGuiCol_ButtonActive]  = c_hl0;

    colors[ImGuiCol_Header]        = withAlpha(c_hl1, 0.25f);
    colors[ImGuiCol_HeaderHovered] = withAlpha(c_hl1, 0.5f);
    colors[ImGuiCol_HeaderActive]  = c_hl1;

    colors[ImGuiCol_Separator]        = withAlpha(c_hl1, 1.f);
    colors[ImGuiCol_SeparatorHovered] = withAlpha(c_hl1, 0.5f);
    colors[ImGuiCol_SeparatorActive]  = c_hl1;

    colors[ImGuiCol_ResizeGrip]        = withAlpha(c_hl1, 0.25f);
    colors[ImGuiCol_ResizeGripHovered] = withAlpha(c_hl1, 0.5f);
    colors[ImGuiCol_ResizeGripActive]  = c_hl1;

    colors[ImGuiCol_Tab]                 = withAlpha(c_hl1, 0.25f);
    colors[ImGuiCol_TabHovered]          = withAlpha(c_hl1, 0.5f);
    colors[ImGuiCol_TabSelected]         = c_hl1;
    colors[ImGuiCol_TabSelectedOverline] = c_hl0;

    colors[ImGuiCol_TabDimmed]                 = withAlpha(c_hl1, 0.1f);
    colors[ImGuiCol_TabDimmedSelected]         = withAlpha(c_hl1, 0.25f);
    colors[ImGuiCol_TabDimmedSelectedOverline] = withAlpha(c_hl1, 0.0f);

    colors[ImGuiCol_DockingPreview] = withAlpha(c_hl0, 0.25f);

    colors[ImGuiCol_TableHeaderBg]     = withAlpha(c_hl1, 0.25f);
    colors[ImGuiCol_TableRowBgAlt]     = rgba(0, 0, 0, 0.f);
    colors[ImGuiCol_TableRowBg]        = rgba(0, 0, 0, 0.f);
    colors[ImGuiCol_TableBorderLight]  = rgba(128, 128, 128, 0.25f);
    colors[ImGuiCol_TableBorderStrong] = rgba(128, 128, 128, 0.5f);
}

auto Application::LoadFonts() -> bool {
    ImGuiIO& io = ImGui::GetIO();

    m_fontRoboto = io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto-VariableFont_wdth,wght.ttf");

    if (!m_fontRoboto)
        return false;

    return true;
}

void Application::ShutdownImGui() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void Application::ShutdownGLFW() {
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

void Application::Run() {

    while (!glfwWindowShouldClose(m_window)) {

        glfwPollEvents();
        if (glfwGetWindowAttrib(m_window, GLFW_ICONIFIED) != 0) {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (true)
            ImGui::ShowDemoWindow();

        UI_Main();

        ImGui::ShowMetricsWindow();

        // render stuff

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(m_window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(m_clearColor.x * m_clearColor.w, m_clearColor.y * m_clearColor.w, m_clearColor.z * m_clearColor.w, m_clearColor.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(m_window);
    }
}

void Application::UI_Main() noexcept {
    UI_MainMenuBar();

    ImGui::DockSpaceOverViewport(
        0, ImGui::GetMainViewport(),
        ImGuiDockNodeFlags_PassthruCentralNode);

    // UI_Settings();
    UI_NDISources();

    if (m_source) {
        UI_ActiveSource();
        UI_SourceStats();
        UI_RenderSettings();
    }
}

void Application::UI_MainMenuBar() const noexcept {
    ImGui::BeginMainMenuBar();

    // File menu
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Exit")) {
            glfwSetWindowShouldClose(glfwGetCurrentContext(), true);
        }
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void Application::UI_Settings() const noexcept {
    ImGui::Begin("Settings");

    ImGui::End();
}

void Application::UI_NDISources() noexcept {
    const auto sources = m_ndiSourceProvider->GetSources();

    ImGui::Begin("NDI Sources");
    if (ImGui::BeginTable("NDI Sources Table", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Select", ImGuiTableColumnFlags_::ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("URL");
        ImGui::TableHeadersRow();
        for (const auto& source : sources) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Button(std::format("Select##{}", source.name).c_str())) {
                if (m_source) {
                    m_source->Stop();
                }
                m_source = std::make_unique<NDISource>(
                    *m_openclDeviceProvider,
                    source.AsNDIlibSource());
                if (m_source->Start() != ErrorCode::None) {
                    std::println("Failed to start NDI source: {}", m_source->GetName());
                    m_source.reset();
                }
            }
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(source.name.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(source.urlAddress.c_str());
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

void Application::UI_ActiveSource() noexcept {
    auto& sourceRenderer = m_source->GetRenderer();
    if (sourceRenderer.needsResizeFlag_mainThread) {
        sourceRenderer.ResizeGLTextures();
    }

    auto sourceTextures = sourceRenderer.GetTargetTextures();

    auto& sourcePreview = sourceTextures->sourcePreview;

    const auto& renderSettings = m_source->GetRenderSettings();

    /// SOURCE PREVIEW

    ImGui::SetNextWindowSizeConstraints(ImVec2(160, 32 + 90), c_uiMaxSize);
    ImGui::Begin(sourcePreview.description.data(), nullptr, ImGuiWindowFlags_NoCollapse);
    ImGuiUtilImageRender(sourcePreview, ScaleBehavior::ScaleToFit);
    ImGui::End();

    /// WAVEFORMS

    auto& wfLuma = sourceTextures->wfLuma;

    ImGui::SetNextWindowSizeConstraints(c_uiMinWFSize, c_uiMaxSize, WindowSizeConstraints::AspectWithOffset, (void*)&c_uiWFAspect);
    ImGui::Begin(wfLuma.description.data(), nullptr, ImGuiWindowFlags_NoCollapse);
    ImGuiUtilRenderLumaWF(wfLuma, renderSettings.yuvRange,
                          ScaleBehavior::ScaleToFit, FlipBehavior::FlipVertically);
    ImGui::End();

    auto& wfRgb = sourceTextures->wfRGB;
    ImGui::SetNextWindowSizeConstraints(c_uiMinWFSize, c_uiMaxSize, WindowSizeConstraints::AspectWithOffset, (void*)&c_uiWFAspect);
    ImGui::Begin(wfRgb.description.data(), nullptr, ImGuiWindowFlags_NoCollapse);
    ImGuiUtilRenderRGBWF(wfRgb, ScaleBehavior::ScaleToFit, FlipBehavior::FlipVertically);
    ImGui::End();

    auto& wfRgbParade = sourceTextures->wfRGBParade;

    ImGui::SetNextWindowSizeConstraints(c_uiMinWFSize, c_uiMaxSize, WindowSizeConstraints::AspectWithOffset, (void*)&c_uiWFAspect);
    ImGui::Begin(wfRgbParade.description.data(), nullptr, ImGuiWindowFlags_NoCollapse);
    ImGuiUtilRenderParade(wfRgbParade, ScaleBehavior::ScaleToFit, FlipBehavior::FlipVertically);
    ImGui::End();

    auto& wfRgbBlacks = sourceTextures->wfRGBBlacks;

    ImGui::SetNextWindowSizeConstraints(c_uiMinWFSize, c_uiMaxSize, WindowSizeConstraints::AspectWithOffset, (void*)&c_uiWFAspect);
    ImGui::Begin(wfRgbBlacks.description.data(), nullptr, ImGuiWindowFlags_NoCollapse);
    ImGuiUtilRenderBlacklevel(wfRgbBlacks, ScaleBehavior::ScaleToFit, FlipBehavior::FlipVertically);
    ImGui::End();

    auto& wfYuvParade = sourceTextures->wfYUVParade;

    ImGui::SetNextWindowSizeConstraints(c_uiMinWFSize, c_uiMaxSize, WindowSizeConstraints::AspectWithOffset, (void*)&c_uiWFAspect);
    ImGui::Begin(wfYuvParade.description.data(), nullptr, ImGuiWindowFlags_NoCollapse);
    ImGuiUtilRenderParade(wfYuvParade, ScaleBehavior::ScaleToFit, FlipBehavior::FlipVertically);
    ImGui::End();

    /// SCOPES

    auto& scUV = sourceTextures->scUV;
    ImGui::SetNextWindowSizeConstraints(c_uiMinSCSize, c_uiMaxSize, WindowSizeConstraints::SquareWithOffset, (void*)&c_uiSCWindowSizeOffset);
    ImGui::Begin(scUV.description.data(), nullptr, ImGuiWindowFlags_NoCollapse);
    ImGuiUtilRenderUV(scUV, ScaleBehavior::ScaleToFit);
    ImGui::End();

    auto& scXYZ = sourceTextures->scXYZ;
    ImGui::SetNextWindowSizeConstraints(c_uiMinSCSize, c_uiMaxSize, WindowSizeConstraints::SquareWithOffset, (void*)&c_uiSCWindowSizeOffset);
    ImGui::Begin(scXYZ.description.data(), nullptr, ImGuiWindowFlags_NoCollapse);
    ImGuiUtilRenderCIE(scXYZ, renderSettings.colorSpace, ScaleBehavior::ScaleToFit, FlipBehavior::FlipVertically);
    ImGui::End();

    auto& scDia = sourceTextures->scDia;
    ImGui::SetNextWindowSizeConstraints(c_uiMinSCSize, c_uiMaxSize, WindowSizeConstraints::SquareWithOffset, (void*)&c_uiSCWindowSizeOffset);
    ImGui::Begin(scDia.description.data(), nullptr, ImGuiWindowFlags_NoCollapse);
    ImGuiUtilRenderDia(scDia, ScaleBehavior::ScaleToFit);
    ImGui::End();
}

void Application::UI_SourceStats() noexcept {
    const auto& sourceStats = m_source->GetStats();
    ImGui::SetNextWindowSizeConstraints(ImVec2(200, 50), c_uiMaxSize);
    ImGui::Begin("Source Stats", nullptr, ImGuiWindowFlags_NoCollapse);
    ImGui::Text("Name: %s", m_source->GetName().data());
    ImGui::Text("Dimensions: %ux%u", sourceStats.sourceDims.width, sourceStats.sourceDims.height);
    ImGui::Text("FPS: %.2f", sourceStats.sourceFPS);
    ImGui::Text(std::format("Format: {}", sourceStats.sourceFormat).c_str());
    ImGui::Text("Max Render FPS: %.2f", sourceStats.maxRenderFPS);
    ImGui::Text("Render Duration: %.4f ms", sourceStats.renderDurationMS);
    ImGui::Text("Avg Max Render FPS: %.2f", sourceStats.avgMaxRenderFPS);
    ImGui::Text("Avg Render Duration: %.4f ms", sourceStats.avgRenderDurationMS);
    ImGui::End();
}

void Application::UI_RenderSettings() noexcept {
    auto& renderSettings = m_source->GetRenderSettings();

    ImGui::Begin("Render Settings", nullptr, ImGuiWindowFlags_NoCollapse);
    // Dropdown for color space
    if (ImGui::BeginCombo(
            "Color Space",
            SourceColorSpaceToString(renderSettings.colorSpace).data())) {
        for (int i = 0; i < static_cast<int>(SourceColorSpace::max); ++i) {
            const auto colorSpace = static_cast<SourceColorSpace>(i);
            if (ImGui::Selectable(SourceColorSpaceToString(colorSpace).data(), renderSettings.colorSpace == colorSpace)) {
                renderSettings.colorSpace = colorSpace;
            }
        }
        ImGui::EndCombo();
    }

    // Dropdown for YUV range
    if (ImGui::BeginCombo(
            "YUV Range",
            SourceYUVRangeToString(renderSettings.yuvRange).data())) {
        for (int i = 0; i < static_cast<int>(SourceYUVRange::max); ++i) {
            const auto yuvRange = static_cast<SourceYUVRange>(i);
            if (ImGui::Selectable(SourceYUVRangeToString(yuvRange).data(), renderSettings.yuvRange == yuvRange)) {
                renderSettings.yuvRange = yuvRange;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::End();
}

} // namespace scpp
