#pragma once

#include "imgui.h"

class PdfLibrary;
class PdfViewer;
class SetlistManager;

void RenderLibraryPanel(PdfLibrary &library,
                        PdfViewer &viewer,
                        SetlistManager &setlistManager,
                        int &selectedIndex,
                        int &selectedSetlistIndex,
                        int &selectedSetlistItemIndex,
                        const ImGuiViewport *viewport);

void RenderControlsPanel(PdfViewer &viewer,
                         SetlistManager &setlistManager,
                         const ImGuiIO &io,
                         const ImGuiViewport *viewport);

void RenderSplitters(ImGuiIO &io, const ImGuiViewport *viewport,
                     bool showRightSplitter);

void RenderNotesPanel(SetlistManager &setlistManager,
                      const ImGuiViewport *viewport);

void RenderViewerPanel(const PdfViewer &viewer,
                       const SetlistManager &setlistManager,
                       const ImGuiViewport *viewport);
