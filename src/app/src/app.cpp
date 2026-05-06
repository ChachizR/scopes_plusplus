#include "app.hpp"
#include "runtime_paths.hpp"

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

    m_sourceProvider = std::make_unique<SourceProvider>();
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
    UI_Sources();

    if (m_source) {
        UI_ActiveSource();
        UI_SourceStats();
        UI_RenderSettings();
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
