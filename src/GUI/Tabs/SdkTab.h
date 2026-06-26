// ============================================================================
//  SdkTab - SDK status and integration hints.
// ============================================================================

#pragma once

#include "../ITab.h"

namespace fc {

class SdkTab : public ITab
{
public:
    const char* Title() const override { return "SDK"; }
    void Render() override;
};

} // namespace fc
