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

    bool CaseInsensitiveFilenameLess(const PdfEntry &left,
                                     const PdfEntry &right)
    {
        const size_t sharedLength =
            (std::min)(left.filename.size(), right.filename.size());

        for (size_t i = 0; i < sharedLength; i++)
        {
            const auto leftChar = static_cast<unsigned char>(left.filename[i]);
            const auto rightChar =
                static_cast<unsigned char>(right.filename[i]);
            const int foldedLeft = std::tolower(leftChar);
            const int foldedRight = std::tolower(rightChar);

            if (foldedLeft != foldedRight)
                return foldedLeft < foldedRight;
        }

        if (left.filename.size() != right.filename.size())
            return left.filename.size() < right.filename.size();

        // Keep the order deterministic when filenames differ only by case.
        return left.filename < right.filename;
    }
} // namespace

bool PdfLibrary::LoadFolder(const std::string &folderPath)
{
    // Validate the folder exists
    fs::path path(folderPath);
    std::error_code pathError;
    if (!fs::exists(path, pathError) || pathError ||
        !fs::is_directory(path, pathError) || pathError)
    {
        return false;
    }

    Clear();
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

    std::sort(m_files.begin(), m_files.end(), CaseInsensitiveFilenameLess);
}
