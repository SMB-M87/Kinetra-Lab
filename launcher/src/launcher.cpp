#include <chrono>
#include <kinetra/bitmap_text.h>
#include <kinetra/launcher.h>
#include <stdexcept>

namespace kinetra {
MenuLayout::MenuLayout(Vec2 viewport, std::size_t rows)
    : scale(std::min({2.f, viewport.x / 320, viewport.y / std::max(270.f, 150.f + rows * 40)})) {
    if (!finite(viewport) || viewport.x <= 0 || viewport.y <= 0)
        throw std::invalid_argument("Invalid menu viewport");
    origin = (viewport - Vec2{300, std::max(240.f, 120.f + rows * 40)} * scale) / 2;
}
Vec2 MenuLayout::row_center(std::size_t index) const {
    return origin + Vec2{150, 80.f + index * 40} * scale;
}
bool MenuLayout::contains(std::size_t index, Vec2 point) const {
    const auto offset = point - row_center(index);
    return std::abs(offset.x) <= 140 * scale && std::abs(offset.y) <= 16 * scale;
}
LaunchMenu::LaunchMenu(std::vector<LaunchChoice> choices) : choices_(std::move(choices)) {
    if (choices_.empty())
        throw std::invalid_argument("Launcher requires at least one application");
    for (const auto &choice : choices_) {
        if (choice.name.empty() ||
            (choice.configurations.empty() ? choice.default_configuration != 0
                                           : choice.default_configuration >= choice.configurations.size()))
            throw std::invalid_argument("Invalid launcher name or default configuration");
        configurations_.push_back(choice.default_configuration);
    }
}
std::size_t LaunchMenu::selected() const {
    return configuring_ ? configuration() : application_;
}
std::size_t LaunchMenu::row_count() const {
    return configuring_ ? choices_[application_].configurations.size() : choices_.size();
}
std::string_view LaunchMenu::row_name(std::size_t row) const {
    return configuring_ ? choices_[application_].configurations.at(row) : choices_.at(row).name;
}
std::string_view LaunchMenu::description() const {
    return choices_[application_].description;
}
std::optional<LaunchSelection> LaunchMenu::update(const Input &input, Vec2 viewport) {
    if (configuring_ && input.hit(Key::m)) {
        configuring_ = false;
        return std::nullopt;
    }
    auto &selection = configuring_ ? configurations_[application_] : application_;
    if (input.hit(Key::down))
        selection = (selection + 1) % row_count();
    if (input.hit(Key::up))
        selection = (selection + row_count() - 1) % row_count();
    bool confirm = input.hit(Key::enter) || input.hit(Key::space);
    if (input.clicked) {
        const MenuLayout layout(viewport, row_count());
        for (std::size_t i = 0; i < row_count(); ++i)
            if (layout.contains(i, input.mouse)) {
                selection = i;
                confirm = true;
                break;
            }
    }
    if (!confirm)
        return std::nullopt;
    if (!configuring_ && !choices_[application_].configurations.empty()) {
        configuring_ = true;
        return std::nullopt;
    }
    configuring_ = false;
    return LaunchSelection{application_, configuration()};
}
void LaunchMenu::draw(Runtime &runtime) const {
    const MenuLayout layout(runtime.camera.viewport, row_count());
    runtime.begin();
    draw_text(runtime, "KINETRA", layout.origin + Vec2{10, 10} * layout.scale, layout.scale * 3,
              {0.55f, 1, 0.35f});
    draw_text(runtime, configuring_ ? "SELECT CONFIGURATION" : "SELECT APPLICATION",
              layout.origin + Vec2{10, 44} * layout.scale, layout.scale, {0.8f, 0.85f, 1});
    for (std::size_t i = 0; i < row_count(); ++i) {
        runtime.box(runtime.camera.screen_to_world(layout.row_center(i)),
                    Vec2{140, 16} * (layout.scale / runtime.camera.pixels_per_unit), 0,
                    i == selected() ? Color{0.15f, 0.3f, 0.4f} : Color{0.06f, 0.1f, 0.16f});
        draw_text(runtime, row_name(i), layout.row_center(i) - Vec2{130, 4} * layout.scale, layout.scale,
                  {0.95f, 0.95f, 1});
    }
    const float footer = 102.f + row_count() * 40;
    draw_text(runtime, "ARROWS / CLICK: SELECT   ENTER: START",
              layout.origin + Vec2{10, footer} * layout.scale, layout.scale, {0.8f, 0.85f, 1});
    draw_text(runtime, configuring_ ? "M: BACK   ESC: EXIT" : "ESC: EXIT",
              layout.origin + Vec2{10, footer + 18} * layout.scale, layout.scale, {0.8f, 0.85f, 1});
    runtime.end();
}
SessionHost::SessionHost(std::vector<ApplicationRegistration> applications)
    : applications_(std::move(applications)) {
    LaunchMenu validate(choices());
    for (const auto &application : applications_)
        if (!application.create)
            throw std::invalid_argument("Missing application factory");
}
std::vector<LaunchChoice> SessionHost::choices() const {
    std::vector<LaunchChoice> result;
    for (const auto &application : applications_)
        result.push_back(application.choice);
    return result;
}
void SessionHost::start(LaunchSelection selection, Camera2D &camera) {
    const auto &entry = applications_.at(selection.application);
    const auto count = entry.choice.configurations.size();
    if ((count == 0 && selection.configuration != 0) || (count && selection.configuration >= count))
        throw std::out_of_range("Unknown application configuration");
    auto next_camera = camera;
    auto next = entry.create(next_camera, selection.configuration);
    if (!next)
        throw std::runtime_error("Application factory returned no session");
    session_ = std::move(next);
    camera = next_camera;
    fresh_ = true;
}
void SessionHost::return_to_menu() {
    session_.reset();
    fresh_ = false;
}
void SessionHost::advance(double elapsed, const Input &input, Camera2D &camera) {
    if (!session_)
        return;
    if (fresh_) {
        session_->advance(0, {}, camera);
        fresh_ = false;
    } else
        session_->advance(elapsed, input, camera);
}
int run_launcher(Runtime &runtime, std::vector<ApplicationRegistration> applications,
                 std::optional<LaunchSelection> initial, int frame_limit) {
    if (frame_limit < 0)
        throw std::invalid_argument("Negative frame limit");
    SessionHost host(std::move(applications));
    LaunchMenu menu(host.choices());
    if (initial)
        host.start(*initial, runtime.camera);
    using Clock = std::chrono::steady_clock;
    auto previous = Clock::now(), last_title = previous;
    int frames = 0;
    while (runtime.poll()) {
        const auto now = Clock::now();
        const double elapsed = std::chrono::duration<double>(now - previous).count();
        previous = now;
        bool switched = false;
        if (host.session() && runtime.input().hit(Key::m)) {
            host.return_to_menu();
            switched = true;
        } else if (!host.session()) {
            if (const auto selection = menu.update(runtime.input(), runtime.camera.viewport)) {
                host.start(*selection, runtime.camera);
                switched = true;
            }
        }
        // World construction must not accrue catch-up simulation time.
        if (switched)
            previous = Clock::now();
        if (host.session()) {
            host.advance(elapsed, runtime.input(), runtime.camera);
            host.session()->render(runtime);
        } else
            menu.draw(runtime);
        if (switched || now - last_title > std::chrono::milliseconds(500)) {
            runtime.title(host.session() ? host.session()->title() : "Kinetra | Start menu");
            last_title = now;
        }
        if (++frames == frame_limit)
            break;
    }
    return frames;
}
} // namespace kinetra
