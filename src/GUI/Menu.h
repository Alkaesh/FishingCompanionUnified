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
    const char* SearchText() const { return m_searchText.data(); }
    bool HasSearchText() const { return m_searchText[0] != '\0'; }

private:
    Menu() = default;

    bool m_defaultTabsRegistered = false;
    int m_selectedTab = 0;
    std::array<char, 64> m_searchText{};
    struct TabEntry
    {
        std::unique_ptr<ITab> tab;
        void* owner = nullptr;
    };

    std::vector<TabEntry> m_tabs;
};

} // namespace fc
