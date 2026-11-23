#pragma once
#include <cstddef>
#include <cstdint>
#include <kinetra/runtime/launcher.h>
#include <optional>
#include <vector>

/// Composition of the bundled `kinetra` launcher: which applications it offers, in which
/// order, and how each one starts.
namespace launcher {

/// The bundled applications in menu order; the value is the registration index.
enum class Variant { demo, dotio, i4sim };

/// Choices that apply to every launch of one program run.
struct Options {
    /// Number of initial Demo terrain agents, 0..300.
    int agents{};
    /// World seed shared by all applications.
    std::uint32_t seed{};
    /// Default Dotio difficulty, an index into dotio::difficulty_names().
    std::size_t dotio_difficulty{};
    /// Default Demo scenario, an index into demo::scenario_labels().
    std::size_t demo_scenario{};
};

/// Lab's launcher keys: no menu key and no Escape shutdown. Sessions leave through their
/// settings panel (LAUNCHER, EXIT) and the window's close button.
inline const kinetra::LauncherBindings bindings{std::nullopt, false};

/// Registrations of the bundled applications, indexed by Variant. Each factory resets the
/// camera to the application's initial view before creating the session.
std::vector<kinetra::ApplicationRegistration> applications(const Options &options);

/// The selection that starts `variant` directly with its configured scenario or difficulty.
kinetra::LaunchSelection selection(Variant variant, const Options &options);

} // namespace launcher
