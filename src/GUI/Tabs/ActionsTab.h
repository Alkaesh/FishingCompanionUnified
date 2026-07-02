#pragma once

#include "../ITab.h"

namespace fc {

class ActionsTab : public ITab
{
public:
    const char* Title() const override { return "Actions"; }
    const char* SearchKeywords() const override;
    void Render() override;
};

} // namespace fc
