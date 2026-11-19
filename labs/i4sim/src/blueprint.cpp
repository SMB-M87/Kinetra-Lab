#include "blueprint.h"
#include "json_fields.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <kinetra/weighted_grid.h>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX // keeps std::min and std::max usable
#endif
#include <windows.h>
#endif

namespace i4sim {
namespace {
using json_fields::lower;
using json_fields::normalize;
using json_fields::number;
using json_fields::vector;
using Json = nlohmann::json;
const std::array<std::string, 11> names{"APM4220", "APM4221", "APM4230", "APM4330", "APM4331", "APM4350",
                                        "APM4550", "Kuka",    "Staubli", "Viper",   "Manuel"};
std::uint64_t integer(const Json &v) {
    if (!v.is_number_unsigned() && !(v.is_number_integer() && v.get<std::int64_t>() >= 0))
        throw std::runtime_error("Expected a nonnegative integer");
    return v.get<std::uint64_t>();
}
Model model(const Json &v) {
    const auto name = v.get<std::string>();
    for (std::size_t i = 0; i < names.size(); ++i)
        if (lower(name) == lower(names[i]))
            return static_cast<Model>(i);
    throw std::runtime_error("Unknown unit model: " + name);
}
std::string identifier(const Json &v) {
    if (v.is_string()) {
        auto id = v.get<std::string>();
        if (id.empty())
            throw std::runtime_error("Empty unit ID");
        return id;
    }
    return std::to_string(integer(v));
}
const Json &list(const Json &v, const char *key) {
    static const Json empty = Json::array();
    if (!v.contains(key))
        return empty;
    if (!v.at(key).is_array())
        throw std::runtime_error(std::string(key) + " must be an array");
    return v.at(key);
}
void positive(Vec2 v, const char *name) {
    if (v.x <= 0 || v.y <= 0)
        throw std::runtime_error(std::string(name) + " must be positive");
}
void inside(Vec2 p, Vec2 size, Vec2 area) {
    if (!kinetra::finite(p) || p.x < 0 || p.y < 0 || p.x + size.x > area.x + 0.01f ||
        p.y + size.y > area.y + 0.01f)
        throw std::runtime_error("Unit or zone lies outside blueprint dimensions");
}
} // namespace
MoverSpec mover_spec(Model m) {
    static constexpr std::array<MoverSpec, 7> specs{{{{113, 113}, 0.6f},
                                                     {{127, 127}, 1},
                                                     {{115, 155}, 0.8f},
                                                     {{155, 155}, 1.8f},
                                                     {{150, 150}, 1.2f},
                                                     {{155, 235}, 3},
                                                     {{235, 235}, 4.5f}}};
    return specs.at(static_cast<std::size_t>(m));
}
std::string model_name(Model m) {
    return names.at(static_cast<std::size_t>(m));
}
bool supports(Model m, Interaction i) {
    switch (m) {
    case Model::Kuka:
        return i == Interaction::place_housing || i == Interaction::place_trimmer;
    case Model::Staubli:
        return i == Interaction::place_lever || i == Interaction::place_card;
    case Model::Viper:
        return i == Interaction::personalize_card;
    case Model::Manuel:
        return i == Interaction::place_lever || i == Interaction::remove_assy;
    default:
        return false;
    }
}
namespace {
/// Upper bounds of a blueprint: file size in bytes, and movers or producers per kind.
constexpr std::uintmax_t largest_blueprint_file = 8 * 1024 * 1024;
constexpr std::size_t most_units = 10000;

/// Reads the scalar properties and checks that the floor makes a valid grid.
Blueprint read_floor(const Json &root) {
    Blueprint b;
    b.name = root.at("name").get<std::string>();
    if (b.name.empty())
        throw std::runtime_error("Blueprint name is empty");
    b.dimension = vector(root.at("dimension"));
    b.cell_size = vector(root.at("cellsize"));
    positive(b.dimension, "dimension");
    positive(b.cell_size, "cellSize");
    // Constructed for its validation only: it rejects a floor the route grid cannot hold.
    kinetra::WeightedGrid validate_grid(b.dimension, b.cell_size);
    if (root.contains("tickcap"))
        b.tick_cap = integer(root.at("tickcap"));
    b.mover_max_extent = number(root.at("movermaxextent"));
    if (b.mover_max_extent <= 0)
        throw std::runtime_error("moverMaxExtent must be positive");
    const auto queue = integer(root.at("producermaxqueue"));
    if (queue == 0 || queue > 100000)
        throw std::runtime_error("producerMaxQueue must be in 1..100000");
    b.producer_max_queue = unsigned(queue);
    return b;
}

/// Adds movers and producers to a blueprint, each under its model-qualified ID
/// ("APM4220_1"), which must be unique across both kinds.
class UnitReader {
  public:
    explicit UnitReader(Blueprint &blueprint) : blueprint_(blueprint) {}

    /// `position` is the mover's top-left corner; its footprint must lie on the floor.
    void add_mover(const std::string &id, Model model, Vec2 position) {
        if (blueprint_.movers.size() >= most_units)
            throw std::runtime_error("Too many movers");
        inside(position, mover_spec(model).dimension, blueprint_.dimension);
        blueprint_.movers.push_back({qualified(id, model), model, position});
    }
    /// `position` is the producer's center and `processing` the top-left corner of its
    /// processing place.
    void add_producer(const std::string &id, Model model, Vec2 position, Vec2 processing, bool rectangular) {
        if (model < Model::Kuka || blueprint_.producers.size() >= most_units)
            throw std::runtime_error("Invalid producer model or count");
        // The H layout (showcase/h-crossing.json) deliberately draws stations outside the
        // navigable envelope; only their processing areas must be accessible inside the factory.
        if (std::abs(position.x) > blueprint_.dimension.x * 2 ||
            std::abs(position.y) > blueprint_.dimension.y * 2)
            throw std::runtime_error("Producer position exceeds supported envelope");
        inside(processing, {blueprint_.mover_max_extent, blueprint_.mover_max_extent}, blueprint_.dimension);
        blueprint_.producers.push_back({qualified(id, model), model, position, processing, rectangular});
    }
    /// Expands a `moverGroups` entry: movers of one model in rows and columns that fill
    /// the group's rectangle, numbered from the group's `id`.
    void add_mover_group(const Json &group) {
        const auto m = model(group.at("model"));
        const auto size = mover_spec(m).dimension, p = vector(group.at("position")),
                   extent = vector(group.at("dimension"));
        const float spacing = group_spacing(group);
        positive(extent, "group dimension");
        auto id = integer(group.at("id"));
        // C# initializes size on its first insertion: the first row/column is
        // entered even if the group is narrower than one mover.
        bool first = true;
        for (float y = p.y; y + (first ? 0 : size.y) < p.y + extent.y; y += size.y + spacing)
            for (float x = p.x; x + (first ? 0 : size.x) < p.x + extent.x; x += size.x + spacing) {
                add_mover(std::to_string(id++), m, {x, y});
                first = false;
            }
    }
    /// Expands a `producerGroups` entry: producers of one model on a grid of the larger
    /// cell side, each with the group's processing offset.
    void add_producer_group(const Json &group) {
        const auto m = model(group.at("model"));
        const auto p = vector(group.at("position")), extent = vector(group.at("dimension")),
                   processing = vector(group.at("processingpos"));
        const float spacing = group_spacing(group);
        positive(extent, "group dimension");
        const auto cell = blueprint_.cell_size;
        const float size = std::max(cell.x, cell.y);
        auto id = integer(group.at("id"));
        for (float y = p.y; y + size + spacing <= p.y + extent.y; y += size + spacing)
            for (float x = p.x; x + size + spacing <= p.x + extent.x; x += size + spacing)
                add_producer(std::to_string(id++), m, {x, y},
                             processing + Vec2{x - cell.x / 2, y - cell.y / 2}, false);
    }

  private:
    static float group_spacing(const Json &group) {
        const float spacing = group.contains("spacing") ? number(group.at("spacing")) : 0;
        if (spacing < 0)
            throw std::runtime_error("Negative group spacing");
        return spacing;
    }
    std::string qualified(const std::string &id, Model model) {
        auto name = model_name(model) + "_" + id;
        if (!ids_.insert(name).second)
            throw std::runtime_error("Duplicate unit ID: " + name);
        return name;
    }

    Blueprint &blueprint_;
    std::set<std::string> ids_;
};

void read_zones(const Json &root, Blueprint &b) {
    for (const auto &v : list(root, "forbiddenzones")) {
        const auto p = vector(v.at("position")), size = vector(v.at("dimension"));
        positive(size, "zone dimension");
        inside(p, size, b.dimension);
        b.zones.push_back({identifier(v.at("id")), p, size});
    }
}
} // namespace
Blueprint load_blueprint(const std::filesystem::path &path) {
    try {
        if (std::filesystem::file_size(path) > largest_blueprint_file)
            throw std::runtime_error("Blueprint exceeds 8 MiB");
        std::ifstream stream(path);
        if (!stream)
            throw std::runtime_error("Cannot open file");
        const auto root = normalize(Json::parse(stream));
        Blueprint b = read_floor(root);
        UnitReader units(b);
        for (const auto &v : list(root, "movers"))
            units.add_mover(identifier(v.at("id")), model(v.at("model")), vector(v.at("position")));
        for (const auto &v : list(root, "producers"))
            units.add_producer(identifier(v.at("id")), model(v.at("model")), vector(v.at("position")),
                               vector(v.at("processingpos")), v.value("rect", false));
        for (const auto &group : list(root, "movergroups"))
            units.add_mover_group(group);
        for (const auto &group : list(root, "producergroups"))
            units.add_producer_group(group);
        read_zones(root, b);
        return b;
    } catch (const std::exception &e) {
        throw std::runtime_error(path.string() + ": " + e.what());
    }
}
namespace {
/// Path of the running executable, independent of the working directory and of
/// argv[0]/PATH. Throws std::runtime_error (Windows) or std::filesystem::filesystem_error
/// (Linux) when the system cannot report it.
std::filesystem::path executable_path() {
#ifdef _WIN32
    // GetModuleFileNameW truncates silently; retry with a larger buffer until it fits.
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), DWORD(buffer.size()));
        if (length == 0)
            throw std::runtime_error("GetModuleFileNameW failed with error " +
                                     std::to_string(GetLastError()));
        if (length < buffer.size()) {
            buffer.resize(length);
            return buffer;
        }
        buffer.resize(buffer.size() * 2);
    }
#else
    return std::filesystem::read_symlink("/proc/self/exe");
#endif
}
} // namespace
std::vector<std::filesystem::path> discover_blueprints(const std::filesystem::path &directory) {
    std::vector<std::filesystem::path> result;
    for (const auto &entry : std::filesystem::directory_iterator(directory))
        if (entry.is_regular_file() && lower(entry.path().extension().string()) == ".json")
            result.push_back(entry.path());
    std::sort(result.begin(), result.end());
    return result;
}
std::filesystem::path default_blueprint_directory() {
    return executable_path().parent_path() / "i4sim-blueprints";
}
} // namespace i4sim
