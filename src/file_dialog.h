#pragma once

#include <string>
#include <vector>

/**
 * @brief Platform-specific file dialog utilities.
 */
namespace FileDialog
{
    /**
     * @brief Open a native file dialog to select a PDF file.
     * @return The selected file path (UTF-8 encoded), or empty string if cancelled.
     */
    std::string OpenPDF();

    /**
     * @brief Open a native file dialog to select one or more PDF files.
     * @return The selected file paths (UTF-8 encoded), or empty if cancelled.
     */
    std::vector<std::string> OpenMultiplePDFs();
    
    /**
     * @brief Open a native file dialog to select any file with custom filters.
     * @param filterName Display name for the filter (e.g., "Text Files")
     * @param filterPattern File pattern (e.g., "*.txt")
     * @return The selected file path (UTF-8 encoded), or empty string if cancelled.
     */
    std::string Open(const char* filterName, const char* filterPattern);
    
    /**
     * @brief Open a native folder selection dialog.
     * @return The selected folder path (UTF-8 encoded), or empty string if cancelled.
     */
    std::string OpenFolder();
}
