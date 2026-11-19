#pragma once

#include <kinetra/runtime/session.h>
#include <string>

namespace lab {

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
};

} // namespace lab
