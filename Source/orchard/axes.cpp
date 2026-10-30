#include <orchard/axes.hpp>
#include <orchard/scene.hpp>

#include <cmath>

#include <array>
#include <cctype>
#include <utility>
#include <vector>

namespace orchard {

const char* to_string(Axes axes) {
    switch (axes) {
    case Axes::y_up: return "y-up";
    case Axes::y_up_left: return "y-up-left";
    case Axes::z_up: return "z-up";
    case Axes::z_up_left: return "z-up-left";
    }
    return "y-up";
}

std::optional<Axes> parse_axes(const std::string& text) {
    if (text == "y-up" || text == "y-up-right" || text == "opengl" || text == "godot") return Axes::y_up;
    if (text == "y-up-left" || text == "unity" || text == "directx") return Axes::y_up_left;
    if (text == "z-up" || text == "z-up-right" || text == "blender") return Axes::z_up;
    if (text == "z-up-left" || text == "unreal") return Axes::z_up_left;
    return std::nullopt;
}

const char* to_string(YAxis2D y) {
    return y == YAxis2D::down ? "down" : "up";
}

std::optional<YAxis2D> parse_y_axis(const std::string& text) {
    if (text == "up" || text == "y-up") return YAxis2D::up;
    if (text == "down" || text == "y-down") return YAxis2D::down;
    return std::nullopt;
}

bool mirrored(Axes axes) {
    return axes == Axes::y_up_left || axes == Axes::z_up_left;
}

Vec3 to_render(Axes axes, Vec3 w) {
    switch (axes) {
    case Axes::y_up: return w;
    case Axes::y_up_left: return {w.x, w.y, -w.z};
    case Axes::z_up: return {w.x, w.z, -w.y};
    case Axes::z_up_left: return {w.y, w.z, -w.x};
    }
    return w;
}

Vec3 from_render(Axes axes, Vec3 r) {
    switch (axes) {
    case Axes::y_up: return r;
    case Axes::y_up_left: return {r.x, r.y, -r.z};
    case Axes::z_up: return {r.x, -r.z, r.y};
    case Axes::z_up_left: return {-r.z, r.x, r.y};
    }
    return r;
}

Mat4 to_render_matrix(Axes axes) {
    const Vec3 x = to_render(axes, {1.0f, 0.0f, 0.0f});
    const Vec3 y = to_render(axes, {0.0f, 1.0f, 0.0f});
    const Vec3 z = to_render(axes, {0.0f, 0.0f, 1.0f});
    Mat4 m;
    m.at(0, 0) = x.x;
    m.at(1, 0) = x.y;
    m.at(2, 0) = x.z;
    m.at(0, 1) = y.x;
    m.at(1, 1) = y.y;
    m.at(2, 1) = y.z;
    m.at(0, 2) = z.x;
    m.at(1, 2) = z.y;
    m.at(2, 2) = z.z;
    return m;
}

Vec3 up_of(Axes axes) {
    return from_render(axes, {0.0f, 1.0f, 0.0f});
}

Vec3 forward_of(Axes axes) {
    return from_render(axes, {0.0f, 0.0f, -1.0f});
}

Vec3 right_of(Axes axes) {
    return from_render(axes, {1.0f, 0.0f, 0.0f});
}

float height_of(Axes axes, Vec3 p) {
    const Vec3 u = up_of(axes);
    return p.x * u.x + p.y * u.y + p.z * u.z;
}

Vec3 gravity_of(Axes axes, float g) {
    return up_of(axes) * -g;
}

Vec3 gravity_of(YAxis2D y, float g) {
    return {0.0f, y == YAxis2D::down ? g : -g, 0.0f};
}

namespace {

std::vector<std::string> words(const std::string& text) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : text) {
        if (c == ' ' || c == ',' || c == '+' || c == '\t' || c == '|') {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

std::optional<AxisFlip> flip_word(const std::string& w) {
    if (w == "x" || w == "flip-x" || w == "-x") return AxisFlip::x;
    if (w == "y" || w == "flip-y" || w == "-y") return AxisFlip::y;
    if (w == "z" || w == "flip-z" || w == "-z") return AxisFlip::z;
    return std::nullopt;
}

} // namespace

std::string to_string(AxisFlip flips) {
    std::string out;
    for (auto [bit, name] : {std::pair<AxisFlip, const char*>{AxisFlip::x, "x"}, {AxisFlip::y, "y"}, {AxisFlip::z, "z"}}) {
        if (!has_flip(flips, bit)) continue;
        if (!out.empty()) out += ' ';
        out += name;
    }
    return out.empty() ? std::string("none") : out;
}

std::optional<AxisFlip> parse_axis_flips(const std::string& text) {
    AxisFlip out = AxisFlip::none;
    for (const std::string& w : words(text)) {
        if (w == "none") continue;
        const std::optional<AxisFlip> f = flip_word(w);
        if (!f) return std::nullopt;
        out = out | *f;
    }
    return out;
}

Vec3 AxisSystem::flip_world(Vec3 v) const {
    if (flipped(AxisFlip::x)) v.x = -v.x;
    if (flipped(AxisFlip::y)) v.y = -v.y;
    if (flipped(AxisFlip::z)) v.z = -v.z;
    return v;
}

bool AxisSystem::mirrored() const {
    const int n = (flipped(AxisFlip::x) ? 1 : 0) + (flipped(AxisFlip::y) ? 1 : 0) + (flipped(AxisFlip::z) ? 1 : 0);
    return orchard::mirrored(m_base) != (n % 2 == 1);
}

Vec3 AxisSystem::to_render(Vec3 world) const {
    return orchard::to_render(m_base, flip_world(world));
}

Vec3 AxisSystem::from_render(Vec3 render) const {
    return flip_world(orchard::from_render(m_base, render));
}

Mat4 AxisSystem::to_render_matrix() const {
    const Vec3 x = to_render({1.0f, 0.0f, 0.0f});
    const Vec3 y = to_render({0.0f, 1.0f, 0.0f});
    const Vec3 z = to_render({0.0f, 0.0f, 1.0f});
    Mat4 m;
    m.at(0, 0) = x.x;
    m.at(1, 0) = x.y;
    m.at(2, 0) = x.z;
    m.at(0, 1) = y.x;
    m.at(1, 1) = y.y;
    m.at(2, 1) = y.z;
    m.at(0, 2) = z.x;
    m.at(1, 2) = z.y;
    m.at(2, 2) = z.z;
    return m;
}

Vec3 AxisSystem::up() const {
    return from_render({0.0f, 1.0f, 0.0f});
}

Vec3 AxisSystem::forward() const {
    return from_render({0.0f, 0.0f, -1.0f});
}

Vec3 AxisSystem::right() const {
    return from_render({1.0f, 0.0f, 0.0f});
}

float AxisSystem::height(Vec3 p) const {
    const Vec3 u = up();
    return p.x * u.x + p.y * u.y + p.z * u.z;
}

Vec3 AxisSystem::gravity(float g) const {
    return up() * -g;
}

bool AxisSystem::operator==(const AxisSystem& o) const {
    for (const Vec3 v : {Vec3{1.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f}, Vec3{0.0f, 0.0f, 1.0f}}) {
        const Vec3 a = to_render(v), b = o.to_render(v);
        if (a.x != b.x || a.y != b.y || a.z != b.z) return false;
    }
    return true;
}

std::string AxisSystem::name() const {
    for (Axes a : {Axes::y_up, Axes::y_up_left, Axes::z_up, Axes::z_up_left}) {
        if (*this == AxisSystem(a)) return to_string(a);
    }
    std::string out = to_string(m_base);
    if (flipped(AxisFlip::x)) out += " flip-x";
    if (flipped(AxisFlip::y)) out += " flip-y";
    if (flipped(AxisFlip::z)) out += " flip-z";
    return out;
}

std::optional<AxisSystem> AxisSystem::parse(const std::string& text) {
    const std::vector<std::string> ws = words(text);
    if (ws.empty()) return std::nullopt;
    AxisSystem out;
    std::size_t i = 0;
    if (ws[0] == "source") {
        out = source();
        i = 1;
    } else if (const std::optional<Axes> base = parse_axes(ws[0])) {
        out = AxisSystem(*base);
        i = 1;
    }
    for (; i < ws.size(); ++i) {
        const std::optional<AxisFlip> f = flip_word(ws[i]);
        if (!f) return std::nullopt;
        out.flip(*f);
    }
    return out;
}

namespace {

using M3 = std::array<std::array<float, 3>, 3>;

M3 mul(const M3& a, const M3& b) {
    M3 r{};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            float v = 0.0f;
            for (int k = 0; k < 3; ++k) v += a[i][k] * b[k][j];
            r[i][j] = v;
        }
    }
    return r;
}

M3 of_quat(Quat q) {
    const Mat4 r = rotation(q);
    M3 out{};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) out[i][j] = r.at(i, j);
    }
    return out;
}

Quat to_quat(const M3& m) {
    const float trace = m[0][0] + m[1][1] + m[2][2];
    Quat q;
    if (trace > 0.0f) {
        const float s = std::sqrt(trace + 1.0f) * 2.0f;
        q = {(m[2][1] - m[1][2]) / s, (m[0][2] - m[2][0]) / s, (m[1][0] - m[0][1]) / s, 0.25f * s};
    } else if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) {
        const float s = std::sqrt(1.0f + m[0][0] - m[1][1] - m[2][2]) * 2.0f;
        q = {0.25f * s, (m[0][1] + m[1][0]) / s, (m[0][2] + m[2][0]) / s, (m[2][1] - m[1][2]) / s};
    } else if (m[1][1] > m[2][2]) {
        const float s = std::sqrt(1.0f + m[1][1] - m[0][0] - m[2][2]) * 2.0f;
        q = {(m[0][1] + m[1][0]) / s, 0.25f * s, (m[1][2] + m[2][1]) / s, (m[0][2] - m[2][0]) / s};
    } else {
        const float s = std::sqrt(1.0f + m[2][2] - m[0][0] - m[1][1]) * 2.0f;
        q = {(m[0][2] + m[2][0]) / s, (m[1][2] + m[2][1]) / s, 0.25f * s, (m[1][0] - m[0][1]) / s};
    }
    return normalize(q);
}

M3 of_change(const AxisChange& c) {
    M3 out{};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) out[i][j] = c.m[i][j];
    }
    return out;
}

Vec3 apply_m3(const M3& m, Vec3 v) {
    return {m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z, m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z, m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z};
}

float det(const M3& m) {
    return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
}

} // namespace

AxisChange AxisChange::between(const AxisSystem& from, const AxisSystem& to) {
    AxisChange c;
    const Vec3 cols[3] = {to.from_render(from.to_render({1.0f, 0.0f, 0.0f})), to.from_render(from.to_render({0.0f, 1.0f, 0.0f})), to.from_render(from.to_render({0.0f, 0.0f, 1.0f}))};
    for (int j = 0; j < 3; ++j) {
        c.m[0][j] = cols[j].x;
        c.m[1][j] = cols[j].y;
        c.m[2][j] = cols[j].z;
    }
    return c;
}

AxisChange AxisChange::flips_2d(bool flip_x, bool flip_y) {
    AxisChange c;
    c.m[0][0] = flip_x ? -1.0f : 1.0f;
    c.m[1][1] = flip_y ? -1.0f : 1.0f;
    return c;
}

Vec3 AxisChange::apply(Vec3 v) const {
    return apply_m3(of_change(*this), v);
}

bool AxisChange::identity() const {
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (m[i][j] != (i == j ? 1.0f : 0.0f)) return false;
        }
    }
    return true;
}

bool AxisChange::mirrors() const {
    return det(of_change(*this)) < 0.0f;
}

void remap_scene(Scene& scene, const AxisChange& change) {
    if (change.identity()) return;
    const M3 m = of_change(change);
    M3 n{};
    n[0][0] = n[1][1] = n[2][2] = 1.0f;
    if (change.mirrors()) {
        bool diagonal = true;
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) diagonal = diagonal && (i == j || m[i][j] == 0.0f);
        }
        if (diagonal) n = m;
        else n[0][0] = -1.0f;
    }
    const bool reflect = change.mirrors();
    for (Entity e : scene.entities()) {
        Transform* t = scene.transform(e);
        if (!t) continue;
        const M3 r = of_quat(t->rotation);
        if (!scene.alive(scene.parent(e))) {
            t->position = apply_m3(m, t->position);
            t->rotation = to_quat(mul(mul(m, r), n));
        } else if (reflect) {
            t->position = apply_m3(n, t->position);
            t->rotation = to_quat(mul(mul(n, r), n));
        }
        if (!reflect) continue;
        if (std::vector<Collider>* cs = scene.colliders(e)) {
            for (Collider& c : *cs) c.offset = apply_m3(n, c.offset);
        }
    }
}

void remap_scene(Scene& scene, const AxisSystem& from, const AxisSystem& to) {
    remap_scene(scene, AxisChange::between(from, to));
}

} // namespace orchard
