#pragma once
// The captures in docs/media: every screenshot and clip, with its size and its script.
#include <kinetra/view/debug_menu.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace capture {
class Stage;

/// One click on a row of an application's debug menu: the row, counted from the first row
/// below the heading in the menu as the click finds it, and the label that row must have
/// then. The scripts click by row; the label lets a test notice a changed menu.
struct MenuClick {
    std::size_t row;
    std::string_view label;
};

/// Blueprint of the i4sim screenshots and clips, relative to the catalog: the largest
/// original floor that stays readable at clip size, with 29 movers and 18 producers.
inline constexpr std::string_view i4sim_capture_blueprint = "4320x2400.json";
/// Blueprint of the launcher's i4sim preview: the small original floor, with four producers
/// and few enough movers that the whole floor stays calm and sharp at the still's size.
inline constexpr std::string_view i4sim_preview_blueprint = "1680x960+.json";
/// Seconds of the preview floor, from Space, that are searched for the moment most of its
/// movers drive.
inline constexpr int i4sim_preview_search_seconds = 60;
/// The clicks that give the i4sim captures their motion settings, starting from the menu of
/// a newly loaded blueprint (SIMULATION open): the Reciprocal profile, which steers most
/// evenly, and plain A*, whose routes are straight lines instead of detours around crowds.
/// PROFILE and PLANNER each open a list below them that the next click chooses from.
inline constexpr std::array<MenuClick, 5> i4sim_motion_clicks{
    {{7, "MOTION"}, {3, "PROFILE"}, {7, "RECIPROCAL"}, {4, "PLANNER"}, {6, "A*"}}};
/// The LAYERS heading in the menu i4sim_motion_clicks leaves behind, with MOTION open.
inline constexpr MenuClick i4sim_layers_click{1, "LAYERS"};

/// Runs `click` on `menu`, the rows of `application`'s debug menu as the click finds it: the
/// action of row `click.row`, which is what a session does for a click on that row's label.
/// Throws std::runtime_error that names the application and the row when the row does not
/// exist, has another label or has no action: the menu changed, and the clicks here must
/// follow.
void apply_menu_click(std::span<const kinetra::DebugEntry> menu, const MenuClick &click,
                      std::string_view application);

/// The clicks that give the Dotio debug scenes their view, starting from the menu of a new
/// session (SIMULATION open): INVULNERABLE, then the LAYERS section and every layer in it.
/// PATHS opens its four parts right below it, so the rows after it are clicked in their new
/// places. GHOST CAMERA stays off.
inline constexpr std::array<MenuClick, 13> dotio_debug_clicks{{{5, "INVULNERABLE"},
                                                               {6, "LAYERS"},
                                                               {2, "VECTORS"},
                                                               {3, "STEERING PROBES"},
                                                               {4, "PATHS"},
                                                               {5, "WANDER"},
                                                               {6, "PURSUE"},
                                                               {7, "KAMIKAZE"},
                                                               {8, "RECOVERY"},
                                                               {9, "COLLISION SHAPES"},
                                                               {10, "ACTIVE AGENTS"},
                                                               {11, "AI STATES"},
                                                               {12, "HIDING HITS"}}};
/// The MOTION heading in the debug menu of a new Demo session (SIMULATION open); the Demo
/// debug screenshot shows that section's tuning rows.
inline constexpr MenuClick demo_motion_click{7, "MOTION"};

/// What a scene writes.
enum class SceneKind {
    /// One PNG file, `<scene>.png`.
    screenshot,
    /// A directory of numbered PNG frames, `<scene>/frame-NNNN.png`, that the media script
    /// turns into `<scene>.gif`.
    clip,
};

/// The README whose media a scene belongs to.
enum class Gallery {
    /// Kinetra-Lab's docs/media: the applications and the launcher's pages.
    lab,
    /// Kinetra's docs/media (external/Kinetra/docs/media): the shared UI, captured from Demo.
    kinetra,
};

/// The subdirectory of the tool's output that holds `gallery`'s files: the output itself for
/// Lab's, `kinetra` for Kinetra's, which capture_media.sh copies to external/Kinetra/docs/media.
std::filesystem::path output_subdirectory(Gallery gallery);

/// One capture: a name that is also its file name, its window size and its script.
struct Scene {
    /// File name without extension, as used in its gallery's docs/media.
    std::string_view name;
    /// Whether the scene writes a screenshot or the frames of a clip.
    SceneKind kind;
    /// Window size in logical pixels.
    int width, height;
    /// Starts a session on the stage, drives it and writes the scene's file or frames.
    void (*script)(Stage &stage);
    /// The README whose media the scene belongs to.
    Gallery gallery{Gallery::lab};
};

/// Every scene, in the order the galleries show them: the launcher pages, then Demo, Dotio
/// and i4sim with their screenshots and clips, then the shared UI for Kinetra's README.
const std::vector<Scene> &scenes();

/// The scenes named in `names`, in that order, or all scenes when `names` is empty.
/// Throws std::invalid_argument for a name that is not a scene.
std::vector<const Scene *> select_scenes(std::span<const std::string> names);
} // namespace capture
