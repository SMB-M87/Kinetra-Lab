#include "demo/world/inspector.h"

#include "demo/world/scene.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace demo {
std::optional<kinetra::InspectorSnapshot> inspect_agent(const Scene &scene) {
    const auto selected = scene.selected_agent();
    return selected ? inspect_agent(scene, *selected) : std::nullopt;
}

std::optional<kinetra::InspectorSnapshot> inspect_agent(const Scene &scene, kinetra::Vec2 home) {
    using kinetra::inspector_number;
    using kinetra::inspector_vector;
    const auto agents = scene.agents();
    const auto agent = std::find_if(agents.begin(), agents.end(), [&](const Agent &candidate) {
        return candidate.home().x == home.x && candidate.home().y == home.y;
    });
    if (agent == agents.end())
        return std::nullopt;
    const auto &body = scene.world().body(agent->body_id());
    const auto &tuning = scene.tuning().motion;
    const auto &wander = scene.tuning().wander;
    kinetra::InspectorGroup route{"ROUTE",
                                  {{"GOAL", inspector_vector(agent->goal(), " UNIT")},
                                   {"WAYPOINTS LEFT", std::to_string(agent->route().size())}}};
    std::size_t i = 0;
    for (const auto point : agent->route())
        route.rows.push_back({std::to_string(++i), inspector_vector(point, " UNIT")});
    constexpr int id_precision = std::numeric_limits<float>::max_digits10;
    return kinetra::InspectorSnapshot{
        "AGENT " + inspector_number(home.x, id_precision) + "," + inspector_number(home.y, id_precision),
        {{"STATE",
          {{"ACTIVITY", !agent->active() ? "DORMANT" : (agent->wandering() ? "WANDERING" : "COMMANDED")},
           {"HOME", inspector_vector(home, " UNIT")}}},
         {"MOTION",
          {{"PROFILE", scene.reciprocal_avoidance() ? "RECIPROCAL" : "LEGACY"},
           {"WANDER MODE", wander.mode == WanderMode::steering ? "STEERING" : "ROUTE"},
           {"SPEED", inspector_number(kinetra::length(body.velocity)) + " UNIT/S"},
           {"ACCELERATION", inspector_vector(body.acceleration, " UNIT/S/S")},
           {"SPEED LIMIT", inspector_number(tuning.max_speed) + " UNIT/S"},
           {"FORCE LIMIT", inspector_number(tuning.max_acceleration) + " UNIT/S/S"},
           {"RESPONSE TIME", inspector_number(tuning.response_time) + " S"},
           {"CLEARANCE", inspector_number(tuning.clearance) + " UNIT"},
           {"HORIZON",
            inspector_number(scene.reciprocal_avoidance() ? tuning.horizon : tuning.lookahead) + " S"}}},
         std::move(route)}};
}
} // namespace demo
