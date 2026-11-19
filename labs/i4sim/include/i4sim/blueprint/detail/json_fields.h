#pragma once
// JSON field readers shared by the blueprint and scenario loaders. Private to
// kinetra_i4sim_application. Every reader throws std::runtime_error with a short message;
// the loaders add the file path.
#include <cctype>
#include <cmath>
#include <kinetra/simulation/math.h>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace i4sim::json_fields {
using Json = nlohmann::json;
using kinetra::Vec2;
/// `s` in ASCII lower case; blueprint property names compare case-insensitively.
inline std::string lower(std::string s) {
    for (char &c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
/// Copy of `value` with every object key in lower case. Throws when two keys of one object
/// differ only in case.
inline Json normalize(const Json &value) {
    if (value.is_object()) {
        Json out = Json::object();
        for (auto it = value.begin(); it != value.end(); ++it) {
            const auto key = lower(it.key());
            if (out.contains(key))
                throw std::runtime_error("Duplicate case-insensitive property: " + key);
            out[key] = normalize(it.value());
        }
        return out;
    }
    if (value.is_array()) {
        Json out = Json::array();
        for (const auto &v : value)
            out.push_back(normalize(v));
        return out;
    }
    return value;
}
/// A finite JSON number as float.
inline float number(const Json &v) {
    if (!v.is_number())
        throw std::runtime_error("Expected a number");
    const float result = v.get<float>();
    if (!std::isfinite(result))
        throw std::runtime_error("Number is not finite");
    return result;
}
/// An object with numbers `x` and `y`.
inline Vec2 vector(const Json &v) {
    return {number(v.at("x")), number(v.at("y"))};
}
} // namespace i4sim::json_fields
