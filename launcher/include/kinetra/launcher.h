#pragma once

#include <functional>
#include <kinetra/session.h>
#include <optional>
#include <string_view>
#include <vector>

namespace kinetra {
struct LaunchChoice {
    std::string name;
    std::string description;
    std::vector<std::string> configurations;
    std::size_t default_configuration{};
};
struct LaunchSelection {
    std::size_t application{}, configuration{};
};
/// Shared logical-pixel geometry for both rendering and picking.
struct MenuLayout {
    Vec2 origin;
    float scale;
    explicit MenuLayout(Vec2 viewport, std::size_t rows = 3);
    Vec2 row_center(std::size_t index) const;
    bool contains(std::size_t index, Vec2 point) const;
};
/// Value-owned registration metadata; configuration names have no engine semantics.
class LaunchMenu {
  public:
    explicit LaunchMenu(std::vector<LaunchChoice> choices);
    std::optional<LaunchSelection> update(const Input &input, Vec2 viewport);
    std::size_t selected() const;
    bool configuring() const { return configuring_; }
    std::size_t configuration() const { return configurations_[application_]; }
    std::size_t row_count() const;
    std::string_view row_name(std::size_t row) const;
    std::string_view description() const;
    void draw(Runtime &runtime) const;

  private:
    std::vector<LaunchChoice> choices_;
    std::vector<std::size_t> configurations_;
    std::size_t application_{};
    bool configuring_{};
};
struct ApplicationRegistration {
    LaunchChoice choice;
    std::function<std::unique_ptr<Session>(Camera2D &, std::size_t)> create;
};
/// Optional host for standard or custom launchers. Factory failure leaves the old
/// session and camera intact; successful transitions suppress selection input/time.
class SessionHost {
  public:
    explicit SessionHost(std::vector<ApplicationRegistration> applications);
    std::vector<LaunchChoice> choices() const;
    void start(LaunchSelection selection, Camera2D &camera);
    void return_to_menu();
    Session *session() const { return session_.get(); }
    void advance(double elapsed, const Input &input, Camera2D &camera);

  private:
    std::vector<ApplicationRegistration> applications_;
    std::unique_ptr<Session> session_;
    bool fresh_{};
};
/// Standard menu/session event loop. M returns to the menu; factories own application
/// configuration and bindings. Zero frame limit means unlimited; returns presented frames.
int run_launcher(Runtime &runtime, std::vector<ApplicationRegistration> applications,
                 std::optional<LaunchSelection> initial = std::nullopt, int frame_limit = 0);
} // namespace kinetra
