#include "app_init.h"

#include <GLFW/glfw3.h>
#include <fpdfview.h>

#include <cstdio>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "ui_helpers.h"

static void GlfwErrorCallback(int error, const char *description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

GLFWwindow *InitWindow(int width, int height, const char *title)
{
    glfwSetErrorCallback(GlfwErrorCallback);
    if (!glfwInit())
        return nullptr;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow *window =
        glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (window)
    {
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1); // VSync
    }
    return window;
}

void InitPDFium()
{
    FPDF_LIBRARY_CONFIG config;
    config.version = 2;
    config.m_pUserFontPaths = nullptr;
    config.m_pIsolate = nullptr;
    config.m_v8EmbedderSlot = 0;
    FPDF_InitLibraryWithConfig(&config);
}

void InitImGui(GLFWwindow *window, const char *glslVersion)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    // Load custom font with Korean support
    ImFont *font = io.Fonts->AddFontFromFileTTF(
        "font.ttf", 22.0f, nullptr, io.Fonts->GetGlyphRangesKorean());
    if (!font)
    {
        printf("Failed to load font, using default.\n");
        io.Fonts->AddFontDefault();
    }

    ApplyCustomTheme();

    // Adjust style for multi-viewport
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGuiStyle &style = ImGui::GetStyle();
        style.WindowRounding = 6.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glslVersion);
}

void Shutdown(GLFWwindow *window)
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    FPDF_DestroyLibrary();
    glfwDestroyWindow(window);
    glfwTerminate();
}
