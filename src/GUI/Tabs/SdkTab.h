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
    const char* SearchKeywords() const override { return "sdk modules plugins loader abi imgui quickstart events"; }
    void Render() override;
};

} // namespace fc
