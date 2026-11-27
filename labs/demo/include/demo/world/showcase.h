#pragma once

#include "demo/world/agent.h"

#include <kinetra/simulation/math.h>

#include <cstdint>
#include <optional>
#include <random>
#include <span>
#include <utility>
#include <vector>

namespace demo {

/// When the agents of a shuttle turn toward their next stop.
enum class Turn {
    /// All at once, when every agent has held for the pause (as i4sim's showcase scenes do).
    together,
    /// Each agent on its own, when it has held for the pause.
    alone,
};

/// A closed loop that a lure travels at a fixed speed, for Showcase::follow().
struct Lure {
    /// Corners of the loop in world units, at least two; the last one connects to the first.
    std::vector<kinetra::Vec2> loop;
    /// Units per second along the loop.
    float speed{};
    /// Where the lure starts, as a distance along the loop in world units.
    float start{};
};

/// An agent's place behind a lure, for Showcase::follow().
struct Slot {
    /// Index into the lures.
    std::size_t lure{};
    /// Offset from the lure in world units: in the lure's frame (x ahead, y to its left) when
    /// the showcase rotates slots, in the world's frame otherwise.
    kinetra::Vec2 offset;
};

/// The script of a Demo showcase: it tells the agents of an authored scenario where to go
/// next, so its motion repeats without input. It commands agents only through
/// Agent::retarget(), and each agent must hold on arrival (Agent::hold_on_arrival()), which
/// is how the script sees that a leg has ended. Agent i of the scene is agent i here.
///
/// Distances are world units, times simulated seconds. A value type: a copy is an
/// independent script.
class Showcase {
  public:
    /// Agent i cycles through `stops[i]`, starting at its first stop, and waits `pause`
    /// seconds at each stop; `turn` says whether agents turn together or alone. The first
    /// update() sends every agent to its second stop at once. Throws std::invalid_argument
    /// for an agent with fewer than two stops, a non-finite stop or a negative or
    /// non-finite pause.
    static Showcase shuttle(std::vector<std::vector<kinetra::Vec2>> stops, Turn turn, float pause);
    /// `agents` agents each travel to a random point of `area`, hold there for `pause`
    /// seconds and choose the next point; the points come from `seed`. Throws
    /// std::invalid_argument for an empty area or a negative or non-finite pause.
    static Showcase roam(std::size_t agents, kinetra::Aabb area, float pause, std::uint32_t seed);
    /// Agent i follows `slots[i]` of `lures`. Every quarter second each agent is sent to its
    /// slot, half a second ahead of its lure's motion; with `rotate` the offsets turn with
    /// the lure's heading, as in a formation. Throws std::invalid_argument for a lure with
    /// fewer than two corners, a loop of zero length, a negative or non-finite speed, or a
    /// slot that names no lure.
    static Showcase follow(std::vector<Lure> lures, std::vector<Slot> slots, bool rotate);

    /// Number of agents the script expects.
    std::size_t agent_count() const { return agents_.size(); }
    /// Current position of lure `index`. Throws std::out_of_range for another index.
    kinetra::Vec2 lure_position(std::size_t index) const;
    /// The stop that agent `index` of a shuttle heads for or holds at; none in a roam or
    /// follow script. Throws std::out_of_range for an index beyond agent_count().
    std::optional<kinetra::Vec2> stop(std::size_t index) const;
    /// Where agent `index` of a follow script heads now: its slot, half a second ahead of
    /// its lure. Throws std::out_of_range for an index beyond the slots.
    kinetra::Vec2 slot_target(std::size_t index) const;

    /// Advances the script by `dt` seconds and commands the awake agents whose leg begins.
    /// A dormant agent waits until it wakes; in a shared turn the group does not wait for
    /// it, so it falls one stop out of step (the authored scenes keep every agent awake).
    /// An agent released to a user's command misses the shared turns until it holds.
    /// Returns the indices of the agents it commanded, valid until the next update(), so
    /// the caller can end what their old goal started (a passing plan). Throws
    /// std::invalid_argument, before anything changes, for a negative or non-finite `dt`,
    /// or when `agents` holds another number of agents than agent_count().
    std::span<const std::size_t> update(float dt, std::span<Agent> agents);
    /// Leaves agent `index` to a command of the user until it holds at that goal; then the
    /// script takes it back. Throws std::out_of_range for an index beyond agent_count().
    void release(std::size_t index);

  private:
    enum class Kind { shuttle, roam, follow };
    /// Seconds between two commands of a following agent.
    static constexpr float follow_interval = 0.25f;
    /// Seconds of its lure's travel by which a following agent's target leads the lure.
    static constexpr float follow_lead = 0.5f;

    /// Script state of one agent.
    struct State {
        /// Shuttle: the stops, and the index of the stop the agent heads for or holds at.
        std::vector<kinetra::Vec2> stops;
        std::size_t stop{};
        /// Seconds the agent has held since its last command.
        float held{};
        /// Whether a command of the user owns the agent until it holds.
        bool released{};
    };

    Kind kind_{};
    Turn turn_{Turn::alone};
    float pause_{};
    std::vector<State> agents_;
    /// Together: seconds every agent has held at once.
    float group_held_{};
    /// Roam: the area and the random sequence.
    kinetra::Aabb area_{};
    std::mt19937 random_;
    /// Follow: lures, slots, whether slots rotate, how far along its loop each lure is (kept
    /// within one lap, so a long run keeps its precision) and seconds until the next command.
    std::vector<Lure> lures_;
    std::vector<Slot> slots_;
    bool rotate_{};
    std::vector<float> travelled_;
    float until_command_{};
    /// The agents commanded by the last update().
    std::vector<std::size_t> commanded_;

    /// A seeded random value in [0, 1).
    float unit() { return float(random_() >> 8) / 16777216.f; }
    /// Position and unit direction of `lure` after `distance` units along its loop.
    static std::pair<kinetra::Vec2, kinetra::Vec2> along(const Lure &lure, float distance);
    /// Sends agent `index` to its next stop or point, unless it is dormant.
    void send(std::size_t index, Agent &agent);
};

} // namespace demo
