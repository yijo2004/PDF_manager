#include "app_init.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include <GLFW/glfw3.h>
#include <fpdfview.h>

#include <cstdio>
#include <filesystem>
#include <string>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "ui_helpers.h"

static void GlfwErrorCallback(int error, const char *description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static std::string RuntimeAssetPath(const char *filename)
{
#ifdef _WIN32
    std::string exePath(MAX_PATH, '\0');
    DWORD length = 0;

    while (true)
    {
        length = GetModuleFileNameA(nullptr, exePath.data(),
                                    static_cast<DWORD>(exePath.size()));
        if (length == 0)
            return filename;

        if (length < exePath.size())
            break;

        exePath.resize(exePath.size() * 2);
    }

    exePath.resize(length);
    return (std::filesystem::path(exePath).parent_path() / filename).string();
#else
#ifdef __APPLE__
    uint32_t pathSize = 0;
    _NSGetExecutablePath(nullptr, &pathSize);
    std::string executablePath(pathSize, '\0');
    if (_NSGetExecutablePath(executablePath.data(), &pathSize) == 0)
    {
        executablePath.resize(pathSize > 0 ? pathSize - 1 : 0);
        const std::filesystem::path resources =
            std::filesystem::path(executablePath).parent_path().parent_path() /
            "Resources";
        return (resources / filename).string();
    }
#endif
    return filename;
#endif
}

GLFWwindow *InitWindow(int width, int height, const char *title)
{
    glfwSetErrorCallback(GlfwErrorCallback);
    if (!glfwInit())
        return nullptr;

#ifdef __APPLE__
    // macOS exposes modern OpenGL only through a forward-compatible core
    // profile. OpenGL 3.0/3.1 contexts are not available on macOS.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

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
#ifdef __APPLE__
    // Match the OpenGL 3.2 core profile requested by InitWindow().
    glslVersion = "#version 150";
#endif

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    // Load custom font with Korean support from the executable directory.
    const std::string fontPath = RuntimeAssetPath("font.ttf");
    ImFont *font = io.Fonts->AddFontFromFileTTF(
        fontPath.c_str(), 22.0f, nullptr, io.Fonts->GetGlyphRangesKorean());
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
