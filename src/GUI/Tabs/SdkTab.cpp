// ============================================================================
//  SdkTab.cpp - built-in SDK console.
// ============================================================================

#include "SdkTab.h"
#include "../../SDK/FCSDK.h"
#include "../../SDK/ModuleLoader.h"

#include "imgui.h"

#include <string>

namespace {

ImVec4 RGBA(unsigned int hex)
{
    return ImVec4(
        ((hex >> 24) & 0xFF) / 255.0f,
        ((hex >> 16) & 0xFF) / 255.0f,
        ((hex >> 8)  & 0xFF) / 255.0f,
        ((hex)       & 0xFF) / 255.0f);
}

bool BeginCard(const char* id, const ImVec2& size)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, RGBA(0x172131F5));
    ImGui::PushStyleColor(ImGuiCol_Border, RGBA(0x2F3D4EFF));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(13.0f, 12.0f));

#if IMGUI_VERSION_NUM >= 19000
    return ImGui::BeginChild(id, size, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
#else
    return ImGui::BeginChild(id, size, true, ImGuiWindowFlags_AlwaysUseWindowPadding);
#endif
}

void EndCard()
{
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void StatusLine(const char* label, const char* value, const ImVec4& color)
{
    ImGui::TextColored(RGBA(0x7F91A0FF), "%s", label);
    ImGui::SameLine(180.0f);
    ImGui::TextColored(color, "%s", value);
}

std::string Narrow(const std::wstring& value)
{
    if (value.empty())
        return {};

    const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1)
        return {};

    std::string result(static_cast<size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), size, nullptr, nullptr);
    return result;
}

} // namespace

namespace fc {

void SdkTab::Render()
{
    ImGui::SeparatorText("SDK Console");

    if (BeginCard("##sdk_status", ImVec2(0.0f, 126.0f)))
    {
        const auto& loader = sdk::ModuleLoader::Get();
        const std::string modsPath = Narrow(loader.ModsDirectory());

        StatusLine("Version", FCSDK_GetVersionString(), RGBA(0x31D3C6FF));
        StatusLine("ABI", "C exports + C++ helpers", RGBA(0xFFCF66FF));
        StatusLine("ImGui", FCSDK_GetImGuiVersion(), RGBA(0x7FF4EAFF));
        StatusLine("Mods", modsPath.empty() ? "mods/" : modsPath.c_str(), RGBA(0xFF7A66FF));
    }
    EndCard();

    ImGui::Spacing();
    ImGui::SeparatorText("Module Quickstart");

    if (BeginCard("##sdk_quickstart", ImVec2(0.0f, 178.0f)))
    {
        ImGui::TextWrapped("Подключи src/SDK/FCSDK.h, заполни FCSDK_TabDesc и вызови FCSDK_RegisterTab(). "
                           "Render-callback вызывается внутри активного ImGui frame, поэтому мод может рисовать "
                           "свои controls напрямую или использовать helpers из FCSDK_UI.h.");
        ImGui::Spacing();
        ImGui::TextColored(RGBA(0x7F91A0FF), "Minimal flow");
        ImGui::BulletText("include FCSDK.h");
        ImGui::BulletText("implement void Render(void*)");
        ImGui::BulletText("call FCSDK_RegisterTab(&desc)");
    }
    EndCard();

    ImGui::Spacing();
    ImGui::SeparatorText("Loaded Modules");
    if (BeginCard("##sdk_modules", ImVec2(0.0f, 120.0f)))
    {
        const auto& loader = sdk::ModuleLoader::Get();
        if (loader.LoadedModules().empty() && loader.FailedModules().empty())
        {
            ImGui::TextColored(RGBA(0x7F91A0FF), "No external modules loaded yet.");
        }
        else
        {
            for (const std::wstring& name : loader.LoadedModules())
                ImGui::TextColored(RGBA(0x31D3C6FF), "loaded  %s", Narrow(name).c_str());

            for (const std::wstring& name : loader.FailedModules())
                ImGui::TextColored(RGBA(0xFF7A66FF), "failed  %s", Narrow(name).c_str());
        }
    }
    EndCard();

    ImGui::Spacing();
    ImGui::TextColored(RGBA(0x7F91A0FF), "Safety scope: UI, overlays, allowed data sources. No anti-cheat bypass helpers.");
}

} // namespace fc
