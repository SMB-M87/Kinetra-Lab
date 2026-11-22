#pragma once

#include <kinetra/view/inspector.h>

#include <nlohmann/json.hpp>

#include <charconv>
#include <cstdint>
#include <ostream>
#include <stdexcept>
#include <string_view>

namespace kinetra_lab {
/// Read an entire numeric command-line value without locale-dependent parsing.
template <typename Number> Number inspector_argument(std::string_view value) {
    Number result{};
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (end != value.data() + value.size() || error != std::errc{})
        throw std::invalid_argument("Invalid numeric argument: " + std::string(value));
    return result;
}

/// Write one JSON record for the completed tick. A missing snapshot writes null groups
/// for a requested agent that is absent at this tick.
inline void write_inspector_tick(std::ostream &output, std::uint64_t tick, std::string_view id,
                                 const kinetra::InspectorSnapshot *snapshot) {
    using Json = nlohmann::json;
    Json groups;
    if (snapshot) {
        groups = Json::array();
        for (const auto &group : snapshot->groups) {
            Json rows = Json::array();
            for (const auto &row : group.rows)
                rows.push_back({{"label", row.label}, {"value", row.value}});
            groups.push_back({{"heading", group.heading}, {"rows", std::move(rows)}});
        }
    }
    output << Json{{"tick", tick}, {"id", id}, {"groups", std::move(groups)}}.dump() << '\n';
}
} // namespace kinetra_lab
