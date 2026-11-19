# Windows dependencies for a MinGW-w64 cross build from Linux; see docs/windows.md.
#
# Included by the root CMakeLists.txt before Kinetra, only when cross-compiling for
# Windows. A native Windows build (MSYS2, as in CI) finds its packages normally.
#
# - SDL2: the official SDL2-devel-<version>-mingw archive, pinned by version and
#   SHA-256, downloaded once per build directory. Set SDL2_DIR to use another copy.
# - nlohmann_json: the official json.tar.xz release, pinned the same way. The host's
#   package cannot be reused: its target adds /usr/include, whose Linux C headers then
#   shadow MinGW's.
# - SDL2.dll is copied next to the executables, which all share
#   CMAKE_RUNTIME_OUTPUT_DIRECTORY.

include_guard(GLOBAL)
include(FetchContent)
if(CMAKE_VERSION VERSION_LESS 3.24)
    message(FATAL_ERROR "The Windows cross build needs CMake 3.24 or newer (FetchContent OVERRIDE_FIND_PACKAGE)")
endif()

set(KINETRA_LAB_SDL2_VERSION 2.32.10)
# Digest published with the release asset on GitHub.
set(KINETRA_LAB_SDL2_SHA256 83a5d74012311edc3c0d40ea6faecbe57ad692aa033fa5dc273cc937e3938ff2)

set(KINETRA_LAB_JSON_VERSION 3.12.0)
# GitHub publishes no digest for this asset; computed from the downloaded release.
set(KINETRA_LAB_JSON_SHA256 42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa)
# OVERRIDE_FIND_PACKAGE lets i4sim's find_package(nlohmann_json 3.11) use this copy.
FetchContent_Declare(nlohmann_json
    URL "https://github.com/nlohmann/json/releases/download/v${KINETRA_LAB_JSON_VERSION}/json.tar.xz"
    URL_HASH SHA256=${KINETRA_LAB_JSON_SHA256}
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    OVERRIDE_FIND_PACKAGE)
FetchContent_MakeAvailable(nlohmann_json)

# Logic-only builds (KINETRA_LAB_BUILD_RUNTIME=OFF) need no SDL2.
if(NOT KINETRA_LAB_BUILD_RUNTIME)
    return()
endif()
if(NOT SDL2_DIR)
    FetchContent_Declare(kinetra_lab_sdl2_mingw
        URL "https://github.com/libsdl-org/SDL/releases/download/release-${KINETRA_LAB_SDL2_VERSION}/SDL2-devel-${KINETRA_LAB_SDL2_VERSION}-mingw.tar.gz"
        URL_HASH SHA256=${KINETRA_LAB_SDL2_SHA256}
        DOWNLOAD_EXTRACT_TIMESTAMP ON)
    FetchContent_MakeAvailable(kinetra_lab_sdl2_mingw)
    # The archive's top-level config picks the x86_64 or i686 tree by pointer size.
    set(SDL2_DIR "${kinetra_lab_sdl2_mingw_SOURCE_DIR}/cmake" CACHE PATH "SDL2 MinGW development package" FORCE)
endif()
find_package(SDL2 REQUIRED)
# The archive's config exports only include/SDL2, while Kinetra includes <SDL2/SDL.h>
# like the Linux packages, which export both directories. The import is directory
# scoped, so Kinetra's own find_package(SDL2) reuses this target.
get_filename_component(sdl2_include_root "${SDL2_INCLUDE_DIR}" DIRECTORY)
set_property(TARGET SDL2::SDL2 APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${sdl2_include_root}")

get_target_property(sdl2_dll SDL2::SDL2 IMPORTED_LOCATION)
file(COPY "${sdl2_dll}" DESTINATION "${CMAKE_BINARY_DIR}")
