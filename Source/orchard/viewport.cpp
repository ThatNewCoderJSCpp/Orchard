#include <orchard/viewport.hpp>

#include <algorithm>
#include <cmath>

namespace orchard {

namespace {

const Color grid_minor{44, 42, 54};
const Color grid_major{62, 59, 76};
const Color axis_x{232, 96, 96};
const Color axis_y{120, 210, 120};
const Color axis_z{96, 150, 240};
const Color shape{190, 184, 205};
const Color selected_shape{247, 180, 72};
const Color camera_shape{139, 213, 202};
const Color light_shape{242, 201, 109};

Color blend(Color a, Color b, float t) {
    auto mix = [t](std::uint8_t x, std::uint8_t y) { return static_cast<std::uint8_t>(x + (y - x) * t); };
    return {mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), 255};
}

bool is_camera(const Scene& scene, Entity e) {
    return scene.name(e).find("Camera") != std::string::npos;
}

bool is_light(const Scene& scene, Entity e) {
    return scene.name(e).find("Light") != std::string::npos;
}

} // namespace

void Viewport::resize(unsigned width, unsigned height) {
    width = std::min(width, 8192u);
    height = std::min(height, 8192u);
    if (width == m_width && height == m_height) return;
    m_width = width;
    m_height = height;
    m_rgba.assign(static_cast<std::size_t>(width) * height * 4, 0);
}

void Viewport::set_view_size(unsigned width, unsigned height) {
    m_width = std::min(width, 8192u);
    m_height = std::min(height, 8192u);
}

void Viewport::orbit(float dx, float dy) {
    if (m_dimension == Dimension::two_d) {
        pan(dx, dy);
        return;
    }
    m_orbit.yaw = std::fmod(m_orbit.yaw - dx * 0.4f, 360.0f);
    m_orbit.pitch = std::clamp(m_orbit.pitch + dy * 0.4f, -89.0f, 89.0f);
}

void Viewport::pan(float dx, float dy) {
    if (m_dimension == Dimension::two_d) {
        m_pan.center.x -= (m_flip_x_2d ? -dx : dx) / m_pan.pixels_per_unit;
        m_pan.center.y += (m_y_axis == YAxis2D::down ? -dy : dy) / m_pan.pixels_per_unit;
        return;
    }
    const float yaw = radians(m_orbit.yaw);
    const Vec3 right{std::cos(yaw), 0.0f, -std::sin(yaw)};
    const float scale = m_orbit.distance * 0.002f;
    m_orbit.target += m_system.from_render(right * (-dx * scale) + Vec3{0.0f, dy * scale, 0.0f});
}

void Viewport::zoom(float steps) {
    const float factor = std::pow(1.15f, steps);
    if (m_dimension == Dimension::two_d) {
        m_pan.pixels_per_unit = std::clamp(m_pan.pixels_per_unit * factor, 4.0f, 2048.0f);
    } else {
        m_orbit.distance = std::clamp(m_orbit.distance / factor, 0.5f, 5000.0f);
    }
}

void Viewport::reset_camera() {
    m_orbit = OrbitCamera{};
    m_pan = PanCamera{};
}

Vec3 Viewport::eye() const {
    const float yaw = radians(m_orbit.yaw);
    const float pitch = radians(m_orbit.pitch);
    const Vec3 offset{std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch)};
    return m_orbit.target + m_system.from_render(offset * m_orbit.distance);
}

Mat4 Viewport::view_projection() const {
    const float aspect = m_height ? static_cast<float>(m_width) / static_cast<float>(m_height) : 1.0f;
    const Vec3 eye_r = m_system.to_render(eye());
    const Vec3 target_r = m_system.to_render(m_orbit.target);
    return perspective(m_orbit.fov, aspect, 0.05f, 10000.0f) * look_at(eye_r, target_r, {0.0f, 1.0f, 0.0f}) * m_system.to_render_matrix();
}

bool Viewport::project(Vec3 world, float& sx, float& sy) const {
    if (m_dimension == Dimension::two_d) {
        sx = (m_flip_x_2d ? m_pan.center.x - world.x : world.x - m_pan.center.x) * m_pan.pixels_per_unit + m_width * 0.5f;
        const float dy = (world.y - m_pan.center.y) * m_pan.pixels_per_unit;
        sy = m_y_axis == YAxis2D::down ? m_height * 0.5f + dy : m_height * 0.5f - dy;
        return true;
    }
    const Vec4 c = view_projection() * Vec4{world.x, world.y, world.z, 1.0f};
    if (c.w <= 0.05f) return false;
    sx = (c.x / c.w * 0.5f + 0.5f) * m_width;
    sy = (0.5f - c.y / c.w * 0.5f) * m_height;
    return true;
}

void Viewport::clear(Color top, Color bottom) {
    for (unsigned y = 0; y < m_height; ++y) {
        const Color c = blend(top, bottom, m_height > 1 ? static_cast<float>(y) / (m_height - 1) : 0.0f);
        std::uint8_t* row = m_rgba.data() + static_cast<std::size_t>(y) * m_width * 4;
        for (unsigned x = 0; x < m_width; ++x) {
            row[x * 4 + 0] = c.r;
            row[x * 4 + 1] = c.g;
            row[x * 4 + 2] = c.b;
            row[x * 4 + 3] = 255;
        }
    }
}

void Viewport::pixel(int x, int y, Color c) {
    if (x < 0 || y < 0 || x >= static_cast<int>(m_width) || y >= static_cast<int>(m_height)) return;
    std::uint8_t* p = m_rgba.data() + (static_cast<std::size_t>(y) * m_width + x) * 4;
    p[0] = c.r;
    p[1] = c.g;
    p[2] = c.b;
    p[3] = c.a;
}

void Viewport::line(float x0, float y0, float x1, float y1, Color c) {
    const float w = static_cast<float>(m_width), h = static_cast<float>(m_height);
    if ((x0 < 0 && x1 < 0) || (y0 < 0 && y1 < 0) || (x0 >= w && x1 >= w) || (y0 >= h && y1 >= h)) return;
    const float dx = x1 - x0, dy = y1 - y0;
    const int steps = static_cast<int>(std::min(std::max(std::fabs(dx), std::fabs(dy)), 20000.0f));
    if (steps == 0) {
        pixel(static_cast<int>(x0), static_cast<int>(y0), c);
        return;
    }
    const float sx = dx / steps, sy = dy / steps;
    float x = x0, y = y0;
    for (int i = 0; i <= steps; ++i) {
        pixel(static_cast<int>(std::lround(x)), static_cast<int>(std::lround(y)), c);
        x += sx;
        y += sy;
    }
}

void Viewport::line3(Vec3 a, Vec3 b, Color c) {
    if (m_dimension == Dimension::two_d) {
        float ax, ay, bx, by;
        project(a, ax, ay);
        project(b, bx, by);
        line(ax, ay, bx, by, c);
        return;
    }
    const Mat4 vp = view_projection();
    Vec4 ca = vp * Vec4{a.x, a.y, a.z, 1.0f};
    Vec4 cb = vp * Vec4{b.x, b.y, b.z, 1.0f};
    const float near_w = 0.05f;
    if (ca.w < near_w && cb.w < near_w) return;
    if (ca.w < near_w || cb.w < near_w) {
        const float t = (near_w - ca.w) / (cb.w - ca.w);
        const Vec4 hit{ca.x + (cb.x - ca.x) * t, ca.y + (cb.y - ca.y) * t, ca.z + (cb.z - ca.z) * t, near_w};
        if (ca.w < near_w) ca = hit;
        else cb = hit;
    }
    auto screen = [this](Vec4 v, float& x, float& y) {
        x = (v.x / v.w * 0.5f + 0.5f) * m_width;
        y = (0.5f - v.y / v.w * 0.5f) * m_height;
    };
    float ax, ay, bx, by;
    screen(ca, ax, ay);
    screen(cb, bx, by);
    line(ax, ay, bx, by, c);
}

void Viewport::rect(float x, float y, float w, float h, Color c) {
    line(x, y, x + w, y, c);
    line(x + w, y, x + w, y + h, c);
    line(x + w, y + h, x, y + h, c);
    line(x, y + h, x, y, c);
}

void Viewport::grid_2d() {
    float step = 1.0f;
    while (step * m_pan.pixels_per_unit < 16.0f) step *= 5.0f;
    const float half_w = m_width * 0.5f / m_pan.pixels_per_unit;
    const float half_h = m_height * 0.5f / m_pan.pixels_per_unit;
    const float left = m_pan.center.x - half_w, right = m_pan.center.x + half_w;
    const float bottom = m_pan.center.y - half_h, top = m_pan.center.y + half_h;
    for (float x = std::floor(left / step) * step; x <= right; x += step) {
        const bool major = std::fmod(std::fabs(x), step * 5.0f) < step * 0.5f;
        line3({x, bottom, 0.0f}, {x, top, 0.0f}, std::fabs(x) < step * 0.5f ? axis_y : major ? grid_major : grid_minor);
    }
    for (float y = std::floor(bottom / step) * step; y <= top; y += step) {
        const bool major = std::fmod(std::fabs(y), step * 5.0f) < step * 0.5f;
        line3({left, y, 0.0f}, {right, y, 0.0f}, std::fabs(y) < step * 0.5f ? axis_x : major ? grid_major : grid_minor);
    }
}

void Viewport::grid_3d() {
    float step = 1.0f;
    while (m_orbit.distance / step > 40.0f) step *= 10.0f;
    const int lines = 20;
    const Vec3 target_r = m_system.to_render(m_orbit.target);
    const float cx = std::round(target_r.x / step) * step;
    const float cz = std::round(target_r.z / step) * step;
    const float extent = lines * step;
    auto w = [this](float x, float z) { return m_system.from_render({x, 0.0f, z}); };
    for (int i = -lines; i <= lines; ++i) {
        const float x = cx + i * step;
        const float z = cz + i * step;
        const Color cxl = std::fabs(x) < step * 0.5f ? axis_z : (i % 5 == 0 ? grid_major : grid_minor);
        const Color czl = std::fabs(z) < step * 0.5f ? axis_x : (i % 5 == 0 ? grid_major : grid_minor);
        line3(w(x, cz - extent), w(x, cz + extent), cxl);
        line3(w(cx - extent, z), w(cx + extent, z), czl);
    }
    line3({0.0f, 0.0f, 0.0f}, m_system.up() * (step * 2.0f), axis_y);
}

void Viewport::entity_2d(const Scene& scene, Entity e, bool selected) {
    const Mat4 m = scene.world_matrix(e);
    const Color c = selected ? selected_shape : is_camera(scene, e) ? camera_shape : shape;
    const Vec3 corners[4] = {{-0.5f, -0.5f, 0.0f}, {0.5f, -0.5f, 0.0f}, {0.5f, 0.5f, 0.0f}, {-0.5f, 0.5f, 0.0f}};
    Vec3 w[4];
    for (int i = 0; i < 4; ++i) {
        const Vec4 v = m * Vec4{corners[i].x, corners[i].y, corners[i].z, 1.0f};
        w[i] = {v.x, v.y, v.z};
    }
    for (int i = 0; i < 4; ++i) line3(w[i], w[(i + 1) % 4], c);
    if (selected) {
        line3(w[0], w[2], c);
        line3(w[1], w[3], c);
    }
}

void Viewport::entity_3d(const Scene& scene, Entity e, bool selected) {
    const Mat4 m = scene.world_matrix(e);
    const Color c = selected ? selected_shape : is_camera(scene, e) ? camera_shape : shape;
    if (is_camera(scene, e)) {
        const Vec4 o = m * Vec4{0.0f, 0.0f, 0.0f, 1.0f};
        const Vec3 origin{o.x, o.y, o.z};
        const Vec3 tip[4] = {{-0.4f, -0.3f, -0.8f}, {0.4f, -0.3f, -0.8f}, {0.4f, 0.3f, -0.8f}, {-0.4f, 0.3f, -0.8f}};
        Vec3 w[4];
        for (int i = 0; i < 4; ++i) {
            const Vec4 v = m * Vec4{tip[i].x, tip[i].y, tip[i].z, 1.0f};
            w[i] = {v.x, v.y, v.z};
            line3(origin, w[i], c);
        }
        for (int i = 0; i < 4; ++i) line3(w[i], w[(i + 1) % 4], c);
        return;
    }
    if (is_light(scene, e)) {
        const Color lc = selected ? selected_shape : light_shape;
        const Vec4 o = m * Vec4{0.0f, 0.0f, 0.0f, 1.0f};
        const Vec3 origin{o.x, o.y, o.z};
        const Vec3 dirs[6] = {{0.35f, 0, 0}, {-0.35f, 0, 0}, {0, 0.35f, 0}, {0, -0.35f, 0}, {0, 0, 0.35f}, {0, 0, -0.35f}};
        for (const Vec3& d : dirs) line3(origin, origin + d, lc);
        line3(origin + Vec3{0.25f, 0.25f, 0}, origin - Vec3{0.25f, 0.25f, 0}, lc);
        line3(origin + Vec3{-0.25f, 0.25f, 0}, origin - Vec3{-0.25f, 0.25f, 0}, lc);
        line3(origin, origin - m_system.up() * m_system.height(origin), grid_major);
        return;
    }
    Vec3 w[8];
    for (int i = 0; i < 8; ++i) {
        const Vec4 v = m * Vec4{(i & 1) ? 0.5f : -0.5f, (i & 2) ? 0.5f : -0.5f, (i & 4) ? 0.5f : -0.5f, 1.0f};
        w[i] = {v.x, v.y, v.z};
    }
    const int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (const auto& edge : edges) line3(w[edge[0]], w[edge[1]], c);
}

const std::vector<std::uint8_t>& Viewport::render(const Scene& scene, Entity selected) {
    if (m_width == 0 || m_height == 0) return m_rgba;
    if (m_rgba.size() != static_cast<std::size_t>(m_width) * m_height * 4) m_rgba.assign(static_cast<std::size_t>(m_width) * m_height * 4, 0);
    clear({30, 28, 37}, {20, 19, 25});
    if (m_dimension == Dimension::two_d) {
        grid_2d();
    } else {
        grid_3d();
    }
    for (Entity e : scene.entities()) {
        if (e == selected) continue;
        if (m_dimension == Dimension::two_d) entity_2d(scene, e, false);
        else entity_3d(scene, e, false);
    }
    if (scene.alive(selected)) {
        if (m_dimension == Dimension::two_d) entity_2d(scene, selected, true);
        else entity_3d(scene, selected, true);
        float sx, sy;
        const Vec4 o = scene.world_matrix(selected) * Vec4{0.0f, 0.0f, 0.0f, 1.0f};
        if (project({o.x, o.y, o.z}, sx, sy)) rect(sx - 3.0f, sy - 3.0f, 6.0f, 6.0f, selected_shape);
    }
    return m_rgba;
}

Entity Viewport::pick(const Scene& scene, float x, float y) const {
    Entity best;
    float best_d = 14.0f * 14.0f;
    for (Entity e : scene.entities()) {
        const Vec4 o = scene.world_matrix(e) * Vec4{0.0f, 0.0f, 0.0f, 1.0f};
        float sx, sy;
        if (!project({o.x, o.y, o.z}, sx, sy)) continue;
        const float d = (sx - x) * (sx - x) + (sy - y) * (sy - y);
        if (d < best_d) {
            best_d = d;
            best = e;
        }
    }
    return best;
}

} // namespace orchard
