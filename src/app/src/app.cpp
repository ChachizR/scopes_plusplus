#include "app.hpp"
#include "runtime_paths.hpp"

namespace scpp {

Application::Application(std::optional<std::filesystem::path> initialVideoFile) {
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

    m_sourceProvider = std::make_unique<SourceProvider>();

    if (initialVideoFile) {
        const auto pathText = initialVideoFile->string();
        const auto copySize = (std::min)(pathText.size(), m_videoFilePath.size() - 1u);
        std::copy_n(pathText.data(), copySize, m_videoFilePath.data());
        m_videoFilePath[copySize] = '\0';

        StartVideoFileSource(*initialVideoFile);
    }
}

Application::~Application() {
    ShutdownImGui();
    ShutdownGLFW();
}

auto Application::InitGLFW() -> bool {
    glfwSetErrorCallback(GLFWErrorCallback);

#if defined(__linux__) && defined(GLFW_PLATFORM_X11)
    // Linux OpenCL/GL interop below creates the CL context from GLX handles.
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif

    if (!glfwInit()) {
        std::println("Failed to initialize GLFW");
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#endif

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

auto Application::StartVideoFileSource(const std::filesystem::path& path) noexcept -> bool {
    if (m_source) {
        m_source->Stop();
    }

    m_source = m_sourceProvider->CreateVideoFileSource(*m_openclDeviceProvider, path);
    if (!m_source) {
        std::println("Failed to create video file source");
        return false;
    }

    if (m_source->Start() != ErrorCode::None) {
        std::println("Failed to start video file source: {}", path.string());
        m_source.reset();
        return false;
    }

    return true;
}

void Application::QueueScopeLayout(ScopeLayoutPreset preset) noexcept {
    auto showOnly = [this](bool sourcePreview, bool falseColor, bool wfLuma, bool wfRgb, bool wfRgbParade, bool wfRgbBlacks, bool wfYuvParade, bool scUV, bool scXYZ, bool scDia) {
        m_showSourcePreview = sourcePreview;
        m_showFalseColor    = falseColor;
        m_showWFLuma        = wfLuma;
        m_showWFRgb         = wfRgb;
        m_showWFRgbParade   = wfRgbParade;
        m_showWFRgbBlacks   = wfRgbBlacks;
        m_showWFYuvParade   = wfYuvParade;
        m_showSCUV          = scUV;
        m_showSCXYZ         = scXYZ;
        m_showSCDia         = scDia;
    };

    m_pendingWindowPlacementCount = 0u;

    const auto* viewport = ImGui::GetMainViewport();
    const ImVec2 origin  = viewport->WorkPos;
    const ImVec2 extent  = viewport->WorkSize;

    constexpr float gap = 8.0f;

    auto place = [this](std::string_view name, float x, float y, float w, float h) {
        if (m_pendingWindowPlacementCount >= m_pendingWindowPlacements.size()) {
            return;
        }

        m_pendingWindowPlacements[m_pendingWindowPlacementCount++] = WindowPlacement{
            .name = name,
            .pos  = ImVec2(x, y),
            .size = ImVec2((std::max)(80.0f, w), (std::max)(80.0f, h))};
    };

    auto grid = [&](std::span<const std::string_view> names, int columns) {
        const int rows = static_cast<int>((names.size() + static_cast<size_t>(columns) - 1u) / static_cast<size_t>(columns));
        const float cellW = (extent.x - gap * static_cast<float>(columns + 1)) / static_cast<float>(columns);
        const float cellH = (extent.y - gap * static_cast<float>(rows + 1)) / static_cast<float>(rows);

        for (size_t index = 0; index < names.size(); ++index) {
            const int col = static_cast<int>(index % static_cast<size_t>(columns));
            const int row = static_cast<int>(index / static_cast<size_t>(columns));
            place(names[index],
                  origin.x + gap + static_cast<float>(col) * (cellW + gap),
                  origin.y + gap + static_cast<float>(row) * (cellH + gap),
                  cellW,
                  cellH);
        }
    };

    switch (preset) {
    case ScopeLayoutPreset::FourUpReview: {
        showOnly(true, true, true, false, true, false, false, false, false, false);

        constexpr auto names = std::to_array<std::string_view>({
            "Source Preview"sv,
            "False Color"sv,
            "Luminance Waveform"sv,
            "RGB Parade"sv,
        });
        grid(names, 2);
        break;
    }
    case ScopeLayoutPreset::SixUpQC: {
        showOnly(true, true, true, false, true, false, false, true, true, false);

        constexpr auto names = std::to_array<std::string_view>({
            "Source Preview"sv,
            "False Color"sv,
            "Luminance Waveform"sv,
            "RGB Parade"sv,
            "UV Vectorscope"sv,
            "CIE 1931 Chromaticity"sv,
        });
        grid(names, 3);
        break;
    }
    case ScopeLayoutPreset::AllScopesGrid: {
        showOnly(true, true, true, true, true, true, true, true, true, true);

        constexpr auto names = std::to_array<std::string_view>({
            "Source Preview"sv,
            "False Color"sv,
            "Luminance Waveform"sv,
            "RGB Waveform"sv,
            "RGB Parade"sv,
            "RGB Blacklevel"sv,
            "YUV Parade"sv,
            "UV Vectorscope"sv,
            "CIE 1931 Chromaticity"sv,
            "Double Diamond"sv,
        });
        grid(names, 4);
        break;
    }
    case ScopeLayoutPreset::WaveformColumns: {
        showOnly(true, false, true, true, true, true, true, false, false, false);

        const float previewW = extent.x * 0.42f - gap * 1.5f;
        const float waveW    = extent.x - previewW - gap * 3.0f;
        const float waveH    = (extent.y - gap * 6.0f) / 5.0f;

        place("Source Preview"sv, origin.x + gap, origin.y + gap, previewW, extent.y - gap * 2.0f);

        constexpr auto names = std::to_array<std::string_view>({
            "Luminance Waveform"sv,
            "RGB Waveform"sv,
            "RGB Parade"sv,
            "RGB Blacklevel"sv,
            "YUV Parade"sv,
        });

        for (size_t index = 0; index < names.size(); ++index) {
            place(names[index],
                  origin.x + previewW + gap * 2.0f,
                  origin.y + gap + static_cast<float>(index) * (waveH + gap),
                  waveW,
                  waveH);
        }
        break;
    }
    }
}

void Application::ApplyPendingWindowPlacement(std::string_view windowName) const noexcept {
    for (size_t index = 0; index < m_pendingWindowPlacementCount; ++index) {
        const auto& placement = m_pendingWindowPlacements[index];
        if (placement.name != windowName) {
            continue;
        }

        ImGui::SetNextWindowDockID(0, ImGuiCond_Always);
        ImGui::SetNextWindowPos(placement.pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(placement.size, ImGuiCond_Always);
        return;
    }
}

auto Application::LoadFonts() -> bool {
    ImGuiIO& io = ImGui::GetIO();

    const auto fontPath = ResolveRuntimePath("assets/fonts/Roboto-VariableFont_wdth,wght.ttf");
    m_fontRoboto        = io.Fonts->AddFontFromFileTTF(fontPath.string().c_str());

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

        if (m_source) {
            SyncRenderFeaturesFromUI();
            m_source->UpdateOnMainThread();
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (false)
            ImGui::ShowDemoWindow();

        UI_Main();

        if (m_showImGuiMetrics) {
            ImGui::ShowMetricsWindow(&m_showImGuiMetrics);
        }

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
    UI_Sources();

    if (m_source) {
        UI_ActiveSource();
        UI_SourceStats();
        UI_RenderSettings();
    }

    m_pendingWindowPlacementCount = 0u;
}

void Application::UI_MainMenuBar() noexcept {
    ImGui::BeginMainMenuBar();

    // File menu
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Exit")) {
            glfwSetWindowShouldClose(glfwGetCurrentContext(), true);
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Source Preview", nullptr, &m_showSourcePreview);
        ImGui::MenuItem("False Color", nullptr, &m_showFalseColor);
        ImGui::Separator();
        ImGui::MenuItem("Luminance Waveform", nullptr, &m_showWFLuma);
        ImGui::MenuItem("RGB Waveform", nullptr, &m_showWFRgb);
        ImGui::MenuItem("RGB Parade", nullptr, &m_showWFRgbParade);
        ImGui::MenuItem("RGB Blacklevel", nullptr, &m_showWFRgbBlacks);
        ImGui::MenuItem("YUV Parade", nullptr, &m_showWFYuvParade);
        ImGui::Separator();
        ImGui::MenuItem("UV Vectorscope", nullptr, &m_showSCUV);
        ImGui::MenuItem("CIE 1931 Chromaticity", nullptr, &m_showSCXYZ);
        ImGui::MenuItem("Double Diamond", nullptr, &m_showSCDia);
        ImGui::Separator();
        if (ImGui::BeginMenu("Layouts")) {
            if (ImGui::MenuItem("4-Up Review")) {
                QueueScopeLayout(ScopeLayoutPreset::FourUpReview);
            }
            if (ImGui::MenuItem("6-Up QC")) {
                QueueScopeLayout(ScopeLayoutPreset::SixUpQC);
            }
            if (ImGui::MenuItem("All Scopes Grid")) {
                QueueScopeLayout(ScopeLayoutPreset::AllScopesGrid);
            }
            if (ImGui::MenuItem("Waveform Columns")) {
                QueueScopeLayout(ScopeLayoutPreset::WaveformColumns);
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        ImGui::MenuItem("ImGui Metrics", nullptr, &m_showImGuiMetrics);
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void Application::SyncRenderFeaturesFromUI() noexcept {
    if (!m_source) {
        return;
    }

    auto& renderSettings = m_source->GetRenderSettings();
    renderSettings.enabledFeatures = RenderFeature::None;

    SetRenderFeatureFlag(renderSettings.enabledFeatures, RenderFeature::FalseColor, m_showFalseColor);
    SetRenderFeatureFlag(renderSettings.enabledFeatures, RenderFeature::WFLuma, m_showWFLuma);
    SetRenderFeatureFlag(renderSettings.enabledFeatures, RenderFeature::WFRgb, m_showWFRgb);
    SetRenderFeatureFlag(renderSettings.enabledFeatures, RenderFeature::WFRgbParade, m_showWFRgbParade);
    SetRenderFeatureFlag(renderSettings.enabledFeatures, RenderFeature::WFRgbBlacks, m_showWFRgbBlacks);
    SetRenderFeatureFlag(renderSettings.enabledFeatures, RenderFeature::WFYuvParade, m_showWFYuvParade);
    SetRenderFeatureFlag(renderSettings.enabledFeatures, RenderFeature::SCUV, m_showSCUV);
    SetRenderFeatureFlag(renderSettings.enabledFeatures, RenderFeature::SCXYZ, m_showSCXYZ);
    SetRenderFeatureFlag(renderSettings.enabledFeatures, RenderFeature::SCDia, m_showSCDia);
}

void Application::UI_Settings() const noexcept {
    ApplyPendingWindowPlacement("Settings"sv);
    ImGui::Begin("Settings");

    ImGui::End();
}

void Application::UI_Sources() noexcept {
    const auto sources = m_sourceProvider->GetSources();

    ApplyPendingWindowPlacement("Sources"sv);
    ImGui::Begin("Sources");

    ImGui::SeparatorText("File / SRT URL");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##video-file-path", m_videoFilePath.data(), m_videoFilePath.size());

    if (ImGui::Button("Open Source")) {
        StartVideoFileSource(m_videoFilePath.data());
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Built-in");

    if (ImGui::BeginTable("Sources Table", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Select", ImGuiTableColumnFlags_::ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Details");
        ImGui::TableHeadersRow();
        for (const auto& source : sources) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Button(std::format("Select##{}", source.name).c_str())) {
                if (m_source) {
                    m_source->Stop();
                }
                m_source = m_sourceProvider->CreateSource(*m_openclDeviceProvider, source);
                if (!m_source) {
                    std::println("Failed to create source: {}", source.name);
                    continue;
                }
                if (m_source->Start() != ErrorCode::None) {
                    std::println("Failed to start source: {}", m_source->GetName());
                    m_source.reset();
                }
            }
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(source.name.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(source.details.c_str());
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

void Application::UI_SourceStats() noexcept {
    const auto& sourceStats = m_source->GetStats();
    ImGui::SetNextWindowSizeConstraints(ImVec2(200, 50), c_uiMaxSize);
    ApplyPendingWindowPlacement("Source Stats"sv);
    ImGui::Begin("Source Stats", nullptr, ImGuiWindowFlags_NoCollapse);
    ImGui::Text("Name: %s", m_source->GetName().data());
    ImGui::Text("Dimensions: %ux%u", sourceStats.sourceDims.width, sourceStats.sourceDims.height);
    ImGui::Text("FPS: %.2f", sourceStats.sourceFPS);
    const auto formatLabel = std::format("Format: {}", sourceStats.sourceFormat);
    ImGui::Text("%s", formatLabel.c_str());
    ImGui::Text("Max Render FPS: %.2f", sourceStats.maxRenderFPS);
    ImGui::Text("Render Duration: %.4f ms", sourceStats.renderDurationMS);
    ImGui::Text("Avg Max Render FPS: %.2f", sourceStats.avgMaxRenderFPS);
    ImGui::Text("Avg Render Duration: %.4f ms", sourceStats.avgRenderDurationMS);
    ImGui::SeparatorText("Decode");
    ImGui::Text("sws_scale: %.4f ms avg %.4f", sourceStats.decodeConvertMS, sourceStats.avgDecodeConvertMS);
    ImGui::Text("Frame copy: %.4f ms avg %.4f", sourceStats.decodeCopyMS, sourceStats.avgDecodeCopyMS);
    ImGui::Text("Decoded Frames: %llu", static_cast<unsigned long long>(sourceStats.decodedFrameCount));
    ImGui::Text("Rendered Frames: %llu", static_cast<unsigned long long>(sourceStats.renderedFrameCount));
    ImGui::Text("Dropped Frames: %llu", static_cast<unsigned long long>(sourceStats.droppedFrameCount));
    ImGui::Text("Queued Frames: %u", sourceStats.queuedFrameCount);

    const auto& sourceRenderer = m_source->GetRenderer();
    ImGui::Text("Submitted Frames: %llu", static_cast<unsigned long long>(sourceRenderer.GetSubmittedFrameCount()));
    const auto pipelineStatus = sourceRenderer.GetLastPipelineStatus();
    ImGui::TextWrapped("Pipeline: %.*s", static_cast<int>(pipelineStatus.size()), pipelineStatus.data());
    const auto& clTiming = sourceRenderer.GetLastTimingStats();
    ImGui::SeparatorText("OpenCL");
    ImGui::Text("GPU commands: %.4f ms", clTiming.totalGPUCommandMS);
    ImGui::Text("Upload: %.4f ms", clTiming.uploadMS);
    ImGui::Text("Reset: %.4f ms", clTiming.resetMS);
    ImGui::Text("Acquire GL: %.4f ms", clTiming.acquireGLMS);
    ImGui::Text("Convert: %.4f ms", clTiming.convertMS);
    ImGui::Text("Waveforms: %.4f ms", clTiming.waveformMS());
    ImGui::Text("  Accumulate: %.4f ms", clTiming.waveformAccumMS);
    ImGui::Text("  Image: %.4f ms", clTiming.waveformImageMS);
    ImGui::Text("Scopes: %.4f ms", clTiming.scopeMS());
    ImGui::Text("  UV: %.4f ms", clTiming.scopeUVMS);
    ImGui::Text("  XYZ: %.4f ms", clTiming.scopeXYZMS);
    ImGui::Text("  Diamond: %.4f ms", clTiming.scopeDiaMS);
    ImGui::Text("  Image: %.4f ms", clTiming.scopeImageMS);
    ImGui::Text("False Color: %.4f ms", clTiming.falseColorMS);
    ImGui::Text("Release GL: %.4f ms", clTiming.releaseGLMS);
    ImGui::Text("CPU wait at finish: %.4f ms", clTiming.finishWaitMS);
    ImGui::End();
}

void Application::UI_RenderSettings() noexcept {
    auto& renderSettings = m_source->GetRenderSettings();

    ApplyPendingWindowPlacement("Render Settings"sv);
    ImGui::Begin("Render Settings", nullptr, ImGuiWindowFlags_NoCollapse);
    ImGui::SeparatorText("Source");

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

    ImGui::SeparatorText("Performance");
    const auto currentIntervalLabel = std::format("Every {} frame{}", renderSettings.analysisFrameInterval, renderSettings.analysisFrameInterval == 1u ? "" : "s");
    if (ImGui::BeginCombo("Scope Refresh", currentIntervalLabel.c_str())) {
        constexpr auto intervals = std::to_array<uint32_t>({1u, 2u, 4u, 8u, 16u});
        for (const auto interval : intervals) {
            const auto label = std::format("Every {} frame{}", interval, interval == 1u ? "" : "s");
            if (ImGui::Selectable(label.c_str(), renderSettings.analysisFrameInterval == interval)) {
                renderSettings.analysisFrameInterval = interval;
            }
        }
        ImGui::EndCombo();
    }

    const auto currentResolutionLabel = std::format("1/{}", renderSettings.analysisResolutionDivisor);
    if (ImGui::BeginCombo("Analysis Resolution", renderSettings.analysisResolutionDivisor == 1u ? "Full" : currentResolutionLabel.c_str())) {
        constexpr auto divisors = std::to_array<uint32_t>({1u, 2u, 4u});
        for (const auto divisor : divisors) {
            const auto label = divisor == 1u ? std::string{"Full"} : std::format("1/{}", divisor);
            if (ImGui::Selectable(label.c_str(), renderSettings.analysisResolutionDivisor == divisor)) {
                renderSettings.analysisResolutionDivisor = divisor;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::TextWrapped("Source preview stays live; analytical scopes can update less often and sample fewer pixels.");

    ImGui::SeparatorText("False Color");

    auto& sourceRenderer = m_source->GetRenderer();

    if (ImGui::BeginCombo("False Color Map", sourceRenderer.GetSelectedFalseColorMapName().data())) {

        for (const auto [name, map] : c_falseColorMapsSpan) {
            if (ImGui::Selectable(name.data(), sourceRenderer.GetSelectedFalseColorMapName() == name)) {
                sourceRenderer.SetFalseColorMap(map, name);
            }
        }

        ImGui::EndCombo();
    }

    ImGui::End();
}

void Application::UI_SourcePreview(const scpp::TargetTextures* sourceTextures) noexcept {
    if (!m_showSourcePreview) {
        return;
    }

    const auto& sourceStats = m_source->GetStats();

    const auto sourceAspect = WindowAspectData{
        .targetAspectRatio = static_cast<float>(sourceStats.sourceDims.width) / static_cast<float>(sourceStats.sourceDims.height),
        .offset            = ImVec2(0.f, 32.f) // Account for title bar height
    };

    auto& sourcePreview = sourceTextures->sourcePreview;

    ImGui::SetNextWindowSizeConstraints(ImVec2(160, 32 + 90), c_uiMaxSize, WindowSizeConstraints::AspectWithOffset, (void*)&sourceAspect);
    ApplyPendingWindowPlacement(sourcePreview.description);
    if (!ImGui::Begin(sourcePreview.description.data(), &m_showSourcePreview, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }
    ImGuiUtilImageRender(sourcePreview, ScaleBehavior::ScaleToFit);
    ImGui::End();
}

void Application::UI_FalseColor(const scpp::TargetTextures* sourceTextures) noexcept {
    if (!m_showFalseColor) {
        return;
    }

    // --- Constants for layout ---
    constexpr float kScaleTotalWidth = 95.0f; // total width for the scale column (labels + ticks + color band)
    constexpr float kBandWidth       = 40.0f; // width of the colored bar itself (right-aligned inside the scale column)
    constexpr float kLabelGap        = 6.0f;  // gap between labels/ticks area and the color band
    constexpr float kLabelPadRight   = 4.0f;
    constexpr float kInnerSpacing    = 4.0f; // spacing between image child and scale child

    // Tick lengths
    constexpr float kTickThinLen  = 6.0f;
    constexpr float kTickMidLen   = 10.0f;
    constexpr float kTickThickLen = 14.0f;

    const auto& falseColorTex    = sourceTextures->falseColor;
    const auto& sourcePreviewTex = sourceTextures->sourcePreview;
    auto&       renderSettings   = m_source->GetRenderSettings();
    const bool  useLimited       = (renderSettings.yuvRange == SourceYUVRange::Limited);
    auto&       sourceRenderer   = m_source->GetRenderer();
    const auto& fcMap            = sourceRenderer.GetFalseColorMap();
    const auto& fcData           = useLimited ? fcMap.GetDataLimitedRange() : fcMap.GetDataFullRange();

    const auto& sourceStats  = m_source->GetStats();
    const auto  sourceAspect = WindowAspectData{
         .targetAspectRatio = static_cast<float>(sourceStats.sourceDims.width) / static_cast<float>(sourceStats.sourceDims.height),
         .offset            = ImVec2(kScaleTotalWidth + kInnerSpacing, 32.f) // Account for title bar height and width of scale
    };

    ImGui::SetNextWindowSizeConstraints(ImVec2(160 + kScaleTotalWidth, 32 + 90), c_uiMaxSize, WindowSizeConstraints::AspectWithOffset, (void*)&sourceAspect);
    ApplyPendingWindowPlacement(falseColorTex.description);

    auto falseColorVisible = ImGui::Begin(falseColorTex.description.data(), &m_showFalseColor, ImGuiWindowFlags_NoCollapse);

    if (falseColorVisible) {

        ImVec2      avail            = ImGui::GetContentRegionAvail();
        const float scaleColumnWidth = kScaleTotalWidth;
        const float imageColumnWidth = (std::max)(0.0f, avail.x - scaleColumnWidth - kInnerSpacing);

        // -----------------------
        // LEFT: image area child
        // -----------------------
        ImGui::BeginChild("FalseColorImageChild", ImVec2(imageColumnWidth, avail.y), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        const auto   imgSize = falseColorTex.size.ToImVec2();
        const auto   scale   = ImGuiUtilGetContentScale(imgSize, ScaleBehavior::ScaleToFit);
        const ImVec2 dispSize(imgSize.x * scale, imgSize.y * scale);

        const auto topLeft = ImGui::GetCursorScreenPos();

        const auto [uv0, uv1]        = ImGuiUtilGetUVs(FlipBehavior::DoNotFlip);
        const auto imTextureIDSource = static_cast<ImTextureID>(sourcePreviewTex.glTextureID);
        ImGui::ImageWithBg(imTextureIDSource, dispSize, uv0, uv1, c_bgColor);

        ImGui::SetCursorScreenPos(topLeft);

        const auto imTextureIDFC = static_cast<ImTextureID>(falseColorTex.glTextureID);
        ImGui::ImageWithBg(imTextureIDFC, dispSize, uv0, uv1, ImVec4(0.f, 0.f, 0.f, 0.f));

        ImGui::EndChild();

        // -----------------------
        // RIGHT: scale column
        // -----------------------
        ImGui::SameLine(0.0f, kInnerSpacing);
        ImGui::BeginChild("FalseColorScaleChild", ImVec2(scaleColumnWidth, avail.y), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        {
            ImDrawList*  dl   = ImGui::GetWindowDrawList();
            const ImVec2 base = ImGui::GetCursorScreenPos();

            const float bandLeft    = base.x;
            const float bandRight   = bandLeft + kBandWidth;
            const float ticksLeft   = bandRight + kLabelGap;
            const float labelsRight = base.x + scaleColumnWidth;

            const float bandTop    = base.y + 16.f;
            const float bandBottom = bandTop + dispSize.y - 32.f;

            // ---- Draw the colored rectangles (0..255) ----
            // We draw top bottom with y mapped so that index 255 is at the top (to match the Flutter mapping where 0% sits at bottom)
            // If you prefer 0 at top, flip the mapping.
            for (int v = 0; v < 256; ++v) {
                const float y0 = mapRange(static_cast<float>(v), 0.f, 255.f, bandBottom, bandTop);
                const float y1 = mapRange(static_cast<float>(v) + 1.f, 0.f, 255.f, bandBottom, bandTop);

                const glm::u8vec4 c   = fcData[v];
                const ImU32       col = IM_COL32(c.r, c.g, c.b, c.a);

                dl->AddRectFilled(ImVec2(bandLeft, y1), ImVec2(bandRight, y0), col);
            }

            // ---- Ticks + labels (0..100%), mapped by YUV range ----

            // Mapping range for percentages:
            //   Full   : 0% at luma 0,   100% at luma 255
            //   Limited: 0% at luma 16,  100% at luma 235
            const float luma0   = useLimited ? 16.f : 0.f;
            const float luma100 = useLimited ? 235.f : 255.f;

            constexpr static auto tickCol   = IM_COL32(255, 255, 255, 217);
            constexpr static auto tickCol5  = IM_COL32(255, 255, 255, 235);
            constexpr static auto tickCol10 = IM_COL32(255, 255, 255, 255);

            const ImU32 textCol = ImGui::GetColorU32(ImGui::GetStyle().Colors[ImGuiCol_Text]);

            ImFont*     font     = ImGui::GetFont();
            const float fontSize = ImGui::GetFontSize();

            // Minor ticks every 1%, medium every 5%, thick every 10%, labels only at 10%
            for (int p = 0; p <= 100; ++p) {
                const bool is10 = (p % 10) == 0;
                const bool is5  = (p % 5) == 0;

                const float luma = mapRange(static_cast<float>(p), 0.f, 100.f, luma0, luma100);
                const float y    = mapRange(luma, 0.f, 255.f, bandBottom, bandTop);

                const auto [len, col] = [&]() -> std::pair<float, ImU32> {
                    if (is10)
                        return {kTickThickLen, tickCol10};
                    if (is5)
                        return {kTickMidLen, tickCol5};
                    return {kTickThinLen, tickCol};
                }();

                dl->AddLine(ImVec2(ticksLeft, y), ImVec2(ticksLeft + len, y), col, 1.0f);

                // Label every 10%
                if (is10) {
                    std::string buf = std::format("{}", p);

                    ImVec2      textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, buf.c_str());
                    const float tx       = labelsRight - kLabelPadRight - textSize.x;
                    const float ty       = y - textSize.y * 0.5f;
                    dl->AddText(ImVec2(tx, ty), textCol, buf.c_str());
                }
            }
        }

        ImGui::EndChild(); // scale
    }
    ImGui::End(); // window
    (void)falseColorVisible;
}

void Application::UI_Waveforms(const scpp::TargetTextures* sourceTextures) noexcept {
    auto& renderSettings = m_source->GetRenderSettings();

    if (m_showWFLuma) {
        auto& wfLuma = sourceTextures->wfLuma;
        ImGui::SetNextWindowSizeConstraints(c_uiMinWFSize, c_uiMaxSize, WindowSizeConstraints::AspectWithOffset, (void*)&c_uiWFAspect);
        ApplyPendingWindowPlacement(wfLuma.description);
        const auto wfLumaVisible = ImGui::Begin(wfLuma.description.data(), &m_showWFLuma, ImGuiWindowFlags_NoCollapse);
        if (wfLumaVisible)
            ImGuiUtilRenderLumaWF(wfLuma, renderSettings.yuvRange, ScaleBehavior::ScaleToFit, FlipBehavior::FlipVertically);
        ImGui::End();
    }

    if (m_showWFRgb) {
        auto& wfRgb = sourceTextures->wfRGB;
        ImGui::SetNextWindowSizeConstraints(c_uiMinWFSize, c_uiMaxSize, WindowSizeConstraints::AspectWithOffset, (void*)&c_uiWFAspect);
        ApplyPendingWindowPlacement(wfRgb.description);
        const auto wfRgbVisible = ImGui::Begin(wfRgb.description.data(), &m_showWFRgb, ImGuiWindowFlags_NoCollapse);
        if (wfRgbVisible)
            ImGuiUtilRenderRGBWF(wfRgb, ScaleBehavior::ScaleToFit, FlipBehavior::FlipVertically);
        ImGui::End();
    }

    if (m_showWFRgbParade) {
        auto& wfRgbParade = sourceTextures->wfRGBParade;
        ImGui::SetNextWindowSizeConstraints(c_uiMinWFSize, c_uiMaxSize, WindowSizeConstraints::AspectWithOffset, (void*)&c_uiWFAspect);
        ApplyPendingWindowPlacement(wfRgbParade.description);
        const auto wfRgbParadeVisible = ImGui::Begin(wfRgbParade.description.data(), &m_showWFRgbParade, ImGuiWindowFlags_NoCollapse);
        if (wfRgbParadeVisible)
            ImGuiUtilRenderParade(wfRgbParade, ScaleBehavior::ScaleToFit, FlipBehavior::FlipVertically);
        ImGui::End();
    }

    if (m_showWFRgbBlacks) {
        auto& wfRgbBlacks = sourceTextures->wfRGBBlacks;
        ImGui::SetNextWindowSizeConstraints(c_uiMinWFSize, c_uiMaxSize, WindowSizeConstraints::AspectWithOffset, (void*)&c_uiWFAspect);
        ApplyPendingWindowPlacement(wfRgbBlacks.description);
        const auto wfRgbBlacksVisible = ImGui::Begin(wfRgbBlacks.description.data(), &m_showWFRgbBlacks, ImGuiWindowFlags_NoCollapse);
        if (wfRgbBlacksVisible)
            ImGuiUtilRenderBlacklevel(wfRgbBlacks, ScaleBehavior::ScaleToFit, FlipBehavior::FlipVertically);
        ImGui::End();
    }

    if (m_showWFYuvParade) {
        auto& wfYuvParade = sourceTextures->wfYUVParade;
        ImGui::SetNextWindowSizeConstraints(c_uiMinWFSize, c_uiMaxSize, WindowSizeConstraints::AspectWithOffset, (void*)&c_uiWFAspect);
        ApplyPendingWindowPlacement(wfYuvParade.description);
        const auto wfYuvParadeVisible = ImGui::Begin(wfYuvParade.description.data(), &m_showWFYuvParade, ImGuiWindowFlags_NoCollapse);
        if (wfYuvParadeVisible)
            ImGuiUtilRenderParade(wfYuvParade, ScaleBehavior::ScaleToFit, FlipBehavior::FlipVertically);
        ImGui::End();
    }
}

void Application::UI_Scopes(const scpp::TargetTextures* sourceTextures) noexcept {
    auto& renderSettings = m_source->GetRenderSettings();

    if (m_showSCUV) {
        auto& scUV = sourceTextures->scUV;
        ImGui::SetNextWindowSizeConstraints(c_uiMinSCSize, c_uiMaxSize, WindowSizeConstraints::SquareWithOffset, (void*)&c_uiSCWindowSizeOffset);
        ApplyPendingWindowPlacement(scUV.description);
        auto scUVVisible = ImGui::Begin(scUV.description.data(), &m_showSCUV, ImGuiWindowFlags_NoCollapse);
        if (scUVVisible)
            ImGuiUtilRenderUV(scUV, ScaleBehavior::ScaleToFit);
        ImGui::End();
    }

    if (m_showSCXYZ) {
        auto& scXYZ = sourceTextures->scXYZ;
        ImGui::SetNextWindowSizeConstraints(c_uiMinSCSize, c_uiMaxSize, WindowSizeConstraints::SquareWithOffset, (void*)&c_uiSCWindowSizeOffset);
        ApplyPendingWindowPlacement(scXYZ.description);
        auto scXYZVisible = ImGui::Begin(scXYZ.description.data(), &m_showSCXYZ, ImGuiWindowFlags_NoCollapse);
        if (scXYZVisible)
            ImGuiUtilRenderCIE(scXYZ, renderSettings.colorSpace, ScaleBehavior::ScaleToFit, FlipBehavior::FlipVertically);
        ImGui::End();
    }

    if (m_showSCDia) {
        auto& scDia = sourceTextures->scDia;
        ImGui::SetNextWindowSizeConstraints(c_uiMinSCSize, c_uiMaxSize, WindowSizeConstraints::SquareWithOffset, (void*)&c_uiSCWindowSizeOffset);
        ApplyPendingWindowPlacement(scDia.description);
        auto scDiaVisible = ImGui::Begin(scDia.description.data(), &m_showSCDia, ImGuiWindowFlags_NoCollapse);
        if (scDiaVisible)
            ImGuiUtilRenderDia(scDia, ScaleBehavior::ScaleToFit);
        ImGui::End();
    }
}

void Application::UI_ActiveSource() noexcept {
    auto& sourceRenderer = m_source->GetRenderer();

    if (sourceRenderer.needsResizeFlag_mainThread) {
        sourceRenderer.ResizeGLTextures();
    }

    auto sourceTextures = sourceRenderer.GetTargetTextures();

    UI_SourcePreview(sourceTextures);
    UI_FalseColor(sourceTextures);
    UI_Waveforms(sourceTextures);
    UI_Scopes(sourceTextures);
}

} // namespace scpp
