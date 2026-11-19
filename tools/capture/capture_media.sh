#!/usr/bin/env bash
# Refreshes docs/media: renders every screenshot and clip with kinetra_capture, turns the
# clips into looping GIFs with ffmpeg and copies the results into docs/media.
#
# Usage: tools/capture/capture_media.sh [build-directory] [scene...]
#   build-directory  a configured build with the runtime, relative to the current directory
#                    (default: build-graphics in the repository)
#   scene            only these scenes (default: all; see `kinetra_capture --help`)
# Needs bash, ffmpeg and a kinetra_capture binary in the build directory. The tool renders
# offscreen with software Mesa, so it needs no display and gives the same pixels every run.
set -euo pipefail

repository=$(cd "$(dirname "$0")/../.." && pwd)
build=${1:-$repository/build-graphics}
shift || true
tool="$build/kinetra_capture"
media="$repository/docs/media"

if [[ ! -x $tool ]]; then
    echo "capture_media: $tool not found; build it with: cmake --build --preset graphics --target kinetra_capture" >&2
    exit 1
fi
if ! command -v ffmpeg > /dev/null; then
    echo "capture_media: ffmpeg not found; install it to turn the clips into GIFs" >&2
    exit 1
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

"$tool" "$work" "$@" 2> >(grep -v '^MESA' >&2)

for clip in "$work"/*/; do
    [[ -d $clip ]] || continue
    name=$(basename "$clip")
    # The tool writes the rate next to the frames. The frames already have the GIF's size;
    # one palette for the whole clip (palettegen) is applied without dithering, because the
    # applications draw flat colors, and only the changed rectangle of each frame is stored.
    ffmpeg -loglevel error -y -framerate "$(< "$clip/framerate")" -i "$clip/frame-%04d.png" \
        -vf "split[a][b];[a]palettegen=stats_mode=full[p];[b][p]paletteuse=dither=none:diff_mode=rectangle" \
        -loop 0 "$work/$name.gif"
done

shopt -s nullglob
files=("$work"/*.png "$work"/*.gif)
cp "${files[@]}" "$media/"
for file in "${files[@]}"; do
    echo "docs/media/$(basename "$file") $(du -h "$file" | cut -f1)"
done
