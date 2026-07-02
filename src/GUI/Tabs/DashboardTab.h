// ============================================================================
//  DashboardTab - live overview for runtime, SDK, and hotkeys.
// ============================================================================

#pragma once

#include "../ITab.h"

namespace fc {

class DashboardTab : public ITab
{
public:
    const char* Title() const override { return "Dashboard"; }
    const char* SearchKeywords() const override { return "overview status runtime modules hotkeys health"; }
    void Render() override;
};

} // namespace fc
