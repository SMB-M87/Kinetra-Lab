add_library(kinetra_dotio_logic
    gmath/src/gmath.cpp
    logic/src/arrow.cpp
    logic/src/bolt.cpp
    logic/src/enemy.cpp
    logic/src/fuzzy.cpp
    logic/src/graph.cpp
    logic/src/grid.cpp
    logic/src/moving_object.cpp
    logic/src/physics_object.cpp
    logic/src/player.cpp
    logic/src/priority_queue.cpp
    logic/src/stack.cpp
    logic/src/state.cpp
    logic/src/static_object.cpp
    logic/src/vertex.cpp
    logic/src/world.cpp
)
target_compile_features(kinetra_dotio_logic PUBLIC cxx_std_20)
target_include_directories(kinetra_dotio_logic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include logic/include
    gmath/include)
target_link_libraries(kinetra_dotio_logic PUBLIC Kinetra::Simulation)
if(BUILD_TESTING)
    add_executable(kinetra_dotio_tests tests/dotio_tests.cpp)
    target_link_libraries(kinetra_dotio_tests PRIVATE kinetra_dotio_logic)
    add_test(NAME kinetra_dotio COMMAND kinetra_dotio_tests)
endif()
if(KINETRA_BUILD_RUNTIME)
    add_library(kinetra_dotio_presentation src/presentation.cpp src/session.cpp)
    target_link_libraries(kinetra_dotio_presentation PUBLIC kinetra_dotio_logic Kinetra::Runtime)
endif()
if(BUILD_TESTING AND KINETRA_BUILD_RUNTIME AND KINETRA_TEST_GRAPHICS)
    add_executable(kinetra_dotio_presentation_tests tests/presentation_tests.cpp)
    target_link_libraries(kinetra_dotio_presentation_tests PRIVATE kinetra_dotio_presentation)
    add_test(NAME kinetra_dotio_graphics COMMAND kinetra_dotio_presentation_tests)
    set_tests_properties(kinetra_dotio_graphics PROPERTIES LABELS graphics)
endif()
if(KINETRA_BUILD_RUNTIME)
    add_executable(kinetra_dotio src/main.cpp)
    target_link_libraries(kinetra_dotio PRIVATE kinetra_dotio_presentation)
endif()
