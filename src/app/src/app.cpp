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

bool Application::InitGLFW() {
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

bool Application::InitImGui() {
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

bool Application::LoadFonts() {
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

void Application::RenderApp() {
    UI_Main();
}

void Application::UI_Main() const noexcept {
    UI_MainMenuBar();

    ImGui::DockSpaceOverViewport(
        0, ImGui::GetMainViewport(),
        ImGuiDockNodeFlags_PassthruCentralNode);

    UI_Settings();
    UI_NDISources();
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
void Application::UI_NDISources() const noexcept {
    const auto sources = m_ndiSourceProvider->GetSources();

    ImGui::Begin("NDI Sources");
    if (ImGui::BeginTable("NDI Sources Table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Source Name");
        ImGui::TableSetupColumn("Source Type");
        for (const auto& source : sources) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(source.p_ndi_name);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(source.p_url_address);
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

void Application::Run() {

    std::unique_ptr<scpp::VideoSource> source = nullptr;

    while (!glfwWindowShouldClose(m_window)) {

        glfwPollEvents();
        if (glfwGetWindowAttrib(m_window, GLFW_ICONIFIED) != 0) {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (!source) {
            const auto sources = m_ndiSourceProvider->GetSources();

            if (!sources.empty()) {

                source = std::make_unique<scpp::NDISource>(*m_openclDeviceProvider, sources[0]);

                if (source->Start() != scpp::ErrorCode::None) {
                    std::println("Failed to start NDI source: {}", source->GetName());
                    source.reset();
                }
            }
        }

        if (false)
            ImGui::ShowDemoWindow();

        RenderApp();

        if (source) {
            auto& sourceRenderer = source->GetRenderer();
            if (sourceRenderer.needsResizeFlag_mainThread) {
                sourceRenderer.ResizeGLTextures();
            }

            auto sourceTextures = sourceRenderer.GetTargetTextures();

            ImGui::Begin("ImagePreview", nullptr, ImGuiWindowFlags_NoCollapse);

            ImGuiImageRender(sourceTextures->sourcePreview);

            ImGui::End();
        }

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

} // namespace scpp