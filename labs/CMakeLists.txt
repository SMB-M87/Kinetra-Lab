find_package(nlohmann_json 3.11 REQUIRED)

if(BUILD_TESTING)
    # Header-only helpers (the Fingerprint hash) for the test suites of every application.
    add_library(kinetra_lab_testing INTERFACE)
    target_include_directories(kinetra_lab_testing INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/shared/tests/include)
    target_link_libraries(kinetra_lab_testing INTERFACE Kinetra::Simulation)
    target_compile_features(kinetra_lab_testing INTERFACE cxx_std_20)
endif()
# Lab policy shared by the applications (labs/shared): input mapping, modal pause, the
# debug-menu and settings rows every application shows, and the session base. Header-only.
# kinetra_lab/input.h and kinetra_lab/debug.h are headless; kinetra_lab/settings.h and
# kinetra_lab/session.h use Runtime types, so only presentation targets, which link
# Kinetra::Runtime, may include them.
add_library(kinetra_lab INTERFACE)
target_include_directories(kinetra_lab INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/shared/include)
target_link_libraries(kinetra_lab INTERFACE Kinetra::Simulation)
target_compile_features(kinetra_lab INTERFACE cxx_std_20)
if(BUILD_TESTING)
    kinetra_add_doctest(kinetra_lab_tests PREFIX kinetra_lab
        SOURCES shared/tests/shared_tests.cpp LIBRARIES kinetra_lab)
endif()
add_subdirectory(demo)
add_subdirectory(dotio)
add_subdirectory(i4sim)
