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

#include <GLFW/glfw3.h>
#include <fpdfview.h>

#include <cstdio>
#include <filesystem>
#include <vector>
#include <string>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "ui_helpers.h"

static void GlfwErrorCallback(int error, const char *description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

struct AppFontBucket
{
    int sizePx = 0;
    ImFont *font = nullptr;
};

static const int APP_FONT_SIZES[] = {18, 20, 22, 24, 26, 30};
static std::vector<AppFontBucket> g_appFonts;
static ImFont *g_fallbackFont = nullptr;

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
    return filename;
#endif
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

    // Load custom font sizes with Korean support from the executable directory.
    const std::string fontPath = RuntimeAssetPath("font.ttf");
    const ImWchar *koreanRanges = io.Fonts->GetGlyphRangesKorean();
    g_appFonts.clear();
    g_appFonts.reserve(sizeof(APP_FONT_SIZES) / sizeof(APP_FONT_SIZES[0]));
    for (int sizePx : APP_FONT_SIZES)
    {
        ImFont *font = io.Fonts->AddFontFromFileTTF(
            fontPath.c_str(), static_cast<float>(sizePx), nullptr,
            koreanRanges);
        if (font)
            g_appFonts.push_back({sizePx, font});
    }

    if (g_appFonts.empty())
    {
        printf("Failed to load font, using default.\n");
        g_fallbackFont = io.Fonts->AddFontDefault();
    }
    else
    {
        g_fallbackFont = g_appFonts.front().font;
        for (const AppFontBucket &bucket : g_appFonts)
        {
            if (bucket.sizePx == 22)
            {
                g_fallbackFont = bucket.font;
                break;
            }
        }
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

int ChooseAutoAppFontSizePx(GLFWwindow *window)
{
    int displayW = 0;
    int displayH = 0;
    if (window)
        glfwGetFramebufferSize(window, &displayW, &displayH);

    if (displayH <= 0)
    {
        GLFWmonitor *monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode *mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
        if (mode)
            displayH = mode->height;
    }

    if (displayH < 900)
        return 20;
    if (displayH < 1440)
        return 22;
    if (displayH < 2160)
        return 24;
    return 30;
}

static ImFont *FindAppFont(int sizePx)
{
    for (const AppFontBucket &bucket : g_appFonts)
    {
        if (bucket.sizePx == sizePx)
            return bucket.font;
    }
    return g_fallbackFont;
}

void ApplyAppFont(GLFWwindow *window, bool manualMode, int fontSizePx)
{
    ImGuiIO &io = ImGui::GetIO();
    int activeSize = manualMode ? fontSizePx : ChooseAutoAppFontSizePx(window);
    ImFont *font = FindAppFont(activeSize);
    if (font)
        io.FontDefault = font;
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
