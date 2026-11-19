#pragma once
#include "kinetra_lab/session.h"
#include <cstddef>
#include <cstdint>
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
class Application;

/// A Dotio session that also shows the application it runs.
///
/// Input alone cannot tell a tool where the enemies are, and the AI is not seeded, so a
/// script cannot know it in advance. A tool that plays the game, such as the capture tool's
/// scripted player, reads the world here and answers with ordinary input.
class Session : public lab::Session {
  public:
    /// The application this session advances and draws; it lives as long as the session.
    /// For reading between two advance() calls. The session owns the rules: a caller that
    /// changes the application bypasses them.
    virtual Application &application() = 0;
};

/// Creates a Dotio session at the difficulty with index `difficulty`; throws
/// std::invalid_argument for an index outside difficulty_names(). The caller sets the
/// camera to dotio::initial_zoom for a fresh start. Dotio's world is a singleton, so at
/// most one session may exist at a time.
std::unique_ptr<Session> create_session(std::uint32_t seed, std::size_t difficulty);
} // namespace dotio
