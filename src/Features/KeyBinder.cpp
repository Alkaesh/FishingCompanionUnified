// ============================================================================
//  KeyBinder.cpp - reusable ImGui hotkey assignment control.
// ============================================================================

#include "KeyBinder.h"

#include <Windows.h>
#include "imgui.h"

namespace fc {

const char* KeyBinder::KeyName(int vk)
{
    static char name[32];

    if (vk == 0)
        return "None";

    // Letters and digits are displayed as their printable character.
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9'))
    {
        name[0] = static_cast<char>(vk);
        name[1] = '\0';
        return name;
    }

    switch (vk)
    {
    case VK_INSERT: return "Insert";
    case VK_DELETE: return "Delete";
    case VK_HOME:   return "Home";
    case VK_END:    return "End";
    case VK_LEFT:   return "Left";
    case VK_RIGHT:  return "Right";
    case VK_UP:     return "Up";
    case VK_DOWN:   return "Down";
    case VK_PRIOR:  return "PageUp";
    case VK_NEXT:   return "PageDown";
    case VK_F1: return "F1"; case VK_F2: return "F2"; case VK_F3: return "F3";
    case VK_F4: return "F4"; case VK_F5: return "F5"; case VK_F6: return "F6";
    case VK_F7: return "F7"; case VK_F8: return "F8"; case VK_F9: return "F9";
    case VK_F10: return "F10"; case VK_F11: return "F11"; case VK_F12: return "F12";
    case VK_SPACE:   return "Space";
    case VK_RETURN:  return "Enter";
    case VK_TAB:     return "Tab";
    case VK_BACK:    return "Backspace";
    case VK_SHIFT:   return "Shift";
    case VK_CONTROL: return "Ctrl";
    case VK_MENU:    return "Alt";
    case VK_LSHIFT:   return "LShift";
    case VK_RSHIFT:   return "RShift";
    case VK_LCONTROL: return "LCtrl";
    case VK_RCONTROL: return "RCtrl";
    case VK_LMENU:    return "LAlt";
    case VK_RMENU:    return "RAlt";
    default:
        wsprintfA(name, "VK_0x%02X", vk);
        return name;
    }
}

bool KeyBinder::Draw(const char* label, int* outKey)
{
    if (!label || !outKey)
        return false;

    bool changed = false;

    ImGui::PushID(outKey);
    const float buttonWidth = 170.0f;
    const float rowStartX = ImGui::GetCursorPosX();
    const float availableWidth = ImGui::GetContentRegionAvail().x;

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    if (availableWidth >= 340.0f)
        ImGui::SameLine(rowStartX + availableWidth - buttonWidth);

    // Each key slot needs its own "listening for input" state.
    static const void* s_listeningId = nullptr;
    const bool listening = (s_listeningId == outKey);

    const char* caption = listening ? "[ press a key ]" : KeyName(*outKey);

    if (ImGui::Button(caption, ImVec2(buttonWidth, 0.0f)))
        s_listeningId = outKey;

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Esc - cancel, Backspace/Delete - clear");

    if (listening)
    {
        // Scan virtual-key codes and capture the first pressed key.
        for (int vk = 0x08; vk <= 0xFE; ++vk)
        {
            if (GetAsyncKeyState(vk) & 0x8000)
            {
                if (vk == VK_ESCAPE)
                {
                    s_listeningId = nullptr;
                    break;
                }

                if (vk == VK_BACK || vk == VK_DELETE)
                {
                    *outKey = 0;
                    s_listeningId = nullptr;
                    changed = true;
                    break;
                }

                *outKey = vk;
                s_listeningId = nullptr;
                changed = true;
                break;
            }
        }
    }

    ImGui::PopID();
    return changed;
}

} // namespace fc
