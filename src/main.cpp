/**
 * PDF Viewer Application
 *
 * A simple PDF viewer built with ImGui, GLFW, OpenGL, and PDFium.
 */

#include <GLFW/glfw3.h>

#include <cstdio>
#include <string>

#include "app_init.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "pdf_library.h"
#include "pdf_viewer.h"
#include "setlist_gen.h"
#include "ui_panels.h"

int main(int, char **)
{
    // Initialize systems
    GLFWwindow *window = InitWindow(1280, 720, "PDF Manager");
    if (!window)
        return 1;

    InitPDFium();
    InitImGui(window, "#version 130");

    // Create application state
    PdfLibrary library;
    PdfViewer viewer;
    int selectedFileIndex = -1;
    int selectedSetlistIndex = -1;
    int selectedSetlistItemIndex = -1;
    SetlistManager setlistManager;
    AppUiState uiState;
    LoadUiSettings(uiState);
    uiState.activeFontMode = uiState.fontMode;
    uiState.activeFontSizePx = uiState.fontSizePx;

    // Auto-load saved setlists
    {
        std::string savePath = SetlistManager::GetDefaultSavePath();
        if (setlistManager.LoadFromFile(savePath))
        {
            printf("[App] Loaded setlists from %s\n", savePath.c_str());
            if (setlistManager.GetSetlistCount() > 0)
                selectedSetlistIndex = 0;
        }
    }

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Update viewer (renders page if needed)
        viewer.Update();

        // Begin ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ApplyAppFont(window, uiState.activeFontMode == AppFontMode::Manual,
                     uiState.activeFontSizePx);
        ImGui::NewFrame();

        ImGuiIO &io = ImGui::GetIO();
        RenderMainMenuBar(library, viewer, setlistManager, uiState,
                          selectedFileIndex, selectedSetlistIndex,
                          selectedSetlistItemIndex);

        // Create dockable workspace
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        // Render UI panels
        ImGuiViewport *viewport = ImGui::GetMainViewport();
        RenderLibraryPanel(library, viewer, setlistManager,
                           uiState,
                           selectedFileIndex, selectedSetlistIndex,
                           selectedSetlistItemIndex, viewport);
        RenderDocumentToolbar(viewer, setlistManager, uiState, io, viewport);
        RenderNotesPanel(setlistManager, uiState, viewport);
        RenderViewerPanel(viewer, setlistManager, uiState, viewport);
        RenderSplitters(uiState, io, viewport,
                        setlistManager.IsActive() && uiState.notesVisible);
        if (uiState.exitRequested)
            glfwSetWindowShouldClose(window, GLFW_TRUE);

        // Render frame
        ImGui::Render();
        int displayW, displayH;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.07f, 0.08f, 0.11f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Handle multi-viewport
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow *backupContext = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backupContext);
        }

        glfwSwapBuffers(window);
    }

    // Auto-save setlists on exit
    if (uiState.autoSaveSetlists && setlistManager.GetSetlistCount() > 0)
    {
        std::string savePath = SetlistManager::GetDefaultSavePath();
        if (setlistManager.SaveToFile(savePath))
            printf("[App] Saved setlists to %s\n", savePath.c_str());
    }
    SaveUiSettings(uiState);

    // Cleanup
    viewer.Close();
    Shutdown(window);

    return 0;
}
