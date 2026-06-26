// ============================================================================
//  Settings.h - persistent user settings for the overlay.
// ----------------------------------------------------------------------------
//  Stores hotkeys and the interface scale, written as JSON next to the host
//  process exe. The header is free of nlohmann/json so GUI translation units
//  do not pull in the heavy single-header; json.hpp stays private to
//  Settings.cpp (fc_features only).
//
//  Flow:
//    - Menu::RegisterDefaultTabs() calls Settings::LoadAndApply() once, before
//      the first render. This reads the file and pushes the values into
//      fc::Input::ToggleKey()/UnloadKey() and FontGlobalScale.
//    - SettingsTab calls Set*() when the user changes something; that marks
//      the store dirty.
//    - Menu::Render() calls Settings::SaveIfDirty() once per frame on the
//      render thread, which flushes a dirty store to disk (cheap no-op
//      otherwise).
// ============================================================================

#pragma once

namespace fc {

class Settings
{
public:
    static Settings& Get();

    // Reads the JSON file (if present) into the store. Does not touch live
    // runtime values - call ApplyToRuntime() afterwards, or use LoadAndApply().
    void Load();

    // Pushes the stored values into fc::Input hotkeys and ImGui font scale.
    void ApplyToRuntime();

    // Convenience: Load() then ApplyToRuntime(). Safe to call once at startup.
    void LoadAndApply();

    // Snapshots the live runtime values back into the store (used when we want
    // to seed defaults from the current Input state before the first save).
    void CaptureFromRuntime();

    // Flushes the store to disk if it changed since the last save. Cheap when
    // clean. Intended to be called every frame on the render thread.
    void SaveIfDirty();

    // Hotkeys (virtual-key codes; 0 == unbound).
    int ToggleKey() const { return m_toggleKey; }
    int UnloadKey() const { return m_unloadKey; }
    int AutoFishKey() const { return m_autoFishKey; }
    void SetToggleKey(int vk) { Mark(vk != m_toggleKey); m_toggleKey = vk; }
    void SetUnloadKey(int vk) { Mark(vk != m_unloadKey); m_unloadKey = vk; }
    void SetAutoFishKey(int vk) { Mark(vk != m_autoFishKey); m_autoFishKey = vk; }

    // Interface scale (ImGui FontGlobalScale multiplier).
    float UiScale() const { return m_uiScale; }
    void SetUiScale(float scale);

private:
    Settings();

    void Mark(bool changed) { if (changed) m_dirty = true; }

    int m_toggleKey = 0;
    int m_unloadKey = 0;
    int m_autoFishKey = 0;
    float m_uiScale = 1.0f;
    bool m_loaded = false;
    bool m_dirty = false;
};

} // namespace fc
