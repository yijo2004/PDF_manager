#include "pdf_library.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace
{
    // Case-insensitive string comparison for extension check
    bool EndsWithPdf(const std::string &filename)
    {
        if (filename.size() < 4)
            return false;

        std::string ext = filename.substr(filename.size() - 4);
        for (char &c : ext)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        return ext == ".pdf";
    }

    // Extract filename from path
    std::string GetFilename(const fs::path &path)
    {
        return path.filename().string();
    }
} // namespace

bool PdfLibrary::LoadFolder(const std::string &folderPath)
{
    Clear();

    // Validate the folder exists
    fs::path path(folderPath);
    if (!fs::exists(path) || !fs::is_directory(path))
    {
        return false;
    }

    m_folderPath = folderPath;
    m_folderName = path.filename().string();

    // Handle root drives on Windows (e.g., "C:\")
    if (m_folderName.empty())
    {
        m_folderName = folderPath;
    }

    ScanFolder();
    return true;
}

void PdfLibrary::Clear()
{
    m_folderPath.clear();
    m_folderName.clear();
    m_files.clear();
}

void PdfLibrary::Refresh()
{
    if (!m_folderPath.empty())
    {
        m_files.clear();
        ScanFolder();
    }
}

void PdfLibrary::ScanFolder()
{
    fs::path folderPath(m_folderPath);

    try
    {
        for (const auto &entry : fs::directory_iterator(folderPath))
        {
            if (entry.is_regular_file())
            {
                std::string filename = GetFilename(entry.path());
                if (EndsWithPdf(filename))
                {
                    std::string fullPath = entry.path().string();
                    m_files.emplace_back(filename, fullPath);
                }
            }
        }
    }
    catch (const fs::filesystem_error &)
    {
        // Silently handle permission errors, etc.
        // TODO: Handle errors
    }
}
