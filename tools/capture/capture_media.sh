#!/usr/bin/env bash
# Refreshes docs/media: renders every screenshot and clip with kinetra_capture, turns the
# clips into looping GIFs with ffmpeg and copies the results into docs/media, and the
# shared-UI clips of Kinetra's README into external/Kinetra/docs/media.
# Refreshes the launcher's embedded stills from the lab scenes they come from (demo-preview,
# dotio, i4sim-preview) and rebuilds the capture tool before recording the launcher.
#
# Usage: tools/capture/capture_media.sh [build-directory] [scene...]
#   build-directory  a configured build with the runtime, relative to the current directory
#                    (default: build-graphics in the repository)
#   scene            only these scenes (default: all; see `kinetra_capture --help`)
# Needs bash, CMake, ffmpeg and a kinetra_capture binary in the build directory. The tool renders
# offscreen with software Mesa, so it needs no display and gives the same pixels every run.
set -euo pipefail

repository=$(cd "$(dirname "$0")/../.." && pwd)
build=${1:-$repository/build-graphics}
shift || true
tool="$build/kinetra_capture"
media="$repository/docs/media"
kinetra_media="$repository/external/Kinetra/docs/media"

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

capture_scenes() {
    "$tool" "$work" "$@" 2> >(grep -v '^MESA' >&2)
}

launcher_scenes=()
if (($# == 0)); then
    capture_scenes
    launcher_scenes=(launcher ui-launcher)
else
    other_scenes=()
    for scene in "$@"; do
        case "$scene" in
            launcher|ui-launcher) launcher_scenes+=("$scene") ;;
            *) other_scenes+=("$scene") ;;
        esac
    done
    if ((${#other_scenes[@]})); then
        capture_scenes "${other_scenes[@]}"
    fi
fi

# Each lab's launcher still and the scene it is scaled down from: Demo and i4sim have a
# preview scene of their own, Dotio's still is its main screenshot.
for still_scene in demo:demo-preview dotio:dotio i4sim:i4sim-preview; do
    lab=${still_scene%%:*}
    scene=${still_scene#*:}
    [[ -f $work/$scene.png ]] || continue
    ffmpeg -loglevel error -y -i "$work/$scene.png" -vf scale=320:180:flags=area \
        -frames:v 1 -c:v rawvideo -threads:v 1 -pix_fmt rgba -f rawvideo "$work/$lab.rgba"
    still="$repository/launcher/previews/$lab.rgba"
    if ! cmp -s "$work/$lab.rgba" "$still"; then
        cp "$work/$lab.rgba" "$still"
    fi
done

if ((${#launcher_scenes[@]})); then
    # Even a launcher-only refresh can follow an earlier change to an embedded still.
    cmake --build "$build" --target kinetra_capture
    # The default capture may already have frames with the old images. Render the clip
    # into an empty directory so it cannot inherit frames from that first pass.
    rm -rf -- "$work/kinetra/ui-launcher"
    capture_scenes "${launcher_scenes[@]}"
fi

shopt -s nullglob

# Turns every clip directory in $1 into $1/<clip>.gif. A clip directory holds the frames and
# the rate the tool wrote; the tool's kinetra/ subdirectory is a gallery, not a clip.
encode_clips() {
    local clip name
    for clip in "$1"/*/; do
        [[ -f $clip/framerate ]] || continue
        name=$(basename "$clip")
        # The frames already have the GIF's size; one palette for the whole clip (palettegen)
        # is applied without dithering, because the applications draw flat colors, and only
        # the changed rectangle of each frame is stored.
        ffmpeg -loglevel error -y -framerate "$(< "$clip/framerate")" -i "$clip/frame-%04d.png" \
            -vf "split[a][b];[a]palettegen=stats_mode=full[p];[b][p]paletteuse=dither=none:diff_mode=rectangle" \
            -loop 0 "$1/$name.gif"
    done
}

# Copies the PNGs and GIFs of $1 into the directory $2 and lists them with their sizes.
copy_media() {
    local files=("$1"/*.png "$1"/*.gif) file
    ((${#files[@]})) || return 0
    cp "${files[@]}" "$2/"
    for file in "${files[@]}"; do
        echo "${2#"$repository"/}/$(basename "$file") $(du -h "$file" | cut -f1)"
    done
}

encode_clips "$work"
encode_clips "$work/kinetra"
copy_media "$work" "$media"
if [[ -d $work/kinetra ]]; then
    # An empty submodule directory would take the GIFs outside any Kinetra checkout.
    if [[ ! -d $kinetra_media ]]; then
        echo "capture_media: $kinetra_media not found; run: git submodule update --init --recursive" >&2
        exit 1
    fi
    copy_media "$work/kinetra" "$kinetra_media"
fi
