add_library(kinetra_dotio_logic
    src/ai/fuzzy.cpp
    src/ai/state.cpp
    src/ai/wander.cpp
    src/difficulty.cpp
    src/entities/arrow.cpp
    src/entities/bolt.cpp
    src/entities/enemy.cpp
    src/entities/moving_object.cpp
    src/entities/physics_object.cpp
    src/entities/player.cpp
    src/entities/static_object.cpp
    src/inspector.cpp
    src/math/cell.cpp
    src/math/vector.cpp
    src/navigation/grid.cpp
    src/navigation/route.cpp
    src/world.cpp
)
target_compile_features(kinetra_dotio_logic PUBLIC cxx_std_20)
target_include_directories(kinetra_dotio_logic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# kinetra_lab/debug.h is headless: the menu rows that every Lab application shares.
target_link_libraries(kinetra_dotio_logic PUBLIC Kinetra::Simulation kinetra_lab)
if(BUILD_TESTING)
    kinetra_add_doctest(kinetra_dotio_tests PREFIX kinetra_dotio
        SOURCES tests/dotio_tests.cpp LIBRARIES kinetra_dotio_logic)
    # Seeded statistics of enemy behavior; minutes per full run, so CTest runs only a smoke.
    add_executable(kinetra_dotio_behavior_benchmark tests/behavior_benchmark.cpp)
    target_link_libraries(kinetra_dotio_behavior_benchmark PRIVATE kinetra_dotio_logic)
    add_test(NAME kinetra_dotio_behavior_smoke
        COMMAND kinetra_dotio_behavior_benchmark --seeds 1 --seconds 10)
endif()
if(KINETRA_BUILD_RUNTIME)
    add_library(kinetra_dotio_presentation src/presentation/presentation.cpp src/presentation/session.cpp)
    target_link_libraries(kinetra_dotio_presentation PUBLIC kinetra_dotio_logic kinetra_lab Kinetra::Runtime)
endif()
if(BUILD_TESTING AND KINETRA_BUILD_RUNTIME AND KINETRA_TEST_GRAPHICS)
    kinetra_add_doctest(kinetra_dotio_presentation_tests PREFIX kinetra_dotio_graphics
        SOURCES tests/presentation_tests.cpp LIBRARIES kinetra_dotio_presentation
        LABELS graphics)
endif()
if(KINETRA_BUILD_RUNTIME)
    add_executable(kinetra_dotio src/main.cpp)
    target_link_libraries(kinetra_dotio PRIVATE kinetra_dotio_presentation nlohmann_json::nlohmann_json)
    kinetra_executable_icon(kinetra_dotio)
    if(BUILD_TESTING)
        add_test(NAME kinetra_dotio_inspector_trace COMMAND ${CMAKE_COMMAND}
            "-DPROGRAM=$<TARGET_FILE:kinetra_dotio>"
            "-DARGS=--seed;42;--ticks;300;--inspect;0-1"
            "-DTICKS=300" "-DEXPECTED=STATE FORCE"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/../shared/tests/inspector_trace.cmake")
    endif()
    kinetra_lab_add_smoke(dotio kinetra_dotio --difficulty medium --seed 42)
endif()
