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
- `Actions`: action queue, command buttons, catch result controls, diagnostics, and event log
- `Settings`: implemented hotkeys and interface scale
- `SDK`: module loader status and diagnostics

Do not add visible controls for features that do not exist yet. Placeholder toggles such as radar, HUD visibility, or reminder settings should stay out of the default menu until the backing runtime exists.

The top search box filters sections by title and keywords. Current useful searches include `actions`, `diagnostics`, `modules`, `sdk`, `hotkeys`, and `settings`.

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

## Built-in Diagnostics Panels

The host has two built-in diagnostic surfaces that should stay compact and readable:

- `Actions` / `Runtime`: current action runtime state, queue count, last command result, and newest runtime events from `fc::actions::Status::recent_events`.
- `SDK` / `Loaded Modules` and `Loader Events`: module load results and host-side SDK loader events from `fc::sdk::ModuleLoader`.

When adding new runtime or loader events, keep messages short and actionable. Prefer:

```text
queued: auto_cast
example_mod.dll: missing FCSDK_ModuleInit export
```

Avoid long per-frame spam in these UI logs. High-frequency details should remain in file logs or diagnostics snapshots.

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
