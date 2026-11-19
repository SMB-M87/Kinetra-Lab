#include "session.h"
#include "blueprint_screen.h"
#include "hud.h"
#include "scenario.h"
#include "simulation.h"
#include "view.h"
#include "world_view.h"
#include <iostream>
#include <kinetra/bitmap_text.h>
#include <kinetra/launcher.h>
#include <kinetra/settings_view.h>
#include <lab/input.h>
#include <lab/settings.h>

namespace i4sim {
namespace {
using namespace kinetra;
/// No debug layer at all: what the world view draws outside debug mode.
constexpr ViewLayers hidden_layers{false, false, false, false, false, false, false};
class FactorySession final : public lab::Session {
    std::uint32_t seed_;
    BlueprintScreen screen_;
    /// Setup of the loaded blueprint; R replays it.
    std::optional<Scenario> scenario_;
    std::unique_ptr<Simulation> simulation_;
    bool back_{}, exit_{}, fitted_{}, running_{};
    bool workload_{true};
    DebugMenu menu_;
    bool statistics_{true}, debug_{true}, frame_times_{};
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
            rows.push_back({"GRID", &layers_.grid});
            rows.push_back({"HEATMAP", &layers_.heatmap});
            rows.push_back({"PATHS", &layers_.paths});
            rows.push_back({"FORCES", &layers_.forces});
            rows.push_back({"BORDERS", &layers_.borders});
            rows.push_back({"DETECTION", &layers_.detection});
            rows.push_back({"CONTACT SHAPES", &layers_.contact_shapes});
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
    ViewLayers layers_;
    std::optional<std::size_t> selected_mover_, selected_producer_;
    std::string error_;
    double error_remaining_{};
    unsigned ups_;
    void report_error(const std::string &message) {
        error_ = message;
        std::cerr << "i4sim: " << error_ << '\n';
    }
    void refresh_catalog() {
        try {
            screen_.refresh();
            error_.clear();
        } catch (const std::exception &e) {
            report_error(e.what());
        }
    }
    void load(const std::filesystem::path &path) {
        try {
            auto next = std::make_unique<Simulation>(load_blueprint(path), seed_);
            auto scenario = load_scenario(path);
            if (scenario)
                apply_scenario(*scenario, *next);
            simulation_ = std::move(next);
            scenario_ = std::move(scenario);
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
        camera.position = flip_y(size / 2);
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
        : seed_(seed), screen_(directory), running_(running), ups_(ups) {
        if (ups < 1 || ups > 10000)
            throw std::invalid_argument("UPS must be in 1..10000");
        refresh_catalog();
        if (initial)
            load(*initial);
    }
    void advance(double elapsed, const Input &input, Camera2D &camera) override {
        if (!simulation_) {
            const auto event = screen_.update(input, camera.viewport);
            switch (event.action) {
            case ScreenAction::leave:
                back_ = true;
                break;
            case ScreenAction::exit:
                exit_ = true;
                break;
            case ScreenAction::load:
                load(event.blueprint);
                break;
            case ScreenAction::rediscover:
                refresh_catalog();
                break;
            case ScreenAction::none:
                break;
            }
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
        if (input.hit(Key::r)) {
            const auto config = simulation_->motion_settings();
            simulation_ = std::make_unique<Simulation>(simulation_->blueprint(), seed_);
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
                    return;
                }
            } else
                simulation_->configure_motion(config);
            selected_mover_.reset();
            selected_producer_.reset();
        }
        if (input.hit(Key::g))
            simulation_->set_workload(!simulation_->workload());
        if (input.hit(Key::f))
            fit(camera);
        if (input.hit(Key::one))
            layers_.grid = !layers_.grid;
        if (input.hit(Key::two))
            layers_.heatmap = !layers_.heatmap;
        if (input.hit(Key::three))
            layers_.paths = !layers_.paths;
        if (input.hit(Key::four))
            layers_.forces = !layers_.forces;
        if (input.hit(Key::five))
            layers_.borders = !layers_.borders;
        if (input.hit(Key::six))
            layers_.detection = !layers_.detection;
        if (input.hit(Key::q))
            simulation_->updates_per_second(std::max(1u, simulation_->updates_per_second() / 2));
        if (input.hit(Key::e))
            simulation_->updates_per_second(std::min(10000u, simulation_->updates_per_second() * 2));
        kinetra::zoom(camera, debug_ && !captured ? input.wheel : 0, zoom_limits);
        const float step = float(std::min(elapsed, 0.1)) * 400 / camera.pixels_per_unit;
        camera.position += lab::direction(input) * step;
        if (!captured && (input.clicked || input.right_clicked)) {
            const auto point = flip_y(camera.screen_to_world(input.mouse));
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
    }
    void render(Runtime &runtime) override {
        if (!simulation_) {
            screen_.draw(runtime, error_);
            return;
        }
        if (!fitted_)
            fit(runtime.camera);
        runtime.begin(world_background);
        // Outside debug mode the layers keep their selection but are not drawn.
        draw_world(runtime, *simulation_, debug_ ? layers_ : hidden_layers, selected_mover_);
        draw_hud(runtime, *simulation_,
                 {statistics_, selected_mover_, selected_producer_,
                  scenario_ ? std::string_view(scenario_->description) : std::string_view(), error_});
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
