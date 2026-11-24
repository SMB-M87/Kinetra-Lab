// Seeded statistics of Dotio's enemy behavior at each difficulty: what enemies do, how
// often they find and lose the player, whether their routes exist and whether they get
// stuck. A scripted player walks the maze and shoots at visible enemies, so pursuit,
// recovery and kamikaze happen as in a real game.
//
// The harness only reads the world and answers with ordinary commands. It plans the
// player's own walks with kinetra::WeightedGrid, so it does not depend on the enemy route
// planning it measures. Usage:
//
//   kinetra_dotio_behavior_benchmark [--seeds N] [--first-seed S] [--seconds T]
//                                    [--wander steering|path] [--per-seed]
#include "dotio/application.h"

#include <kinetra/simulation/weighted_grid.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
// --- Access to the world. The only part that follows changes of Dotio's interface. -----

constexpr int rows = dotio::Grid::rows, columns = dotio::Grid::columns;
constexpr float cell = 100; // Legacy pixels per grid cell.

bool wall(dotio::Application &application, int row, int column) {
    return application.world().grid().cell(row, column) != 0;
}

bool sight_blocked(dotio::Application &application, kinetra::Vec2 from, kinetra::Vec2 to) {
    kinetra::Vec2 hit{};
    return application.world().grid().is_intersecting_any_cell({from, to}, hit);
}

bool route_empty(const dotio::Enemy &enemy) {
    return enemy.path.empty();
}

// --- Scripted player. -----------------------------------------------------------------

/// Ticks per simulated second; Dotio's fixed update rate.
constexpr int ticks_per_second = 60;
/// Distance at which the player counts a waypoint as reached, in legacy pixels.
constexpr float waypoint_reach = 35;
/// Ticks without getting closer to the next waypoint before the player picks a new walk.
constexpr int player_patience_ticks = 90;
/// Farthest enemy the player shoots at, in legacy pixels: about the normal view.
constexpr float shooting_range = 900;

/// Walks to random free cells 6 to 16 cells away and shoots at the nearest enemy in sight.
/// Deterministic for a seed: its own generator, and the world's state.
class Player {
  public:
    explicit Player(std::uint32_t seed) : random_(seed ^ 0x504C4159u) {}

    dotio::Commands commands(dotio::Application &application) {
        auto &world = application.world();
        const auto position = world.get_player().position;
        const auto offset = world.world_offset();
        if (offset.x != offset_.x || offset.y != offset_.y || world.reset_count() != resets_) {
            route_.clear(); // The window moved or the player respawned: the route is stale.
            offset_ = offset;
            resets_ = world.reset_count();
        }
        if (next_ >= route_.size() || ++waiting_ > player_patience_ticks)
            plan(application, position);
        dotio::Commands commands;
        if (next_ < route_.size()) {
            auto to = route_[next_] - position;
            const float distance = kinetra::length(to);
            if (distance < best_) {
                best_ = distance;
                waiting_ = 0;
            }
            if (distance < waypoint_reach) {
                ++next_;
                best_ = std::numeric_limits<float>::infinity();
            } else {
                to = to / distance;
                commands.direction = {to.x, -to.y};
            }
        }
        if (const auto *target = nearest_visible_enemy(application, position)) {
            commands.aim = dotio::to_world(target->position);
            commands.shoot = true;
        }
        return commands;
    }

  private:
    void plan(dotio::Application &application, kinetra::Vec2 position) {
        route_.clear();
        next_ = 0;
        waiting_ = 0;
        best_ = std::numeric_limits<float>::infinity();
        kinetra::WeightedGrid grid({columns * cell, rows * cell}, {cell, cell});
        for (int row = 0; row < rows; ++row)
            for (int column = 0; column < columns; ++column)
                grid.block(row * columns + column, wall(application, row, column));
        const int row = int(position.y / cell), column = int(position.x / cell);
        for (int attempt = 0; attempt < 40 && route_.empty(); ++attempt) {
            const int distance = 6 + int(random_() % 11);
            const float angle = float(random_() % 3600) * 0.1f * 3.14159265f / 180.f;
            const int goal_row = row + int(std::lround(std::sin(angle) * float(distance)));
            const int goal_column = column + int(std::lround(std::cos(angle) * float(distance)));
            if (goal_row < 1 || goal_column < 1 || goal_row >= rows - 1 || goal_column >= columns - 1 ||
                wall(application, goal_row, goal_column))
                continue;
            for (const auto point :
                 grid.path(position, {(float(goal_column) + 0.5f) * cell, (float(goal_row) + 0.5f) * cell}))
                route_.push_back(point);
        }
    }

    const dotio::Enemy *nearest_visible_enemy(dotio::Application &application, kinetra::Vec2 position) {
        const dotio::Enemy *nearest = nullptr;
        float nearest_distance = shooting_range;
        for (const auto &enemy : application.world().get_enemies()) {
            if (!enemy.is_active || enemy.frozen)
                continue;
            const float distance = kinetra::length(enemy.position - position);
            if (distance < nearest_distance && !sight_blocked(application, position, enemy.position)) {
                nearest = &enemy;
                nearest_distance = distance;
            }
        }
        return nearest;
    }

    std::mt19937 random_;
    std::vector<kinetra::Vec2> route_;
    std::size_t next_{};
    int waiting_{};
    float best_{std::numeric_limits<float>::infinity()};
    kinetra::Vec2 offset_{};
    std::uint64_t resets_{};
};

// --- Observation. ---------------------------------------------------------------------

enum class State { wander, attack, pursue, recover, kamikaze };
constexpr int state_count = 5;
constexpr std::array<const char *, state_count> state_names{"wander", "attack", "pursue", "recover",
                                                            "kamikaze"};

State state_of(dotio::Enemy &enemy) {
    return enemy.is_attacking()    ? State::attack
           : enemy.is_pursueing()  ? State::pursue
           : enemy.is_recovering() ? State::recover
           : enemy.is_kamikazing() ? State::kamikaze
                                   : State::wander;
}

/// Ticks a moving enemy may stay within stuck_radius of one point before it counts as stuck.
constexpr int stuck_ticks = 2 * ticks_per_second;
/// Distance an enemy must leave its anchor point by to count as moving, in legacy pixels.
constexpr float stuck_radius = 25;
/// Depth an enemy's body may reach into a wall cell before the tick counts as an overlap, in
/// legacy pixels. Contact pushes a body out after each move, so a small depth is normal.
constexpr float overlap_tolerance = 2;

/// Sums over every tick of every run; rates are formed when printing.
struct Totals {
    double ticks{}, enemy_ticks{}, resident_ticks{};
    std::array<double, state_count> state_ticks{};
    double spawns{}, kills{}, contacts{}, kamikaze_contacts{}, retired{};
    double damage{}, deaths{}, arrows{};
    double sightings{}, pursuits{}, pursuit_found{}, pursuit_gave_up{}, pursuit_ticks{};
    double recoveries{}, recover_to_pursue{}, recover_to_kamikaze{}, kamikazes{};
    double following_ticks{}, routeless_ticks{};
    double stuck{}, overlap_ticks{};
    double attack_distance{}, attack_distance_ticks{};
    std::uint64_t fingerprint{1469598103934665603ull};

    Totals &operator+=(const Totals &other) {
        ticks += other.ticks;
        enemy_ticks += other.enemy_ticks;
        resident_ticks += other.resident_ticks;
        for (int i = 0; i < state_count; ++i)
            state_ticks[i] += other.state_ticks[i];
        spawns += other.spawns;
        kills += other.kills;
        contacts += other.contacts;
        kamikaze_contacts += other.kamikaze_contacts;
        retired += other.retired;
        damage += other.damage;
        deaths += other.deaths;
        arrows += other.arrows;
        sightings += other.sightings;
        pursuits += other.pursuits;
        pursuit_found += other.pursuit_found;
        pursuit_gave_up += other.pursuit_gave_up;
        pursuit_ticks += other.pursuit_ticks;
        recoveries += other.recoveries;
        recover_to_pursue += other.recover_to_pursue;
        recover_to_kamikaze += other.recover_to_kamikaze;
        kamikazes += other.kamikazes;
        following_ticks += other.following_ticks;
        routeless_ticks += other.routeless_ticks;
        stuck += other.stuck;
        overlap_ticks += other.overlap_ticks;
        attack_distance += other.attack_distance;
        attack_distance_ticks += other.attack_distance_ticks;
        fingerprint = fingerprint * 31 + other.fingerprint;
        return *this;
    }
};

/// What the observer remembers about one enemy slot between two ticks.
struct Slot {
    bool active{};
    State state{State::wander};
    kinetra::Vec2 position{}, anchor{};
    int anchor_tick{}, pursuit_start{};
    bool stuck{};
};

/// Deepest overlap of `enemy`'s body with a wall cell, in legacy pixels; zero when clear.
float wall_overlap(dotio::Application &application, const dotio::Enemy &enemy) {
    float deepest = 0;
    const int row = int(enemy.position.y / cell), column = int(enemy.position.x / cell);
    for (int y = std::max(0, row - 1); y <= std::min(rows - 1, row + 1); ++y)
        for (int x = std::max(0, column - 1); x <= std::min(columns - 1, column + 1); ++x) {
            if (!wall(application, y, x))
                continue;
            const float nearest_x = std::clamp(enemy.position.x, float(x) * cell, float(x + 1) * cell);
            const float nearest_y = std::clamp(enemy.position.y, float(y) * cell, float(y + 1) * cell);
            const float distance = std::hypot(enemy.position.x - nearest_x, enemy.position.y - nearest_y);
            deepest = std::max(deepest, enemy.radius - distance);
        }
    return deepest;
}

void mix(std::uint64_t &hash, float value) {
    hash = (hash ^ std::uint64_t(std::int64_t(std::lround(double(value) * 16)))) * 1099511628211ull;
}

/// Plays `seconds` simulated seconds of a world at `difficulty` and returns its totals.
Totals play(std::uint32_t seed, dotio::Difficulty difficulty, double seconds, dotio::WanderMode wander_mode) {
    auto application = std::make_unique<dotio::Application>(seed, difficulty);
    auto &world = application->world();
    world.enemy_tuning().wander = wander_mode;
    kinetra::Camera2D camera;
    camera.viewport = {1280, 720};
    camera.pixels_per_unit = dotio::initial_zoom;
    Player player(seed);
    Totals totals;
    std::vector<Slot> slots(world.get_enemies().get_capacity());
    std::vector<bool> arrows(world.get_arrows().get_capacity());
    const int ticks = int(seconds * ticks_per_second);
    for (int tick = 0; tick < ticks; ++tick) {
        const auto commands = player.commands(*application);
        const float health = world.get_player().health;
        const int kills = world.get_player().kills;
        const auto resets = world.reset_count();
        const auto offset_before = world.world_offset();
        const auto player_before = world.get_player().position;
        application->advance(1.0 / ticks_per_second, commands, camera);
        const bool reset = world.reset_count() != resets;
        const auto shift = world.world_offset() - offset_before;
        auto &self = world.get_player();
        totals.ticks += 1;
        totals.deaths += reset;
        if (!reset) {
            totals.damage += std::max(0.f, health - self.health);
            totals.kills += std::max(0, self.kills - kills);
        }
        std::size_t index = 0;
        for (const auto &arrow : world.get_arrows()) {
            totals.arrows += arrow.is_active && !arrows[index];
            arrows[index++] = arrow.is_active;
        }
        index = 0;
        for (auto &enemy : world.get_enemies()) {
            auto &slot = slots[index++];
            slot.position -= shift;
            slot.anchor -= shift;
            if (!enemy.is_active) {
                if (slot.active && !reset) {
                    const float gap = kinetra::length(slot.position - (player_before - shift));
                    if (gap < 2 * enemy.radius + 12) {
                        ++totals.contacts;
                        totals.kamikaze_contacts += slot.state == State::kamikaze;
                    } else if (self.kills > kills) {
                        // Counted in kills above.
                    } else {
                        ++totals.retired;
                    }
                }
                slot.active = false;
                continue;
            }
            const State state = state_of(enemy);
            if (!slot.active) {
                ++totals.spawns;
                slot = {true, state, enemy.position, enemy.position, tick, 0, false};
                continue;
            }
            ++totals.resident_ticks;
            if (enemy.frozen) {
                slot.state = state;
                slot.position = slot.anchor = enemy.position;
                slot.anchor_tick = tick;
                continue;
            }
            ++totals.enemy_ticks;
            ++totals.state_ticks[static_cast<int>(state)];
            if (state != slot.state) {
                totals.sightings += slot.state == State::wander && state == State::attack;
                if (state == State::pursue) {
                    ++totals.pursuits;
                    slot.pursuit_start = tick;
                }
                if (slot.state == State::pursue) {
                    totals.pursuit_found += state == State::attack;
                    totals.pursuit_gave_up += state == State::wander;
                    totals.pursuit_ticks += tick - slot.pursuit_start;
                }
                totals.recoveries += state == State::recover;
                totals.recover_to_pursue += slot.state == State::recover && state == State::pursue;
                totals.recover_to_kamikaze += slot.state == State::recover && state == State::kamikaze;
                totals.kamikazes += state == State::kamikaze;
                slot.anchor = enemy.position;
                slot.anchor_tick = tick;
                slot.stuck = false;
            }
            const bool follows_route = state == State::pursue || state == State::recover ||
                                       (state == State::kamikaze && !enemy.line_of_sight);
            if (follows_route) {
                ++totals.following_ticks;
                totals.routeless_ticks += route_empty(enemy);
            }
            if (state == State::attack) {
                totals.attack_distance += kinetra::length(enemy.position - self.position);
                ++totals.attack_distance_ticks;
            }
            if (kinetra::length(enemy.position - slot.anchor) > stuck_radius) {
                slot.anchor = enemy.position;
                slot.anchor_tick = tick;
                slot.stuck = false;
            } else if (state != State::attack && !slot.stuck && tick - slot.anchor_tick >= stuck_ticks) {
                slot.stuck = true;
                ++totals.stuck;
            }
            totals.overlap_ticks += wall_overlap(*application, enemy) > overlap_tolerance;
            slot.state = state;
            slot.position = enemy.position;
            if (tick % ticks_per_second == 0) {
                mix(totals.fingerprint, enemy.position.x);
                mix(totals.fingerprint, enemy.position.y);
            }
        }
        if (tick % ticks_per_second == 0) {
            mix(totals.fingerprint, self.position.x);
            mix(totals.fingerprint, self.position.y);
        }
    }
    return totals;
}

// --- Report. --------------------------------------------------------------------------

struct Options {
    int seeds{12};
    std::uint32_t first_seed{1};
    double seconds{180};
    bool per_seed{};
    dotio::WanderMode wander{dotio::WanderMode::steering};
};

Options parse(int argc, char **argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string option = argv[i];
        const bool has_value = i + 1 < argc;
        if (option == "--seeds" && has_value)
            options.seeds = std::stoi(argv[++i]);
        else if (option == "--first-seed" && has_value)
            options.first_seed = std::uint32_t(std::stoul(argv[++i]));
        else if (option == "--seconds" && has_value)
            options.seconds = std::stod(argv[++i]);
        else if (option == "--wander" && has_value) {
            const std::string mode = argv[++i];
            if (mode != "steering" && mode != "path")
                throw std::invalid_argument("--wander must be steering or path");
            options.wander = mode == "path" ? dotio::WanderMode::path : dotio::WanderMode::steering;
        } else if (option == "--per-seed")
            options.per_seed = true;
        else
            throw std::invalid_argument(
                "Usage: kinetra_dotio_behavior_benchmark [--seeds N] [--first-seed S] "
                "[--seconds T] [--wander steering|path] [--per-seed]");
    }
    if (options.seeds <= 0 || !(options.seconds > 0))
        throw std::invalid_argument("Seeds and seconds must be positive");
    return options;
}

double ratio(double part, double whole) {
    return whole > 0 ? part / whole : 0;
}

void row(const std::string &label, const std::array<Totals, 3> &totals,
         const std::function<double(const Totals &)> &value, const char *format = "%.2f") {
    std::printf("| %s |", label.c_str());
    for (const auto &total : totals) {
        std::printf(" ");
        std::printf(format, value(total));
        std::printf(" |");
    }
    std::printf("\n");
}

void report(const std::array<Totals, 3> &totals) {
    std::printf("| Statistic | Easy | Medium | Hard |\n| --- | --- | --- | --- |\n");
    // Rates per minute of simulated time, or per enemy-minute: one simulated enemy for 60 s.
    row("Simulated enemies (mean)", totals, [](const Totals &t) { return ratio(t.enemy_ticks, t.ticks); });
    row("Resident enemies (mean)", totals, [](const Totals &t) { return ratio(t.resident_ticks, t.ticks); });
    row("Spawns / min", totals, [](const Totals &t) { return ratio(t.spawns, t.ticks / 3600); });
    row("Player kills / min", totals, [](const Totals &t) { return ratio(t.kills, t.ticks / 3600); });
    row("Contacts with the player / min", totals,
        [](const Totals &t) { return ratio(t.contacts, t.ticks / 3600); });
    row("Kamikaze contacts / min", totals,
        [](const Totals &t) { return ratio(t.kamikaze_contacts, t.ticks / 3600); });
    row(
        "Damage to player / min", totals, [](const Totals &t) { return ratio(t.damage, t.ticks / 3600); },
        "%.1f");
    row("Player deaths / 10 min", totals, [](const Totals &t) { return ratio(t.deaths, t.ticks / 36000); });
    row("Arrows / enemy-min", totals, [](const Totals &t) { return ratio(t.arrows, t.enemy_ticks / 3600); });
    for (int state = 0; state < state_count; ++state)
        row(
            std::string("Time in ") + state_names[state] + " (%)", totals,
            [state](const Totals &t) { return 100 * ratio(t.state_ticks[state], t.enemy_ticks); }, "%.1f");
    row("Sightings from wander / enemy-min", totals,
        [](const Totals &t) { return ratio(t.sightings, t.enemy_ticks / 3600); });
    row("Pursuits / enemy-min", totals,
        [](const Totals &t) { return ratio(t.pursuits, t.enemy_ticks / 3600); });
    row(
        "Pursuits that find the player (%)", totals,
        [](const Totals &t) { return 100 * ratio(t.pursuit_found, t.pursuit_found + t.pursuit_gave_up); },
        "%.1f");
    row("Pursuit duration (s)", totals,
        [](const Totals &t) { return ratio(t.pursuit_ticks / 60, t.pursuit_found + t.pursuit_gave_up); });
    row("Recoveries / enemy-min", totals,
        [](const Totals &t) { return ratio(t.recoveries, t.enemy_ticks / 3600); });
    row(
        "Recoveries that reload (%)", totals,
        [](const Totals &t) { return 100 * ratio(t.recover_to_pursue, t.recoveries); }, "%.1f");
    row("Kamikaze entries / enemy-min", totals,
        [](const Totals &t) { return ratio(t.kamikazes, t.enemy_ticks / 3600); });
    row(
        "Route-following ticks without a route (%)", totals,
        [](const Totals &t) { return 100 * ratio(t.routeless_ticks, t.following_ticks); }, "%.1f");
    row(
        "Stuck episodes / enemy-min", totals,
        [](const Totals &t) { return ratio(t.stuck, t.enemy_ticks / 3600); }, "%.3f");
    row(
        "Ticks overlapping a wall (%)", totals,
        [](const Totals &t) { return 100 * ratio(t.overlap_ticks, t.enemy_ticks); }, "%.2f");
    row("Attack distance (cells)", totals,
        [](const Totals &t) { return ratio(t.attack_distance / cell, t.attack_distance_ticks); });
    std::printf("\nFingerprint (same seeds give the same value only if runs are reproducible):");
    for (const auto &total : totals)
        std::printf(" %016llx", static_cast<unsigned long long>(total.fingerprint));
    std::printf("\n");
}
} // namespace

int main(int argc, char **argv) {
    try {
        const auto options = parse(argc, argv);
        std::printf("Dotio behavior: %d seeds from %u, %.0f simulated seconds each, %s wander\n\n",
                    options.seeds, options.first_seed, options.seconds,
                    options.wander == dotio::WanderMode::path ? "path" : "steering");
        std::array<Totals, 3> totals;
        const std::array difficulties{dotio::Difficulty::easy, dotio::Difficulty::medium,
                                      dotio::Difficulty::hard};
        for (std::size_t d = 0; d < difficulties.size(); ++d)
            for (int i = 0; i < options.seeds; ++i) {
                const auto seed = options.first_seed + std::uint32_t(i);
                const auto run = play(seed, difficulties[d], options.seconds, options.wander);
                if (options.per_seed)
                    std::printf("difficulty %zu seed %u: kills %.0f damage %.0f deaths %.0f pursuits %.0f "
                                "stuck %.0f fingerprint %016llx\n",
                                d, seed, run.kills, run.damage, run.deaths, run.pursuits, run.stuck,
                                static_cast<unsigned long long>(run.fingerprint));
                totals[d] += run;
            }
        if (options.per_seed)
            std::printf("\n");
        report(totals);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
