add_library(kinetra_sandbox_application src/sandbox_application.cpp)
target_include_directories(kinetra_sandbox_application PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(kinetra_sandbox_application PUBLIC Kinetra::Simulation)
target_compile_features(kinetra_sandbox_application PUBLIC cxx_std_20)
if(BUILD_TESTING)
    # 60 simulated seconds per seed; labelled soak so quick runs can exclude them.
    kinetra_add_doctest(kinetra_demo_crowd_tests PREFIX kinetra_demo_crowd
        SOURCES tests/crowd_stability_tests.cpp LIBRARIES kinetra_sandbox_application
        LABELS soak TIMEOUT 180)
    kinetra_add_doctest(kinetra_demo_crowd_characterization_tests PREFIX kinetra_demo_crowd_characterization
        SOURCES tests/crowd_characterization_tests.cpp
        LIBRARIES kinetra_sandbox_application kinetra_lab_test_support)
    kinetra_add_doctest(kinetra_demo_contact_tests PREFIX kinetra_demo_contacts
        SOURCES tests/contact_tests.cpp LIBRARIES Kinetra::Simulation)
    kinetra_add_doctest(kinetra_terrain_tests PREFIX kinetra_terrain
        SOURCES tests/terrain_tests.cpp LIBRARIES Kinetra::Simulation)
    kinetra_add_doctest(kinetra_sandbox_tests PREFIX kinetra_sandbox_application
        SOURCES tests/sandbox_application_tests.cpp LIBRARIES kinetra_sandbox_application kinetra_lab_common)
endif()
if(KINETRA_BUILD_RUNTIME)
    add_library(kinetra_demo_presentation src/sandbox_renderer.cpp src/session.cpp)
    target_link_libraries(kinetra_demo_presentation
        PUBLIC kinetra_sandbox_application kinetra_lab_common Kinetra::Runtime)
endif()
if(BUILD_TESTING AND KINETRA_BUILD_RUNTIME AND KINETRA_TEST_GRAPHICS)
    find_package(SDL2 REQUIRED)
    kinetra_add_doctest(kinetra_demo_presentation_tests PREFIX kinetra_demo_graphics
        SOURCES tests/presentation_tests.cpp LIBRARIES kinetra_demo_presentation SDL2::SDL2
        LABELS graphics)
endif()
if(KINETRA_BUILD_RUNTIME)
    add_executable(kinetra_demo src/main.cpp)
    target_link_libraries(kinetra_demo PRIVATE kinetra_demo_presentation)
    kinetra_executable_icon(kinetra_demo)
endif()
