#include "gpu_view.hpp"

#include <unordered_map>

#include <tuple>

#ifndef FIZMO
#define FIZMO
#endif
#ifndef FIZMO_GRAPHICS
#define FIZMO_GRAPHICS
#endif
#include <fizmo/includes.hpp>
#include <fizmo/../physics.hpp>

#include <algorithm>
#include <array>
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
const Color camera_color(80, 220, 255);
const Color particle_color(242, 165, 74);
const Color icon_outline(16, 18, 15);
const Color icon_body(235, 240, 230);
const Color light_color(242, 201, 109);
const Color selected_color(247, 180, 72);

bool named(const Scene& scene, Entity e, const char* what) {
    return scene.name(e).find(what) != std::string::npos;
}

Color with_alpha(Color c, std::uint8_t a) {
    return Color(c.red(), c.green(), c.blue(), a);
}

fizmo::text::TextStyle text_style(float size, Color c) {
    fizmo::text::TextStyle st;
    st.set_size(size);
    st.set_fg_color(c);
    return st;
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

Color light_icon_color(const Light& l, double seconds) {
    if (!l.enabled) return Color(90, 90, 80);
    const LightAnimationSample a = l.sample(seconds);
    const float m = std::max({a.color.x, a.color.y, a.color.z, 1e-4f});
    const float k = std::clamp(0.25f + 0.75f * std::min(a.brightness, 1.0f), 0.0f, 1.0f);
    auto ch = [&](float c) {
        const float v = std::pow(std::clamp(c / m, 0.0f, 1.0f), 1.0f / 2.2f) * k;
        return static_cast<std::uint8_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f));
    };
    return Color(ch(a.color.x), ch(a.color.y), ch(a.color.z));
}

std::uint8_t to_byte(float linear) {
    const float c = std::clamp(linear, 0.0f, 1.0f);
    const float s = c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
    return static_cast<std::uint8_t>(std::lround(std::clamp(s, 0.0f, 1.0f) * 255.0f));
}

Color material_color(const Material& m) {
    const float see = std::clamp(m.opacity, 0.0f, 1.0f) * (1.0f - 0.85f * std::clamp(m.transmission, 0.0f, 1.0f));
    return Color(to_byte(m.color.x), to_byte(m.color.y), to_byte(m.color.z), static_cast<std::uint8_t>(std::lround(std::clamp(see, 0.03f, 1.0f) * 255.0f)));
}

fg::Material3D material_3d(const Material& m) {
    fg::Material3D out = fg::Material3D::physical(std::clamp(m.metallic, 0.0f, 1.0f), std::clamp(m.roughness, 0.0f, 1.0f));
    out.cull = m.double_sided ? fg::Cull3D::None : fg::Cull3D::Back;
    out.reflectivity = std::clamp(std::max(m.reflectivity, m.metallic * (1.0f - m.roughness) * 0.6f), 0.0f, 1.0f);
    const float glow = std::max({m.emission.x, m.emission.y, m.emission.z}) * m.emission_strength;
    if (glow > 0.0f) {
        out.emissive = Color(to_byte(m.emission.x), to_byte(m.emission.y), to_byte(m.emission.z));
        out.emissive_strength = std::min(16.0f, m.emission_strength);
    }
    const Color c = material_color(m);
    if (c.alpha() < 255 || out.reflectivity > 0.02f) {
        out.blend = fg::Blend3D::Alpha;
        out.depth_write = c.alpha() >= 250;
    }
    out.shadow = m.casts_shadows ? fg::Shadow3D::Cast : fg::Shadow3D::None;
    return out;
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
    mesh.add_quad(fg::Vertex3D({a.x, a.y, a.z}, nn, white, 0.0f, 1.0f), fg::Vertex3D({b.x, b.y, b.z}, nn, white, 1.0f, 1.0f), fg::Vertex3D({c.x, c.y, c.z}, nn, white, 1.0f, 0.0f), fg::Vertex3D({d.x, d.y, d.z}, nn, white, 0.0f, 0.0f));
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

fg::Mesh3D make_cylinder(int segments, float top) {
    fg::Mesh3D m;
    const Color white(255, 255, 255);
    const float slope = 0.5f - top * 0.5f;
    const float len = std::sqrt(1.0f + slope * slope);
    for (int j = 0; j <= segments; ++j) {
        const float u = static_cast<float>(j) / segments;
        const float theta = u * 2.0f * pi;
        const float cx = std::cos(theta), cz = std::sin(theta);
        const vector3d n{cx / len, slope / len, cz / len};
        m.add_vertex(fg::Vertex3D(vector3d{cx * 0.5f, -0.5f, cz * 0.5f}, n, white, u, 0.0f));
        m.add_vertex(fg::Vertex3D(vector3d{cx * 0.5f * top, 0.5f, cz * 0.5f * top}, n, white, u, 1.0f));
    }
    for (int j = 0; j < segments; ++j) {
        const auto a = static_cast<std::uint32_t>(j * 2);
        m.add_triangle(a, a + 1, a + 2);
        m.add_triangle(a + 1, a + 3, a + 2);
    }
    for (int side = 0; side < 2; ++side) {
        const float y = side ? 0.5f : -0.5f;
        const float r = side ? top * 0.5f : 0.5f;
        if (r <= 0.0f) continue;
        const vector3d n{0.0f, side ? 1.0f : -1.0f, 0.0f};
        const auto center = static_cast<std::uint32_t>(m.vertices().size());
        m.add_vertex(fg::Vertex3D(vector3d{0.0f, y, 0.0f}, n, white, 0.5f, 0.5f));
        for (int j = 0; j <= segments; ++j) {
            const float theta = static_cast<float>(j) / segments * 2.0f * pi;
            m.add_vertex(fg::Vertex3D(vector3d{std::cos(theta) * r, y, std::sin(theta) * r}, n, white, 0.5f + 0.5f * std::cos(theta), 0.5f + 0.5f * std::sin(theta)));
        }
        for (int j = 0; j < segments; ++j) {
            const auto a = center + 1 + static_cast<std::uint32_t>(j);
            if (side) m.add_triangle(center, a + 1, a);
            else m.add_triangle(center, a, a + 1);
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
    fg::Mesh3D cylinder = make_cylinder(24, 1.0f);
    fg::Mesh3D cone = make_cylinder(24, 0.0f);
    fg::Mesh3D sphere_lod[2] = {make_sphere(8, 12, 0.0f), make_sphere(5, 8, 0.0f)};
    fg::Mesh3D cylinder_lod[2] = {make_cylinder(12, 1.0f), make_cylinder(8, 1.0f)};
    fg::Mesh3D cone_lod[2] = {make_cylinder(12, 0.0f), make_cylinder(8, 0.0f)};
    fg::Visibility3D vis;
    std::unordered_map<std::uint64_t, fg::Visibility3D::Handle> handles;
    std::unordered_map<std::uint64_t, std::uint64_t> seen;
    std::uint64_t frame = 0;
    std::map<std::pair<const fg::Mesh3D*, std::uint32_t>, fg::Mesh3D> tinted;
    std::unique_ptr<fg::GpuPathTracer> tracer;
    fg::Renderer* traced_with = nullptr;
    const fizmo::lighting::Scene* traced_scene = nullptr;
    fizmo::lighting::LightingSettings traced_settings;
    std::string trace_error;
    const fizmo::lighting::Scene* lights = nullptr;
    const std::vector<xylem::FluidDraw>* fluids = nullptr;
    std::vector<fg::Mesh3D> fluid_meshes;
    std::map<std::array<int, 5>, fg::Mesh3D> sections;
    fg::TextureLibrary textures;
    std::map<std::uint64_t, std::shared_ptr<fizmo::images::BitmapImage>> fluid_images;
    std::map<std::uint64_t, fg::Texture> fluid_textures;
    std::string texture_folder;

    const fg::Texture* fluid_texture(const xylem::FluidDraw& d) {
        if (d.image.empty() || d.image_width <= 0 || d.image_height <= 0) return nullptr;
        const std::size_t count = static_cast<std::size_t>(d.image_width) * static_cast<std::size_t>(d.image_height);
        if (d.image.size() < count * 4) return nullptr;
        std::shared_ptr<fizmo::images::BitmapImage>& img = fluid_images[d.entity.id()];
        if (!img || img->width() != static_cast<unsigned>(d.image_width) || img->height() != static_cast<unsigned>(d.image_height)) {
            img = std::make_shared<fizmo::images::BitmapImage>(static_cast<unsigned>(d.image_width), static_cast<unsigned>(d.image_height));
            fluid_textures[d.entity.id()] = fg::Texture(std::shared_ptr<const fizmo::images::BitmapImage>(img), d.level == FluidLevel::blocky ? fg::SampleFilter::Nearest : fg::SampleFilter::Bilinear);
        }
        std::vector<Color>& px = img->mutable_pixels();
        for (std::size_t i = 0; i < count && i < px.size(); ++i) px[i] = Color(d.image[i * 4], d.image[i * 4 + 1], d.image[i * 4 + 2], d.image[i * 4 + 3]);
        return &fluid_textures[d.entity.id()];
    }
    struct TextureSlot {
        std::shared_ptr<const fg::Texture> texture;
        std::uint64_t retry_at = 0;
    };
    std::map<std::string, TextureSlot> loaded;
    std::uint64_t texture_frame = 0;

    std::string texture_path(const std::string& file) const {
        if (file.empty()) return file;
        const bool absolute = file[0] == '/' || file[0] == '\\' || (file.size() > 1 && file[1] == ':');
        if (absolute || texture_folder.empty()) return file;
        return texture_folder + "/" + file;
    }

    const fg::Texture* texture(const std::string& file, fg::TextureRole role, bool pixelated) {
        if (file.empty()) return nullptr;
        const std::string key = std::to_string(static_cast<int>(role)) + (pixelated ? "p|" : "s|") + file;
        TextureSlot& slot = loaded[key];
        if (!slot.texture && slot.retry_at <= texture_frame) {
            fg::TextureSettings settings = fg::TextureSettings::for_role(role);
            if (pixelated) settings.with_filter(fg::SampleFilter::Nearest).with_mipmaps(false);
            slot.texture = textures.load(texture_path(file), settings);
            if (!slot.texture) slot.retry_at = texture_frame + 240;
        }
        return slot.texture.get();
    }

    const fg::Texture* packed(const std::string& rough, const std::string& metal, bool pixelated) {
        const std::string key = std::string("orm") + (pixelated ? "p|" : "s|") + rough + "|" + metal;
        TextureSlot& slot = loaded[key];
        if (slot.texture || slot.retry_at > texture_frame) return slot.texture.get();
        const fg::Texture* r = texture(rough, fg::TextureRole::Roughness, pixelated);
        const fg::Texture* m = texture(metal, fg::TextureRole::Metallic, pixelated);
        if ((!rough.empty() && !r) || (!metal.empty() && !m)) {
            slot.retry_at = texture_frame + 240;
            return nullptr;
        }
        fg::TextureSettings settings = fg::TextureSettings::data();
        if (pixelated) settings.with_filter(fg::SampleFilter::Nearest).with_mipmaps(false);
        slot.texture = textures.add(key, fg::TextureLibrary::pack_orm(nullptr, r ? r->image() : nullptr, m ? m->image() : nullptr), settings);
        return slot.texture.get();
    }

    void apply_textures(const Material& m, fg::Material3D& out) {
        if (!m.has_textures()) return;
        const bool px = m.pixelated;
        out.texture = texture(m.base_map, fg::TextureRole::Color, px);
        if (const fg::Texture* t = texture(m.normal_map, fg::TextureRole::Normal, px)) out.with_normal_map(*t, m.normal_strength);
        if (const fg::Texture* t = texture(m.orm_map, fg::TextureRole::Packed, px)) out.with_orm_map(*t, true);
        else if (!m.roughness_map.empty() || !m.metallic_map.empty()) {
            if (const fg::Texture* t = packed(m.roughness_map, m.metallic_map, px)) {
                out.with_orm_map(*t, false);
                if (!m.roughness_map.empty()) out.roughness = 1.0f;
                if (!m.metallic_map.empty()) out.metallic = 1.0f;
            }
        }
        if (const fg::Texture* t = texture(m.emission_map, fg::TextureRole::Emissive, px)) {
            out.with_emissive_map(*t);
            if (out.emissive_strength <= 0.0f || (out.emissive.red() == 0 && out.emissive.green() == 0 && out.emissive.blue() == 0)) {
                out.emissive = Color(255, 255, 255);
                out.emissive_strength = std::max(1.0f, std::min(16.0f, m.emission_strength));
            }
        }
        if (const fg::Texture* t = texture(m.opacity_map, fg::TextureRole::Opacity, px)) {
            out.with_opacity_map(*t);
            if (m.alpha_cutoff <= 0.0f) {
                out.blend = fg::Blend3D::Alpha;
                out.depth_write = false;
            }
        }
        if (m.alpha_cutoff > 0.0f) {
            out.with_alpha_cutoff(std::clamp(m.alpha_cutoff, 0.0f, 1.0f));
            out.blend = fg::Blend3D::Opaque;
            out.depth_write = true;
        }
        out.with_uv(fg::UvTransform::make(m.tiling_u, m.tiling_v, m.offset_u, m.offset_v, m.uv_rotation * pi / 180.0f));
        if (m.triplanar) out.with_triplanar(1.0f / std::max(0.01f, m.triplanar_scale), 4.0f);
    }

    void tick_textures() {
        ++texture_frame;
        if (texture_frame % 90 == 0 && textures.size() > 0) textures.reload_changed();
    }

    const fg::Mesh3D& section(const Collider& c) {
        const std::array<int, 5> key{static_cast<int>(std::lround(c.polar_min * 10.0f)), static_cast<int>(std::lround(c.polar_max * 10.0f)), static_cast<int>(std::lround(c.azimuth_min * 10.0f)), static_cast<int>(std::lround(c.azimuth_max * 10.0f)), static_cast<int>(std::lround(c.shell * 1000.0f / std::max({c.size.x, c.size.y, c.size.z, 1e-4f})))};
        auto it = sections.find(key);
        if (it != sections.end()) return it->second;
        fizmo::physics::SphereSection3D sec;
        sec.radius = 0.5f;
        sec.polar_min = std::clamp(c.polar_min, 0.0f, 180.0f);
        sec.polar_max = std::clamp(c.polar_max, sec.polar_min, 180.0f);
        sec.azimuth_min = c.azimuth_min;
        sec.azimuth_max = std::min(std::max(c.azimuth_max, c.azimuth_min), c.azimuth_min + 360.0f);
        const float d = std::max({c.size.x, c.size.y, c.size.z, 1e-4f});
        sec.inner_radius = c.shell > 0.0f ? std::max(0.0f, 0.5f - c.shell / d) : 0.0f;
        fg::Mesh3D m;
        const Color white(255, 255, 255);
        for (const auto& t : fizmo::physics::sphere_section_triangles(sec, 32)) {
            const fizmo::physics::RVec3 e1 = t[1] - t[0], e2 = t[2] - t[0];
            fizmo::physics::RVec3 n{e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x};
            const float l = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
            if (l <= 1e-12f) continue;
            n = n * (1.0f / l);
            const auto base = static_cast<std::uint32_t>(m.vertices().size());
            for (int k = 0; k < 3; ++k) {
                const fizmo::physics::RVec3& p = t[static_cast<std::size_t>(k)];
                const float pl = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
                vector3d nn{n.x, n.y, n.z};
                if (pl > 1e-6f && std::fabs(pl - 0.5f) < 0.02f && (p.x * n.x + p.y * n.y + p.z * n.z) > 0.0f) nn = vector3d{p.x / pl, p.y / pl, p.z / pl};
                m.add_vertex(fg::Vertex3D(vector3d{p.x, p.y, p.z}, nn, white));
            }
            m.add_triangle(base, base + 1, base + 2);
        }
        return sections.emplace(key, std::move(m)).first->second;
    }

    const fg::Mesh3D& tint(const fg::Mesh3D& base, Color c) {
        const std::uint32_t key = fg::Vertex3D::pack(c);
        auto it = tinted.find({&base, key});
        if (it != tinted.end()) return it->second;
        fg::Mesh3D m = base;
        for (fg::Vertex3D& v : m.edit_vertices()) v.set_color(c);
        return tinted.emplace(std::make_pair(&base, key), std::move(m)).first->second;
    }
};

GpuView::GpuView() : m_meshes(std::make_unique<Meshes>()) {
    Meshes* mm = m_meshes.get();
    m_particles.set_texture_lookup([mm](const std::string& file) { return mm->texture(file, fg::TextureRole::Color, false); });
}

void GpuView::set_lights(const fizmo::lighting::Scene* lights) {
    m_meshes->lights = lights;
}

void GpuView::set_fluids(const std::vector<xylem::FluidDraw>* fluids) {
    m_meshes->fluids = fluids;
}

std::function<std::shared_ptr<const fizmo::lighting::Texture>(const std::string& path, bool color)> GpuView::lighting_textures() {
    Meshes* mm = m_meshes.get();
    return [mm](const std::string& path, bool color) -> std::shared_ptr<const fizmo::lighting::Texture> {
        if (path.empty()) return nullptr;
        return mm->textures.load_lighting(mm->texture_path(path), color ? fg::TextureSettings::color() : fg::TextureSettings::data());
    };
}

void GpuView::set_texture_folder(const std::string& folder) {
    if (m_meshes->texture_folder == folder) return;
    m_meshes->texture_folder = folder;
    m_meshes->loaded.clear();
    m_meshes->textures.clear();
}

GpuView::~GpuView() {
    if (m_meshes && m_meshes->tracer) static_cast<void>(m_meshes->tracer.release());
}

void GpuView::draw(grove::GpuCanvasFrame& f, const Scene& scene, Entity selected, Viewport& view, bool playing) {
    draw(f, scene, selected, view, playing, 0.0);
}

void GpuView::draw(grove::GpuCanvasFrame& f, const Scene& scene, Entity selected, Viewport& view, bool playing, double seconds) {
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

    auto paint_cast = [&](const fg::Mesh3D& mesh, const Mat4& model, Color color, bool cast) {
        const fg::Mesh3D& tinted_mesh = m_meshes->tint(mesh, color);
        fg::Material3D mat = tinted(color, true);
        mat.shadow = cast ? fg::Shadow3D::Cast : fg::Shadow3D::None;
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
            bool any_reflective = false;
            if (m_options.materials) {
                m_meshes->tick_textures();
                for (Entity e : scene.entities()) {
                    const Material* mm = scene.material(e);
                    if (mm && (mm->reflectivity > 0.02f || mm->metallic * (1.0f - mm->roughness) > 0.05f || mm->transmission > 0.0f || mm->opacity < 1.0f)) any_reflective = true;
                }
            }
            if (m_options.shadows || (m_options.scene_lights && m_meshes->lights) || any_reflective) {
                fg::SunShadow3D sun = m_options.shadows ? fg::SunShadow3D::medium() : fg::SunShadow3D::off();
                sun.distance = std::max(1.0, m_options.shadow_distance);
                fg::SceneLighting3D lighting = fg::SceneLighting3D::daylight(sun);
                if (any_reflective) {
                    lighting.surfaces.screen_reflections = true;
                    lighting.surfaces.reflection_steps = 24;
                    lighting.surfaces.reflection_distance = static_cast<float>(std::max(16.0, static_cast<double>(view.orbit_camera().distance) * 4.0));
                }
                if (m_options.scene_lights && m_meshes->lights) {
                    for (const fizmo::lighting::Light& l : m_meshes->lights->lights()) {
                        if (!l.enabled || l.shape == fizmo::lighting::LightShape::directional) continue;
                        if (lighting.point_lights.size() >= fg::SceneLighting3D::MAX_POINT_LIGHTS) break;
                        fg::PointLight3D p = fg::to_point_light(l);
                        p.casts_shadows = p.casts_shadows && m_options.shadows;
                        lighting.point_lights.push_back(p);
                    }
                }
                lighting.sun_direction = wv(-0.4f, -1.0f, -0.6f);
                lighting.atmosphere.sun_position = wv(0.4f, 1.0f, 0.6f);
                lighting.atmosphere.moon_position = wv(-0.4f, -1.0f, -0.6f);
                lighting.atmosphere.up = wv(0.0f, 1.0f, 0.0f);
                r.set_scene_lighting(lighting);
            } else {
                r.set_scene_lighting(fg::SceneLighting3D::off());
            }
            float step = 1.0f;
            while (oc.distance / step > 40.0f) step *= 10.0f;
            const int lines = 20;
            const Vec3 target_r = axes.to_render(oc.target);
            const float cx = std::round(target_r.x / step) * step, cz = std::round(target_r.z / step) * step;
            const float extent = lines * step;
            for (int i = -lines; i <= lines; ++i) {
                const float x = cx + i * step, z = cz + i * step;
                const Color line_color = i % 5 == 0 ? grid_major : grid_minor;
                if (std::fabs(x) >= step * 0.5f) r.draw_line_3d(wv(x, 0.0f, cz - extent), wv(x, 0.0f, cz + extent), line_color, 1.0f, true);
                if (std::fabs(z) >= step * 0.5f) r.draw_line_3d(wv(cx - extent, 0.0f, z), wv(cx + extent, 0.0f, z), line_color, 1.0f, true);
            }
            {
                const Vec3 world_x = axes.from_render({1.0f, 0.0f, 0.0f});
                const Vec3 world_z = axes.from_render({0.0f, 0.0f, 1.0f});
                const Color render_x = std::fabs(world_x.x) > 0.5f ? axis_x : (std::fabs(world_x.y) > 0.5f ? axis_y : axis_z);
                const Color render_z = std::fabs(world_z.x) > 0.5f ? axis_x : (std::fabs(world_z.y) > 0.5f ? axis_y : axis_z);
                const Color render_y = [&] {
                    const Vec3 u = axes.from_render({0.0f, 1.0f, 0.0f});
                    return std::fabs(u.x) > 0.5f ? axis_x : (std::fabs(u.y) > 0.5f ? axis_y : axis_z);
                }();
                const float lift = step * 0.004f;
                const float reach = extent * 1.5f;
                const float height = std::max(step * 3.0f, oc.distance * 0.35f);
                auto origin_axis = [&](vector3d a, vector3d b, Color c) {
                    r.draw_line_3d(a, b, with_alpha(c, 70), 2.0f, false);
                    r.draw_line_3d(a, b, c, 3.0f, true);
                };
                origin_axis(wv(-reach, lift, 0.0f), wv(reach, lift, 0.0f), render_x);
                origin_axis(wv(0.0f, lift, -reach), wv(0.0f, lift, reach), render_z);
                origin_axis(wv(0.0f, 0.0f, 0.0f), wv(0.0f, height, 0.0f), render_y);
            }

            enum class Kind { cube, sphere, cylinder, cone, section };
            struct Item {
                Kind kind;
                Mat4 m;
                Color color;
                std::uint64_t key;
                const Material* mat = nullptr;
                const fg::Mesh3D* custom = nullptr;
            };
            std::vector<Item> items;
            struct Icon {
                vector3d at;
                int kind;
                Color color;
                bool selected;
            };
            std::vector<Icon> icons;
            for (Entity e : scene.entities()) {
                const Mat4 world = scene.world_matrix(e);
                const std::uint64_t base = static_cast<std::uint64_t>(e.id()) << 20;
                const std::vector<Collider>* cs = scene.colliders(e);
                if (cs && !cs->empty()) {
                    for (std::size_t ci = 0; ci < cs->size(); ++ci) {
                        const Collider& c = (*cs)[ci];
                        const Mat4 m = shape_matrix(world, c);
                        const Material* mat = m_options.materials ? scene.material(e) : nullptr;
                        if (mat && !mat->visible) continue;
                        const Color col = mat ? material_color(*mat) : body_color(scene, e, c);
                        const std::uint64_t key = base | (static_cast<std::uint64_t>(ci & 0xffff) << 4);
                        if (c.sensor || !c.enabled) {
                            if (c.shape == ColliderShape::box || c.shape == ColliderShape::rounded_box) box_edges(r, m, with_alpha(col, c.enabled ? 255 : 110), 1.5f, true);
                            else ring_edges(r, m, with_alpha(col, c.enabled ? 255 : 110), 1.5f, true);
                            continue;
                        }
                        if (c.shape == ColliderShape::box || c.shape == ColliderShape::rounded_box) {
                            items.push_back({Kind::cube, m, col, key, mat});
                        } else if (c.shape == ColliderShape::sphere) {
                            items.push_back({Kind::sphere, m, col, key, mat});
                        } else if (c.shape == ColliderShape::sphere_section) {
                            const float d = std::max({c.size.x, c.size.y, c.size.z});
                            items.push_back({Kind::section, world * translation(c.offset) * scaling({d, d, d}), col, key, mat, &m_meshes->section(c)});
                        } else if (c.shape == ColliderShape::cylinder || c.shape == ColliderShape::cone) {
                            const float rad = std::max(c.size.x, c.size.z);
                            items.push_back({c.shape == ColliderShape::cone ? Kind::cone : Kind::cylinder, world * translation(c.offset) * scaling({rad, c.size.y, rad}), col, key, mat});
                        } else {
                            const float rad = std::max(c.size.x, c.size.z);
                            const float body_h = std::max(0.0f, c.size.y - rad);
                            items.push_back({Kind::sphere, world * translation(c.offset + Vec3{0.0f, body_h * 0.5f, 0.0f}) * scaling({rad, rad, rad}), col, key | 1, mat});
                            items.push_back({Kind::sphere, world * translation(c.offset - Vec3{0.0f, body_h * 0.5f, 0.0f}) * scaling({rad, rad, rad}), col, key | 2, mat});
                            if (body_h > 0.0f) items.push_back({Kind::cube, world * translation(c.offset) * scaling({rad * 0.85f, body_h, rad * 0.85f}), col, key | 3, mat});
                        }
                    }
                } else if (named(scene, e, "Camera")) {
                    if (playing) continue;
                    const vector3d o = point(world, {0, 0, 0});
                    const Vec3 tip[4] = {{-0.5f, -0.32f, -0.9f}, {0.5f, -0.32f, -0.9f}, {0.5f, 0.32f, -0.9f}, {-0.5f, 0.32f, -0.9f}};
                    for (int pass = 0; pass < 2; ++pass) {
                        const Color c = pass == 0 ? with_alpha(camera_color, 80) : camera_color;
                        const float lw = pass == 0 ? 1.5f : 2.5f;
                        for (int i = 0; i < 4; ++i) {
                            r.draw_line_3d(o, point(world, tip[i]), c, lw, pass == 1);
                            r.draw_line_3d(point(world, tip[i]), point(world, tip[(i + 1) % 4]), c, lw, pass == 1);
                        }
                        r.draw_line_3d(point(world, {-0.18f, 0.36f, -0.9f}), point(world, {0.0f, 0.55f, -0.9f}), c, lw, pass == 1);
                        r.draw_line_3d(point(world, {0.0f, 0.55f, -0.9f}), point(world, {0.18f, 0.36f, -0.9f}), c, lw, pass == 1);
                    }
                    icons.push_back({o, 1, camera_color, e == selected});
                } else if (scene.light(e) || named(scene, e, "Light")) {
                    const vector3d o = point(world, {0, 0, 0});
                    Color icon = light_color;
                    if (const Light* l = scene.light(e)) icon = light_icon_color(*l, seconds);
                    if (!playing) {
                        const Vec3 ow{static_cast<float>(o.x), static_cast<float>(o.y), static_cast<float>(o.z)};
                        const Vec3 ground = ow - axes.up() * axes.height(ow);
                        r.draw_line_3d(o, vector3d{ground.x, ground.y, ground.z}, with_alpha(icon, 60), 1.0f, false);
                        r.draw_line_3d(o, vector3d{ground.x, ground.y, ground.z}, with_alpha(icon, 160), 1.5f, true);
                        if (const Light* l = scene.light(e)) {
                            if (l->type == LightType::spot || l->type == LightType::directional) {
                                const vector3d d = point(world, axes.forward() * 1.2f);
                                r.draw_line_3d(o, d, with_alpha(icon, 200), 2.0f, false);
                            }
                        }
                        icons.push_back({o, 0, icon, e == selected});
                    }
                } else if (scene.emitter(e)) {
                    if (!playing) icons.push_back({point(world, {0, 0, 0}), 2, particle_color, e == selected});
                } else if (!scene.fluid(e)) {
                    items.push_back({Kind::cube, world, plain, base | 0xf});
                }
            }

            auto column = [](const Mat4& m, int c) { return Vec3{m.at(0, c), m.at(1, c), m.at(2, c)}; };
            auto aligned = [&](const Mat4& m) {
                for (int c = 0; c < 3; ++c) {
                    const Vec3 v = column(m, c);
                    const float big = std::max({std::fabs(v.x), std::fabs(v.y), std::fabs(v.z)});
                    const float sum = std::fabs(v.x) + std::fabs(v.y) + std::fabs(v.z);
                    if (sum - big > 1e-4f * std::max(big, 1e-6f)) return false;
                }
                return true;
            };
            auto uniform_scale = [&](const Mat4& m, float& scale) {
                const float a = orchard::length(column(m, 0)), b = orchard::length(column(m, 1)), c = orchard::length(column(m, 2));
                scale = a;
                return std::fabs(a - b) <= 1e-4f * std::max(a, 1e-6f) && std::fabs(a - c) <= 1e-4f * std::max(a, 1e-6f);
            };
            auto bounds_of = [&](const Mat4& m) {
                vector3d lo{1e30, 1e30, 1e30}, hi{-1e30, -1e30, -1e30};
                for (int i = 0; i < 8; ++i) {
                    const vector3d p = point(m, {(i & 1) ? 0.5f : -0.5f, (i & 2) ? 0.5f : -0.5f, (i & 4) ? 0.5f : -0.5f});
                    lo = vector3d{std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
                    hi = vector3d{std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
                }
                return fg::VisBox3D(lo, hi);
            };

            Meshes& mesh = *m_meshes;
            m_stats = GpuViewStats{};
            m_stats.items = items.size();
            std::vector<fg::Visibility3D::Handle> handle(items.size(), fg::Visibility3D::INVALID);
            const bool culling = m_options.culling;
            if (culling) {
                ++mesh.frame;
                const Vec3 up = axes.up();
                fg::VisibilitySettings3D vs = mesh.vis.settings();
                vs.occlusion = m_options.occlusion;
                vs.up_axis = std::fabs(up.x) > 0.5f ? 0 : (std::fabs(up.z) > 0.5f ? 2 : 1);
                vs.max_distance = 5000.0;
                vs.min_screen_pixels = 0.75;
                vs.shadows.enabled = m_options.shadows;
                vs.shadows.real_distance = m_options.shadow_distance;
                vs.shadows.blob_distance = std::max(m_options.blob_distance, m_options.shadow_distance);
                const int up_axis = vs.up_axis;
                vs.ground = [up_axis](const vector3d& from, const vector3d&, double reach, vector3d& hit, vector3d& normal) {
                    const double height = up_axis == 0 ? from.x : (up_axis == 2 ? from.z : from.y);
                    if (height < -1e-6 || height > reach) return false;
                    hit = from;
                    normal = vector3d{0.0, 0.0, 0.0};
                    if (up_axis == 0) { hit.x = 0.0; normal.x = 1.0; }
                    else if (up_axis == 2) { hit.z = 0.0; normal.z = 1.0; }
                    else { hit.y = 0.0; normal.y = 1.0; }
                    return true;
                };
                vs.reuse_move = 0.0;
                mesh.vis.set_settings(vs);
                const fg::LodLevels3D lods = m_options.lod ? fg::LodLevels3D::of({40.0, 120.0}) : fg::LodLevels3D::none();
                for (std::size_t i = 0; i < items.size(); ++i) {
                    const Item& it = items[i];
                    const fg::VisBox3D box = bounds_of(it.m);
                    const bool solid = it.kind == Kind::cube && aligned(it.m);
                    auto found = mesh.handles.find(it.key);
                    fg::Visibility3D::Handle h;
                    if (found == mesh.handles.end()) {
                        h = mesh.vis.add(box, fg::VIS_CASTER, it.kind == Kind::cube ? fg::LodLevels3D::none() : lods, it.key);
                        mesh.handles.emplace(it.key, h);
                    } else {
                        h = found->second;
                        mesh.vis.update(h, box);
                        mesh.vis.set_lods(h, it.kind == Kind::cube ? fg::LodLevels3D::none() : lods);
                    }
                    mesh.vis.set_flags(h, fg::VIS_CASTER | (solid ? fg::VIS_OCCLUDER : 0u));
                    if (solid) mesh.vis.set_occluder_box(h, box);
                    mesh.seen[it.key] = mesh.frame;
                    handle[i] = h;
                }
                for (auto it = mesh.handles.begin(); it != mesh.handles.end();) {
                    if (mesh.seen[it->first] != mesh.frame) {
                        mesh.vis.remove(it->second);
                        mesh.seen.erase(it->first);
                        it = mesh.handles.erase(it);
                    } else {
                        ++it;
                    }
                }
                mesh.vis.cull(cam);
                m_stats.occluded = mesh.vis.stats().occluded;
                m_stats.real_shadows = mesh.vis.stats().real_shadows;
                m_stats.blob_shadows = mesh.vis.stats().blob_shadows;
            }

            struct BatchKey {
                const fg::Mesh3D* mesh;
                std::uint32_t color;
                bool cast;
                bool operator<(const BatchKey& o) const { return std::tie(mesh, color, cast) < std::tie(o.mesh, o.color, o.cast); }
            };
            std::map<BatchKey, std::pair<Color, std::vector<fg::Instance3D>>> batches;
            for (std::size_t i = 0; i < items.size(); ++i) {
                const Item& it = items[i];
                int lod = 0;
                bool cast = true;
                if (culling) {
                    if (!mesh.vis.visible(handle[i])) {
                        ++m_stats.culled;
                        continue;
                    }
                    lod = std::clamp(mesh.vis.lod(handle[i]), 0, 2);
                    cast = !m_options.shadows || mesh.vis.shadow(handle[i]) == fg::ShadowMode3D::Real;
                }
                const fg::Mesh3D* m = &mesh.cube;
                if (it.kind == Kind::sphere) m = lod == 0 ? &mesh.sphere : &mesh.sphere_lod[lod - 1];
                else if (it.kind == Kind::cylinder) m = lod == 0 ? &mesh.cylinder : &mesh.cylinder_lod[lod - 1];
                else if (it.kind == Kind::cone) m = lod == 0 ? &mesh.cone : &mesh.cone_lod[lod - 1];
                else if (it.kind == Kind::section && it.custom) m = it.custom;
                ++m_stats.drawn;
                if (it.mat) {
                    fg::Material3D mat = material_3d(*it.mat);
                    if (m_options.materials) m_meshes->apply_textures(*it.mat, mat);
                    if (!cast) mat.shadow = fg::Shadow3D::None;
                    r.draw_mesh(m_meshes->tint(*m, it.color), to_fizmo(it.m), mat);
                    continue;
                }
                float scale = 1.0f;
                const bool instanceable = m_options.batching && uniform_scale(it.m, scale) && (it.kind == Kind::sphere || (it.kind == Kind::cube && aligned(it.m)));
                if (instanceable) {
                    const vector3d c = point(it.m, {0.0f, 0.0f, 0.0f});
                    auto& batch = batches[{m, fg::Vertex3D::pack(it.color), cast}];
                    batch.first = it.color;
                    batch.second.emplace_back(static_cast<float>(c.x), static_cast<float>(c.y), static_cast<float>(c.z), scale, it.color);
                    ++m_stats.instanced;
                } else {
                    paint_cast(*m, it.m, it.color, cast);
                }
            }
            for (const auto& kv : batches) {
                fg::Material3D mat = tinted(kv.second.first, true);
                mat.shadow = kv.first.cast ? fg::Shadow3D::Cast : fg::Shadow3D::None;
                r.draw_instances(*kv.first.mesh, vector3d{0.0, 0.0, 0.0}, kv.second.second, mat);
                ++m_stats.batches;
            }
            if (culling && m_options.shadows) {
                const Vec3 up = axes.up();
                for (const fg::BlobShadow3D& b : mesh.vis.blobs()) {
                    const float d = static_cast<float>(b.radius * 2.0);
                    const Vec3 size{std::fabs(up.x) > 0.5f ? 0.01f : d, std::fabs(up.y) > 0.5f ? 0.01f : d, std::fabs(up.z) > 0.5f ? 0.01f : d};
                    const Vec3 at = Vec3{static_cast<float>(b.center.x), static_cast<float>(b.center.y), static_cast<float>(b.center.z)} + up * 0.012f;
                    fg::Material3D mat = fg::Material3D::unlit();
                    mat.blend = fg::Blend3D::Alpha;
                    mat.depth_write = false;
                    mat.cull = fg::Cull3D::None;
                    const Color shade(0, 0, 0, static_cast<std::uint8_t>(std::clamp(b.opacity, 0.0f, 1.0f) * 255.0f));
                    r.draw_mesh(mesh.tint(mesh.sphere_lod[0], shade), to_fizmo(translation(at) * scaling(size)), mat);
                }
            }

            if (scene.alive(selected) && !playing) {
                const Mat4 world = scene.world_matrix(selected);
                const std::vector<Collider>* cs = scene.colliders(selected);
                if (cs && !cs->empty()) {
                    for (const Collider& c : *cs) {
                        const Mat4 m = shape_matrix(world, c);
                        if (c.shape == ColliderShape::box || c.shape == ColliderShape::rounded_box) box_edges(r, m, selected_color, 2.0f, false);
                        else ring_edges(r, m, selected_color, 2.0f, false);
                    }
                } else if (!named(scene, selected, "Camera") && !scene.light(selected) && !named(scene, selected, "Light")) {
                    box_edges(r, world * scaling({1.04f, 1.04f, 1.04f}), selected_color, 2.0f, false);
                }
            }
            if (m_meshes->fluids && !m_meshes->fluids->empty()) {
                Meshes& mm = *m_meshes;
                std::size_t used = 0;
                auto fresh = [&mm, &used]() -> fg::Mesh3D& {
                    if (used >= mm.fluid_meshes.size()) mm.fluid_meshes.emplace_back();
                    fg::Mesh3D& m = mm.fluid_meshes[used++];
                    m = fg::Mesh3D();
                    return m;
                };
                auto liquid = [](const xylem::FluidDraw& d) {
                    fg::Material3D mat = fg::Material3D::physical(0.0f, 0.06f);
                    mat.cull = fg::Cull3D::None;
                    mat.reflectivity = 0.3f;
                    mat.shadow = fg::Shadow3D::None;
                    if (d.opacity < 0.999f) {
                        mat.blend = fg::Blend3D::Alpha;
                        mat.depth_write = false;
                    }
                    return mat;
                };
                auto tint = [](const xylem::FluidDraw& d) {
                    return Color(to_byte(d.color.x), to_byte(d.color.y), to_byte(d.color.z), static_cast<std::uint8_t>(std::lround(std::clamp(d.opacity, 0.05f, 1.0f) * 255.0f)));
                };
                auto put = [](fg::Mesh3D& m, Vec3 p, Vec3 n, Color c) {
                    m.add_vertex(fg::Vertex3D(vector3d{p.x, p.y, p.z}, vector3d{n.x, n.y, n.z}, c));
                };
                auto quad = [&put](fg::Mesh3D& m, Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 n, Color col) {
                    const auto base = static_cast<std::uint32_t>(m.vertices().size());
                    put(m, a, n, col);
                    put(m, b, n, col);
                    put(m, c, n, col);
                    put(m, d, n, col);
                    m.add_triangle(base, base + 1, base + 2);
                    m.add_triangle(base, base + 2, base + 3);
                };
                auto slab = [&quad](fg::Mesh3D& m, Vec3 o, Vec3 ex, Vec3 ey, Vec3 ez, Color col) {
                    const Vec3 nx = normalize(ex), ny = normalize(ey), nz = normalize(ez);
                    quad(m, o + ey, o + ey + ex, o + ey + ex + ez, o + ey + ez, ny, col);
                    quad(m, o, o + ez, o + ez + ex, o + ex, ny * -1.0f, col);
                    quad(m, o, o + ex, o + ex + ey, o + ey, nz * -1.0f, col);
                    quad(m, o + ez, o + ez + ey, o + ez + ey + ex, o + ez + ex, nz, col);
                    quad(m, o, o + ey, o + ey + ez, o + ez, nx * -1.0f, col);
                    quad(m, o + ex, o + ex + ez, o + ex + ez + ey, o + ex + ey, nx, col);
                };
                const Vec3 up_world = axes.up();
                for (const xylem::FluidDraw& d : *m_meshes->fluids) {
                    const Color col = tint(d);
                    const fg::Material3D mat = liquid(d);
                    if (d.cell < 0.0f) {
                        fg::Mesh3D& m = fresh();
                        const float frac = d.size.y > 0.0f ? std::clamp(d.level_height / d.size.y, 0.0f, 1.0f) : 0.0f;
                        Vec3 ex{d.size.x, 0.0f, 0.0f}, ey{0.0f, d.size.y, 0.0f}, ez{0.0f, 0.0f, d.size.z};
                        if (std::fabs(up_world.y) > 0.5f) ey = ey * frac;
                        else if (std::fabs(up_world.z) > 0.5f) ez = ez * frac;
                        else ex = ex * frac;
                        Vec3 o = d.origin;
                        if (up_world.x < -0.5f) o = o + Vec3{d.size.x, 0.0f, 0.0f} - ex;
                        if (up_world.y < -0.5f) o = o + Vec3{0.0f, d.size.y, 0.0f} - ey;
                        if (up_world.z < -0.5f) o = o + Vec3{0.0f, 0.0f, d.size.z} - ez;
                        if (frac > 0.0f) slab(m, o, ex, ey, ez, col);
                        r.draw_mesh(m, fizmo::linalg::Matrix4d::identity(), mat);
                        continue;
                    }
                    if (d.level == FluidLevel::still) {
                        fg::Mesh3D& m = fresh();
                        if (d.level_height > 0.0f) slab(m, d.origin, d.axis_u * d.size.x, d.axis_up * d.level_height, d.axis_w * d.size.z, col);
                        r.draw_mesh(m, fizmo::linalg::Matrix4d::identity(), mat);
                        continue;
                    }
                    if (d.level == FluidLevel::waves && d.nx > 1 && d.nz > 1) {
                        fg::Mesh3D& m = fresh();
                        const std::size_t cols = static_cast<std::size_t>(d.nx) * static_cast<std::size_t>(d.nz);
                        if (d.values.size() < cols * 2) continue;
                        auto at = [&](int i, int k) {
                            const float h = d.values[static_cast<std::size_t>(k) * static_cast<std::size_t>(d.nx) + static_cast<std::size_t>(i)];
                            return d.origin + d.axis_u * ((static_cast<float>(i) + 0.5f) * d.cell) + d.axis_w * ((static_cast<float>(k) + 0.5f) * d.cell) + d.axis_up * h;
                        };
                        auto wet = [&](int i, int k) { return d.values[cols + static_cast<std::size_t>(k) * static_cast<std::size_t>(d.nx) + static_cast<std::size_t>(i)] > 1e-3f; };
                        for (int k = 0; k < d.nz; ++k) {
                            for (int i = 0; i < d.nx; ++i) {
                                const Vec3 p = at(i, k);
                                const Vec3 dx = at(std::min(i + 1, d.nx - 1), k) - at(std::max(i - 1, 0), k);
                                const Vec3 dz = at(i, std::min(k + 1, d.nz - 1)) - at(i, std::max(k - 1, 0));
                                Vec3 n = normalize(cross(dz, dx));
                                if (dot(n, d.axis_up) < 0.0f) n = n * -1.0f;
                                put(m, p, n, col);
                            }
                        }
                        for (int k = 0; k + 1 < d.nz; ++k) {
                            for (int i = 0; i + 1 < d.nx; ++i) {
                                if (!wet(i, k) && !wet(i + 1, k) && !wet(i, k + 1) && !wet(i + 1, k + 1)) continue;
                                const auto a = static_cast<std::uint32_t>(k * d.nx + i);
                                const auto b = a + 1, c2 = a + static_cast<std::uint32_t>(d.nx), e2 = c2 + 1;
                                m.add_triangle(a, c2, b);
                                m.add_triangle(b, c2, e2);
                            }
                        }
                        r.draw_mesh(m, fizmo::linalg::Matrix4d::identity(), mat);
                        continue;
                    }
                    if ((d.level == FluidLevel::blocky || d.level == FluidLevel::flowing) && d.nx > 0 && d.ny > 0 && d.nz > 0) {
                        fg::Mesh3D& m = fresh();
                        auto fill_at = [&](int i, int j, int k) -> float {
                            if (i < 0 || j < 0 || k < 0 || i >= d.nx || j >= d.ny || k >= d.nz) return 0.0f;
                            const std::size_t idx = (static_cast<std::size_t>(j) * static_cast<std::size_t>(d.nz) + static_cast<std::size_t>(k)) * static_cast<std::size_t>(d.nx) + static_cast<std::size_t>(i);
                            return idx < d.values.size() ? d.values[idx] : 0.0f;
                        };
                        const Vec3 eu = d.axis_u * d.cell, ew = d.axis_w * d.cell;
                        for (int j = 0; j < d.ny; ++j) {
                            for (int k = 0; k < d.nz; ++k) {
                                for (int i = 0; i < d.nx; ++i) {
                                    const float f0 = std::min(fill_at(i, j, k), 1.0f);
                                    if (f0 <= 0.02f) continue;
                                    const bool covered = fill_at(i, j + 1, k) > 0.02f;
                                    const float h = covered ? 1.0f : f0;
                                    const Vec3 o = d.origin + d.axis_u * (static_cast<float>(i) * d.cell) + d.axis_up * (static_cast<float>(j) * d.cell) + d.axis_w * (static_cast<float>(k) * d.cell);
                                    const Vec3 ey = d.axis_up * (h * d.cell);
                                    if (!covered) quad(m, o + ey, o + ey + eu, o + ey + eu + ew, o + ey + ew, d.axis_up, col);
                                    if (fill_at(i - 1, j, k) <= 0.02f) quad(m, o, o + ey, o + ey + ew, o + ew, d.axis_u * -1.0f, col);
                                    if (fill_at(i + 1, j, k) <= 0.02f) quad(m, o + eu, o + eu + ew, o + eu + ew + ey, o + eu + ey, d.axis_u, col);
                                    if (fill_at(i, j, k - 1) <= 0.02f) quad(m, o, o + eu, o + eu + ey, o + ey, d.axis_w * -1.0f, col);
                                    if (fill_at(i, j, k + 1) <= 0.02f) quad(m, o + ew, o + ew + ey, o + ew + ey + eu, o + ew + eu, d.axis_w, col);
                                }
                            }
                        }
                        r.draw_mesh(m, fizmo::linalg::Matrix4d::identity(), mat);
                        continue;
                    }
                    if (d.level == FluidLevel::realistic && !d.particles.empty()) {
                        std::vector<fg::Instance3D> drops;
                        drops.reserve(d.particles.size());
                        for (const Vec3& p : d.particles) drops.emplace_back(p.x, p.y, p.z, d.radius * 2.2f, col);
                        r.draw_instances(mm.sphere_lod[1], vector3d{0.0, 0.0, 0.0}, drops, mat);
                    }
                }
            }
            if (m_options.particles) {
                m_particles.set_budget(m_options.particle_budget, m_options.particle_quality);
                m_particles.sync(scene, axes.up(), m_options.particles_running);
                if (m_options.particles_running) m_particles.step_3d(f.dt, cam);
                m_particles.draw_3d(r);
            }
            if (m_options.move_gizmo && scene.alive(selected) && !playing) {
                const Vec4 c4 = scene.world_matrix(selected) * Vec4{0.0f, 0.0f, 0.0f, 1.0f};
                const Vec3 at{c4.x, c4.y, c4.z};
                const float len = view.units_per_pixel(at) * 90.0f;
                const Color colors[3] = {axis_x, axis_y, axis_z};
                for (int i = 0; i < 3; ++i) {
                    Vec3 d{};
                    (i == 0 ? d.x : i == 1 ? d.y : d.z) = 1.0f;
                    const bool hot = m_options.hot_axis == i || (m_options.hot_axis == 3 && std::fabs(dot(d, axes.up())) > 0.5f);
                    const Color c = hot ? selected_color : colors[i];
                    const Vec3 tip = at + d * len;
                    Vec3 side = cross(d, view.view_direction());
                    if (length(side) < 1e-4f) side = cross(d, Vec3{0.3f, 0.5f, 0.8f});
                    side = normalize(side) * (len * 0.09f);
                    const Vec3 back = tip - d * (len * 0.2f);
                    r.draw_line_3d(vector3d{at.x, at.y, at.z}, vector3d{tip.x, tip.y, tip.z}, c, hot ? 5.0f : 4.0f, false);
                    r.draw_line_3d(vector3d{tip.x, tip.y, tip.z}, vector3d{back.x + side.x, back.y + side.y, back.z + side.z}, c, 4.0f, false);
                    r.draw_line_3d(vector3d{tip.x, tip.y, tip.z}, vector3d{back.x - side.x, back.y - side.y, back.z - side.z}, c, 4.0f, false);
                }
            }
            r.end_3d();

            const double sx = f.width / std::max(1u, f.viewport_width);
            const double sy = f.height / std::max(1u, f.viewport_height);
            if (m_options.icons) {
                for (const Icon& ic : icons) {
                    const fg::ScreenPoint3D sp = cam.world_to_screen(ic.at);
                    if (!sp.in_front) continue;
                    const int x = static_cast<int>(std::lround(sp.x * sx));
                    const int y = static_cast<int>(std::lround(sp.y * sy));
                    const Color edge = ic.selected ? selected_color : icon_outline;
                    if (ic.kind == 2) {
                        for (int k = 0; k < 6; ++k) {
                            const double a = k * pi / 3.0 + pi / 6.0;
                            const int x1 = x + static_cast<int>(std::lround(std::cos(a) * 14.0)), y1 = y + static_cast<int>(std::lround(std::sin(a) * 14.0));
                            r.draw_line(x, y, x1, y1, fg::Paint::stroke(edge, 5));
                            r.draw_line(x, y, x1, y1, fg::Paint::stroke(ic.color, 3));
                            r.draw_circle(x1, y1, 3, fg::Paint::fill(ic.color));
                        }
                        r.draw_circle(x, y, 6, fg::Paint::fill(edge));
                        r.draw_circle(x, y, 4, fg::Paint::fill(Color(255, 236, 200)));
                    } else if (ic.kind == 0) {
                        for (int k = 0; k < 8; ++k) {
                            const double a = k * pi / 4.0;
                            const int x0 = x + static_cast<int>(std::lround(std::cos(a) * 13.0)), y0 = y + static_cast<int>(std::lround(std::sin(a) * 13.0));
                            const int x1 = x + static_cast<int>(std::lround(std::cos(a) * 20.0)), y1 = y + static_cast<int>(std::lround(std::sin(a) * 20.0));
                            r.draw_line(x0, y0, x1, y1, fg::Paint::stroke(icon_outline, 5));
                            r.draw_line(x0, y0, x1, y1, fg::Paint::stroke(ic.color, 3));
                        }
                        r.draw_circle(x, y, 11, fg::Paint::fill(edge));
                        r.draw_circle(x, y, 9, fg::Paint::fill(ic.color));
                        r.draw_circle(x, y, 4, fg::Paint::fill(Color(255, 255, 240)));
                    } else {
                        const int bw = 26, bh = 18;
                        r.draw_rect(x - bw / 2 - 2, y - bh / 2 - 2, bw + 4, bh + 4, fg::Paint::fill(edge));
                        r.draw_rect(x - bw / 2, y - bh / 2, bw, bh, fg::Paint::fill(ic.color));
                        const fg::RenderPoint lens[3] = {fg::RenderPoint(x + bw / 2.0 + 1.0, y), fg::RenderPoint(x + bw / 2.0 + 11.0, y - 9.0), fg::RenderPoint(x + bw / 2.0 + 11.0, y + 9.0)};
                        const fg::RenderPoint lens_edge[3] = {fg::RenderPoint(x + bw / 2.0 - 1.0, y), fg::RenderPoint(x + bw / 2.0 + 13.0, y - 11.0), fg::RenderPoint(x + bw / 2.0 + 13.0, y + 11.0)};
                        r.draw_polygon(lens_edge, 3, fg::Paint::fill(edge));
                        r.draw_polygon(lens, 3, fg::Paint::fill(ic.color));
                        r.draw_circle(x - 6, y - bh / 2 - 5, 5, fg::Paint::fill(edge));
                        r.draw_circle(x - 6, y - bh / 2 - 5, 3, fg::Paint::fill(ic.color));
                        r.draw_circle(x + 5, y - bh / 2 - 5, 5, fg::Paint::fill(edge));
                        r.draw_circle(x + 5, y - bh / 2 - 5, 3, fg::Paint::fill(ic.color));
                        r.draw_circle(x, y, 4, fg::Paint::fill(icon_outline));
                    }
                }
            }
            if (m_options.orientation_gizmo) {
                const double cx = std::max(1.0, f.width) - 58.0, cy = 58.0, radius = 42.0;
                r.draw_circle(static_cast<int>(cx), static_cast<int>(cy), static_cast<unsigned>(radius + 6.0), fg::Paint::fill(Color(12, 14, 11, 150)));
                const Vec3 t = oc.target;
                const fg::ScreenPoint3D c0 = cam.world_to_screen(vector3d{t.x, t.y, t.z});
                struct End {
                    double x, y, depth;
                    int axis;
                    bool positive;
                };
                std::vector<End> ends;
                const double probe = std::max(0.01, static_cast<double>(oc.distance) * 0.05);
                const Color colors[3] = {axis_x, axis_y, axis_z};
                const char* names[3] = {"X", "Y", "Z"};
                for (int i = 0; i < 3; ++i) {
                    for (int sgn = 0; sgn < 2; ++sgn) {
                        vector3d p{t.x, t.y, t.z};
                        const double k = sgn == 0 ? probe : -probe;
                        if (i == 0) p.x += k;
                        else if (i == 1) p.y += k;
                        else p.z += k;
                        const fg::ScreenPoint3D sp = cam.world_to_screen(p);
                        double dx = sp.x - c0.x, dy = sp.y - c0.y;
                        const double len = std::hypot(dx, dy);
                        const Vec3 dir = normalize(Vec3{static_cast<float>(p.x - t.x), static_cast<float>(p.y - t.y), static_cast<float>(p.z - t.z)});
                        const double facing = dot(dir, view.view_direction());
                        const double scale = std::sqrt(std::max(0.0, 1.0 - facing * facing));
                        if (len > 1e-9) {
                            dx = dx / len * scale;
                            dy = dy / len * scale;
                        } else {
                            dx = dy = 0.0;
                        }
                        ends.push_back({cx + dx * radius, cy + dy * radius, facing, i, sgn == 0});
                    }
                }
                std::sort(ends.begin(), ends.end(), [](const End& a, const End& b) { return a.depth > b.depth; });
                for (const End& e : ends) {
                    const Color c = colors[e.axis];
                    if (e.positive) {
                        r.draw_line(static_cast<int>(cx), static_cast<int>(cy), static_cast<int>(std::lround(e.x)), static_cast<int>(std::lround(e.y)), fg::Paint::stroke(c, 3));
                        r.draw_circle(static_cast<int>(std::lround(e.x)), static_cast<int>(std::lround(e.y)), 10, fg::Paint::fill(c));
                        const fizmo::text::TextStyle st = text_style(12.0f, Color(16, 18, 15));
                        const fizmo::text::TextMetrics m = r.measure_text(names[e.axis], st);
                        r.draw_text(static_cast<int>(std::lround(e.x - m.width * 0.5)), static_cast<int>(std::lround(e.y - m.height * 0.5)), names[e.axis], st);
                    } else {
                        r.draw_circle(static_cast<int>(std::lround(e.x)), static_cast<int>(std::lround(e.y)), 7, fg::Paint::fill(with_alpha(c, 110)));
                        r.draw_circle(static_cast<int>(std::lround(e.x)), static_cast<int>(std::lround(e.y)), 7, fg::Paint::stroke(c, 1));
                    }
                }
            }
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
        } else if (scene.emitter(e)) {
            if (!playing) {
                const fg::RenderPoint c = local(world, 0.0f, 0.0f);
                for (int k = 0; k < 6; ++k) {
                    const double a = k * pi / 3.0 + pi / 6.0;
                    line(c, fg::RenderPoint(c.x + std::cos(a) * 12.0, c.y + std::sin(a) * 12.0), sel ? selected_color : particle_color, 2);
                }
            }
        } else if (!scene.fluid(e)) {
            const Color col = named(scene, e, "Camera") ? camera_color : plain;
            quad(world, with_alpha(col, 50), sel ? selected_color : col, sel ? 2u : 1u);
        }
    }
    if (m_meshes->fluids) {
        for (const xylem::FluidDraw& d : *m_meshes->fluids) {
            const Color tint(static_cast<std::uint8_t>(std::clamp(d.color.x, 0.0f, 1.0f) * 255.0f), static_cast<std::uint8_t>(std::clamp(d.color.y, 0.0f, 1.0f) * 255.0f), static_cast<std::uint8_t>(std::clamp(d.color.z, 0.0f, 1.0f) * 255.0f), static_cast<std::uint8_t>(std::clamp(d.opacity, 0.05f, 1.0f) * 255.0f));
            const fg::RenderPoint box[4] = {px({d.origin.x, d.origin.y, 0.0f}), px({d.origin.x + d.size.x, d.origin.y, 0.0f}), px({d.origin.x + d.size.x, d.origin.y + d.size.y, 0.0f}), px({d.origin.x, d.origin.y + d.size.y, 0.0f})};
            if (const fg::Texture* tex = m_meshes->fluid_texture(d)) {
                const fg::RenderPoint quad[4] = {px({d.image_min.x, d.image_max.y, 0.0f}), px({d.image_max.x, d.image_max.y, 0.0f}), px({d.image_max.x, d.image_min.y, 0.0f}), px({d.image_min.x, d.image_min.y, 0.0f})};
                r.draw_texture_quad(*tex, quad, 1.0f);
            } else {
                const float hgt = std::clamp(d.level_height, 0.0f, d.size.y);
                if (hgt > 0.0f) {
                    const fg::RenderPoint water[4] = {px({d.origin.x, d.origin.y, 0.0f}), px({d.origin.x + d.size.x, d.origin.y, 0.0f}), px({d.origin.x + d.size.x, d.origin.y + hgt, 0.0f}), px({d.origin.x, d.origin.y + hgt, 0.0f})};
                    r.draw_polygon(water, 4, fg::Paint::fill(tint));
                    line(water[3], water[2], with_alpha(tint, 255), 2);
                }
            }
            if (!playing) r.draw_polyline(box, 4, fg::Paint::stroke(with_alpha(tint, 140), 1), true);
        }
    }
    if (m_options.particles) {
        m_particles.set_budget(m_options.particle_budget, m_options.particle_quality);
        const bool screen_up = px({0.0f, 1.0f, 0.0f}).y < px({0.0f, 0.0f, 0.0f}).y;
        m_particles.sync(scene, Vec3{0.0f, screen_up ? 1.0f : -1.0f, 0.0f}, m_options.particles_running);
        if (m_options.particles_running) m_particles.step_2d(f.dt, left, bottom, right, top);
        m_particles.draw_2d(r, [&px](float x, float y, float& sx, float& sy) {
            const fg::RenderPoint p = px({x, y, 0.0f});
            sx = static_cast<float>(p.x);
            sy = static_cast<float>(p.y);
        });
    }
}

bool GpuView::draw_traced(grove::GpuCanvasFrame& f, fizmo::lighting::Scene& scene, const fizmo::lighting::LightingSettings& settings, const fizmo::lighting::Camera& camera) {
    if (!prepare_tracer(f, scene, settings, camera)) return false;
    const unsigned cw = static_cast<unsigned>(std::max(1.0, std::round(f.width)));
    const unsigned ch = static_cast<unsigned>(std::max(1.0, std::round(f.height)));
    m_meshes->tracer->draw(f.renderer, 0, 0, cw, ch);
    return true;
}

bool GpuView::draw_traced_over(grove::GpuCanvasFrame& f, fizmo::lighting::Scene& scene, const fizmo::lighting::LightingSettings& settings, const fizmo::lighting::Camera& camera, std::uint32_t full_after_samples) {
    if (!prepare_tracer(f, scene, settings, camera)) return false;
    const std::uint32_t have = m_meshes->tracer->samples();
    const float opacity = full_after_samples == 0 ? 1.0f : std::clamp(static_cast<float>(have) / static_cast<float>(full_after_samples), 0.0f, 1.0f);
    const unsigned cw = static_cast<unsigned>(std::max(1.0, std::round(f.width)));
    const unsigned ch = static_cast<unsigned>(std::max(1.0, std::round(f.height)));
    m_meshes->tracer->draw(f.renderer, 0, 0, cw, ch, opacity);
    return true;
}

bool GpuView::prepare_tracer(grove::GpuCanvasFrame& f, fizmo::lighting::Scene& scene, const fizmo::lighting::LightingSettings& settings, const fizmo::lighting::Camera& camera) {
    fg::Renderer& r = f.renderer;
    if (!r.gpu_device()) {
        m_meshes->trace_error = "path tracing in the viewport needs a GPU";
        return false;
    }
    Meshes& m = *m_meshes;
    if (!m.tracer || m.traced_with != &r) {
        m.tracer = std::make_unique<fg::GpuPathTracer>();
        m.traced_with = &r;
        m.traced_scene = nullptr;
        if (!m.tracer->attach(r, 1, 1)) {
            m.trace_error = m.tracer->error();
            m.tracer.reset();
            return false;
        }
        m.tracer->set_settings(settings);
        m.traced_settings = settings;
    }
    if (!m.tracer->supported()) {
        m.trace_error = m.tracer->error();
        return false;
    }
    const unsigned w = static_cast<unsigned>(std::max(1.0, std::round(f.viewport_width)));
    const unsigned h = static_cast<unsigned>(std::max(1.0, std::round(f.viewport_height)));
    m.tracer->resize(w, h);
    if (settings.quality != m.traced_settings.quality || settings.transport != m.traced_settings.transport || settings.caustics != m.traced_settings.caustics || settings.max_bounces != m.traced_settings.max_bounces || settings.exposure != m.traced_settings.exposure || settings.tone_mapper != m.traced_settings.tone_mapper || settings.firefly_clamp != m.traced_settings.firefly_clamp) {
        m.tracer->set_settings(settings);
        m.traced_settings = settings;
    }
    if (m.traced_scene != &scene) {
        m.tracer->set_scene(scene);
        m.traced_scene = &scene;
    }
    m.tracer->set_camera(camera);
    m.trace_error.clear();
    return true;
}

std::uint32_t GpuView::traced_samples() const {
    return m_meshes->tracer ? m_meshes->tracer->samples() : 0u;
}

std::string GpuView::trace_error() const {
    return m_meshes->trace_error;
}

void GpuView::stop_tracing() {
    if (m_meshes->tracer) m_meshes->tracer->detach();
    m_meshes->tracer.reset();
    m_meshes->traced_with = nullptr;
    m_meshes->traced_scene = nullptr;
}

} // namespace editor
} // namespace orchard
