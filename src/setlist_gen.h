#pragma once

#include <string>
#include <vector>

#include "pdf_library.h"
#include "pdf_viewer.h"

/**
 * @brief Represents a single PDF file entry within a setlist.
 */
struct SetlistItem
{
    std::string name;
    std::string fullPath;
};

/**
 * @brief An ordered collection of PDF files that can be played through
 *        sequentially as a single combined document.
 */
class Setlist
{
public:
    explicit Setlist(const std::string &name);

    const std::string &GetName() const { return m_name; }
    void SetName(const std::string &name) { m_name = name; }

    const std::vector<SetlistItem> &GetItems() const { return m_items; }
    size_t GetItemCount() const { return m_items.size(); }

    /**
     * @brief Add a PDF file to the end of the setlist from a library entry.
     * @param entry The PdfEntry to add.
     * @return true if added successfully, false if the path was empty.
     */
    bool AddItem(const PdfEntry &entry);

    /**
     * @brief Add a PDF file to the end of the setlist by name and path.
     * @param name Display name for the item.
     * @param fullPath Absolute path to the PDF file.
     * @return true if added successfully, false if the path was empty.
     */
    bool AddItem(const std::string &name, const std::string &fullPath);

    /**
     * @brief Remove an item from the setlist by index.
     * @param index Zero-based index of the item to remove.
     * @return true if removed, false if index was out of range.
     */
    bool RemoveItem(size_t index);

    /**
     * @brief Reorder an item by moving it from one position to another.
     * @param fromIndex Current index of the item.
     * @param toIndex Target index to move the item to.
     * @return true if moved, false if either index was out of range.
     */
    bool MoveItem(size_t fromIndex, size_t toIndex);

    /**
     * @brief Remove all items from the setlist.
     */
    void Clear();

private:
    std::string m_name;
    std::vector<SetlistItem> m_items;
};

/**
 * @brief Manages multiple setlists and handles combined PDF navigation
 *        across files within the active setlist.
 *
 * When a setlist is activated, Next/Previous will traverse pages within
 * each PDF and automatically advance to the next/previous PDF in the
 * setlist when the end/start of a file is reached.
 */
class SetlistManager
{
public:
    /**
     * @brief Create a new empty setlist.
     * @param name Name for the setlist. If empty, a default name is generated.
     * @return Index of the newly created setlist.
     */
    size_t CreateSetlist(const std::string &name);

    /**
     * @brief Remove a setlist by index.
     * @param index Zero-based index of the setlist to remove.
     * @return true if removed, false if index was out of range.
     */
    bool RemoveSetlist(size_t index);

    size_t GetSetlistCount() const { return m_setlists.size(); }
    const std::vector<Setlist> &GetSetlists() const { return m_setlists; }

    Setlist *GetSetlist(size_t index);
    const Setlist *GetSetlist(size_t index) const;


    bool IsActive() const { return m_activeSetlistIndex >= 0; }
    int GetActiveSetlistIndex() const { return m_activeSetlistIndex; }
    int GetActiveItemIndex() const { return m_activeItemIndex; }

    /**
     * @brief Activate a setlist, loading the first item into the viewer.
     * @param index Zero-based index of the setlist to activate.
     * @param viewer The PdfViewer to load the first file into.
     * @return true if activated, false if the setlist was empty or load failed.
     */
    bool ActivateSetlist(size_t index, PdfViewer &viewer);

    /**
     * @brief Deactivate the current setlist, returning to normal navigation.
     */
    void Deactivate();

    bool JumpToItem(size_t setlistIndex, size_t itemIndex, PdfViewer &viewer);

    bool Next(PdfViewer &viewer);
    bool Previous(PdfViewer &viewer);

    bool CanGoNext(const PdfViewer &viewer) const;
    bool CanGoPrevious(const PdfViewer &viewer) const;

    bool SaveToFile(const std::string &filepath) const;
    bool LoadFromFile(const std::string &filepath);
    static std::string GetDefaultSavePath();

private:
    bool LoadActiveItem(PdfViewer &viewer, int itemIndex);
    const Setlist *GetActiveSetlist() const;

    std::vector<Setlist> m_setlists;
    int m_activeSetlistIndex = -1;
    int m_activeItemIndex = -1;
};
