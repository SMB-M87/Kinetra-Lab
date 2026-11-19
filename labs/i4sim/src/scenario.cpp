#include "scenario.h"
#include "json_fields.h"
#include <array>
#include <fstream>
#include <initializer_list>
#include <stdexcept>
#include <string_view>

namespace i4sim {
namespace {
using json_fields::Json;
using json_fields::lower;

/// Profile names as written in a scenario, in SteeringProfile order.
constexpr std::array<std::string_view, 4> profile_names{"improved", "original", "experimentalrvo",
                                                        "reciprocal"};

/// Rejects keys of `object` that are not in `allowed` (lower case), naming `where`.
void only_keys(const Json &object, std::initializer_list<std::string_view> allowed, std::string_view where) {
    if (!object.is_object())
        throw std::runtime_error(std::string(where) + " must be an object");
    for (auto it = object.begin(); it != object.end(); ++it) {
        bool known = false;
        for (const auto key : allowed)
            known = known || it.key() == key;
        if (!known)
            throw std::runtime_error("Unknown property in " + std::string(where) + ": " + it.key());
    }
}
bool flag(const Json &object, const char *key, bool fallback) {
    if (!object.contains(key))
        return fallback;
    if (!object.at(key).is_boolean())
        throw std::runtime_error(std::string(key) + " must be true or false");
    return object.at(key).get<bool>();
}
SteeringProfile profile(const Json &value) {
    if (!value.is_string())
        throw std::runtime_error("profile must be a string");
    // "experimentalRvo" and "experimental_rvo" both name the RVO profile.
    std::string name;
    for (const char c : lower(value.get<std::string>()))
        if (c != '_')
            name += c;
    for (std::size_t i = 0; i < profile_names.size(); ++i)
        if (name == profile_names[i])
            return SteeringProfile(i);
    throw std::runtime_error("Unknown profile: " + value.get<std::string>());
}
MotionSettings motion(const Json &object) {
    only_keys(object,
              {"profile", "heatmap", "recovery", "parkingtraffic", "junctiontraffic", "corridortraffic",
               "predictivepassing"},
              "scenario.motion");
    MotionSettings settings;
    if (object.contains("profile"))
        settings.profile = profile(object.at("profile"));
    settings.heatmap = flag(object, "heatmap", settings.heatmap);
    settings.recovery = flag(object, "recovery", settings.recovery);
    settings.parking_traffic = flag(object, "parkingtraffic", settings.parking_traffic);
    settings.junction_traffic = flag(object, "junctiontraffic", settings.junction_traffic);
    settings.corridor_traffic = flag(object, "corridortraffic", settings.corridor_traffic);
    settings.predictive_passing = flag(object, "predictivepassing", settings.predictive_passing);
    return settings;
}
ScenarioGoal goal(const Json &object) {
    only_keys(object, {"mover", "position"}, "scenario.goals");
    if (!object.contains("mover") || !object.at("mover").is_string() ||
        object.at("mover").get<std::string>().empty())
        throw std::runtime_error("A goal needs a mover ID");
    if (!object.contains("position"))
        throw std::runtime_error("A goal needs a position");
    only_keys(object.at("position"), {"x", "y"}, "scenario.goals[].position");
    return {object.at("mover").get<std::string>(), json_fields::vector(object.at("position"))};
}
} // namespace

std::optional<Scenario> load_scenario(const std::filesystem::path &path) {
    try {
        std::ifstream stream(path);
        if (!stream)
            throw std::runtime_error("Cannot open file");
        const auto root = json_fields::normalize(Json::parse(stream));
        if (!root.contains("scenario"))
            return std::nullopt;
        const auto &section = root.at("scenario");
        only_keys(section, {"description", "motion", "workload", "goals"}, "scenario");
        Scenario scenario;
        if (section.contains("description")) {
            if (!section.at("description").is_string())
                throw std::runtime_error("scenario.description must be a string");
            scenario.description = section.at("description").get<std::string>();
        }
        if (section.contains("motion"))
            scenario.motion = motion(section.at("motion"));
        scenario.workload = flag(section, "workload", scenario.workload);
        if (section.contains("goals")) {
            if (!section.at("goals").is_array())
                throw std::runtime_error("scenario.goals must be an array");
            for (const auto &entry : section.at("goals"))
                scenario.goals.push_back(goal(entry));
        }
        return scenario;
    } catch (const std::exception &e) {
        throw std::runtime_error(path.string() + ": " + e.what());
    }
}

void apply_scenario(const Scenario &scenario, Simulation &simulation) {
    simulation.configure_motion(scenario.motion);
    simulation.set_workload(scenario.workload);
    const auto &movers = simulation.blueprint().movers;
    for (const auto &target : scenario.goals) {
        // load_blueprint prefixes IDs with the model, so match the blueprint's own form.
        std::optional<std::size_t> index;
        for (std::size_t i = 0; i < movers.size(); ++i) {
            if (movers[i].id != model_name(movers[i].model) + "_" + target.mover)
                continue;
            if (index)
                throw std::runtime_error("Scenario goal mover is ambiguous: " + target.mover);
            index = i;
        }
        if (!index)
            throw std::runtime_error("Scenario goal names no mover: " + target.mover);
        if (!simulation.move_to(*index, target.position))
            throw std::runtime_error("Scenario goal rejected for mover " + target.mover);
    }
}
} // namespace i4sim
