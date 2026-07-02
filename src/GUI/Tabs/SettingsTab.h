// ============================================================================
//  SettingsTab - hotkeys and overlay appearance.
// ============================================================================

#pragma once

#include "../ITab.h"

namespace fc {

class SettingsTab : public ITab
{
public:
    const char* Title() const override { return "Settings"; }
    const char* SearchKeywords() const override { return "settings hotkeys menu unload scale appearance reset"; }
    void Render() override;
};

} // namespace fc
