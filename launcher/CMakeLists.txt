if(KINETRA_LAB_BUILD_RUNTIME)
    if(BUILD_TESTING)
        # Window-free: builds the registrations but never creates a session.
        kinetra_add_doctest(kinetra_menu_tests PREFIX kinetra_menu
            SOURCES tests/menu_tests.cpp LIBRARIES kinetra_sessions)
    endif()
    set(preview_dir "${CMAKE_CURRENT_SOURCE_DIR}/previews")
    set(preview_header "${CMAKE_CURRENT_BINARY_DIR}/preview_pixels.h")
    add_custom_command(OUTPUT "${preview_header}"
        COMMAND "${CMAKE_COMMAND}" "-Dinput_dir=${preview_dir}" "-Doutput=${preview_header}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/EmbedPreviews.cmake"
        DEPENDS EmbedPreviews.cmake previews/demo.rgba previews/dotio.rgba previews/i4sim.rgba
        VERBATIM)
    add_library(kinetra_sessions src/applications.cpp "${preview_header}")
    target_include_directories(kinetra_sessions PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_include_directories(kinetra_sessions PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(kinetra_sessions PUBLIC kinetra_demo_presentation kinetra_dotio_presentation kinetra_i4sim_presentation Kinetra::Launcher)
    add_executable(kinetra src/main.cpp)
    target_link_libraries(kinetra PRIVATE kinetra_sessions)
    kinetra_i4sim_bundle_blueprints(kinetra)
    kinetra_executable_icon(kinetra)
    kinetra_lab_add_smoke(launcher kinetra)
    kinetra_lab_add_smoke(launcher-demo kinetra --variant demo --agents 12)
    kinetra_lab_add_smoke(launcher-dotio kinetra --variant dotio --difficulty medium)
    kinetra_lab_add_smoke(launcher-i4sim kinetra --variant i4sim)
    if(BUILD_TESTING AND KINETRA_TEST_GRAPHICS)
        find_package(SDL2 REQUIRED)
        find_package(OpenGL REQUIRED)
        kinetra_add_doctest(kinetra_runtime_tests PREFIX kinetra_graphics
            SOURCES tests/runtime_tests.cpp LIBRARIES kinetra_sessions SDL2::SDL2 OpenGL::GL LABELS graphics)
        kinetra_i4sim_bundle_blueprints(kinetra_runtime_tests)
    endif()
endif()
