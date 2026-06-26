// ============================================================================
//  Fishing Companion — точка входа DLL
// ----------------------------------------------------------------------------
//  Здесь только то, что обязано быть в DllMain: минимум кода, без блокирующих
//  операций и без обращения к загрузчику (loader lock). Вся реальная работа
//  выносится в отдельный поток, который инициализирует оверлей.
// ============================================================================

#include <Windows.h>
#include "Actions/ActionRuntime.h"
#include "Core/Overlay.h"

// Дескриптор нашего модуля — пригодится для корректной выгрузки (FreeLibrary).
HMODULE g_hSelfModule = nullptr;

// Рабочий поток UI. Запускается из DllMain, живёт всё время работы модуля.
static DWORD WINAPI MainThread(LPVOID lpReserved)
{
    // Инициализация оверлея: установка хука Present, поднятие ImGui + DX11.
    // Возвращает false, если что-то пошло не так (например, не нашли SwapChain).
    if (!fc::Overlay::Get().Initialize())
    {
        // TODO: при желании добавьте логирование в файл/OutputDebugString.
        FreeLibraryAndExitThread(g_hSelfModule, 1);
        return 1;
    }

    // Ждём сигнала на выгрузку (например, по горячей клавише END).
    // Сам цикл рендера крутится внутри хука Present, поэтому здесь только ожидание.
    fc::actions::Start();
    fc::Overlay::Get().WaitForShutdown();

    // Аккуратно снимаем хуки и освобождаем ресурсы ImGui/DX11.
    fc::actions::Stop();
    fc::Overlay::Get().Shutdown();

    // Выгружаем DLL из процесса.
    FreeLibraryAndExitThread(g_hSelfModule, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        g_hSelfModule = hModule;

        // Нам не нужны уведомления о потоках — отключаем для производительности.
        DisableThreadLibraryCalls(hModule);

        // Создаём поток. Никакой тяжёлой логики прямо в DllMain быть не должно.
        if (HANDLE hThread = CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr))
            CloseHandle(hThread);
        break;

    case DLL_PROCESS_DETACH:
        if (lpReserved == nullptr)
            fc::Overlay::Get().RequestShutdown();
        // Если процесс закрывается резко — основная очистка уже не критична,
        // ОС освободит память. Грациозную выгрузку делает MainThread.
        break;
    }
    return TRUE;
}
