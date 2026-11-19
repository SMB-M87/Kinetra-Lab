find_package(nlohmann_json 3.11 REQUIRED)
add_library(kinetra_i4sim_application src/blueprint.cpp src/flow_metrics.cpp src/floor_plan.cpp src/geometry.cpp src/goals.cpp src/motion_step.cpp src/production.cpp src/routing.cpp src/shuttle.cpp src/simulation.cpp src/simulation_core.cpp src/traffic.cpp
    src/predictive_passing.cpp src/scenario.cpp)
target_include_directories(kinetra_i4sim_application PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(kinetra_i4sim_application PUBLIC Kinetra::Simulation PRIVATE nlohmann_json::nlohmann_json)
target_compile_features(kinetra_i4sim_application PUBLIC cxx_std_20)
# Keep resources beside every executable that can open the factory. Resolve their
# location at runtime so moving a complete bundle does not retain a build-machine path.
# All executables share CMAKE_RUNTIME_OUTPUT_DIRECTORY, so one target copies the catalog
# once per build. A copy per executable raced: concurrent `cmake -E copy_directory` runs
# into one directory fail with "No such file or directory" when executables link in parallel.
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    message(FATAL_ERROR "The i4sim blueprint copy needs CMAKE_RUNTIME_OUTPUT_DIRECTORY, "
        "which the root CMakeLists.txt sets for all executables")
endif()
get_property(kinetra_i4sim_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
set(kinetra_i4sim_blueprint_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
if(kinetra_i4sim_multi_config)
    # Multi-config generators append the configuration to the output directory.
    string(APPEND kinetra_i4sim_blueprint_directory "/$<CONFIG>")
endif()
# Always out of date, so every build refreshes the catalog: edited, added or renamed
# blueprints arrive without a file list or a relink (the former copy ran only after a
# relink). Dependents wait for it but do not relink because of it. The old copy is removed
# first, so a deleted or renamed blueprint does not linger in an existing build directory.
add_custom_target(kinetra_i4sim_blueprints ALL
    # remove_directory rather than rm: `cmake -E rm` needs CMake 3.17, the minimum is 3.16.
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${kinetra_i4sim_blueprint_directory}/i4sim-blueprints"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/blueprints"
        "${kinetra_i4sim_blueprint_directory}/i4sim-blueprints"
    COMMENT "Copying i4sim blueprints"
    VERBATIM)
# Makes `target` build after the blueprint copy. Fails at configure time when the target
# writes its executable outside the shared output directory, where it would miss the catalog.
function(kinetra_i4sim_bundle_blueprints target)
    get_target_property(output_directory ${target} RUNTIME_OUTPUT_DIRECTORY)
    if(NOT output_directory STREQUAL CMAKE_RUNTIME_OUTPUT_DIRECTORY)
        message(FATAL_ERROR "${target} writes to '${output_directory}', but the i4sim blueprints "
            "are copied to CMAKE_RUNTIME_OUTPUT_DIRECTORY '${CMAKE_RUNTIME_OUTPUT_DIRECTORY}'")
    endif()
    add_dependencies(${target} kinetra_i4sim_blueprints)
endfunction()
add_executable(kinetra_i4sim src/main.cpp)
target_link_libraries(kinetra_i4sim PRIVATE kinetra_i4sim_application)
kinetra_i4sim_bundle_blueprints(kinetra_i4sim)
kinetra_executable_icon(kinetra_i4sim)
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
        SOURCES tests/simulation_tests.cpp LIBRARIES kinetra_i4sim_application kinetra_lab_test_support)
    kinetra_i4sim_bundle_blueprints(kinetra_i4sim_tests)
    kinetra_add_doctest(kinetra_i4sim_golden_tests PREFIX kinetra_i4sim_golden
        SOURCES tests/golden_tests.cpp LIBRARIES kinetra_i4sim_application kinetra_lab_test_support)
    kinetra_i4sim_bundle_blueprints(kinetra_i4sim_golden_tests)
    # Recorded passing outcomes under pressure; reads its fixtures from tests/.
    kinetra_add_doctest(kinetra_i4sim_pressure_tests PREFIX kinetra_i4sim_pressure
        SOURCES tests/pressure_tests.cpp LIBRARIES kinetra_i4sim_application kinetra_lab_test_support)
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
