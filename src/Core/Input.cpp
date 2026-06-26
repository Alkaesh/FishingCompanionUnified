// ============================================================================
//  Input.cpp — обработка горячих клавиш модуля.
// ============================================================================

#include "Input.h"
#include "Overlay.h"

namespace fc {

int& Input::ToggleKey()
{
    static int key = VK_INSERT; // Клавиша вызова меню по умолчанию.
    return key;
}

int& Input::UnloadKey()
{
    static int key = VK_END;    // Клавиша выгрузки модуля.
    return key;
}

bool Input::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg != WM_KEYDOWN && msg != WM_SYSKEYDOWN)
        return false;

    const int key = static_cast<int>(wParam);
    const bool isHotkey = (key == ToggleKey() || key == UnloadKey());
    if (!isHotkey)
        return false;

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

    // TODO: здесь же можно опрашивать пользовательские хоткеи из Keybinder,
    //       чтобы скрывать/показывать отдельные элементы HUD.
    return false;
}

} // namespace fc
