#include "app.hpp"
#include "runtime_paths.hpp"

namespace scpp {

namespace {

constexpr auto c_layoutIniMarker = "[ImGuiIni]\n"sv;

[[nodiscard]]
auto ParseBoolSetting(std::string_view line, std::string_view key, bool& value) noexcept -> bool {
    if (!line.starts_with(key) || line.size() <= key.size() || line[key.size()] != '=') {
        return false;
    }

    const auto boolText = line.substr(key.size() + 1u);
    value = boolText == "1"sv || boolText == "true"sv || boolText == "True"sv;
    return true;
}

} // namespace

Application::Application(std::optional<std::filesystem::path> initialVideoFile) {
#if SCPP_USE_VULKAN_UI
    std::println("Starting Scopes++ with Vulkan UI preview backend");
#else
    std::println("Starting Scopes++ with legacy OpenGL/OpenCL backend");
#endif

    if (!InitGLFW()) {
        std::println("Failed to initialize GLFW");
        throw std::runtime_error("GLFW initialization failed");
    }

#if SCPP_USE_VULKAN_UI
    m_vulkanRenderer = std::make_unique<VulkanRenderer>(m_window);
    if (!m_vulkanRenderer->IsInitialized()) {
        const auto error = std::format("Vulkan renderer initialization failed: {}", m_vulkanRenderer->GetLastError());
        std::println("{}", error);
        ShutdownGLFW();
        throw std::runtime_error(error);
    }
#endif

    if (!InitImGui()) {
        std::println("Failed to initialize ImGui");
        m_vulkanRenderer.reset();
        ShutdownGLFW();
        throw std::runtime_error("ImGui initialization failed");
    }

    SetImGuiStyle();

#if !SCPP_USE_VULKAN_UI
    m_vulkanRenderer = std::make_unique<VulkanRenderer>();
    if (!m_vulkanRenderer->IsInitialized()) {
        std::println("Vulkan renderer skeleton unavailable: {}", m_vulkanRenderer->GetLastError());
    }

    m_openclDeviceProvider = std::make_unique<OpenCLDeviceProvider>();

    if (!m_openclDeviceProvider->IsInitialized()) {
        std::println("Failed to initialize OpenCL device provider");
        throw std::runtime_error("OpenCL device provider initialization failed");
    }
#else
    std::println("OpenCL/OpenGL scopes are disabled while SCPP_USE_VULKAN_UI is enabled");
#endif

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
    m_vulkanRenderer.reset();
    ShutdownGLFW();
}

auto Application::InitGLFW() -> bool {
    glfwSetErrorCallback(GLFWErrorCallback);

    if (!glfwInit()) {
        std::println("Failed to initialize GLFW");
        return false;
    }

#if SCPP_USE_VULKAN_UI
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#endif
#endif

    m_mainScale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());

    m_window = glfwCreateWindow((int32_t)(1280 * m_mainScale), (int32_t)(720 * m_mainScale), "Scopes++", nullptr, nullptr);

    if (!m_window) {
        std::println("Failed to create GLFW window");
        glfwTerminate();
        return false;
    }

#if !SCPP_USE_VULKAN_UI
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);
#endif
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
#if !SCPP_USE_VULKAN_UI
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-Viewport / Platform Windows
#endif
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
#if SCPP_USE_VULKAN_UI
    if (!m_vulkanRenderer || !m_vulkanRenderer->InitImGuiBackend()) {
        return false;
    }
#else
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init(m_glslVersion.data());
#endif

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

    const auto fontPath = ResolveRuntimePath("assets/fonts/Roboto-VariableFont_wdth,wght.ttf");
    m_fontRoboto        = io.Fonts->AddFontFromFileTTF(fontPath.string().c_str());

    if (!m_fontRoboto)
        return false;

    return true;
}

auto Application::StartVideoFileSource(const std::filesystem::path& path) noexcept -> bool {
    if (m_source) {
        m_source->Stop();
    }

#if SCPP_USE_VULKAN_UI
    m_source = m_sourceProvider->CreateVideoFileSource(path);
#else
    m_source = m_sourceProvider->CreateVideoFileSource(*m_openclDeviceProvider, path);
#endif
    if (!m_source) {
        std::println("Failed to create video/SRT source");
        return false;
    }

    if (m_source->Start() != ErrorCode::None) {
        std::println("Failed to start video/SRT source: {}", path.string());
        m_source.reset();
        return false;
    }

    return true;
}

auto Application::LayoutPresetPath(uint32_t presetIndex) const -> std::filesystem::path {
    return GetRuntimeBasePath() / "layouts" / std::format("layout_p{}.ini", presetIndex);
}

auto Application::SaveLayoutPreset(uint32_t presetIndex) const noexcept -> bool {
    if (presetIndex < 1u || presetIndex > 3u) {
        return false;
    }

    std::error_code ec;
    const auto presetPath = LayoutPresetPath(presetIndex);
    std::filesystem::create_directories(presetPath.parent_path(), ec);
    if (ec) {
        std::println("Failed to create layout preset directory '{}': {}", presetPath.parent_path().string(), ec.message());
        return false;
    }

    size_t imguiIniSize = 0u;
    const char* imguiIni = ImGui::SaveIniSettingsToMemory(&imguiIniSize);
    if (imguiIni == nullptr) {
        std::println("Failed to read ImGui layout data for preset P{}", presetIndex);
        return false;
    }

    std::ofstream out{presetPath, std::ios::binary | std::ios::trunc};
    if (!out) {
        std::println("Failed to open layout preset '{}' for writing", presetPath.string());
        return false;
    }

    out << "# Scopes++ layout preset v1\n";
    out << "[ScopesPlusPlus][View]\n";
    out << "SourcePreview=" << (m_showSourcePreview ? 1 : 0) << '\n';
    out << "FalseColor=" << (m_showFalseColor ? 1 : 0) << '\n';
    out << "WFLuma=" << (m_showWFLuma ? 1 : 0) << '\n';
    out << "WFRgb=" << (m_showWFRgb ? 1 : 0) << '\n';
    out << "WFRgbParade=" << (m_showWFRgbParade ? 1 : 0) << '\n';
    out << "WFRgbBlacks=" << (m_showWFRgbBlacks ? 1 : 0) << '\n';
    out << "WFYuvParade=" << (m_showWFYuvParade ? 1 : 0) << '\n';
    out << "SCUV=" << (m_showSCUV ? 1 : 0) << '\n';
    out << "SCXYZ=" << (m_showSCXYZ ? 1 : 0) << '\n';
    out << "SCDia=" << (m_showSCDia ? 1 : 0) << "\n\n";
    out << c_layoutIniMarker;
    out.write(imguiIni, static_cast<std::streamsize>(imguiIniSize));

    if (!out) {
        std::println("Failed while writing layout preset '{}'", presetPath.string());
        return false;
    }

    std::println("Saved layout preset P{} to '{}'", presetIndex, presetPath.string());
    return true;
}

auto Application::LoadLayoutPreset(uint32_t presetIndex) noexcept -> bool {
    if (presetIndex < 1u || presetIndex > 3u) {
        return false;
    }

    const auto presetPath = LayoutPresetPath(presetIndex);
    std::ifstream in{presetPath, std::ios::binary};
    if (!in) {
        std::println("Layout preset P{} does not exist at '{}'", presetIndex, presetPath.string());
        return false;
    }

    const std::string contents{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    const auto markerOffset = contents.find(c_layoutIniMarker);
    if (markerOffset == std::string::npos) {
        std::println("Layout preset P{} is invalid: missing ImGui layout data", presetIndex);
        return false;
    }

    const auto viewSettings = std::string_view{contents}.substr(0u, markerOffset);
    size_t lineStart = 0u;
    while (lineStart < viewSettings.size()) {
        const auto lineEnd = viewSettings.find('\n', lineStart);
        const auto line = viewSettings.substr(lineStart, lineEnd == std::string_view::npos ? std::string_view::npos : lineEnd - lineStart);

        ParseBoolSetting(line, "SourcePreview"sv, m_showSourcePreview) ||
            ParseBoolSetting(line, "FalseColor"sv, m_showFalseColor) ||
            ParseBoolSetting(line, "WFLuma"sv, m_showWFLuma) ||
            ParseBoolSetting(line, "WFRgb"sv, m_showWFRgb) ||
            ParseBoolSetting(line, "WFRgbParade"sv, m_showWFRgbParade) ||
            ParseBoolSetting(line, "WFRgbBlacks"sv, m_showWFRgbBlacks) ||
            ParseBoolSetting(line, "WFYuvParade"sv, m_showWFYuvParade) ||
            ParseBoolSetting(line, "SCUV"sv, m_showSCUV) ||
            ParseBoolSetting(line, "SCXYZ"sv, m_showSCXYZ) ||
            ParseBoolSetting(line, "SCDia"sv, m_showSCDia);

        if (lineEnd == std::string_view::npos) {
            break;
        }
        lineStart = lineEnd + 1u;
    }

    const auto imguiIniOffset = markerOffset + c_layoutIniMarker.size();
    const auto imguiIni = contents.substr(imguiIniOffset);
    ImGui::LoadIniSettingsFromMemory(imguiIni.data(), imguiIni.size());

    std::println("Loaded layout preset P{} from '{}'", presetIndex, presetPath.string());
    return true;
}

void Application::ShutdownImGui() {
#if SCPP_USE_VULKAN_UI
    if (m_vulkanRenderer) {
        m_vulkanRenderer->ShutdownImGuiBackend();
    }
#else
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
#endif
    ImGui::DestroyContext();
}

void Application::ShutdownGLFW() {
    if (m_window != nullptr) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
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
#if SCPP_USE_VULKAN_UI
            if (auto frame = m_source->GetSourcePreviewFrame()) {
                [[maybe_unused]] const auto uploaded = m_vulkanRenderer->UploadSourcePreview(*frame);
                if (m_showFalseColor) {
                    [[maybe_unused]] const auto falseColorRendered = m_vulkanRenderer->RenderFalseColor(m_source->GetRenderSettings());
                }
                if (m_showWFLuma) {
                    [[maybe_unused]] const auto lumaWaveformRendered = m_vulkanRenderer->RenderLumaWaveform(m_source->GetRenderSettings());
                }
                if (m_showWFRgb) {
                    [[maybe_unused]] const auto rgbWaveformRendered = m_vulkanRenderer->RenderRgbWaveform(m_source->GetRenderSettings());
                }
                if (m_showWFRgbParade) {
                    [[maybe_unused]] const auto rgbParadeRendered = m_vulkanRenderer->RenderRgbParade(m_source->GetRenderSettings());
                }
                if (m_showWFRgbBlacks) {
                    [[maybe_unused]] const auto rgbBlacklevelRendered = m_vulkanRenderer->RenderRgbBlacklevel(m_source->GetRenderSettings());
                }
                if (m_showWFYuvParade) {
                    [[maybe_unused]] const auto yuvParadeRendered = m_vulkanRenderer->RenderYuvParade(m_source->GetRenderSettings());
                }
                if (m_showSCUV) {
                    [[maybe_unused]] const auto uvScopeRendered = m_vulkanRenderer->RenderUvScope(m_source->GetRenderSettings());
                }
                if (m_showSCXYZ) {
                    [[maybe_unused]] const auto xyzScopeRendered = m_vulkanRenderer->RenderXyzScope(m_source->GetRenderSettings());
                }
                if (m_showSCDia) {
                    [[maybe_unused]] const auto diamondScopeRendered = m_vulkanRenderer->RenderDiamondScope(m_source->GetRenderSettings());
                }
            }
#endif
        }

#if SCPP_USE_VULKAN_UI
        m_vulkanRenderer->NewFrame();
#else
        ImGui_ImplOpenGL3_NewFrame();
#endif
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (false)
            ImGui::ShowDemoWindow();

        UI_Main();

        ImGui::ShowMetricsWindow();

        // render stuff

        ImGui::Render();
#if SCPP_USE_VULKAN_UI
        m_vulkanRenderer->RenderFrame(m_clearColor);
#else
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
#endif
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
#if SCPP_USE_VULKAN_UI
        UI_VulkanSourcePreview();
        UI_VulkanFalseColor();
        UI_VulkanLumaWaveform();
        UI_VulkanRgbWaveform();
        UI_VulkanRgbParade();
        UI_VulkanRgbBlacklevel();
        UI_VulkanYuvParade();
        UI_VulkanUvScope();
        UI_VulkanXyzScope();
        UI_VulkanDiamondScope();
        UI_SourceStats();
        UI_VulkanRenderSettings();
#else
        UI_ActiveSource();
        UI_SourceStats();
        UI_RenderSettings();
#endif
    }
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
        if (ImGui::BeginMenu("Save layout")) {
            for (uint32_t presetIndex = 1u; presetIndex <= 3u; ++presetIndex) {
                if (ImGui::MenuItem(std::format("P{}", presetIndex).c_str())) {
                    SaveLayoutPreset(presetIndex);
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Load Layout")) {
            for (uint32_t presetIndex = 1u; presetIndex <= 3u; ++presetIndex) {
                const auto presetExists = std::filesystem::exists(LayoutPresetPath(presetIndex));
                if (ImGui::MenuItem(std::format("P{}", presetIndex).c_str(), nullptr, false, presetExists)) {
                    LoadLayoutPreset(presetIndex);
                }
            }
            ImGui::EndMenu();
        }
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
    ImGui::Begin("Settings");

    ImGui::End();
}

void Application::UI_Sources() noexcept {
    const auto sources = m_sourceProvider->GetSources();

    ImGui::Begin("Sources");

    ImGui::SeparatorText("Render Backends");
#if SCPP_USE_VULKAN_UI
    ImGui::TextUnformatted("UI Mode: Vulkan");
#else
    ImGui::TextUnformatted("UI Mode: OpenGL/OpenCL");
#endif
    if (m_vulkanRenderer && m_vulkanRenderer->IsInitialized()) {
        ImGui::Text("Vulkan: %s", m_vulkanRenderer->GetDeviceName().data());
    } else {
        const auto error = m_vulkanRenderer ? m_vulkanRenderer->GetLastError() : "not created"sv;
        ImGui::Text("Vulkan: unavailable (%s)", error.data());
    }
    ImGui::Text("OpenCL/OpenGL: %s", m_openclDeviceProvider && m_openclDeviceProvider->IsInitialized() ? "available" : "unavailable");

    ImGui::SeparatorText("File / SRT URL");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##video-file-path", m_videoFilePath.data(), m_videoFilePath.size());

    const bool openCLSourcesAvailable = m_openclDeviceProvider && m_openclDeviceProvider->IsInitialized();
    const bool videoFileAvailable     = openCLSourcesAvailable
#if SCPP_USE_VULKAN_UI
                                    || (m_vulkanRenderer && m_vulkanRenderer->IsInitialized())
#endif
        ;
    if (!videoFileAvailable) {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("Open Source")) {
        StartVideoFileSource(m_videoFilePath.data());
    }
    if (!videoFileAvailable) {
        ImGui::EndDisabled();
    }

    if (m_source) {
        ImGui::Spacing();
        ImGui::SeparatorText("Current Source");
        ImGui::Text("Name: %s", m_source->GetName().data());
        ImGui::Text("State: %s", m_source->IsRunning() ? "Running" : "Stopped");

        if (m_source->IsRunning()) {
            if (ImGui::Button("Stop Source")) {
                m_source->Stop();
            }
        } else {
            if (ImGui::Button("Start Source")) {
                if (m_source->Start() != ErrorCode::None) {
                    std::println("Failed to start source: {}", m_source->GetName());
                }
            }
        }
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Built-in");

    if (!openCLSourcesAvailable) {
        ImGui::BeginDisabled();
    }
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
    if (!openCLSourcesAvailable) {
        ImGui::EndDisabled();
        ImGui::TextWrapped("Built-in sources are waiting for Vulkan render pipeline support.");
    }
    ImGui::End();
}

void Application::UI_SourceStats() noexcept {
    const auto& sourceStats = m_source->GetStats();
    ImGui::SetNextWindowSizeConstraints(ImVec2(200, 50), c_uiMaxSize);
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
    ImGui::End();
}

void Application::UI_RenderSettings() noexcept {
    auto& renderSettings = m_source->GetRenderSettings();

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
    if (!ImGui::Begin(sourcePreview.description.data(), &m_showSourcePreview, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }
    ImGuiUtilImageRender(sourcePreview, ScaleBehavior::ScaleToFit);
    ImGui::End();
}

void Application::UI_VulkanSourcePreview() noexcept {
    if (!m_showSourcePreview || !m_vulkanRenderer || !m_vulkanRenderer->HasSourcePreview()) {
        return;
    }

    const auto& sourceStats = m_source->GetStats();
    const auto sourceAspect = WindowAspectData{
        .targetAspectRatio = static_cast<float>(sourceStats.sourceDims.width) / static_cast<float>(sourceStats.sourceDims.height),
        .offset            = ImVec2(0.f, 32.f)};

    const auto sourceDims = m_vulkanRenderer->GetSourcePreviewDims();

    ImGui::SetNextWindowSizeConstraints(ImVec2(160, 32 + 90), c_uiMaxSize, WindowSizeConstraints::AspectWithOffset, (void*)&sourceAspect);
    if (!ImGui::Begin("Source Preview", &m_showSourcePreview, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    const auto imgSize    = ImGuiUtilGetImageSize(sourceDims.ToImVec2(), ScaleBehavior::ScaleToFit);
    const auto [uv0, uv1] = ImGuiUtilGetUVs(FlipBehavior::DoNotFlip);
    ImGui::ImageWithBg(m_vulkanRenderer->GetSourcePreviewTextureID(), imgSize, uv0, uv1, c_bgColor);
    ImGui::End();
}

void Application::UI_VulkanFalseColor() noexcept {
    if (!m_showFalseColor || !m_vulkanRenderer || !m_vulkanRenderer->HasFalseColor()) {
        return;
    }

    const auto& sourceStats = m_source->GetStats();
    const auto sourceAspect = WindowAspectData{
        .targetAspectRatio = static_cast<float>(sourceStats.sourceDims.width) / static_cast<float>(sourceStats.sourceDims.height),
        .offset            = ImVec2(0.f, 32.f)};

    const auto sourceDims = m_vulkanRenderer->GetSourcePreviewDims();

    ImGui::SetNextWindowSizeConstraints(ImVec2(160, 32 + 90), c_uiMaxSize, WindowSizeConstraints::AspectWithOffset, (void*)&sourceAspect);
    if (!ImGui::Begin("False Color", &m_showFalseColor, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    const auto imgSize    = ImGuiUtilGetImageSize(sourceDims.ToImVec2(), ScaleBehavior::ScaleToFit);
    const auto [uv0, uv1] = ImGuiUtilGetUVs(FlipBehavior::DoNotFlip);

    const auto topLeft = ImGui::GetCursorScreenPos();
    ImGui::ImageWithBg(m_vulkanRenderer->GetSourcePreviewTextureID(), imgSize, uv0, uv1, c_bgColor);

    ImGui::SetCursorScreenPos(topLeft);
    ImGui::ImageWithBg(m_vulkanRenderer->GetFalseColorTextureID(), imgSize, uv0, uv1, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::End();
}

void Application::UI_VulkanLumaWaveform() noexcept {
    if (!m_showWFLuma || !m_vulkanRenderer || !m_vulkanRenderer->HasLumaWaveform()) {
        return;
    }

    ImGui::SetNextWindowSizeConstraints(c_uiMinWFSize, c_uiMaxSize, WindowSizeConstraints::AspectWithOffset, (void*)&c_uiWFAspect);
    if (!ImGui::Begin("Luminance Waveform", &m_showWFLuma, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    const auto imgSize    = ImGuiUtilGetImageSize(ImVec2(580.f, 256.f), ScaleBehavior::ScaleToFit);
    const auto [uv0, uv1] = ImGuiUtilGetUVs(FlipBehavior::DoNotFlip);
    ImGui::ImageWithBg(m_vulkanRenderer->GetLumaWaveformTextureID(), imgSize, uv0, uv1, ImVec4(0.f, 0.f, 0.f, 1.f));
    ImGui::End();
}

void Application::UI_VulkanRgbWaveform() noexcept {
    if (!m_showWFRgb || !m_vulkanRenderer || !m_vulkanRenderer->HasRgbWaveform()) {
        return;
    }

    ImGui::SetNextWindowSizeConstraints(c_uiMinWFSize, c_uiMaxSize, WindowSizeConstraints::AspectWithOffset, (void*)&c_uiWFAspect);
    if (!ImGui::Begin("RGB Waveform", &m_showWFRgb, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    const auto imgSize    = ImGuiUtilGetImageSize(ImVec2(580.f, 256.f), ScaleBehavior::ScaleToFit);
    const auto [uv0, uv1] = ImGuiUtilGetUVs(FlipBehavior::DoNotFlip);
    const auto topLeft    = ImGui::GetCursorScreenPos();
    ImGui::ImageWithBg(m_vulkanRenderer->GetRgbWaveformTextureID(), imgSize, uv0, uv1, c_bgColor);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    for (int i = 0; i <= 256; i += 32) {
        const float y = topLeft.y + imgSize.y - (i / 255.f) * imgSize.y;
        drawList->AddLine(ImVec2(topLeft.x, y), ImVec2(topLeft.x + imgSize.x, y), c_lineColor, c_lineThickness);
    }

    ImGui::End();
}

void Application::UI_VulkanRgbParade() noexcept {
    if (!m_showWFRgbParade || !m_vulkanRenderer || !m_vulkanRenderer->HasRgbParade()) {
        return;
    }

    ImGui::SetNextWindowSizeConstraints(c_uiMinWFSize, c_uiMaxSize, WindowSizeConstraints::AspectWithOffset, (void*)&c_uiWFAspect);
    if (!ImGui::Begin("RGB Parade", &m_showWFRgbParade, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    const auto imgSize    = ImGuiUtilGetImageSize(ImVec2(580.f, 256.f), ScaleBehavior::ScaleToFit);
    const auto [uv0, uv1] = ImGuiUtilGetUVs(FlipBehavior::DoNotFlip);
    const auto topLeft    = ImGui::GetCursorScreenPos();
    ImGui::ImageWithBg(m_vulkanRenderer->GetRgbParadeTextureID(), imgSize, uv0, uv1, c_bgColor);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    for (int i = 0; i <= 256; i += 32) {
        const float y = topLeft.y + imgSize.y - (i / 255.f) * imgSize.y;
        drawList->AddLine(ImVec2(topLeft.x, y), ImVec2(topLeft.x + imgSize.x, y), c_lineColor, c_lineThickness);
    }

    const auto p1 = ImGuiUtilRelPosToImagePos(ImVec2(1.f / 3.f, 0.f), imgSize, topLeft);
    const auto p2 = ImGuiUtilRelPosToImagePos(ImVec2(1.f / 3.f, 1.f), imgSize, topLeft);
    drawList->AddLine(p1, p2, c_lineColor, c_lineThickness);

    const auto p3 = ImGuiUtilRelPosToImagePos(ImVec2(2.f / 3.f, 0.f), imgSize, topLeft);
    const auto p4 = ImGuiUtilRelPosToImagePos(ImVec2(2.f / 3.f, 1.f), imgSize, topLeft);
    drawList->AddLine(p3, p4, c_lineColor, c_lineThickness);

    ImGui::End();
}

void Application::UI_VulkanRgbBlacklevel() noexcept {
    if (!m_showWFRgbBlacks || !m_vulkanRenderer || !m_vulkanRenderer->HasRgbBlacklevel()) {
        return;
    }

    ImGui::SetNextWindowSizeConstraints(c_uiMinWFSize, c_uiMaxSize, WindowSizeConstraints::AspectWithOffset, (void*)&c_uiWFAspect);
    if (!ImGui::Begin("RGB Blacklevel", &m_showWFRgbBlacks, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    const auto imgSize    = ImGuiUtilGetImageSize(ImVec2(580.f, 256.f), ScaleBehavior::ScaleToFit);
    const auto [uv0, uv1] = ImGuiUtilGetUVs(FlipBehavior::DoNotFlip);
    const auto topLeft    = ImGui::GetCursorScreenPos();
    ImGui::ImageWithBg(m_vulkanRenderer->GetRgbBlacklevelTextureID(), imgSize, uv0, uv1, c_bgColor);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    constexpr auto increment = 1.f / 35.f;
    for (int i = 0; i < 35; ++i) {
        const auto color = (i % 5 == 0) ? c_lineColor : c_lineColorQuart;
        const auto p1 = ImGuiUtilRelPosToImagePos(ImVec2(0.f, i * increment), imgSize, topLeft);
        const auto p2 = ImGuiUtilRelPosToImagePos(ImVec2(1.f, i * increment), imgSize, topLeft);
        drawList->AddLine(p1, p2, color, c_lineThickness);
    }

    ImGui::End();
}

void Application::UI_VulkanYuvParade() noexcept {
    if (!m_showWFYuvParade || !m_vulkanRenderer || !m_vulkanRenderer->HasYuvParade()) {
        return;
    }

    ImGui::SetNextWindowSizeConstraints(c_uiMinWFSize, c_uiMaxSize, WindowSizeConstraints::AspectWithOffset, (void*)&c_uiWFAspect);
    if (!ImGui::Begin("YUV Parade", &m_showWFYuvParade, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    const auto imgSize    = ImGuiUtilGetImageSize(ImVec2(580.f, 256.f), ScaleBehavior::ScaleToFit);
    const auto [uv0, uv1] = ImGuiUtilGetUVs(FlipBehavior::DoNotFlip);
    const auto topLeft    = ImGui::GetCursorScreenPos();
    ImGui::ImageWithBg(m_vulkanRenderer->GetYuvParadeTextureID(), imgSize, uv0, uv1, c_bgColor);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    for (int i = 0; i <= 256; i += 32) {
        const auto p1 = ImGuiUtilRelPosToImagePos(ImVec2(0.f, i / 255.f), imgSize, topLeft);
        const auto p2 = ImGuiUtilRelPosToImagePos(ImVec2(1.f, i / 255.f), imgSize, topLeft);
        drawList->AddLine(p1, p2, c_lineColor, c_lineThickness);
    }

    const auto p1 = ImGuiUtilRelPosToImagePos(ImVec2(1.f / 3.f, 0.f), imgSize, topLeft);
    const auto p2 = ImGuiUtilRelPosToImagePos(ImVec2(1.f / 3.f, 1.f), imgSize, topLeft);
    drawList->AddLine(p1, p2, c_lineColor, c_lineThickness);

    const auto p3 = ImGuiUtilRelPosToImagePos(ImVec2(2.f / 3.f, 0.f), imgSize, topLeft);
    const auto p4 = ImGuiUtilRelPosToImagePos(ImVec2(2.f / 3.f, 1.f), imgSize, topLeft);
    drawList->AddLine(p3, p4, c_lineColor, c_lineThickness);

    ImGui::End();
}

void Application::UI_VulkanUvScope() noexcept {
    if (!m_showSCUV || !m_vulkanRenderer || !m_vulkanRenderer->HasUvScope()) {
        return;
    }

    ImGui::SetNextWindowSizeConstraints(c_uiMinSCSize, c_uiMaxSize, WindowSizeConstraints::SquareWithOffset, (void*)&c_uiSCWindowSizeOffset);
    if (!ImGui::Begin("UV Vectorscope", &m_showSCUV, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    const auto imgSize    = ImGuiUtilGetImageSize(ImVec2(256.f, 256.f), ScaleBehavior::ScaleToFit);
    const auto [uv0, uv1] = ImGuiUtilGetUVs(FlipBehavior::DoNotFlip);
    const auto topLeft    = ImGui::GetCursorScreenPos();
    ImGui::ImageWithBg(m_vulkanRenderer->GetUvScopeTextureID(), imgSize, uv0, uv1, c_bgColor);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const auto p0        = ImGuiUtilRelPosToImagePos(ImVec2(.5f, .5f), imgSize, topLeft);

    for (size_t i = 0; i < c_uvVectors.size() - 1; ++i) {
        const auto uva = c_uvVectors[i];
        const auto uvb = c_uvVectors[i + 1];
        const auto pa  = ImGuiUtilRelPosToImagePos(ImVec2(uva.x, uva.y), imgSize, topLeft);
        const auto pb  = ImGuiUtilRelPosToImagePos(ImVec2(uvb.x, uvb.y), imgSize, topLeft);
        drawList->AddLine(pa, pb, c_lineColor, c_lineThickness);
        drawList->AddLine(p0, pa, c_lineColor, c_lineThickness);
    }

    ImGui::End();
}

void Application::UI_VulkanXyzScope() noexcept {
    if (!m_showSCXYZ || !m_vulkanRenderer || !m_vulkanRenderer->HasXyzScope()) {
        return;
    }

    ImGui::SetNextWindowSizeConstraints(c_uiMinSCSize, c_uiMaxSize, WindowSizeConstraints::SquareWithOffset, (void*)&c_uiSCWindowSizeOffset);
    if (!ImGui::Begin("CIE 1931 Chromaticity", &m_showSCXYZ, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    const auto imgSize    = ImGuiUtilGetImageSize(ImVec2(256.f, 256.f), ScaleBehavior::ScaleToFit);
    const auto [uv0, uv1] = ImGuiUtilGetUVs(FlipBehavior::DoNotFlip);
    const auto topLeft    = ImGui::GetCursorScreenPos();
    ImGui::ImageWithBg(m_vulkanRenderer->GetXyzScopeTextureID(), imgSize, uv0, uv1, c_bgColor);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    for (int i = 0; i <= 10; ++i) {
        const float x     = topLeft.x + (i / 10.f) * imgSize.x;
        const float y     = topLeft.y + (i / 10.f) * imgSize.y;
        const ImU32 color = (i % 2 == 0) ? c_lineColorHalf : c_lineColorQuart;
        drawList->AddLine(ImVec2(x, topLeft.y), ImVec2(x, topLeft.y + imgSize.y), color, c_lineThickness);
        drawList->AddLine(ImVec2(topLeft.x, y), ImVec2(topLeft.x + imgSize.x, y), color, c_lineThickness);
    }

    const auto& renderSettings = m_source->GetRenderSettings();
    for (const auto colorSpace : c_cieTriangleColorspaces) {
        const auto primaries = GetCIEPrimaries(colorSpace);
        const auto p1 = ImVec2(topLeft.x + primaries[0].x * imgSize.x, topLeft.y + (1.f - primaries[0].y) * imgSize.y);
        const auto p2 = ImVec2(topLeft.x + primaries[1].x * imgSize.x, topLeft.y + (1.f - primaries[1].y) * imgSize.y);
        const auto p3 = ImVec2(topLeft.x + primaries[2].x * imgSize.x, topLeft.y + (1.f - primaries[2].y) * imgSize.y);
        const ImU32 color = (colorSpace == renderSettings.colorSpace) ? c_lineColor : c_lineColorHalf;
        drawList->AddTriangle(p1, p2, p3, color);
    }

    for (size_t i = 0; i < c_cieLocus.size() - 1; ++i) {
        const auto p1 = ImVec2(topLeft.x + c_cieLocus[i].x * imgSize.x, topLeft.y + (1.f - c_cieLocus[i].y) * imgSize.y);
        const auto p2 = ImVec2(topLeft.x + c_cieLocus[i + 1].x * imgSize.x, topLeft.y + (1.f - c_cieLocus[i + 1].y) * imgSize.y);
        drawList->AddLine(p1, p2, c_lineColorHalf, c_lineThickness);
    }

    ImGui::End();
}

void Application::UI_VulkanDiamondScope() noexcept {
    if (!m_showSCDia || !m_vulkanRenderer || !m_vulkanRenderer->HasDiamondScope()) {
        return;
    }

    ImGui::SetNextWindowSizeConstraints(c_uiMinSCSize, c_uiMaxSize, WindowSizeConstraints::SquareWithOffset, (void*)&c_uiSCWindowSizeOffset);
    if (!ImGui::Begin("Double Diamond", &m_showSCDia, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    const auto imgSize    = ImGuiUtilGetImageSize(ImVec2(256.f, 256.f), ScaleBehavior::ScaleToFit);
    const auto [uv0, uv1] = ImGuiUtilGetUVs(FlipBehavior::DoNotFlip);
    const auto topLeft    = ImGui::GetCursorScreenPos();
    ImGui::ImageWithBg(m_vulkanRenderer->GetDiamondScopeTextureID(), imgSize, uv0, uv1, c_bgColor);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    for (size_t i = 0; i < c_diaVerts.size() - 1; ++i) {
        const auto p1 = ImGuiUtilRelPosToImagePos(c_diaVerts[i], imgSize, topLeft);
        const auto p2 = ImGuiUtilRelPosToImagePos(c_diaVerts[i + 1], imgSize, topLeft);
        drawList->AddLine(p1, p2, c_lineColor, c_lineThickness);
    }

    ImGui::End();
}

void Application::UI_VulkanRenderSettings() noexcept {
    if (!m_vulkanRenderer) {
        return;
    }

    auto& renderSettings = m_source->GetRenderSettings();

    ImGui::Begin("Render Settings", nullptr, ImGuiWindowFlags_NoCollapse);
    ImGui::SeparatorText("Source");

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

    ImGui::SeparatorText("False Color");

    if (ImGui::BeginCombo("False Color Map", m_vulkanRenderer->GetSelectedFalseColorMapName().data())) {
        for (const auto [name, map] : c_falseColorMapsSpan) {
            if (ImGui::Selectable(name.data(), m_vulkanRenderer->GetSelectedFalseColorMapName() == name)) {
                m_vulkanRenderer->SetFalseColorMap(map, name);
            }
        }
        ImGui::EndCombo();
    }

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
        const auto wfLumaVisible = ImGui::Begin(wfLuma.description.data(), &m_showWFLuma, ImGuiWindowFlags_NoCollapse);
        if (wfLumaVisible)
            ImGuiUtilRenderLumaWF(wfLuma, renderSettings.yuvRange, ScaleBehavior::ScaleToFit, FlipBehavior::FlipVertically);
        ImGui::End();
    }

    if (m_showWFRgb) {
        auto& wfRgb = sourceTextures->wfRGB;
        ImGui::SetNextWindowSizeConstraints(c_uiMinWFSize, c_uiMaxSize, WindowSizeConstraints::AspectWithOffset, (void*)&c_uiWFAspect);
        const auto wfRgbVisible = ImGui::Begin(wfRgb.description.data(), &m_showWFRgb, ImGuiWindowFlags_NoCollapse);
        if (wfRgbVisible)
            ImGuiUtilRenderRGBWF(wfRgb, ScaleBehavior::ScaleToFit, FlipBehavior::FlipVertically);
        ImGui::End();
    }

    if (m_showWFRgbParade) {
        auto& wfRgbParade = sourceTextures->wfRGBParade;
        ImGui::SetNextWindowSizeConstraints(c_uiMinWFSize, c_uiMaxSize, WindowSizeConstraints::AspectWithOffset, (void*)&c_uiWFAspect);
        const auto wfRgbParadeVisible = ImGui::Begin(wfRgbParade.description.data(), &m_showWFRgbParade, ImGuiWindowFlags_NoCollapse);
        if (wfRgbParadeVisible)
            ImGuiUtilRenderParade(wfRgbParade, ScaleBehavior::ScaleToFit, FlipBehavior::FlipVertically);
        ImGui::End();
    }

    if (m_showWFRgbBlacks) {
        auto& wfRgbBlacks = sourceTextures->wfRGBBlacks;
        ImGui::SetNextWindowSizeConstraints(c_uiMinWFSize, c_uiMaxSize, WindowSizeConstraints::AspectWithOffset, (void*)&c_uiWFAspect);
        const auto wfRgbBlacksVisible = ImGui::Begin(wfRgbBlacks.description.data(), &m_showWFRgbBlacks, ImGuiWindowFlags_NoCollapse);
        if (wfRgbBlacksVisible)
            ImGuiUtilRenderBlacklevel(wfRgbBlacks, ScaleBehavior::ScaleToFit, FlipBehavior::FlipVertically);
        ImGui::End();
    }

    if (m_showWFYuvParade) {
        auto& wfYuvParade = sourceTextures->wfYUVParade;
        ImGui::SetNextWindowSizeConstraints(c_uiMinWFSize, c_uiMaxSize, WindowSizeConstraints::AspectWithOffset, (void*)&c_uiWFAspect);
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
        auto scUVVisible = ImGui::Begin(scUV.description.data(), &m_showSCUV, ImGuiWindowFlags_NoCollapse);
        if (scUVVisible)
            ImGuiUtilRenderUV(scUV, ScaleBehavior::ScaleToFit);
        ImGui::End();
    }

    if (m_showSCXYZ) {
        auto& scXYZ = sourceTextures->scXYZ;
        ImGui::SetNextWindowSizeConstraints(c_uiMinSCSize, c_uiMaxSize, WindowSizeConstraints::SquareWithOffset, (void*)&c_uiSCWindowSizeOffset);
        auto scXYZVisible = ImGui::Begin(scXYZ.description.data(), &m_showSCXYZ, ImGuiWindowFlags_NoCollapse);
        if (scXYZVisible)
            ImGuiUtilRenderCIE(scXYZ, renderSettings.colorSpace, ScaleBehavior::ScaleToFit, FlipBehavior::FlipVertically);
        ImGui::End();
    }

    if (m_showSCDia) {
        auto& scDia = sourceTextures->scDia;
        ImGui::SetNextWindowSizeConstraints(c_uiMinSCSize, c_uiMaxSize, WindowSizeConstraints::SquareWithOffset, (void*)&c_uiSCWindowSizeOffset);
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
