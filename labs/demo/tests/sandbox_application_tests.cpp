#include "../include/sandbox_application.h"
#include "../include/sandbox_input.h"
#include <iostream>
#include <stdexcept>

namespace {
using namespace sandbox;
using namespace kinetra;

void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
Vec2 player_position(const SandboxApplication &application) {
    const auto &scene = application.scene();
    return scene.world.body(scene.player).position;
}

void pause_and_resume() {
    SandboxApplication application(0, 42);
    Camera2D camera;
    FrameCommands commands;
    require(!application.presentation().help, "Demo help must start hidden");
    commands.toggle_help = true;
    application.advance(0, commands, camera);
    require(application.presentation().help, "H did not show Demo help");
    application.advance(0, commands, camera);
    require(!application.presentation().help, "H did not hide Demo help");
    commands = {};
    commands.toggle_pause = true;
    commands.player.direction = {1, 0};
    const Vec2 start = player_position(application);
    application.advance(0.1, commands, camera);
    require(application.paused() && length(player_position(application) - start) == 0,
            "Pausing must precede fixed updates");
    commands.toggle_pause = false;
    application.advance(0.1, commands, camera);
    commands.toggle_pause = true;
    application.advance(0, commands, camera);
    require(!application.paused() && length(player_position(application) - start) == 0,
            "Resuming replayed paused time");
    commands.toggle_pause = false;
    application.advance(1.0 / 120, commands, camera);
    require(player_position(application).x > start.x, "Player movement did not resume");
}

void camera_and_pointer_order() {
    SandboxApplication application(0, 42);
    Camera2D camera;
    FrameCommands commands;
    application.advance(0, commands, camera);
    require(length(camera.position - player_position(application)) == 0, "Default camera must follow player");
    commands.debug = commands.ghost = true;
    commands.camera_pan = {1, 0};
    commands.player.direction = {1, 0};
    const auto start = player_position(application);
    application.advance(0.1, commands, camera);
    require(camera.position.x > start.x && length(player_position(application) - start) == 0,
            "Ghost input moved player or failed to pan camera");
    commands = {};
    commands.zoom_steps = -100;
    application.advance(0, commands, camera);
    require(camera.pixels_per_unit == 17, "Normal zoom must be limited");
    commands.debug = true;
    application.advance(0, commands, camera);
    require(camera.pixels_per_unit == 5.f, "Debug zoom must extend visibility");
    commands = {};
    application.advance(0, commands, camera);
    require(length(camera.position - player_position(application)) == 0, "Leaving ghost failed to follow");
}

void pointer_results_and_reset() {
    SandboxApplication application(6, 42);
    Camera2D camera;
    application.advance(0, {}, camera);
    FrameCommands commands;
    commands.toggle_pause = true;
    commands.pointer = PointerCommand{PointerAction::walk, camera.world_to_screen({12, 0})};
    application.advance(0, commands, camera);
    require(application.last_action() == ActionResult::walking && application.scene().player_goal,
            "Walk command lost its route");
    const auto original_path = application.scene().player_path;
    std::vector<Vec2> original_homes;
    for (const auto &agent : application.scene().agents())
        original_homes.push_back(agent.home());
    const Vec2 original_position = player_position(application);
    commands = {};
    commands.debug = commands.ghost = true;
    commands.pointer = PointerCommand{PointerAction::teleport, camera.world_to_screen({9999, 9999})};
    application.advance(0, commands, camera);
    require(application.last_action() == ActionResult::teleport_blocked &&
                length(player_position(application) - original_position) == 0 &&
                application.scene().player_path.size() == original_path.size() &&
                application.scene().player_goal,
            "Rejected teleport changed the player or route");
    commands.debug = commands.ghost = false;
    commands.pointer->action = PointerAction::walk;
    application.advance(0, commands, camera);
    require(application.last_action() == ActionResult::no_route && !application.scene().player_goal &&
                application.scene().player_path.empty(),
            "Failed walk retained the previous route");
    commands.pointer = PointerCommand{PointerAction::target_agents, camera.world_to_screen({3, 4})};
    application.advance(0, commands, camera);
    require(application.last_action() == ActionResult::agent_goals_set, "Agent command result lost");
    require(!application.scene().agents().empty(), "Missing agents for command test");
    for (const auto &agent : application.scene().agents())
        require(!agent.active() || length(agent.goal() - Vec2{3, 4}) == 0, "Active agent missed command");

    commands.debug = commands.ghost = true;
    commands.pointer = PointerCommand{PointerAction::walk, camera.world_to_screen({0, 0})};
    application.advance(0, commands, camera);
    require(application.last_action() == ActionResult::teleported &&
                length(player_position(application)) == 0,
            "Teleport before reset failed");
}

void held_rotation_cancels_navigation() {
    Sandbox scene(0, 42);
    scene.stream(Camera2D{});
    require(scene.walk_to({12, 0}), "Missing initial route");
    PlayerControl control;
    control.turn_left = control.turn_right = true;
    scene.update(1.f / 120, control);
    require(!scene.player_goal && scene.player_path.empty() &&
                scene.world.body(scene.player).angular_velocity == 0,
            "Opposing held rotation controls must cancel navigation without rotating");
}
void input_commands() {
    Input input;
    input.held[static_cast<std::size_t>(Key::w)] = true;
    input.held[static_cast<std::size_t>(Key::q)] = true;
    input.held[static_cast<std::size_t>(Key::e)] = true;
    input.held[static_cast<std::size_t>(Key::right)] = true;
    input.held[static_cast<std::size_t>(Key::space)] = true;
    input.pressed[static_cast<std::size_t>(Key::h)] = true;
    input.wheel = -2;
    auto commands = sandbox::read_commands(input);
    require(commands.player.direction.y == 1 && commands.player.turn_left && commands.player.turn_right &&
                commands.camera_pan.x == 1 && commands.zoom_steps == -2,
            "Held controls lost in command adapter");
    require(!commands.toggle_pause && commands.toggle_help && !commands.pointer,
            "Held keys became toggles or an unclicked pointer became a command");
    input.held[static_cast<std::size_t>(Key::shift)] = true;
    commands = sandbox::read_commands(input);
    require(commands.player.turn_right && commands.player.direction.x == 0,
            "Shift+arrow rotation also translated the player");
    input.clicked = true;
    input.mouse = {12, 34};
    commands = sandbox::read_commands(input);
    require(commands.pointer->action == sandbox::PointerAction::walk &&
                commands.pointer->screen_position.x == 12 && commands.pointer->screen_position.y == 34,
            "Plain click was not mapped to walking at logical mouse coordinates");
    input.held[static_cast<std::size_t>(Key::c)] = true;
    require(sandbox::read_commands(input).pointer->action == sandbox::PointerAction::walk,
            "Agent click mapping changed");
    input.held[static_cast<std::size_t>(Key::t)] = true;
    require(sandbox::read_commands(input).pointer->action == sandbox::PointerAction::walk,
            "T must not change click behavior");
}

} // namespace

int main() {
    try {
        input_commands();
        pause_and_resume();
        camera_and_pointer_order();
        pointer_results_and_reset();
        held_rotation_cancels_navigation();
        std::cout << "PASS sandbox application pause, camera, commands, reset and player intent\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
