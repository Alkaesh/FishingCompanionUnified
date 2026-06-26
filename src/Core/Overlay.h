#pragma once

#include <dxgiformat.h>
#include <atomic>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11RenderTargetView;
struct IDXGISwapChain;

namespace fc {

class Overlay
{
public:
    static Overlay& Get();

    bool Initialize();
    void Shutdown();
    void WaitForShutdown();

    void RequestShutdown() { m_shutdownRequested = true; }

    void ToggleMenu();
    void SetMenuVisible(bool visible);
    bool IsMenuVisible() const { return m_menuVisible.load(); }

private:
    Overlay() = default;
    ~Overlay() = default;
    Overlay(const Overlay&) = delete;
    Overlay& operator=(const Overlay&) = delete;

    friend long __stdcall hkPresent(IDXGISwapChain*, unsigned int, unsigned int);
    friend long __stdcall hkResizeBuffers(IDXGISwapChain*, unsigned int, unsigned int,
                                          unsigned int, DXGI_FORMAT, unsigned int);

    bool InitImGuiOnce(IDXGISwapChain* pSwapChain);
    void RenderFrame();
    void UpdateMenuInputState();
    void ApplyMenuVisible(bool visible);
    void ApplyPendingMenuVisible();
    bool EnterHook();
    void LeaveHook();
    void WaitForHookDrain();

    void ReleaseRenderTarget();
    void CreateRenderTarget(IDXGISwapChain* pSwapChain);

private:
    bool m_initialized = false;
    bool m_hooksActive = false;
    std::atomic<bool> m_menuVisible{ true };
    std::atomic<bool> m_shutdownRequested{ false };
    std::atomic<bool> m_shuttingDown{ false };
    std::atomic<int> m_inFlightHooks{ 0 };
    std::atomic<int> m_pendingMenuVisible{ -1 };
    std::atomic<unsigned long> m_renderThreadId{ 0 };

    ID3D11Device*           m_device   = nullptr;
    ID3D11DeviceContext*    m_context  = nullptr;
    ID3D11RenderTargetView* m_rtv      = nullptr;
    void*                   m_window   = nullptr;
};

} // namespace fc
