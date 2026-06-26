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
    struct ModuleRecord
    {
        std::wstring name;
        bool loaded = false;
        std::string detail;
    };

    struct Event
    {
        std::string level;
        std::string message;
    };

    static ModuleLoader& Get();

    void LoadAll(HMODULE hostModule);
    void UnloadAll();
    bool RegisterTab(std::unique_ptr<fc::ITab> tab);

    bool HasLoaded() const { return m_scanned; }
    const std::wstring& ModsDirectory() const { return m_modsDirectory; }
    const std::vector<std::wstring>& LoadedModules() const { return m_loadedModuleNames; }
    const std::vector<std::wstring>& FailedModules() const { return m_failedModuleNames; }
    const std::vector<ModuleRecord>& ModuleRecords() const { return m_moduleRecords; }
    const std::vector<Event>& Events() const { return m_events; }

private:
    struct LoadedModule
    {
        HMODULE handle = nullptr;
        void (*shutdown)() = nullptr;
        std::wstring name;
    };

    ModuleLoader() = default;
    void BeginModuleInit(HMODULE module);
    void CommitModuleInit(HMODULE module);
    void RollbackModuleInit();
    void RecordEvent(std::string level, std::string message);
    void RecordModule(std::wstring name, bool loaded, std::string detail);

    bool m_scanned = false;
    HMODULE m_loadingModule = nullptr;
    std::thread::id m_loadingThread{};
    std::vector<std::unique_ptr<fc::ITab>> m_pendingTabs;
    std::wstring m_modsDirectory;
    std::vector<LoadedModule> m_modules;
    std::vector<std::wstring> m_loadedModuleNames;
    std::vector<std::wstring> m_failedModuleNames;
    std::vector<ModuleRecord> m_moduleRecords;
    std::vector<Event> m_events;
};

} // namespace fc::sdk
