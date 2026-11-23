# kinetra_capture renders the screenshots and clip frames of docs/media (README.md).
# It needs the runtime and zlib for its PNG files; without zlib it is skipped.
find_package(ZLIB)
if(NOT ZLIB_FOUND)
    message(STATUS "kinetra_capture skipped: zlib was not found")
    return()
endif()

# A composition root, like the launcher: the scenes start sessions through
# launcher::applications.
add_library(kinetra_capture_core STATIC src/dotio_player.cpp src/png.cpp src/stage.cpp
    src/scenes/busy_stretch.cpp src/scenes/scenes.cpp src/scenes/showcase.cpp
    src/tour/canvas.cpp src/tour/pointer_path.cpp src/tour/tour.cpp src/tour/tour_script.cpp)
target_include_directories(kinetra_capture_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(kinetra_capture_core PUBLIC ZLIB::ZLIB kinetra_sessions)

add_executable(kinetra_capture src/main.cpp)
target_link_libraries(kinetra_capture PRIVATE kinetra_capture_core)
kinetra_i4sim_bundle_blueprints(kinetra_capture)

if(BUILD_TESTING)
    # Window-free: the PNG writer, the scene list, the clicked menu rows, the scripted Dotio
    # player and the tour's parts that need no Runtime; the only sessions are the Demo ones
    # of the tour's start check, which need no window.
    kinetra_add_doctest(kinetra_capture_tests PREFIX kinetra_capture
        SOURCES tests/capture_tests.cpp LIBRARIES kinetra_capture_core)
    # The scene-list case checks the committed files and the gallery in the source tree.
    target_compile_definitions(kinetra_capture_tests PRIVATE KINETRA_LAB_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    if(KINETRA_TEST_GRAPHICS)
        # Renders one screenshot and three clips end to end, so the scripts keep working.
        add_test(NAME kinetra_capture_smoke
            COMMAND ${CMAKE_COMMAND} -DTOOL=$<TARGET_FILE:kinetra_capture>
                -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/capture-smoke -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke.cmake)
        set_tests_properties(kinetra_capture_smoke PROPERTIES LABELS "graphics;unit")
    endif()
endif()
