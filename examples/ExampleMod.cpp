// ============================================================================
//  ExampleMod.cpp - minimal external Fishing Companion SDK module.
// ============================================================================

#include "../src/SDK/FCSDK.h"
#include "../src/SDK/FCSDK_UI.h"

#include "imgui.h"

namespace {

int g_counter = 0;
bool g_enabled = true;

void RenderExampleTab(void*)
{
    void* context = FCSDK_GetImGuiContext();
    if (!context)
        return;

    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(context));

    namespace ui = fc::sdk::ui;

    ui::Section("Example Module");

    if (ui::BeginCard("##example_state", ImVec2(0.0f, 128.0f)))
    {
        ui::Metric("Status", g_enabled ? "Enabled" : "Disabled", g_enabled ? ui::Accent::Cyan : ui::Accent::Coral);
        ImGui::Checkbox("Enable module", &g_enabled);

        if (ImGui::Button("Increment counter", ImVec2(180.0f, 0.0f)))
            ++g_counter;

        ImGui::SameLine();
        ImGui::Text("Counter: %d", g_counter);
    }
    ui::EndCard();

    ImGui::Spacing();
    ImGui::TextWrapped("This tab was registered from an SDK module through FCSDK_RegisterTab.");
}

} // namespace

extern "C" __declspec(dllexport) FCSDK_Bool FCSDK_ModuleInit()
{
    FCSDK_TabDesc tab{};
    tab.size = sizeof(tab);
    tab.title = "Example Mod";
    tab.render = &RenderExampleTab;
    tab.user_data = nullptr;

    return FCSDK_RegisterTab(&tab);
}
