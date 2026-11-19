#pragma once
#include "i4sim/simulation/events.h"

#include <span>
#include <vector>

namespace i4sim::detail {
/// The simulation's opt-in event log (see `Simulation::record_events`). Recording only
/// observes: callers build and add events only while `recording()` is true, and no
/// decision reads the log.
class EventLog {
  public:
    bool recording() const { return recording_; }
    /// Turns recording on or off; events recorded so far stay.
    void record(bool enabled) { recording_ = enabled; }
    /// Appends `event` while recording; does nothing otherwise.
    void add(const SimulationEvent &event) {
        if (recording_)
            events_.push_back(event);
    }
    std::span<const SimulationEvent> events() const { return events_; }

  private:
    bool recording_{};
    std::vector<SimulationEvent> events_;
};
} // namespace i4sim::detail
