#pragma once
// The screen an i4sim session shows while no blueprint is loaded: the catalog as a menu.
#include <kinetra/engine.h>
#include <kinetra/launcher.h>

#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

namespace i4sim {
/// What the blueprint screen asks its session to do after a frame of input.
enum class ScreenAction {
    none,
    /// Escape or M outside the SHOWCASE page: leave i4sim.
    leave,
    /// EXIT was clicked: end the program.
    exit,
    /// A blueprint was chosen; `ScreenEvent::blueprint` is its path.
    load,
    /// Enter was pressed while the catalog is unavailable: try refresh() again.
    rediscover,
};
/// Outcome of one frame on the blueprint screen.
struct ScreenEvent {
    ScreenAction action{ScreenAction::none};
    /// The chosen file; set only for ScreenAction::load.
    std::filesystem::path blueprint;
};

/// The blueprint catalog as a paged menu: one row per JSON file in the catalog directory
/// and, when its `showcase/` subdirectory holds scenes, a last row SHOWCASE whose second
/// page lists them. Reuses kinetra::LaunchMenu for layout, paging and picking.
///
/// A new screen has no catalog until refresh() succeeds. Without one it draws a load-error
/// screen and waits for Enter.
class BlueprintScreen {
  public:
    /// `directory` is the catalog; it is read by refresh(), not here.
    explicit BlueprintScreen(std::filesystem::path directory);

    /// Reads the catalog again. Throws std::filesystem::filesystem_error when the directory
    /// cannot be read and std::runtime_error when it holds no blueprint; the screen then
    /// has no catalog.
    void refresh();
    /// Whether a catalog is listed.
    bool available() const { return menu_ != nullptr; }
    /// Applies one frame of input; `viewport` in logical window pixels. On the SHOWCASE
    /// page, Escape and M return to the blueprint list instead of leaving.
    ScreenEvent update(const kinetra::Input &input, kinetra::Vec2 viewport);
    /// Draws a whole frame: the menu with `message` as its status line, or without a
    /// catalog the load-error screen with `message` wrapped over several lines.
    void draw(kinetra::Runtime &runtime, std::string_view message) const;

  private:
    std::vector<kinetra::LaunchChoice> choices() const;

    std::filesystem::path directory_;
    /// Top-level blueprints, then the scenes of the showcase subdirectory, each sorted by name.
    std::vector<std::filesystem::path> files_, showcase_files_;
    std::unique_ptr<kinetra::LaunchMenu> menu_;
};
} // namespace i4sim
