#pragma once
#include <cstdint>
#include <filesystem>
#include <kinetra/math.h>
#include <limits>
#include <string>
#include <vector>

namespace i4sim {
using kinetra::Vec2;
/// The unit models of the original factory: the seven mover types (`APM...`), then the
/// four producers. Blueprints name a model without regard to case. The order is fixed:
/// models before `Kuka` are movers, the others producers.
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
/// One step of a product recipe; `supports` says which producer performs it.
enum class Interaction {
    place_housing,
    place_trimmer,
    place_lever,
    place_card,
    personalize_card,
    remove_assy
};
/// Physical data of a mover model.
struct MoverSpec {
    /// Width and height of the footprint, in millimeters.
    Vec2 dimension;
    /// Payload from the original model table; the simulation does not use it.
    float payload;
};
/// The footprint and payload of mover model `model`. Throws std::out_of_range for a
/// producer model.
MoverSpec mover_spec(Model model);
/// Whether a producer of `model` performs `interaction`; false for every mover model.
bool supports(Model model, Interaction interaction);
/// The model's name as blueprints write it, such as "APM4220" or "Kuka".
std::string model_name(Model model);
/// One mover of a blueprint.
struct MoverDefinition {
    /// Unique among all units of the blueprint: the model name, an underscore and the
    /// blueprint's own id, such as "APM4220_1".
    std::string id;
    Model model;
    /// Top-left corner of the footprint at the start, in millimeters; also the mover's
    /// parking place.
    Vec2 position;
};
/// One producer of a blueprint.
struct ProducerDefinition {
    /// Unique among all units, formed like a mover's id.
    std::string id;
    Model model;
    /// Center of the producer's symbol, and top-left corner of the square where a mover
    /// stands while the producer works on its product; both in millimeters.
    Vec2 position, processing_position;
    /// Drawn as a square instead of a circle.
    bool rectangular{};
};
/// A rectangle of the floor that movers may not enter.
struct Zone {
    std::string id;
    /// Top-left corner and size, in millimeters.
    Vec2 position, dimension;
};
/// A validated factory layout. Coordinates are millimeters from the floor's top-left
/// corner, with Y down.
struct Blueprint {
    /// Shown as the scene's name; never empty.
    std::string name;
    /// Tick at which a simulation of this blueprint finishes; unlimited by default.
    std::uint64_t tick_cap{std::numeric_limits<std::uint64_t>::max()};
    /// Size of the floor, and of one cell of its route grid.
    Vec2 dimension, cell_size;
    /// Side of the square a processing place reserves: the largest mover that may use it.
    float mover_max_extent{};
    /// Products a producer may have assigned at once, in 1..100000.
    unsigned producer_max_queue{};
    /// Single units and expanded groups, in file order: units before groups.
    std::vector<MoverDefinition> movers;
    std::vector<ProducerDefinition> producers;
    /// The forbidden zones.
    std::vector<Zone> zones;
};
/// Reads and validates the blueprint at `path`, and expands its mover and producer groups.
///
/// Newtonsoft-compatible case-insensitive property names. Positions of movers and
/// processing areas are top-left; producer positions are centers, all in millimeters.
/// A `scenario` section is ignored here (see load_scenario). Limits: 8 MiB per file and
/// 10000 movers and producers each. Throws std::runtime_error with the path and the
/// validation detail; never partially loads.
Blueprint load_blueprint(const std::filesystem::path &path);
/// The `.json` files directly in `directory`, sorted by path; subdirectories are not
/// searched. The extension matches without regard to case. Throws
/// std::filesystem::filesystem_error when the directory cannot be read.
std::vector<std::filesystem::path> discover_blueprints(const std::filesystem::path &directory);
/// The `i4sim-blueprints` directory beside the running executable, where CMake copies
/// the bundled catalog. Linux resolves the executable through /proc/self/exe, Windows
/// through GetModuleFileNameW.
std::filesystem::path default_blueprint_directory();
} // namespace i4sim
