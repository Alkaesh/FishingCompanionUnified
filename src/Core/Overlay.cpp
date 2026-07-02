// ============================================================================
//  Overlay.cpp — реализация хука Present (DX11) и цикла рендера ImGui.
// ----------------------------------------------------------------------------
//  Используется MinHook (https://github.com/TsudaKageyu/minhook).
//  Адрес Present берём через временный (dummy) SwapChain — стандартный приём,
//  чтобы не ждать, пока игра сама создаст устройство.
// ============================================================================

#include "Overlay.h"
#include "Input.h"
#include "../GUI/Menu.h"
#include "../GUI/Theme.h"
#include "../SDK/ModuleLoader.h"

#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include "MinHook.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#pragma comment(lib, "d3d11.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
extern HMODULE g_hSelfModule;

namespace fc {

// Индексы методов в vtable IDXGISwapChain (COM-интерфейс).
//   8  -> Present
//   13 -> ResizeBuffers
static constexpr int kPresentIndex       = 8;
static constexpr int kResizeBuffersIndex = 13;

// Типы оригинальных функций.
using PresentFn       = long(__stdcall*)(IDXGISwapChain*, UINT, UINT);
using ResizeBuffersFn = long(__stdcall*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

static PresentFn       o_Present       = nullptr;
static ResizeBuffersFn o_ResizeBuffers = nullptr;

// Сохранённая оригинальная оконная процедура игры (для восстановления ввода).
static WNDPROC o_WndProc = nullptr;

// ImGui сам предоставляет обработчик оконных сообщений.

// ----------------------------------------------------------------------------
//  Перехваченная оконная процедура: кормим ImGui вводом и ловим горячие клавиши.
// ----------------------------------------------------------------------------
static LRESULT __stdcall hkWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Обработка глобальных горячих клавиш (Insert — меню, End — выгрузка).
    if (fc::Input::HandleMessage(msg, wParam, lParam))
        return TRUE;

    // Когда меню открыто — отдаём ввод ImGui и не пускаем его в игру.
    if (Overlay::Get().IsMenuVisible())
    {
        ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
        // Блокируем клики/клавиши мыши и клавиатуры от попадания в игру,
        // когда курсор взаимодействует с нашим меню.
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse || io.WantCaptureKeyboard)
            return TRUE;
    }

    if (o_WndProc)
        return CallWindowProc(o_WndProc, hWnd, msg, wParam, lParam);

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// ----------------------------------------------------------------------------
//  Синглтон.
// ----------------------------------------------------------------------------
Overlay& Overlay::Get()
{
    static Overlay instance;
    return instance;
}

void Overlay::ToggleMenu()
{
    SetMenuVisible(!m_menuVisible.load());
}

void Overlay::SetMenuVisible(bool visible)
{
    const unsigned long renderThreadId = m_renderThreadId.load();
    if (renderThreadId != 0 && GetCurrentThreadId() != renderThreadId)
    {
        m_menuVisible.store(visible);
        m_pendingMenuVisible.store(visible ? 1 : 0);
        return;
    }

    ApplyMenuVisible(visible);
}

void Overlay::ApplyMenuVisible(bool visible)
{
    m_menuVisible.store(visible);
    UpdateMenuInputState();
}

void Overlay::ApplyPendingMenuVisible()
{
    const int pending = m_pendingMenuVisible.exchange(-1);
    if (pending >= 0)
        ApplyMenuVisible(pending != 0);
}

void Overlay::UpdateMenuInputState()
{
    if (!ImGui::GetCurrentContext())
        return;

    ImGuiIO& io = ImGui::GetIO();
    io.MouseDrawCursor = m_menuVisible.load();

    if (m_menuVisible.load())
    {
        ClipCursor(nullptr);
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
    }
}

bool Overlay::EnterHook()
{
    if (m_shuttingDown.load())
        return false;

    m_inFlightHooks.fetch_add(1);
    if (m_shuttingDown.load())
    {
        LeaveHook();
        return false;
    }

    return true;
}

void Overlay::LeaveHook()
{
    m_inFlightHooks.fetch_sub(1);
}

void Overlay::WaitForHookDrain()
{
    while (m_inFlightHooks.load() > 0)
        Sleep(1);
}

// ----------------------------------------------------------------------------
//  Получение адреса Present через временный SwapChain.
// ----------------------------------------------------------------------------
static bool GetSwapChainVTable(void**& outVTable)
{
    WNDCLASSEXA wc = { sizeof(WNDCLASSEXA), CS_HREDRAW | CS_VREDRAW, DefWindowProcA,
                       0, 0, GetModuleHandle(nullptr), nullptr, nullptr, nullptr,
                       nullptr, "FC_DummyWnd", nullptr };
    RegisterClassExA(&wc);
    HWND hwnd = CreateWindowA(wc.lpszClassName, "", WS_OVERLAPPEDWINDOW,
                              0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount       = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow      = hwnd;
    sd.SampleDesc.Count  = 1;
    sd.Windowed          = TRUE;
    sd.SwapEffect        = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL       featureLevel;
    IDXGISwapChain*         swapChain = nullptr;
    ID3D11Device*           device    = nullptr;
    ID3D11DeviceContext*    context   = nullptr;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        levels, _countof(levels), D3D11_SDK_VERSION,
        &sd, &swapChain, &device, &featureLevel, &context);

    bool ok = false;
    if (SUCCEEDED(hr))
    {
        // vtable объекта = первый указатель в памяти COM-объекта.
        outVTable = *reinterpret_cast<void***>(swapChain);
        ok = true;
    }

    if (swapChain) swapChain->Release();
    if (device)    device->Release();
    if (context)   context->Release();
    DestroyWindow(hwnd);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);
    return ok;
}

// ----------------------------------------------------------------------------
//  Хук Present: первая отрисовка инициализирует ImGui, далее — кадр оверлея.
// ----------------------------------------------------------------------------
long __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT syncInterval, UINT flags)
{
    Overlay& ov = Overlay::Get();
    if (!ov.EnterHook())
        return o_Present(pSwapChain, syncInterval, flags);

    if (!ov.m_initialized)
    {
        if (!ov.InitImGuiOnce(pSwapChain))
        {
            ov.LeaveHook();
            return o_Present(pSwapChain, syncInterval, flags);
        }
    }

    ov.RenderFrame();
    ov.LeaveHook();
    return o_Present(pSwapChain, syncInterval, flags);
}

// ----------------------------------------------------------------------------
//  Хук ResizeBuffers: при ресайзе окна пересоздаём render target view.
// ----------------------------------------------------------------------------
long __stdcall hkResizeBuffers(IDXGISwapChain* pSwapChain, UINT bufferCount,
                               UINT width, UINT height, DXGI_FORMAT format, UINT flags)
{
    Overlay& ov = Overlay::Get();
    if (!ov.EnterHook())
        return o_ResizeBuffers(pSwapChain, bufferCount, width, height, format, flags);

    ov.ReleaseRenderTarget();

    long hr = o_ResizeBuffers(pSwapChain, bufferCount, width, height, format, flags);

    if (ov.m_initialized && SUCCEEDED(hr))
        ov.CreateRenderTarget(pSwapChain);

    ov.LeaveHook();
    return hr;
}

// ----------------------------------------------------------------------------
//  Ленивая инициализация ImGui + DX11.
// ----------------------------------------------------------------------------
bool Overlay::InitImGuiOnce(IDXGISwapChain* pSwapChain)
{
    m_renderThreadId.store(GetCurrentThreadId());

    if (FAILED(pSwapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&m_device))))
        return false;

    m_device->GetImmediateContext(&m_context);

    DXGI_SWAP_CHAIN_DESC sd;
    pSwapChain->GetDesc(&sd);
    m_window = sd.OutputWindow;

    CreateRenderTarget(pSwapChain);

    // Контекст ImGui + конфигурация.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.IniFilename = nullptr; // Не плодим imgui.ini рядом с игрой.

    // Load font before backend font textures are created.
    fc::Theme::LoadFonts();
    fc::Theme::Apply();

    ImGui_ImplWin32_Init(m_window);
    ImGui_ImplDX11_Init(m_device, m_context);

    // Перехватываем оконную процедуру для ввода.
    o_WndProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtr(static_cast<HWND>(m_window), GWLP_WNDPROC,
                         reinterpret_cast<LONG_PTR>(hkWndProc)));

    // Регистрируем стандартные вкладки в меню.
    fc::Menu::Get().RegisterDefaultTabs();
    fc::sdk::ModuleLoader::Get().LoadAll(g_hSelfModule);

    m_initialized = true;
    return true;
}

void Overlay::CreateRenderTarget(IDXGISwapChain* pSwapChain)
{
    ID3D11Texture2D* backBuffer = nullptr;
    if (SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                        reinterpret_cast<void**>(&backBuffer))) && backBuffer)
    {
        m_device->CreateRenderTargetView(backBuffer, nullptr, &m_rtv);
        backBuffer->Release();
    }
}

void Overlay::ReleaseRenderTarget()
{
    if (m_rtv) { m_rtv->Release(); m_rtv = nullptr; }
}

// ----------------------------------------------------------------------------
//  Отрисовка одного кадра оверлея.
// ----------------------------------------------------------------------------
void Overlay::RenderFrame()
{
    m_renderThreadId.store(GetCurrentThreadId());
    ApplyPendingMenuVisible();

    // TODO: сюда добавьте вызов вашей функции обновления данных, например:
    // чтобы цифры в дашборде обновлялись каждый кадр.

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    UpdateMenuInputState();

    if (m_menuVisible.load())
        fc::Menu::Get().Render();

    // TODO: здесь же можно рисовать постоянный HUD (поверх игры, без окна),
    //       который виден даже при закрытом меню.

    ImGui::Render();
    if (!m_context || !m_rtv)
        return;

    m_context->OMSetRenderTargets(1, &m_rtv, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

// ----------------------------------------------------------------------------
//  Установка хуков.
// ----------------------------------------------------------------------------
bool Overlay::Initialize()
{
    if (MH_Initialize() != MH_OK)
        return false;

    void** vtable = nullptr;
    if (!GetSwapChainVTable(vtable))
    {
        MH_Uninitialize();
        return false;
    }

    // Хук Present.
    if (MH_CreateHook(vtable[kPresentIndex],
                      reinterpret_cast<void*>(&hkPresent),
                      reinterpret_cast<void**>(&o_Present)) != MH_OK)
    {
        MH_Uninitialize();
        return false;
    }

    // Хук ResizeBuffers (корректная работа при смене разрешения/окна).
    if (MH_CreateHook(vtable[kResizeBuffersIndex],
                      reinterpret_cast<void*>(&hkResizeBuffers),
                      reinterpret_cast<void**>(&o_ResizeBuffers)) != MH_OK)
    {
        MH_RemoveHook(vtable[kPresentIndex]);
        MH_Uninitialize();
        return false;
    }

    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK)
    {
        MH_RemoveHook(vtable[kPresentIndex]);
        MH_RemoveHook(vtable[kResizeBuffersIndex]);
        MH_Uninitialize();
        return false;
    }

    m_hooksActive = true;
    return true;
}

void Overlay::WaitForShutdown()
{
    while (!m_shutdownRequested)
    {
        if ((GetAsyncKeyState(fc::Input::UnloadKey()) & 1) != 0)
            RequestShutdown();
        Sleep(50);
    }
}

// ----------------------------------------------------------------------------
//  Выгрузка: снимаем хуки, восстанавливаем WndProc, чистим ImGui.
// ----------------------------------------------------------------------------
void Overlay::Shutdown()
{
    m_shuttingDown.store(true);

    if (m_hooksActive)
    {
        MH_DisableHook(MH_ALL_HOOKS);
        WaitForHookDrain();
        MH_Uninitialize();
        m_hooksActive = false;
    }

    // Возвращаем родную оконную процедуру.
    if (m_window && o_WndProc)
        SetWindowLongPtr(static_cast<HWND>(m_window), GWLP_WNDPROC,
                         reinterpret_cast<LONG_PTR>(o_WndProc));

    if (m_initialized)
    {
        fc::sdk::ModuleLoader::Get().UnloadAll();
        ImGui::GetIO().MouseDrawCursor = false;
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    ReleaseRenderTarget();
    if (m_context) { m_context->Release(); m_context = nullptr; }
    if (m_device)  { m_device->Release();  m_device  = nullptr; }

    // Небольшая пауза, чтобы крутящийся Present успел выйти из нашего кода.
    m_initialized = false;
}

} // namespace fc
