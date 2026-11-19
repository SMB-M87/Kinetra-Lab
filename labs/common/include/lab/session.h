#pragma once

#include <kinetra/session.h>

namespace lab {

/// Base of every Lab session. Lab applications leave through their settings panel
/// (LAUNCHER or EXIT) or the window's close button, never through Escape, which their
/// screens use themselves.
class Session : public kinetra::Session {
  public:
    bool close_on_escape() const override { return false; }
};

} // namespace lab
