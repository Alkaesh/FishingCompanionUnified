// ============================================================================
//  CommandPalette.h - global command search dropdown for the menu topbar.
// ----------------------------------------------------------------------------
//  The topbar search field is a command filter: as the user types, this draws
//  an autocomplete dropdown listing every matching command (from kActions[])
//  and quick navigation entry (from kQuickNav[]). Selecting an entry either
//  queues the command or switches to the target tab.
//
//  Matching is multi-word AND: the query is split on whitespace and EVERY token
//  must appear (case-insensitive) in the entry's label or keywords. This avoids
//  the old single-substring behavior that matched loosely.
// ============================================================================

#pragma once

#include "ActionsTable.h"
#include "../Actions/ActionRuntime.h"

namespace fc::gui {

// Result of an interaction with the palette dropdown.
enum class PaletteAction
{
    None,        // nothing selected this frame
    RunCommand,  // command field is valid; queue it
    Navigate,    // navTabTitle is set; switch to that tab
};

struct PaletteResult
{
    PaletteAction action = PaletteAction::None;
    actions::Command command = actions::Command::Refresh; // valid when RunCommand
    const char* navTabTitle = nullptr;                    // valid when Navigate
};

// Draws the dropdown anchored at the given screen-space top-left, with the
// given width. Pass the current query text and the selected index (kept by the
// caller so Up/Down/Enter state survives across frames). Returns the action to
// perform (if the user activated an entry), or PaletteAction::None otherwise.
// When there are no matches, draws a short "no match" line.
PaletteResult RenderCommandPalette(
    const char* query,
    float originX,
    float originY,
    float width,
    int& selectedIndex);

} // namespace fc::gui
