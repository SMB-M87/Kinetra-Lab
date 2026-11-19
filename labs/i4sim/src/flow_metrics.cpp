#include "flow_metrics.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace i4sim {
namespace {
bool positive(float value) {
    return std::isfinite(value) && value > 0;
}
} // namespace

FlowMetrics::FlowMetrics(FlowLimits limits) : limits_(limits) {
    if (!positive(limits.waiting_speed) || !positive(limits.arrival_distance) ||
        !positive(limits.progress_distance) || limits.stuck_ticks == 0)
        throw std::invalid_argument("Invalid flow limits");
}

void FlowMetrics::observe(const Simulation &simulation) {
    const auto &movers = simulation.movers();
    if (!started_) {
        tracks_.resize(movers.size());
        started_ = true;
    } else if (tracks_.size() != movers.size()) {
        throw std::invalid_argument("FlowMetrics observes one simulation");
    }
    ++observations_;
    for (std::size_t i = 0; i < movers.size(); ++i) {
        const auto &mover = movers[i];
        auto &track = tracks_[i];
        if (!mover.moving || mover.blocked) {
            track.active = track.stuck = false;
            continue;
        }
        const float remaining = kinetra::distance(mover.body.position, mover.destination);
        const bool arriving = remaining <= limits_.arrival_distance;
        if (kinetra::length(mover.body.velocity) < limits_.waiting_speed && !arriving)
            ++waiting_ticks_;
        // Progress is measured against the best distance so far, not the latest one, so
        // moving back and forth without getting closer counts as no progress. A new goal
        // or the final approach starts a fresh measurement.
        if (!track.active || kinetra::distance(track.destination, mover.destination) > 0 || arriving ||
            remaining < track.best - limits_.progress_distance) {
            track.destination = mover.destination;
            track.best = remaining;
            track.since = observations_;
            track.active = true;
        }
        track.stuck = observations_ - track.since + 1 >= limits_.stuck_ticks;
        track.ever_stuck = track.ever_stuck || track.stuck;
    }
}

std::size_t FlowMetrics::stuck_movers() const {
    return std::size_t(
        std::count_if(tracks_.begin(), tracks_.end(), [](const Track &t) { return t.ever_stuck; }));
}

std::size_t FlowMetrics::stuck_now() const {
    return std::size_t(std::count_if(tracks_.begin(), tracks_.end(), [](const Track &t) { return t.stuck; }));
}
} // namespace i4sim
