// ============================================================================
//  Menu - Byster shell and tab container.
// ============================================================================

#pragma once

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

private:
    Menu() = default;

    bool m_defaultTabsRegistered = false;
    int m_selectedTab = 0;
    struct TabEntry
    {
        std::unique_ptr<ITab> tab;
        void* owner = nullptr;
    };

    std::vector<TabEntry> m_tabs;
};

} // namespace fc
