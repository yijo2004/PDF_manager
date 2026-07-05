#include "ui_panels.h"

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

#include <algorithm>
#include <cstdint>
#include <cstdlib>
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
static const float TOOLBAR_HEIGHT = 58.0f;
static const int FONT_SIZE_OPTIONS[] = {18, 20, 22, 24, 26, 30};

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
#ifdef __APPLE__
    if (const char *home = std::getenv("HOME"))
    {
        try
        {
            const std::filesystem::path dataDirectory =
                std::filesystem::path(home) / "Library" /
                "Application Support" / "PDF Manager";
            std::filesystem::create_directories(dataDirectory);
            return (dataDirectory / "ui_settings.dat").string();
        }
        catch (const std::filesystem::filesystem_error &)
        {
        }
    }
#endif
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

static char ToLowerAscii(char c)
{
    if (c >= 'A' && c <= 'Z')
        return static_cast<char>(c - 'A' + 'a');
    return c;
}

static bool FuzzyMatch(const char *text, const char *query)
{
    if (!query || query[0] == '\0')
        return true;
    if (!text)
        return false;

    while (*text && *query)
    {
        if (ToLowerAscii(*text) == ToLowerAscii(*query))
            query++;
        text++;
    }

    return *query == '\0';
}

static void TooltipIfHovered(const char *text)
{
    if (text && text[0] != '\0' &&
        ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip |
                             ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", text);
}

static float BadgeWidth(const char *label)
{
    ImVec2 textSize = ImGui::CalcTextSize(label);
    return textSize.x + 18.0f;
}

static void ClippedText(const char *id,
                        const char *text,
                        float width,
                        const ImVec4 &color)
{
    width = (std::max)(0.0f, width);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoScrollWithMouse |
                             ImGuiWindowFlags_NoBackground;
    ImGui::BeginChild(id, ImVec2(width, ImGui::GetFrameHeight()), false,
                      flags);
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(color, "%s", text);
    TooltipIfHovered(text);
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

static void HeaderWithBadge(const char *id,
                            const char *title,
                            const char *tooltip,
                            const char *badge,
                            const ImVec4 &badgeColor)
{
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float badgeWidth = BadgeWidth(badge);
    float titleWidth = availableWidth - badgeWidth - spacing;

    if (titleWidth < 80.0f)
    {
        ImGui::TextColored(ImVec4(0.70f, 0.78f, 0.88f, 1.0f), "%s", title);
        TooltipIfHovered(tooltip);
        StatusBadge(badge, badgeColor);
        return;
    }

    ClippedText(id, title, titleWidth,
                ImVec4(0.70f, 0.78f, 0.88f, 1.0f));
    TooltipIfHovered(tooltip);
    ImGui::SameLine();
    StatusBadge(badge, badgeColor);
}

static bool CompactButtonWithTooltip(bool primary,
                                     const char *label,
                                     const char *tooltip,
                                     const ImVec2 &size)
{
    bool clicked = primary ? PrimaryButton(label, size)
                           : SecondaryButton(label, size);
    TooltipIfHovered(tooltip);
    return clicked;
}

static void OpenLibraryFolder(PdfLibrary &library,
                              PdfViewer &viewer,
                              SetlistManager &setlistManager,
                              int &selectedFileIndex)
{
    std::string folderPath = FileDialog::OpenFolder();
    if (!folderPath.empty() && library.LoadFolder(folderPath))
    {
        selectedFileIndex = -1;
        setlistManager.Deactivate();
        viewer.Close();
    }
}

static void RefreshLibrary(PdfLibrary &library, int &selectedFileIndex)
{
    std::string selectedPath;
    const auto &oldFiles = library.GetFiles();
    if (selectedFileIndex >= 0 &&
        selectedFileIndex < static_cast<int>(oldFiles.size()))
        selectedPath = oldFiles[static_cast<size_t>(selectedFileIndex)].fullPath;

    library.Refresh();
    selectedFileIndex = -1;
    if (selectedPath.empty())
        return;

    const auto &newFiles = library.GetFiles();
    for (size_t i = 0; i < newFiles.size(); i++)
    {
        if (newFiles[i].fullPath == selectedPath)
        {
            selectedFileIndex = static_cast<int>(i);
            break;
        }
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

static bool IsFontSizeOption(int sizePx)
{
    for (int option : FONT_SIZE_OPTIONS)
    {
        if (option == sizePx)
            return true;
    }
    return false;
}

static int ParseFontSize(const std::string &value, int fallback)
{
    try
    {
        int parsed = std::stoi(value);
        return IsFontSizeOption(parsed) ? parsed : fallback;
    }
    catch (...)
    {
        return fallback;
    }
}

static bool FontSettingChanged(const AppUiState &uiState)
{
    return uiState.fontMode != uiState.activeFontMode ||
           uiState.fontSizePx != uiState.activeFontSizePx;
}

static void RenderFontRestartPopup(AppUiState &uiState)
{
    if (uiState.fontRestartPromptOpen)
    {
        ImGui::OpenPopup("Restart Required");
        uiState.fontRestartPromptOpen = false;
    }

    if (ImGui::BeginPopupModal("Restart Required", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted(
            "Font size changes will apply after restarting the app.");
        ImGui::Spacing();
        if (PrimaryButton("Close App", ImVec2(120.0f, 0.0f)))
        {
            uiState.exitRequested = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (SecondaryButton("Later", ImVec2(100.0f, 0.0f)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
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
        ImGui::Checkbox("Restore last library and setlist",
                        &uiState.restoreLastSession);

        ImGui::Separator();
        std::string autoFontLabel =
            "Auto (" + std::to_string(uiState.autoFontSizePx) + " px)";
        std::string fontPreview =
            uiState.fontMode == AppFontMode::Auto
                ? autoFontLabel
                : std::to_string(uiState.fontSizePx) + " px";
        if (ImGui::BeginCombo("Font size", fontPreview.c_str()))
        {
            bool autoSelected = uiState.fontMode == AppFontMode::Auto;
            if (ImGui::Selectable(autoFontLabel.c_str(), autoSelected))
                uiState.fontMode = AppFontMode::Auto;
            if (autoSelected)
                ImGui::SetItemDefaultFocus();

            for (int sizePx : FONT_SIZE_OPTIONS)
            {
                std::string label = std::to_string(sizePx) + " px";
                bool selected = uiState.fontMode == AppFontMode::Manual &&
                                uiState.fontSizePx == sizePx;
                if (ImGui::Selectable(label.c_str(), selected))
                {
                    uiState.fontMode = AppFontMode::Manual;
                    uiState.fontSizePx = sizePx;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        ImGui::Separator();
        if (PrimaryButton("Save Settings", ImVec2(150.0f, 0.0f)))
        {
            bool fontChanged = FontSettingChanged(uiState);
            SaveUiSettings(uiState);
            ImGui::CloseCurrentPopup();
            if (fontChanged)
                uiState.fontRestartPromptOpen = true;
        }
        ImGui::SameLine();
        if (SecondaryButton("Close", ImVec2(100.0f, 0.0f)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    RenderFontRestartPopup(uiState);
}

// =============================================================================
// Settings persistence
// =============================================================================

bool LoadUiSettings(AppUiState &uiState)
{
    const std::string settingsPath = UiSettingsPath();
    std::ifstream in(std::filesystem::path(settingsPath), std::ios::in);
#ifdef __APPLE__
    // Older macOS builds stored settings relative to their working directory.
    // Read that location as a migration fallback, but save to Application
    // Support from now on.
    if (!in.is_open())
    {
        const std::string legacyPath = RuntimeSiblingPath("ui_settings.dat");
        if (legacyPath != settingsPath)
        {
            in.clear();
            in.open(std::filesystem::path(legacyPath), std::ios::in);
        }
    }
#endif
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
            continue;
        else if (key == "autoSaveSetlists")
            uiState.autoSaveSetlists = value == "1";
        else if (key == "restoreLastSession")
            uiState.restoreLastSession = value == "1";
        else if (key == "fontMode")
            uiState.fontMode = value == "manual" || value == "Manual" ||
                                       value == "1"
                                   ? AppFontMode::Manual
                                   : AppFontMode::Auto;
        else if (key == "fontSizePx")
            uiState.fontSizePx = ParseFontSize(value, uiState.fontSizePx);
        else if (key == "sidebarWidthRatio")
            uiState.sidebarWidthRatio =
                parseRatio(value, uiState.sidebarWidthRatio,
                           MIN_SIDEBAR_RATIO, MAX_SIDEBAR_RATIO);
        else if (key == "notesWidthRatio")
            uiState.notesWidthRatio =
                parseRatio(value, uiState.notesWidthRatio, MIN_NOTES_RATIO,
                           MAX_NOTES_RATIO);
        else if (key == "lastLibraryPath")
            uiState.lastLibraryPath = value;
        else if (key == "lastSetlistIndex")
        {
            try
            {
                uiState.lastSetlistIndex = std::stoi(value);
            }
            catch (...)
            {
                uiState.lastSetlistIndex = -1;
            }
        }
        else if (key == "lastSetlistName")
            uiState.lastSetlistName = value;
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
    out << "autoSaveSetlists=" << (uiState.autoSaveSetlists ? 1 : 0)
        << "\n";
    out << "restoreLastSession=" << (uiState.restoreLastSession ? 1 : 0)
        << "\n";
    out << "fontMode="
        << (uiState.fontMode == AppFontMode::Manual ? "manual" : "auto")
        << "\n";
    out << "fontSizePx=" << uiState.fontSizePx << "\n";
    out << "sidebarWidthRatio=" << uiState.sidebarWidthRatio << "\n";
    out << "notesWidthRatio=" << uiState.notesWidthRatio << "\n";
    out << "lastLibraryPath=" << uiState.lastLibraryPath << "\n";
    out << "lastSetlistIndex=" << uiState.lastSetlistIndex << "\n";
    out << "lastSetlistName=" << uiState.lastSetlistName << "\n";
    out.flush();
    const bool writeSucceeded = out.good();
    out.close();
    return writeSucceeded && !out.fail();
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
                RefreshLibrary(library, selectedFileIndex);

            ImGui::Separator();
            if (ImGui::MenuItem("Save Setlists"))
                SaveSetlists(setlistManager, uiState);

            if (ImGui::MenuItem("Load Setlists"))
                LoadSetlists(setlistManager, uiState, selectedSetlistIndex,
                             selectedSetlistItemIndex);

            ImGui::Separator();
            if (ImGui::MenuItem("Close PDF", nullptr, false,
                                viewer.IsLoaded()))
            {
                setlistManager.Deactivate();
                viewer.Close();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Library Sidebar", nullptr,
                            &uiState.sidebarVisible);
            ImGui::MenuItem("Notes Panel", nullptr, &uiState.notesVisible);

            ImGui::Separator();
            if (ImGui::MenuItem("Reset Zoom", nullptr, false,
                                viewer.IsLoaded()))
                viewer.ResetZoom();

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

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoResize;
    ImGui::Begin("Workspace", nullptr, flags);

    ImGui::TextUnformatted("PDF Manager");
    ImGui::TextDisabled("Library and setlists");
    ImGui::Spacing();

    bool selectSetlistsTab = uiState.setlistsPanelOpenRequested;
    uiState.setlistsPanelOpenRequested = false;

    if (ImGui::BeginTabBar("WorkspaceTabs", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("Library"))
        {
            static char pdfSearch[128] = "";

            float halfWidth =
                (ImGui::GetContentRegionAvail().x -
                 ImGui::GetStyle().ItemSpacing.x) *
                0.5f;

            if (PrimaryButton("Open Folder", ImVec2(halfWidth, 0.0f)))
                OpenLibraryFolder(library, viewer, setlistManager,
                                  selectedIndex);

            ImGui::SameLine();
            ImGui::BeginDisabled(!library.IsLoaded());
            if (SecondaryButton("Refresh", ImVec2(-1.0f, 0.0f)))
                RefreshLibrary(library, selectedIndex);
            ImGui::EndDisabled();

            ImGui::Spacing();

            if (library.IsLoaded())
            {
                std::string countLabel =
                    std::to_string(library.GetFileCount()) + " PDFs";
                HeaderWithBadge("##LibraryFolderName",
                                library.GetFolderName().c_str(),
                                library.GetFolderPath().c_str(),
                                countLabel.c_str(),
                                ImVec4(0.350f, 0.730f, 0.710f, 1.0f));

                ImGui::SetNextItemWidth(-1.0f);
                ImGui::InputTextWithHint("##PdfSearch", "Search PDFs...",
                                         pdfSearch, IM_ARRAYSIZE(pdfSearch));

                float listHeight = ImGui::GetContentRegionAvail().y;
                if (listHeight < 160.0f)
                    listHeight = 160.0f;
                ImGui::BeginChild("PdfList", ImVec2(0.0f, listHeight), true);

                const auto &files = library.GetFiles();
                bool hasVisiblePdfs = false;
                for (size_t i = 0; i < files.size(); i++)
                {
                    const auto &entry = files[i];
                    if (!FuzzyMatch(entry.filename.c_str(), pdfSearch))
                        continue;

                    hasVisiblePdfs = true;
                    bool isSelected = static_cast<int>(i) == selectedIndex;
                    std::string label =
                        entry.filename + "##pdf_" + std::to_string(i);
                    if (ImGui::Selectable(label.c_str(), isSelected,
                                          ImGuiSelectableFlags_AllowDoubleClick))
                    {
                        selectedIndex = static_cast<int>(i);
                        if (viewer.GetFilepath() != entry.fullPath)
                        {
                            setlistManager.Deactivate();
                            viewer.Load(entry.fullPath);
                        }
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", entry.fullPath.c_str());
                }

                if (files.empty())
                {
                    ImGui::Spacing();
                    ImGui::TextDisabled("No PDF files found in this folder.");
                }
                else if (!hasVisiblePdfs)
                {
                    ImGui::Spacing();
                    ImGui::TextDisabled("No PDFs match the search.");
                }

                ImGui::EndChild();
            }
            else
            {
                ImGui::BeginChild("LibraryEmpty", ImVec2(0.0f, 0.0f), true);
                ImVec2 avail = ImGui::GetContentRegionAvail();
                const char *title = "Choose a folder to begin";
                const char *body = "PDF files in that folder will appear here.";
                ImVec2 titleSize = ImGui::CalcTextSize(title);
                ImVec2 bodySize = ImGui::CalcTextSize(body);
                float blockHeight =
                    titleSize.y + bodySize.y + ImGui::GetFrameHeight() + 28.0f;
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() +
                                     (std::max)(0.0f,
                                                (avail.y - blockHeight) *
                                                    0.45f));
                ImGui::SetCursorPosX((std::max)(
                    0.0f, (avail.x - titleSize.x) * 0.5f));
                ImGui::TextColored(ImVec4(0.70f, 0.78f, 0.88f, 1.0f), "%s",
                                   title);
                ImGui::SetCursorPosX((std::max)(
                    0.0f, (avail.x - bodySize.x) * 0.5f));
                ImGui::TextDisabled("%s", body);
                ImGui::Spacing();
                float buttonWidth = 150.0f;
                ImGui::SetCursorPosX((std::max)(
                    0.0f, (avail.x - buttonWidth) * 0.5f));
                if (PrimaryButton("Open Folder", ImVec2(buttonWidth, 0.0f)))
                    OpenLibraryFolder(library, viewer, setlistManager,
                                      selectedIndex);
                ImGui::EndChild();
            }

            ImGui::EndTabItem();
        }

        ImGuiTabItemFlags setlistTabFlags =
            selectSetlistsTab ? ImGuiTabItemFlags_SetSelected
                              : ImGuiTabItemFlags_None;
        if (ImGui::BeginTabItem("Setlists", nullptr, setlistTabFlags))
        {
            static char setlistSearch[64] = "";
            static char newSetlistName[64] = "";
            static bool focusNewSetlistName = false;

            if (PrimaryButton("Create Setlist", ImVec2(-1.0f, 0.0f)))
            {
                newSetlistName[0] = '\0';
                focusNewSetlistName = true;
                ImGui::OpenPopup("Create Setlist");
            }

            if (ImGui::BeginPopupModal("Create Setlist", nullptr,
                                       ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextUnformatted("Setlist name");
                ImGui::SetNextItemWidth(320.0f);
                if (focusNewSetlistName)
                {
                    ImGui::SetKeyboardFocusHere();
                    focusNewSetlistName = false;
                }

                bool enterPressed = ImGui::InputText(
                    "##NewSetlistName", newSetlistName,
                    IM_ARRAYSIZE(newSetlistName),
                    ImGuiInputTextFlags_EnterReturnsTrue);

                bool createClicked =
                    PrimaryButton("Create", ImVec2(120.0f, 0.0f));
                if (enterPressed || createClicked)
                {
                    size_t newIndex =
                        setlistManager.CreateSetlist(newSetlistName);
                    selectedSetlistIndex = static_cast<int>(newIndex);
                    selectedSetlistItemIndex = -1;
                    newSetlistName[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }

                ImGui::SameLine();
                if (SecondaryButton("Cancel", ImVec2(120.0f, 0.0f)))
                {
                    newSetlistName[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##SetlistSearch", "Search setlists...",
                                     setlistSearch,
                                     IM_ARRAYSIZE(setlistSearch));

            float setlistHeight =
                (std::max)(120.0f, viewport->WorkSize.y * 0.22f);
            ImGui::BeginChild("SetlistList", ImVec2(0.0f, setlistHeight),
                              true);
            const auto &setlists = setlistManager.GetSetlists();
            bool hasVisibleSetlists = false;
            for (size_t i = 0; i < setlists.size(); i++)
            {
                const Setlist &setlist = setlists[i];
                if (!FuzzyMatch(setlist.GetName().c_str(), setlistSearch))
                    continue;

                hasVisibleSetlists = true;
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
            else if (!hasVisibleSetlists)
                ImGui::TextDisabled("No matching setlists.");
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
                (ImGui::GetContentRegionAvail().x -
                 ImGui::GetStyle().ItemSpacing.x) *
                0.5f;

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
            static int pendingDeleteSetlistIndex = -1;
            static std::string pendingDeleteSetlistName;
            ImGui::BeginDisabled(selectedSetlistIndex < 0);
            if (DangerButton("Remove", ImVec2(-1.0f, 0.0f)))
            {
                pendingDeleteSetlistIndex = selectedSetlistIndex;
                pendingDeleteSetlistName =
                    selectedSetlist ? selectedSetlist->GetName() : "";
                ImGui::OpenPopup("Delete Setlist");
            }
            ImGui::EndDisabled();

            if (ImGui::BeginPopupModal("Delete Setlist", nullptr,
                                       ImGuiWindowFlags_AlwaysAutoResize))
            {
                bool hasPendingDelete =
                    pendingDeleteSetlistIndex >= 0 &&
                    pendingDeleteSetlistIndex <
                        static_cast<int>(setlistManager.GetSetlistCount());

                if (hasPendingDelete)
                {
                    ImGui::Text("Delete setlist \"%s\"?",
                                pendingDeleteSetlistName.c_str());
                    ImGui::TextDisabled("This cannot be undone.");
                    ImGui::Spacing();

                    if (DangerButton("Delete", ImVec2(120.0f, 0.0f)))
                    {
                        size_t removeIndex =
                            static_cast<size_t>(pendingDeleteSetlistIndex);
                        int newSelected = -1;
                        if (static_cast<int>(setlistManager.GetSetlistCount()) >
                            1)
                            newSelected = pendingDeleteSetlistIndex > 0
                                              ? pendingDeleteSetlistIndex - 1
                                              : 0;

                        setlistManager.RemoveSetlist(removeIndex);
                        selectedSetlistIndex = newSelected;
                        selectedSetlistItemIndex = -1;
                        selectedSetlist =
                            selectedSetlistIndex >= 0
                                ? setlistManager.GetSetlist(
                                      static_cast<size_t>(
                                          selectedSetlistIndex))
                                : nullptr;
                        pendingDeleteSetlistIndex = -1;
                        pendingDeleteSetlistName.clear();
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::SameLine();
                    if (SecondaryButton("Cancel", ImVec2(120.0f, 0.0f)))
                    {
                        pendingDeleteSetlistIndex = -1;
                        pendingDeleteSetlistName.clear();
                        ImGui::CloseCurrentPopup();
                    }
                }
                else
                {
                    pendingDeleteSetlistIndex = -1;
                    pendingDeleteSetlistName.clear();
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            if (selectedSetlist)
            {
                ImGui::Spacing();
                std::string itemCount =
                    std::to_string(selectedSetlist->GetItemCount()) + " items";
                HeaderWithBadge("##SelectedSetlistName",
                                selectedSetlist->GetName().c_str(),
                                selectedSetlist->GetName().c_str(),
                                itemCount.c_str(),
                                ImVec4(0.48f, 0.76f, 0.56f, 1.0f));

                bool hasFiles = library.GetFileCount() > 0;
                ImGui::BeginDisabled(!hasFiles);
                const auto &files = library.GetFiles();
                static int comboFileIndex = 0;
                if (comboFileIndex >= static_cast<int>(files.size()))
                    comboFileIndex = 0;

                const char *previewName =
                    files.empty()
                        ? "Load a PDF folder first"
                        : files[static_cast<size_t>(comboFileIndex)]
                              .filename.c_str();
                float addRowWidth = ImGui::GetContentRegionAvail().x;
                float addButtonWidth = 54.0f;
                bool stackAddControls = addRowWidth < 240.0f;
                float comboWidth =
                    stackAddControls
                        ? -1.0f
                        : addRowWidth - addButtonWidth -
                              ImGui::GetStyle().ItemSpacing.x;
                ImGui::SetNextItemWidth(comboWidth);
                if (ImGui::BeginCombo("##AddPdfToSetlist", previewName))
                {
                    for (size_t i = 0; i < files.size(); i++)
                    {
                        bool selected = static_cast<int>(i) == comboFileIndex;
                        std::string label =
                            files[i].filename + "##combo_" +
                            std::to_string(i);
                        if (ImGui::Selectable(label.c_str(), selected))
                            comboFileIndex = static_cast<int>(i);
                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                if (!stackAddControls)
                    ImGui::SameLine();
                if (PrimaryButton("Add", ImVec2(stackAddControls ? -1.0f
                                                                 : addButtonWidth,
                                                0.0f)) &&
                    !files.empty())
                    selectedSetlist->AddItem(
                        files[static_cast<size_t>(comboFileIndex)]);
                ImGui::EndDisabled();

                if (!hasFiles)
                    ImGui::TextDisabled(
                        "Open a folder before adding PDFs to this setlist.");

                float buttonAreaHeight =
                    ImGui::GetFrameHeightWithSpacing() * 2.2f;
                float itemsHeight =
                    ImGui::GetContentRegionAvail().y - buttonAreaHeight;
                if (itemsHeight < 130.0f)
                    itemsHeight = 130.0f;

                ImGui::BeginChild("SetlistItems", ImVec2(0.0f, itemsHeight),
                                  true);
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
                        ImGui::PushStyleColor(
                            ImGuiCol_Text,
                            ImVec4(0.48f, 0.76f, 0.56f, 1.0f));

                    std::string label =
                        std::to_string(i + 1) + ". " + item.name;
                    if (isPlaying)
                        label = "Playing  " + label;
                    label += "##item_" + std::to_string(i);

                    if (ImGui::Selectable(
                            label.c_str(), isSelected,
                            ImGuiSelectableFlags_AllowDoubleClick))
                    {
                        selectedSetlistItemIndex = static_cast<int>(i);
                        if (ImGui::IsMouseDoubleClicked(
                                ImGuiMouseButton_Left))
                        {
                            if (setlistManager.JumpToItem(
                                    static_cast<size_t>(selectedSetlistIndex),
                                    i, viewer))
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
                            static_cast<size_t>(selectedSetlistItemIndex),
                            viewer))
                        uiState.notesVisible = true;
                }

                ImGui::SameLine();
                bool canMoveUp =
                    hasItemSelected && selectedSetlistItemIndex > 0;
                ImGui::BeginDisabled(!canMoveUp);
                if (SecondaryButton("Up", ImVec2(thirdWidth, 0.0f)))
                {
                    setlistManager.MoveItem(
                        static_cast<size_t>(selectedSetlistIndex),
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
                if (SecondaryButton("Down", ImVec2(-1.0f, 0.0f)))
                {
                    setlistManager.MoveItem(
                        static_cast<size_t>(selectedSetlistIndex),
                        static_cast<size_t>(selectedSetlistItemIndex),
                        static_cast<size_t>(selectedSetlistItemIndex + 1));
                    selectedSetlistItemIndex++;
                }
                ImGui::EndDisabled();

                halfWidth =
                    (ImGui::GetContentRegionAvail().x -
                     ImGui::GetStyle().ItemSpacing.x) *
                    0.5f;
                if (DangerButton("Remove Item", ImVec2(halfWidth, 0.0f)))
                {
                    setlistManager.RemoveItem(
                        static_cast<size_t>(selectedSetlistIndex),
                        static_cast<size_t>(selectedSetlistItemIndex));
                    if (selectedSetlistItemIndex >=
                        static_cast<int>(selectedSetlist->GetItemCount()))
                    {
                        selectedSetlistItemIndex =
                            static_cast<int>(selectedSetlist->GetItemCount()) -
                            1;
                    }
                }
                ImGui::EndDisabled();

                ImGui::SameLine();
                static int pendingClearSetlistIndex = -1;
                ImGui::BeginDisabled(selectedSetlist->GetItemCount() == 0);
                if (DangerButton("Clear", ImVec2(-1.0f, 0.0f)))
                {
                    pendingClearSetlistIndex = selectedSetlistIndex;
                    ImGui::OpenPopup("Clear Setlist");
                }
                ImGui::EndDisabled();

                if (ImGui::BeginPopupModal("Clear Setlist", nullptr,
                                           ImGuiWindowFlags_AlwaysAutoResize))
                {
                    bool canClear =
                        pendingClearSetlistIndex >= 0 &&
                        pendingClearSetlistIndex <
                            static_cast<int>(
                                setlistManager.GetSetlistCount());
                    Setlist *pendingClearSetlist =
                        canClear
                            ? setlistManager.GetSetlist(static_cast<size_t>(
                                  pendingClearSetlistIndex))
                            : nullptr;

                    if (pendingClearSetlist)
                    {
                        ImGui::Text("Clear all items from \"%s\"?",
                                    pendingClearSetlist->GetName().c_str());
                        ImGui::TextDisabled("This cannot be undone.");
                        ImGui::Spacing();

                        if (DangerButton("Clear", ImVec2(120.0f, 0.0f)))
                        {
                            setlistManager.ClearSetlist(
                                static_cast<size_t>(pendingClearSetlistIndex));
                            if (selectedSetlistIndex ==
                                pendingClearSetlistIndex)
                                selectedSetlistItemIndex = -1;
                            pendingClearSetlistIndex = -1;
                            ImGui::CloseCurrentPopup();
                        }

                        ImGui::SameLine();
                        if (SecondaryButton("Cancel", ImVec2(120.0f, 0.0f)))
                        {
                            pendingClearSetlistIndex = -1;
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    else
                    {
                        pendingClearSetlistIndex = -1;
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::EndPopup();
                }
            }
            else
            {
                ImGui::BeginChild("SetlistEmpty", ImVec2(0.0f, 0.0f), true);
                ImGui::TextDisabled(
                    "Create or select a setlist to organize PDFs.");
                ImGui::EndChild();
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
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

        std::string pageLabel =
            "Page " + std::to_string(viewer.GetCurrentPage() + 1) + " / " +
            std::to_string(viewer.GetPageCount());
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float setlistWidth = 0.0f;
        const Setlist *activeSetlist = nullptr;
        std::string setlistBadge;
        if (setlistManager.IsActive())
        {
            activeSetlist =
                setlistManager.GetSetlist(static_cast<size_t>(
                    setlistManager.GetActiveSetlistIndex()));
            if (activeSetlist)
            {
                setlistBadge =
                    "Setlist " +
                    std::to_string(setlistManager.GetActiveItemIndex() + 1) +
                    "/" + std::to_string(activeSetlist->GetItemCount());
                setlistWidth = BadgeWidth(setlistBadge.c_str()) + 58.0f +
                               spacing * 2.0f;
            }
        }

        float fixedWidth = ImGui::CalcTextSize(pageLabel.c_str()).x +
                           38.0f * 2.0f + 34.0f * 2.0f + 58.0f + 48.0f +
                           38.0f + setlistWidth + spacing * 11.0f;
        float titleWidth = ImGui::GetContentRegionAvail().x - fixedWidth;
        bool showTitle = titleWidth >= 90.0f;

        if (showTitle)
        {
            ClippedText("##ToolbarDocumentName",
                        viewer.GetFilename().c_str(), titleWidth,
                        ImVec4(0.70f, 0.78f, 0.88f, 1.0f));
            ImGui::SameLine();
        }

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", pageLabel.c_str());

        ImGui::SameLine();
        ImGui::Dummy(ImVec2(10.0f, 0.0f));
        ImGui::SameLine();
        ImGui::BeginDisabled(!canGoPrevious);
        bool previousClicked =
            CompactButtonWithTooltip(false, "<##PreviousPage",
                                     "Previous page", ImVec2(38.0f, 0.0f));
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
        bool nextClicked =
            CompactButtonWithTooltip(false, ">##NextPage", "Next page",
                                     ImVec2(38.0f, 0.0f));
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
        ImGui::Dummy(ImVec2(8.0f, 0.0f));
        ImGui::SameLine();
        if (SubtleButton("-##ZoomOut", ImVec2(34.0f, 0.0f)))
            viewer.ZoomOut();
        TooltipIfHovered("Zoom out");
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%.0f%%", viewer.GetZoom() * 100.0f);
        TooltipIfHovered("Current zoom");
        ImGui::SameLine();
        if (SubtleButton("+##ZoomIn", ImVec2(34.0f, 0.0f)))
            viewer.ZoomIn();
        TooltipIfHovered("Zoom in");
        ImGui::SameLine();
        if (SubtleButton("1:1##ResetZoom", ImVec2(48.0f, 0.0f)))
            viewer.ResetZoom();
        TooltipIfHovered("Reset zoom");

        if (io.KeyCtrl && io.MouseWheel != 0.0f)
        {
            if (io.MouseWheel > 0.0f)
                viewer.ZoomIn(1.1f);
            else
                viewer.ZoomOut(1.1f);
        }

        ImGui::SameLine();
        if (SecondaryButton("X##ClosePdf", ImVec2(38.0f, 0.0f)))
        {
            setlistManager.Deactivate();
            viewer.Close();
        }
        TooltipIfHovered("Close PDF");

        if (activeSetlist)
        {
            if (ImGui::GetContentRegionAvail().x >
                BadgeWidth(setlistBadge.c_str()) + 68.0f)
            {
                ImGui::SameLine();
                StatusBadge(setlistBadge.c_str(),
                            ImVec4(0.48f, 0.76f, 0.56f, 1.0f));
                TooltipIfHovered(activeSetlist->GetName().c_str());
                ImGui::SameLine();
            }

            if (DangerButton("Stop##StopSetlist", ImVec2(58.0f, 0.0f)))
                setlistManager.Deactivate();
            TooltipIfHovered("Stop setlist");
        }
    }
    else
    {
        float messageWidth = ImGui::GetContentRegionAvail().x;
        ClippedText("##NoDocumentMessage",
                    "No document loaded - open a folder and choose a PDF from the Library tab.",
                    messageWidth,
                    ImVec4(0.55f, 0.58f, 0.64f, 1.0f));
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
        const char *title =
            viewer.IsLoaded() ? "Rendering document" : "No document selected";
        const char *body = viewer.IsLoaded()
                               ? "The page preview will appear here."
                               : "Choose a PDF from the Library tab.";
        ImVec2 titleSize = ImGui::CalcTextSize(title);
        ImVec2 bodySize = ImGui::CalcTextSize(body);
        float blockHeight = titleSize.y + bodySize.y +
                            ImGui::GetStyle().ItemSpacing.y;
        ImVec2 start = ImGui::GetCursorPos();
        ImGui::SetCursorPos(
            ImVec2(start.x + (std::max)(0.0f,
                                        (availSize.x - titleSize.x) * 0.5f),
                   start.y + (std::max)(0.0f,
                                        (availSize.y - blockHeight) * 0.48f)));
        ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.64f, 1.0f), "%s", title);
        ImGui::SetCursorPos(
            ImVec2(start.x + (std::max)(0.0f,
                                        (availSize.x - bodySize.x) * 0.5f),
                   ImGui::GetCursorPosY()));
        ImGui::TextDisabled("%s", body);
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

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoResize;
    ImGui::Begin("Notes", nullptr, flags);

    const SetlistItem &item =
        mutSetlist->GetItems()[static_cast<size_t>(activeIdx)];
    ImGui::TextUnformatted("Notes");
    ImGui::Spacing();
    std::string itemBadge =
        "Item " + std::to_string(activeIdx + 1) + " / " +
        std::to_string(mutSetlist->GetItemCount());
    HeaderWithBadge("##NotesItemName", item.name.c_str(),
                    item.fullPath.c_str(), itemBadge.c_str(),
                    ImVec4(0.48f, 0.76f, 0.56f, 1.0f));
    ImGui::Separator();

    static char notesBuf[4096] = "";
    static int lastActiveSetlist = -1;
    static int lastActiveItem = -1;
    static std::string lastActiveItemPath;

    int curSetlist = setlistManager.GetActiveSetlistIndex();
    if (curSetlist != lastActiveSetlist || activeIdx != lastActiveItem ||
        item.fullPath != lastActiveItemPath)
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
        lastActiveItemPath = item.fullPath;
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
