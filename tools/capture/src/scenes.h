#pragma once
// The captures in docs/media: every screenshot and clip, with its size and its script.
#include <array>
#include <cstddef>
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
/// The clicks that give the i4sim captures their motion settings, starting from the menu of
/// a newly loaded blueprint (SIMULATION open): the Reciprocal profile, which steers most
/// evenly, and plain A*, whose routes are straight lines instead of detours around crowds.
/// PROFILE and PLANNER each open a list below them that the next click chooses from.
inline constexpr std::array<MenuClick, 5> i4sim_motion_clicks{
    {{6, "MOTION"}, {3, "PROFILE"}, {7, "RECIPROCAL"}, {4, "PLANNER"}, {6, "A*"}}};
/// The LAYERS heading in the menu i4sim_motion_clicks leaves behind, with MOTION open.
inline constexpr MenuClick i4sim_layers_click{1, "LAYERS"};

/// The rows of Dotio's debug menu the debug scenes click, first to last in this order:
/// every row but GHOST CAMERA (row 0). PATHS (row 3) opens its four filters right below
/// it, so the rows after it are clicked in their new places, down to INVULNERABLE.
inline constexpr std::size_t dotio_first_debug_row = 1, dotio_last_debug_row = 12;

/// What a scene writes.
enum class SceneKind {
    /// One PNG file, `<scene>.png`.
    screenshot,
    /// A directory of numbered PNG frames, `<scene>/frame-NNNN.png`, that the media script
    /// turns into `<scene>.gif`.
    clip,
};

/// One capture: a name that is also its file name, its window size and its script.
struct Scene {
    /// File name without extension, as used in docs/media.
    std::string_view name;
    /// Whether the scene writes a screenshot or the frames of a clip.
    SceneKind kind;
    /// Window size in logical pixels.
    int width, height;
    /// Starts a session on the stage, drives it and writes the scene's file or frames.
    void (*script)(Stage &stage);
};

/// Every scene, in the order the gallery shows them: the launcher pages, then Demo, Dotio
/// and i4sim with their screenshots and clips.
const std::vector<Scene> &scenes();

/// The scenes named in `names`, in that order, or all scenes when `names` is empty.
/// Throws std::invalid_argument for a name that is not a scene.
std::vector<const Scene *> select_scenes(std::span<const std::string> names);
} // namespace capture
