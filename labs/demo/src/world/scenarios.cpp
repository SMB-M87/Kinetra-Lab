#include "demo/world/scene.h"

#include <array>
#include <cmath>
#include <optional>
#include <random>
#include <stdexcept>
#include <utility>

namespace demo {
namespace {
using kinetra::Body;
using kinetra::Motion;
using kinetra::Shape;
using kinetra::Vec2;

Body box(Vec2 position, Vec2 half_extents, Motion motion = Motion::fixed) {
    Body body;
    body.shape = Shape::box;
    body.motion = motion;
    body.position = position;
    body.half_extents = half_extents;
    return body;
}

Body pillar(Vec2 position, float radius) {
    Body body;
    body.motion = Motion::fixed;
    body.position = position;
    body.radius = radius;
    return body;
}

Body player_at(Vec2 position) {
    auto player = box(position, {0.65f, 0.4f}, Motion::controlled);
    player.friction = 0;
    return player;
}

void add_gravity_props(std::vector<Body> &movers, std::vector<Body> &walls) {
    // Slanted platforms redirect the falling shapes into different contacts.
    auto left = box({-8, 1}, {5, 0.5f});
    left.angle = -0.22f;
    auto right = box({8, -3}, {5, 0.5f});
    right.angle = 0.22f;
    walls.insert(walls.end(), {left, right});
    for (int i = 0; i < 6; ++i) {
        const Vec2 position{-12.f + 4.8f * static_cast<float>(i), 6.f + static_cast<float>(i % 3)};
        Body prop = box(position, {0.8f, 0.65f}, Motion::dynamic);
        if (i % 3 == 0) {
            prop.shape = Shape::circle;
            prop.radius = 0.85f;
        } else if (i % 3 == 2) {
            prop = kinetra::regular_polygon(5, 0.95f);
            prop.position = position;
        }
        prop.restitution = 0.65f;
        prop.angular_velocity = 0.5f;
        movers.push_back(prop);
    }
}

/// What one authored scenario places besides the frame: walls, the player, the agents'
/// homes and size, and the script that moves them.
struct Layout {
    std::vector<Body> walls;
    Vec2 player{-20, -12};
    std::vector<Vec2> homes;
    float radius{0.55f};
    std::optional<Showcase> showcase;
};

/// Two stops per agent: its home and the home mirrored by `mirror`. Mirrors swap the rows of a
/// group, so the agent in front, which arrives first, holds at the far end, out of the way.
template <typename Mirror>
std::vector<std::vector<Vec2>> there_and_back(const Layout &layout, Mirror mirror) {
    std::vector<std::vector<Vec2>> stops;
    for (const auto home : layout.homes)
        stops.push_back({home, mirror(home)});
    return stops;
}

/// Maze cells: 11 columns and 7 rows of 4 units fill the authored field.
constexpr int maze_columns = 11, maze_rows = 7;
constexpr float maze_cell = 4, maze_wall = 0.5f;

Vec2 maze_center(int column, int row) {
    return {-20 + maze_cell * float(column), -12 + maze_cell * float(row)};
}

/// A labyrinth from a constant seed, so the layout stays fixed: a depth-first maze, which
/// links every cell by one path, plus openings that add loops, so agents meet from both
/// sides and a blocked corridor has a way around. Walls in one line merge into one box.
Layout maze() {
    // east[c][r]: wall between (c, r) and (c + 1, r); north[c][r]: between (c, r) and (c, r + 1).
    std::array<std::array<bool, maze_rows>, maze_columns> east{}, north{}, visited{};
    for (auto &column : east)
        column.fill(true);
    for (auto &column : north)
        column.fill(true);
    std::mt19937 random(20261127);
    std::vector<std::pair<int, int>> stack{{0, 0}};
    visited[0][0] = true;
    while (!stack.empty()) {
        const auto [c, r] = stack.back();
        std::array<std::pair<int, int>, 4> options;
        std::size_t count = 0;
        for (const auto &[dc, dr] : {std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}}) {
            const int nc = c + dc, nr = r + dr;
            if (nc >= 0 && nc < maze_columns && nr >= 0 && nr < maze_rows && !visited[nc][nr])
                options[count++] = {nc, nr};
        }
        if (count == 0) {
            stack.pop_back();
            continue;
        }
        const auto [nc, nr] = options[random() % count];
        if (nc != c)
            east[std::min(c, nc)][r] = false;
        else
            north[c][std::min(r, nr)] = false;
        visited[nc][nr] = true;
        stack.emplace_back(nc, nr);
    }
    for (int opened = 0; opened < 5;) {
        const int c = int(random() % maze_columns), r = int(random() % maze_rows);
        if (random() % 2 == 0 && c + 1 < maze_columns && east[c][r]) {
            east[c][r] = false;
            ++opened;
        } else if (r + 1 < maze_rows && north[c][r]) {
            north[c][r] = false;
            ++opened;
        }
    }
    Layout layout;
    const auto line = [&](Vec2 from, Vec2 to) {
        // Each run of walls overlaps its neighbors' thickness, so corners close.
        const Vec2 center = (from + to) * 0.5f;
        const Vec2 half{std::abs(to.x - from.x) / 2 + maze_wall, std::abs(to.y - from.y) / 2 + maze_wall};
        layout.walls.push_back(box(center, half));
    };
    for (int c = 0; c + 1 < maze_columns; ++c)
        for (int r = 0; r < maze_rows;) {
            if (!east[c][r]) {
                ++r;
                continue;
            }
            const int first = r;
            while (r < maze_rows && east[c][r])
                ++r;
            const float x = -22 + maze_cell * float(c + 1);
            line({x, -14 + maze_cell * float(first)}, {x, -14 + maze_cell * float(r)});
        }
    for (int r = 0; r + 1 < maze_rows; ++r)
        for (int c = 0; c < maze_columns;) {
            if (!north[c][r]) {
                ++c;
                continue;
            }
            const int first = c;
            while (c < maze_columns && north[c][r])
                ++c;
            const float y = -14 + maze_cell * float(r + 1);
            line({-22 + maze_cell * float(first), y}, {-22 + maze_cell * float(c), y});
        }
    // Six agents between cells far apart, three starting on each side, so their routes cross
    // the maze and meet in it. Each route is at most 56 units, which an agent walks well
    // within its 20 seconds of commanded travel.
    const std::array<std::pair<Vec2, Vec2>, 6> trips{
        std::pair{maze_center(0, 6), maze_center(7, 1)},  std::pair{maze_center(0, 1), maze_center(6, 6)},
        std::pair{maze_center(1, 6), maze_center(9, 4)},  std::pair{maze_center(10, 2), maze_center(2, 4)},
        std::pair{maze_center(10, 1), maze_center(3, 6)}, std::pair{maze_center(10, 0), maze_center(4, 4)}};
    std::vector<std::vector<Vec2>> stops;
    for (const auto &[from, to] : trips) {
        layout.homes.push_back(from);
        stops.push_back({from, to});
    }
    layout.showcase = Showcase::shuttle(std::move(stops), Turn::alone, 1);
    return layout;
}

Layout doorway() {
    Layout layout;
    layout.player = {-17, -9};
    layout.walls = {box({0, -8.5f}, {0.7f, 5.5f}), box({0, 8.5f}, {0.7f, 5.5f})};
    for (const float side : {-1.f, 1.f})
        for (int row = 0; row < 3; ++row)
            layout.homes.push_back({side * (9.f + 2.f * float(row)), 1.8f * float(row - 1)});
    layout.showcase = Showcase::shuttle(
        there_and_back(layout,
                       [](Vec2 home) { return Vec2{std::copysign(22 - std::abs(home.x), -home.x), home.y}; }),
        Turn::together, 2);
    return layout;
}

/// Two groups of six swap ends through a corridor 7 units wide and 28 long.
Layout corridor() {
    Layout layout;
    layout.walls = {box({0, 9}, {14, 5.5f}), box({0, -9}, {14, 5.5f})};
    for (const float side : {-1.f, 1.f})
        for (const float x : {17.f, 20.f})
            for (const float y : {-2.4f, 0.f, 2.4f})
                layout.homes.push_back({side * x, y});
    layout.showcase = Showcase::shuttle(
        there_and_back(layout,
                       [](Vec2 home) { return Vec2{std::copysign(37 - std::abs(home.x), -home.x), home.y}; }),
        Turn::together, 2);
    return layout;
}

/// Two streets 9 units wide cross between four blocks; one group per street drives across.
Layout crossing() {
    Layout layout;
    layout.player = {-21, -13};
    for (const float x : {-1.f, 1.f})
        for (const float y : {-1.f, 1.f})
            layout.walls.push_back(box({x * 12.25f, y * 8.25f}, {7.75f, 3.75f}));
    for (const float x : {-19.f, -16.f})
        for (const float y : {-2.5f, 0.f, 2.5f})
            layout.homes.push_back({x, y});
    for (const float y : {-12.f, -9.f})
        for (const float x : {-2.5f, 0.f, 2.5f})
            layout.homes.push_back({x, y});
    layout.showcase = Showcase::shuttle(there_and_back(layout,
                                                       [](Vec2 home) {
                                                           return home.y < -6 ? Vec2{home.x, 21 + home.y}
                                                                              : Vec2{35 + home.x, home.y};
                                                       }),
                                        Turn::together, 2);
    return layout;
}

/// Sixteen agents on a circle each cross to the opposite point, so all meet in the middle.
Layout circle() {
    Layout layout;
    constexpr int agents = 16;
    for (int i = 0; i < agents; ++i)
        layout.homes.push_back(kinetra::rotated({11, 0}, 6.283185307f * float(i) / agents));
    layout.showcase =
        Showcase::shuttle(there_and_back(layout, [](Vec2 home) { return home * -1.f; }), Turn::together, 2);
    return layout;
}

/// Sixteen agents leave a room through one exit 2.4 units wide, and come back.
Layout bottleneck() {
    Layout layout;
    layout.radius = 0.45f;
    layout.walls = {box({-6, -7.6f}, {0.5f, 6.4f}), box({-6, 7.6f}, {0.5f, 6.4f})};
    for (const float x : {-19.f, -16.5f, -14.f, -11.5f})
        for (const float y : {-4.5f, -1.5f, 1.5f, 4.5f})
            layout.homes.push_back({x, y});
    layout.showcase = Showcase::shuttle(
        there_and_back(layout, [](Vec2 home) { return Vec2{home.x + 27, home.y}; }), Turn::together, 2);
    return layout;
}

/// Forty agents travel to random points between five pillars, each on its own.
Layout chaos(std::uint32_t seed) {
    Layout layout;
    layout.player = {-20.5f, -12.8f};
    layout.radius = 0.45f;
    for (const Vec2 position : {Vec2{-8, 6}, Vec2{8, -6}, Vec2{0, 2}, Vec2{-12, -2}, Vec2{12, 2}})
        layout.walls.push_back(pillar(position, 1.2f));
    for (int column = 0; column < 8; ++column)
        for (int row = 0; row < 5; ++row)
            layout.homes.push_back({-14 + 4 * float(column), -8 + 4 * float(row)});
    layout.showcase = Showcase::roam(layout.homes.size(), {{-19, -11}, {19, 11}}, 0.5f, seed);
    return layout;
}

/// Points of the closed curve `point` at `count` even steps of its parameter, from `start`.
template <typename Curve> std::vector<Vec2> sampled(Curve point, int count, float start = 0) {
    std::vector<Vec2> loop;
    for (int i = 0; i < count; ++i)
        loop.push_back(point(start + 6.283185307f * float(i) / float(count)));
    return loop;
}

/// Places the agents where a follow script first sends them.
void home_at_slots(Layout &layout, std::size_t agents) {
    for (std::size_t i = 0; i < agents; ++i)
        layout.homes.push_back(layout.showcase->slot_target(i));
}

/// Two arrow formations of six travel one oval in opposite directions, meet head-on twice
/// per lap and pass two pillars on the way.
Layout fleet() {
    Layout layout;
    const auto oval = sampled([](float t) { return Vec2{15 * std::cos(t), 7.5f * std::sin(t)}; }, 48);
    auto reversed = std::vector(oval.rbegin(), oval.rend());
    const Lure first{oval, 2.5f, 0};
    // Started half a lap on, the second fleet meets the first at the top and the bottom.
    Lure second{std::move(reversed), 2.5f, 0};
    float lap = 0;
    for (std::size_t i = 0; i < second.loop.size(); ++i)
        lap += kinetra::length(second.loop[(i + 1) % second.loop.size()] - second.loop[i]);
    second.start = lap / 2;
    for (const auto angle : {0.785398f, 3.926991f})
        layout.walls.push_back(pillar({15 * std::cos(angle), 7.5f * std::sin(angle)}, 1));
    const std::array<Vec2, 6> arrow{Vec2{0, 0},        Vec2{-1.8f, 1.6f}, Vec2{-1.8f, -1.6f},
                                    Vec2{-3.6f, 3.2f}, Vec2{-3.6f, 0},    Vec2{-3.6f, -3.2f}};
    std::vector<Slot> slots;
    for (std::size_t lure = 0; lure < 2; ++lure)
        for (const auto offset : arrow)
            slots.push_back({lure, offset});
    layout.showcase = Showcase::follow({first, second}, std::move(slots), true);
    home_at_slots(layout, 12);
    return layout;
}

/// Thirty agents swarm around one lure on a figure eight, whose crossing holds a pillar.
Layout swarm() {
    Layout layout;
    layout.radius = 0.45f;
    layout.walls.push_back(pillar({0, 0}, 1.5f));
    // Starting a quarter turn in puts the lure at the right end of the eight, clear of the pillar.
    const auto eight =
        sampled([](float t) { return Vec2{15 * std::sin(t), 8 * std::sin(2 * t)}; }, 64, 1.5707963f);
    std::vector<Slot> slots;
    constexpr int agents = 30;
    for (int i = 0; i < agents; ++i)
        // A sunflower spiral packs the slots evenly, about 1.06 units apart.
        slots.push_back({0, kinetra::rotated({0.6f * std::sqrt(float(i)), 0}, 2.3999632f * float(i))});
    layout.showcase = Showcase::follow({Lure{eight, 3, 0}}, std::move(slots), false);
    home_at_slots(layout, agents);
    return layout;
}

Layout layout_of(Scenario scenario, std::uint32_t seed) {
    switch (scenario) {
    case Scenario::gravity: {
        Layout layout;
        layout.player = {-17, -9};
        return layout;
    }
    case Scenario::maze:
        return maze();
    case Scenario::doorway:
        return doorway();
    case Scenario::corridor:
        return corridor();
    case Scenario::crossing:
        return crossing();
    case Scenario::circle:
        return circle();
    case Scenario::bottleneck:
        return bottleneck();
    case Scenario::chaos:
        return chaos(seed);
    case Scenario::fleet:
        return fleet();
    case Scenario::swarm:
        return swarm();
    case Scenario::terrain:
        break;
    }
    throw std::invalid_argument("Unknown fixed Demo scenario");
}
} // namespace

void Scene::arrange_scenario(std::uint32_t seed) {
    auto layout = layout_of(scenario_, seed);
    // The frame lies outside the playable rectangle, leaving its full width for routing.
    std::vector walls{box({0, -14.5f}, {23, 0.5f}), box({0, 14.5f}, {23, 0.5f}), box({-22.5f, 0}, {0.5f, 14}),
                      box({22.5f, 0}, {0.5f, 14})};
    walls.insert(walls.end(), layout.walls.begin(), layout.walls.end());
    std::vector movers{player_at(layout.player)};
    if (scenario_ == Scenario::gravity)
        add_gravity_props(movers, walls);
    for (const auto home : layout.homes) {
        Body mover;
        mover.motion = Motion::controlled;
        mover.position = home;
        mover.radius = layout.radius;
        mover.friction = 0;
        agents_.emplace_back(movers.size(), home, seed);
        movers.push_back(mover);
    }
    arrange(std::move(movers), 0, std::move(walls), scenario_area);
    for (auto &agent : agents_)
        agent.update_visibility(scenario_area, world_.body(agent.body_id()));
    // The showcase sends the agents on their first legs at once, even before the first
    // camera update.
    if (layout.showcase)
        set_showcase(std::move(*layout.showcase));
}
} // namespace demo
