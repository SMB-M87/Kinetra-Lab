#pragma once
#include "../../labs/dotio/logic/include/difficulty.h"
#include <kinetra/launcher.h>

namespace launcher {
enum class Variant { demo, dotio, i4sim };
using MenuLayout = kinetra::MenuLayout;
inline std::vector<kinetra::LaunchChoice> choices(logic::Difficulty difficulty) {
    logic::settings_for(difficulty);
    return {{"DEMO", "PHYSICS AND NAVIGATION", {}, 0},
            {"DOTIO", "SURVIVAL", {"EASY", "MEDIUM", "HARD"}, std::size_t(difficulty)},
            {"I4SIM", "LOCAL FACTORY SIMULATION", {}, 0}};
}
/// Application composition supplies difficulty semantics to the generic menu.
class Menu {
    kinetra::LaunchMenu menu_;

  public:
    explicit Menu(logic::Difficulty difficulty = logic::Difficulty::medium) : menu_(choices(difficulty)) {}
    bool exit_requested() const { return menu_.exit_requested(); }
    std::size_t selected() const { return menu_.selected(); }
    bool choosing_difficulty() const { return menu_.configuring(); }
    logic::Difficulty difficulty() const { return static_cast<logic::Difficulty>(menu_.configuration()); }
    std::size_t row_count() const { return menu_.row_count(); }
    std::string_view row_name(std::size_t index) const { return menu_.row_name(index); }
    std::optional<Variant> update(const kinetra::Input &input, kinetra::Vec2 viewport) {
        auto selection = menu_.update(input, viewport);
        if (!selection)
            return std::nullopt;
        return static_cast<Variant>(selection->application);
    }
    void draw(kinetra::Runtime &runtime) const { menu_.draw(runtime); }
};
void render_menu(kinetra::Runtime &runtime, const Menu &menu);
} // namespace launcher
