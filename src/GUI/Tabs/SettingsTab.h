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
    void Render() override;
};

} // namespace fc
