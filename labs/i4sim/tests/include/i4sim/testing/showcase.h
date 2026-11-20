#pragma once
// Location of the blueprints in the source tree, for i4sim tests. The showcase scenes
// double as traffic fixtures, so tests read them where they are maintained rather than
// from the copy beside the executable. Fixtures that the catalog does not ship live in
// labs/i4sim/tests.
#include <filesystem>
#include <string>
#include <string_view>

namespace i4sim::test {
/// The directory `labs/i4sim/blueprints` in the source tree; the test-support target sets
/// KINETRA_I4SIM_BLUEPRINT_SOURCE_DIR, so the path does not depend on this header's location.
inline std::filesystem::path blueprint_directory() {
    return KINETRA_I4SIM_BLUEPRINT_SOURCE_DIR;
}
/// Path of `labs/i4sim/tests/<name>.json`, a blueprint kept only for tests, such as the
/// original `MAS-demonstrator`. The test-support target sets KINETRA_I4SIM_TEST_FIXTURE_DIR.
inline std::filesystem::path fixture_blueprint(std::string_view name) {
    return std::filesystem::path(KINETRA_I4SIM_TEST_FIXTURE_DIR) / (std::string(name) + ".json");
}
/// Path of `labs/i4sim/blueprints/showcase/<name>.json`.
inline std::filesystem::path showcase_blueprint(std::string_view name) {
    return blueprint_directory() / "showcase" / (std::string(name) + ".json");
}
} // namespace i4sim::test
