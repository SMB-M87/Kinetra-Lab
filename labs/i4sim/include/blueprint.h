#pragma once
#include <cstdint>
#include <filesystem>
#include <kinetra/math.h>
#include <limits>
#include <string>
#include <vector>

namespace i4sim {
using kinetra::Vec2;
enum class Model {
    APM4220,
    APM4221,
    APM4230,
    APM4330,
    APM4331,
    APM4350,
    APM4550,
    Kuka,
    Staubli,
    Viper,
    Manuel
};
enum class Interaction {
    place_housing,
    place_trimmer,
    place_lever,
    place_card,
    personalize_card,
    remove_assy
};
struct MoverSpec {
    Vec2 dimension;
    float payload;
};
MoverSpec mover_spec(Model model);
bool supports(Model model, Interaction interaction);
std::string model_name(Model model);
struct MoverDefinition {
    std::string id;
    Model model;
    Vec2 position;
};
struct ProducerDefinition {
    std::string id;
    Model model;
    Vec2 position, processing_position;
    bool rectangular{};
};
struct Zone {
    std::string id;
    Vec2 position, dimension;
};
struct Blueprint {
    std::string name;
    std::uint64_t tick_cap{std::numeric_limits<std::uint64_t>::max()};
    Vec2 dimension, cell_size;
    float mover_max_extent{};
    unsigned producer_max_queue{};
    std::vector<MoverDefinition> movers;
    std::vector<ProducerDefinition> producers;
    std::vector<Zone> zones;
};
/// Newtonsoft-compatible case-insensitive property names. Positions of movers and
/// processing areas are top-left; producer positions are centers, all in millimeters.
/// Throws runtime_error with path and validation details; never partially loads.
Blueprint load_blueprint(const std::filesystem::path &path);
std::vector<std::filesystem::path> discover_blueprints(const std::filesystem::path &directory);
std::filesystem::path default_blueprint_directory();
} // namespace i4sim
