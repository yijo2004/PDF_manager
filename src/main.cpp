#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>

// Define this before including glfw3.h
#define GL_SILENCE_DEPRECATION 
#include <GLFW/glfw3.h> 

// ### PDFium Header
#include <fpdfview.h>

// Simple error callback for GLFW
static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(int, char**)
{
    // -----------------------------------------------------------------------
    // 1. Setup GLFW (Windowing System)
    // -----------------------------------------------------------------------
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
    // For Windows/Linux, GL 3.0 + GLSL 130 is usually sufficient and safest.
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(1280, 720, "My PDF Viewer", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // -----------------------------------------------------------------------
    // 2. Initialize PDFium (The PDF Library)
    // -----------------------------------------------------------------------
    // We must initialize the library once before loading any documents.
    FPDF_LIBRARY_CONFIG config;
    config.version = 2;
    config.m_pUserFontPaths = nullptr;
    config.m_pIsolate = nullptr;
    config.m_v8EmbedderSlot = 0;

    FPDF_InitLibraryWithConfig(&config);
    printf("PDFium Initialized successfully!\n");

    // -----------------------------------------------------------------------
    // 3. Setup ImGui Context
    // -----------------------------------------------------------------------
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    // Enable useful features
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows

    // Setup ImGui style
    ImGui::StyleColorsDark();

    // Tweak WindowRounding/WindowBg so platform windows look identical to regular ones.
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // -----------------------------------------------------------------------
    // 4. Main Application Loop
    // -----------------------------------------------------------------------
    while (!glfwWindowShouldClose(window))
    {
        // Poll and handle events (inputs, window resize, etc.)
        glfwPollEvents();

        // Start the ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // --- Create a fullscreen DockSpace ---
        // This allows you to dock windows to the main application background
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        // --- Your Application Code Here ---
        
        // 1. A simple Control Window
        ImGui::Begin("Status");
        ImGui::Text("PDFium Status: Running");
        ImGui::Text("FPS: %.1f", io.Framerate);
        ImGui::End();

        // 2. The ImGui Demo (Very useful for learning widgets)
        ImGui::ShowDemoWindow();

        // ----------------------------------

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Render draw data
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);
    }

    // -----------------------------------------------------------------------
    // 5. Cleanup
    // -----------------------------------------------------------------------
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // Destroy PDFium Library
    FPDF_DestroyLibrary();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}