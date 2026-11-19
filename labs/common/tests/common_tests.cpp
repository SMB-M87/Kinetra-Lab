// Window-free tests for the input policy shared by the Lab applications
// (labs/common/include/lab/input.h).
#include <lab/input.h>

#include <doctest/doctest.h>

#include <cstddef>
#include <initializer_list>

namespace {
kinetra::Input holding(std::initializer_list<kinetra::Key> keys) {
    kinetra::Input input;
    for (auto key : keys)
        input.held[static_cast<std::size_t>(key)] = true;
    return input;
}
} // namespace

TEST_CASE("wasd and arrows map to the same world direction") {
    using kinetra::Key;
    CHECK(lab::direction(holding({Key::d})).x == 1);
    CHECK(lab::direction(holding({Key::right})).x == 1);
    CHECK(lab::direction(holding({Key::a})).x == -1);
    CHECK(lab::direction(holding({Key::left})).x == -1);
    CHECK(lab::direction(holding({Key::w})).y == 1);
    CHECK(lab::direction(holding({Key::up})).y == 1);
    CHECK(lab::direction(holding({Key::s})).y == -1);
    CHECK(lab::direction(holding({Key::down})).y == -1);
}

TEST_CASE("opposite keys cancel and diagonals stay unnormalized") {
    using kinetra::Key;
    const auto cancelled = lab::direction(holding({Key::a, Key::d, Key::w, Key::down}));
    CHECK(cancelled.x == 0);
    CHECK(cancelled.y == 0);
    const auto diagonal = lab::direction(holding({Key::d, Key::up}));
    CHECK(diagonal.x == 1);
    CHECK(diagonal.y == 1);
    CHECK_MESSAGE(lab::direction(holding({Key::d, Key::right})).x == 1, "a key and its arrow do not add up");
}

TEST_CASE("a modal panel pauses and then restores a running application") {
    lab::ModalPause pause;
    CHECK_FALSE(pause.update(false, false));
    CHECK(pause.update(true, false));
    CHECK_MESSAGE(pause.update(true, true), "the application stays paused while the panel is open");
    CHECK_FALSE(pause.update(false, true));
}

TEST_CASE("a modal panel keeps an application paused that was paused before") {
    lab::ModalPause pause;
    CHECK(pause.update(true, true));
    CHECK(pause.update(false, true));
}

TEST_CASE("without a panel the pause state passes through") {
    lab::ModalPause pause;
    CHECK(pause.update(false, true));
    CHECK_FALSE(pause.update(false, false));
}
