#pragma once
#include "demo/scenario.h"
#include "kinetra_lab/session.h"
#include <cstdint>
#include <memory>

namespace demo {
/// Initial agents of a Demo started without an --agents option.
inline constexpr int default_agent_count = 48;
class Application;

/// A Demo session that also shows the application it runs.
///
/// Input alone cannot tell a tool where an agent is. A tool that clicks on one, such as the
/// capture tool's tour, reads the scene here and answers with ordinary input.
class Session : public kinetra_lab::Session {
  public:
    /// The application this session advances and draws; it lives as long as the session.
    /// For reading between two advance() calls.
    virtual const Application &application() const = 0;
};

/// Creates a Demo session for `scenario`. The streamed terrain starts with `count` agents
/// and uses `seed` for terrain and population. The caller sets the camera's initial zoom.
std::unique_ptr<Session> create_session(int count, std::uint32_t seed, Scenario scenario = Scenario::terrain);
} // namespace demo
