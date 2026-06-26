// ============================================================================
//  ModuleLoader.cpp - runtime loader for SDK modules.
// ============================================================================

#include "ModuleLoader.h"

#include "FCSDK.h"
#include "../GUI/Menu.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace {

fs::path GetHostDirectory(HMODULE hostModule)
{
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(hostModule, buffer, MAX_PATH);
    if (length == 0)
        return fs::current_path();

    return fs::path(buffer).parent_path();
}

} // namespace

namespace fc::sdk {

ModuleLoader& ModuleLoader::Get()
{
    static ModuleLoader instance;
    return instance;
}

void ModuleLoader::BeginModuleInit(HMODULE module)
{
    m_loadingModule = module;
    m_loadingThread = std::this_thread::get_id();
    m_pendingTabs.clear();
}

void ModuleLoader::CommitModuleInit(HMODULE module)
{
    for (std::unique_ptr<fc::ITab>& tab : m_pendingTabs)
        fc::Menu::Get().AddTab(std::move(tab), module);

    m_pendingTabs.clear();
    m_loadingModule = nullptr;
    m_loadingThread = {};
}

void ModuleLoader::RollbackModuleInit()
{
    m_pendingTabs.clear();
    m_loadingModule = nullptr;
    m_loadingThread = {};
}

bool ModuleLoader::RegisterTab(std::unique_ptr<fc::ITab> tab)
{
    if (!tab || !m_loadingModule || m_loadingThread != std::this_thread::get_id())
        return false;

    m_pendingTabs.push_back(std::move(tab));
    return true;
}

void ModuleLoader::LoadAll(HMODULE hostModule)
{
    if (m_scanned)
        return;

    m_scanned = true;
    const fs::path modsPath = GetHostDirectory(hostModule) / L"mods";
    m_modsDirectory = modsPath.wstring();

    std::error_code ec;
    fs::create_directories(modsPath, ec);
    if (ec || !fs::exists(modsPath))
        return;

    for (const fs::directory_entry& entry : fs::directory_iterator(modsPath, ec))
    {
        if (ec)
            break;

        if (!entry.is_regular_file() || entry.path().extension() != L".dll")
            continue;

        const std::wstring moduleName = entry.path().filename().wstring();
        HMODULE module = LoadLibraryW(entry.path().c_str());
        if (!module)
        {
            m_failedModuleNames.push_back(moduleName);
            continue;
        }

        auto init = reinterpret_cast<FCSDK_ModuleInitFn>(GetProcAddress(module, FCSDK_MODULE_INIT_NAME));
        auto shutdown = reinterpret_cast<FCSDK_ModuleShutdownFn>(
            GetProcAddress(module, FCSDK_MODULE_SHUTDOWN_NAME));

        BeginModuleInit(module);
        const bool initialized = init && init() == FCSDK_TRUE;
        if (!initialized)
        {
            RollbackModuleInit();
            FreeLibrary(module);
            m_failedModuleNames.push_back(moduleName);
            continue;
        }

        CommitModuleInit(module);
        m_modules.push_back(LoadedModule{module, shutdown});
        m_loadedModuleNames.push_back(moduleName);
    }
}

void ModuleLoader::UnloadAll()
{
    for (LoadedModule& module : m_modules)
    {
        if (!module.handle)
            continue;

        fc::Menu::Get().RemoveTabsByOwner(module.handle);
        if (module.shutdown)
            module.shutdown();
        FreeLibrary(module.handle);
    }

    m_modules.clear();
    m_loadedModuleNames.clear();
}

} // namespace fc::sdk
