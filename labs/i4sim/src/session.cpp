#include "session.h"
#include "simulation.h"
#include "factory_settings.h"
#include <cctype>
#include <iostream>
#include <kinetra/bitmap_text.h>
#include <kinetra/launcher.h>
#include <sstream>

namespace i4sim {
namespace {
using namespace kinetra;
Vec2 world(Vec2 p) {
    return {p.x, -p.y};
}
void factory_text(Runtime &runtime, std::string_view text, Vec2 origin, float scale, Color color) {
    std::string visible(text);
    for (char &c : visible) c = char(std::toupper(static_cast<unsigned char>(c)));
    kinetra::draw_text(runtime, visible, origin, scale, color);
    for (std::size_t i=0; i<visible.size(); ++i) {
        if (visible[i] != '.' && visible[i] != '_' && visible[i] != '-') continue;
        const bool dot = visible[i] == '.';
        const float y = visible[i] == '-' ? 3.5f : 6.5f;
        runtime.box(runtime.camera.screen_to_world(origin + Vec2{float(i)*6+2.5f,y}*scale),
                    Vec2{dot ? .5f : 2.5f,.5f}*scale/runtime.camera.pixels_per_unit, 0, color);
    }
}
class FactorySession final : public lab::Session {
    std::uint32_t seed_;
    std::vector<std::filesystem::path> files_;
    std::unique_ptr<LaunchMenu> loadscreen_;
    std::unique_ptr<Simulation> simulation_;
    bool back_{}, exit_{}, fitted_{}, running_{};
    FactorySettings settings_;
    DebugMenu menu_;
    bool statistics_{true}, debug_{true}, contacts_{true};
    unsigned page_{};
    bool modal_before_{};
    bool paused_before_settings_{};
    void change_motion(const std::function<void(MotionSettings &)> &change) {
        const bool was_paused = simulation_->paused;
        auto config = simulation_->motion_settings();
        change(config);
        simulation_->paused = true;
        try {
            simulation_->configure_motion(config);
            error_.clear();
        } catch (const std::exception &e) {
            error_ = e.what();
            error_remaining_ = 4;
        }
        simulation_->paused = was_paused;
    }
    std::vector<DebugOption> options() {
        std::vector<DebugOption> rows;
        rows.push_back({page_ ? "PAGE: MOTION" : "PAGE: VIEW", nullptr, [this] { page_ = 1-page_; }});
        if (!page_) {
            rows.push_back({"GRID", &grid_});
            rows.push_back({"HEATMAP", &heat_});
            rows.push_back({"PATHS", &paths_});
            rows.push_back({"FORCES", &vectors_});
            rows.push_back({"BORDERS", &borders_});
            rows.push_back({"DETECTION", &detection_});
            rows.push_back({"CONTACT SHAPES", &contacts_});
            rows.push_back({"PAUSE / RUN", nullptr, [this] { simulation_->paused = !simulation_->paused; }});
            rows.push_back({"SINGLE TICK", nullptr, [this] { simulation_->single_step(); }});
            rows.push_back({"WORKLOAD", &simulation_->workload});
            rows.push_back({"FRAME SCENE", nullptr, [this] { fitted_ = false; }});
            rows.push_back({"BLUEPRINTS", nullptr, [this] { back_to_blueprints_ = true; }});
        } else {
            rows.push_back({"PROFILE", nullptr, [this] { change_motion([](auto &c) {
                c.profile = SteeringProfile((int(c.profile)+1)%4); }); }, [this] {
                    switch (simulation_->motion_settings().profile) {
                    case SteeringProfile::original: return std::string("ORIGINAL");
                    case SteeringProfile::experimental_rvo: return std::string("RVO EXPERIMENT");
                    case SteeringProfile::reciprocal: return std::string("RECIPROCAL");
                    default: return std::string("IMPROVED"); } }});
            rows.push_back({"PLANNER", nullptr, [this] { change_motion([](auto &c) { c.heatmap = !c.heatmap; }); },
                [this] { return simulation_->motion_settings().heatmap ? "HEAT A*" : "A*"; }});
            rows.push_back({"RECOVERY", nullptr, [this] { change_motion([](auto &c) { c.recovery = !c.recovery; }); },
                [this] { return simulation_->motion_settings().recovery ? "ON" : "OFF"; }});
            rows.push_back({"SPEED +", nullptr, [this] { change_motion([](auto &c) { c.speed *= 1.25f; }); }});
            rows.push_back({"SPEED -", nullptr, [this] { change_motion([](auto &c) { c.speed /= 1.25f; }); }});
            rows.push_back({"FORCE +", nullptr, [this] { change_motion([](auto &c) { c.force *= 1.25f; }); }});
            rows.push_back({"FORCE -", nullptr, [this] { change_motion([](auto &c) { c.force /= 1.25f; }); }});
            rows.push_back({"HORIZON +", nullptr, [this] { change_motion([](auto &c) { c.horizon = std::min(64.f,c.horizon+1); }); }});
            rows.push_back({"HORIZON -", nullptr, [this] { change_motion([](auto &c) { c.horizon = std::max(1.f,c.horizon-1); }); }});
            rows.push_back({"RESET PARAMETERS", nullptr, [this] { change_motion([](auto &c) { c.speed=2; c.force=.6f; c.horizon=8; }); }});
            rows.push_back({"UPS +", nullptr, [this] { simulation_->updates_per_second(std::min(10000u,simulation_->updates_per_second()*2)); }});
            rows.push_back({"UPS -", nullptr, [this] { simulation_->updates_per_second(std::max(1u,simulation_->updates_per_second()/2)); }});
        }
        if (page_ != 0)
            rows.push_back({"PARKING TRAFFIC", nullptr, [this] { change_motion([](auto &c) {
                c.parking_traffic = !c.parking_traffic; }); },
                [this] { return simulation_->motion_settings().parking_traffic ? "ON" : "OFF"; }});
        if (page_ != 0)
            rows.push_back({"JUNCTION TRAFFIC", nullptr, [this] { change_motion([](auto &c) {
                c.junction_traffic = !c.junction_traffic; }); },
                [this] { return simulation_->motion_settings().junction_traffic ? "ON" : "OFF"; }});
        // Keep the motion panel above the statistics on short viewports.
        if (page_ == 0)
            rows.push_back({"CORRIDOR TRAFFIC", nullptr, [this] { change_motion([](auto &c) {
                c.corridor_traffic = !c.corridor_traffic; }); },
                [this] { return simulation_->motion_settings().corridor_traffic ? "ON" : "OFF"; }});
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
        loadscreen_.reset();
        try {
            files_ = discover_blueprints(directory_);
            if (files_.empty())
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
        return result;
    }
    void load(const std::filesystem::path &path) {
        try {
            auto next = std::make_unique<Simulation>(load_blueprint(path), seed_);
            simulation_ = std::move(next);
            simulation_->paused = !running_;
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
        camera.pixels_per_unit = std::max(camera.pixels_per_unit, 0.005f);
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
        : seed_(seed), running_(running), ups_(ups),
          directory_(directory) {
        if (ups < 1 || ups > 10000)
            throw std::invalid_argument("UPS must be in 1..10000");
        refresh_catalog();
        if (initial)
            load(*initial);
    }
    void advance(double elapsed, const Input &input, Camera2D &camera) override {
        if (!simulation_) {
            if (input.hit(Key::escape) || input.hit(Key::m)) {
                back_ = true;
                return;
            }
            if (loadscreen_) {
                if (const auto choice = loadscreen_->update(input, camera.viewport)) {
                    load(files_.at(choice->application));
                    return;
                }
                exit_ = loadscreen_->exit_requested();
            } else if (input.hit(Key::enter))
                refresh_catalog();
            return;
        }
        if (back_to_blueprints_) {
            simulation_.reset();
            back_to_blueprints_ = false;
            return;
        }
        if (error_remaining_ > 0) {
            error_remaining_ = std::max(0.0, error_remaining_ - elapsed);
            if (error_remaining_ == 0)
                error_.clear();
        }
        const bool modal = settings_.update(input, camera, statistics_, debug_, .005f, 10.f);
        if (modal && !modal_before_) {
            paused_before_settings_ = simulation_->paused;
            simulation_->paused = true;
        }
        if (!settings_.visible && modal_before_)
            simulation_->paused = paused_before_settings_;
        modal_before_ = settings_.visible;
        if (modal) { menu_.visible = false; return; }
        if (!debug_) menu_.visible = false;
        auto rows = options();
        const bool captured = debug_ && settings_.update_debug(input, camera, menu_, rows);
        if (!fitted_)
            fit(camera);
        if (input.hit(Key::p) || input.hit(Key::space))
            simulation_->paused = !simulation_->paused;
        if (input.hit(Key::r)) {
            const auto config = simulation_->motion_settings();
            simulation_ = std::make_unique<Simulation>(simulation_->blueprint(), seed_);
            simulation_->configure_motion(config);
            error_.clear();
            selected_mover_.reset();
            selected_producer_.reset();

        }
        if (input.hit(Key::g))
            simulation_->workload = !simulation_->workload;
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
        camera.pixels_per_unit =
            std::clamp(camera.pixels_per_unit * std::pow(1.1f, debug_ && !captured ? input.wheel : 0), 0.005f, 10.f);
        const float step = float(std::min(elapsed, 0.1)) * 400 / camera.pixels_per_unit;
        camera.position.x += (input.down(Key::d) || input.down(Key::right) ? step : 0) -
                             (input.down(Key::a) || input.down(Key::left) ? step : 0);
        camera.position.y += (input.down(Key::w) || input.down(Key::up) ? step : 0) -
                             (input.down(Key::s) || input.down(Key::down) ? step : 0);
        if (!captured && (input.clicked || input.right_clicked)) {
            const auto point = world(camera.screen_to_world(input.mouse));
            if (input.down(Key::shift) && input.clicked && selected_mover_)
                { if (!simulation_->move_to(*selected_mover_, point))
                    { error_ = "GOAL REJECTED: MOVER BUSY, BLOCKED OR GOAL UNREACHABLE";
                      error_remaining_ = 4; }
                  else error_.clear(); }
            else
                pick(point, input.right_clicked);
        }
        simulation_->advance(elapsed);
    }
    void render(Runtime &runtime) override {
        if (!simulation_) {
            if (loadscreen_)
                loadscreen_->draw(runtime, error_);
            if (!loadscreen_) {
                runtime.begin();
                factory_text(runtime, "I4SIM LOAD ERROR - ESC TO LAUNCHER", {12, 12}, 1, {1, 0.4f, 0.3f});
                factory_text(runtime, "ENTER RETRIES BLUEPRINT DISCOVERY", {12, 30}, 1, {1, 1, 1});
                const auto columns = std::size_t(std::max(1.f, (runtime.camera.viewport.x - 24) / 6));
                float y = 50;
                for (std::size_t offset = 0; offset < error_.size() && y + 8 < runtime.camera.viewport.y;
                     offset += columns, y += 12)
                    factory_text(runtime, std::string_view(error_).substr(offset, columns), {12, y}, 1,
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
        for (std::size_t i = 0; i < simulation_->movers().size(); ++i) {
            const auto &m = simulation_->movers()[i];
            const auto center = world(m.body.position);
            runtime.box(center, m.body.half_extents, 0,
                        (m.blocked || m.contacting) ? Color{1, 0, 0} : Color{.7f, .7f, .7f});
            if (selected_mover_ == i)
                runtime.circle(center, 6 / runtime.camera.pixels_per_unit, {1, 1, 0});
            if (debug_ && contacts_)
                draw_debug_box(runtime, center, m.body.half_extents, 0, {1, 0, 0});
            if (debug_ && detection_)
                draw_debug_circle(runtime, center, length(m.body.half_extents) +
                    simulation_->motion_settings().speed * simulation_->motion_settings().horizon,
                    {0.25f, 0.4f, 0.6f});
            if (debug_ && paths_) {
                Vec2 from = m.body.position;
                for (auto target : m.route) {
                    runtime.line(world(from), world(target), 2 / runtime.camera.pixels_per_unit,
                                 {0.7f, 0.4f, 1});
                    from = target;
                }
            }
            if (debug_ && vectors_) {
                draw_debug_vector(runtime, center, world(m.forces.border * 50), {1,.5f,0});
                draw_debug_vector(runtime, center, world(m.forces.collision * 50), {1,0,0});
                draw_debug_vector(runtime, center, world(m.forces.predictive * 50), {1,0,1});
                draw_debug_vector(runtime, center, world(m.forces.seek * 50), {0,1,0});
                runtime.line(center, world(m.body.position + m.body.velocity * 40),
                             3 / runtime.camera.pixels_per_unit, {0, 1, 1});
                runtime.line(center, world(m.body.position + m.forces.acceleration * 100),
                             2 / runtime.camera.pixels_per_unit, {1, 1, 0});
            }
        }
        if (debug_ && borders_ && simulation_->motion_settings().junction_traffic)
            for (std::size_t i = 0; i < simulation_->junction_count(); ++i) {
                draw_debug_box(runtime, world(simulation_->junction_center(i)),
                               b.cell_size / 2, 0, {0, 0.8f, 1});
                if (const auto exit = simulation_->junction_exit_reservation(i))
                    draw_debug_box(runtime, world((exit->min + exit->max) / 2),
                                   (exit->max - exit->min) / 2, 0, {0.6f, 1, 0});
            }
        if (debug_ && borders_ && simulation_->motion_settings().corridor_traffic)
            for (std::size_t i = 0; i < simulation_->corridor_count(); ++i) {
                const auto area = simulation_->corridor_bounds(i);
                draw_debug_box(runtime, world((area.min + area.max) / 2),
                               (area.max - area.min) / 2, 0, {0.2f, 0.5f, 1});
                if (const auto exit = simulation_->corridor_exit_reservation(i))
                    draw_debug_box(runtime, world((exit->min + exit->max) / 2),
                                   (exit->max - exit->min) / 2, 0, {0.6f, 1, 0});
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
            Vec2 label = anchor + Vec2{-width/2, -16};
            Aabb box{label - Vec2{3,3}, label + Vec2{width+3,10}};
            for (std::size_t attempt=0; attempt<=labels.size(); ++attempt) {
                bool overlap=false;
                for (const auto &other : labels) if (box.overlaps(other)) overlap=true;
                if (!overlap) break;
                label.y -= 14;
                box = {label-Vec2{3,3},label+Vec2{width+3,10}};
            }
            labels.push_back(box);
            runtime.line(runtime.camera.screen_to_world(anchor),
                         runtime.camera.screen_to_world(label+Vec2{width/2,7}),
                         1/runtime.camera.pixels_per_unit, {.7f,.7f,.7f});
            runtime.box(runtime.camera.screen_to_world(label+Vec2{width/2,4}),
                        Vec2{width/2+3,7}/runtime.camera.pixels_per_unit, 0, {.025f,.035f,.06f});
            factory_text(runtime,id,label,1,{1,1,1});
        };
        for (const auto &m : simulation_->movers()) name(m.definition.id,m.body.position);
        for (const auto &p : simulation_->producers()) name(p.definition.id,p.definition.position);
        if (statistics_) {
            std::ostringstream stats;
            stats << "TICK " << simulation_->ticks() << " UPS " << simulation_->updates_per_second()
                  << " CONTACTS " << simulation_->collisions() << " DURATION " << simulation_->contact_ticks();
            factory_text(runtime, stats.str(), {10, runtime.camera.viewport.y - 40}, 1.5f, {1, 1, 0});
            const auto &c = simulation_->motion_settings();
            std::ostringstream motion;
            motion << (simulation_->paused ? "PAUSED" : "RUNNING") << " SPEED " << c.speed
                   << " FORCE " << c.force << " HORIZON " << c.horizon;
            if (c.profile == SteeringProfile::reciprocal)
                motion << " AVOID LIMIT " << simulation_->avoidance_limited();
            if (c.parking_traffic)
                motion << " PARK WAIT " << simulation_->parking_waiting();
            if (c.junction_traffic)
                motion << " CROSS WAIT " << simulation_->junction_waiting() << "/" << simulation_->junction_count();
            if (c.corridor_traffic)
                motion << " LANE WAIT " << simulation_->corridor_waiting() << "/" << simulation_->corridor_count();
            if (c.junction_traffic || c.corridor_traffic)
                motion << " EXIT " << simulation_->junction_exit_waiting();
            factory_text(runtime, motion.str(), {10, runtime.camera.viewport.y - 20}, 1.5f, {1, 1, 0});
        }
        if (selected_mover_) {
            const auto &m = simulation_->movers()[*selected_mover_];
            std::ostringstream text;
            text << m.definition.id << " | DISTANCE " << int(m.distance) << " MM | CONTACT TICKS " << m.collision_ticks
                 << " | "
                 << (m.blocked       ? "BLOCKED"
                     : m.unreachable ? "UNREACHABLE"
                     : m.product     ? "ALLOCATED"
                                     : "IDLE - SHIFT CLICK GOAL");
            factory_text(runtime, text.str(), {10, runtime.camera.viewport.y - 80}, 1.5f, {1, 1, 0});
            std::ostringstream details;
            details << "SPEED " << length(m.body.velocity) << " FORCE " << length(m.body.acceleration)
                    << " GOAL " << int(m.destination.x) << "," << int(m.destination.y)
                    << (m.no_progress >= 120 ? " STALLED" : "");
            factory_text(runtime, details.str(), {10, runtime.camera.viewport.y - 60}, 1.5f, {1, 1, 0});
        }
        if (selected_producer_) {
            const auto &p = simulation_->producers()[*selected_producer_];
            const auto text = p.definition.id + " | QUEUE " + std::to_string(p.queue.size()) +
                              " | REMAINING " + std::to_string(p.remaining) + " | TASKS " +
                              std::to_string(p.interactions) + (p.blocked ? " | BLOCKED" : "");
            factory_text(runtime, text, {10, runtime.camera.viewport.y - 60}, 1.5f, {1, 1, 0});
        }
        if (!error_.empty())
            factory_text(runtime, error_, {12, 98}, 0.8f, {1, 0.4f, 0.3f});
        auto rows = options();
        menu_.draw(runtime, rows);
        settings_.draw(runtime, statistics_, debug_, false);
        runtime.end();
    }
    bool return_to_menu() const override { return back_ || settings_.return_requested; }
    bool exit_requested() const override { return exit_ || settings_.exit_requested; }
    std::string title() override {
        if (!simulation_)
            return error_.empty() ? "i4sim | Loadscreen" : "i4sim | Loadscreen | " + error_;
        return "i4sim | " + simulation_->blueprint().name +
               (simulation_->paused ? " | PAUSED" : " | RUNNING");
    }
};
} // namespace
std::unique_ptr<lab::Session> create_session(std::uint32_t seed, std::filesystem::path directory,
                                             std::optional<std::filesystem::path> initial, bool running,
                                             unsigned ups, std::optional<std::filesystem::path> output) {
    return std::make_unique<FactorySession>(seed, directory, initial, running, ups, std::move(output));
}
} // namespace i4sim
