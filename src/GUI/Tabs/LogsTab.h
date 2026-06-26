// ============================================================================
//  LogsTab - unified runtime and SDK event viewer.
// ============================================================================

#pragma once

#include <array>
#include "../ITab.h"

namespace fc {

class LogsTab : public ITab
{
public:
    const char* Title() const override { return "Logs"; }
    const char* SearchKeywords() const override
    {
        return "logs log events runtime action sdk loader info warn error search open folder copy";
    }
    void Render() override;

private:
    // Filter/view state kept on the instance (not function-static) so it never
    // leaks across menu lifecycles and stays reset-safe.
    std::array<char, 96> m_localSearch{};
    bool m_showInfo = true;
    bool m_showWarn = true;
    bool m_showError = true;
    bool m_autoScroll = true;
    bool m_firstFrame = true;
};

} // namespace fc
