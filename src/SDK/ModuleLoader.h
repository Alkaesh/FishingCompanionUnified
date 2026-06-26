// ============================================================================
//  ModuleLoader.h - runtime loader for SDK modules.
// ============================================================================

#pragma once

#include <Windows.h>

#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace fc {
class ITab;
}

namespace fc::sdk {

class ModuleLoader
{
public:
    static ModuleLoader& Get();

    void LoadAll(HMODULE hostModule);
    void UnloadAll();
    bool RegisterTab(std::unique_ptr<fc::ITab> tab);

    bool HasLoaded() const { return m_scanned; }
    const std::wstring& ModsDirectory() const { return m_modsDirectory; }
    const std::vector<std::wstring>& LoadedModules() const { return m_loadedModuleNames; }
    const std::vector<std::wstring>& FailedModules() const { return m_failedModuleNames; }

private:
    struct LoadedModule
    {
        HMODULE handle = nullptr;
        void (*shutdown)() = nullptr;
    };

    ModuleLoader() = default;
    void BeginModuleInit(HMODULE module);
    void CommitModuleInit(HMODULE module);
    void RollbackModuleInit();

    bool m_scanned = false;
    HMODULE m_loadingModule = nullptr;
    std::thread::id m_loadingThread{};
    std::vector<std::unique_ptr<fc::ITab>> m_pendingTabs;
    std::wstring m_modsDirectory;
    std::vector<LoadedModule> m_modules;
    std::vector<std::wstring> m_loadedModuleNames;
    std::vector<std::wstring> m_failedModuleNames;
};

} // namespace fc::sdk
