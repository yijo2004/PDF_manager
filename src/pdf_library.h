#pragma once

#include <string>
#include <vector>

/**
 * @brief Represents a PDF file entry in the library.
 */
struct PdfEntry
{
    std::string filename;
    std::string fullPath;
    
    PdfEntry(const std::string& name, const std::string& path)
        : filename(name), fullPath(path) {}
};

/**
 * @brief Manages a collection of PDF files from a selected folder.
 */
class PdfLibrary
{
public:
    PdfLibrary() = default;
    
    /**
     * @brief Scan a folder for PDF files.
     * @param folderPath Path to the folder to scan.
     * @return true if folder was successfully scanned, false otherwise.
     */
    bool LoadFolder(const std::string& folderPath);
    
    /**
     * @brief Clear the current library.
     */
    void Clear();
    
    /**
     * @brief Refresh the current folder (rescan for PDFs).
     */
    void Refresh();
    
    /**
     * @brief Check if a folder is loaded.
     */
    bool IsLoaded() const { return !m_folderPath.empty(); }
    
    /**
     * @brief Get the current folder path.
     */
    const std::string& GetFolderPath() const { return m_folderPath; }
    
    /**
     * @brief Get the folder name (last component of path).
     */
    const std::string& GetFolderName() const { return m_folderName; }
    
    /**
     * @brief Get the list of PDF files.
     */
    const std::vector<PdfEntry>& GetFiles() const { return m_files; }
    
    /**
     * @brief Get number of PDF files.
     */
    size_t GetFileCount() const { return m_files.size(); }

private:
    void ScanFolder();
    
    std::string m_folderPath;
    std::string m_folderName;
    std::vector<PdfEntry> m_files;
};
