# UI Guide

Public UI helper header: `src/SDK/FCSDK_UI.h`

The visual direction is a modern mod-tool shell:

- dark graphite base
- cyan primary accent
- amber/coral secondary accents
- left navigation
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

Use cyan for primary state, amber for important values, coral for warnings, and muted for secondary text.

## Layout Rules

- Keep tab content scrollable.
- Use cards for grouped settings and metrics.
- Prefer `SeparatorText` or `fc::sdk::ui::Section` over large headings.
- Avoid giant hero text inside the overlay.
- Keep render callbacks fast and deterministic.
- Use stable ImGui IDs such as `##module_card`.

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
