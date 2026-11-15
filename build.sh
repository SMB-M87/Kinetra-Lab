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

refresh_moved_cache build-core "$repo_dir/external/Kinetra"
refresh_moved_cache build "$repo_dir"

cmake -S external/Kinetra -B build-core \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build-core -j2
ctest --test-dir build-core --output-on-failure

cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DKINETRA_SOURCE_DIR="$repo_dir/external/Kinetra"
cmake --build build -j2
ctest --test-dir build --output-on-failure

echo "✓ Build en tests geslaagd!"
