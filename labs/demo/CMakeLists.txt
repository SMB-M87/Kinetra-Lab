add_library(kinetra_demo_logic
    src/application.cpp src/debug_panel.cpp src/input/mapping.cpp
    src/world/agent.cpp src/world/inspector.cpp src/world/scene.cpp src/world/terrain.cpp src/world/tuning.cpp)
target_include_directories(kinetra_demo_logic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# kinetra_lab/debug.h is headless: the menu rows that every Lab application shares.
target_link_libraries(kinetra_demo_logic PUBLIC Kinetra::Simulation kinetra_lab)
target_compile_features(kinetra_demo_logic PUBLIC cxx_std_20)
if(BUILD_TESTING)
    # 60 simulated seconds per seed; labelled soak so quick runs can exclude them.
    kinetra_add_doctest(kinetra_demo_crowd_tests PREFIX kinetra_demo_crowd
        SOURCES tests/crowd_stability_tests.cpp LIBRARIES kinetra_demo_logic
        LABELS soak TIMEOUT 180)
    kinetra_add_doctest(kinetra_demo_crowd_characterization_tests PREFIX kinetra_demo_crowd_characterization
        SOURCES tests/crowd_characterization_tests.cpp
        LIBRARIES kinetra_demo_logic kinetra_lab_testing)
    kinetra_add_doctest(kinetra_demo_terrain_tests PREFIX kinetra_demo_terrain
        SOURCES tests/terrain_tests.cpp LIBRARIES kinetra_demo_logic)
    kinetra_add_doctest(kinetra_demo_tests PREFIX kinetra_demo
        SOURCES tests/application_tests.cpp tests/debug_panel_tests.cpp
        LIBRARIES kinetra_demo_logic kinetra_lab)
endif()
if(KINETRA_BUILD_RUNTIME)
    add_library(kinetra_demo_presentation
    src/presentation/help_overlay.cpp src/presentation/renderer.cpp src/presentation/session.cpp)
    target_link_libraries(kinetra_demo_presentation
        PUBLIC kinetra_demo_logic kinetra_lab Kinetra::Runtime)
endif()
if(BUILD_TESTING AND KINETRA_BUILD_RUNTIME AND KINETRA_TEST_GRAPHICS)
    find_package(SDL2 REQUIRED)
    kinetra_add_doctest(kinetra_demo_presentation_tests PREFIX kinetra_demo_graphics
        SOURCES tests/presentation_tests.cpp LIBRARIES kinetra_demo_presentation SDL2::SDL2
        LABELS graphics)
endif()
if(KINETRA_BUILD_RUNTIME)
    add_executable(kinetra_demo src/main.cpp)
    target_link_libraries(kinetra_demo PRIVATE kinetra_demo_presentation nlohmann_json::nlohmann_json)
    kinetra_executable_icon(kinetra_demo)
    if(BUILD_TESTING)
        add_test(NAME kinetra_demo_inspector_trace COMMAND ${CMAKE_COMMAND}
            "-DPROGRAM=$<TARGET_FILE:kinetra_demo>"
            "-DARGS=--seed;42;--ticks;3;--inspect;12345,67890"
            "-DTICKS=3" "-DEXPECTED=AGENT 12345,67890"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/../shared/tests/inspector_trace.cmake")
    endif()
endif()
