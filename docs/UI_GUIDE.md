# UI Guide

Public UI helper header: `src/SDK/FCSDK_UI.h`

The visual direction is a modern mod-tool shell:

- dark graphite base
- amber primary accent
- muted gray panels with coral reserved for warnings
- top navigation
- card-based content
- compact controls
- no stock-looking ImGui tab bar in the main shell

## Helpers

```cpp
fc::sdk::ui::Section("Title");
```

Draws a section separator.

```cpp
if (fc::sdk::ui::BeginCard("##card", ImVec2(0, 120)))
{
    ImGui::TextUnformatted("Card content");
}
fc::sdk::ui::EndCard();
```

Draws a style-compatible child panel.

```cpp
fc::sdk::ui::Metric("Status", "Ready", fc::sdk::ui::Accent::Cyan);
```

Draws a muted label and colored value.

## Accent Colors

```cpp
enum class Accent
{
    Cyan,
    Amber,
    Coral,
    Muted,
    Text
};
```

Use amber for primary state and important values, coral for warnings, muted for secondary text, and cyan only when a module needs a secondary contrast color.

The built-in host shell now uses the `Byster` brand treatment: top navigation, compact toolbar, amber active underline, dark bordered content panel, and short ASCII labels. External SDK modules can still use `Accent::Cyan` where their own panel needs contrast.

## Active Host Sections

The default host menu registers only implemented sections:

- `Dashboard`: live runtime/module/hotkey overview
- `Actions`: action queue, command buttons, catch result controls, and diagnostics controls
- `Logs`: unified runtime/SDK event viewer with level filters, local search, highlight, auto-scroll, and log-folder opener
- `Health`: compact runtime/SDK diagnostics, queue state, module health, loader event counts, and useful paths
- `Settings`: implemented hotkeys and interface scale
- `SDK`: module loader status and diagnostics

Do not add visible controls for features that do not exist yet. Placeholder toggles such as radar, HUD visibility, or reminder settings should stay out of the default menu until the backing runtime exists.

The top search box filters sections by title, tab keywords, and built-in action button labels. Current useful searches include `auto scout`, `continue fishing`, `release fish`, `snapshot`, `logs`, `health`, `diagnostics`, `modules`, `sdk`, `hotkeys`, and `settings`.

When an action button search has matches, `Actions` switches to a compact `Search Results` view that renders only the matching command buttons and highlights the matching label text. Keep new action buttons in the search index when adding commands.

Action buttons reflect runtime readiness. Commands that need resolved game actions are disabled while the action runtime is still waiting; maintenance controls such as `Refresh`, `Stop All`, `Snapshot`, and log toggling remain available. Busy and queued states are reflected directly in button captions.

Topbar controls are right-aligned from the window edge: settings, collapse, then search. Do not place toolbar controls with fixed offsets from the left or with `SameLine` chains that can push the last button outside the menu. Toolbar buttons should perform a real action; decorative buttons are not allowed in the host shell.

## Layout Rules

- Keep tab content scrollable.
- Use cards for grouped settings and metrics.
- Prefer `SeparatorText` or `fc::sdk::ui::Section` over large headings.
- Avoid giant hero text inside the overlay.
- Keep render callbacks fast and deterministic.
- Use stable ImGui IDs such as `##module_card`.

## Copy And Localization

Built-in host UI copy currently uses short English ASCII strings. This keeps the overlay readable across compiler, console, and font setups, and avoids mojibake in injected UI.

When adding built-in labels:

- use concise English text for now
- keep button labels short enough for the current fixed-width controls
- avoid mixing Russian and English inside the same built-in panel
- add a dedicated localization layer before reintroducing translated host UI copy

## Host UI Internals (built-in shell only)

The built-in host tabs share their helpers and palette from two internal headers (not part of the SDK ABI). External modules keep using `fc::sdk::ui` from `FCSDK_UI.h`; the host shell uses these:

- `src/GUI/Theme.h` - `fc::Palette` is the single source of host colors (RGBA hex `0xRRGGBBAA`), plus `fc::Color(hex)` / `fc::ColorU32(hex)` converters. Do not inline a hex literal in a host tab - reference a `Palette::*` constant.
- `src/GUI/UI.h` - `fc::gui::ui` with `BeginCard/EndCard`, `BeginStatCard`, `Metric`, `StatusLine`, `WrappedStatusLine`, `Narrow`, `ProcessDirectory`/`ProcessDirectoryW`, and the case-insensitive `ContainsNoCase` / `FindNoCase` / `HighlightText` / `HighlightLabel` text helpers.
- `src/GUI/ActionsTable.h` - the data-driven action catalog (`kActions[]`, grouped by `ActionGroup`) that drives both the Actions button grid and the top search results. Add or remove a command here only.

The host palette (amber/graphite) and the SDK palette (cyan/blue, `FCSDK_UI.h`) are intentionally separate. External SDK modules render in their own palette so a module never has to depend on host internals. Do not merge the two without a deliberate design decision.

## Persistent Settings

User settings (hotkeys and interface scale) are persisted by `fc::Settings` (`src/Features/Settings.h`) as `FishingCompanion_settings.json` next to the host process exe:

- `Menu::RegisterDefaultTabs()` loads and applies settings once before the first render.
- `SettingsTab` writes through `Settings::Set*()`; `Menu::Render()` flushes dirty changes once per frame via `Settings::SaveIfDirty()`.

Hotkeys remain a single virtual-key code (no modifier combos), because `fc::Input` in `fc_core` parses one VK. Adding combos is a core-layer change, not a GUI one.

## Keyboard Navigation

The top section tabs also respond to arrow left/right, in addition to the mouse. The custom `TopTab` control uses `ImGui::InvisibleButton` and does not capture ImGui nav, so `Menu::Render()` drives the cycle manually. Pressing an arrow clears the search box.

## Minimal Styled Tab

```cpp
static void Render(void*)
{
    namespace ui = fc::sdk::ui;

    ui::Section("Module");

    if (ui::BeginCard("##module_state", ImVec2(0, 100)))
    {
        ui::Metric("Status", "Ready", ui::Accent::Cyan);
        ImGui::TextWrapped("Module-specific controls go here.");
    }
    ui::EndCard();
}
```
