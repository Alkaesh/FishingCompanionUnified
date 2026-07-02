// ============================================================================
//  KeyBinder - reusable ImGui hotkey assignment control.
// ----------------------------------------------------------------------------
//  Usage: KeyBinder::Draw("Label", &myVirtualKey).
//  Click the button, press a key, and the virtual-key code is stored.
// ============================================================================

#pragma once

namespace fc {

class KeyBinder
{
public:
    // Returns true when the key changed during this frame.
    static bool Draw(const char* label, int* outKey);

    // Human-readable name for a virtual-key code.
    static const char* KeyName(int vk);
};

} // namespace fc
