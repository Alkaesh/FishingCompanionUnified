// ============================================================================
//  ITab - menu tab interface.
// ============================================================================

#pragma once

namespace fc {

class ITab
{
public:
    virtual ~ITab() = default;

    virtual const char* Title() const = 0;

    // Extra words used by the top search box to find this tab.
    virtual const char* SearchKeywords() const { return ""; }

    virtual void Render() = 0;
};

} // namespace fc
