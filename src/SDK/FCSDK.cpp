// ============================================================================
//  FCSDK.cpp - exported SDK entry points.
// ============================================================================

#include "FCSDK.h"

#include "../Core/Overlay.h"
#include "../GUI/ITab.h"
#include "../GUI/Menu.h"
#include "ModuleLoader.h"

#include "imgui.h"

#include <memory>
#include <string>

namespace {

class CallbackTab final : public fc::ITab
{
public:
    explicit CallbackTab(const FCSDK_TabDesc& desc)
        : m_title(desc.title ? desc.title : "SDK Tab"),
          m_render(desc.render),
          m_userData(desc.user_data)
    {
    }

    const char* Title() const override
    {
        return m_title.c_str();
    }

    void Render() override
    {
        if (m_render)
            m_render(m_userData);
    }

private:
    std::string m_title;
    FCSDK_RenderCallback m_render = nullptr;
    void* m_userData = nullptr;
};

bool IsValidTabDesc(const FCSDK_TabDesc* desc)
{
    return desc &&
           desc->size >= sizeof(FCSDK_TabDesc) &&
           desc->title &&
           desc->title[0] != '\0' &&
           desc->render;
}

} // namespace

extern "C" {

FCSDK_API uint32_t FCSDK_GetVersion(void)
{
    return (FCSDK_VERSION_MAJOR << 16) | (FCSDK_VERSION_MINOR << 8) | FCSDK_VERSION_PATCH;
}

FCSDK_API const char* FCSDK_GetVersionString(void)
{
    return "0.2.0";
}

FCSDK_API void* FCSDK_GetImGuiContext(void)
{
    return ImGui::GetCurrentContext();
}

FCSDK_API const char* FCSDK_GetImGuiVersion(void)
{
    return ImGui::GetVersion();
}

FCSDK_API FCSDK_Bool FCSDK_RegisterTab(const FCSDK_TabDesc* desc)
{
    if (!IsValidTabDesc(desc))
        return FCSDK_FALSE;

    return fc::sdk::ModuleLoader::Get().RegisterTab(std::make_unique<CallbackTab>(*desc))
        ? FCSDK_TRUE
        : FCSDK_FALSE;
}

FCSDK_API void FCSDK_SetMenuVisible(FCSDK_Bool visible)
{
    fc::Overlay::Get().SetMenuVisible(visible == FCSDK_TRUE);
}

FCSDK_API FCSDK_Bool FCSDK_IsMenuVisible(void)
{
    return fc::Overlay::Get().IsMenuVisible() ? FCSDK_TRUE : FCSDK_FALSE;
}

FCSDK_API void FCSDK_RequestShutdown(void)
{
    fc::Overlay::Get().RequestShutdown();
}

} // extern "C"
