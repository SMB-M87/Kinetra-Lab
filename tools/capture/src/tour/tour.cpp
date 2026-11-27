#include "capture/tour/tour.h"

#include "demo/presentation/session.h"
#include "dotio/presentation/session.h"

#include <kinetra/runtime/debug_view.h>
#include <kinetra_lab/settings.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

namespace capture {
namespace {
using kinetra::Color;
using kinetra::DebugMenu;
using kinetra::Input;
using kinetra::Key;
using kinetra::SettingsOverlay;
using kinetra::Vec2;

constexpr double frame_seconds = 1.0 / tour_frames_per_second;
/// How long a button stays down in a click, and a key in the second half of Shift+Ctrl.
constexpr double click_seconds = 0.09, chord_lead_seconds = 0.1;
/// A click ring grows from the first to the second radius while it fades.
constexpr double ring_seconds = 0.45;
constexpr float ring_start_radius = 7, ring_end_radius = 30, ring_thickness = 3;
constexpr Color left_ring{1, 1, 1}, right_ring{1, 0.6f, 0.2f};
/// Seconds a keycap takes to fade after its key is released, and a caption to fade in or out.
constexpr double keycap_fade_seconds = 0.45, caption_fade_seconds = 0.25;
/// Share of the remaining way the pointer covers per frame while it tracks a moving target,
/// and the longest step it takes then, in pixels.
constexpr float tracking_gain = 0.22f, tracking_step = 34;
/// Distance at which the pointer counts as being on a target it follows, in pixels.
constexpr float on_target = 3;
/// Frames between two notches of the wheel.
constexpr int frames_per_notch = 5;
/// Seconds between two clicks on a `[-]` or `[+]` button.
constexpr double step_click_seconds = 0.17;

/// Captions: text size, distance from the top edge, padding of the backdrop.
constexpr int caption_scale = 3, title_scale = 8;
constexpr float caption_top = 22, caption_padding = 14, title_padding = 36;
constexpr float backdrop_alpha = 0.68f;
/// Where a title's top edge sits, as a fraction of the picture's height: below the middle,
/// which the launcher's menu, its EXIT button and the settings panel occupy.
constexpr float title_height_fraction = 0.79f;
/// Keycaps: distance of the row from the bottom edge and between two caps.
constexpr float keycap_bottom = 22, keycap_gap = 10;

/// The keys the tour shows as caps, in the order of the row, with their labels.
constexpr std::pair<Key, std::string_view> keycaps[]{
    {Key::shift, "SHIFT"}, {Key::control, "CTRL"}, {Key::escape, "ESC"}, {Key::enter, "ENTER"},
    {Key::space, "SPACE"}, {Key::w, "W"},          {Key::a, "A"},        {Key::s, "S"},
    {Key::d, "D"},         {Key::q, "Q"},          {Key::e, "E"},        {Key::r, "R"},
    {Key::g, "G"},         {Key::f, "F"},          {Key::m, "M"},        {Key::up, "UP"},
    {Key::down, "DOWN"},   {Key::left, "LEFT"},    {Key::right, "RIGHT"}};

launcher::Options tour_options() {
    return {demo::default_agent_count, capture_seed, dotio::default_difficulty};
}

std::size_t index(Key key) {
    return static_cast<std::size_t>(key);
}

/// Uniform value in [0, 1) from 24 raw bits.
double unit(std::mt19937 &random) {
    return static_cast<double>(random() >> 8) / 16777216.0;
}

int frames_for(double seconds) {
    return std::max(1, static_cast<int>(std::lround(seconds * tour_frames_per_second)));
}

/// `text` on a dark backdrop, centered on x `center` with its top at `top`.
void draw_caption(Canvas &canvas, std::string_view text, float center, float top, int scale, float padding,
                  float strength) {
    const auto width = static_cast<float>(text_width(text, scale));
    const auto height = static_cast<float>(glyph_height * scale);
    fill(canvas, {center - width / 2 - padding, top - padding}, {width + 2 * padding, height + 2 * padding},
         {0, 0, 0, backdrop_alpha * strength});
    draw_text(canvas, text, {center - width / 2, top}, scale, {1, 1, 1, strength});
}
} // namespace

std::size_t row_with_label(std::span<const kinetra_lab::DebugRow> rows, std::string_view label) {
    for (std::size_t row = 0; row < rows.size(); ++row)
        if (rows[row].label == label)
            return row;
    std::string present;
    for (const auto &row : rows)
        present += (present.empty() ? "" : ", ") + row.label;
    throw std::runtime_error("The debug menu has no row '" + std::string(label) +
                             "'; its rows are: " + (present.empty() ? "none" : present));
}

struct Tour::PanelMirror {
    bool statistics{}, debug{}, frame_times{}, frame_limit{};
    SettingsOverlay overlay;
    kinetra::Camera2D camera;

    PanelMirror(std::size_t sim_lines, std::size_t debug_lines, Vec2 viewport)
        : overlay(kinetra_lab::settings_rows(
              statistics, debug, frame_times, frame_limit, std::vector<std::string>(sim_lines, "LINE"),
              // settings_rows adds the two lines every application shares.
              std::vector<std::string>(debug_lines > 2 ? debug_lines - 2 : 0, "LINE"))) {
        camera.viewport = viewport;
        Input open;
        open.pressed[index(Key::control)] = true;
        overlay.update(open, camera);
    }
    /// Screen position of panel point {x, y}.
    Vec2 screen(float x, float y) const {
        return camera.viewport / 2 +
               (Vec2{x, y} - overlay.half_extent()) * SettingsOverlay::scale(camera.viewport);
    }
    /// Applies the click the session's panel got at `point`, so sections open here too.
    void click(Vec2 point) {
        Input input;
        input.clicked = input.mouse_down = true;
        input.mouse = point;
        overlay.update(input, camera);
    }
};

Tour::Tour(kinetra::Runtime &runtime, FrameSink sink, std::uint32_t seed)
    : runtime_(runtime), sink_(std::move(sink)), host_(launcher::applications(tour_options())),
      menu_(host_.choices()), random_(seed), pointer_(runtime.camera.viewport * 0.5f + Vec2{260, 200}),
      scene_(static_cast<int>(runtime.camera.viewport.x), static_cast<int>(runtime.camera.viewport.y)),
      canvas_(scene_) {
    // read_frame() takes each picture from the frame that end() read back.
    runtime_.read_back(true);
}

kinetra_lab::Session &Tour::session() {
    auto *running = dynamic_cast<kinetra_lab::Session *>(host_.session());
    if (!running)
        throw std::logic_error("The tour asked for its session while the launcher menu shows");
    return *running;
}

void Tour::step(const Input &input) {
    auto &camera = runtime_.camera;
    if (!host_.session()) {
        if (const auto selection = menu_.update(input, camera.viewport))
            host_.start(*selection, camera);
        finished_ = menu_.exit_requested();
    } else {
        host_.advance(frame_seconds, input, camera);
        const auto request = host_.session()->request();
        if (request == kinetra::SessionRequest::exit)
            finished_ = true;
        else if (request == kinetra::SessionRequest::menu ||
                 request == kinetra::SessionRequest::configurations) {
            const auto ran = host_.selection();
            // Destroyed before the menu draws, and back on the page of what ran, as in run_launcher.
            host_.return_to_menu();
            if (request == kinetra::SessionRequest::configurations && ran)
                menu_.show_configurations(*ran);
        }
    }
    if (finished_)
        return;
    if (host_.session())
        host_.session()->render(runtime_);
    else
        menu_.draw(runtime_);
    read_frame();
}

void Tour::read_frame() {
    const auto &frame = runtime_.frame();
    // The canvas has the window's logical size; with display scaling the drawable is larger.
    if (frame.width != scene_.width || frame.height != scene_.height)
        throw std::runtime_error("The tour needs a drawable of the window's size, without display scaling");
    // Both run top-down; the canvas has no alpha, and the window is opaque.
    for (std::size_t pixel = 0; pixel < scene_.rgb.size() / 3; ++pixel)
        std::copy_n(frame.rgba.data() + pixel * 4, 3, scene_.rgb.data() + pixel * 3);
}

void Tour::draw_overlays() {
    canvas_ = scene_;
    const auto width = static_cast<float>(canvas_.width), height = static_cast<float>(canvas_.height);
    for (const auto &ring : rings_) {
        const auto progress = static_cast<float>(ring.age / ring_seconds);
        Color color = ring.color;
        color.a = 1 - progress;
        draw_ring(canvas_, ring.center, ring_start_radius + (ring_end_radius - ring_start_radius) * progress,
                  ring_thickness, color);
    }
    if (caption_strength_ > 0 && !shown_caption_.empty())
        draw_caption(canvas_, shown_caption_, width / 2, caption_top + caption_padding, caption_scale,
                     caption_padding, static_cast<float>(caption_strength_));
    if (title_strength_ > 0 && !title_.empty())
        draw_caption(canvas_, title_, width / 2, height * title_height_fraction, title_scale, title_padding,
                     static_cast<float>(title_strength_));
    // The keys that are down, or were a moment ago, as a centered row of caps.
    float row_width = 0;
    for (const auto &[key, label] : keycaps)
        if (key_glow_[index(key)] > 0)
            row_width += static_cast<float>(keycap_width(label)) + keycap_gap;
    const std::string_view wheel_label = wheel_up_ ? "WHEEL UP" : "WHEEL DOWN";
    if (wheel_glow_ > 0)
        row_width += static_cast<float>(keycap_width(wheel_label)) + keycap_gap;
    float x = (width - row_width + keycap_gap) / 2;
    const float y = height - keycap_bottom - static_cast<float>(keycap_height);
    for (const auto &[key, label] : keycaps)
        if (const float glow = key_glow_[index(key)]; glow > 0) {
            draw_keycap(canvas_, label, {x, y}, glow);
            x += static_cast<float>(keycap_width(label)) + keycap_gap;
        }
    if (wheel_glow_ > 0)
        draw_keycap(canvas_, wheel_label, {x, y}, wheel_glow_);
    draw_pointer(canvas_, pointer_);
    if (brightness_ < 1)
        dim(canvas_, brightness_);
}

void Tour::frame() {
    Input input;
    input.held = held_;
    input.pressed = pressed_;
    input.mouse = pointer_;
    input.mouse_down = mouse_down_;
    input.clicked = clicked_;
    input.right_mouse_down = right_down_;
    input.right_clicked = right_clicked_;
    input.wheel = wheel_;
    // An edge happens once; what is held stays for the next frame.
    pressed_.fill(false);
    clicked_ = right_clicked_ = false;
    wheel_ = 0;
    // After EXIT nothing runs any more; the last picture stays, for a fade-out.
    if (!finished_)
        step(input);
    draw_overlays();
    sink_(canvas_);

    for (auto &ring : rings_)
        ring.age += frame_seconds;
    std::erase_if(rings_, [](const Ring &ring) { return ring.age >= ring_seconds; });
    const auto key_fade = static_cast<float>(frame_seconds / keycap_fade_seconds);
    for (std::size_t key = 0; key < key_count; ++key)
        key_glow_[key] = held_[key] ? 1 : std::max(0.f, key_glow_[key] - key_fade);
    wheel_glow_ = std::max(0.f, wheel_glow_ - key_fade);
    // A new caption waits until the old one has faded.
    const double caption_fade = frame_seconds / caption_fade_seconds;
    if (shown_caption_ != caption_) {
        caption_strength_ = std::max(0.0, caption_strength_ - caption_fade);
        if (caption_strength_ == 0)
            shown_caption_ = caption_;
    } else if (!caption_.empty())
        caption_strength_ = std::min(1.0, caption_strength_ + caption_fade);
    title_remaining_ = std::max(0.0, title_remaining_ - frame_seconds);
    title_strength_ = title_remaining_ > 0 ? std::min(1.0, title_strength_ + caption_fade)
                                           : std::max(0.0, title_strength_ - caption_fade);
}

void Tour::wait(double seconds) {
    for (int i = frames_for(seconds); i > 0; --i)
        frame();
}

void Tour::pause(double shortest, double longest) {
    wait(shortest + (longest - shortest) * unit(random_));
}

void Tour::play(double seconds, const std::function<void(int frame)> &before) {
    const int frames = frames_for(seconds);
    for (int i = 0; i < frames; ++i) {
        before(i);
        frame();
    }
}

void Tour::fade_in(double seconds) {
    const int frames = frames_for(seconds);
    for (int i = 1; i <= frames; ++i) {
        brightness_ = static_cast<float>(i) / static_cast<float>(frames);
        frame();
    }
}

void Tour::fade_out(double seconds) {
    const int frames = frames_for(seconds);
    for (int i = frames - 1; i >= 0; --i) {
        brightness_ = static_cast<float>(i) / static_cast<float>(frames);
        frame();
    }
}

void Tour::move_to(Vec2 point) {
    const PointerPath path = plan_pointer_path(pointer_, point, random_);
    const int frames = static_cast<int>(std::ceil(path.duration * tour_frames_per_second));
    for (int i = 1; i <= frames; ++i) {
        pointer_ = path.at(i * frame_seconds);
        frame();
    }
    pointer_ = point;
}

void Tour::aim_at(Vec2 point) {
    const Vec2 way = (point - pointer_) * tracking_gain;
    const float distance = kinetra::length(way);
    pointer_ += distance > tracking_step ? way * (tracking_step / distance) : way;
}

void Tour::press_mouse(bool down) {
    if (down && !mouse_down_)
        clicked_ = true;
    mouse_down_ = down;
}

void Tour::press_button(bool right) {
    (right ? right_down_ : mouse_down_) = true;
    (right ? right_clicked_ : clicked_) = true;
    rings_.push_back({pointer_, 0, right ? right_ring : left_ring});
    wait(click_seconds);
    (right ? right_down_ : mouse_down_) = false;
    frame();
}

void Tour::click() {
    press_button(false);
}

void Tour::right_click() {
    press_button(true);
}

void Tour::click_at(Vec2 point) {
    move_to(point);
    pause(0.1, 0.22);
    click();
}

void Tour::click_on(const std::function<Vec2()> &target, double longest) {
    for (int i = frames_for(longest); i > 0; --i) {
        const Vec2 goal = target();
        if (kinetra::length(goal - pointer_) <= on_target)
            break;
        aim_at(goal);
        frame();
    }
    // A target that kept ahead of the pointer is reached with one more planned move rather
    // than a jump, which would give the script away.
    if (kinetra::length(target() - pointer_) > on_target)
        move_to(target());
    click();
}

void Tour::scroll(int notches) {
    for (int i = 0; i < std::abs(notches); ++i) {
        wheel_ = notches > 0 ? 1.f : -1.f;
        wheel_up_ = notches > 0;
        wheel_glow_ = 1;
        for (int rest = 0; rest < frames_per_notch; ++rest)
            frame();
    }
}

void Tour::set_held(std::initializer_list<Key> keys) {
    std::array<bool, key_count> next{};
    for (const Key key : keys)
        next[index(key)] = true;
    for (std::size_t key = 0; key < key_count; ++key)
        pressed_[key] = pressed_[key] || (next[key] && !held_[key]);
    held_ = next;
}

void Tour::hold(std::initializer_list<Key> keys, double seconds) {
    for (const Key key : keys)
        held_[index(key)] = pressed_[index(key)] = true;
    wait(seconds);
    for (const Key key : keys)
        held_[index(key)] = false;
    // The frame of the release: the settings panel acts on it.
    frame();
}

void Tour::caption(std::string text) {
    caption_ = std::move(text);
}

void Tour::title(std::string text, double seconds) {
    title_ = std::move(text);
    title_remaining_ = seconds;
}

kinetra::MenuLayout Tour::menu_layout(std::size_t rows) const {
    return in_menu() ? menu_.layout(viewport()) : kinetra::MenuLayout(viewport(), rows);
}

void Tour::click_menu_page_row(std::size_t row, std::size_t rows) {
    click_at(menu_layout(rows).row_center(row));
}

void Tour::click_menu_footer(kinetra::FooterButton button, std::size_t rows) {
    // i4sim's blueprint screen is a menu with LAUNCHER on every page; a menu with as many
    // rows puts its footer where i4sim's is.
    const auto x = in_menu()
                       ? menu_.footer_x(button)
                       : kinetra::LaunchMenu(std::vector<kinetra::LaunchChoice>(rows, {"ROW", "", {}, 0}), "",
                                             kinetra::LauncherButton::every_page)
                             .footer_x(button);
    if (!x)
        throw std::runtime_error("The menu page shows no such footer button");
    click_at(menu_layout(rows).footer_center(*x));
}

void Tour::toggle_debug_menu() {
    // Shift goes down first and Ctrl joins it, as a hand presses the two.
    held_[index(Key::shift)] = pressed_[index(Key::shift)] = true;
    wait(chord_lead_seconds);
    held_[index(Key::control)] = pressed_[index(Key::control)] = true;
    wait(click_seconds);
    held_[index(Key::shift)] = held_[index(Key::control)] = false;
    frame();
}

Vec2 Tour::row_point(std::size_t row, float panel_x) const {
    return debug_menu_origin +
           Vec2{panel_x, DebugMenu::row_top(row) + DebugMenu::row_height / 2} * DebugMenu::scale(viewport());
}

void Tour::click_row(std::string_view label) {
    const auto rows = session().debug_rows();
    const std::size_t row = row_with_label(rows, label);
    // On the label's text, a little into it, where a hand would aim.
    const float into = std::min(kinetra::text_width(label) / 2, 34.f);
    click_at(row_point(row, DebugMenu::label_left(rows[row].depth) + into));
}

void Tour::step_row(std::string_view label, bool up, int times) {
    const auto rows = session().debug_rows();
    const std::size_t row = row_with_label(rows, label);
    move_to(row_point(row, (up ? DebugMenu::increase_left : DebugMenu::decrease_left) +
                               DebugMenu::stepper_width / 2));
    pause(0.1, 0.22);
    for (int i = 0; i < times; ++i) {
        click();
        wait(step_click_seconds);
    }
}

void Tour::open_settings(std::size_t sim_lines, std::size_t debug_lines) {
    tap({Key::control});
    panel_ = std::make_shared<PanelMirror>(sim_lines, debug_lines, viewport());
}

void Tour::click_setting(SettingsRow row) {
    if (!panel_)
        throw std::logic_error("The tour clicked a settings row without an open panel");
    // On the row's label, right of its mark.
    const Vec2 point = panel_->screen(64, panel_->overlay.row_y(static_cast<std::size_t>(row)) + 3);
    click_at(point);
    panel_->click(point);
}

void Tour::click_settings_action(float x) {
    if (!panel_)
        throw std::logic_error("The tour clicked a settings button without an open panel");
    click_at(panel_->screen(x, panel_->overlay.actions_y() + 3));
    panel_.reset();
}
} // namespace capture
