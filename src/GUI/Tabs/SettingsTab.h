// ============================================================================
//  SettingsTab — «⚙️ Настройки»: горячие клавиши и параметры оверлея.
// ============================================================================

#pragma once

#include "../ITab.h"

namespace fc {

class SettingsTab : public ITab
{
public:
    const char* Title() const override { return "Настройки"; }
    void Render() override;
};

} // namespace fc
