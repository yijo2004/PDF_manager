#include "setlist_gen.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

Setlist::Setlist(const std::string &name) : m_name(name) {}

bool Setlist::AddItem(const PdfEntry &entry)
{
    return AddItem(entry.filename, entry.fullPath);
}

bool Setlist::AddItem(const std::string &name, const std::string &fullPath)
{
    if (fullPath.empty())
        return false;

    m_items.push_back({name, fullPath});
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
    if (!viewer.Load(item.fullPath))
        return false;

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

bool SetlistManager::SaveToFile(const std::string &filepath) const
{
    std::ofstream out(std::filesystem::path(filepath),
                      std::ios::out | std::ios::trunc);
    if (!out.is_open())
    {
        std::cerr << "[SetlistManager] Failed to save: " << filepath << "\n";
        return false;
    }

    out << "SETLISTS_V1\n";

    for (const auto &setlist : m_setlists)
    {
        out << "SETLIST:" << setlist.GetName() << "\n";
        for (const auto &item : setlist.GetItems())
        {
            out << "ITEM:" << item.name << "\t" << item.fullPath << "\n";
        }
    }

    out << "END\n";
    out.close();
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

    // Check header
    if (!std::getline(in, line) || line != "SETLISTS_V1")
    {
        std::cerr << "[SetlistManager] Invalid save file format\n";
        return false;
    }

    // Clear existing setlists before loading
    Deactivate();
    m_setlists.clear();

    Setlist *current = nullptr;

    while (std::getline(in, line))
    {
        if (line == "END")
            break;

        if (line.rfind("SETLIST:", 0) == 0)
        {
            // New setlist — name is everything after "SETLIST:"
            std::string name = line.substr(8);
            m_setlists.emplace_back(name);
            current = &m_setlists.back();
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
        // Skip unknown lines gracefully
    }

    in.close();
    return true;
}

std::string SetlistManager::GetDefaultSavePath()
{
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
