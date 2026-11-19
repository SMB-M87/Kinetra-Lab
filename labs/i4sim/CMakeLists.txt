find_package(nlohmann_json 3.11 REQUIRED)
add_library(kinetra_i4sim_application src/blueprint.cpp src/flow_metrics.cpp src/simulation.cpp
    src/predictive_passing.cpp)
target_include_directories(kinetra_i4sim_application PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(kinetra_i4sim_application PUBLIC Kinetra::Simulation PRIVATE nlohmann_json::nlohmann_json)
target_compile_features(kinetra_i4sim_application PUBLIC cxx_std_20)
# Keep resources beside every executable that can open the factory. Resolve their
# location at runtime so moving a complete bundle does not retain a build-machine path.
function(kinetra_i4sim_bundle_blueprints target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "$<TARGET_PROPERTY:kinetra_i4sim_application,SOURCE_DIR>/blueprints"
            "$<TARGET_FILE_DIR:${target}>/i4sim-blueprints"
        VERBATIM)
endfunction()
file(COPY blueprints/ DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/i4sim-blueprints")
add_executable(kinetra_i4sim src/main.cpp)
target_link_libraries(kinetra_i4sim PRIVATE kinetra_i4sim_application)
kinetra_i4sim_bundle_blueprints(kinetra_i4sim)
if(KINETRA_BUILD_RUNTIME)
    add_library(kinetra_i4sim_presentation src/session.cpp)
    # Launcher: the blueprint screen reuses LaunchMenu's pagination.
    target_link_libraries(kinetra_i4sim_presentation PUBLIC kinetra_i4sim_application kinetra_lab_common Kinetra::Launcher)
    target_compile_definitions(kinetra_i4sim PRIVATE I4SIM_RUNTIME)
    target_link_libraries(kinetra_i4sim PRIVATE kinetra_i4sim_presentation)
endif()
if(BUILD_TESTING)
    # Explicit benchmark target: comparisons are useful, but too slow for every CTest run.
    add_executable(kinetra_i4sim_motion_benchmark tests/motion_benchmark.cpp)
    target_link_libraries(kinetra_i4sim_motion_benchmark PRIVATE kinetra_i4sim_application)
    kinetra_i4sim_bundle_blueprints(kinetra_i4sim_motion_benchmark)
    kinetra_add_doctest(kinetra_i4sim_tests PREFIX kinetra_i4sim
        SOURCES tests/simulation_tests.cpp LIBRARIES kinetra_i4sim_application)
    kinetra_i4sim_bundle_blueprints(kinetra_i4sim_tests)
    kinetra_add_doctest(kinetra_i4sim_golden_tests PREFIX kinetra_i4sim_golden
        SOURCES tests/golden_tests.cpp LIBRARIES kinetra_i4sim_application kinetra_lab_test_support)
    kinetra_i4sim_bundle_blueprints(kinetra_i4sim_golden_tests)
    # Runs the catalog case of kinetra_i4sim_tests from a relocated copy of the executable.
    add_test(NAME kinetra_i4sim_relocation COMMAND ${CMAKE_COMMAND}
        "-DPROBE=$<TARGET_FILE:kinetra_i4sim_tests>"
        "-DRESOURCES=${CMAKE_CURRENT_SOURCE_DIR}/blueprints"
        "-DTEST_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/relocation-test"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/relocation_tests.cmake")
    if(KINETRA_BUILD_RUNTIME AND KINETRA_TEST_GRAPHICS)
        find_package(SDL2 REQUIRED)
        find_package(OpenGL REQUIRED)
        kinetra_add_doctest(kinetra_i4sim_presentation_tests PREFIX kinetra_i4sim_graphics
            SOURCES tests/presentation_tests.cpp
            LIBRARIES kinetra_i4sim_presentation SDL2::SDL2 OpenGL::GL
            LABELS graphics)
        kinetra_i4sim_bundle_blueprints(kinetra_i4sim_presentation_tests)
    endif()
endif()
