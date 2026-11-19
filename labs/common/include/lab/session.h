#pragma once

#include <kinetra/debug_menu.h>
#include <kinetra/session.h>

#include <span>
#include <string>
#include <vector>

namespace lab {
/// A row of a session's debug menu as a tool sees it: its text and its nesting level.
struct DebugRow {
    std::string label;
    int depth{};
};

/// The label and depth of every entry of `entries`, in order: what a session returns from
/// Session::debug_rows().
inline std::vector<DebugRow> debug_rows_of(std::span<const kinetra::DebugEntry> entries) {
    std::vector<DebugRow> rows;
    rows.reserve(entries.size());
    for (const auto &entry : entries)
        rows.push_back({entry.label, entry.depth});
    return rows;
}

/// Base of every Lab session. Lab applications leave through their settings panel
/// (LAUNCHER or EXIT) or the window's close button, never through Escape, which their
/// screens use themselves.
class Session : public kinetra::Session {
  public:
    bool close_on_escape() const override { return false; }
    /// One-line summary of the session's state (pause, counters, last action) for tests
    /// and diagnostics. The window title shows only the application's name, so this text
    /// is never on screen.
    virtual std::string status() = 0;
    /// The rows of the session's debug menu as they stand, top to bottom, whether the menu
    /// shows or not; empty when the session has none at the moment. For tools and tests
    /// that click a row by its label instead of its number. The rows change with every
    /// click that opens a section or a list, so ask again after one.
    virtual std::vector<DebugRow> debug_rows() { return {}; }
};

} // namespace lab
