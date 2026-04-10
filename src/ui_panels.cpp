#include "ui_panels.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

#include "file_dialog.h"
#include "imgui.h"
#include "pdf_library.h"
#include "pdf_viewer.h"
#include "setlist_gen.h"
#include "ui_helpers.h"

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
// Right sidebar (notes panel) width ratio
static float g_notesSidebarWidthRatio = 0.20f;
static const float MIN_NOTES_SIDEBAR_RATIO = 0.10f;
static const float MAX_NOTES_SIDEBAR_RATIO = 0.40f;
// Splitter drag state
static bool g_draggingHorizontal = false;
static bool g_draggingVertical = false;
static bool g_draggingRightSplitter = false;
static const float SPLITTER_THICKNESS = 6.0f;

// =============================================================================
// UI Rendering
// =============================================================================

void RenderLibraryPanel(PdfLibrary &library,
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

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 7.0f));
    // Folder selection button
    if (PrimaryButton("Open Folder...", ImVec2(-1, 32)))
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
    ImGui::PopStyleVar();

    if (ImGui::BeginTabBar("File Bar"))
    {
        //  All files tab
        if (ImGui::BeginTabItem("All Files"))
        {
            if (!library.IsLoaded()){
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Select a folder to browse PDFs");
            }
            else {
                if (ImGui::Button("Refresh Library", ImVec2(-1, 0)))
                {
                    library.Refresh();
                }
                SectionTitle("Library");

                ImGui::BeginChild("FolderInfoCard", ImVec2(0, 47), true);
                ImGui::TextColored(ImVec4(0.71f, 0.86f, 1.0f, 1.0f), "%s", library.GetFolderName().c_str());
                ImGui::EndChild();

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
                                ImVec4(0.30f, 0.46f, 0.66f, 0.90f));
                            ImGui::PushStyleColor(
                                ImGuiCol_HeaderHovered,
                                ImVec4(0.37f, 0.55f, 0.78f, 0.95f));
                            ImGui::PushStyleColor(
                                ImGuiCol_HeaderActive,
                                ImVec4(0.42f, 0.62f, 0.86f, 1.0f));
                        }

                        if (ImGui::Selectable(entry.filename.c_str(), isSelected,
                                                ImGuiSelectableFlags_AllowDoubleClick))
                        {
                            selectedIndex = static_cast<int>(i);

                            if (viewer.GetFilename() != entry.filename)
                            {
                                setlistManager.Deactivate();
                                viewer.Load(entry.fullPath);
                            }
                        }
                        if (isSelected)
                            ImGui::PopStyleColor(3);

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
                    ImGui::TextColored(ImVec4(0.62f, 0.64f, 0.70f, 1.0f), "No PDF files found");
                }
            }

            ImGui::EndTabItem();
        }
        //  Tab for file set addition (always visible when library is loaded)
        if (ImGui::BeginTabItem("Setlists"))
        {
            SectionTitle("Create Setlist");
            // --- Create new setlist ---
            static char newSetlistName[64] = "";
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 70.0f);
            bool enterPressed = ImGui::InputText("##NewName", newSetlistName,
                IM_ARRAYSIZE(newSetlistName),
                ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            if (PrimaryButton("Create", ImVec2(-1, 0)) || enterPressed)
            {
                size_t newIndex = setlistManager.CreateSetlist(newSetlistName);
                selectedSetlistIndex = static_cast<int>(newIndex);
                selectedSetlistItemIndex = -1;
                newSetlistName[0] = '\0';
            }

            // Save / Load buttons
            SectionTitle("Persistence");
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

            // --- Setlist list ---
            SectionTitle("Setlists");
            bool isActiveSetlist = setlistManager.IsActive() &&
                selectedSetlistIndex == setlistManager.GetActiveSetlistIndex();

            float setlistListHeight = 120.0f;
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
                if (PrimaryButton(isActiveSetlist ? "Reactivate" : "Activate",
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

            SectionTitle("Setlist Items");

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
                    if (PrimaryButton("Add", ImVec2(-1, 0)))
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
                if (PrimaryButton("Open", ImVec2(thirdWidth, 0)))
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

    ImGui::End();
}

void RenderControlsPanel(PdfViewer &viewer,
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
            SectionTitle("Playback");
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.20f, 0.17f, 0.80f));
            ImGui::BeginChild("ActiveSetlistCard", ImVec2(0, 90), true);
            ImGui::TextColored(ImVec4(0.52f, 0.95f, 0.63f, 1.0f), "Setlist mode is active");
            ImGui::TextWrapped("%s", activeSetlist->GetName().c_str());
            ImGui::TextDisabled("Item %d / %zu",
                                setlistManager.GetActiveItemIndex() + 1,
                                activeSetlist->GetItemCount());
            if (ImGui::Button("Deactivate Setlist", ImVec2(-1, 0)))
            {
                setlistManager.Deactivate();
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
    }

    if (viewer.IsLoaded())
    {
        SectionTitle("Document");
        ImGui::TextColored(ImVec4(0.71f, 0.86f, 1.0f, 1.0f), "Current File");
        ImGui::TextWrapped("%s", viewer.GetFilename().c_str());
        ImGui::TextDisabled("%d pages", viewer.GetPageCount());

        SectionTitle("Navigation");
        ImGui::Text("Page: %d / %d", viewer.GetCurrentPage() + 1, viewer.GetPageCount());

        bool canGoPrevious = setlistManager.IsActive()
                                 ? setlistManager.CanGoPrevious(viewer)
                                 : viewer.CanGoPrevious();
        bool canGoNext = setlistManager.IsActive()
                             ? setlistManager.CanGoNext(viewer)
                             : viewer.CanGoNext();
        float halfWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

        ImGui::BeginDisabled(!canGoPrevious);
        if (PrimaryButton("< Prev", ImVec2(halfWidth, 0)) ||
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
        if (PrimaryButton("Next >", ImVec2(-1, 0)) ||
            ImGui::IsKeyPressed(ImGuiKey_RightArrow))
        {
            if (setlistManager.IsActive())
                setlistManager.Next(viewer);
            else
                viewer.NextPage();
        }
        ImGui::EndDisabled();

        SectionTitle("Zoom");
        ImGui::Text("Zoom: %.0f%%", viewer.GetZoom() * 100.0f);
        float thirdWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2.0f) / 3.0f;

        if (ImGui::Button("-", ImVec2(thirdWidth, 0)))
            viewer.ZoomOut();
        ImGui::SameLine();
        if (ImGui::Button("+", ImVec2(thirdWidth, 0)))
            viewer.ZoomIn();
        ImGui::SameLine();
        if (ImGui::Button("Reset", ImVec2(-1, 0)))
            viewer.ResetZoom();

        // Ctrl+Scroll zoom
        if (io.KeyCtrl && io.MouseWheel != 0.0f)
        {
            if (io.MouseWheel > 0)
                viewer.ZoomIn(1.1f);
            else
                viewer.ZoomOut(1.1f);
        }

        SectionTitle("Actions");
        if (ImGui::Button("Close PDF", ImVec2(-1, 0)))
            viewer.Close();
    }
    else
    {
        SectionTitle("Document");
        ImGui::TextColored(ImVec4(0.62f, 0.64f, 0.70f, 1.0f), "No PDF loaded");
        ImGui::TextWrapped("Select a PDF from the library panel to view it.");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("FPS: %.1f", io.Framerate);

    ImGui::End();
}

// Manual splitters for sidebar width, controls/library height, and notes sidebar
void RenderSplitters(ImGuiIO &io, const ImGuiViewport *viewport,
                     bool showRightSplitter)
{
    float sidebarWidth = viewport->WorkSize.x * g_sidebarWidthRatio;
    float controlsHeight = viewport->WorkSize.y * g_controlsHeightRatio;

    // Colors for splitters
    ImVec4 splitterColor = ImVec4(0.20f, 0.25f, 0.32f, 0.85f);
    ImVec4 splitterHoveredColor = ImVec4(0.31f, 0.40f, 0.52f, 0.95f);
    ImVec4 splitterActiveColor = ImVec4(0.41f, 0.55f, 0.73f, 1.00f);

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

    // Right splitter (resize notes sidebar width) — only when notes panel is visible
    if (showRightSplitter || g_draggingRightSplitter)
    {
        float notesSidebarWidth = viewport->WorkSize.x * g_notesSidebarWidthRatio;
        float splitterX = viewport->WorkPos.x + viewport->WorkSize.x
                          - notesSidebarWidth - SPLITTER_THICKNESS / 2;

        ImDrawList *drawList = ImGui::GetForegroundDrawList();
        ImVec2 p1 = ImVec2(splitterX, viewport->WorkPos.y);
        ImVec2 p2 = ImVec2(splitterX + SPLITTER_THICKNESS,
                            viewport->WorkPos.y + viewport->WorkSize.y);

        ImVec4 color = splitterColor;
        if (g_draggingRightSplitter)
            color = splitterActiveColor;
        else if (io.MousePos.x >= p1.x && io.MousePos.x <= p2.x &&
                 io.MousePos.y >= p1.y && io.MousePos.y <= p2.y)
            color = splitterHoveredColor;

        drawList->AddRectFilled(p1, p2, ImGui::ColorConvertFloat4ToU32(color));

        bool isHovered = (io.MousePos.x >= p1.x && io.MousePos.x <= p2.x &&
                          io.MousePos.y >= p1.y && io.MousePos.y <= p2.y);

        if (isHovered || g_draggingRightSplitter)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

        if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            g_draggingRightSplitter = true;

        if (g_draggingRightSplitter)
        {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                float rightEdge = viewport->WorkPos.x + viewport->WorkSize.x;
                float newRatio = (rightEdge - io.MousePos.x) / viewport->WorkSize.x;
                g_notesSidebarWidthRatio = (std::clamp)(newRatio,
                    MIN_NOTES_SIDEBAR_RATIO, MAX_NOTES_SIDEBAR_RATIO);
            }
            else
            {
                g_draggingRightSplitter = false;
            }
        }
    }
}

void RenderNotesPanel(SetlistManager &setlistManager,
                      const ImGuiViewport *viewport)
{
    if (!setlistManager.IsActive())
        return;

    Setlist *mutSetlist = setlistManager.GetSetlist(
        static_cast<size_t>(setlistManager.GetActiveSetlistIndex()));
    int activeIdx = setlistManager.GetActiveItemIndex();
    if (!mutSetlist || activeIdx < 0 ||
        activeIdx >= static_cast<int>(mutSetlist->GetItemCount()))
        return;

    float notesSidebarWidth = viewport->WorkSize.x * g_notesSidebarWidthRatio;
    float panelHeight = viewport->WorkSize.y;
    float notesX = viewport->WorkPos.x + viewport->WorkSize.x - notesSidebarWidth;

    ImGui::SetNextWindowPos(ImVec2(notesX, viewport->WorkPos.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(notesSidebarWidth, panelHeight), ImGuiCond_Always);

    ImGui::Begin("Notes", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoResize);

    const SetlistItem &item = mutSetlist->GetItems()[static_cast<size_t>(activeIdx)];
    ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "%s", item.name.c_str());
    ImGui::TextDisabled("Item %d / %zu", activeIdx + 1, mutSetlist->GetItemCount());
    ImGui::Separator();

    static char notesBuf[4096] = "";
    static int lastActiveSetlist = -1;
    static int lastActiveItem = -1;

    // Sync buffer when the active item changes
    int curSetlist = setlistManager.GetActiveSetlistIndex();
    if (curSetlist != lastActiveSetlist || activeIdx != lastActiveItem)
    {
        const std::string &notes =
            mutSetlist->GetItemNotes(static_cast<size_t>(activeIdx));
        size_t copyLen = notes.size();
        if (copyLen >= sizeof(notesBuf))
            copyLen = sizeof(notesBuf) - 1;
        std::memcpy(notesBuf, notes.c_str(), copyLen);
        notesBuf[copyLen] = '\0';
        lastActiveSetlist = curSetlist;
        lastActiveItem = activeIdx;
    }

    ImVec2 notesSize = ImVec2(-1, ImGui::GetContentRegionAvail().y);
    if (ImGui::InputTextMultiline("##ItemNotes", notesBuf, sizeof(notesBuf),
                                  notesSize))
    {
        mutSetlist->SetItemNotes(static_cast<size_t>(activeIdx),
                                 std::string(notesBuf));
    }

    ImGui::End();
}

void RenderViewerPanel(const PdfViewer &viewer,
                       const SetlistManager &setlistManager,
                       const ImGuiViewport *viewport)
{
    float sidebarWidth = viewport->WorkSize.x * g_sidebarWidthRatio;
    float notesSidebarWidth = setlistManager.IsActive()
        ? viewport->WorkSize.x * g_notesSidebarWidthRatio
        : 0.0f;
    float viewerWidth = viewport->WorkSize.x - sidebarWidth - notesSidebarWidth;
    float panelHeight = viewport->WorkSize.y;

    ImVec2 viewerPos = ImVec2(viewport->WorkPos.x + sidebarWidth, viewport->WorkPos.y);

    ImGui::SetNextWindowPos(viewerPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewerWidth, panelHeight), ImGuiCond_Always);

    ImGui::Begin("PDF Viewer", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

    if (viewer.IsLoaded())
    {
        ImGui::Text("%s", viewer.GetFilename().c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("| %d pages | %.0f%%",
                            viewer.GetPageCount(),
                            viewer.GetZoom() * 100.0f);
    }
    else
    {
        ImGui::TextDisabled("No document loaded");
    }
    ImGui::Separator();

    GLuint texture = viewer.GetTexture();
    int texWidth = viewer.GetTextureWidth();
    int texHeight = viewer.GetTextureHeight();

    ImGui::BeginChild("ViewerCanvas", ImVec2(0, 0), true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    if (texture && texWidth > 0 && texHeight > 0)
    {
        ImVec2 availSize = ImGui::GetContentRegionAvail();

        // Compute fit-to-panel base size (aspect-ratio preserving)
        float aspectRatio = static_cast<float>(texWidth) / static_cast<float>(texHeight);
        float fitWidth = availSize.x;
        float fitHeight = fitWidth / aspectRatio;
        if (fitHeight > availSize.y)
        {
            fitHeight = availSize.y;
            fitWidth = fitHeight * aspectRatio;
        }

        // Apply zoom on top of the fitted size
        float displayWidth = fitWidth * viewer.GetZoom();
        float displayHeight = fitHeight * viewer.GetZoom();

        // Center image when it fits in the canvas
        ImVec2 cursor = ImGui::GetCursorPos();
        if (displayWidth < availSize.x)
            cursor.x += (availSize.x - displayWidth) * 0.5f;
        if (displayHeight < availSize.y)
            cursor.y += (availSize.y - displayHeight) * 0.5f;
        ImGui::SetCursorPos(cursor);

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
        ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.64f, 1.0f), "%s", placeholder);
    }
    ImGui::EndChild();

    ImGui::End();
}
