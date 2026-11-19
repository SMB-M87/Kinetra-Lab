if(BUILD_TESTING)
    # Header-only helpers (the Fingerprint hash) for the test suites of every application.
    add_library(kinetra_lab_test_support INTERFACE)
    target_include_directories(kinetra_lab_test_support INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/test_support)
    target_link_libraries(kinetra_lab_test_support INTERFACE Kinetra::Simulation)
    target_compile_features(kinetra_lab_test_support INTERFACE cxx_std_20)
endif()
# Lab policy shared by the applications: input mapping, modal pause, the debug-menu and
# settings rows every application shows, and the session base. Header-only. lab/input.h and
# lab/debug.h are headless; lab/settings.h and lab/session.h use Runtime types, so only
# presentation targets, which link Kinetra::Runtime, may include them.
add_library(kinetra_lab_common INTERFACE)
target_include_directories(kinetra_lab_common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/common/include)
target_link_libraries(kinetra_lab_common INTERFACE Kinetra::Simulation)
target_compile_features(kinetra_lab_common INTERFACE cxx_std_20)
if(BUILD_TESTING)
    kinetra_add_doctest(kinetra_lab_common_tests PREFIX kinetra_lab_common
        SOURCES common/tests/common_tests.cpp LIBRARIES kinetra_lab_common)
endif()
add_subdirectory(demo)
add_subdirectory(dotio)
add_subdirectory(i4sim)
