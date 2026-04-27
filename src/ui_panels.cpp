#include "ui_panels.h"

#include <GLFW/glfw3.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include "file_dialog.h"
#include "imgui.h"
#include "pdf_library.h"
#include "pdf_viewer.h"
#include "setlist_gen.h"
#include "ui_helpers.h"

// =============================================================================
// Layout constants
// =============================================================================

static const float MIN_SIDEBAR_RATIO = 0.16f;
static const float MAX_SIDEBAR_RATIO = 0.40f;
static const float MIN_NOTES_RATIO = 0.14f;
static const float MAX_NOTES_RATIO = 0.36f;
static const float SPLITTER_THICKNESS = 6.0f;
static const float TOOLBAR_HEIGHT = 50.0f;

static bool g_draggingSidebar = false;
static bool g_draggingNotes = false;

static std::string RuntimeSiblingPath(const char *filename)
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

static std::string UiSettingsPath()
{
    return RuntimeSiblingPath("ui_settings.dat");
}

static void SetSaveStatus(AppUiState &uiState, bool ok)
{
    uiState.saveStatusVisible = true;
    uiState.saveStatusOk = ok;
    uiState.saveStatusTimer = 2.0f;
}

static bool SaveSetlists(SetlistManager &setlistManager, AppUiState &uiState)
{
    bool ok = setlistManager.SaveToFile(SetlistManager::GetDefaultSavePath());
    SetSaveStatus(uiState, ok);
    return ok;
}

static bool LoadSetlists(SetlistManager &setlistManager,
                         AppUiState &uiState,
                         int &selectedSetlistIndex,
                         int &selectedSetlistItemIndex)
{
    bool ok = setlistManager.LoadFromFile(SetlistManager::GetDefaultSavePath());
    if (ok)
    {
        selectedSetlistIndex =
            setlistManager.GetSetlistCount() > 0 ? 0 : -1;
        selectedSetlistItemIndex = -1;
    }
    SetSaveStatus(uiState, ok);
    return ok;
}

static void OpenLibraryFolder(PdfLibrary &library,
                              PdfViewer &viewer,
                              SetlistManager &setlistManager,
                              int &selectedFileIndex)
{
    std::string folderPath = FileDialog::OpenFolder();
    if (!folderPath.empty())
    {
        library.LoadFolder(folderPath);
        selectedFileIndex = -1;
        setlistManager.Deactivate();
        viewer.Close();
    }
}

static bool NotesPanelShown(const SetlistManager &setlistManager,
                            const AppUiState &uiState)
{
    return setlistManager.IsActive() && uiState.notesVisible;
}

static float SidebarWidth(const ImGuiViewport *viewport,
                          const AppUiState &uiState)
{
    return uiState.sidebarVisible
               ? viewport->WorkSize.x * uiState.sidebarWidthRatio
               : 0.0f;
}

static float NotesWidth(const ImGuiViewport *viewport,
                        const SetlistManager &setlistManager,
                        const AppUiState &uiState)
{
    return NotesPanelShown(setlistManager, uiState)
               ? viewport->WorkSize.x * uiState.notesWidthRatio
               : 0.0f;
}

static void RenderSettingsPopup(AppUiState &uiState)
{
    if (uiState.settingsOpen)
    {
        ImGui::OpenPopup("Settings");
        uiState.settingsOpen = false;
    }

    if (ImGui::BeginPopupModal("Settings", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Checkbox("Show library sidebar", &uiState.sidebarVisible);
        ImGui::Checkbox("Show notes during setlists", &uiState.notesVisible);
        ImGui::Checkbox("Auto-save setlists on exit",
                        &uiState.autoSaveSetlists);

        ImGui::Separator();
        bool compact = uiState.compactDensity;
        if (ImGui::RadioButton("Comfortable density", !compact))
            uiState.compactDensity = false;
        if (ImGui::RadioButton("Compact density", compact))
            uiState.compactDensity = true;

        ImGui::Separator();
        if (PrimaryButton("Save Settings", ImVec2(150.0f, 0.0f)))
        {
            SaveUiSettings(uiState);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(100.0f, 0.0f)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}

// =============================================================================
// Settings persistence
// =============================================================================

bool LoadUiSettings(AppUiState &uiState)
{
    std::ifstream in(std::filesystem::path(UiSettingsPath()), std::ios::in);
    if (!in.is_open())
        return false;

    std::string line;
    if (!std::getline(in, line) || line != "PDF_MANAGER_UI_V1")
        return false;

    auto parseRatio = [](const std::string &value, float fallback,
                         float minValue, float maxValue) {
        try
        {
            return (std::clamp)(std::stof(value), minValue, maxValue);
        }
        catch (...)
        {
            return fallback;
        }
    };

    while (std::getline(in, line))
    {
        size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        if (key == "sidebarVisible")
            uiState.sidebarVisible = value == "1";
        else if (key == "notesVisible")
            uiState.notesVisible = value == "1";
        else if (key == "compactDensity")
            uiState.compactDensity = value == "1";
        else if (key == "autoSaveSetlists")
            uiState.autoSaveSetlists = value == "1";
        else if (key == "sidebarWidthRatio")
            uiState.sidebarWidthRatio =
                parseRatio(value, uiState.sidebarWidthRatio,
                           MIN_SIDEBAR_RATIO, MAX_SIDEBAR_RATIO);
        else if (key == "notesWidthRatio")
            uiState.notesWidthRatio =
                parseRatio(value, uiState.notesWidthRatio, MIN_NOTES_RATIO,
                           MAX_NOTES_RATIO);
    }

    return true;
}

bool SaveUiSettings(const AppUiState &uiState)
{
    std::ofstream out(std::filesystem::path(UiSettingsPath()),
                      std::ios::out | std::ios::trunc);
    if (!out.is_open())
        return false;

    out << "PDF_MANAGER_UI_V1\n";
    out << "sidebarVisible=" << (uiState.sidebarVisible ? 1 : 0) << "\n";
    out << "notesVisible=" << (uiState.notesVisible ? 1 : 0) << "\n";
    out << "compactDensity=" << (uiState.compactDensity ? 1 : 0) << "\n";
    out << "autoSaveSetlists=" << (uiState.autoSaveSetlists ? 1 : 0)
        << "\n";
    out << "sidebarWidthRatio=" << uiState.sidebarWidthRatio << "\n";
    out << "notesWidthRatio=" << uiState.notesWidthRatio << "\n";
    return true;
}

void ApplyUiDensity(const AppUiState &uiState)
{
    ImGuiStyle &style = ImGui::GetStyle();
    if (uiState.compactDensity)
    {
        style.WindowPadding = ImVec2(9.0f, 7.0f);
        style.FramePadding = ImVec2(7.0f, 4.0f);
        style.ItemSpacing = ImVec2(6.0f, 4.0f);
    }
    else
    {
        style.WindowPadding = ImVec2(12.0f, 10.0f);
        style.FramePadding = ImVec2(8.0f, 6.0f);
        style.ItemSpacing = ImVec2(8.0f, 6.0f);
    }
}

// =============================================================================
// Main menu
// =============================================================================

void RenderMainMenuBar(PdfLibrary &library,
                       PdfViewer &viewer,
                       SetlistManager &setlistManager,
                       AppUiState &uiState,
                       int &selectedFileIndex,
                       int &selectedSetlistIndex,
                       int &selectedSetlistItemIndex)
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open Folder..."))
                OpenLibraryFolder(library, viewer, setlistManager,
                                  selectedFileIndex);

            if (ImGui::MenuItem("Refresh Library", nullptr, false,
                                library.IsLoaded()))
                library.Refresh();

            ImGui::Separator();
            if (ImGui::MenuItem("Save Setlists", nullptr, false,
                                setlistManager.GetSetlistCount() > 0))
                SaveSetlists(setlistManager, uiState);

            if (ImGui::MenuItem("Load Setlists"))
                LoadSetlists(setlistManager, uiState, selectedSetlistIndex,
                             selectedSetlistItemIndex);

            ImGui::Separator();
            if (ImGui::MenuItem("Close PDF", nullptr, false,
                                viewer.IsLoaded()))
                viewer.Close();

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Library Sidebar", nullptr,
                            &uiState.sidebarVisible);
            ImGui::MenuItem("Notes Panel", nullptr, &uiState.notesVisible);

            ImGui::Separator();
            if (ImGui::MenuItem("Comfortable Density", nullptr,
                                !uiState.compactDensity))
                uiState.compactDensity = false;
            if (ImGui::MenuItem("Compact Density", nullptr,
                                uiState.compactDensity))
                uiState.compactDensity = true;

            ImGui::Separator();
            if (ImGui::MenuItem("Reset Zoom", nullptr, false,
                                viewer.IsLoaded()))
                viewer.ResetZoom();

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Setlist"))
        {
            Setlist *selectedSetlist = nullptr;
            if (selectedSetlistIndex >= 0 &&
                selectedSetlistIndex <
                    static_cast<int>(setlistManager.GetSetlistCount()))
            {
                selectedSetlist = setlistManager.GetSetlist(
                    static_cast<size_t>(selectedSetlistIndex));
            }

            bool canActivate = selectedSetlist &&
                               selectedSetlist->GetItemCount() > 0;
            if (ImGui::MenuItem("Activate Selected", nullptr, false,
                                canActivate))
            {
                if (setlistManager.ActivateSetlist(
                        static_cast<size_t>(selectedSetlistIndex), viewer))
                    uiState.notesVisible = true;
            }

            if (ImGui::MenuItem("Deactivate", nullptr, false,
                                setlistManager.IsActive()))
                setlistManager.Deactivate();

            ImGui::Separator();
            if (ImGui::MenuItem("Save Setlists", nullptr, false,
                                setlistManager.GetSetlistCount() > 0))
                SaveSetlists(setlistManager, uiState);
            if (ImGui::MenuItem("Load Setlists"))
                LoadSetlists(setlistManager, uiState, selectedSetlistIndex,
                             selectedSetlistItemIndex);

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Settings"))
        {
            if (ImGui::MenuItem("Preferences..."))
                uiState.settingsOpen = true;
            ImGui::MenuItem("Auto-save Setlists", nullptr,
                            &uiState.autoSaveSetlists);
            ImGui::EndMenu();
        }

        if (uiState.saveStatusVisible)
        {
            uiState.saveStatusTimer -= ImGui::GetIO().DeltaTime;
            if (uiState.saveStatusTimer <= 0.0f)
                uiState.saveStatusVisible = false;
            else
            {
                ImGui::SameLine();
                ImGui::TextColored(
                    uiState.saveStatusOk
                        ? ImVec4(0.48f, 0.76f, 0.56f, 1.0f)
                        : ImVec4(0.90f, 0.42f, 0.42f, 1.0f),
                    uiState.saveStatusOk ? "Setlists saved" : "Setlist I/O failed");
            }
        }

        ImGui::EndMainMenuBar();
    }

    RenderSettingsPopup(uiState);
}

// =============================================================================
// Sidebar
// =============================================================================

void RenderLibraryPanel(PdfLibrary &library,
                        PdfViewer &viewer,
                        SetlistManager &setlistManager,
                        AppUiState &uiState,
                        int &selectedIndex,
                        int &selectedSetlistIndex,
                        int &selectedSetlistItemIndex,
                        const ImGuiViewport *viewport)
{
    if (!uiState.sidebarVisible)
        return;

    float sidebarWidth = SidebarWidth(viewport, uiState);
    ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(sidebarWidth, viewport->WorkSize.y),
                             ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoResize;
    ImGui::Begin("Library / Setlists", nullptr, flags);

    if (ImGui::CollapsingHeader("Library", ImGuiTreeNodeFlags_DefaultOpen))
    {
        float halfWidth =
            (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x)
            * 0.5f;

        if (PrimaryButton("Open Folder", ImVec2(halfWidth, 0.0f)))
            OpenLibraryFolder(library, viewer, setlistManager, selectedIndex);

        ImGui::SameLine();
        ImGui::BeginDisabled(!library.IsLoaded());
        if (ImGui::Button("Refresh", ImVec2(-1.0f, 0.0f)))
            library.Refresh();
        ImGui::EndDisabled();

        if (library.IsLoaded())
        {
            ImGui::TextColored(ImVec4(0.70f, 0.78f, 0.88f, 1.0f), "%s",
                               library.GetFolderName().c_str());
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", library.GetFolderPath().c_str());

            float listHeight = (std::max)(130.0f,
                viewport->WorkSize.y * 0.26f);
            ImGui::BeginChild("PdfList", ImVec2(0.0f, listHeight), true);

            const auto &files = library.GetFiles();
            for (size_t i = 0; i < files.size(); i++)
            {
                const auto &entry = files[i];
                bool isSelected = static_cast<int>(i) == selectedIndex;
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
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", entry.fullPath.c_str());
            }

            if (files.empty())
                ImGui::TextDisabled("No PDF files found.");

            ImGui::EndChild();
        }
        else
        {
            ImGui::TextDisabled("Select a folder to browse PDFs.");
        }
    }

    if (ImGui::CollapsingHeader("Setlists", ImGuiTreeNodeFlags_DefaultOpen))
    {
        static char newSetlistName[64] = "";
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 76.0f);
        bool enterPressed = ImGui::InputText(
            "##NewSetlistName", newSetlistName, IM_ARRAYSIZE(newSetlistName),
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (PrimaryButton("Create", ImVec2(-1.0f, 0.0f)) || enterPressed)
        {
            size_t newIndex = setlistManager.CreateSetlist(newSetlistName);
            selectedSetlistIndex = static_cast<int>(newIndex);
            selectedSetlistItemIndex = -1;
            newSetlistName[0] = '\0';
        }

        float setlistHeight = 110.0f;
        ImGui::BeginChild("SetlistList", ImVec2(0.0f, setlistHeight), true);
        const auto &setlists = setlistManager.GetSetlists();
        for (size_t i = 0; i < setlists.size(); i++)
        {
            const Setlist &setlist = setlists[i];
            bool isSelected = static_cast<int>(i) == selectedSetlistIndex;
            bool isActive = setlistManager.IsActive() &&
                            setlistManager.GetActiveSetlistIndex() ==
                                static_cast<int>(i);

            std::string label = isActive ? "Playing  " : "";
            label += setlist.GetName();
            label += " (" + std::to_string(setlist.GetItemCount()) + ")";
            label += "##setlist_" + std::to_string(i);

            if (ImGui::Selectable(label.c_str(), isSelected))
            {
                selectedSetlistIndex = static_cast<int>(i);
                selectedSetlistItemIndex = -1;
            }
        }
        if (setlists.empty())
            ImGui::TextDisabled("No setlists yet.");
        ImGui::EndChild();

        Setlist *selectedSetlist = nullptr;
        if (selectedSetlistIndex >= 0 &&
            selectedSetlistIndex <
                static_cast<int>(setlistManager.GetSetlistCount()))
        {
            selectedSetlist =
                setlistManager.GetSetlist(static_cast<size_t>(
                    selectedSetlistIndex));
        }

        float halfWidth =
            (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x)
            * 0.5f;

        bool canActivate = selectedSetlist &&
                           selectedSetlist->GetItemCount() > 0;
        ImGui::BeginDisabled(!canActivate);
        if (PrimaryButton("Activate", ImVec2(halfWidth, 0.0f)))
        {
            if (setlistManager.ActivateSetlist(
                    static_cast<size_t>(selectedSetlistIndex), viewer))
                uiState.notesVisible = true;
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(selectedSetlistIndex < 0);
        if (ImGui::Button("Remove", ImVec2(-1.0f, 0.0f)))
        {
            size_t removeIndex = static_cast<size_t>(selectedSetlistIndex);
            int newSelected = -1;
            if (static_cast<int>(setlistManager.GetSetlistCount()) > 1)
                newSelected = selectedSetlistIndex > 0
                                  ? selectedSetlistIndex - 1
                                  : 0;
            setlistManager.RemoveSetlist(removeIndex);
            selectedSetlistIndex = newSelected;
            selectedSetlistItemIndex = -1;
            selectedSetlist = selectedSetlistIndex >= 0
                                  ? setlistManager.GetSetlist(
                                        static_cast<size_t>(
                                            selectedSetlistIndex))
                                  : nullptr;
        }
        ImGui::EndDisabled();

        if (selectedSetlist)
        {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.70f, 0.78f, 0.88f, 1.0f), "%s",
                               selectedSetlist->GetName().c_str());

            bool hasFiles = library.GetFileCount() > 0;
            ImGui::BeginDisabled(!hasFiles);
            const auto &files = library.GetFiles();
            static int comboFileIndex = 0;
            if (comboFileIndex >= static_cast<int>(files.size()))
                comboFileIndex = 0;

            const char *previewName = files.empty()
                                          ? "No PDFs loaded"
                                          : files[static_cast<size_t>(
                                                comboFileIndex)].filename.c_str();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 48.0f);
            if (ImGui::BeginCombo("##AddPdfToSetlist", previewName))
            {
                for (size_t i = 0; i < files.size(); i++)
                {
                    bool selected = static_cast<int>(i) == comboFileIndex;
                    std::string label =
                        files[i].filename + "##combo_" + std::to_string(i);
                    if (ImGui::Selectable(label.c_str(), selected))
                        comboFileIndex = static_cast<int>(i);
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            if (PrimaryButton("Add", ImVec2(-1.0f, 0.0f)) && !files.empty())
                selectedSetlist->AddItem(files[static_cast<size_t>(
                    comboFileIndex)]);
            ImGui::EndDisabled();

            float buttonAreaHeight =
                ImGui::GetFrameHeightWithSpacing() * 2.2f;
            float itemsHeight =
                ImGui::GetContentRegionAvail().y - buttonAreaHeight;
            if (itemsHeight < 120.0f)
                itemsHeight = 120.0f;

            ImGui::BeginChild("SetlistItems", ImVec2(0.0f, itemsHeight), true);
            const auto &items = selectedSetlist->GetItems();
            for (size_t i = 0; i < items.size(); i++)
            {
                const SetlistItem &item = items[i];
                bool isSelected =
                    static_cast<int>(i) == selectedSetlistItemIndex;
                bool isPlaying = setlistManager.IsActive() &&
                                 setlistManager.GetActiveSetlistIndex() ==
                                     selectedSetlistIndex &&
                                 setlistManager.GetActiveItemIndex() ==
                                     static_cast<int>(i);

                if (isPlaying)
                    ImGui::PushStyleColor(ImGuiCol_Text,
                                          ImVec4(0.48f, 0.76f, 0.56f, 1.0f));

                std::string label = std::to_string(i + 1) + ". " + item.name;
                if (isPlaying)
                    label = "Now  " + label;
                label += "##item_" + std::to_string(i);

                if (ImGui::Selectable(label.c_str(), isSelected,
                                      ImGuiSelectableFlags_AllowDoubleClick))
                {
                    selectedSetlistItemIndex = static_cast<int>(i);
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        if (setlistManager.JumpToItem(
                                static_cast<size_t>(selectedSetlistIndex), i,
                                viewer))
                            uiState.notesVisible = true;
                    }
                }

                if (isPlaying)
                    ImGui::PopStyleColor();

                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", item.fullPath.c_str());
            }
            if (items.empty())
                ImGui::TextDisabled("Add PDFs to build this setlist.");
            ImGui::EndChild();

            bool hasItemSelected =
                selectedSetlistItemIndex >= 0 &&
                selectedSetlistItemIndex <
                    static_cast<int>(selectedSetlist->GetItemCount());
            float thirdWidth =
                (ImGui::GetContentRegionAvail().x -
                 ImGui::GetStyle().ItemSpacing.x * 2.0f) /
                3.0f;

            ImGui::BeginDisabled(!hasItemSelected);
            if (PrimaryButton("Open", ImVec2(thirdWidth, 0.0f)))
            {
                if (setlistManager.JumpToItem(
                        static_cast<size_t>(selectedSetlistIndex),
                        static_cast<size_t>(selectedSetlistItemIndex), viewer))
                    uiState.notesVisible = true;
            }

            ImGui::SameLine();
            bool canMoveUp = hasItemSelected && selectedSetlistItemIndex > 0;
            ImGui::BeginDisabled(!canMoveUp);
            if (ImGui::Button("Up", ImVec2(thirdWidth, 0.0f)))
            {
                selectedSetlist->MoveItem(
                    static_cast<size_t>(selectedSetlistItemIndex),
                    static_cast<size_t>(selectedSetlistItemIndex - 1));
                selectedSetlistItemIndex--;
            }
            ImGui::EndDisabled();

            ImGui::SameLine();
            bool canMoveDown =
                hasItemSelected &&
                selectedSetlistItemIndex + 1 <
                    static_cast<int>(selectedSetlist->GetItemCount());
            ImGui::BeginDisabled(!canMoveDown);
            if (ImGui::Button("Down", ImVec2(-1.0f, 0.0f)))
            {
                selectedSetlist->MoveItem(
                    static_cast<size_t>(selectedSetlistItemIndex),
                    static_cast<size_t>(selectedSetlistItemIndex + 1));
                selectedSetlistItemIndex++;
            }
            ImGui::EndDisabled();

            halfWidth =
                (ImGui::GetContentRegionAvail().x -
                 ImGui::GetStyle().ItemSpacing.x) *
                0.5f;
            if (ImGui::Button("Remove Item", ImVec2(halfWidth, 0.0f)))
            {
                selectedSetlist->RemoveItem(
                    static_cast<size_t>(selectedSetlistItemIndex));
                if (selectedSetlistItemIndex >=
                    static_cast<int>(selectedSetlist->GetItemCount()))
                {
                    selectedSetlistItemIndex =
                        static_cast<int>(selectedSetlist->GetItemCount()) - 1;
                }
            }
            ImGui::EndDisabled();

            ImGui::SameLine();
            ImGui::BeginDisabled(selectedSetlist->GetItemCount() == 0);
            if (ImGui::Button("Clear", ImVec2(-1.0f, 0.0f)))
            {
                selectedSetlist->Clear();
                selectedSetlistItemIndex = -1;
            }
            ImGui::EndDisabled();
        }
    }

    ImGui::End();
}

// =============================================================================
// Document toolbar and viewer
// =============================================================================

void RenderDocumentToolbar(PdfViewer &viewer,
                           SetlistManager &setlistManager,
                           AppUiState &uiState,
                           const ImGuiIO &io,
                           const ImGuiViewport *viewport)
{
    float sidebarWidth = SidebarWidth(viewport, uiState);
    float notesWidth = NotesWidth(viewport, setlistManager, uiState);
    float toolbarWidth = viewport->WorkSize.x - sidebarWidth - notesWidth;
    if (toolbarWidth < 100.0f)
        return;

    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x + sidebarWidth, viewport->WorkPos.y),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(toolbarWidth, TOOLBAR_HEIGHT),
                             ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::Begin("Document Toolbar", nullptr, flags);

    if (viewer.IsLoaded())
    {
        bool canGoPrevious = setlistManager.IsActive()
                                 ? setlistManager.CanGoPrevious(viewer)
                                 : viewer.CanGoPrevious();
        bool canGoNext = setlistManager.IsActive()
                             ? setlistManager.CanGoNext(viewer)
                             : viewer.CanGoNext();
        bool allowKeyboardNavigation =
            !setlistManager.IsActive() || !uiState.notesInputActive;

        ImGui::BeginDisabled(!canGoPrevious);
        bool previousClicked = ImGui::Button("<", ImVec2(38.0f, 0.0f));
        bool previousKeyPressed = allowKeyboardNavigation &&
                                  ImGui::IsKeyPressed(ImGuiKey_LeftArrow);
        if (previousClicked || previousKeyPressed)
        {
            if (setlistManager.IsActive())
                setlistManager.Previous(viewer);
            else
                viewer.PreviousPage();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!canGoNext);
        bool nextClicked = ImGui::Button(">", ImVec2(38.0f, 0.0f));
        bool nextKeyPressed = allowKeyboardNavigation &&
                              ImGui::IsKeyPressed(ImGuiKey_RightArrow);
        if (nextClicked || nextKeyPressed)
        {
            if (setlistManager.IsActive())
                setlistManager.Next(viewer);
            else
                viewer.NextPage();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::Text("Page %d / %d", viewer.GetCurrentPage() + 1,
                    viewer.GetPageCount());

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        if (ImGui::Button("-", ImVec2(34.0f, 0.0f)))
            viewer.ZoomOut();
        ImGui::SameLine();
        ImGui::Text("%.0f%%", viewer.GetZoom() * 100.0f);
        ImGui::SameLine();
        if (ImGui::Button("+", ImVec2(34.0f, 0.0f)))
            viewer.ZoomIn();
        ImGui::SameLine();
        if (ImGui::Button("Reset", ImVec2(70.0f, 0.0f)))
            viewer.ResetZoom();

        if (io.KeyCtrl && io.MouseWheel != 0.0f)
        {
            if (io.MouseWheel > 0.0f)
                viewer.ZoomIn(1.1f);
            else
                viewer.ZoomOut(1.1f);
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(70.0f, 0.0f)))
            viewer.Close();

        if (setlistManager.IsActive())
        {
            const Setlist *activeSetlist =
                setlistManager.GetSetlist(static_cast<size_t>(
                    setlistManager.GetActiveSetlistIndex()));
            if (activeSetlist)
            {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.48f, 0.76f, 0.56f, 1.0f),
                                   "Setlist %d/%zu",
                                   setlistManager.GetActiveItemIndex() + 1,
                                   activeSetlist->GetItemCount());
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", activeSetlist->GetName().c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("Stop"))
                    setlistManager.Deactivate();
            }
        }
    }
    else
    {
        ImGui::TextDisabled("No document loaded");
    }

    ImGui::End();
}

void RenderViewerPanel(const PdfViewer &viewer,
                       const SetlistManager &setlistManager,
                       const AppUiState &uiState,
                       const ImGuiViewport *viewport)
{
    float sidebarWidth = SidebarWidth(viewport, uiState);
    float notesWidth = NotesWidth(viewport, setlistManager, uiState);
    float viewerWidth = viewport->WorkSize.x - sidebarWidth - notesWidth;
    float viewerHeight = viewport->WorkSize.y - TOOLBAR_HEIGHT;
    if (viewerWidth < 100.0f || viewerHeight < 100.0f)
        return;

    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + sidebarWidth,
                                   viewport->WorkPos.y + TOOLBAR_HEIGHT),
                            ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewerWidth, viewerHeight),
                             ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoResize;
    ImGui::Begin("PDF Viewer", nullptr, flags);

    GLuint texture = viewer.GetTexture();
    int texWidth = viewer.GetTextureWidth();
    int texHeight = viewer.GetTextureHeight();

    ImGui::BeginChild("ViewerCanvas", ImVec2(0.0f, 0.0f), true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    if (texture && texWidth > 0 && texHeight > 0)
    {
        ImVec2 availSize = ImGui::GetContentRegionAvail();
        float aspectRatio =
            static_cast<float>(texWidth) / static_cast<float>(texHeight);
        float fitWidth = availSize.x;
        float fitHeight = fitWidth / aspectRatio;
        if (fitHeight > availSize.y)
        {
            fitHeight = availSize.y;
            fitWidth = fitHeight * aspectRatio;
        }

        float displayWidth = fitWidth * viewer.GetZoom();
        float displayHeight = fitHeight * viewer.GetZoom();

        ImVec2 cursor = ImGui::GetCursorPos();
        if (displayWidth < availSize.x)
            cursor.x += (availSize.x - displayWidth) * 0.5f;
        if (displayHeight < availSize.y)
            cursor.y += (availSize.y - displayHeight) * 0.5f;
        ImGui::SetCursorPos(cursor);

        ImGui::Image((ImTextureID)(void *)(uintptr_t)texture,
                     ImVec2(displayWidth, displayHeight));
    }
    else
    {
        ImVec2 availSize = ImGui::GetContentRegionAvail();
        const char *placeholder = "Select a PDF from the library";
        ImVec2 textSize = ImGui::CalcTextSize(placeholder);
        ImGui::SetCursorPos(
            ImVec2((availSize.x - textSize.x) * 0.5f + ImGui::GetCursorPosX(),
                   (availSize.y - textSize.y) * 0.5f + ImGui::GetCursorPosY()));
        ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.64f, 1.0f), "%s",
                           placeholder);
    }
    ImGui::EndChild();

    ImGui::End();
}

// =============================================================================
// Notes and splitters
// =============================================================================

void RenderNotesPanel(SetlistManager &setlistManager,
                      AppUiState &uiState,
                      const ImGuiViewport *viewport)
{
    if (!NotesPanelShown(setlistManager, uiState))
    {
        uiState.notesInputActive = false;
        return;
    }

    Setlist *mutSetlist = setlistManager.GetSetlist(
        static_cast<size_t>(setlistManager.GetActiveSetlistIndex()));
    int activeIdx = setlistManager.GetActiveItemIndex();
    if (!mutSetlist || activeIdx < 0 ||
        activeIdx >= static_cast<int>(mutSetlist->GetItemCount()))
    {
        uiState.notesInputActive = false;
        return;
    }

    float notesWidth = NotesWidth(viewport, setlistManager, uiState);
    float notesX = viewport->WorkPos.x + viewport->WorkSize.x - notesWidth;

    ImGui::SetNextWindowPos(ImVec2(notesX, viewport->WorkPos.y),
                            ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(notesWidth, viewport->WorkSize.y),
                             ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoResize;
    ImGui::Begin("Notes", nullptr, flags);

    const SetlistItem &item =
        mutSetlist->GetItems()[static_cast<size_t>(activeIdx)];
    ImGui::TextColored(ImVec4(0.70f, 0.78f, 0.88f, 1.0f), "%s",
                       item.name.c_str());
    ImGui::TextDisabled("Item %d / %zu", activeIdx + 1,
                        mutSetlist->GetItemCount());
    ImGui::Separator();

    static char notesBuf[4096] = "";
    static int lastActiveSetlist = -1;
    static int lastActiveItem = -1;

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

    ImVec2 notesSize = ImVec2(-1.0f, ImGui::GetContentRegionAvail().y);
    bool notesChanged = ImGui::InputTextMultiline(
        "##ItemNotes", notesBuf, sizeof(notesBuf), notesSize);
    uiState.notesInputActive = ImGui::IsItemActive() || ImGui::IsItemFocused();
    if (uiState.notesInputActive)
    {
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        ImU32 color = ImGui::ColorConvertFloat4ToU32(
            ImVec4(0.44f, 0.62f, 0.86f, 1.0f));
        drawList->AddRect(ImVec2(min.x - 2.0f, min.y - 2.0f),
                          ImVec2(max.x + 2.0f, max.y + 2.0f), color, 4.0f, 0,
                          2.0f);
    }
    if (notesChanged)
    {
        mutSetlist->SetItemNotes(static_cast<size_t>(activeIdx),
                                 std::string(notesBuf));
    }

    ImGui::End();
}

void RenderSplitters(AppUiState &uiState,
                     ImGuiIO &io,
                     const ImGuiViewport *viewport,
                     bool showNotesSplitter)
{
    ImVec4 normal = ImVec4(0.22f, 0.24f, 0.28f, 0.90f);
    ImVec4 hovered = ImVec4(0.30f, 0.38f, 0.50f, 1.0f);
    ImVec4 active = ImVec4(0.36f, 0.48f, 0.66f, 1.0f);
    ImDrawList *drawList = ImGui::GetForegroundDrawList();

    if (uiState.sidebarVisible || g_draggingSidebar)
    {
        float sidebarWidth = SidebarWidth(viewport, uiState);
        ImVec2 p1 = ImVec2(viewport->WorkPos.x + sidebarWidth -
                               SPLITTER_THICKNESS * 0.5f,
                           viewport->WorkPos.y);
        ImVec2 p2 =
            ImVec2(p1.x + SPLITTER_THICKNESS,
                   viewport->WorkPos.y + viewport->WorkSize.y);
        bool isHovered = io.MousePos.x >= p1.x && io.MousePos.x <= p2.x &&
                         io.MousePos.y >= p1.y && io.MousePos.y <= p2.y;
        ImVec4 color = g_draggingSidebar ? active : isHovered ? hovered : normal;
        drawList->AddRectFilled(p1, p2, ImGui::ColorConvertFloat4ToU32(color));

        if (isHovered || g_draggingSidebar)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            g_draggingSidebar = true;
        if (g_draggingSidebar)
        {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                float newRatio =
                    (io.MousePos.x - viewport->WorkPos.x) /
                    viewport->WorkSize.x;
                uiState.sidebarWidthRatio =
                    (std::clamp)(newRatio, MIN_SIDEBAR_RATIO,
                                 MAX_SIDEBAR_RATIO);
            }
            else
            {
                g_draggingSidebar = false;
            }
        }
    }

    if (showNotesSplitter || g_draggingNotes)
    {
        float notesWidth = viewport->WorkSize.x * uiState.notesWidthRatio;
        float splitterX = viewport->WorkPos.x + viewport->WorkSize.x -
                          notesWidth - SPLITTER_THICKNESS * 0.5f;
        ImVec2 p1 = ImVec2(splitterX, viewport->WorkPos.y);
        ImVec2 p2 =
            ImVec2(splitterX + SPLITTER_THICKNESS,
                   viewport->WorkPos.y + viewport->WorkSize.y);
        bool isHovered = io.MousePos.x >= p1.x && io.MousePos.x <= p2.x &&
                         io.MousePos.y >= p1.y && io.MousePos.y <= p2.y;
        ImVec4 color = g_draggingNotes ? active : isHovered ? hovered : normal;
        drawList->AddRectFilled(p1, p2, ImGui::ColorConvertFloat4ToU32(color));

        if (isHovered || g_draggingNotes)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            g_draggingNotes = true;
        if (g_draggingNotes)
        {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                float rightEdge = viewport->WorkPos.x + viewport->WorkSize.x;
                float newRatio = (rightEdge - io.MousePos.x) /
                                 viewport->WorkSize.x;
                uiState.notesWidthRatio =
                    (std::clamp)(newRatio, MIN_NOTES_RATIO, MAX_NOTES_RATIO);
            }
            else
            {
                g_draggingNotes = false;
            }
        }
    }
}
