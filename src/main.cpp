#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <string>
#include <cstdint>
#include <vector>
#include <fstream>
#include <filesystem>

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

// OpenGL constant not always defined in basic headers
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#include <fpdfview.h>

// Windows-specific includes for file dialog
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shobjidl.h>
#endif

// Global state for PDF viewing
static FPDF_DOCUMENT g_pdfDocument = nullptr;
static FPDF_PAGE g_pdfPage = nullptr;
static GLuint g_pdfTexture = 0;
static int g_textureWidth = 0;
static int g_textureHeight = 0;
static int g_currentPage = 0;
static int g_pageCount = 0;
static std::string g_loadedFilename;
static float g_zoomLevel = 1.0f;
static bool g_needRender = true;
static std::vector<unsigned char> g_pdfData; // Keep PDF data in memory

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// UTF-16 ot UTF-8 conversion
std::string WideToUtf8(const std::wstring& wstr)
{
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// Opens a native Windows file dialog and returns the selected file path
std::string OpenFileDialog()
{
    std::string result;
#ifdef _WIN32
    wchar_t filename[MAX_PATH] = L"";
    
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = L"PDF Files (*.pdf)\0*.pdf\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Select a PDF File";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    
    if (GetOpenFileNameW(&ofn))
    {
        result = WideToUtf8(filename);
    }
#endif
    return result;
}

// Clean up current PDF resources
void ClosePDF()
{
    if (g_pdfPage)
    {
        FPDF_ClosePage(g_pdfPage);
        g_pdfPage = nullptr;
    }
    if (g_pdfDocument)
    {
        FPDF_CloseDocument(g_pdfDocument);
        g_pdfDocument = nullptr;
    }
    if (g_pdfTexture)
    {
        glDeleteTextures(1, &g_pdfTexture);
        g_pdfTexture = 0;
    }
    g_textureWidth = 0;
    g_textureHeight = 0;
    g_currentPage = 0;
    g_pageCount = 0;
    g_loadedFilename.clear();
    g_pdfData.clear();
    g_zoomLevel = 1.0f;
    g_needRender = false;
}

// Render the current page to an OpenGL texture
void RenderPageToTexture()
{
    if (!g_pdfDocument || g_currentPage < 0 || g_currentPage >= g_pageCount)
        return;

    // Close previous page if open
    if (g_pdfPage)
    {
        FPDF_ClosePage(g_pdfPage);
        g_pdfPage = nullptr;
    }

    // Load the current page
    g_pdfPage = FPDF_LoadPage(g_pdfDocument, g_currentPage);
    if (!g_pdfPage)
    {
        printf("Failed to load page %d\n", g_currentPage);
        return;
    }

    // Get page dimensions and apply zoom
    double pageWidth = FPDF_GetPageWidth(g_pdfPage);
    double pageHeight = FPDF_GetPageHeight(g_pdfPage);
    
    int renderWidth = (int)(pageWidth * g_zoomLevel);
    int renderHeight = (int)(pageHeight * g_zoomLevel);
    
    // Clamp to reasonable size
    if (renderWidth > 4096) { renderWidth = 4096; }
    if (renderHeight > 4096) { renderHeight = 4096; }

    // Allocate bitmap buffer (BGRA format)
    int stride = renderWidth * 4;
    unsigned char* buffer = new unsigned char[stride * renderHeight];
    
    // Fill with white background
    memset(buffer, 0xFF, stride * renderHeight);

    // Create PDFium bitmap pointing to our buffer
    FPDF_BITMAP bitmap = FPDFBitmap_CreateEx(renderWidth, renderHeight, FPDFBitmap_BGRA, buffer, stride);
    
    // Fill background white
    FPDFBitmap_FillRect(bitmap, 0, 0, renderWidth, renderHeight, 0xFFFFFFFF);
    
    // Render the page
    FPDF_RenderPageBitmap(bitmap, g_pdfPage, 0, 0, renderWidth, renderHeight, 0, FPDF_ANNOT);
    
    // Convert BGRA to RGBA for OpenGL
    for (int i = 0; i < renderWidth * renderHeight; i++)
    {
        unsigned char temp = buffer[i * 4];         // B
        buffer[i * 4] = buffer[i * 4 + 2];          // R -> B position
        buffer[i * 4 + 2] = temp;                   // B -> R position
    }

    // Create or update OpenGL texture
    if (g_pdfTexture == 0)
    {
        glGenTextures(1, &g_pdfTexture);
    }
    
    glBindTexture(GL_TEXTURE_2D, g_pdfTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, renderWidth, renderHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
    
    g_textureWidth = renderWidth;
    g_textureHeight = renderHeight;

    // Cleanup
    FPDFBitmap_Destroy(bitmap);
    delete[] buffer;
    
    printf("Rendered page %d/%d at %dx%d\n", g_currentPage + 1, g_pageCount, renderWidth, renderHeight);
}

// Load a PDF file
bool LoadPDF(const std::string& filepath)
{
    ClosePDF();
    
    // Read file into memory to avoid path encoding issues on Windows
    std::filesystem::path path = std::filesystem::u8path(filepath);
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        printf("Failed to open file: %s\n", filepath.c_str());
        return false;
    }
    
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    g_pdfData.resize(static_cast<size_t>(fileSize));
    if (!file.read(reinterpret_cast<char*>(g_pdfData.data()), fileSize))
    {
        printf("Failed to read file contents\n");
        g_pdfData.clear();
        return false;
    }
    file.close();
    
    // printf("Read %lld bytes from file\n", (long long)fileSize);
    
    // Load PDF from memory
    g_pdfDocument = FPDF_LoadMemDocument(g_pdfData.data(), static_cast<int>(g_pdfData.size()), nullptr);
    if (!g_pdfDocument)
    {
        unsigned long error = FPDF_GetLastError();
        printf("Failed to load PDF: error code %lu\n", error);
        g_pdfData.clear();
        return false;
    }
    
    g_pageCount = FPDF_GetPageCount(g_pdfDocument);
    g_currentPage = 0;
    
    // Extract filename for display
    size_t lastSlash = filepath.find_last_of("/\\");
    g_loadedFilename = (lastSlash != std::string::npos) ? filepath.substr(lastSlash + 1) : filepath;
        
    RenderPageToTexture();
    return true;
}

int main(int, char**)
{
    // Initialize GLFW windowing system
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "My PDF Viewer", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // Initialize PDFium library
    FPDF_LIBRARY_CONFIG config;
    config.version = 2;
    config.m_pUserFontPaths = nullptr;
    config.m_pIsolate = nullptr;
    config.m_v8EmbedderSlot = 0;
    FPDF_InitLibraryWithConfig(&config);
    printf("PDFium Initialized successfully!\n");

    // Setup ImGui context and configure features
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    // Font
    ImFont* myFont = io.Fonts->AddFontFromFileTTF("NotoSerifKR-Regular.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesKorean());
    if (myFont == NULL) {
        printf("Failed to load font! loading default instead.\n");
        io.Fonts->AddFontDefault();
    }
    ImGui::PushFont(myFont);

    ImGui::StyleColorsDark();

    // Adjust style for multi-viewport support
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Initialize ImGui backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Main application loop
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Only re-render PDF page if needed
        if (g_needRender && g_pdfDocument) {
            RenderPageToTexture();
            g_needRender = false;
        }

        // Begin ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Create dockable workspace
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        // Control Panel
        ImGui::Begin("PDF Controls");
        
        // Open PDF button
        if (ImGui::Button("Open PDF...", ImVec2(120, 30)))
        {
            std::string filepath = OpenFileDialog();
            if (!filepath.empty())
            {
                LoadPDF(filepath);
            }
        }
        
        ImGui::Separator();
        
        if (g_pdfDocument)
        {
            ImGui::Text("File: %s", g_loadedFilename.c_str());
            ImGui::Text("Page: %d / %d", g_currentPage + 1, g_pageCount);
            
            // Navigation buttons
            ImGui::BeginDisabled(g_currentPage <= 0);
            if (ImGui::Button("< Prev"))
            {
                g_currentPage--;
                g_needRender = true;
            }
            ImGui::EndDisabled();
            
            ImGui::SameLine();
            
            ImGui::BeginDisabled(g_currentPage >= g_pageCount - 1);
            if (ImGui::Button("Next >"))
            {
                g_currentPage++;
                g_needRender = true;
            }
            ImGui::EndDisabled();
            
            // Zoom controls
            ImGui::Separator();
            ImGui::Text("Zoom: %.0f%%", g_zoomLevel * 100.0f);
            
            if (ImGui::Button("-")) { g_zoomLevel = (g_zoomLevel > 0.25f) ? g_zoomLevel - 0.25f : 0.25f; g_needRender = true; }
            ImGui::SameLine();
            if (ImGui::Button("+")) { g_zoomLevel = (g_zoomLevel < 4.0f) ? g_zoomLevel + 0.25f : 4.0f; g_needRender = true; }
            ImGui::SameLine();
            if (ImGui::Button("Reset")) { g_zoomLevel = 1.0f; g_needRender = true; }

            if (io.KeyCtrl && io.MouseWheel != 0.0f) {
                if (io.MouseWheel > 0) {
                    g_zoomLevel *= 1.1f;
                }
                else {
                    g_zoomLevel /= 1.1f;
                }
                if (g_zoomLevel < 0.1f){
                    g_zoomLevel = 0.1f;
                }
                if (g_zoomLevel > 5.0f){
                    g_zoomLevel = 5.0f;
                }
                g_needRender = true;
            }
            
            ImGui::Separator();
            if (ImGui::Button("Close PDF"))
            {
                ClosePDF();
            }
        }
        else
        {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No PDF loaded");
        }
        
        ImGui::Separator();
        ImGui::Text("FPS: %.1f", io.Framerate);
        ImGui::End();

        // PDF Viewer Window
        ImGui::Begin("PDF Viewer");
        if (g_pdfTexture && g_textureWidth > 0 && g_textureHeight > 0)
        {
            // Get available size in the window
            ImVec2 availSize = ImGui::GetContentRegionAvail();
            
            // Calculate size maintaining aspect ratio
            float aspectRatio = (float)g_textureWidth / (float)g_textureHeight;
            float displayWidth = availSize.x;
            float displayHeight = displayWidth / aspectRatio;
            
            if (displayHeight > availSize.y)
            {
                displayHeight = availSize.y;
                displayWidth = displayHeight * aspectRatio;
            }

            displayWidth *= g_zoomLevel;
            displayHeight *= g_zoomLevel;
            
            // Center the image
            float offsetX = (availSize.x - displayWidth) * 0.5f;
            if (offsetX > 0)
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
            
            ImGui::Image((ImTextureID)(intptr_t)g_pdfTexture, ImVec2(displayWidth, displayHeight));
        }
        else
        {
            ImVec2 availSize = ImGui::GetContentRegionAvail();
            ImVec2 textSize = ImGui::CalcTextSize("Click 'Open PDF...' to load a document");
            ImGui::SetCursorPos(ImVec2(
                (availSize.x - textSize.x) * 0.5f + ImGui::GetCursorPosX(),
                (availSize.y - textSize.y) * 0.5f + ImGui::GetCursorPosY()
            ));
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Click 'Open PDF...' to load a document");
        }
        ImGui::End();

        // Render frame
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Handle multi-viewport rendering
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);
    }

    // Cleanup resources
    ClosePDF();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    FPDF_DestroyLibrary();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
