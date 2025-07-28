#include "ui.hpp"

namespace scpp::ui {

void MainMenuBar() {
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

void SettingsWindow() {
    ImGui::Begin("Settings");

    ImGui::End();
}

void MainUI() {

    MainMenuBar();

    ImGui::DockSpaceOverViewport(
        0, ImGui::GetMainViewport(),
        ImGuiDockNodeFlags_PassthruCentralNode);

    SettingsWindow();
}

} // namespace scpp::ui
