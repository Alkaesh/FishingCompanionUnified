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
    void Render() override;
};

} // namespace fc
