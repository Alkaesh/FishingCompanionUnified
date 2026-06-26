// ============================================================================
//  DashboardTab — «📊 Статистика»: вывод игровых показателей + переключатели HUD.
// ============================================================================

#pragma once

#include "../ITab.h"

namespace fc {

class DashboardTab : public ITab
{
public:
    const char* Title() const override { return "Статистика"; }
    void Render() override;
};

} // namespace fc
