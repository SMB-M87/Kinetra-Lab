// Window-free tests of the bundled launcher's registrations (launcher/include/applications.h)
// through kinetra::LaunchMenu: names and order, Dotio's difficulties and default, hover and
// keyboard selection, and picking at resized viewports. No session is created.
#include "demo/presentation/session.h"
#include "dotio/presentation/session.h"
#include "launcher/applications.h"

#include <doctest/doctest.h>

#include <cstddef>
#include <stdexcept>

namespace {
using launcher::Variant;

kinetra::LaunchMenu lab_menu(std::size_t dotio_difficulty = dotio::default_difficulty) {
    std::vector<kinetra::LaunchChoice> choices;
    for (const auto &registration : launcher::applications({0, 42, dotio_difficulty}))
        choices.push_back(registration.choice);
    return kinetra::LaunchMenu(choices);
}
kinetra::Input key(kinetra::Key k) {
    kinetra::Input input;
    input.pressed[std::size_t(k)] = true;
    return input;
}
constexpr std::size_t dotio_row = static_cast<std::size_t>(Variant::dotio);
constexpr std::size_t demo_row = static_cast<std::size_t>(Variant::demo);
} // namespace

TEST_CASE("the registrations follow the variant order") {
    const auto registrations = launcher::applications({0, 42, dotio::default_difficulty});
    REQUIRE(registrations.size() == 3);
    CHECK(registrations[std::size_t(Variant::demo)].choice.name == "DEMO");
    CHECK(registrations[std::size_t(Variant::dotio)].choice.name == "DOTIO");
    CHECK(registrations[std::size_t(Variant::i4sim)].choice.name == "I4SIM");
}

TEST_CASE("every bundled application has its own still and description") {
    const auto registrations = launcher::applications({0, 42, dotio::default_difficulty});
    for (const auto &registration : registrations) {
        const auto &choice = registration.choice;
        CAPTURE(choice.name);
        CHECK_FALSE(choice.description.empty());
        REQUIRE(choice.preview);
        CHECK(choice.preview->width() == 320);
        CHECK(choice.preview->height() == 180);
    }
    CHECK(registrations[0].choice.preview != registrations[1].choice.preview);
    CHECK(registrations[1].choice.preview != registrations[2].choice.preview);
    CHECK(registrations[0].choice.preview != registrations[2].choice.preview);
}

TEST_CASE("dotio offers its difficulties with the configured default") {
    const auto registrations = launcher::applications({0, 42, 2});
    const auto &dotio_choice = registrations[dotio_row].choice;
    CHECK(dotio_choice.configurations == std::vector<std::string>{"EASY", "MEDIUM", "HARD"});
    CHECK(dotio_choice.default_configuration == 2);
    CHECK(launcher::selection(Variant::dotio, {0, 42, 2}).configuration == 2);
    CHECK(launcher::selection(Variant::demo, {0, 42, 2}).configuration == 0);
}

TEST_CASE("difficulty names parse and unknown names are rejected") {
    CHECK(dotio::difficulty_index("easy") == 0);
    CHECK(dotio::difficulty_index("medium") == dotio::default_difficulty);
    CHECK(dotio::difficulty_index("hard") == 2);
    CHECK_THROWS_AS(dotio::difficulty_index("impossible"), std::invalid_argument);
}

TEST_CASE("Demo offers its eleven scenarios and starts the selected scene") {
    const launcher::Options options{0, 42, 0, 2};
    const auto registrations = launcher::applications(options);
    const auto &registration = registrations[demo_row];
    CHECK(registration.choice.configurations ==
          std::vector<std::string>{"TERRAIN", "GRAVITY", "MAZE", "DOORWAY", "CORRIDOR", "CROSSING", "CIRCLE",
                                   "BOTTLENECK", "CHAOS", "FLEET", "SWARM"});
    CHECK(registration.choice.default_configuration == 2);
    const auto selection = launcher::selection(Variant::demo, options);
    CHECK(selection.configuration == 2);
    kinetra::Camera2D camera;
    for (std::size_t index = 0; index < registration.choice.configurations.size(); ++index) {
        auto session = registration.create(camera, index);
        session->advance(0, {}, camera);
        const auto name = registration.choice.configurations[index];
        CHECK(session->title() == (index == 0 ? "Demo" : "Demo - " + name));
    }
    CHECK(demo::parse_scenario("terrain") == demo::Scenario::terrain);
    CHECK(demo::parse_scenario("doorway") == demo::Scenario::doorway);
    CHECK_THROWS_AS(demo::parse_scenario("impossible"), std::invalid_argument);
}

TEST_CASE("hover preselects and keyboard selection survives a resting pointer") {
    auto menu = lab_menu();
    const kinetra::Vec2 viewport{1280, 800};
    kinetra::Input input;
    input.mouse = menu.layout(viewport).row_center(dotio_row);
    CHECK_MESSAGE((!menu.update(input, viewport) && menu.selected() == dotio_row && !menu.configuring()),
                  "hover must preselect Dotio without launching");
    input.pressed[std::size_t(kinetra::Key::up)] = true;
    CHECK_MESSAGE((!menu.update(input, viewport) && menu.selected() == 0),
                  "keyboard navigation must override a resting pointer");
    input.pressed = {};
    CHECK_MESSAGE((!menu.update(input, viewport) && menu.selected() == 0),
                  "a resting pointer must preserve the keyboard selection");
    input.mouse = menu.layout(viewport).row_center(0);
    menu.update(input, viewport);
    input.mouse = menu.layout(viewport).row_center(dotio_row);
    CHECK_MESSAGE((!menu.update(input, viewport) && menu.selected() == dotio_row),
                  "moving back over Dotio must restore the hover selection");
}

TEST_CASE("dotio opens its difficulty screen at the default and confirms a choice") {
    auto menu = lab_menu();
    const kinetra::Vec2 viewport{1280, 800};
    menu.update(key(kinetra::Key::down), viewport);
    REQUIRE(menu.selected() == dotio_row);
    CHECK(!menu.update(key(kinetra::Key::enter), viewport));
    CHECK_MESSAGE((menu.configuring() && menu.configuration() == dotio::default_difficulty),
                  "Dotio did not open its Medium-default difficulty screen");
    menu.update(key(kinetra::Key::down), viewport);
    CHECK(menu.configuration() == 2);
    const auto confirmed = menu.update(key(kinetra::Key::enter), viewport);
    REQUIRE(confirmed);
    CHECK(confirmed->application == dotio_row);
    CHECK(confirmed->configuration == 2);
    CHECK_FALSE(menu.configuring());
}

TEST_CASE("cancel leaves the difficulty screen") {
    auto menu = lab_menu();
    const kinetra::Vec2 viewport{1280, 800};
    menu.update(key(kinetra::Key::down), viewport);
    menu.update(key(kinetra::Key::enter), viewport);
    REQUIRE(menu.configuring());
    CHECK(!menu.update(key(kinetra::Key::m), viewport));
    CHECK_FALSE(menu.configuring());
}

TEST_CASE("picking works at resized viewports") {
    for (const auto viewport : {kinetra::Vec2{1280, 800}, kinetra::Vec2{200, 150}, kinetra::Vec2{360, 620}}) {
        CAPTURE(viewport.x);
        for (std::size_t scenario = 0; scenario < demo::scenario_labels().size(); ++scenario) {
            CAPTURE(scenario);
            auto menu = lab_menu();
            kinetra::Input input;
            input.clicked = true;
            input.mouse = menu.layout(viewport).row_center(demo_row);
            CHECK((!menu.update(input, viewport) && menu.configuring()));
            // The scenarios fill two pages; NEXT shows the second.
            const auto page_size = kinetra::MenuLayout::page_size;
            if (scenario >= page_size) {
                input.mouse =
                    menu.layout(viewport).footer_center(*menu.footer_x(kinetra::FooterButton::next));
                CHECK_FALSE(menu.update(input, viewport));
            }
            input.mouse = menu.layout(viewport).row_center(scenario % page_size);
            const auto picked = menu.update(input, viewport);
            CHECK((picked && picked->application == demo_row && picked->configuration == scenario));
        }
        {
            auto menu = lab_menu();
            kinetra::Input input;
            input.clicked = true;
            input.mouse = menu.layout(viewport).row_center(std::size_t(Variant::i4sim));
            const auto picked = menu.update(input, viewport);
            CHECK((picked && picked->application == std::size_t(Variant::i4sim)));
        }
        for (std::size_t difficulty = 0; difficulty < 3; ++difficulty) {
            CAPTURE(difficulty);
            auto menu = lab_menu();
            kinetra::Input input;
            input.clicked = true;
            input.mouse = menu.layout(viewport).row_center(dotio_row);
            CHECK_MESSAGE((!menu.update(input, viewport) && menu.configuring()),
                          "Dotio click skipped difficulty");
            input.mouse = menu.layout(viewport).row_center(difficulty);
            const auto picked = menu.update(input, viewport);
            CHECK_MESSAGE((picked && picked->application == dotio_row && picked->configuration == difficulty),
                          "resized difficulty picking missed");
        }
    }
}
