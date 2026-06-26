// ============================================================================
//  Input - overlay hotkey handling through the hooked window procedure.
// ----------------------------------------------------------------------------
//  Insert toggles the menu. End unloads the module.
//  Keys are stored as variables so KeyBinder can reassign them at runtime.
// ============================================================================

#pragma once

#include <Windows.h>

namespace fc {

class Input
{
public:
    // Called from the hooked window procedure.
    static bool HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    // Current menu toggle virtual-key code.
    static int& ToggleKey();

    // Current unload virtual-key code.
    static int& UnloadKey();
};

} // namespace fc
