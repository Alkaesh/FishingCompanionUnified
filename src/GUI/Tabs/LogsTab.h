// ============================================================================
//  LogsTab - unified runtime and SDK event viewer.
// ============================================================================

#pragma once

#include "../ITab.h"

namespace fc {

class LogsTab : public ITab
{
public:
    const char* Title() const override { return "Logs"; }
    const char* SearchKeywords() const override
    {
        return "logs log events runtime action sdk loader info warn error search open folder";
    }
    void Render() override;
};

} // namespace fc
