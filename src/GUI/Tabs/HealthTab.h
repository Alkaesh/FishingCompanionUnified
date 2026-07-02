// ============================================================================
//  HealthTab - compact runtime and SDK diagnostics.
// ============================================================================

#pragma once

#include "../ITab.h"

namespace fc {

class HealthTab : public ITab
{
public:
    const char* Title() const override { return "Health"; }
    const char* SearchKeywords() const override
    {
        return "health diagnostics status runtime ready busy queue sdk modules logs errors warnings";
    }
    void Render() override;
};

} // namespace fc
