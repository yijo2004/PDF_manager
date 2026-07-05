#include "setlist_gen.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

Setlist::Setlist(const std::string &name) : m_name(name) {}

bool Setlist::AddItem(const PdfEntry &entry)
{
    return AddItem(entry.filename, entry.fullPath);
}

bool Setlist::AddItem(const std::string &name, const std::string &fullPath)
{
    if (fullPath.empty())
        return false;

    m_items.push_back({name, fullPath, {}});
    return true;
}

bool Setlist::RemoveItem(size_t index)
{
    if (index >= m_items.size())
        return false;

    m_items.erase(m_items.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool Setlist::MoveItem(size_t fromIndex, size_t toIndex)
{
    if (fromIndex >= m_items.size() || toIndex >= m_items.size())
        return false;

    if (fromIndex == toIndex)
        return true;

    SetlistItem item = m_items[fromIndex];
    m_items.erase(m_items.begin() + static_cast<std::ptrdiff_t>(fromIndex));
    m_items.insert(m_items.begin() + static_cast<std::ptrdiff_t>(toIndex), item);
    return true;
}

void Setlist::Clear()
{
    m_items.clear();
}

static const std::string EMPTY_STRING;

const std::string &Setlist::GetItemNotes(size_t index) const
{
    if (index >= m_items.size())
        return EMPTY_STRING;
    return m_items[index].notes;
}

void Setlist::SetItemNotes(size_t index, const std::string &notes)
{
    if (index < m_items.size())
        m_items[index].notes = notes;
}

size_t SetlistManager::CreateSetlist(const std::string &name)
{
    std::string finalName = name;
    if (finalName.empty())
    {
        finalName = "Setlist " + std::to_string(m_setlists.size() + 1);
    }

    m_setlists.emplace_back(finalName);
    return m_setlists.size() - 1;
}

bool SetlistManager::RemoveSetlist(size_t index)
{
    if (index >= m_setlists.size())
        return false;

    if (static_cast<int>(index) == m_activeSetlistIndex)
    {
        Deactivate();
    }
    else if (static_cast<int>(index) < m_activeSetlistIndex)
    {
        m_activeSetlistIndex--;
    }

    m_setlists.erase(m_setlists.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool SetlistManager::RemoveItem(size_t setlistIndex, size_t itemIndex)
{
    Setlist *setlist = GetSetlist(setlistIndex);
    if (!setlist || itemIndex >= setlist->GetItemCount())
        return false;

    if (static_cast<int>(setlistIndex) == m_activeSetlistIndex)
    {
        const int removedIndex = static_cast<int>(itemIndex);
        if (removedIndex == m_activeItemIndex)
            Deactivate();
        else if (removedIndex < m_activeItemIndex)
            m_activeItemIndex--;
    }

    return setlist->RemoveItem(itemIndex);
}

bool SetlistManager::MoveItem(size_t setlistIndex,
                              size_t fromIndex,
                              size_t toIndex)
{
    Setlist *setlist = GetSetlist(setlistIndex);
    if (!setlist || fromIndex >= setlist->GetItemCount() ||
        toIndex >= setlist->GetItemCount())
        return false;

    if (static_cast<int>(setlistIndex) == m_activeSetlistIndex)
    {
        const int from = static_cast<int>(fromIndex);
        const int to = static_cast<int>(toIndex);
        if (m_activeItemIndex == from)
            m_activeItemIndex = to;
        else if (from < m_activeItemIndex && to >= m_activeItemIndex)
            m_activeItemIndex--;
        else if (from > m_activeItemIndex && to <= m_activeItemIndex)
            m_activeItemIndex++;
    }

    return setlist->MoveItem(fromIndex, toIndex);
}

bool SetlistManager::ClearSetlist(size_t setlistIndex)
{
    Setlist *setlist = GetSetlist(setlistIndex);
    if (!setlist)
        return false;

    if (static_cast<int>(setlistIndex) == m_activeSetlistIndex)
        Deactivate();
    setlist->Clear();
    return true;
}

Setlist *SetlistManager::GetSetlist(size_t index)
{
    if (index >= m_setlists.size())
        return nullptr;

    return &m_setlists[index];
}

const Setlist *SetlistManager::GetSetlist(size_t index) const
{
    if (index >= m_setlists.size())
        return nullptr;

    return &m_setlists[index];
}

const Setlist *SetlistManager::GetActiveSetlist() const
{
    if (m_activeSetlistIndex < 0 ||
        m_activeSetlistIndex >= static_cast<int>(m_setlists.size()))
        return nullptr;

    return &m_setlists[static_cast<size_t>(m_activeSetlistIndex)];
}

bool SetlistManager::ActivateSetlist(size_t index, PdfViewer &viewer)
{
    if (index >= m_setlists.size())
        return false;

    const Setlist &setlist = m_setlists[index];
    if (setlist.GetItemCount() == 0)
        return false;

    int previousSetlist = m_activeSetlistIndex;
    int previousItem = m_activeItemIndex;

    m_activeSetlistIndex = static_cast<int>(index);
    m_activeItemIndex = 0;

    if (!LoadActiveItem(viewer, m_activeItemIndex))
    {
        m_activeSetlistIndex = previousSetlist;
        m_activeItemIndex = previousItem;
        return false;
    }

    return true;
}

void SetlistManager::Deactivate()
{
    m_activeSetlistIndex = -1;
    m_activeItemIndex = -1;
}

bool SetlistManager::JumpToItem(size_t setlistIndex,
                                size_t itemIndex,
                                PdfViewer &viewer)
{
    if (setlistIndex >= m_setlists.size())
        return false;

    const Setlist &setlist = m_setlists[setlistIndex];
    if (itemIndex >= setlist.GetItemCount())
        return false;

    int previousSetlist = m_activeSetlistIndex;
    int previousItem = m_activeItemIndex;

    m_activeSetlistIndex = static_cast<int>(setlistIndex);
    m_activeItemIndex = static_cast<int>(itemIndex);

    if (!LoadActiveItem(viewer, m_activeItemIndex))
    {
        m_activeSetlistIndex = previousSetlist;
        m_activeItemIndex = previousItem;
        return false;
    }

    return true;
}

bool SetlistManager::LoadActiveItem(PdfViewer &viewer, int itemIndex)
{
    const Setlist *setlist = GetActiveSetlist();
    if (!setlist)
        return false;

    if (itemIndex < 0 ||
        itemIndex >= static_cast<int>(setlist->GetItemCount()))
        return false;

    const SetlistItem &item =
        setlist->GetItems()[static_cast<size_t>(itemIndex)];
    float currentZoom = viewer.GetZoom();
    if (!viewer.Load(item.fullPath))
        return false;
    viewer.SetZoom(currentZoom);

    m_activeItemIndex = itemIndex;
    return true;
}

bool SetlistManager::Next(PdfViewer &viewer)
{
    const Setlist *setlist = GetActiveSetlist();
    if (!setlist)
        return false;

    if (viewer.IsLoaded() && viewer.CanGoNext())
    {
        viewer.NextPage();
        return true;
    }

    int nextItem = m_activeItemIndex + 1;
    if (nextItem < static_cast<int>(setlist->GetItemCount()))
    {
        return LoadActiveItem(viewer, nextItem);
    }

    return false;
}

bool SetlistManager::Previous(PdfViewer &viewer)
{
    const Setlist *setlist = GetActiveSetlist();
    if (!setlist)
        return false;

    if (viewer.IsLoaded() && viewer.CanGoPrevious())
    {
        viewer.PreviousPage();
        return true;
    }

    int prevItem = m_activeItemIndex - 1;
    if (prevItem >= 0)
    {
        if (!LoadActiveItem(viewer, prevItem))
            return false;

        if (viewer.GetPageCount() > 0)
        {
            viewer.GoToPage(viewer.GetPageCount() - 1);
        }
        return true;
    }

    return false;
}

bool SetlistManager::CanGoNext(const PdfViewer &viewer) const
{
    const Setlist *setlist = GetActiveSetlist();
    if (!setlist)
        return false;

    if (viewer.IsLoaded() && viewer.CanGoNext())
        return true;

    int nextItem = m_activeItemIndex + 1;
    return nextItem < static_cast<int>(setlist->GetItemCount());
}

bool SetlistManager::CanGoPrevious(const PdfViewer &viewer) const
{
    const Setlist *setlist = GetActiveSetlist();
    if (!setlist)
        return false;

    if (viewer.IsLoaded() && viewer.CanGoPrevious())
        return true;

    int prevItem = m_activeItemIndex - 1;
    return prevItem >= 0;
}

// =============================================================================
// Persistence
// =============================================================================
//
// File format (plain text, line-based):
//
//   SETLISTS_V1
//   SETLIST:<name>
//   ITEM:<display_name>\t<full_path>
//   ITEM:<display_name>\t<full_path>
//   SETLIST:<name>
//   ITEM:<display_name>\t<full_path>
//   END
//

namespace
{
bool ReplaceSaveFile(const std::filesystem::path &temporaryPath,
                     const std::filesystem::path &destinationPath)
{
#ifdef _WIN32
    if (MoveFileExW(temporaryPath.c_str(), destinationPath.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        return true;

    std::error_code cleanupError;
    std::filesystem::remove(temporaryPath, cleanupError);
    return false;
#else
    std::error_code renameError;
    std::filesystem::rename(temporaryPath, destinationPath, renameError);
    if (!renameError)
        return true;

    std::error_code cleanupError;
    std::filesystem::remove(temporaryPath, cleanupError);
    return false;
#endif
}
} // namespace

bool SetlistManager::SaveToFile(const std::string &filepath) const
{
    const std::filesystem::path destinationPath(filepath);
    std::filesystem::path temporaryPath = destinationPath;
    temporaryPath += ".tmp";

    std::ofstream out(temporaryPath, std::ios::out | std::ios::trunc);
    if (!out.is_open())
    {
        std::cerr << "[SetlistManager] Failed to save: " << filepath << "\n";
        return false;
    }

    out << "SETLISTS_V2\n";

    for (const auto &setlist : m_setlists)
    {
        out << "SETLIST:" << setlist.GetName() << "\n";
        for (const auto &item : setlist.GetItems())
        {
            out << "ITEM:" << item.name << "\t" << item.fullPath << "\n";
            if (!item.notes.empty())
            {
                // Encode notes: replace newlines with \n literal for safe
                // single-line storage
                std::string encoded = item.notes;
                // Replace backslashes first to avoid double-encoding
                for (size_t pos = 0;
                     (pos = encoded.find('\\', pos)) != std::string::npos;
                     pos += 2)
                {
                    encoded.replace(pos, 1, "\\\\");
                }
                for (size_t pos = 0;
                     (pos = encoded.find('\n', pos)) != std::string::npos;
                     pos += 2)
                {
                    encoded.replace(pos, 1, "\\n");
                }
                out << "NOTES:" << encoded << "\n";
            }
        }
    }

    out << "END\n";
    out.flush();
    const bool writeSucceeded = out.good();
    out.close();
    if (!writeSucceeded || out.fail())
    {
        std::error_code cleanupError;
        std::filesystem::remove(temporaryPath, cleanupError);
        std::cerr << "[SetlistManager] Failed while writing: " << filepath
                  << "\n";
        return false;
    }

    if (!ReplaceSaveFile(temporaryPath, destinationPath))
    {
        std::cerr << "[SetlistManager] Failed to replace save file: "
                  << filepath << "\n";
        return false;
    }

    return true;
}

bool SetlistManager::LoadFromFile(const std::string &filepath)
{
    std::ifstream in(std::filesystem::path(filepath), std::ios::in);
    if (!in.is_open())
    {
        // Not an error — file may just not exist yet
        return false;
    }

    std::string line;

    // Check header — accept V1 or V2
    if (!std::getline(in, line) ||
        (line != "SETLISTS_V1" && line != "SETLISTS_V2"))
    {
        std::cerr << "[SetlistManager] Invalid save file format\n";
        return false;
    }

    std::vector<Setlist> loadedSetlists;
    Setlist *current = nullptr;
    bool foundEnd = false;

    while (std::getline(in, line))
    {
        if (line == "END")
        {
            foundEnd = true;
            break;
        }

        if (line.rfind("SETLIST:", 0) == 0)
        {
            // New setlist — name is everything after "SETLIST:"
            std::string name = line.substr(8);
            loadedSetlists.emplace_back(name);
            current = &loadedSetlists.back();
        }
        else if (line.rfind("ITEM:", 0) == 0 && current)
        {
            // Item — format is "ITEM:<name>\t<path>"
            std::string rest = line.substr(5);
            size_t tabPos = rest.find('\t');
            if (tabPos != std::string::npos)
            {
                std::string name = rest.substr(0, tabPos);
                std::string path = rest.substr(tabPos + 1);
                current->AddItem(name, path);
            }
        }
        else if (line.rfind("NOTES:", 0) == 0 && current &&
                 current->GetItemCount() > 0)
        {
            // Notes for the most recently added item
            std::string encoded = line.substr(6);
            // Decode: \\n -> \n, \\\\ -> backslash
            std::string decoded;
            decoded.reserve(encoded.size());
            for (size_t i = 0; i < encoded.size(); i++)
            {
                if (encoded[i] == '\\' && i + 1 < encoded.size())
                {
                    if (encoded[i + 1] == 'n')
                    {
                        decoded += '\n';
                        i++;
                    }
                    else if (encoded[i + 1] == '\\')
                    {
                        decoded += '\\';
                        i++;
                    }
                    else
                    {
                        decoded += encoded[i];
                    }
                }
                else
                {
                    decoded += encoded[i];
                }
            }
            current->SetItemNotes(current->GetItemCount() - 1, decoded);
        }
        // Skip unknown lines gracefully
    }

    if (!foundEnd || in.bad())
    {
        std::cerr << "[SetlistManager] Incomplete or unreadable save file\n";
        return false;
    }

    in.close();
    Deactivate();
    m_setlists = std::move(loadedSetlists);
    return true;
}

std::string SetlistManager::GetDefaultSavePath()
{
#ifdef __APPLE__
    // macOS application bundles are not a writable data location. Keep user
    // data in the standard per-user Application Support directory instead.
    if (const char *home = std::getenv("HOME"))
    {
        try
        {
            const std::filesystem::path dataDirectory =
                std::filesystem::path(home) / "Library" /
                "Application Support" / "PDF Manager";
            std::filesystem::create_directories(dataDirectory);
            return (dataDirectory / "setlists.dat").string();
        }
        catch (const std::filesystem::filesystem_error &)
        {
            // Fall through to the existing working-directory behavior.
        }
    }
#endif

    // Save next to the executable
    try
    {
        auto exePath = std::filesystem::current_path();
        return (exePath / "setlists.dat").string();
    }
    catch (...)
    {
        return "setlists.dat";
    }
}
