#include "app.hpp"
#include "imgui_util.hpp"

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
    // io.ConfigViewportsNoAutoMerge = true;
    // io.ConfigViewportsNoTaskBarIcon = true;

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

    ImGui::Begin(sourcePreview.description.data(), nullptr, ImGuiWindowFlags_NoCollapse);
    ImGuiUtilImageRender(sourcePreview, ScaleBehavior::ScaleToFit);
    ImGui::End();

    auto& wfLuma = sourceTextures->wfLuma;

    ImGui::Begin(wfLuma.description.data(), nullptr, ImGuiWindowFlags_NoCollapse);
    ImGuiUtilRenderLumaWF(wfLuma, m_source->GetRenderSettings().yuvRange,
                          ScaleBehavior::ScaleToFit, FlipBehavior::FlipVertically);
    ImGui::End();

    auto& wfRgb = sourceTextures->wfRGB;

    ImGui::Begin(wfRgb.description.data(), nullptr, ImGuiWindowFlags_NoCollapse);
    ImGuiUtilRenderRGBWF(wfRgb, ScaleBehavior::ScaleToFit, FlipBehavior::FlipVertically);
    ImGui::End();

    auto& wfRgbParade = sourceTextures->wfRGBParade;

    ImGui::Begin(wfRgbParade.description.data(), nullptr, ImGuiWindowFlags_NoCollapse);
    ImGuiUtilImageRender(wfRgbParade, ScaleBehavior::ScaleToFit, FlipBehavior::FlipVertically);
    ImGui::End();

    auto& wfRgbBlacks = sourceTextures->wfRGBBlacks;

    ImGui::Begin(wfRgbBlacks.description.data(), nullptr, ImGuiWindowFlags_NoCollapse);
    ImGuiUtilImageRender(wfRgbBlacks, ScaleBehavior::ScaleToFit, FlipBehavior::FlipVertically);
    ImGui::End();

    auto& wfYuvParade = sourceTextures->wfYUVParade;

    ImGui::Begin(wfYuvParade.description.data(), nullptr, ImGuiWindowFlags_NoCollapse);
    ImGuiUtilImageRender(wfYuvParade, ScaleBehavior::ScaleToFit, FlipBehavior::FlipVertically);
    ImGui::End();

    auto& scUV = sourceTextures->scUV;
    ImGui::Begin(scUV.description.data(), nullptr, ImGuiWindowFlags_NoCollapse);
    ImGuiUtilImageRender(scUV, ScaleBehavior::ScaleToFit);
    ImGui::End();

    auto& scXYZ = sourceTextures->scXYZ;
    ImGui::Begin(scXYZ.description.data(), nullptr, ImGuiWindowFlags_NoCollapse);
    ImGuiUtilRenderCIE(scXYZ, m_source->GetRenderSettings().colorSpace, ScaleBehavior::ScaleToFit, FlipBehavior::FlipVertically);
    ImGui::End();

    auto& scDia = sourceTextures->scDia;
    ImGui::Begin(scDia.description.data(), nullptr, ImGuiWindowFlags_NoCollapse);
    ImGuiUtilImageRender(scDia, ScaleBehavior::ScaleToFit);
    ImGui::End();
}

void Application::UI_SourceStats() noexcept {
    const auto& sourceStats = m_source->GetStats();
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