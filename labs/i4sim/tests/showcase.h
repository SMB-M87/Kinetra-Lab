#pragma once
// Location of the blueprints in the source tree, for i4sim tests. The showcase scenes
// double as traffic fixtures, so tests read them where they are maintained rather than
// from the copy beside the executable.
#include <filesystem>
#include <string>
#include <string_view>

namespace i4sim::test {
/// The directory `labs/i4sim/blueprints` in the source tree.
inline std::filesystem::path blueprint_directory() {
    return std::filesystem::path(__FILE__).parent_path().parent_path() / "blueprints";
}
/// Path of `labs/i4sim/blueprints/<name>.json`, one of the original factory blueprints.
inline std::filesystem::path original_blueprint(std::string_view name) {
    return blueprint_directory() / (std::string(name) + ".json");
}
/// Path of `labs/i4sim/blueprints/showcase/<name>.json`.
inline std::filesystem::path showcase_blueprint(std::string_view name) {
    return blueprint_directory() / "showcase" / (std::string(name) + ".json");
}
} // namespace i4sim::test
