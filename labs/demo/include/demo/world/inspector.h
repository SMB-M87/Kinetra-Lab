#pragma once

#include <kinetra/simulation/math.h>
#include <kinetra/view/inspector.h>

#include <optional>

namespace demo {
class Scene;
/// Details for the currently selected agent, if it still belongs to the scene.
std::optional<kinetra::InspectorSnapshot> inspect_agent(const Scene &scene);
/// Details for the agent whose home matches these world coordinates, if it is resident.
std::optional<kinetra::InspectorSnapshot> inspect_agent(const Scene &scene, kinetra::Vec2 home);
} // namespace demo
