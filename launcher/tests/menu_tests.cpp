#include "../include/menu.h"
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
} // namespace
int main() {
    try {
        launcher::Menu menu;
        kinetra::Input input;
        input.pressed[std::size_t(kinetra::Key::down)] = true;
        require(!menu.update(input, {1280, 800}) && menu.selected() == 1, "Menu selection failed");
        input = {};
        input.pressed[std::size_t(kinetra::Key::enter)] = true;
        require(!menu.update(input, {1280, 800}) && menu.choosing_difficulty() &&
                    menu.difficulty() == logic::Difficulty::medium,
                "Dotio did not open its Medium-default difficulty screen");
        input = {};
        input.pressed[std::size_t(kinetra::Key::down)] = true;
        require(!menu.update(input, {1280, 800}) && menu.difficulty() == logic::Difficulty::hard,
                "Difficulty keyboard selection failed");
        input = {};
        input.pressed[std::size_t(kinetra::Key::enter)] = true;
        require(menu.update(input, {1280, 800}) == launcher::Variant::dotio &&
                    menu.difficulty() == logic::Difficulty::hard && !menu.choosing_difficulty(),
                "Hard confirmation failed");
        for (const auto viewport : {kinetra::Vec2{1280, 800}, kinetra::Vec2{200, 150}}) {
            input = {};
            input.clicked = true;
            input.mouse = launcher::MenuLayout(viewport).row_center(0);
            require(menu.update(input, viewport) == launcher::Variant::demo,
                    "Resized Demo menu click missed");
        }
        for (const auto viewport : {kinetra::Vec2{1280, 800}, kinetra::Vec2{200, 150}}) {
            for (std::size_t difficulty = 0; difficulty < 3; ++difficulty) {
                input = {};
                input.clicked = true;
                input.mouse = launcher::MenuLayout(viewport).row_center(1);
                require(!menu.update(input, viewport) && menu.choosing_difficulty(),
                        "Dotio click skipped difficulty");
                input.mouse = launcher::MenuLayout(viewport).row_center(difficulty);
                require(menu.update(input, viewport) == launcher::Variant::dotio &&
                            menu.difficulty() == static_cast<logic::Difficulty>(difficulty),
                        "Resized difficulty picking missed");
            }
        }
        input = {};
        input.pressed[std::size_t(kinetra::Key::enter)] = true;
        menu.update(input, {1280, 800});
        input = {};
        input.pressed[std::size_t(kinetra::Key::m)] = true;
        require(!menu.update(input, {1280, 800}) && !menu.choosing_difficulty(),
                "Back did not cancel difficulty selection");
        require(logic::parse_difficulty("easy") == logic::Difficulty::easy &&
                    logic::parse_difficulty("hard") == logic::Difficulty::hard,
                "CLI mode parsing failed");
        bool rejected = false;
        try {
            logic::parse_difficulty("impossible");
        } catch (const std::invalid_argument &) {
            rejected = true;
        }
        require(rejected, "Unknown difficulty was accepted");
        std::cout << "PASS launcher menu selection and resized picking\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
