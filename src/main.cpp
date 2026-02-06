/**
 * PDF Viewer Application
 *
 * A simple PDF viewer built with ImGui, GLFW, OpenGL, and PDFium.
 */

#include <GLFW/glfw3.h>
#include <fpdfview.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>

#include "file_dialog.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "pdf_library.h"
#include "pdf_viewer.h"
#include "setlist_gen.h"

// =============================================================================
// Error Callbacks
// =============================================================================

static void GlfwErrorCallback(int error, const char *description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// =============================================================================
// Application Initialization
// =============================================================================

static GLFWwindow *InitWindow(int width, int height, const char *title)
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

static void InitPDFium()
{
    FPDF_LIBRARY_CONFIG config;
    config.version = 2;
    config.m_pUserFontPaths = nullptr;
    config.m_pIsolate = nullptr;
    config.m_v8EmbedderSlot = 0;
    FPDF_InitLibraryWithConfig(&config);
}

static void InitImGui(GLFWwindow *window, const char *glslVersion)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    // Load custom font with Korean support
    ImFont *font = io.Fonts->AddFontFromFileTTF(
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
        ImGuiStyle &style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glslVersion);
}

static void Shutdown(GLFWwindow *window)
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

static void RenderLibraryPanel(PdfLibrary &library,
                               PdfViewer &viewer,
                               SetlistManager &setlistManager,
                               int &selectedIndex,
                               int &selectedSetlistIndex,
                               int &selectedSetlistItemIndex,
                               const ImGuiViewport *viewport)
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
            setlistManager.Deactivate();
            viewer.Close();
        }
    }

    if (library.IsLoaded())
    {
        if (ImGui::BeginTabBar("File Bar"))
        {
            //  All files tab
            if (ImGui::BeginTabItem("All Files"))
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

                    const auto &files = library.GetFiles();
                    for (size_t i = 0; i < files.size(); i++)
                    {
                        const auto &entry = files[i];
                        bool isSelected =
                            (static_cast<int>(i) == selectedIndex);

                        // Highlight the currently selected/open file
                        if (isSelected)
                        {
                            ImGui::PushStyleColor(
                                ImGuiCol_Header,
                                ImVec4(0.3f, 0.5f, 0.7f, 1.0f));
                            ImGui::PushStyleColor(
                                ImGuiCol_HeaderHovered,
                                ImVec4(0.4f, 0.6f, 0.8f, 1.0f));
                            ImGui::PopStyleColor(2);
                        }

                        if (ImGui::Selectable(entry.filename.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick))
                        {
                            selectedIndex = static_cast<int>(i);

                            if (viewer.GetFilename() != entry.filename)
                            {
                                setlistManager.Deactivate();
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

                ImGui::EndTabItem();
            }
            //  Tab for file set addition (always visible when library is loaded)
            if (ImGui::BeginTabItem("Setlists"))
            {
                // --- Create new setlist ---
                static char newSetlistName[64] = "";
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 70.0f);
                bool enterPressed = ImGui::InputText("##NewName", newSetlistName,
                    IM_ARRAYSIZE(newSetlistName),
                    ImGuiInputTextFlags_EnterReturnsTrue);
                ImGui::SameLine();
                if (ImGui::Button("Create", ImVec2(-1, 0)) || enterPressed)
                {
                    size_t newIndex = setlistManager.CreateSetlist(newSetlistName);
                    selectedSetlistIndex = static_cast<int>(newIndex);
                    selectedSetlistItemIndex = -1;
                    newSetlistName[0] = '\0';
                }

                // Save / Load buttons
                {
                    static bool showSaveStatus = false;
                    static bool saveOk = false;
                    static float statusTimer = 0.0f;

                    float halfWidth = (ImGui::GetContentRegionAvail().x
                        - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

                    ImGui::BeginDisabled(setlistManager.GetSetlistCount() == 0);
                    if (ImGui::Button("Save", ImVec2(halfWidth, 0)))
                    {
                        saveOk = setlistManager.SaveToFile(
                            SetlistManager::GetDefaultSavePath());
                        showSaveStatus = true;
                        statusTimer = 2.0f;
                    }
                    ImGui::EndDisabled();

                    ImGui::SameLine();

                    if (ImGui::Button("Load", ImVec2(-1, 0)))
                    {
                        if (setlistManager.LoadFromFile(
                                SetlistManager::GetDefaultSavePath()))
                        {
                            selectedSetlistIndex = setlistManager.GetSetlistCount() > 0 ? 0 : -1;
                            selectedSetlistItemIndex = -1;
                            saveOk = true;
                        }
                        else
                        {
                            saveOk = false;
                        }
                        showSaveStatus = true;
                        statusTimer = 2.0f;
                    }

                    if (showSaveStatus)
                    {
                        ImVec4 color = saveOk
                            ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f)
                            : ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
                        const char *msg = saveOk ? "OK!" : "Failed";
                        ImGui::SameLine();
                        ImGui::TextColored(color, "%s", msg);
                        statusTimer -= ImGui::GetIO().DeltaTime;
                        if (statusTimer <= 0.0f)
                            showSaveStatus = false;
                    }
                }

                ImGui::Separator();

                // --- Setlist list ---
                bool isActiveSetlist = setlistManager.IsActive() &&
                    selectedSetlistIndex == setlistManager.GetActiveSetlistIndex();

                float setlistListHeight = 100.0f;
                ImGui::BeginChild("SetlistList", ImVec2(0, setlistListHeight), true);
                const auto &setlists = setlistManager.GetSetlists();
                for (size_t i = 0; i < setlists.size(); i++)
                {
                    const Setlist &sl = setlists[i];
                    bool isSel = (static_cast<int>(i) == selectedSetlistIndex);
                    bool isThisActive = setlistManager.IsActive() &&
                        setlistManager.GetActiveSetlistIndex() == static_cast<int>(i);

                    // Build display label with active indicator + unique ID
                    std::string displayLabel;
                    if (isThisActive)
                        displayLabel = "[ACTIVE] ";
                    displayLabel += sl.GetName();
                    displayLabel += " (" + std::to_string(sl.GetItemCount()) + ")";
                    displayLabel += "##setlist_" + std::to_string(i);

                    if (ImGui::Selectable(displayLabel.c_str(), isSel))
                    {
                        selectedSetlistIndex = static_cast<int>(i);
                        selectedSetlistItemIndex = -1;
                    }
                }
                ImGui::EndChild();

                // --- Setlist action buttons ---
                Setlist *selectedSetlist = nullptr;
                if (selectedSetlistIndex >= 0 &&
                    selectedSetlistIndex < static_cast<int>(setlistManager.GetSetlistCount()))
                {
                    selectedSetlist = setlistManager.GetSetlist(
                        static_cast<size_t>(selectedSetlistIndex));
                }

                {
                    bool canActivate = selectedSetlist &&
                                       selectedSetlist->GetItemCount() > 0;
                    float halfWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

                    ImGui::BeginDisabled(!canActivate);
                    if (ImGui::Button(isActiveSetlist ? "Reactivate" : "Activate",
                                      ImVec2(halfWidth, 0)))
                    {
                        setlistManager.ActivateSetlist(
                            static_cast<size_t>(selectedSetlistIndex), viewer);
                    }
                    ImGui::EndDisabled();

                    ImGui::SameLine();

                    ImGui::BeginDisabled(selectedSetlistIndex < 0);
                    if (ImGui::Button("Remove", ImVec2(-1, 0)))
                    {
                        size_t removeIdx = static_cast<size_t>(selectedSetlistIndex);
                        // Adjust selectedSetlistIndex before removal
                        int newSelected = -1;
                        if (static_cast<int>(setlistManager.GetSetlistCount()) > 1)
                        {
                            if (selectedSetlistIndex > 0)
                                newSelected = selectedSetlistIndex - 1;
                            else
                                newSelected = 0;
                        }
                        setlistManager.RemoveSetlist(removeIdx);
                        selectedSetlistIndex = newSelected;
                        selectedSetlistItemIndex = -1;
                        // Refresh pointer after removal
                        selectedSetlist = nullptr;
                        if (selectedSetlistIndex >= 0)
                        {
                            selectedSetlist = setlistManager.GetSetlist(
                                static_cast<size_t>(selectedSetlistIndex));
                        }
                    }
                    ImGui::EndDisabled();
                }

                ImGui::Separator();

                // --- Selected setlist contents ---
                if (selectedSetlist)
                {
                    ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f),
                        "%s  (%zu items)", selectedSetlist->GetName().c_str(),
                        selectedSetlist->GetItemCount());

                    // Add file from dropdown
                    bool hasFiles = library.GetFileCount() > 0;
                    ImGui::BeginDisabled(!hasFiles);
                    {
                        const auto &files = library.GetFiles();
                        float addBtnWidth = 45.0f;
                        ImGui::SetNextItemWidth(
                            ImGui::GetContentRegionAvail().x - addBtnWidth
                            - ImGui::GetStyle().ItemSpacing.x);

                        static int comboFileIndex = 0;
                        if (comboFileIndex >= static_cast<int>(files.size()))
                            comboFileIndex = 0;

                        const char *previewName = files.empty()
                            ? "No files" : files[static_cast<size_t>(comboFileIndex)].filename.c_str();
                        if (ImGui::BeginCombo("##AddFileCombo", previewName))
                        {
                            for (size_t i = 0; i < files.size(); i++)
                            {
                                bool selected = (static_cast<int>(i) == comboFileIndex);
                                std::string comboLabel = files[i].filename
                                    + "##combo_" + std::to_string(i);
                                if (ImGui::Selectable(comboLabel.c_str(), selected))
                                {
                                    comboFileIndex = static_cast<int>(i);
                                }
                                if (selected)
                                    ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Add", ImVec2(-1, 0)))
                        {
                            if (comboFileIndex >= 0 &&
                                comboFileIndex < static_cast<int>(files.size()))
                            {
                                selectedSetlist->AddItem(
                                    files[static_cast<size_t>(comboFileIndex)]);
                            }
                        }
                    }
                    ImGui::EndDisabled();

                    // Items list — use all remaining space minus button area
                    float buttonAreaHeight = ImGui::GetFrameHeightWithSpacing() * 3.0f + 4.0f;
                    float itemsHeight = ImGui::GetContentRegionAvail().y - buttonAreaHeight;
                    if (itemsHeight < 60.0f)
                        itemsHeight = 60.0f;

                    ImGui::BeginChild("SetlistItems", ImVec2(0, itemsHeight), true);
                    const auto &items = selectedSetlist->GetItems();
                    for (size_t i = 0; i < items.size(); i++)
                    {
                        const SetlistItem &item = items[i];
                        bool isSel = (static_cast<int>(i) == selectedSetlistItemIndex);
                        bool isThisPlaying = setlistManager.IsActive() &&
                            setlistManager.GetActiveSetlistIndex() == selectedSetlistIndex &&
                            setlistManager.GetActiveItemIndex() == static_cast<int>(i);

                        // Color playing item
                        if (isThisPlaying)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text,
                                ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
                        }

                        std::string displayName =
                            std::to_string(i + 1) + ". " + item.name;
                        if (isThisPlaying)
                            displayName = "> " + displayName;

                        std::string label =
                            displayName + "##setlist_item_" + std::to_string(i);

                        if (ImGui::Selectable(label.c_str(), isSel,
                                              ImGuiSelectableFlags_AllowDoubleClick))
                        {
                            selectedSetlistItemIndex = static_cast<int>(i);
                            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                            {
                                setlistManager.JumpToItem(
                                    static_cast<size_t>(selectedSetlistIndex),
                                    i, viewer);
                            }
                        }

                        if (isThisPlaying)
                            ImGui::PopStyleColor();

                        if (ImGui::IsItemHovered())
                        {
                            ImGui::SetTooltip("%s", item.fullPath.c_str());
                        }
                    }
                    ImGui::EndChild();

                    // --- Item action buttons (compact layout) ---
                    bool hasItemSelected = selectedSetlistItemIndex >= 0 &&
                        selectedSetlistItemIndex < static_cast<int>(selectedSetlist->GetItemCount());
                    float thirdWidth = (ImGui::GetContentRegionAvail().x
                        - ImGui::GetStyle().ItemSpacing.x * 2.0f) / 3.0f;

                    // Row 1: Open | Move Up | Move Down
                    ImGui::BeginDisabled(!hasItemSelected);
                    if (ImGui::Button("Open", ImVec2(thirdWidth, 0)))
                    {
                        setlistManager.JumpToItem(
                            static_cast<size_t>(selectedSetlistIndex),
                            static_cast<size_t>(selectedSetlistItemIndex),
                            viewer);
                    }
                    ImGui::SameLine();
                    {
                        bool canMoveUp = hasItemSelected && selectedSetlistItemIndex > 0;
                        ImGui::BeginDisabled(!canMoveUp);
                        if (ImGui::Button("Up", ImVec2(thirdWidth, 0)))
                        {
                            selectedSetlist->MoveItem(
                                static_cast<size_t>(selectedSetlistItemIndex),
                                static_cast<size_t>(selectedSetlistItemIndex - 1));
                            selectedSetlistItemIndex--;
                        }
                        ImGui::EndDisabled();
                    }
                    ImGui::SameLine();
                    {
                        bool canMoveDown = hasItemSelected &&
                            selectedSetlistItemIndex + 1 < static_cast<int>(selectedSetlist->GetItemCount());
                        ImGui::BeginDisabled(!canMoveDown);
                        if (ImGui::Button("Down", ImVec2(-1, 0)))
                        {
                            selectedSetlist->MoveItem(
                                static_cast<size_t>(selectedSetlistItemIndex),
                                static_cast<size_t>(selectedSetlistItemIndex + 1));
                            selectedSetlistItemIndex++;
                        }
                        ImGui::EndDisabled();
                    }

                    // Row 2: Remove Item | Clear All
                    float halfWidth = (ImGui::GetContentRegionAvail().x
                        - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
                    if (ImGui::Button("Remove Item", ImVec2(halfWidth, 0)))
                    {
                        selectedSetlist->RemoveItem(
                            static_cast<size_t>(selectedSetlistItemIndex));
                        if (selectedSetlistItemIndex >= static_cast<int>(selectedSetlist->GetItemCount()))
                            selectedSetlistItemIndex = static_cast<int>(selectedSetlist->GetItemCount()) - 1;
                    }
                    ImGui::EndDisabled();

                    ImGui::SameLine();

                    ImGui::BeginDisabled(selectedSetlist->GetItemCount() == 0);
                    if (ImGui::Button("Clear All", ImVec2(-1, 0)))
                    {
                        selectedSetlist->Clear();
                        selectedSetlistItemIndex = -1;
                    }
                    ImGui::EndDisabled();
                }
                else
                {
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                                       "Create or select a setlist above.");
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    else
    {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Select a folder to browse PDFs");
    }

    ImGui::End();
}

static void RenderControlsPanel(PdfViewer &viewer,
                                SetlistManager &setlistManager,
                                const ImGuiIO &io,
                                const ImGuiViewport *viewport)
{
    float sidebarWidth = viewport->WorkSize.x * g_sidebarWidthRatio;
    float controlsHeight = viewport->WorkSize.y * g_controlsHeightRatio;

    ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(sidebarWidth, controlsHeight), ImGuiCond_Always);

    ImGui::Begin("PDF Controls", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

    if (setlistManager.IsActive())
    {
        const Setlist *activeSetlist =
            setlistManager.GetSetlist(static_cast<size_t>(
                setlistManager.GetActiveSetlistIndex()));

        if (activeSetlist)
        {
            ImGui::PushStyleColor(ImGuiCol_Text,
                ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
            ImGui::Text("Setlist Mode");
            ImGui::PopStyleColor();
            ImGui::TextWrapped("%s", activeSetlist->GetName().c_str());
            ImGui::Text("Item %d / %zu",
                setlistManager.GetActiveItemIndex() + 1,
                activeSetlist->GetItemCount());
            if (ImGui::Button("Deactivate Setlist", ImVec2(-1, 0)))
            {
                setlistManager.Deactivate();
            }
            ImGui::Separator();
        }
    }

    if (viewer.IsLoaded())
    {
        // File info
        ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "Current File:");
        ImGui::TextWrapped("%s", viewer.GetFilename().c_str());

        ImGui::Separator();

        // Page navigation
        ImGui::Text("Page: %d / %d", viewer.GetCurrentPage() + 1, viewer.GetPageCount());

        // Navigation buttons
        bool canGoPrevious = setlistManager.IsActive()
                                 ? setlistManager.CanGoPrevious(viewer)
                                 : viewer.CanGoPrevious();
        bool canGoNext = setlistManager.IsActive()
                             ? setlistManager.CanGoNext(viewer)
                             : viewer.CanGoNext();

        ImGui::BeginDisabled(!canGoPrevious);
        if (ImGui::Button("< Prev", ImVec2(80, 0)) ||
            ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
        {
            if (setlistManager.IsActive())
                setlistManager.Previous(viewer);
            else
                viewer.PreviousPage();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();

        ImGui::BeginDisabled(!canGoNext);
        if (ImGui::Button("Next >", ImVec2(80, 0)) || ImGui::IsKeyPressed(ImGuiKey_RightArrow))
        {
            if (setlistManager.IsActive())
                setlistManager.Next(viewer);
            else
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
static void RenderSplitters(ImGuiIO &io, const ImGuiViewport *viewport)
{
    float sidebarWidth = viewport->WorkSize.x * g_sidebarWidthRatio;
    float controlsHeight = viewport->WorkSize.y * g_controlsHeightRatio;

    // Colors for splitters
    ImVec4 splitterColor = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
    ImVec4 splitterHoveredColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    ImVec4 splitterActiveColor = ImVec4(0.6f, 0.7f, 0.9f, 1.0f);

    // Horizontal splitter (resize sidebar width) - vertical bar on right edge
    // of sidebar
    {
        ImVec2 splitterPos =
            ImVec2(viewport->WorkPos.x + sidebarWidth - SPLITTER_THICKNESS / 2, viewport->WorkPos.y);

        // Draw the splitter bar directly using the draw list
        ImDrawList *drawList = ImGui::GetForegroundDrawList();
        ImVec2 p1 = splitterPos;
        ImVec2 p2 = ImVec2(splitterPos.x + SPLITTER_THICKNESS, splitterPos.y + viewport->WorkSize.y);

        // Determine color based on state
        ImVec4 color = splitterColor;
        if (g_draggingHorizontal)
            color = splitterActiveColor;
        else if (io.MousePos.x >= p1.x && io.MousePos.x <= p2.x && io.MousePos.y >= p1.y && io.MousePos.y <= p2.y)
            color = splitterHoveredColor;

        drawList->AddRectFilled(p1, p2, ImGui::ColorConvertFloat4ToU32(color));

        // Check for hover and drag
        bool isHovered = (io.MousePos.x >= p1.x && io.MousePos.x <= p2.x && io.MousePos.y >= p1.y && io.MousePos.y <= p2.y);

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

    // Vertical splitter (resize controls/library height split) - horizontal bar
    // between panels
    {
        ImVec2 splitterPos = ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + controlsHeight - SPLITTER_THICKNESS / 2);

        // Draw the splitter bar directly using the draw list
        ImDrawList *drawList = ImGui::GetForegroundDrawList();
        ImVec2 p1 = splitterPos;
        ImVec2 p2 = ImVec2(splitterPos.x + sidebarWidth, splitterPos.y + SPLITTER_THICKNESS);

        // Determine color based on state
        ImVec4 color = splitterColor;
        if (g_draggingVertical)
            color = splitterActiveColor;
        else if (io.MousePos.x >= p1.x && io.MousePos.x <= p2.x && io.MousePos.y >= p1.y && io.MousePos.y <= p2.y)
            color = splitterHoveredColor;

        drawList->AddRectFilled(p1, p2, ImGui::ColorConvertFloat4ToU32(color));

        // Check for hover and drag
        bool isHovered = (io.MousePos.x >= p1.x && io.MousePos.x <= p2.x && io.MousePos.y >= p1.y && io.MousePos.y <= p2.y);

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

static void RenderViewerPanel(const PdfViewer &viewer,
                              const ImGuiViewport *viewport)
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

        ImGui::Image((ImTextureID)(void *)(uintptr_t)texture, ImVec2(displayWidth, displayHeight));
    }
    else
    {
        // Placeholder text
        ImVec2 availSize = ImGui::GetContentRegionAvail();
        const char *placeholder = "Select a PDF from the library to view";
        ImVec2 textSize = ImGui::CalcTextSize(placeholder);
        ImGui::SetCursorPos(
            ImVec2((availSize.x - textSize.x) * 0.5f + ImGui::GetCursorPosX(),
                   (availSize.y - textSize.y) * 0.5f + ImGui::GetCursorPosY()));
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", placeholder);
    }

    ImGui::End();
}

// =============================================================================
// Main Application
// =============================================================================

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
        ImGui::NewFrame();

        // Create dockable workspace
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        // Render UI panels
        ImGuiIO &io = ImGui::GetIO();
        ImGuiViewport *viewport = ImGui::GetMainViewport();
        RenderControlsPanel(viewer, setlistManager, io, viewport);
        RenderLibraryPanel(library, viewer, setlistManager,
                           selectedFileIndex, selectedSetlistIndex,
                           selectedSetlistItemIndex, viewport);
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
            GLFWwindow *backupContext = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backupContext);
        }

        glfwSwapBuffers(window);
    }

    // Auto-save setlists on exit
    if (setlistManager.GetSetlistCount() > 0)
    {
        std::string savePath = SetlistManager::GetDefaultSavePath();
        if (setlistManager.SaveToFile(savePath))
            printf("[App] Saved setlists to %s\n", savePath.c_str());
    }

    // Cleanup
    viewer.Close();
    Shutdown(window);

    return 0;
}
