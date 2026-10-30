#include "axes_preview.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace orchard {
namespace editor {

namespace fg = fizmo::graphics;
using fizmo::colors::Color;
using fizmo::linalg::vector3d;

namespace {

const Color background(24, 28, 22);
const Color grid_line(52, 60, 48);
const Color grid_edge(70, 80, 64);
const Color axis_colors[3] = {Color(232, 100, 90), Color(143, 209, 106), Color(96, 156, 245)};
const Color gravity_color(242, 201, 109);
const Color forward_color(232, 238, 226);
const Color turn_color(196, 140, 240);
const Color sample_color(160, 170, 150);
const Color label_muted(148, 160, 140);

const char* const axis_names[3] = {"X", "Y", "Z"};

Color dim(Color c, int alpha) {
    return Color(c.red(), c.green(), c.blue(), static_cast<std::uint8_t>(alpha));
}

std::string axis_label(Vec3 v) {
    const float c[3] = {v.x, v.y, v.z};
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(c[i]) > 0.5f) return std::string(c[i] > 0.0f ? "+" : "-") + axis_names[i];
    }
    return "?";
}

std::string number(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(v));
    std::string s = buf;
    while (s.size() > 1 && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s == "-0" ? "0" : s;
}

std::string vec_text(Vec3 v) {
    return "(" + number(v.x) + ", " + number(v.y) + ", " + number(v.z) + ")";
}

const char* known_as(const AxisSystem& s) {
    if (s == AxisSystem::y_up()) return "Godot, OpenGL, Maya, Fizmo";
    if (s == AxisSystem::y_up_left()) return "Unity, DirectX, Cinema 4D";
    if (s == AxisSystem::z_up()) return "Blender, 3ds Max";
    if (s == AxisSystem::z_up_left()) return "Unreal";
    if (s == AxisSystem::source()) return "Source";
    return "your own";
}

vector3d v3(Vec3 v) {
    return vector3d{v.x, v.y, v.z};
}

Vec3 to_vec(const vector3d& v) {
    return {static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z)};
}

fizmo::text::TextStyle label_style(float size, Color c) {
    fizmo::text::TextStyle st;
    st.set_size(size);
    st.set_fg_color(c);
    return st;
}

} // namespace

fizmo::Axes3D to_fizmo(const AxisSystem& system) {
    fizmo::Axes3D out;
    switch (system.base()) {
    case Axes::y_up_left: out = fizmo::Axes3D::y_up_left(); break;
    case Axes::z_up: out = fizmo::Axes3D::z_up(); break;
    case Axes::z_up_left: out = fizmo::Axes3D::z_up_left(); break;
    case Axes::y_up: break;
    }
    if (system.flipped(AxisFlip::x)) out.flip_x();
    if (system.flipped(AxisFlip::y)) out.flip_y();
    if (system.flipped(AxisFlip::z)) out.flip_z();
    return out;
}

std::vector<std::pair<std::string, std::string>> describe_axes(const AxesChoice& c) {
    std::vector<std::pair<std::string, std::string>> out;
    if (c.dimension == Dimension::two_d) {
        const bool down = c.y_axis == YAxis2D::down;
        out.push_back({"+X points", c.flip_x_2d ? "left" : "right"});
        out.push_back({"+Y points", down ? "down (screen style)" : "up (math style)"});
        out.push_back({"Gravity", "(0, " + std::string(down ? "9.81" : "-9.81") + ")"});
        out.push_back({"Positive angles", down != c.flip_x_2d ? "turn clockwise" : "turn counter-clockwise"});
        out.push_back({"Like", down ? (c.flip_x_2d ? "your own" : "screens, Godot 2D, GameMaker") : (c.flip_x_2d ? "your own" : "math, Box2D, Unity 2D")});
        return out;
    }
    const AxisSystem& s = c.system;
    out.push_back({"Up", axis_label(s.up())});
    out.push_back({"Forward", axis_label(s.forward())});
    out.push_back({"Right", axis_label(s.right())});
    out.push_back({"Handedness", s.mirrored() ? "left-handed" : "right-handed"});
    out.push_back({"Gravity", vec_text(s.gravity(9.81f))});
    out.push_back({"Positive yaw", s.mirrored() ? "turns right (clockwise from above)" : "turns left (counter-clockwise from above)"});
    out.push_back({"Like", known_as(s)});
    return out;
}

void AxesPreview::orbit(float dx, float dy) {
    m_yaw = std::fmod(m_yaw - dx * 0.4f, 360.0f);
    m_pitch = std::clamp(m_pitch + dy * 0.4f, -80.0f, 80.0f);
}

void AxesPreview::reset() {
    m_yaw = 35.0f;
    m_pitch = 22.0f;
}

void AxesPreview::draw(grove::GpuCanvasFrame& f, const AxesChoice& choice) {
    fg::Renderer& r = f.renderer;
    const unsigned w = static_cast<unsigned>(std::max(1.0, std::round(f.width)));
    const unsigned h = static_cast<unsigned>(std::max(1.0, std::round(f.height)));
    r.draw_rect(0, 0, w, h, fg::Paint::fill(background));
    if (choice.dimension == Dimension::two_d) draw_2d(f, choice);
    else draw_3d(f, choice.system);
}

void AxesPreview::draw_3d(grove::GpuCanvasFrame& f, const AxisSystem& s) {
    fg::Renderer& r = f.renderer;
    const float yaw = m_yaw * 0.017453292f, pitch = m_pitch * 0.017453292f;
    const Vec3 focus = s.up() * 0.9f;
    const Vec3 eye_r{std::sin(yaw) * std::cos(pitch) * 11.5f, std::sin(pitch) * 11.5f, std::cos(yaw) * std::cos(pitch) * 11.5f};
    const Vec3 eye = focus + s.from_render(eye_r);
    fg::Camera3D cam(v3(eye), f.viewport_width, f.viewport_height, 38.0, 0.05, 100.0);
    cam.set_axes(to_fizmo(s));
    cam.look_at(v3(focus));
    const Vec3 e[3] = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};
    const Vec3 up = s.up(), fwd = s.forward(), right = s.right();
    auto ground = [&s](float x, float z) { return s.from_render({x, 0.0f, z}); };
    const Vec3 sample{1.0f, 2.0f, 3.0f};
    const Vec3 sample_floor = sample - up * s.height(sample);
    const Vec3 ball = up * 2.2f - right * 2.2f + fwd * 0.6f;
    const fizmo::Axes3D fa = to_fizmo(s);
    auto turn_point = [&fa](double degrees, double radius) {
        const vector3d d = fa.direction(degrees, 0.0, true);
        return vector3d{d.x * radius, d.y * radius, d.z * radius};
    };

    if (r.begin_3d(cam, f.viewport_x, f.viewport_y, f.viewport_width, f.viewport_height)) {
        for (int i = -3; i <= 3; ++i) {
            const Color c = std::abs(i) == 3 ? grid_edge : grid_line;
            r.draw_line_3d(v3(ground(static_cast<float>(i), -3.0f)), v3(ground(static_cast<float>(i), 3.0f)), c, 1.0f, true);
            r.draw_line_3d(v3(ground(-3.0f, static_cast<float>(i))), v3(ground(3.0f, static_cast<float>(i))), c, 1.0f, true);
        }
        for (int i = 0; i < 3; ++i) {
            const Vec3 tip = e[i] * 2.4f;
            r.draw_line_3d(vector3d{0.0, 0.0, 0.0}, v3(e[i] * -2.4f), dim(axis_colors[i], 90), 1.5f, false);
            r.draw_line_3d(vector3d{0.0, 0.0, 0.0}, v3(tip), axis_colors[i], 3.5f, false);
            for (int j = 0; j < 3; ++j) {
                if (j == i) continue;
                r.draw_line_3d(v3(tip), v3(tip - e[i] * 0.3f + e[j] * 0.13f), axis_colors[i], 3.0f, false);
                r.draw_line_3d(v3(tip), v3(tip - e[i] * 0.3f - e[j] * 0.13f), axis_colors[i], 3.0f, false);
            }
        }
        const Vec3 ftip = fwd * 1.6f;
        r.draw_line_3d(v3(up * 0.02f), v3(ftip), forward_color, 2.0f, false);
        r.draw_line_3d(v3(ftip), v3(ftip - fwd * 0.25f + right * 0.12f), forward_color, 2.0f, false);
        r.draw_line_3d(v3(ftip), v3(ftip - fwd * 0.25f - right * 0.12f), forward_color, 2.0f, false);
        vector3d prev = turn_point(0.0, 1.1);
        for (int a = 6; a <= 96; a += 6) {
            const vector3d next = turn_point(a, 1.1);
            r.draw_line_3d(prev, next, turn_color, 2.0f, false);
            prev = next;
        }
        const vector3d arrow_tip = turn_point(100.0, 1.1);
        r.draw_line_3d(prev, arrow_tip, turn_color, 2.0f, false);
        const vector3d back_in = turn_point(86.0, 0.95), back_out = turn_point(86.0, 1.25);
        r.draw_line_3d(arrow_tip, back_in, turn_color, 2.0f, false);
        r.draw_line_3d(arrow_tip, back_out, turn_color, 2.0f, false);
        const Vec3 drop = ball - up * 1.3f;
        r.draw_line_3d(v3(ball), v3(drop), gravity_color, 2.5f, false);
        r.draw_line_3d(v3(drop), v3(drop + up * 0.25f + right * 0.12f), gravity_color, 2.5f, false);
        r.draw_line_3d(v3(drop), v3(drop + up * 0.25f - right * 0.12f), gravity_color, 2.5f, false);
        for (int k = 0; k < 3; ++k) {
            r.draw_line_3d(v3(ball - e[k] * 0.12f), v3(ball + e[k] * 0.12f), gravity_color, 2.5f, false);
        }
        r.draw_line_3d(v3(sample), v3(sample_floor), sample_color, 1.0f, true);
        const Vec3 along_a = sample_floor - right * (sample_floor.x * right.x + sample_floor.y * right.y + sample_floor.z * right.z);
        const Vec3 along_b = sample_floor - fwd * (sample_floor.x * fwd.x + sample_floor.y * fwd.y + sample_floor.z * fwd.z);
        r.draw_line_3d(v3(sample_floor), v3(along_a), sample_color, 1.0f, true);
        r.draw_line_3d(v3(sample_floor), v3(along_b), sample_color, 1.0f, true);
        for (int k = 0; k < 3; ++k) {
            r.draw_line_3d(v3(sample - e[k] * 0.1f), v3(sample + e[k] * 0.1f), forward_color, 2.0f, false);
        }
        r.end_3d();
    }

    const double sx = f.width / std::max(1u, f.viewport_width);
    const double sy = f.height / std::max(1u, f.viewport_height);
    auto label = [&](Vec3 p, const std::string& text, float size, Color c) {
        const fg::ScreenPoint3D sp = cam.world_to_screen(v3(p));
        if (!sp.in_front) return;
        const fizmo::text::TextStyle st = label_style(size, c);
        const fizmo::text::TextMetrics m = r.measure_text(text, st);
        r.draw_text(static_cast<int>(std::lround(sp.x * sx - m.width * 0.5)), static_cast<int>(std::lround(sp.y * sy - m.height * 0.5)), text, st);
    };
    for (int i = 0; i < 3; ++i) {
        std::string name = std::string("+") + axis_names[i];
        const float along_up = e[i].x * up.x + e[i].y * up.y + e[i].z * up.z;
        if (along_up > 0.5f) name += " up";
        else if (along_up < -0.5f) name += " down";
        label(e[i] * 2.85f, name, 15.0f, axis_colors[i]);
        label(e[i] * -2.7f, std::string("-") + axis_names[i], 12.0f, dim(axis_colors[i], 160));
    }
    label(fwd * 1.95f, "forward", 12.0f, forward_color);
    label(to_vec(turn_point(100.0, 1.45)), "+yaw", 12.0f, turn_color);
    label(ball + up * 0.35f, "gravity", 12.0f, gravity_color);
    label(sample + up * 0.35f, "(1, 2, 3)", 12.0f, forward_color);
    const fizmo::text::TextStyle corner = label_style(12.0f, label_muted);
    r.draw_text(10, 8, s.name() + (s.mirrored() ? "  ·  left-handed" : "  ·  right-handed"), corner);
}

void AxesPreview::draw_2d(grove::GpuCanvasFrame& f, const AxesChoice& c) {
    fg::Renderer& r = f.renderer;
    const double cw = std::max(1.0, f.width), ch = std::max(1.0, f.height);
    const double ox = cw * 0.5, oy = ch * 0.55;
    const double unit = std::min(cw, ch) / 7.0;
    const double xs = c.flip_x_2d ? -1.0 : 1.0;
    const double ys = c.y_axis == YAxis2D::down ? 1.0 : -1.0;
    auto px = [&](double x, double y) { return fg::RenderPoint(ox + x * unit * xs, oy + y * unit * ys); };
    auto line = [&r](fg::RenderPoint a, fg::RenderPoint b, Color col, unsigned width) {
        r.draw_line(static_cast<int>(std::lround(a.x)), static_cast<int>(std::lround(a.y)), static_cast<int>(std::lround(b.x)), static_cast<int>(std::lround(b.y)), fg::Paint::stroke(col, width));
    };
    auto arrow = [&](double x0, double y0, double x1, double y1, Color col, unsigned width) {
        const fg::RenderPoint a = px(x0, y0), b = px(x1, y1);
        line(a, b, col, width);
        const double dx = b.x - a.x, dy = b.y - a.y, len = std::max(1e-6, std::hypot(dx, dy));
        const double ux = dx / len, uy = dy / len;
        line(b, fg::RenderPoint(b.x - ux * 12.0 - uy * 6.0, b.y - uy * 12.0 + ux * 6.0), col, width);
        line(b, fg::RenderPoint(b.x - ux * 12.0 + uy * 6.0, b.y - uy * 12.0 - ux * 6.0), col, width);
    };
    auto label = [&](fg::RenderPoint p, const std::string& text, float size, Color col) {
        const fizmo::text::TextStyle st = label_style(size, col);
        const fizmo::text::TextMetrics m = r.measure_text(text, st);
        r.draw_text(static_cast<int>(std::lround(p.x - m.width * 0.5)), static_cast<int>(std::lround(p.y - m.height * 0.5)), text, st);
    };
    for (int i = -3; i <= 3; ++i) {
        line(px(i, -3), px(i, 3), grid_line, 1);
        line(px(-3, i), px(3, i), grid_line, 1);
    }
    line(px(-2.6, 0), px(0, 0), dim(axis_colors[0], 90), 1);
    line(px(0, -2.6), px(0, 0), dim(axis_colors[1], 90), 1);
    arrow(0, 0, 2.6, 0, axis_colors[0], 3);
    arrow(0, 0, 0, 2.6, axis_colors[1], 3);
    label(px(3.05, 0), "+X", 15.0f, axis_colors[0]);
    label(px(0, 3.0), "+Y", 15.0f, axis_colors[1]);
    std::vector<fg::RenderPoint> arc;
    for (int a = 0; a <= 80; a += 5) arc.push_back(px(1.1 * std::cos(a * 0.017453292), 1.1 * std::sin(a * 0.017453292)));
    r.draw_polyline(arc, fg::Paint::stroke(turn_color, 2));
    const fg::RenderPoint tip = px(1.1 * std::cos(1.4835), 1.1 * std::sin(1.4835));
    const fg::RenderPoint in = px(0.95 * std::cos(1.3), 0.95 * std::sin(1.3)), out = px(1.25 * std::cos(1.3), 1.25 * std::sin(1.3));
    line(tip, in, turn_color, 2);
    line(tip, out, turn_color, 2);
    label(px(1.15, 1.15), "+angle", 12.0f, turn_color);
    const double gy = c.y_axis == YAxis2D::down ? 1.0 : -1.0;
    const double bx = -2.2, by = c.y_axis == YAxis2D::down ? -2.2 : 2.2;
    arrow(bx, by, bx, by + gy * 1.3, gravity_color, 3);
    r.draw_circle(static_cast<int>(std::lround(px(bx, by).x)), static_cast<int>(std::lround(px(bx, by).y)), 6, fg::Paint::fill(gravity_color));
    label(fg::RenderPoint(px(bx, by).x, px(bx, by).y - 18.0), "gravity", 12.0f, gravity_color);
    line(px(2, 1), px(2, 0), sample_color, 1);
    line(px(2, 1), px(0, 1), sample_color, 1);
    r.draw_circle(static_cast<int>(std::lround(px(2, 1).x)), static_cast<int>(std::lround(px(2, 1).y)), 4, fg::Paint::fill(forward_color));
    label(fg::RenderPoint(px(2, 1).x, px(2, 1).y - 16.0), "(2, 1)", 12.0f, forward_color);
    const fizmo::text::TextStyle corner = label_style(12.0f, label_muted);
    r.draw_text(10, 8, std::string(c.y_axis == YAxis2D::down ? "y-down" : "y-up") + (c.flip_x_2d ? " flip-x" : ""), corner);
}

} // namespace editor
} // namespace orchard
