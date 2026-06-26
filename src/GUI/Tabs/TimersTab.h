// ============================================================================
//  TimersTab — «⏱️ Таймеры»: настройка напоминаний слайдерами.
// ============================================================================

#pragma once

#include "../ITab.h"

namespace fc {

class TimersTab : public ITab
{
public:
    const char* Title() const override { return "Таймеры"; }
    void Render() override;
};

} // namespace fc
