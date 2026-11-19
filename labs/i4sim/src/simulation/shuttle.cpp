#include "i4sim/simulation/detail/shuttle.h"

#include <algorithm>

namespace i4sim::detail {
void Shuttle::join(std::size_t mover, Vec2 from, Vec2 to) {
    leave(mover);
    members_.push_back({mover, {from, to}, 1});
    // A new member is under way, so a pause that was running no longer holds.
    arrived_.reset();
}
void Shuttle::leave(std::size_t mover) {
    std::erase_if(members_, [&](const Member &member) { return member.mover == mover; });
}
bool Shuttle::contains(std::size_t mover) const {
    return std::any_of(members_.begin(), members_.end(),
                       [&](const Member &member) { return member.mover == mover; });
}
std::vector<ShuttleLeg> Shuttle::update(const std::vector<Mover> &movers, std::uint64_t tick) {
    // Arrival turns a manual goal automatic, so only a moving mover still needs a manual
    // one; an automatic goal while moving means production or parking took the mover.
    std::erase_if(members_, [&](const Member &member) {
        const auto &m = movers.at(member.mover);
        return m.lifecycle == MoverLifecycle::unreachable || m.product ||
               m.goal == GoalKind::parking_staging || (m.moving() && m.goal != GoalKind::manual);
    });
    const bool all_arrived =
        !members_.empty() && std::all_of(members_.begin(), members_.end(), [&](const Member &member) {
            const auto &m = movers[member.mover];
            return m.lifecycle == MoverLifecycle::idle && !m.held;
        });
    if (!all_arrived) {
        arrived_.reset();
        return {};
    }
    if (!arrived_)
        arrived_ = tick;
    if (tick - *arrived_ < shuttle_pause_ticks)
        return {};
    arrived_.reset();
    std::vector<ShuttleLeg> legs;
    for (auto &member : members_) {
        member.heading = 1 - member.heading;
        legs.push_back({member.mover, member.ends[member.heading]});
    }
    return legs;
}
} // namespace i4sim::detail
