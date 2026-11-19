#pragma once
// The captures in docs/media: every screenshot and clip, with its size and its script.
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace capture {
class Stage;

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
