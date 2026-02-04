#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include <GLFW/glfw3.h>
#include <fpdfview.h>

// OpenGL constant not always defined in basic headers
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

/**
 * @brief Manages PDF document loading, rendering, and display state.
 * 
 * Handles PDFium document lifecycle, page rendering to OpenGL textures,
 * and provides controls for navigation and zoom.
 */
class PdfViewer
{
public:
    PdfViewer();
    ~PdfViewer();

    // Prevent copying (OpenGL resources)
    PdfViewer(const PdfViewer&) = delete;
    PdfViewer& operator=(const PdfViewer&) = delete;

    // --- Document Operations ---
    
    /**
     * @brief Load a PDF file from disk.
     * @param filepath Path to the PDF file (UTF-8 encoded).
     * @return true if loaded successfully, false otherwise.
     */
    bool Load(const std::string& filepath);
    
    /**
     * @brief Close the current document and free resources.
     */
    void Close();
    
    /**
     * @brief Check if a document is currently loaded.
     */
    bool IsLoaded() const { return m_document != nullptr; }

    // --- Page Navigation ---
    
    int GetCurrentPage() const { return m_currentPage; }
    int GetPageCount() const { return m_pageCount; }
    
    void NextPage();
    void PreviousPage();
    void GoToPage(int page);
    
    bool CanGoNext() const { return m_currentPage < m_pageCount - 1; }
    bool CanGoPrevious() const { return m_currentPage > 0; }

    // --- Zoom Controls ---
    
    float GetZoom() const { return m_zoomLevel; }
    void SetZoom(float zoom);
    void ZoomIn(float factor = 1.25f);
    void ZoomOut(float factor = 1.25f);
    void ResetZoom() { SetZoom(1.0f); }

    // --- Rendering ---
    
    /**
     * @brief Request a re-render of the current page.
     * Call this after changing page or zoom.
     */
    void RequestRender() { m_needsRender = true; }
    
    /**
     * @brief Update rendering if needed. Call once per frame.
     */
    void Update();
    
    /**
     * @brief Get the OpenGL texture ID for the rendered page.
     */
    GLuint GetTexture() const { return m_texture; }
    int GetTextureWidth() const { return m_textureWidth; }
    int GetTextureHeight() const { return m_textureHeight; }

    // --- Document Info ---
    
    const std::string& GetFilename() const { return m_filename; }

private:
    void RenderPageToTexture();
    void CleanupTexture();

    // PDFium handles
    FPDF_DOCUMENT m_document = nullptr;
    FPDF_PAGE m_page = nullptr;
    
    // PDF data kept in memory (required by FPDF_LoadMemDocument)
    std::vector<unsigned char> m_pdfData;
    
    // OpenGL texture
    GLuint m_texture = 0;
    int m_textureWidth = 0;
    int m_textureHeight = 0;
    
    // State
    int m_currentPage = 0;
    int m_pageCount = 0;
    float m_zoomLevel = 1.0f;
    bool m_needsRender = false;
    std::string m_filename;

    // Zoom limits
    static constexpr float MIN_ZOOM = 0.1f;
    static constexpr float MAX_ZOOM = 5.0f;
};
