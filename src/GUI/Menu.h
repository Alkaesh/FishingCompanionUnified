// ============================================================================
//  Menu - Byster shell and tab container.
// ============================================================================

#pragma once

#include <array>
#include <memory>
#include <vector>
#include "ITab.h"

namespace fc {

class Menu
{
public:
    static Menu& Get();

    void RegisterDefaultTabs();
    void AddTab(std::unique_ptr<ITab> tab, void* owner = nullptr);
    void RemoveTabsByOwner(void* owner);
    void Render();
    // Switches the active tab to the one whose Title() matches name (no-op if
    // not found). Used by the Settings shortcut and command-search navigation.
    void JumpToTab(const char* name);
    const char* SearchText() const { return m_searchText.data(); }
    bool HasSearchText() const { return m_searchText[0] != '\0'; }

private:
    Menu() = default;

    bool m_defaultTabsRegistered = false;
    int m_selectedTab = 0;
    std::array<char, 64> m_searchText{};
    int m_paletteSelected = 0; // command-search dropdown cursor
    struct TabEntry
    {
        std::unique_ptr<ITab> tab;
        void* owner = nullptr;
    };

    std::vector<TabEntry> m_tabs;
};

} // namespace fc
