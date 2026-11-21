#!/usr/bin/env bash
# Packages a Windows cross build (the "windows" CMake preset) into a zip that runs
# without installing anything: the launcher and the three applications, SDL2.dll,
# the i4sim blueprints and the licenses. See tools/release/README.md.
#
# Usage: tools/release/package_windows.sh <build-dir> <output.zip>
set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "usage: $0 <build-dir> <output.zip>" >&2
    exit 2
fi

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build="$(cd "$1" && pwd)"
output="$(mkdir -p "$(dirname "$2")" && cd "$(dirname "$2")" && pwd)/$(basename "$2")"
name="$(basename "$output" .zip)"
executables=(kinetra.exe kinetra_demo.exe kinetra_dotio.exe kinetra_i4sim.exe)

for file in "${executables[@]}" SDL2.dll i4sim-blueprints; do
    if [[ ! -e "$build/$file" ]]; then
        echo "missing $build/$file; build the applications with the windows preset first" >&2
        exit 1
    fi
done
sdl_license="$(find "$build/_deps" -path '*sdl2*' -maxdepth 2 -name LICENSE.txt | head -n 1)"
json_license="$(find "$build/_deps" -path '*nlohmann_json*' -maxdepth 2 -name 'LICENSE*' | head -n 1)"
if [[ -z "$sdl_license" || -z "$json_license" ]]; then
    echo "missing the SDL2 or nlohmann_json license in $build/_deps" >&2
    exit 1
fi

staging="$(mktemp -d)"
trap 'rm -rf "$staging"' EXIT
package="$staging/$name"
mkdir -p "$package/licenses"

for file in "${executables[@]}" SDL2.dll; do
    cp "$build/$file" "$package/"
done
cp -r "$build/i4sim-blueprints" "$package/"
cp "$root/LICENSE" "$package/licenses/Kinetra-Lab.txt"
cp "$root/external/Kinetra/LICENSE" "$package/licenses/Kinetra.txt"
cp "$sdl_license" "$package/licenses/SDL2.txt"
cp "$json_license" "$package/licenses/nlohmann_json.txt"

commit="$(git -C "$root" rev-parse --short HEAD 2>/dev/null || echo unknown)"
sed "s/@COMMIT@/$commit/" "$root/tools/release/README-windows.txt" | sed 's/$/\r/' > "$package/README.txt"

rm -f "$output"
(cd "$staging" && cmake -E tar cf "$output" --format=zip "$name")
echo "$output"
