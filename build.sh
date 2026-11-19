#!/usr/bin/env bash
set -euo pipefail

# Resolve paths from the script so invocation from another directory also works.
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
repo_dir="$(pwd -P)"

refresh_moved_cache() {
    local build_dir="$1" source_dir="$2"
    local cache="$build_dir/CMakeCache.txt"
    [[ -f "$cache" ]] || return 0
    if ! grep -Fxq "CMAKE_CACHEFILE_DIR:INTERNAL=$repo_dir/$build_dir" "$cache" ||
       ! grep -Fxq "CMAKE_HOME_DIRECTORY:INTERNAL=$source_dir" "$cache"; then
        echo "Verplaatste CMake-cache vernieuwen: $build_dir"
        # Only discard generated configuration; never remove the build directory.
        rm -f -- "$cache"
        rm -rf -- "$build_dir/CMakeFiles"
    fi
}

# Prints the tools the Windows cross build needs but cannot find; empty when all are present.
missing_windows_tools() {
    local tool missing=()
    # The `windows` preset uses the MinGW-w64 toolchain file, Ninja and ccache.
    for tool in x86_64-w64-mingw32-g++ ninja ccache; do
        command -v "$tool" >/dev/null 2>&1 || missing+=("$tool")
    done
    # `:-` keeps an empty array valid under `set -u` in bash before 4.4.
    echo "${missing[*]:-}"
}

refresh_moved_cache build-core "$repo_dir/external/Kinetra"
refresh_moved_cache build "$repo_dir"
refresh_moved_cache build-windows "$repo_dir"

cmake -S external/Kinetra -B build-core \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build-core -j2
ctest --test-dir build-core --output-on-failure

cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DKINETRA_SOURCE_DIR="$repo_dir/external/Kinetra"
cmake --build build -j2
ctest --test-dir build --output-on-failure

# Windows executables are only built, not tested: the tests would run through
# WSL interop and open graphics windows on the Windows desktop.
missing="$(missing_windows_tools)"
if [[ -z "$missing" ]]; then
    cmake --preset windows
    cmake --build --preset windows
    echo "✓ Windows-build geslaagd: build-windows/"
else
    echo "Windows-build overgeslagen; ontbreekt: $missing"
fi

echo "✓ Build en tests geslaagd!"
