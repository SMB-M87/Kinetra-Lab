#pragma once

#include <kinetra/view/input.h>

/// Input policy shared by the Lab applications. Headless: it uses only Kinetra's input
/// types, so the rules are testable without a window.
namespace kinetra_lab {

/// Movement direction from WASD or the arrow keys: x is right minus left, y is up minus
/// down (world axes, Y up). Opposite keys cancel; diagonals are not normalized.
inline kinetra::Vec2 direction(const kinetra::Input &input) {
    using kinetra::Key;
    auto axis = [&](Key positive, Key positive_arrow, Key negative, Key negative_arrow) {
        return static_cast<float>(input.down(positive) || input.down(positive_arrow)) -
               static_cast<float>(input.down(negative) || input.down(negative_arrow));
    };
    return {axis(Key::d, Key::right, Key::a, Key::left), axis(Key::w, Key::up, Key::s, Key::down)};
}

/// Pauses an application while a modal panel is open, and restores the pause state it had
/// when the panel opened.
///
/// Call update() once per frame, after the panel's own update, with the panel's visibility
/// and the current pause state; apply the returned state. While the panel is open the
/// application cannot unpause, so no simulated time passes behind the panel.
class ModalPause {
  public:
    /// Returns the pause state to apply this frame.
    bool update(bool open, bool paused) {
        if (open && !was_open_) {
            paused_before_ = paused;
            paused = true;
        } else if (!open && was_open_)
            paused = paused_before_;
        was_open_ = open;
        return paused;
    }

  private:
    bool was_open_{}, paused_before_{};
};

} // namespace kinetra_lab
