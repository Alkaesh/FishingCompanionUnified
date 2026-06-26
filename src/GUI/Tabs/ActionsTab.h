#pragma once

#include "../ITab.h"

namespace fc {

class ActionsTab : public ITab
{
public:
    const char* Title() const override { return "Actions"; }
    void Render() override;
};

} // namespace fc
