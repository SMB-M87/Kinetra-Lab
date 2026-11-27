#include "i4sim/presentation/detail/blueprint_screen.h"

#include "i4sim/blueprint/blueprint.h"

#include <kinetra/runtime/hud_text.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

namespace i4sim {
namespace {
using namespace kinetra;

/// Subdirectory of the catalog with the showcase scenes, listed under one SHOWCASE row.
constexpr const char *showcase_directory = "showcase";
} // namespace

BlueprintScreen::BlueprintScreen(std::filesystem::path directory) : directory_(std::move(directory)) {
}

void BlueprintScreen::refresh() {
    files_.clear();
    showcase_files_.clear();
    menu_.reset();
    files_ = discover_blueprints(directory_);
    const auto showcase = directory_ / showcase_directory;
    if (std::filesystem::is_directory(showcase))
        showcase_files_ = discover_blueprints(showcase);
    if (files_.empty() && showcase_files_.empty())
        throw std::runtime_error("No JSON blueprints found in " + directory_.string());
    menu_ = std::make_unique<LaunchMenu>(choices(), "I4SIM LOADSCREEN");
}

std::vector<LaunchChoice> BlueprintScreen::choices() const {
    std::vector<LaunchChoice> result;
    // One row whose second page lists the scenes, so the catalog stays one page per group.
    // It comes first: the scenes are what a new viewer should see first.
    if (!showcase_files_.empty()) {
        std::vector<std::string> scenes;
        for (const auto &file : showcase_files_)
            scenes.push_back(file.stem().string());
        result.push_back({"SHOWCASE", "SCENES WITH THEIR OWN SETUP", std::move(scenes), 0});
    }
    for (const auto &file : files_)
        result.push_back({file.stem().string(), "JSON BLUEPRINT", {}, 0});
    return result;
}

ScreenEvent BlueprintScreen::update(const Input &input, Vec2 viewport, std::string_view message) {
    const bool leave = input.hit(Key::escape) || input.hit(Key::m);
    if (leave && menu_ && menu_->configuring()) {
        // On the SHOWCASE page, Escape and M go back to the blueprint list, which
        // LaunchMenu does on M; the launcher itself only sees M from a session.
        Input back;
        back.pressed[std::size_t(Key::m)] = true;
        back.mouse = input.mouse;
        menu_->update(back, viewport);
        return {};
    }
    if (leave)
        return {ScreenAction::leave, {}};
    if (!menu_)
        return {input.hit(Key::enter) ? ScreenAction::rediscover : ScreenAction::none, {}};
    const auto choice = menu_->update(input, viewport, message);
    if (menu_->error_closed())
        return {ScreenAction::dismiss, {}};
    if (choice) {
        // The SHOWCASE row, when there is one, precedes the blueprints; its configuration
        // is the scene.
        const bool showcase = !showcase_files_.empty();
        if (showcase && choice->application == 0)
            return {ScreenAction::load, showcase_files_.at(choice->configuration)};
        return {ScreenAction::load, files_.at(choice->application - (showcase ? 1 : 0))};
    }
    return {menu_->exit_requested() ? ScreenAction::exit : ScreenAction::none, {}};
}

void BlueprintScreen::draw(Runtime &runtime, std::string_view message) const {
    if (menu_) {
        menu_->draw(runtime, message);
        return;
    }
    runtime.begin();
    const auto top = [](int row) { return hud::margin + static_cast<float>(row) * hud::line_height; };
    draw_text(runtime, "I4SIM LOAD ERROR - ESC TO LAUNCHER", {hud::margin, top(0)}, hud::text_scale,
              hud::error_color);
    draw_text(runtime, "ENTER RETRIES BLUEPRINT DISCOVERY", {hud::margin, top(1)}, hud::text_scale,
              {1, 1, 1});
    // The message sits at the bottom, as below the menu when blueprints load, and may use
    // every line below the two above; it can hold a long path.
    hud::draw_error(runtime, message, 1, hud::lines_below(runtime.camera.viewport, top(2)));
    runtime.end();
}
} // namespace i4sim
