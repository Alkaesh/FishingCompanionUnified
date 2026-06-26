// ============================================================================
//  Input.cpp - overlay hotkey handling.
// ============================================================================

#include "Input.h"
#include "Overlay.h"
#include "../Actions/ActionRuntime.h"

namespace fc {

int& Input::ToggleKey()
{
    static int key = VK_INSERT; // Default menu toggle key.
    return key;
}

int& Input::UnloadKey()
{
    static int key = VK_END;    // Default module unload key.
    return key;
}

int& Input::AutoFishKey()
{
    static int key = VK_F9;     // Default AutoFish toggle key.
    return key;
}

bool Input::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg != WM_KEYDOWN && msg != WM_SYSKEYDOWN)
        return false;

    const int key = static_cast<int>(wParam);
    const bool isHotkey = (key == ToggleKey() || key == UnloadKey() || key == AutoFishKey());
    if (!isHotkey)
        return false;

    // Suppress key auto-repeat: only act on the initial press.
    if ((static_cast<unsigned long long>(lParam) & (1ull << 30)) != 0)
        return true;

    if (key == ToggleKey())
    {
        Overlay::Get().ToggleMenu();
        return true;
    }

    if (key == UnloadKey())
    {
        Overlay::Get().RequestShutdown();
        return true;
    }

    if (key == AutoFishKey())
    {
        // Toggle the autonomous fishing FSM directly. This works even before
        // the menu is open, so fishing can be started hands-free.
        actions::SetAutoFish(!actions::IsAutoFishEnabled());
        return true;
    }

    return false;
}

} // namespace fc
