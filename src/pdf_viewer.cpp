#include "pdf_viewer.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

PdfViewer::PdfViewer() {}

PdfViewer::~PdfViewer() { Close(); }

bool PdfViewer::Load(const std::string &filepath)
{
    // Close any existing document first
    Close();

    // Read file into memory to avoid path encoding issues on Windows
    std::filesystem::path path(filepath);
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        printf("[PdfViewer] Failed to open file: %s\n", filepath.c_str());
        return false;
    }

    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    m_pdfData.resize(static_cast<size_t>(fileSize));
    if (!file.read(reinterpret_cast<char *>(m_pdfData.data()), fileSize))
    {
        printf("[PdfViewer] Failed to read file contents\n");
        m_pdfData.clear();
        return false;
    }
    file.close();

    // Load PDF from memory
    m_document = FPDF_LoadMemDocument(
        m_pdfData.data(), static_cast<int>(m_pdfData.size()), nullptr);
    if (!m_document)
    {
        unsigned long error = FPDF_GetLastError();
        printf("[PdfViewer] Failed to load PDF: error code %lu\n", error);
        m_pdfData.clear();
        return false;
    }

    m_pageCount = FPDF_GetPageCount(m_document);
    m_currentPage = 0;
    m_zoomLevel = 1.0f;

    // Extract filename for display
    size_t lastSlash = filepath.find_last_of("/\\");
    m_filename = (lastSlash != std::string::npos)
                     ? filepath.substr(lastSlash + 1)
                     : filepath;

    // Render the first page
    m_needsRender = true;
    RenderPageToTexture();

    return true;
}

void PdfViewer::Close()
{
    if (m_page)
    {
        FPDF_ClosePage(m_page);
        m_page = nullptr;
    }
    if (m_document)
    {
        FPDF_CloseDocument(m_document);
        m_document = nullptr;
    }

    CleanupTexture();

    m_pdfData.clear();
    m_currentPage = 0;
    m_pageCount = 0;
    m_zoomLevel = 1.0f;
    m_needsRender = false;
    m_filename.clear();
}

void PdfViewer::CleanupTexture()
{
    if (m_texture)
    {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }
    m_textureWidth = 0;
    m_textureHeight = 0;
}

void PdfViewer::NextPage()
{
    if (CanGoNext())
    {
        m_currentPage++;
        m_needsRender = true;
    }
}

void PdfViewer::PreviousPage()
{
    if (CanGoPrevious())
    {
        m_currentPage--;
        m_needsRender = true;
    }
}

void PdfViewer::GoToPage(int page)
{
    if (page >= 0 && page < m_pageCount && page != m_currentPage)
    {
        m_currentPage = page;
        m_needsRender = true;
    }
}

void PdfViewer::SetZoom(float zoom)
{
    zoom = (zoom < MIN_ZOOM) ? MIN_ZOOM : (zoom > MAX_ZOOM) ? MAX_ZOOM
                                                            : zoom;
    if (zoom != m_zoomLevel)
    {
        m_zoomLevel = zoom;
        m_needsRender = true;
    }
}

void PdfViewer::ZoomIn(float factor) { SetZoom(m_zoomLevel * factor); }

void PdfViewer::ZoomOut(float factor) { SetZoom(m_zoomLevel / factor); }

void PdfViewer::Update()
{
    if (m_needsRender && m_document)
    {
        RenderPageToTexture();
        m_needsRender = false;
    }
}

void PdfViewer::RenderPageToTexture()
{
    if (!m_document || m_currentPage < 0 || m_currentPage >= m_pageCount)
        return;

    // Close previous page if open
    if (m_page)
    {
        FPDF_ClosePage(m_page);
        m_page = nullptr;
    }

    // Load the current page
    m_page = FPDF_LoadPage(m_document, m_currentPage);
    if (!m_page)
    {
        printf("[PdfViewer] Failed to load page %d\n", m_currentPage);
        return;
    }

    // Get page dimensions and apply zoom
    double pageWidth = FPDF_GetPageWidth(m_page);
    double pageHeight = FPDF_GetPageHeight(m_page);

    int renderWidth = static_cast<int>(pageWidth * m_zoomLevel);
    int renderHeight = static_cast<int>(pageHeight * m_zoomLevel);

    // Clamp to reasonable size
    const int MAX_TEXTURE_SIZE = 4096;
    if (renderWidth > MAX_TEXTURE_SIZE)
        renderWidth = MAX_TEXTURE_SIZE;
    if (renderHeight > MAX_TEXTURE_SIZE)
        renderHeight = MAX_TEXTURE_SIZE;

    // Allocate bitmap buffer (BGRA format)
    int stride = renderWidth * 4;
    std::vector<unsigned char> buffer(stride * renderHeight, 0xFF);

    // Create PDFium bitmap pointing to our buffer
    FPDF_BITMAP bitmap = FPDFBitmap_CreateEx(
        renderWidth, renderHeight, FPDFBitmap_BGRA, buffer.data(), stride);

    // Fill background white
    FPDFBitmap_FillRect(bitmap, 0, 0, renderWidth, renderHeight, 0xFFFFFFFF);

    // Render the page
    FPDF_RenderPageBitmap(bitmap, m_page, 0, 0, renderWidth, renderHeight, 0,
                          FPDF_ANNOT);

    // Convert BGRA to RGBA for OpenGL
    for (int i = 0; i < renderWidth * renderHeight; i++)
    {
        unsigned char temp = buffer[i * 4]; // B
        buffer[i * 4] = buffer[i * 4 + 2];  // R -> B position
        buffer[i * 4 + 2] = temp;           // B -> R position
    }

    // Create or update OpenGL texture
    if (m_texture == 0)
    {
        glGenTextures(1, &m_texture);
    }

    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, renderWidth, renderHeight, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, buffer.data());

    m_textureWidth = renderWidth;
    m_textureHeight = renderHeight;

    // Cleanup PDFium bitmap
    FPDFBitmap_Destroy(bitmap);
}
