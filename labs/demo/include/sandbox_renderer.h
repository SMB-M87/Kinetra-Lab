#pragma once

#include "sandbox_application.h"
#include <kinetra/debug_view.h>
#include <kinetra/settings_view.h>
#include <string>

namespace sandbox {

/// Present the latest scene and debug options without changing application state.
void render(kinetra::Runtime &runtime, const SandboxApplication &application,
            const kinetra::DebugMenu *menu = nullptr, std::span<const kinetra::DebugOption> options = {},
            bool collisions = false, bool agents = false, bool statistics = false, double fps = 0,
            const kinetra::SettingsOverlay *settings = nullptr);
/// State summary for DemoSession::status(): agents, pause, chunks, camera, paths,
/// avoidance and the last pointer action.
std::string status_text(const SandboxApplication &application);

} // namespace sandbox
