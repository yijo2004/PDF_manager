#pragma once

#include "imgui.h"

class PdfLibrary;
class PdfViewer;
class SetlistManager;

struct AppUiState
{
    bool sidebarVisible = true;
    bool notesVisible = true;
    bool compactDensity = false;
    bool autoSaveSetlists = true;
    bool settingsOpen = false;

    float sidebarWidthRatio = 0.24f;
    float notesWidthRatio = 0.22f;

    bool notesInputActive = false;
    bool saveStatusVisible = false;
    bool saveStatusOk = false;
    float saveStatusTimer = 0.0f;
};

bool LoadUiSettings(AppUiState &uiState);
bool SaveUiSettings(const AppUiState &uiState);
void ApplyUiDensity(const AppUiState &uiState);

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
