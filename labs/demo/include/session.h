#pragma once
#include <cstdint>
#include <lab/session.h>
#include <memory>

namespace sandbox {
inline constexpr int default_agent_count = 48;
class SandboxApplication;

/// A Demo session that also shows the application it runs.
///
/// Input alone cannot tell a tool where an agent is. A tool that clicks on one, such as the
/// capture tool's tour, reads the scene here and answers with ordinary input.
class Session : public lab::Session {
  public:
    /// The application this session advances and draws; it lives as long as the session.
    /// For reading between two advance() calls.
    virtual const SandboxApplication &application() const = 0;
};

/// Creates a Demo session with `count` agents and terrain from `seed`. The caller sets the
/// camera to sandbox::initial_zoom for a fresh start.
std::unique_ptr<Session> create_session(int count, std::uint32_t seed);
} // namespace sandbox
