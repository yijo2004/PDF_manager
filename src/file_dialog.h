#pragma once

#include <string>

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
