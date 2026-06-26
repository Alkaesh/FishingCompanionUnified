// ============================================================================
//  Menu — главное окно оверлея и контейнер вкладок.
// ----------------------------------------------------------------------------
//  Хранит список ITab. Чтобы добавить свою вкладку — создайте класс-наследник
//  ITab и вызовите Menu::Get().AddTab(std::make_unique<MyTab>()).
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

    // Регистрация стандартных вкладок (Dashboard / Timers / Settings).
    void RegisterDefaultTabs();

    // Добавить пользовательскую вкладку.
    void AddTab(std::unique_ptr<ITab> tab, void* owner = nullptr);
    void RemoveTabsByOwner(void* owner);

    // Отрисовка главного окна с панелью вкладок.
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
