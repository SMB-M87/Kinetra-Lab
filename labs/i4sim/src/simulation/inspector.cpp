#include "i4sim/simulation/inspector.h"

#include <utility>

namespace i4sim {
namespace {
using kinetra::inspector_number;
using kinetra::inspector_vector;

const char *lifecycle_name(MoverLifecycle lifecycle) {
    switch (lifecycle) {
    case MoverLifecycle::moving:
        return "MOVING";
    case MoverLifecycle::unreachable:
        return "UNREACHABLE";
    case MoverLifecycle::idle:
        return "IDLE";
    }
    return "";
}

const char *goal_name(GoalKind goal) {
    switch (goal) {
    case GoalKind::automatic:
        return "AUTOMATIC";
    case GoalKind::manual:
        return "MANUAL";
    case GoalKind::parking_staging:
        return "PARKING STAGING";
    }
    return "";
}
} // namespace

kinetra::InspectorSnapshot inspect_mover(const Simulation &simulation, std::size_t index) {
    using kinetra::InspectorGroup;
    const auto &mover = simulation.movers().at(index);
    const auto &settings = simulation.motion_settings();
    const char *acceleration_unit = " MM/TICK/TICK";
    InspectorGroup route{"ROUTE",
                         {{"GOAL", inspector_vector(mover.destination, " MM")},
                          {"GOAL TYPE", goal_name(mover.goal)},
                          {"WAYPOINTS LEFT", std::to_string(mover.route.size())}}};
    for (std::size_t i = 0; i < mover.route.size(); ++i)
        route.rows.push_back({std::to_string(i + 1), inspector_vector(mover.route[i], " MM")});
    InspectorGroup passing{
        "PASSING",
        {{"ENABLED",
          settings.predictive_passing && supports_predictive_passing(settings.profile) ? "YES" : "NO"}}};
    if (const auto active = simulation.passing_details(index)) {
        passing.rows.push_back({"PLAN", std::to_string(active->id)});
        passing.rows.push_back({"PARTNER", simulation.movers().at(active->partner).definition.id});
        passing.rows.push_back(
            {"PHASE", active->waiting ? "YIELDING" : (active->crossing ? "CROSSING" : "ENTRY")});
        passing.rows.push_back({"TARGET", inspector_vector(active->target, " MM")});
    } else {
        passing.rows.push_back({"PLAN", "NONE"});
    }
    return {mover.definition.id,
            {{"STATE",
              {{"LIFECYCLE", lifecycle_name(mover.lifecycle)},
               {"COORDINATION", mover.coordination.corridor_retreating ? "CORRIDOR RETREAT"
                                : mover.coordination.passing           ? "PASSING DETOUR"
                                                                       : "NONE"},
               {"HELD", mover.held ? "YES" : "NO"},
               {"PRODUCT", mover.product ? std::to_string(*mover.product) : "NONE"},
               {"STALLED", simulation.stalled(index) ? "YES" : "NO"},
               {"CURRENT WAIT", std::to_string(mover.current_wait_ticks) + " TICKS"},
               {"TOTAL WAIT", std::to_string(mover.total_wait_ticks) + " TICKS"},
               {"DISTANCE", inspector_number(mover.distance) + " MM"},
               {"CONTACT", std::to_string(mover.collision_ticks) + " TICKS"}}},
             {"MOTION",
              {{"PROFILE", profile_name(settings.profile)},
               {"SPEED LIMIT", inspector_number(settings.speed) + " MM/TICK"},
               {"FORCE LIMIT", inspector_number(settings.force) + acceleration_unit},
               {"HORIZON", inspector_number(settings.horizon) + " TICKS"},
               {"SPEED", inspector_number(kinetra::length(mover.body.velocity)) + " MM/TICK"},
               {"ACCELERATION", inspector_vector(mover.body.acceleration, acceleration_unit)},
               {"SEEK", inspector_vector(mover.forces.seek, acceleration_unit)},
               {"BORDER", inspector_vector(mover.forces.border, acceleration_unit)},
               {"COLLISION", inspector_vector(mover.forces.collision, acceleration_unit)},
               {"PREDICTIVE", inspector_vector(mover.forces.predictive, acceleration_unit)}}},
             std::move(passing),
             std::move(route)}};
}

} // namespace i4sim
