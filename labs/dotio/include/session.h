#pragma once
#include <cstddef>
#include <cstdint>
#include <lab/session.h>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace dotio {
/// Difficulty names in menu order (EASY, MEDIUM, HARD). Sessions take an index into it,
/// so callers such as the launcher need not know Dotio's difficulty type.
std::vector<std::string> difficulty_names();
/// Index of a difficulty given as easy, medium or hard. Throws std::invalid_argument
/// for any other name.
std::size_t difficulty_index(std::string_view name);
/// Index of MEDIUM, the default difficulty.
inline constexpr std::size_t default_difficulty = 1;
/// Creates a Dotio session at the difficulty with index `difficulty`; throws
/// std::invalid_argument for an index outside difficulty_names(). The caller sets the
/// camera to dotio::initial_zoom for a fresh start. Dotio's world is a singleton, so at
/// most one session may exist at a time.
std::unique_ptr<lab::Session> create_session(std::uint32_t seed, std::size_t difficulty);
} // namespace dotio
