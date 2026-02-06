/**
 * PDF Viewer Application
 * 
 * A simple PDF viewer built with ImGui, GLFW, OpenGL, and PDFium.
 */

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <fpdfview.h>

#include "pdf_viewer.h"
#include "pdf_library.h"
#include "file_dialog.h"

#include <cstdio>
#include <cstdint>
#include <algorithm>

// =============================================================================
// Error Callbacks
// =============================================================================

static void GlfwErrorCallback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// =============================================================================
// Application Initialization
// =============================================================================

static GLFWwindow* InitWindow(int width, int height, const char* title)
{
    glfwSetErrorCallback(GlfwErrorCallback);
    if (!glfwInit())
        return nullptr;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (window)
    {
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1); // VSync
    }
    return window;
}

static void InitPDFium()
{
    FPDF_LIBRARY_CONFIG config;
    config.version = 2;
    config.m_pUserFontPaths = nullptr;
    config.m_pIsolate = nullptr;
    config.m_v8EmbedderSlot = 0;
    FPDF_InitLibraryWithConfig(&config);
}

static void InitImGui(GLFWwindow* window, const char* glslVersion)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    // Load custom font with Korean support
    ImFont* font = io.Fonts->AddFontFromFileTTF(
        "font.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesKorean());
    if (!font)
    {
        printf("Failed to load font, using default.\n");
        io.Fonts->AddFontDefault();
    }

    ImGui::StyleColorsDark();

    // Adjust style for multi-viewport
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glslVersion);
}

static void Shutdown(GLFWwindow* window)
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    FPDF_DestroyLibrary();
    glfwDestroyWindow(window);
    glfwTerminate();
}

// =============================================================================
// UI Layout State
// =============================================================================

// Sidebar width as a ratio of total window width (default 25%)
static float g_sidebarWidthRatio = 0.25f;
// Controls panel height as a ratio of sidebar height (default 50%)
static float g_controlsHeightRatio = 0.5f;
// Minimum sizes for panels
static const float MIN_SIDEBAR_RATIO = 0.15f;
static const float MAX_SIDEBAR_RATIO = 0.5f;
static const float MIN_PANEL_HEIGHT_RATIO = 0.15f;
static const float MAX_PANEL_HEIGHT_RATIO = 0.85f;
// Splitter drag state
static bool g_draggingHorizontal = false;
static bool g_draggingVertical = false;
static const float SPLITTER_THICKNESS = 6.0f;

// =============================================================================
// UI Rendering
// =============================================================================

static void RenderLibraryPanel(PdfLibrary& library, PdfViewer& viewer, int& selectedIndex, const ImGuiViewport* viewport)
{
    float sidebarWidth = viewport->WorkSize.x * g_sidebarWidthRatio;
    float controlsHeight = viewport->WorkSize.y * g_controlsHeightRatio;
    float libraryHeight = viewport->WorkSize.y - controlsHeight;

    ImVec2 panelPos = ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + controlsHeight);
    ImGui::SetNextWindowPos(panelPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(sidebarWidth, libraryHeight), ImGuiCond_Always);

    ImGui::Begin("PDF Library", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

    // Folder selection button
    if (ImGui::Button("Open Folder...", ImVec2(-1, 30)))
    {
        std::string folderPath = FileDialog::OpenFolder();
        if (!folderPath.empty())
        {
            library.LoadFolder(folderPath);
            selectedIndex = -1;
            viewer.Close();
        }
    }

    if (library.IsLoaded())
    {
        // Refresh button
        if (ImGui::Button("Refresh", ImVec2(-1, 0)))
        {
            library.Refresh();
        }

        ImGui::Separator();

        // Folder info
        ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "Folder:");
        ImGui::TextWrapped("%s", library.GetFolderName().c_str());
        ImGui::Text("%zu PDF files", library.GetFileCount());

        ImGui::Separator();

        // File list
        if (library.GetFileCount() > 0)
        {
            ImGui::BeginChild("FileList", ImVec2(0, 0), true);
            
            const auto& files = library.GetFiles();
            for (size_t i = 0; i < files.size(); i++)
            {
                const auto& entry = files[i];
                bool isSelected = (static_cast<int>(i) == selectedIndex);

                // Highlight the currently selected/open file
                if (isSelected)
                {
                    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.5f, 0.7f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.4f, 0.6f, 0.8f, 1.0f));
                    ImGui::PopStyleColor(2);
                }

                if (ImGui::Selectable(entry.filename.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick))
                {
                    selectedIndex = static_cast<int>(i);
                    
                    if (viewer.GetFilename() != entry.filename)
                    {
                        viewer.Load(entry.fullPath);
                    }
                }

                // Tooltip with full path
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", entry.fullPath.c_str());
                }
            }

            ImGui::EndChild();
        }
        else
        {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No PDF files found");
        }
    }
    else
    {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Select a folder to browse PDFs");
    }

    ImGui::End();
}

static void RenderControlsPanel(PdfViewer& viewer, const ImGuiIO& io, const ImGuiViewport* viewport)
{
    float sidebarWidth = viewport->WorkSize.x * g_sidebarWidthRatio;
    float controlsHeight = viewport->WorkSize.y * g_controlsHeightRatio;

    ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(sidebarWidth, controlsHeight), ImGuiCond_Always);

    ImGui::Begin("PDF Controls", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

    if (viewer.IsLoaded())
    {
        // File info
        ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "Current File:");
        ImGui::TextWrapped("%s", viewer.GetFilename().c_str());
        
        ImGui::Separator();
        
        // Page navigation
        ImGui::Text("Page: %d / %d", viewer.GetCurrentPage() + 1, viewer.GetPageCount());

        // Navigation buttons
        ImGui::BeginDisabled(!viewer.CanGoPrevious());
        if (ImGui::Button("< Prev", ImVec2(80, 0)) || ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
        {
            viewer.PreviousPage();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();

        ImGui::BeginDisabled(!viewer.CanGoNext());
        if (ImGui::Button("Next >", ImVec2(80, 0)) || ImGui::IsKeyPressed(ImGuiKey_RightArrow))
        {
            viewer.NextPage();
        }
        ImGui::EndDisabled();

        // Zoom controls
        ImGui::Separator();
        ImGui::Text("Zoom: %.0f%%", viewer.GetZoom() * 100.0f);

        if (ImGui::Button("-", ImVec2(40, 0)))
        {
            viewer.ZoomOut();
        }
        ImGui::SameLine();
        if (ImGui::Button("+", ImVec2(40, 0)))
        {
            viewer.ZoomIn();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset", ImVec2(60, 0)))
        {
            viewer.ResetZoom();
        }

        // Ctrl+Scroll zoom
        if (io.KeyCtrl && io.MouseWheel != 0.0f)
        {
            if (io.MouseWheel > 0)
                viewer.ZoomIn(1.1f);
            else
                viewer.ZoomOut(1.1f);
        }

        ImGui::Separator();
        if (ImGui::Button("Close PDF", ImVec2(-1, 0)))
        {
            viewer.Close();
        }
    }
    else
    {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No PDF loaded");
        ImGui::TextWrapped("Select a PDF from the library panel to view it.");
    }

    ImGui::Separator();
    ImGui::Text("FPS: %.1f", io.Framerate);
    
    ImGui::End();
}

//  Vibe coded, have no idea how this works but it works
static void RenderSplitters(ImGuiIO& io, const ImGuiViewport* viewport)
{
    float sidebarWidth = viewport->WorkSize.x * g_sidebarWidthRatio;
    float controlsHeight = viewport->WorkSize.y * g_controlsHeightRatio;
    
    // Colors for splitters
    ImVec4 splitterColor = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
    ImVec4 splitterHoveredColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    ImVec4 splitterActiveColor = ImVec4(0.6f, 0.7f, 0.9f, 1.0f);
    
    // Horizontal splitter (resize sidebar width) - vertical bar on right edge of sidebar
    {
        ImVec2 splitterPos = ImVec2(viewport->WorkPos.x + sidebarWidth - SPLITTER_THICKNESS / 2, viewport->WorkPos.y);
        
        // Draw the splitter bar directly using the draw list
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        ImVec2 p1 = splitterPos;
        ImVec2 p2 = ImVec2(splitterPos.x + SPLITTER_THICKNESS, splitterPos.y + viewport->WorkSize.y);
        
        // Determine color based on state
        ImVec4 color = splitterColor;
        if (g_draggingHorizontal)
            color = splitterActiveColor;
        else if (io.MousePos.x >= p1.x && io.MousePos.x <= p2.x && 
                 io.MousePos.y >= p1.y && io.MousePos.y <= p2.y)
            color = splitterHoveredColor;
        
        drawList->AddRectFilled(p1, p2, ImGui::ColorConvertFloat4ToU32(color));
        
        // Check for hover and drag
        bool isHovered = (io.MousePos.x >= p1.x && io.MousePos.x <= p2.x && 
                          io.MousePos.y >= p1.y && io.MousePos.y <= p2.y);
        
        if (isHovered || g_draggingHorizontal)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
        
        // Start dragging
        if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            g_draggingHorizontal = true;
        }
        
        // Continue dragging
        if (g_draggingHorizontal)
        {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                float newRatio = (io.MousePos.x - viewport->WorkPos.x) / viewport->WorkSize.x;
                g_sidebarWidthRatio = (std::clamp)(newRatio, MIN_SIDEBAR_RATIO, MAX_SIDEBAR_RATIO);
            }
            else
            {
                g_draggingHorizontal = false;
            }
        }
    }
    
    // Vertical splitter (resize controls/library height split) - horizontal bar between panels
    {
        ImVec2 splitterPos = ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + controlsHeight - SPLITTER_THICKNESS / 2);
        
        // Draw the splitter bar directly using the draw list
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        ImVec2 p1 = splitterPos;
        ImVec2 p2 = ImVec2(splitterPos.x + sidebarWidth, splitterPos.y + SPLITTER_THICKNESS);
        
        // Determine color based on state
        ImVec4 color = splitterColor;
        if (g_draggingVertical)
            color = splitterActiveColor;
        else if (io.MousePos.x >= p1.x && io.MousePos.x <= p2.x && 
                 io.MousePos.y >= p1.y && io.MousePos.y <= p2.y)
            color = splitterHoveredColor;
        
        drawList->AddRectFilled(p1, p2, ImGui::ColorConvertFloat4ToU32(color));
        
        // Check for hover and drag
        bool isHovered = (io.MousePos.x >= p1.x && io.MousePos.x <= p2.x && 
                          io.MousePos.y >= p1.y && io.MousePos.y <= p2.y);
        
        if (isHovered || g_draggingVertical)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        }
        
        // Start dragging
        if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            g_draggingVertical = true;
        }
        
        // Continue dragging
        if (g_draggingVertical)
        {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                float newRatio = (io.MousePos.y - viewport->WorkPos.y) / viewport->WorkSize.y;
                g_controlsHeightRatio = (std::clamp)(newRatio, MIN_PANEL_HEIGHT_RATIO, MAX_PANEL_HEIGHT_RATIO);
            }
            else
            {
                g_draggingVertical = false;
            }
        }
    }
}

static void RenderViewerPanel(const PdfViewer& viewer, const ImGuiViewport* viewport)
{
    float sidebarWidth = viewport->WorkSize.x * g_sidebarWidthRatio;
    float viewerWidth = viewport->WorkSize.x - sidebarWidth;
    float panelHeight = viewport->WorkSize.y;

    ImVec2 viewerPos = ImVec2(viewport->WorkPos.x + sidebarWidth, viewport->WorkPos.y);

    ImGui::SetNextWindowPos(viewerPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewerWidth, panelHeight), ImGuiCond_Always);

    ImGui::Begin("PDF Viewer", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

    GLuint texture = viewer.GetTexture();
    int texWidth = viewer.GetTextureWidth();
    int texHeight = viewer.GetTextureHeight();

    if (texture && texWidth > 0 && texHeight > 0)
    {
        ImVec2 availSize = ImGui::GetContentRegionAvail();

        // Calculate display size maintaining aspect ratio
        float aspectRatio = static_cast<float>(texWidth) / static_cast<float>(texHeight);
        float displayWidth = availSize.x;
        float displayHeight = displayWidth / aspectRatio;

        if (displayHeight > availSize.y)
        {
            displayHeight = availSize.y;
            displayWidth = displayHeight * aspectRatio;
        }

        // Apply zoom to display (visual zoom, not re-render)
        displayWidth *= viewer.GetZoom();
        displayHeight *= viewer.GetZoom();

        // Center the image
        float offsetX = (availSize.x - displayWidth) * 0.5f;
        if (offsetX > 0)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

        ImGui::Image((ImTextureID)(void*)(uintptr_t)texture,
                     ImVec2(displayWidth, displayHeight));
    }
    else
    {
        // Placeholder text
        ImVec2 availSize = ImGui::GetContentRegionAvail();
        const char* placeholder = "Select a PDF from the library to view";
        ImVec2 textSize = ImGui::CalcTextSize(placeholder);
        ImGui::SetCursorPos(ImVec2(
            (availSize.x - textSize.x) * 0.5f + ImGui::GetCursorPosX(),
            (availSize.y - textSize.y) * 0.5f + ImGui::GetCursorPosY()
        ));
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", placeholder);
    }

    ImGui::End();
}

// =============================================================================
// Main Application
// =============================================================================

int main(int, char**)
{
    // Initialize systems
    GLFWwindow* window = InitWindow(1280, 720, "PDF Manager");
    if (!window)
        return 1;

    InitPDFium();
    InitImGui(window, "#version 130");

    // Create application state
    PdfLibrary library;
    PdfViewer viewer;
    int selectedFileIndex = -1;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Update viewer (renders page if needed)
        viewer.Update();

        // Begin ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Create dockable workspace
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        // Render UI panels
        ImGuiIO& io = ImGui::GetIO();
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        RenderControlsPanel(viewer, io, viewport);
        RenderLibraryPanel(library, viewer, selectedFileIndex, viewport);
        RenderViewerPanel(viewer, viewport);
        RenderSplitters(io, viewport);

        // Render frame
        ImGui::Render();
        int displayW, displayH;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Handle multi-viewport
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backupContext = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backupContext);
        }

        glfwSwapBuffers(window);
    }

    // Cleanup
    viewer.Close();
    Shutdown(window);

    return 0;
}
