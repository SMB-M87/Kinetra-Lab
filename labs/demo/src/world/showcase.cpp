#include "demo/world/showcase.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace demo {
namespace {
using kinetra::Vec2;

bool finite_vector(Vec2 v) {
    return std::isfinite(v.x) && std::isfinite(v.y);
}

void check_pause(float pause) {
    if (!std::isfinite(pause) || pause < 0)
        throw std::invalid_argument("Showcase pause must be finite and not negative");
}

float loop_length(const Lure &lure) {
    float length = 0;
    for (std::size_t i = 0; i < lure.loop.size(); ++i)
        length += kinetra::length(lure.loop[(i + 1) % lure.loop.size()] - lure.loop[i]);
    return length;
}
} // namespace

Showcase Showcase::shuttle(std::vector<std::vector<Vec2>> stops, Turn turn, float pause) {
    check_pause(pause);
    for (const auto &agent : stops)
        if (agent.size() < 2 || !std::ranges::all_of(agent, finite_vector))
            throw std::invalid_argument("Every shuttling agent needs two or more finite stops");
    Showcase showcase;
    showcase.kind_ = Kind::shuttle;
    showcase.turn_ = turn;
    showcase.pause_ = pause;
    for (auto &agent : stops)
        // Holding at the first stop for the whole pause already: the first update turns.
        showcase.agents_.push_back({std::move(agent), 0, pause, false});
    showcase.group_held_ = pause;
    return showcase;
}

Showcase Showcase::roam(std::size_t agents, kinetra::Aabb area, float pause, std::uint32_t seed) {
    check_pause(pause);
    if (!finite_vector(area.min) || !finite_vector(area.max) || !(area.min.x < area.max.x) ||
        !(area.min.y < area.max.y))
        throw std::invalid_argument("A roaming showcase needs a finite area that is not empty");
    Showcase showcase;
    showcase.kind_ = Kind::roam;
    showcase.pause_ = pause;
    showcase.area_ = area;
    showcase.random_.seed(seed);
    showcase.agents_.assign(agents, State{{}, 0, pause, false});
    return showcase;
}

Showcase Showcase::follow(std::vector<Lure> lures, std::vector<Slot> slots, bool rotate) {
    for (const auto &lure : lures)
        if (lure.loop.size() < 2 || !std::ranges::all_of(lure.loop, finite_vector) ||
            !std::isfinite(lure.speed) || lure.speed < 0 || !std::isfinite(lure.start) ||
            !(loop_length(lure) > 0))
            throw std::invalid_argument("Every lure needs a finite loop of some length and a finite speed");
    for (const auto &slot : slots)
        if (slot.lure >= lures.size() || !finite_vector(slot.offset))
            throw std::invalid_argument("Every slot needs one of the lures and a finite offset");
    Showcase showcase;
    showcase.kind_ = Kind::follow;
    showcase.agents_.assign(slots.size(), State{});
    showcase.lures_ = std::move(lures);
    showcase.slots_ = std::move(slots);
    showcase.rotate_ = rotate;
    return showcase;
}

Vec2 Showcase::lure_position(std::size_t index) const {
    const auto &lure = lures_.at(index);
    return along(lure, lure.start + elapsed_ * lure.speed).first;
}

std::optional<Vec2> Showcase::stop(std::size_t index) const {
    const State &state = agents_.at(index);
    if (kind_ != Kind::shuttle)
        return std::nullopt;
    return state.stops[state.stop];
}

std::pair<Vec2, Vec2> Showcase::along(const Lure &lure, float distance) {
    const float total = loop_length(lure);
    float rest = std::fmod(distance, total);
    if (rest < 0)
        rest += total;
    for (std::size_t i = 0;; i = (i + 1) % lure.loop.size()) {
        const Vec2 from = lure.loop[i], to = lure.loop[(i + 1) % lure.loop.size()];
        const float segment = kinetra::length(to - from);
        if (segment > 0 && rest <= segment) {
            const Vec2 direction = (to - from) / segment;
            return {from + direction * rest, direction};
        }
        rest -= segment;
    }
}

Vec2 Showcase::slot_target(std::size_t index) const {
    const Slot &slot = slots_.at(index);
    const Lure &lure = lures_[slot.lure];
    const float distance = lure.start + elapsed_ * lure.speed;
    const auto [position, direction] = along(lure, distance + follow_lead * lure.speed);
    const Vec2 offset = rotate_ ? Vec2{slot.offset.x * direction.x - slot.offset.y * direction.y,
                                       slot.offset.x * direction.y + slot.offset.y * direction.x}
                                : slot.offset;
    return position + offset;
}

void Showcase::send(std::size_t index, Agent &agent) {
    State &state = agents_[index];
    state.held = 0;
    switch (kind_) {
    case Kind::shuttle:
        state.stop = (state.stop + 1) % state.stops.size();
        agent.retarget(state.stops[state.stop]);
        break;
    case Kind::roam:
        agent.retarget({area_.min.x + unit() * (area_.max.x - area_.min.x),
                        area_.min.y + unit() * (area_.max.y - area_.min.y)});
        break;
    case Kind::follow:
        agent.retarget(slot_target(index));
        break;
    }
}

void Showcase::update(float dt, std::span<Agent> agents) {
    if (agents.size() != agents_.size())
        throw std::invalid_argument("A showcase needs exactly the agents it was made for");
    for (std::size_t i = 0; i < agents.size(); ++i) {
        State &state = agents_[i];
        if (agents[i].holding()) {
            // The user's goal is reached: the script owns the agent again.
            state.released = false;
            state.held += dt;
        } else {
            state.held = 0;
        }
    }
    switch (kind_) {
    case Kind::shuttle:
        if (turn_ == Turn::together) {
            const bool all_held = std::ranges::all_of(agents, &Agent::holding);
            group_held_ = all_held ? group_held_ + dt : 0;
            if (all_held && group_held_ >= pause_) {
                for (std::size_t i = 0; i < agents.size(); ++i)
                    send(i, agents[i]);
                group_held_ = 0;
            }
            break;
        }
        [[fallthrough]];
    case Kind::roam:
        for (std::size_t i = 0; i < agents.size(); ++i)
            if (agents[i].holding() && agents_[i].held >= pause_)
                send(i, agents[i]);
        break;
    case Kind::follow:
        elapsed_ += dt;
        until_command_ -= dt;
        if (until_command_ <= 0) {
            for (std::size_t i = 0; i < agents.size(); ++i)
                if (!agents_[i].released)
                    send(i, agents[i]);
            until_command_ = follow_interval;
        }
        break;
    }
}

void Showcase::release(std::size_t index) {
    agents_.at(index).released = true;
}
} // namespace demo
