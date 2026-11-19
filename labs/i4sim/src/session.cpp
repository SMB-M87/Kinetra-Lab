#include "session.h"
#include "scenario.h"
#include "simulation.h"
#include <iostream>
#include <kinetra/bitmap_text.h>
#include <kinetra/launcher.h>
#include <kinetra/settings_view.h>
#include <lab/input.h>
#include <lab/settings.h>
#include <sstream>

namespace i4sim {
namespace {
using namespace kinetra;
/// HUD text for a selected mover's status.
const char *status_text(MoverStatus status) {
    switch (status) {
    case MoverStatus::held:
        return "BLOCKED";
    case MoverStatus::corridor_retreat:
        return "CORRIDOR RETREAT";
    case MoverStatus::passing_detour:
        return "PASSING DETOUR";
    case MoverStatus::unreachable:
        return "UNREACHABLE";
    case MoverStatus::allocated:
        return "ALLOCATED";
    case MoverStatus::available:
        return "IDLE - SHIFT CLICK GOAL";
    }
    return "";
}
Vec2 world(Vec2 p) {
    return {p.x, -p.y};
}
/// Subdirectory of the catalog with the showcase scenes, listed under one SHOWCASE row.
constexpr const char *showcase_directory = "showcase";
/// Camera zoom range in pixels per millimeter: from a whole hall to a single mover.
constexpr ZoomLimits zoom_limits{0.005f, 10};
class FactorySession final : public lab::Session {
    std::uint32_t seed_;
    /// Bundled blueprints in the catalog directory, then the scenes in its showcase/ subdirectory.
    std::vector<std::filesystem::path> files_, showcase_files_;
    /// Setup of the loaded blueprint; R replays it.
    std::optional<Scenario> scenario_;
    /// When a restarting scenario starts again; one per simulation.
    RestartTimer restart_timer_;
    std::unique_ptr<LaunchMenu> loadscreen_;
    std::unique_ptr<Simulation> simulation_;
    bool back_{}, exit_{}, fitted_{}, running_{};
    bool workload_{true};
    DebugMenu menu_;
    bool statistics_{true}, debug_{true}, contacts_{true}, frame_times_{};
    // Declared after the flags it borrows.
    SettingsOverlay settings_{lab::settings_rows([] { return zoom_limits; }, statistics_, debug_,
                                                 frame_times_,
                                                 {"WASD / ARROWS  MOVE", "CLICK SELECT / RIGHT BLOCK",
                                                  "SHIFT CLICK FREE MOVER GOAL", "SPACE PAUSE / R RESET"},
                                                 {"DEBUG PAGES: VIEW / MOTION"}),
                              &debug_};
    unsigned page_{};
    lab::ModalPause pause_;
    void change_motion(const std::function<void(MotionSettings &)> &change) {
        auto config = simulation_->motion_settings();
        change(config);
        try {
            simulation_->configure_motion(config);
            error_.clear();
        } catch (const std::exception &e) {
            error_ = e.what();
            error_remaining_ = 4;
        }
    }
    /// Starts a paused simulation or pauses a running one; a finished one stays finished.
    void toggle_pause() {
        if (simulation_->running())
            simulation_->pause();
        else
            simulation_->run();
    }
    std::vector<DebugOption> options() {
        std::vector<DebugOption> rows;
        // The menu toggles a borrowed bool; advance() copies it back into the simulation.
        workload_ = simulation_->workload();
        rows.push_back({page_ ? "PAGE: MOTION" : "PAGE: VIEW", nullptr, [this] { page_ = 1 - page_; }});
        if (!page_) {
            rows.push_back({"GRID", &grid_});
            rows.push_back({"HEATMAP", &heat_});
            rows.push_back({"PATHS", &paths_});
            rows.push_back({"FORCES", &vectors_});
            rows.push_back({"BORDERS", &borders_});
            rows.push_back({"DETECTION", &detection_});
            rows.push_back({"CONTACT SHAPES", &contacts_});
            rows.push_back({"PAUSE / RUN", nullptr, [this] { toggle_pause(); }});
            rows.push_back({"SINGLE TICK", nullptr, [this] { simulation_->single_step(); }});
            rows.push_back({"WORKLOAD", &workload_});
            rows.push_back({"FRAME SCENE", nullptr, [this] { fitted_ = false; }});
            rows.push_back({"BLUEPRINTS", nullptr, [this] { back_to_blueprints_ = true; }});
        } else {
            rows.push_back({"PROFILE", nullptr,
                            [this] {
                                change_motion(
                                    [](auto &c) { c.profile = SteeringProfile((int(c.profile) + 1) % 4); });
                            },
                            [this] {
                                switch (simulation_->motion_settings().profile) {
                                case SteeringProfile::original:
                                    return std::string("ORIGINAL");
                                case SteeringProfile::experimental_rvo:
                                    return std::string("RVO EXPERIMENT");
                                case SteeringProfile::reciprocal:
                                    return std::string("RECIPROCAL");
                                default:
                                    return std::string("IMPROVED");
                                }
                            }});
            rows.push_back({"PLANNER", nullptr,
                            [this] { change_motion([](auto &c) { c.heatmap = !c.heatmap; }); },
                            [this] { return simulation_->motion_settings().heatmap ? "HEAT A*" : "A*"; }});
            rows.push_back({"RECOVERY", nullptr,
                            [this] { change_motion([](auto &c) { c.recovery = !c.recovery; }); },
                            [this] { return simulation_->motion_settings().recovery ? "ON" : "OFF"; }});
            rows.push_back(
                {"SPEED +", nullptr, [this] { change_motion([](auto &c) { c.speed *= 1.25f; }); }});
            rows.push_back(
                {"SPEED -", nullptr, [this] { change_motion([](auto &c) { c.speed /= 1.25f; }); }});
            rows.push_back(
                {"FORCE +", nullptr, [this] { change_motion([](auto &c) { c.force *= 1.25f; }); }});
            rows.push_back(
                {"FORCE -", nullptr, [this] { change_motion([](auto &c) { c.force /= 1.25f; }); }});
            rows.push_back({"HORIZON +", nullptr, [this] {
                                change_motion([](auto &c) { c.horizon = std::min(64.f, c.horizon + 1); });
                            }});
            rows.push_back({"HORIZON -", nullptr, [this] {
                                change_motion([](auto &c) { c.horizon = std::max(1.f, c.horizon - 1); });
                            }});
            rows.push_back({"RESET PARAMETERS", nullptr, [this] {
                                change_motion([](auto &c) {
                                    c.speed = 2;
                                    c.force = .6f;
                                    c.horizon = 8;
                                });
                            }});
            rows.push_back({"UPS +", nullptr, [this] {
                                simulation_->updates_per_second(
                                    std::min(10000u, simulation_->updates_per_second() * 2));
                            }});
            rows.push_back({"UPS -", nullptr, [this] {
                                simulation_->updates_per_second(
                                    std::max(1u, simulation_->updates_per_second() / 2));
                            }});
        }
        if (page_ != 0)
            rows.push_back(
                {"PARKING TRAFFIC", nullptr,
                 [this] { change_motion([](auto &c) { c.parking_traffic = !c.parking_traffic; }); },
                 [this] { return simulation_->motion_settings().parking_traffic ? "ON" : "OFF"; }});
        if (page_ != 0)
            rows.push_back(
                {"JUNCTION TRAFFIC", nullptr,
                 [this] { change_motion([](auto &c) { c.junction_traffic = !c.junction_traffic; }); },
                 [this] { return simulation_->motion_settings().junction_traffic ? "ON" : "OFF"; }});
        // Keep the motion panel above the statistics on short viewports.
        if (page_ == 0)
            rows.push_back(
                {"CORRIDOR TRAFFIC", nullptr,
                 [this] { change_motion([](auto &c) { c.corridor_traffic = !c.corridor_traffic; }); },
                 [this] { return simulation_->motion_settings().corridor_traffic ? "ON" : "OFF"; }});
        if (page_ == 0)
            rows.push_back(
                {"PREDICTIVE PASS", nullptr,
                 [this] { change_motion([](auto &c) { c.predictive_passing = !c.predictive_passing; }); },
                 [this] {
                     const auto &c = simulation_->motion_settings();
                     if (c.profile == SteeringProfile::original ||
                         c.profile == SteeringProfile::experimental_rvo)
                         return std::string("LEGACY");
                     return std::string(c.predictive_passing ? "ON" : "OFF");
                 }});
        return rows;
    }
    bool back_to_blueprints_{};
    bool grid_{true}, heat_{true}, paths_{true}, vectors_{true}, borders_{true}, detection_{true};
    std::optional<std::size_t> selected_mover_, selected_producer_;
    std::string error_;
    double error_remaining_{};
    unsigned ups_;
    std::filesystem::path directory_;
    void report_error(const std::string &message) {
        error_ = message;
        std::cerr << "i4sim: " << error_ << '\n';
    }
    void refresh_catalog() {
        files_.clear();
        showcase_files_.clear();
        loadscreen_.reset();
        try {
            files_ = discover_blueprints(directory_);
            const auto showcase = directory_ / showcase_directory;
            showcase_files_ = std::filesystem::is_directory(showcase) ? discover_blueprints(showcase)
                                                                      : std::vector<std::filesystem::path>{};
            if (files_.empty() && showcase_files_.empty())
                throw std::runtime_error("No JSON blueprints found in " + directory_.string());
            loadscreen_ = std::make_unique<LaunchMenu>(choices(), "I4SIM LOADSCREEN");
            error_.clear();
        } catch (const std::exception &e) {
            report_error(e.what());
        }
    }
    std::vector<LaunchChoice> choices() const {
        std::vector<LaunchChoice> result;
        for (const auto &f : files_)
            result.push_back({f.stem().string(), "JSON BLUEPRINT", {}, 0});
        // One row whose second page lists the scenes, so the catalog stays one page per group.
        if (!showcase_files_.empty()) {
            std::vector<std::string> scenes;
            for (const auto &f : showcase_files_)
                scenes.push_back(f.stem().string());
            result.push_back({"SHOWCASE", "SCENES WITH THEIR OWN SETUP", std::move(scenes), 0});
        }
        return result;
    }
    /// Rebuilds the simulation from its blueprint and seed, paused, keeping the motion
    /// settings in use; a scene also gets its goals and workload again. Returns false when
    /// the scene cannot be applied: the session is then back on the loadscreen with the cause.
    bool reset() {
        const auto config = simulation_->motion_settings();
        simulation_ = std::make_unique<Simulation>(simulation_->blueprint(), seed_);
        restart_timer_ = {};
        error_.clear();
        if (scenario_) {
            // Replay the scene with the motion settings in use, so a changed setting can be
            // compared against the original run.
            auto replay = *scenario_;
            replay.motion = config;
            try {
                apply_scenario(replay, *simulation_);
            } catch (const std::exception &e) {
                // A partly applied scene is not the scene; return to the loadscreen with
                // the cause, as a failed load does.
                simulation_.reset();
                scenario_.reset();
                pause_ = {};
                report_error(e.what());
                return false;
            }
        } else
            simulation_->configure_motion(config);
        selected_mover_.reset();
        selected_producer_.reset();
        return true;
    }
    void load(const std::filesystem::path &path) {
        try {
            auto next = std::make_unique<Simulation>(load_blueprint(path), seed_);
            auto scenario = load_scenario(path);
            if (scenario)
                apply_scenario(*scenario, *next);
            simulation_ = std::move(next);
            scenario_ = std::move(scenario);
            restart_timer_ = {};
            if (running_)
                simulation_->run();
            simulation_->updates_per_second(ups_);
            selected_mover_.reset();
            selected_producer_.reset();
            error_.clear();
            fitted_ = false;

        } catch (const std::exception &e) {
            report_error(e.what());
        }
    }
    void fit(Camera2D &camera) {
        const auto size = simulation_->blueprint().dimension;
        camera.position = world(size / 2);
        camera.pixels_per_unit =
            std::min(camera.viewport.x / size.x, (camera.viewport.y - 120) / size.y) * 0.9f;
        camera.pixels_per_unit = std::max(camera.pixels_per_unit, zoom_limits.minimum);
        fitted_ = true;
    }
    void pick(Vec2 p, bool toggle) {
        for (std::size_t i = 0; i < simulation_->movers().size(); ++i) {
            const auto &m = simulation_->movers()[i];
            if (bounds(m.body).overlaps({p, p})) {
                selected_mover_ = i;
                selected_producer_.reset();
                if (toggle)
                    simulation_->toggle_mover(i);
                return;
            }
        }
        const float radius =
            std::max(simulation_->blueprint().cell_size.x, simulation_->blueprint().cell_size.y) / 2;
        for (std::size_t i = 0; i < simulation_->producers().size(); ++i)
            if (distance(p, simulation_->producers()[i].definition.position) <= radius) {
                selected_producer_ = i;
                selected_mover_.reset();
                if (toggle)
                    simulation_->toggle_producer(i);
                return;
            }
        if (!toggle) {
            selected_mover_.reset();
            selected_producer_.reset();
        }
    }

  public:
    FactorySession(std::uint32_t seed, const std::filesystem::path &directory,
                   const std::optional<std::filesystem::path> &initial, bool running, unsigned ups,
                   std::optional<std::filesystem::path>)
        : seed_(seed), running_(running), ups_(ups), directory_(directory) {
        if (ups < 1 || ups > 10000)
            throw std::invalid_argument("UPS must be in 1..10000");
        refresh_catalog();
        if (initial)
            load(*initial);
    }
    void advance(double elapsed, const Input &input, Camera2D &camera) override {
        if (!simulation_) {
            const bool leave = input.hit(Key::escape) || input.hit(Key::m);
            if (leave && loadscreen_ && loadscreen_->configuring()) {
                // On the SHOWCASE page, Escape and M go back to the blueprint list, which
                // LaunchMenu does on M; the launcher itself only sees M from a session.
                Input back;
                back.pressed[std::size_t(Key::m)] = true;
                back.mouse = input.mouse;
                loadscreen_->update(back, camera.viewport);
                return;
            }
            if (leave) {
                back_ = true;
                return;
            }
            if (loadscreen_) {
                if (const auto choice = loadscreen_->update(input, camera.viewport)) {
                    load(choice->application < files_.size() ? files_[choice->application]
                                                             : showcase_files_.at(choice->configuration));
                    return;
                }
                exit_ = loadscreen_->exit_requested();
            } else if (input.hit(Key::enter))
                refresh_catalog();
            return;
        }
        if (back_to_blueprints_) {
            simulation_.reset();
            scenario_.reset();
            // The saved pause state belonged to this simulation; the next one starts fresh.
            pause_ = {};
            back_to_blueprints_ = false;
            return;
        }
        if (error_remaining_ > 0) {
            error_remaining_ = std::max(0.0, error_remaining_ - elapsed);
            if (error_remaining_ == 0)
                error_.clear();
        }
        const bool modal = settings_.update(input, camera);
        if (pause_.update(settings_.visible(), !simulation_->running()))
            simulation_->pause();
        else
            simulation_->run();
        if (modal) {
            menu_.visible = false;
            return;
        }
        if (!debug_)
            menu_.visible = false;
        auto rows = options();
        const bool captured = debug_ && settings_.update_debug(input, camera.viewport, menu_, rows);
        if (!fitted_)
            fit(camera);
        if (captured)
            simulation_->set_workload(workload_);
        if (input.hit(Key::p) || input.hit(Key::space))
            toggle_pause();
        if (input.hit(Key::r) && !reset())
            return;
        if (input.hit(Key::g))
            simulation_->set_workload(!simulation_->workload());
        if (input.hit(Key::f))
            fit(camera);
        if (input.hit(Key::one))
            grid_ = !grid_;
        if (input.hit(Key::two))
            heat_ = !heat_;
        if (input.hit(Key::three))
            paths_ = !paths_;
        if (input.hit(Key::four))
            vectors_ = !vectors_;
        if (input.hit(Key::five))
            borders_ = !borders_;
        if (input.hit(Key::six))
            detection_ = !detection_;
        if (input.hit(Key::q))
            simulation_->updates_per_second(std::max(1u, simulation_->updates_per_second() / 2));
        if (input.hit(Key::e))
            simulation_->updates_per_second(std::min(10000u, simulation_->updates_per_second() * 2));
        kinetra::zoom(camera, debug_ && !captured ? input.wheel : 0, zoom_limits);
        const float step = float(std::min(elapsed, 0.1)) * 400 / camera.pixels_per_unit;
        camera.position += lab::direction(input) * step;
        if (!captured && (input.clicked || input.right_clicked)) {
            const auto point = world(camera.screen_to_world(input.mouse));
            if (input.down(Key::shift) && input.clicked && selected_mover_) {
                if (!simulation_->move_to(*selected_mover_, point)) {
                    error_ = "GOAL REJECTED: MOVER BUSY, BLOCKED OR GOAL UNREACHABLE";
                    error_remaining_ = 4;
                } else
                    error_.clear();
            } else
                pick(point, input.right_clicked);
        }
        simulation_->advance(elapsed);
        if (scenario_ && scenario_->repeat == ScenarioRepeat::restart && restart_timer_.due(*simulation_)) {
            // Unlike R, a restart continues the show: same speed, same run state.
            const bool was_running = simulation_->running();
            const auto rate = simulation_->updates_per_second();
            if (reset()) {
                simulation_->updates_per_second(rate);
                if (was_running)
                    simulation_->run();
            }
        }
    }
    void render(Runtime &runtime) override {
        if (!simulation_) {
            if (loadscreen_)
                loadscreen_->draw(runtime, error_);
            if (!loadscreen_) {
                runtime.begin();
                draw_text(runtime, "I4SIM LOAD ERROR - ESC TO LAUNCHER", {12, 12}, 1, {1, 0.4f, 0.3f});
                draw_text(runtime, "ENTER RETRIES BLUEPRINT DISCOVERY", {12, 30}, 1, {1, 1, 1});
                const auto columns = std::size_t(std::max(1.f, (runtime.camera.viewport.x - 24) / 6));
                float y = 50;
                for (std::size_t offset = 0; offset < error_.size() && y + 8 < runtime.camera.viewport.y;
                     offset += columns, y += 12)
                    draw_text(runtime, std::string_view(error_).substr(offset, columns), {12, y}, 1,
                              {1, 0.8f, 0.7f});
                runtime.end();
            }
            return;
        }
        if (!fitted_)
            fit(runtime.camera);
        const auto &b = simulation_->blueprint();
        runtime.begin({0.045f, 0.055f, 0.07f});
        runtime.box(world(b.dimension / 2), b.dimension / 2, 0, {0.13f, 0.15f, 0.18f});
        for (int id = 0; id < simulation_->grid().width() * simulation_->grid().height(); ++id) {
            const auto center = world(simulation_->grid().center(id));
            if (simulation_->grid().blocked(id))
                runtime.box(center, b.cell_size / 2, 0, {0.065f, 0.075f, 0.09f});
            else if (debug_ && heat_) {
                const float weight = std::min(1.f, simulation_->grid().weight(id) / 16);
                runtime.box(center, b.cell_size / 2, 0, {0.13f + weight * 0.6f, 0.15f, 0.2f});
            }
            if (debug_ && grid_) {
                const auto a = simulation_->grid().center(id) - b.cell_size / 2, z = a + b.cell_size;
                runtime.line(world(a), world({z.x, a.y}), 1 / runtime.camera.pixels_per_unit,
                             {0.3f, 0.35f, 0.4f});
                runtime.line(world(a), world({a.x, z.y}), 1 / runtime.camera.pixels_per_unit,
                             {0.3f, 0.35f, 0.4f});
            }
        }
        const float radius = std::max(b.cell_size.x, b.cell_size.y) / 2;
        for (const auto &p : simulation_->producers()) {
            Color color = p.definition.model == Model::Kuka      ? Color{.75f, .375f, 0}
                          : p.definition.model == Model::Staubli ? Color{.8f, .7f, .1f}
                          : p.definition.model == Model::Viper   ? Color{1, 1, 1}
                                                                 : Color{.35f, .5f, .65f};
            if (p.blocked)
                color = {0.8f, 0.15f, 0.2f};
            if (p.definition.rectangular)
                runtime.box(world(p.definition.position), {radius, radius}, 0, color);
            else
                runtime.circle(world(p.definition.position), radius, color);
            const Vec2 extent{b.mover_max_extent, b.mover_max_extent};
            runtime.box(world(p.definition.processing_position + extent / 2), extent / 2, 0,
                        {color.r * 0.4f, color.g * 0.4f, color.b * 0.4f});
            if (p.product)
                runtime.circle(world(p.definition.processing_position + extent / 2), radius / 4, color);
        }
        // Movers are drawn between their last two ticks, so motion stays even when a frame
        // runs three ticks and the next one four.
        const float blend = float(simulation_->interpolation());
        const auto shown = [blend](const Mover &m) {
            return lerp(m.previous_position, m.body.position, blend);
        };
        for (std::size_t i = 0; i < simulation_->movers().size(); ++i) {
            const auto &m = simulation_->movers()[i];
            const Vec2 position = shown(m);
            const auto center = world(position);
            runtime.box(center, m.body.half_extents, 0,
                        (m.held || m.contacting) ? Color{1, 0, 0} : Color{.7f, .7f, .7f});
            if (selected_mover_ == i)
                runtime.circle(center, 6 / runtime.camera.pixels_per_unit, {1, 1, 0});
            if (debug_ && contacts_)
                draw_debug_box(runtime, center, m.body.half_extents, 0, {1, 0, 0});
            if (debug_ && detection_)
                draw_debug_circle(runtime, center,
                                  length(m.body.half_extents) + simulation_->motion_settings().speed *
                                                                    simulation_->motion_settings().horizon,
                                  {0.25f, 0.4f, 0.6f});
            if (debug_ && paths_) {
                Vec2 from = position;
                for (auto target : m.route) {
                    runtime.line(world(from), world(target), 2 / runtime.camera.pixels_per_unit,
                                 {0.7f, 0.4f, 1});
                    from = target;
                }
            }
            if (debug_ && paths_)
                if (const auto target = simulation_->corridor_retreat_target(i))
                    draw_debug_vector(runtime, center, world(*target - position), {1, 0.3f, 0.7f});
            if (debug_ && paths_)
                if (const auto target = simulation_->passing_target(i))
                    draw_debug_vector(runtime, center, world(*target - position), {0.3f, 1, 0.7f});
            if (debug_ && vectors_) {
                draw_debug_vector(runtime, center, world(m.forces.border * 50), {1, .5f, 0});
                draw_debug_vector(runtime, center, world(m.forces.collision * 50), {1, 0, 0});
                draw_debug_vector(runtime, center, world(m.forces.predictive * 50), {1, 0, 1});
                draw_debug_vector(runtime, center, world(m.forces.seek * 50), {0, 1, 0});
                runtime.line(center, world(position + m.body.velocity * 40),
                             3 / runtime.camera.pixels_per_unit, {0, 1, 1});
                runtime.line(center, world(position + m.forces.acceleration * 100),
                             2 / runtime.camera.pixels_per_unit, {1, 1, 0});
            }
        }
        if (debug_ && borders_ && simulation_->motion_settings().junction_traffic)
            for (std::size_t i = 0; i < simulation_->junction_count(); ++i) {
                draw_debug_box(runtime, world(simulation_->junction_center(i)), b.cell_size / 2, 0,
                               {0, 0.8f, 1});
                if (const auto exit = simulation_->junction_exit_reservation(i))
                    draw_debug_box(runtime, world((exit->min + exit->max) / 2), (exit->max - exit->min) / 2,
                                   0, {0.6f, 1, 0});
            }
        if (debug_ && borders_ && simulation_->motion_settings().corridor_traffic)
            for (std::size_t i = 0; i < simulation_->corridor_count(); ++i) {
                const auto area = simulation_->corridor_bounds(i);
                draw_debug_box(runtime, world((area.min + area.max) / 2), (area.max - area.min) / 2, 0,
                               {0.2f, 0.5f, 1});
                if (const auto exit = simulation_->corridor_exit_reservation(i))
                    draw_debug_box(runtime, world((exit->min + exit->max) / 2), (exit->max - exit->min) / 2,
                                   0, {0.6f, 1, 0});
            }
        if (debug_ && borders_)
            for (const auto &s : simulation_->borders())
                runtime.line(world(s.start), world(s.end), 2 / runtime.camera.pixels_per_unit,
                             {1, 0.4f, 0.1f});
        // Place labels in screen space; stacked labels keep dense parking rows readable.
        std::vector<Aabb> labels;
        auto name = [&](std::string_view id, Vec2 position) {
            const Vec2 anchor = runtime.camera.world_to_screen(world(position));
            const float width = float(id.size()) * 6;
            Vec2 label = anchor + Vec2{-width / 2, -16};
            Aabb box{label - Vec2{3, 3}, label + Vec2{width + 3, 10}};
            for (std::size_t attempt = 0; attempt <= labels.size(); ++attempt) {
                bool overlap = false;
                for (const auto &other : labels)
                    if (box.overlaps(other))
                        overlap = true;
                if (!overlap)
                    break;
                label.y -= 14;
                box = {label - Vec2{3, 3}, label + Vec2{width + 3, 10}};
            }
            labels.push_back(box);
            runtime.line(runtime.camera.screen_to_world(anchor),
                         runtime.camera.screen_to_world(label + Vec2{width / 2, 7}),
                         1 / runtime.camera.pixels_per_unit, {.7f, .7f, .7f});
            runtime.box(runtime.camera.screen_to_world(label + Vec2{width / 2, 4}),
                        Vec2{width / 2 + 3, 7} / runtime.camera.pixels_per_unit, 0, {.025f, .035f, .06f});
            draw_text(runtime, id, label, 1, {1, 1, 1});
        };
        for (const auto &m : simulation_->movers())
            name(m.definition.id, shown(m));
        for (const auto &p : simulation_->producers())
            name(p.definition.id, p.definition.position);
        if (statistics_) {
            const auto &c = simulation_->motion_settings();
            std::ostringstream stats;
            stats << "TICK " << simulation_->ticks() << " UPS " << simulation_->updates_per_second()
                  << " CONTACTS " << simulation_->collisions() << " DURATION "
                  << simulation_->contact_ticks();
            if (c.predictive_passing &&
                (c.profile == SteeringProfile::improved || c.profile == SteeringProfile::reciprocal))
                stats << " PASS " << simulation_->predictive_pairs();
            if (c.corridor_traffic)
                stats << " RETREAT " << simulation_->corridor_recovering();
            draw_text(runtime, stats.str(), {10, runtime.camera.viewport.y - 40}, 1.5f, {1, 1, 0});
            std::ostringstream motion;
            motion << (simulation_->running() ? "RUNNING" : "PAUSED") << " SPEED " << c.speed << " FORCE "
                   << c.force << " HORIZON " << c.horizon;
            if (c.profile == SteeringProfile::reciprocal)
                motion << " AVOID LIMIT " << simulation_->avoidance_limited();
            if (c.parking_traffic)
                motion << " PARK WAIT " << simulation_->parking_waiting();
            if (c.junction_traffic)
                motion << " CROSS WAIT " << simulation_->junction_waiting() << "/"
                       << simulation_->junction_count();
            if (c.corridor_traffic)
                motion << " LANE WAIT " << simulation_->corridor_waiting() << "/"
                       << simulation_->corridor_count();
            if (c.junction_traffic || c.corridor_traffic)
                motion << " EXIT " << simulation_->junction_exit_waiting();
            draw_text(runtime, motion.str(), {10, runtime.camera.viewport.y - 20}, 1.5f, {1, 1, 0});
        }
        if (selected_mover_) {
            const auto &m = simulation_->movers()[*selected_mover_];
            std::ostringstream text;
            text << m.definition.id << " | DISTANCE " << int(m.distance) << " MM | CONTACT TICKS "
                 << m.collision_ticks << " | " << status_text(mover_status(m));
            draw_text(runtime, text.str(), {10, runtime.camera.viewport.y - 80}, 1.5f, {1, 1, 0});
            std::ostringstream details;
            details << "SPEED " << length(m.body.velocity) << " FORCE " << length(m.body.acceleration)
                    << " GOAL " << int(m.destination.x) << "," << int(m.destination.y)
                    << (simulation_->stalled(*selected_mover_) ? " STALLED" : "");
            draw_text(runtime, details.str(), {10, runtime.camera.viewport.y - 60}, 1.5f, {1, 1, 0});
        }
        if (selected_producer_) {
            const auto &p = simulation_->producers()[*selected_producer_];
            const auto text = p.definition.id + " | QUEUE " + std::to_string(p.queue.size()) +
                              " | REMAINING " + std::to_string(p.remaining) + " | TASKS " +
                              std::to_string(p.interactions) + (p.blocked ? " | BLOCKED" : "");
            draw_text(runtime, text, {10, runtime.camera.viewport.y - 60}, 1.5f, {1, 1, 0});
        }
        if (scenario_ && !scenario_->description.empty())
            draw_text(runtime, scenario_->description, {10, runtime.camera.viewport.y - 100}, 1.5f,
                      {0.75f, 0.85f, 1});
        if (!error_.empty())
            draw_text(runtime, error_, {12, 98}, 0.8f, {1, 0.4f, 0.3f});
        auto rows = options();
        menu_.draw(runtime, rows);
        if (frame_times_)
            draw_frame_times(runtime, runtime.frame_times());
        settings_.draw(runtime);
        runtime.end();
    }
    SessionRequest request() const override {
        if (exit_ || settings_.request() == SessionRequest::exit)
            return SessionRequest::exit;
        if (back_ || settings_.request() == SessionRequest::menu)
            return SessionRequest::menu;
        return SessionRequest::none;
    }
    std::string title() override { return "i4sim"; }
    std::string status() override {
        if (!simulation_)
            return error_.empty() ? "Loadscreen" : "Loadscreen | " + error_;
        return simulation_->blueprint().name + (simulation_->running() ? " | RUNNING" : " | PAUSED");
    }
};
} // namespace
std::unique_ptr<lab::Session> create_session(std::uint32_t seed, std::filesystem::path directory,
                                             std::optional<std::filesystem::path> initial, bool running,
                                             unsigned ups, std::optional<std::filesystem::path> output) {
    return std::make_unique<FactorySession>(seed, directory, initial, running, ups, std::move(output));
}
} // namespace i4sim
