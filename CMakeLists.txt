cmake_minimum_required(VERSION 3.16)
project(KinetraLab LANGUAGES C CXX)
include(CTest)
option(KINETRA_LAB_BUILD_RUNTIME "Build graphical applications and launcher" ON)
option(KINETRA_TEST_GRAPHICS "Enable tests requiring an OpenGL context" OFF)
option(KINETRA_LAB_WARNINGS "Compile Lab targets with the extended warning set" OFF)
option(KINETRA_LAB_WARNINGS_AS_ERRORS "Treat Lab warnings as errors (requires KINETRA_LAB_WARNINGS)" OFF)
option(KINETRA_LAB_WARNING_FREE_AS_ERRORS
       "Treat warnings in the warning-free Lab modules as errors (requires KINETRA_LAB_WARNINGS)" ON)
set(KINETRA_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/Kinetra" CACHE PATH "Kinetra source checkout")
if(NOT EXISTS "${KINETRA_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Kinetra source missing: initialize external/Kinetra or set KINETRA_SOURCE_DIR to a local checkout")
endif()
if(CMAKE_CROSSCOMPILING AND WIN32)
    include(cmake/WindowsDependencies.cmake)
endif()
set(KINETRA_BUILD_RUNTIME ${KINETRA_LAB_BUILD_RUNTIME} CACHE BOOL "" FORCE)
# Core tests run independently of Lab's CTest suites.
set(lab_testing ${BUILD_TESTING})
set(BUILD_TESTING OFF)
add_subdirectory("${KINETRA_SOURCE_DIR}" kinetra-core)
set(BUILD_TESTING ${lab_testing})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
# Registers a bounded run of a graphical executable, `<target> <args> --frames 120`, as the
# CTest entry `kinetra_smoke:<name>` with the label `graphics`. It passes when the program
# starts, draws 120 frames and exits with 0, so the graphics preset and CI start every
# executable once. A no-op unless graphics tests are on.
function(kinetra_lab_add_smoke name target)
    if(BUILD_TESTING AND KINETRA_LAB_BUILD_RUNTIME AND KINETRA_TEST_GRAPHICS)
        add_test(NAME kinetra_smoke:${name} COMMAND $<TARGET_FILE:${target}> ${ARGN} --frames 120)
        set_tests_properties(kinetra_smoke:${name} PROPERTIES LABELS "graphics;unit" TIMEOUT 120)
    endif()
endfunction()
# Application CMake uses this local switch for its existing presentation targets.
set(KINETRA_BUILD_RUNTIME ${KINETRA_LAB_BUILD_RUNTIME})
add_subdirectory(labs)
if(KINETRA_LAB_BUILD_RUNTIME AND NOT TARGET Kinetra::Launcher)
    message(FATAL_ERROR "Kinetra checkout must supply Kinetra::Launcher; update external/Kinetra or set KINETRA_SOURCE_DIR to an updated checkout")
endif()
add_subdirectory(launcher)
if(KINETRA_LAB_BUILD_RUNTIME)
    add_subdirectory(tools/capture)
endif()

# Lab warnings use Kinetra's warning set (cmake/KinetraWarnings.cmake). They are
# reported, not errors, until each module's roadmap phase makes it warning-free.
if(KINETRA_LAB_WARNINGS)
    set(lab_warning_mode)
    if(KINETRA_LAB_WARNINGS_AS_ERRORS)
        set(lab_warning_mode AS_ERRORS)
    endif()
    kinetra_directory_warnings(${CMAKE_CURRENT_SOURCE_DIR}/labs RECURSE ${lab_warning_mode})
    kinetra_directory_warnings(${CMAKE_CURRENT_SOURCE_DIR}/launcher RECURSE ${lab_warning_mode})
    # Modules that a roadmap phase made warning-free keep it that way. CI's Ubuntu run turns
    # this off: the pinned Arch compiler is the reference for warnings, and another GCC warns
    # about other things.
    set(warning_free_mode ${lab_warning_mode})
    if(KINETRA_LAB_WARNING_FREE_AS_ERRORS)
        set(warning_free_mode AS_ERRORS)
    endif()
    kinetra_directory_warnings(${CMAKE_CURRENT_SOURCE_DIR}/labs/demo RECURSE ${warning_free_mode})
    kinetra_directory_warnings(${CMAKE_CURRENT_SOURCE_DIR}/labs/i4sim RECURSE ${warning_free_mode})
    kinetra_directory_warnings(${CMAKE_CURRENT_SOURCE_DIR}/labs/dotio RECURSE ${warning_free_mode})
    if(KINETRA_LAB_BUILD_RUNTIME)
        kinetra_directory_warnings(${CMAKE_CURRENT_SOURCE_DIR}/tools/capture ${warning_free_mode})
    endif()
endif()
