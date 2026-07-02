// ============================================================================
//  SettingsTab.cpp - hotkeys and overlay appearance.
// ============================================================================

#include "SettingsTab.h"
#include "../UI.h"
#include "../../Core/Input.h"
#include "../../Features/KeyBinder.h"
#include "../../Features/Settings.h"
#include "../../Actions/ActionRuntime.h"

#include "imgui.h"

namespace ui = fc::gui::ui;

namespace fc {

void SettingsTab::Render()
{
    auto& settings = Settings::Get();

    ImGui::SeparatorText("Hotkeys");

    if (ui::BeginCard("##hotkey_card", ImVec2(0.0f, 158.0f)))
    {
        int& toggleKey = Input::ToggleKey();
        int& unloadKey = Input::UnloadKey();
        int& autoFishKey = Input::AutoFishKey();

        if (KeyBinder::Draw("Open/close menu", &toggleKey))
        {
            settings.SetToggleKey(toggleKey);
            settings.CaptureFromRuntime(); // keep the store in sync with the live keys
        }
        if (KeyBinder::Draw("Unload module", &unloadKey))
        {
            settings.SetUnloadKey(unloadKey);
            settings.CaptureFromRuntime();
        }
        if (KeyBinder::Draw("Auto Fish", &autoFishKey))
        {
            settings.SetAutoFishKey(autoFishKey);
            settings.CaptureFromRuntime();
        }
    }
    ui::EndCard();

    ImGui::Spacing();
    ImGui::SeparatorText("Appearance");

    if (ui::BeginCard("##appearance_card", ImVec2(0.0f, 128.0f)))
    {
        float uiScale = settings.UiScale();
        ImGui::SetNextItemWidth(240.0f);
        if (ImGui::SliderFloat("Interface scale", &uiScale, 0.8f, 1.6f, "%.2f"))
        {
            settings.SetUiScale(uiScale);
            ImGui::GetIO().FontGlobalScale = settings.UiScale();
        }

        ImGui::SameLine();
        if (ImGui::Button("Reset", ImVec2(82.0f, 0.0f)))
        {
            settings.SetUiScale(1.0f);
            ImGui::GetIO().FontGlobalScale = 1.0f;
        }

        ImGui::Spacing();
        ImGui::TextColored(
            Color(Palette::TextMuted),
            "Menu: %s   Unload: %s   Auto Fish: %s",
            KeyBinder::KeyName(Input::ToggleKey()),
            KeyBinder::KeyName(Input::UnloadKey()),
            KeyBinder::KeyName(Input::AutoFishKey()));
    }
    ui::EndCard();

    // -- Autonomous fishing tuning ---------------------------------------
    // Live knobs for the AutoFish FSM. Values are clamped by SetAutoFishParams
    // so out-of-range slider input cannot break the state machine.
    ImGui::Spacing();
    ImGui::SeparatorText("Auto Fish");

    const actions::Status fishStatus = actions::GetStatus();
    if (ui::BeginCard("##autofish_card", ImVec2(0.0f, 248.0f)))
    {
        ImGui::TextColored(
            fishStatus.auto_fish_enabled ? Color(Palette::Amber) : Color(Palette::TextMuted),
            "auto fish: %s   state: %s   cycles: %llu",
            fishStatus.auto_fish_enabled ? "on" : "off",
            fishStatus.auto_fish_state.empty() ? "-" : fishStatus.auto_fish_state.c_str(),
            fishStatus.auto_fish_cycles);
        ImGui::TextColored(
            Color(Palette::TextMuted),
            "live rod load: %.3f   reel: %.3f",
            fishStatus.auto_fish_rod_load,
            fishStatus.auto_fish_reel_value);

        ImGui::Spacing();
        actions::AutoFishParams params = actions::GetAutoFishParams();
        bool changed = false;

        ImGui::SetNextItemWidth(240.0f);
        if (ImGui::SliderFloat("Bite load threshold", &params.bite_load_threshold, 0.05f, 1.5f, "%.2f"))
            changed = true;
        ImGui::SameLine();
        ImGui::TextColored(Color(Palette::TextMuted), "Rod+0x110 over baseline = bite");

        ImGui::SetNextItemWidth(240.0f);
        if (ImGui::SliderFloat("Fight load danger", &params.fight_load_danger,
                               params.bite_load_threshold + 0.05f, 2.0f, "%.2f"))
            changed = true;
        ImGui::SameLine();
        ImGui::TextColored(Color(Palette::TextMuted), "ease off above this");

        ImGui::SetNextItemWidth(240.0f);
        if (ImGui::SliderInt("Bite confirm (ms)", &params.bite_confirm_ms, 0, 1500))
            changed = true;
        ImGui::SetNextItemWidth(240.0f);
        if (ImGui::SliderInt("Hook hold (ms)", &params.hook_hold_ms, 100, 2000))
            changed = true;
        ImGui::SetNextItemWidth(240.0f);
        if (ImGui::SliderInt("Bite timeout (s)", &params.bite_timeout_s, 5, 600))
            changed = true;

        if (changed)
            actions::SetAutoFishParams(params);

        ImGui::Spacing();
        if (ImGui::Button(fishStatus.auto_fish_enabled ? "Stop Auto Fish" : "Start Auto Fish",
                          ImVec2(180.0f, 0.0f)))
        {
            actions::SetAutoFish(!fishStatus.auto_fish_enabled);
        }
    }
    ui::EndCard();
}

} // namespace fc
