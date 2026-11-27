# Headless: the simulation, its loaders and the session's controller.
add_library(kinetra_i4sim_logic src/controller.cpp
    src/blueprint/blueprint.cpp src/blueprint/scenario.cpp
    src/simulation/flow_metrics.cpp src/simulation/floor_plan.cpp src/simulation/geometry.cpp
    src/simulation/goals.cpp src/simulation/inspector.cpp src/simulation/make_way.cpp src/simulation/motion_step.cpp src/simulation/predictive_passing.cpp
    src/simulation/production.cpp src/simulation/routing.cpp src/simulation/shuttle.cpp
    src/simulation/simulation.cpp src/simulation/simulation_core.cpp src/simulation/stall_detector.cpp
    src/simulation/traffic.cpp)
target_include_directories(kinetra_i4sim_logic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# kinetra_lab: the controller uses the headless input policy in kinetra_lab/input.h.
target_link_libraries(kinetra_i4sim_logic PUBLIC Kinetra::Simulation kinetra_lab
    PRIVATE nlohmann_json::nlohmann_json)
target_compile_features(kinetra_i4sim_logic PUBLIC cxx_std_20)
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
target_link_libraries(kinetra_i4sim PRIVATE kinetra_i4sim_logic nlohmann_json::nlohmann_json)
kinetra_i4sim_bundle_blueprints(kinetra_i4sim)
kinetra_executable_icon(kinetra_i4sim)
if(KINETRA_BUILD_RUNTIME)
    add_library(kinetra_i4sim_presentation src/presentation/blueprint_screen.cpp src/presentation/hud.cpp
        src/presentation/session.cpp src/presentation/world_view.cpp)
    # Launcher: the blueprint screen reuses LaunchMenu's pagination.
    target_link_libraries(kinetra_i4sim_presentation PUBLIC kinetra_i4sim_logic kinetra_lab Kinetra::Launcher)
    target_compile_definitions(kinetra_i4sim PRIVATE I4SIM_RUNTIME)
    target_link_libraries(kinetra_i4sim PRIVATE kinetra_i4sim_presentation)
    # The blueprint screen, and a loaded factory that runs and draws its movers.
    kinetra_lab_add_smoke(i4sim kinetra_i4sim)
    kinetra_lab_add_smoke(i4sim-run kinetra_i4sim
        --blueprint ${CMAKE_CURRENT_SOURCE_DIR}/blueprints/3840x2400.json --run --ups 200)
    # A blueprint that fails to load leaves the session on the blueprint screen and still
    # exits with 0; report_error's "i4sim: " line on stderr is the sign of it.
    if(TEST kinetra_smoke:i4sim-run)
        set_tests_properties(kinetra_smoke:i4sim-run PROPERTIES FAIL_REGULAR_EXPRESSION "i4sim: ")
    endif()
endif()
if(BUILD_TESTING)
    add_test(NAME kinetra_i4sim_inspector_trace COMMAND ${CMAKE_COMMAND}
        "-DPROGRAM=$<TARGET_FILE:kinetra_i4sim>"
        "-DARGS=--blueprint;${CMAKE_CURRENT_SOURCE_DIR}/blueprints/showcase/corridor-passing.json;--ticks;3;--inspect;0"
        "-DEQUIVALENT_ARGS=--blueprint;${CMAKE_CURRENT_SOURCE_DIR}/blueprints/showcase/corridor-passing.json;--ticks;3;--inspect;APM4220_west"
        "-DNORMAL_ARGS=--blueprint;${CMAKE_CURRENT_SOURCE_DIR}/blueprints/showcase/corridor-passing.json;--ticks;3"
        "-DORACLE=$<TARGET_FILE:kinetra_i4sim_tests>" "-DORACLE_CASE=corridor snapshots for the inspector trace"
        "-DTICKS=3" "-DEXPECTED=CURRENT WAIT"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/../shared/tests/inspector_trace.cmake")
    # The i4sim test helpers (recorder, event counts, showcase files) in tests/include.
    add_library(kinetra_i4sim_testing INTERFACE)
    target_include_directories(kinetra_i4sim_testing INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/tests/include)
    target_link_libraries(kinetra_i4sim_testing INTERFACE kinetra_i4sim_logic kinetra_lab_testing)
    # The catalog, and the fixtures in tests/ that the catalog does not ship (test-environment).
    target_compile_definitions(kinetra_i4sim_testing
        INTERFACE KINETRA_I4SIM_BLUEPRINT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/blueprints"
                  KINETRA_I4SIM_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests")
    # Explicit benchmark target: comparisons are useful, but too slow for every CTest run.
    add_executable(kinetra_i4sim_motion_benchmark tests/motion_benchmark.cpp)
    target_link_libraries(kinetra_i4sim_motion_benchmark PRIVATE kinetra_i4sim_testing)
    kinetra_i4sim_bundle_blueprints(kinetra_i4sim_motion_benchmark)
    add_test(NAME kinetra_i4sim_benchmark_duplicate_blueprint_stems COMMAND ${CMAKE_COMMAND}
        "-DBENCHMARK=$<TARGET_FILE:kinetra_i4sim_motion_benchmark>"
        "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/test-environment.json"
        "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/benchmark-same-stem"
        -DFIRST_STEM=floor -DSECOND_STEM=floor
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_log_names.cmake")
    add_test(NAME kinetra_i4sim_benchmark_duplicate_seeds COMMAND kinetra_i4sim_motion_benchmark
        --seed 42 --seed 42 --ticks 1 --events "${CMAKE_CURRENT_BINARY_DIR}/benchmark-events")
    set_tests_properties(kinetra_i4sim_benchmark_duplicate_seeds PROPERTIES
        PASS_REGULAR_EXPRESSION "Duplicate seed for event logs")
    if(UNIX)
        # Exercise UTF-8 arguments on hosts where the CLI accepts them directly.
        add_test(NAME kinetra_i4sim_benchmark_unicode_log_names COMMAND ${CMAKE_COMMAND}
            "-DBENCHMARK=$<TARGET_FILE:kinetra_i4sim_motion_benchmark>"
            "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/test-environment.json"
            "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/benchmark-unicode"
            -DFIRST_STEM=Étage -DSECOND_STEM=étage
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_log_names.cmake")
    endif()
    kinetra_add_doctest(kinetra_i4sim_tests PREFIX kinetra_i4sim
        SOURCES tests/simulation_tests.cpp LIBRARIES kinetra_i4sim_testing nlohmann_json::nlohmann_json)
    kinetra_i4sim_bundle_blueprints(kinetra_i4sim_tests)
    kinetra_add_doctest(kinetra_i4sim_golden_tests PREFIX kinetra_i4sim_golden
        SOURCES tests/golden_tests.cpp LIBRARIES kinetra_i4sim_testing)
    kinetra_i4sim_bundle_blueprints(kinetra_i4sim_golden_tests)
    # Recorded passing outcomes under pressure; reads its fixtures from tests/.
    kinetra_add_doctest(kinetra_i4sim_pressure_tests PREFIX kinetra_i4sim_pressure
        SOURCES tests/pressure_tests.cpp LIBRARIES kinetra_i4sim_testing)
    # The session's controller without a window; reads its blueprints from the source tree.
    kinetra_add_doctest(kinetra_i4sim_controller_tests PREFIX kinetra_i4sim_controller
        SOURCES tests/controller_tests.cpp LIBRARIES kinetra_i4sim_testing)
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
            LIBRARIES kinetra_i4sim_presentation kinetra_i4sim_testing SDL2::SDL2 OpenGL::GL
            LABELS graphics)
        kinetra_i4sim_bundle_blueprints(kinetra_i4sim_presentation_tests)
    endif()
endif()
