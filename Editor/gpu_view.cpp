#include "gpu_view.hpp"

#ifndef FIZMO
#define FIZMO
#endif
#ifndef FIZMO_GRAPHICS
#define FIZMO_GRAPHICS
#endif
#include <fizmo/includes.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>
#include <vector>

namespace orchard {
namespace editor {

namespace fg = fizmo::graphics;
using fizmo::colors::Color;
using fizmo::linalg::vector3d;

namespace {

fizmo::Axes3D fizmo_axes(Axes axes) {
    switch (axes) {
    case Axes::y_up_left: return fizmo::Axes3D::y_up_left();
    case Axes::z_up: return fizmo::Axes3D::z_up();
    case Axes::z_up_left: return fizmo::Axes3D::z_up_left();
    case Axes::y_up: break;
    }
    return fizmo::Axes3D::y_up();
}

fizmo::Axes3D fizmo_axes(const AxisSystem& system) {
    fizmo::Axes3D out = fizmo_axes(system.base());
    if (system.flipped(AxisFlip::x)) out.flip_x();
    if (system.flipped(AxisFlip::y)) out.flip_y();
    if (system.flipped(AxisFlip::z)) out.flip_z();
    return out;
}

const Color background_top(30, 33, 28);
const Color background_bottom(18, 20, 17);
const Color grid_minor(48, 54, 45);
const Color grid_major(70, 79, 66);
const Color axis_x(232, 96, 96);
const Color axis_y(143, 209, 106);
const Color axis_z(96, 150, 240);
const Color plain(176, 184, 168);
const Color dynamic_color(143, 209, 106);
const Color static_color(128, 134, 124);
const Color kinematic_color(110, 170, 235);
const Color loose_color(232, 176, 92);
const Color sensor_color(242, 201, 109);
const Color camera_color(139, 213, 202);
const Color light_color(242, 201, 109);
const Color selected_color(247, 180, 72);

bool named(const Scene& scene, Entity e, const char* what) {
    return scene.name(e).find(what) != std::string::npos;
}

Color with_alpha(Color c, std::uint8_t a) {
    return Color(c.red(), c.green(), c.blue(), a);
}

fizmo::linalg::Matrix4d to_fizmo(const Mat4& m) {
    return fizmo::linalg::Matrix4d{
        m.at(0, 0), m.at(0, 1), m.at(0, 2), m.at(0, 3),
        m.at(1, 0), m.at(1, 1), m.at(1, 2), m.at(1, 3),
        m.at(2, 0), m.at(2, 1), m.at(2, 2), m.at(2, 3),
        m.at(3, 0), m.at(3, 1), m.at(3, 2), m.at(3, 3)};
}

vector3d point(const Mat4& m, Vec3 p) {
    const Vec4 v = m * Vec4{p.x, p.y, p.z, 1.0f};
    return vector3d{v.x, v.y, v.z};
}

Color body_color(const Scene& scene, Entity e, const Collider& c) {
    if (c.sensor) return sensor_color;
    const Entity owner = scene.body_owner(e);
    const Body* b = scene.body(owner);
    if (!b) return loose_color;
    if (!b->enabled) return static_color;
    switch (b->type) {
    case BodyType::fixed: return static_color;
    case BodyType::kinematic: return kinematic_color;
    case BodyType::dynamic: return dynamic_color;
    }
    return plain;
}

Mat4 shape_matrix(const Mat4& world, const Collider& c) {
    Vec3 size = c.size;
    if (c.shape == ColliderShape::sphere) {
        const float d = std::max({c.size.x, c.size.y, c.size.z});
        size = {d, d, d};
    }
    return world * translation(c.offset) * scaling(size);
}

void box_edges(fg::Renderer& r, const Mat4& m, Color color, float width, bool depth) {
    vector3d p[8];
    for (int i = 0; i < 8; ++i) p[i] = point(m, {(i & 1) ? 0.5f : -0.5f, (i & 2) ? 0.5f : -0.5f, (i & 4) ? 0.5f : -0.5f});
    const int e[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (const auto& k : e) r.draw_line_3d(p[k[0]], p[k[1]], color, width, depth);
}

void ring_edges(fg::Renderer& r, const Mat4& m, Color color, float width, bool depth) {
    const int n = 32;
    for (int axis = 0; axis < 3; ++axis) {
        for (int i = 0; i < n; ++i) {
            const float a0 = 2.0f * pi * i / n, a1 = 2.0f * pi * (i + 1) / n;
            auto at = [axis](float a) {
                const float c = 0.5f * std::cos(a), s = 0.5f * std::sin(a);
                return axis == 0 ? Vec3{0.0f, c, s} : axis == 1 ? Vec3{c, 0.0f, s} : Vec3{c, s, 0.0f};
            };
            r.draw_line_3d(point(m, at(a0)), point(m, at(a1)), color, width, depth);
        }
    }
}

void add_face(fg::Mesh3D& mesh, Vec3 n, Vec3 a, Vec3 b, Vec3 c, Vec3 d) {
    const Color white(255, 255, 255);
    const vector3d nn{n.x, n.y, n.z};
    mesh.add_quad(fg::Vertex3D({a.x, a.y, a.z}, nn, white), fg::Vertex3D({b.x, b.y, b.z}, nn, white), fg::Vertex3D({c.x, c.y, c.z}, nn, white), fg::Vertex3D({d.x, d.y, d.z}, nn, white));
}

fg::Mesh3D make_cube() {
    fg::Mesh3D m;
    const float h = 0.5f;
    add_face(m, {0, 0, 1}, {-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h});
    add_face(m, {0, 0, -1}, {h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h});
    add_face(m, {1, 0, 0}, {h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h});
    add_face(m, {-1, 0, 0}, {-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h});
    add_face(m, {0, 1, 0}, {-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h});
    add_face(m, {0, -1, 0}, {-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h});
    return m;
}

fg::Mesh3D make_sphere(int rings, int segments, float stretch) {
    fg::Mesh3D m;
    const Color white(255, 255, 255);
    for (int i = 0; i <= rings; ++i) {
        const float v = static_cast<float>(i) / rings;
        const float phi = v * pi;
        for (int j = 0; j <= segments; ++j) {
            const float u = static_cast<float>(j) / segments;
            const float theta = u * 2.0f * pi;
            const Vec3 n{std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta)};
            Vec3 p = n * 0.5f;
            if (stretch > 0.0f) p.y += (n.y >= 0.0f ? stretch : -stretch);
            m.add_vertex(fg::Vertex3D(vector3d{p.x, p.y, p.z}, vector3d{n.x, n.y, n.z}, white, u, v));
        }
    }
    const int row = segments + 1;
    for (int i = 0; i < rings; ++i) {
        for (int j = 0; j < segments; ++j) {
            const auto a = static_cast<std::uint32_t>(i * row + j);
            const auto b = static_cast<std::uint32_t>(a + row);
            m.add_triangle(a, a + 1, b);
            m.add_triangle(a + 1, b + 1, b);
        }
    }
    return m;
}

fg::Material3D tinted(Color c, bool lit) {
    fg::Material3D mat = lit ? fg::Material3D::plastic(0.6f) : fg::Material3D::unlit();
    mat.emissive = c;
    mat.emissive_strength = 0.35f;
    mat.cull = fg::Cull3D::None;
    return mat;
}

} // namespace

struct GpuView::Meshes {
    fg::Mesh3D cube = make_cube();
    fg::Mesh3D sphere = make_sphere(16, 24, 0.0f);
    std::map<std::pair<const fg::Mesh3D*, std::uint32_t>, fg::Mesh3D> tinted;

    const fg::Mesh3D& tint(const fg::Mesh3D& base, Color c) {
        const std::uint32_t key = fg::Vertex3D::pack(c);
        auto it = tinted.find({&base, key});
        if (it != tinted.end()) return it->second;
        fg::Mesh3D m = base;
        for (fg::Vertex3D& v : m.edit_vertices()) v.set_color(c);
        return tinted.emplace(std::make_pair(&base, key), std::move(m)).first->second;
    }
};

GpuView::GpuView() : m_meshes(std::make_unique<Meshes>()) {}

GpuView::~GpuView() = default;

void GpuView::draw(grove::GpuCanvasFrame& f, const Scene& scene, Entity selected, Viewport& view, bool playing) {
    fg::Renderer& r = f.renderer;
    const unsigned w = static_cast<unsigned>(std::max(1.0, std::round(f.width)));
    const unsigned h = static_cast<unsigned>(std::max(1.0, std::round(f.height)));
    view.set_view_size(w, h);
    r.draw_rect(0, 0, w, h, fg::Paint::fill(view.dimension() == Dimension::three_d ? background_top : background_bottom));

    auto paint_mesh = [&](const fg::Mesh3D& mesh, const Mat4& model, Color color, std::uint8_t alpha) {
        const fg::Mesh3D& tinted_mesh = m_meshes->tint(mesh, with_alpha(color, alpha));
        fg::Material3D mat = tinted(color, true);
        if (alpha < 255) {
            mat.blend = fg::Blend3D::Alpha;
            mat.depth_write = false;
        }
        r.draw_mesh(tinted_mesh, to_fizmo(model), mat);
    };

    if (view.dimension() == Dimension::three_d) {
        const OrbitCamera& oc = view.orbit_camera();
        const AxisSystem& axes = view.axis_system();
        const Vec3 eye = view.eye();
        fg::Camera3D cam(vector3d{eye.x, eye.y, eye.z}, f.viewport_width, f.viewport_height, oc.fov, 0.05, 5000.0);
        cam.set_axes(fizmo_axes(axes));
        cam.look_at(vector3d{oc.target.x, oc.target.y, oc.target.z});
        auto wv = [&axes](float x, float y, float z) {
            const Vec3 p = axes.from_render({x, y, z});
            return vector3d{p.x, p.y, p.z};
        };
        if (r.begin_3d(cam, f.viewport_x, f.viewport_y, f.viewport_width, f.viewport_height)) {
            r.set_light_3d(fg::Light3D::sun(wv(-0.4f, -1.0f, -0.6f), 0.5f, 0.6f));
            float step = 1.0f;
            while (oc.distance / step > 40.0f) step *= 10.0f;
            const int lines = 20;
            const Vec3 target_r = axes.to_render(oc.target);
            const float cx = std::round(target_r.x / step) * step, cz = std::round(target_r.z / step) * step;
            const float extent = lines * step;
            for (int i = -lines; i <= lines; ++i) {
                const float x = cx + i * step, z = cz + i * step;
                const Color cxl = std::fabs(x) < step * 0.5f ? axis_z : (i % 5 == 0 ? grid_major : grid_minor);
                const Color czl = std::fabs(z) < step * 0.5f ? axis_x : (i % 5 == 0 ? grid_major : grid_minor);
                r.draw_line_3d(wv(x, 0.0f, cz - extent), wv(x, 0.0f, cz + extent), cxl, 1.0f, true);
                r.draw_line_3d(wv(cx - extent, 0.0f, z), wv(cx + extent, 0.0f, z), czl, 1.0f, true);
            }
            r.draw_line_3d(vector3d{0.0, 0.0, 0.0}, wv(0.0f, step * 2.0f, 0.0f), axis_y, 2.0f, true);

            for (Entity e : scene.entities()) {
                const Mat4 world = scene.world_matrix(e);
                const std::vector<Collider>* cs = scene.colliders(e);
                if (cs && !cs->empty()) {
                    for (const Collider& c : *cs) {
                        const Mat4 m = shape_matrix(world, c);
                        const Color col = body_color(scene, e, c);
                        if (c.sensor || !c.enabled) {
                            if (c.shape == ColliderShape::box) box_edges(r, m, with_alpha(col, c.enabled ? 255 : 110), 1.5f, true);
                            else ring_edges(r, m, with_alpha(col, c.enabled ? 255 : 110), 1.5f, true);
                            continue;
                        }
                        if (c.shape == ColliderShape::box) {
                            paint_mesh(m_meshes->cube, m, col, 255);
                        } else if (c.shape == ColliderShape::sphere) {
                            paint_mesh(m_meshes->sphere, m, col, 255);
                        } else {
                            const float rad = std::max(c.size.x, c.size.z);
                            const float body_h = std::max(0.0f, c.size.y - rad);
                            paint_mesh(m_meshes->sphere, world * translation(c.offset + Vec3{0.0f, body_h * 0.5f, 0.0f}) * scaling({rad, rad, rad}), col, 255);
                            paint_mesh(m_meshes->sphere, world * translation(c.offset - Vec3{0.0f, body_h * 0.5f, 0.0f}) * scaling({rad, rad, rad}), col, 255);
                            if (body_h > 0.0f) paint_mesh(m_meshes->cube, world * translation(c.offset) * scaling({rad * 0.85f, body_h, rad * 0.85f}), col, 255);
                        }
                    }
                } else if (named(scene, e, "Camera")) {
                    const vector3d o = point(world, {0, 0, 0});
                    const Vec3 tip[4] = {{-0.4f, -0.3f, -0.8f}, {0.4f, -0.3f, -0.8f}, {0.4f, 0.3f, -0.8f}, {-0.4f, 0.3f, -0.8f}};
                    for (int i = 0; i < 4; ++i) {
                        r.draw_line_3d(o, point(world, tip[i]), camera_color, 1.5f, true);
                        r.draw_line_3d(point(world, tip[i]), point(world, tip[(i + 1) % 4]), camera_color, 1.5f, true);
                    }
                } else if (named(scene, e, "Light")) {
                    const vector3d o = point(world, {0, 0, 0});
                    const Vec3 dirs[6] = {{0.35f, 0, 0}, {-0.35f, 0, 0}, {0, 0.35f, 0}, {0, -0.35f, 0}, {0, 0, 0.35f}, {0, 0, -0.35f}};
                    for (const Vec3& d : dirs) r.draw_line_3d(o, point(world, d), light_color, 2.0f, true);
                    const Vec3 ow{static_cast<float>(o.x), static_cast<float>(o.y), static_cast<float>(o.z)};
                    const Vec3 ground = ow - axes.up() * axes.height(ow);
                    r.draw_line_3d(o, vector3d{ground.x, ground.y, ground.z}, grid_major, 1.0f, true);
                } else {
                    paint_mesh(m_meshes->cube, world, plain, 255);
                }
            }

            if (scene.alive(selected) && !playing) {
                const Mat4 world = scene.world_matrix(selected);
                const std::vector<Collider>* cs = scene.colliders(selected);
                if (cs && !cs->empty()) {
                    for (const Collider& c : *cs) {
                        const Mat4 m = shape_matrix(world, c);
                        if (c.shape == ColliderShape::box) box_edges(r, m, selected_color, 2.0f, false);
                        else ring_edges(r, m, selected_color, 2.0f, false);
                    }
                } else {
                    box_edges(r, world * scaling({1.04f, 1.04f, 1.04f}), selected_color, 2.0f, false);
                }
            }
            r.end_3d();
        }
        return;
    }

    auto px = [&view](Vec3 p) {
        float x = 0.0f, y = 0.0f;
        view.project(p, x, y);
        return fg::RenderPoint(x, y);
    };
    const PanCamera& pc = view.pan_camera();
    float step = 1.0f;
    while (step * pc.pixels_per_unit < 16.0f) step *= 5.0f;
    const float half_w = w * 0.5f / pc.pixels_per_unit, half_h = h * 0.5f / pc.pixels_per_unit;
    const float left = pc.center.x - half_w, right = pc.center.x + half_w, bottom = pc.center.y - half_h, top = pc.center.y + half_h;
    auto line = [&r](fg::RenderPoint a, fg::RenderPoint b, Color c, unsigned width) {
        r.draw_line(static_cast<int>(std::lround(a.x)), static_cast<int>(std::lround(a.y)), static_cast<int>(std::lround(b.x)), static_cast<int>(std::lround(b.y)), fg::Paint::stroke(c, width));
    };
    for (float x = std::floor(left / step) * step; x <= right; x += step) {
        const bool major = std::fmod(std::fabs(x), step * 5.0f) < step * 0.5f;
        line(px({x, bottom, 0}), px({x, top, 0}), std::fabs(x) < step * 0.5f ? axis_y : major ? grid_major : grid_minor, 1);
    }
    for (float y = std::floor(bottom / step) * step; y <= top; y += step) {
        const bool major = std::fmod(std::fabs(y), step * 5.0f) < step * 0.5f;
        line(px({left, y, 0}), px({right, y, 0}), std::fabs(y) < step * 0.5f ? axis_x : major ? grid_major : grid_minor, 1);
    }
    auto local = [&px](const Mat4& m, float x, float y) {
        const vector3d v = point(m, {x, y, 0.0f});
        return px({static_cast<float>(v.x), static_cast<float>(v.y), 0.0f});
    };
    auto quad = [&](const Mat4& m, Color fill, Color edge, unsigned width) {
        const fg::RenderPoint pts[4] = {local(m, -0.5f, -0.5f), local(m, 0.5f, -0.5f), local(m, 0.5f, 0.5f), local(m, -0.5f, 0.5f)};
        if (fill.alpha() > 0) r.draw_polygon(pts, 4, fg::Paint::fill(fill));
        r.draw_polyline(pts, 4, fg::Paint::stroke(edge, width), true);
    };
    auto circle = [&](const Mat4& m, Color fill, Color edge, unsigned width) {
        const vector3d c = point(m, {0, 0, 0});
        const vector3d e = point(m, {0.5f, 0, 0});
        const fg::RenderPoint pc2 = px({static_cast<float>(c.x), static_cast<float>(c.y), 0});
        const fg::RenderPoint pe = px({static_cast<float>(e.x), static_cast<float>(e.y), 0});
        const float rr = std::hypot(pe.x - pc2.x, pe.y - pc2.y);
        const int cx = static_cast<int>(std::lround(pc2.x)), cy = static_cast<int>(std::lround(pc2.y));
        const unsigned rad = static_cast<unsigned>(std::max(1.0f, rr));
        if (fill.alpha() > 0) r.draw_circle(cx, cy, rad, fg::Paint::fill(fill));
        r.draw_circle(cx, cy, rad, fg::Paint::stroke(edge, width));
        line(pc2, pe, edge, width);
    };
    for (Entity e : scene.entities()) {
        const Mat4 world = scene.world_matrix(e);
        const bool sel = e == selected && !playing;
        const std::vector<Collider>* cs = scene.colliders(e);
        if (cs && !cs->empty()) {
            for (const Collider& c : *cs) {
                const Color col = body_color(scene, e, c);
                const Mat4 m = shape_matrix(world, c);
                const Color fill = c.sensor || !c.enabled ? Color(0, 0, 0, 0) : with_alpha(col, 120);
                const Color edge = sel ? selected_color : col;
                if (c.shape == ColliderShape::sphere) circle(m, fill, edge, sel ? 2u : 1u);
                else quad(m, fill, edge, sel ? 2u : 1u);
            }
        } else {
            const Color col = named(scene, e, "Camera") ? camera_color : plain;
            quad(world, with_alpha(col, 50), sel ? selected_color : col, sel ? 2u : 1u);
        }
    }
}

} // namespace editor
} // namespace orchard
