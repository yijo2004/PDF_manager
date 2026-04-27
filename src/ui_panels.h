#pragma once

#include "imgui.h"

class PdfLibrary;
class PdfViewer;
class SetlistManager;

enum class AppFontMode
{
    Auto,
    Manual
};

struct AppUiState
{
    bool sidebarVisible = true;
    bool notesVisible = true;
    bool autoSaveSetlists = true;
    bool settingsOpen = false;
    bool createSetlistOpen = false;

    AppFontMode fontMode = AppFontMode::Auto;
    int fontSizePx = 22;
    AppFontMode activeFontMode = AppFontMode::Auto;
    int activeFontSizePx = 22;

    float sidebarWidthRatio = 0.24f;
    float notesWidthRatio = 0.22f;

    bool notesInputActive = false;
    bool saveStatusVisible = false;
    bool saveStatusOk = false;
    float saveStatusTimer = 0.0f;
    bool fontRestartPromptOpen = false;
    bool exitRequested = false;
};

bool LoadUiSettings(AppUiState &uiState);
bool SaveUiSettings(const AppUiState &uiState);

void RenderMainMenuBar(PdfLibrary &library,
                       PdfViewer &viewer,
                       SetlistManager &setlistManager,
                       AppUiState &uiState,
                       int &selectedFileIndex,
                       int &selectedSetlistIndex,
                       int &selectedSetlistItemIndex);

void RenderLibraryPanel(PdfLibrary &library,
                        PdfViewer &viewer,
                        SetlistManager &setlistManager,
                        AppUiState &uiState,
                        int &selectedIndex,
                        int &selectedSetlistIndex,
                        int &selectedSetlistItemIndex,
                        const ImGuiViewport *viewport);

void RenderDocumentToolbar(PdfViewer &viewer,
                           SetlistManager &setlistManager,
                           AppUiState &uiState,
                           const ImGuiIO &io,
                           const ImGuiViewport *viewport);

void RenderSplitters(AppUiState &uiState,
                     ImGuiIO &io,
                     const ImGuiViewport *viewport,
                     bool showNotesSplitter);

void RenderNotesPanel(SetlistManager &setlistManager,
                      AppUiState &uiState,
                      const ImGuiViewport *viewport);

void RenderViewerPanel(const PdfViewer &viewer,
                       const SetlistManager &setlistManager,
                       const AppUiState &uiState,
                       const ImGuiViewport *viewport);
