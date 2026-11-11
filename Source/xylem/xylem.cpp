#include <xylem/xylem.hpp>

#include <fizmo/../physics.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>

#ifdef XYLEM_WITH_MODELS
#include <fizmo/../../Source/Util Hpp/util_functions.hpp>
#include <fizmo/../../Source/Graphics/model_3d.hpp>
#endif // XYLEM_WITH_MODELS

namespace xylem {

namespace fp = fizmo::physics;
using fp::Real;
using fp::RQuat;
using fp::RVec2;
using fp::RVec3;
using fp::operator""_r;

namespace {

constexpr Real rad_to_deg = 57.29577951308232_r;
constexpr Real deg_to_rad = 0.017453292519943295_r;

RQuat to_fizmo(orchard::Quat q) {
    return RQuat(static_cast<Real>(q.w), static_cast<Real>(q.x), static_cast<Real>(q.y), static_cast<Real>(q.z));
}

orchard::Quat to_orchard(const RQuat& q) {
    return orchard::normalize(orchard::Quat{static_cast<float>(q.x()), static_cast<float>(q.y()), static_cast<float>(q.z()), static_cast<float>(q.w())});
}

fp::CombineMode to_fizmo(orchard::CombineMode m) {
    switch (m) {
    case orchard::CombineMode::average: return fp::CombineMode::Average;
    case orchard::CombineMode::geometric_mean: return fp::CombineMode::GeometricMean;
    case orchard::CombineMode::harmonic_mean: return fp::CombineMode::HarmonicMean;
    case orchard::CombineMode::quadratic_mean: return fp::CombineMode::QuadraticMean;
    case orchard::CombineMode::minimum: return fp::CombineMode::Minimum;
    case orchard::CombineMode::multiply: return fp::CombineMode::Multiply;
    case orchard::CombineMode::maximum: return fp::CombineMode::Maximum;
    case orchard::CombineMode::sum: return fp::CombineMode::Sum;
    case orchard::CombineMode::project_default: break;
    }
    return fp::CombineMode::Default;
}

RVec3 v3(orchard::Vec3 v) {
    return RVec3{static_cast<Real>(v.x), static_cast<Real>(v.y), static_cast<Real>(v.z)};
}

orchard::Vec3 ov(const RVec3& v) {
    return {static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z)};
}

orchard::Quat conjugate(orchard::Quat q) {
    return {-q.x, -q.y, -q.z, q.w};
}

float angle_z(orchard::Quat q) {
    return std::atan2(2.0f * (q.w * q.z + q.x * q.y), 1.0f - 2.0f * (q.y * q.y + q.z * q.z));
}

orchard::Quat about_z(float radians) {
    return {0.0f, 0.0f, std::sin(radians * 0.5f), std::cos(radians * 0.5f)};
}

float angle_2d(orchard::Quat q) {
    const orchard::Vec3 up = orchard::rotate(q, orchard::Vec3{0.0f, 1.0f, 0.0f});
    if (up.x * up.x + up.y * up.y < 0.04f) return angle_z(q);
    return std::atan2(-up.x, up.y);
}

std::vector<orchard::Vec2> hull_2d(std::vector<orchard::Vec2> pts) {
    std::sort(pts.begin(), pts.end(), [](orchard::Vec2 a, orchard::Vec2 b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
    pts.erase(std::unique(pts.begin(), pts.end(), [](orchard::Vec2 a, orchard::Vec2 b) { return std::fabs(a.x - b.x) < 1e-5f && std::fabs(a.y - b.y) < 1e-5f; }), pts.end());
    if (pts.size() < 3) return pts;
    auto turn = [](orchard::Vec2 o, orchard::Vec2 a, orchard::Vec2 b) { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); };
    std::vector<orchard::Vec2> h(pts.size() * 2);
    std::size_t k = 0;
    for (std::size_t i = 0; i < pts.size(); ++i) {
        while (k >= 2 && turn(h[k - 2], h[k - 1], pts[i]) <= 1e-9f) --k;
        h[k++] = pts[i];
    }
    for (std::size_t i = pts.size() - 1, t = k + 1; i > 0; --i) {
        while (k >= t && turn(h[k - 2], h[k - 1], pts[i - 1]) <= 1e-9f) --k;
        h[k++] = pts[i - 1];
    }
    h.resize(k > 0 ? k - 1 : 0);
    while (h.size() > 8) {
        std::size_t best = 0;
        float least = 1e30f;
        for (std::size_t i = 0; i < h.size(); ++i) {
            const orchard::Vec2 a = h[(i + h.size() - 1) % h.size()], b = h[i], c = h[(i + 1) % h.size()];
            const float area = std::fabs(turn(a, b, c));
            if (area < least) {
                least = area;
                best = i;
            }
        }
        h.erase(h.begin() + static_cast<std::ptrdiff_t>(best));
    }
    return h;
}

std::vector<orchard::Vec3> outline_points(const orchard::Collider& c, orchard::Vec3 s) {
    std::vector<orchard::Vec3> out;
    const float pi2 = 6.2831853f;
    switch (c.shape) {
    case orchard::ColliderShape::cylinder:
    case orchard::ColliderShape::cone: {
        const float r = std::max(s.x, s.z) * 0.5f, h = s.y * 0.5f;
        for (int k = 0; k < 16; ++k) {
            const float a = pi2 * static_cast<float>(k) / 16.0f;
            out.push_back({r * std::cos(a), -h, r * std::sin(a)});
            if (c.shape == orchard::ColliderShape::cylinder) out.push_back({r * std::cos(a), h, r * std::sin(a)});
        }
        if (c.shape == orchard::ColliderShape::cone) out.push_back({0.0f, h, 0.0f});
        break;
    }
    case orchard::ColliderShape::triangle:
        return orchard::triangle_prism_points(c, orchard::Vec3{s.x / std::max(c.size.x, 1e-6f), s.y / std::max(c.size.y, 1e-6f), s.z / std::max(c.size.z, 1e-6f)});
    case orchard::ColliderShape::sphere_section: {
        const float r = std::max({s.x, s.y, s.z}) * 0.5f;
        const float p0 = c.polar_min * orchard::pi / 180.0f, p1 = c.polar_max * orchard::pi / 180.0f;
        const float a0 = c.azimuth_min * orchard::pi / 180.0f, a1 = c.azimuth_max * orchard::pi / 180.0f;
        for (int i = 0; i <= 8; ++i) {
            const float th = p0 + (p1 - p0) * static_cast<float>(i) / 8.0f;
            out.push_back({0.0f, r * std::cos(th), 0.0f});
            for (int j = 0; j <= 16; ++j) {
                const float ph = a0 + (a1 - a0) * static_cast<float>(j) / 16.0f;
                out.push_back({r * std::sin(th) * std::cos(ph), r * std::cos(th), r * std::sin(th) * std::sin(ph)});
            }
        }
        break;
    }
    default:
        for (int k = 0; k < 8; ++k) out.push_back({(k & 1 ? 0.5f : -0.5f) * s.x, (k & 2 ? 0.5f : -0.5f) * s.y, (k & 4 ? 0.5f : -0.5f) * s.z});
        break;
    }
    return out;
}

fp::FluidLevel to_fizmo(orchard::FluidLevel level) {
    switch (level) {
    case orchard::FluidLevel::blocky: return fp::FluidLevel::blocky;
    case orchard::FluidLevel::still: return fp::FluidLevel::still;
    case orchard::FluidLevel::flowing: return fp::FluidLevel::flowing;
    case orchard::FluidLevel::realistic: return fp::FluidLevel::realistic;
    case orchard::FluidLevel::waves:
    case orchard::FluidLevel::project_default: break;
    }
    return fp::FluidLevel::waves;
}

fp::SphereSection3D section_of(const orchard::Collider& c, Real diameter) {
    fp::SphereSection3D s;
    s.radius = std::max(0.0005_r, diameter * 0.5_r);
    s.polar_min = std::clamp(static_cast<Real>(c.polar_min), 0.0_r, 180.0_r);
    s.polar_max = std::clamp(static_cast<Real>(c.polar_max), s.polar_min, 180.0_r);
    s.azimuth_min = static_cast<Real>(c.azimuth_min);
    s.azimuth_max = std::max(static_cast<Real>(c.azimuth_max), s.azimuth_min);
    if (s.azimuth_max - s.azimuth_min > 360.0_r) s.azimuth_max = s.azimuth_min + 360.0_r;
    s.inner_radius = c.shell > 0.0f ? std::max(0.0_r, s.radius - static_cast<Real>(c.shell)) : 0.0_r;
    return s;
}

fp::BodyQuality to_fizmo(orchard::BodyQuality q) {
    switch (q) {
    case orchard::BodyQuality::project_default: return fp::BodyQuality::Default;
    case orchard::BodyQuality::arcade: return fp::BodyQuality::Arcade;
    case orchard::BodyQuality::normal: return fp::BodyQuality::Normal;
    case orchard::BodyQuality::high: return fp::BodyQuality::High;
    case orchard::BodyQuality::ultra: return fp::BodyQuality::Ultra;
    }
    return fp::BodyQuality::Default;
}

fp::ContinuousMode to_fizmo(orchard::BodyContinuous c) {
    switch (c) {
    case orchard::BodyContinuous::project_default: return fp::ContinuousMode::Default;
    case orchard::BodyContinuous::on: return fp::ContinuousMode::On;
    case orchard::BodyContinuous::off: return fp::ContinuousMode::Off;
    }
    return fp::ContinuousMode::Default;
}

fp::PhysicsFlags flags_for(const orchard::Body& b) {
    fp::PhysicsFlags f = b.type == orchard::BodyType::fixed ? fp::PhysicsFlags::DefaultStatic
                       : b.type == orchard::BodyType::kinematic ? fp::PhysicsFlags::DefaultKinematic
                       : fp::PhysicsFlags::DefaultDynamic;
    auto set = [&f](fp::PhysicsFlags flag, bool on) {
        if (on) f |= flag;
        else f &= ~flag;
    };
    if (b.type == orchard::BodyType::dynamic) {
        set(fp::PhysicsFlags::Gravity, b.has(orchard::body_gravity));
        set(fp::PhysicsFlags::AllowSleep, b.has(orchard::body_sleep));
        set(fp::PhysicsFlags::StartAwake, b.has(orchard::body_start_awake));
        set(fp::PhysicsFlags::Bullet, b.has(orchard::body_continuous));
    }
    set(fp::PhysicsFlags::CollisionResponse, b.has(orchard::body_collisions));
    set(fp::PhysicsFlags::Intangible, !b.has(orchard::body_collisions));
    set(fp::PhysicsFlags::FrictionResponse, b.has(orchard::body_friction));
    set(fp::PhysicsFlags::RestitutionResponse, b.has(orchard::body_bounce));
    set(fp::PhysicsFlags::ContactEvents, b.has(orchard::body_contact_events));
    set(fp::PhysicsFlags::SensorEvents, b.has(orchard::body_contact_events));
    set(fp::PhysicsFlags::LockPositionX, b.has(orchard::body_lock_position_x));
    set(fp::PhysicsFlags::LockPositionY, b.has(orchard::body_lock_position_y));
    set(fp::PhysicsFlags::LockPositionZ, b.has(orchard::body_lock_position_z));
    set(fp::PhysicsFlags::LockRotationX, b.has(orchard::body_lock_rotation_x));
    set(fp::PhysicsFlags::LockRotationY, b.has(orchard::body_lock_rotation_y));
    set(fp::PhysicsFlags::LockRotationZ, b.has(orchard::body_lock_rotation_z));
    set(fp::PhysicsFlags::Enabled, b.enabled);
    if (b.max_speed > 0.0f) f |= fp::PhysicsFlags::VelocityClamping;
    if (b.max_spin > 0.0f) f |= fp::PhysicsFlags::AngularClamping;
    return f;
}

fp::PhysicsFlags flags_with_overrides(const orchard::Body& b) {
    std::uint64_t raw = static_cast<std::uint64_t>(flags_for(b));
    raw = (raw | b.flags_on) & ~b.flags_off;
    if (!b.enabled) raw &= ~static_cast<std::uint64_t>(fp::PhysicsFlags::Enabled);
    return static_cast<fp::PhysicsFlags>(raw);
}

struct Part {
    orchard::Collider collider;
    orchard::Vec3 offset;
    orchard::Quat rotation;
    orchard::Vec3 scale{1.0f, 1.0f, 1.0f};
};

std::vector<Part> parts_of(const orchard::Scene& scene, orchard::Entity owner) {
    std::vector<Part> out;
    const orchard::Vec3 owner_pos = world_position(scene, owner);
    const orchard::Quat owner_rot = world_rotation(scene, owner);
    const orchard::Quat owner_inv = conjugate(owner_rot);
    std::vector<orchard::Entity> stack{owner};
    while (!stack.empty()) {
        const orchard::Entity e = stack.back();
        stack.pop_back();
        if (e != owner && scene.body(e)) continue;
        const std::vector<orchard::Collider>* cs = scene.colliders(e);
        if (cs) {
            const orchard::Vec3 pos = world_position(scene, e);
            const orchard::Quat rot = world_rotation(scene, e);
            const orchard::Vec3 scale = world_scale(scene, e);
            for (const orchard::Collider& c : *cs) {
                if (!c.enabled) continue;
                Part p;
                p.collider = c;
                const orchard::Vec3 center = pos + orchard::rotate(rot, c.offset * scale);
                p.offset = orchard::rotate(owner_inv, center - owner_pos);
                p.rotation = orchard::normalize(owner_inv * rot);
                p.scale = scale;
                out.push_back(p);
            }
        }
        for (orchard::Entity c : scene.children(e)) stack.push_back(c);
    }
    return out;
}

struct ModelSlot {
    std::shared_ptr<const ModelGeometry> geometry;
    std::filesystem::file_time_type stamp{};
    std::string error;
};

std::mutex& model_mutex() {
    static std::mutex m;
    return m;
}

std::map<std::string, ModelSlot>& model_slots() {
    static std::map<std::string, ModelSlot> slots;
    return slots;
}

std::map<std::string, fp::Shape3D>& shape_slots() {
    static std::map<std::string, fp::Shape3D> slots;
    return slots;
}

std::string scale_key(orchard::Vec3 s) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "%.5g,%.5g,%.5g", static_cast<double>(s.x), static_cast<double>(s.y), static_cast<double>(s.z));
    return buf;
}

std::vector<RVec3> hull_points(const ModelGeometry& g, orchard::Vec3 scale) {
    std::vector<RVec3> out;
    if (g.vertices.empty()) return out;
    std::vector<orchard::Vec3> dirs;
    const int n = 64;
    for (int i = 0; i < n; ++i) {
        const float y = 1.0f - 2.0f * (static_cast<float>(i) + 0.5f) / static_cast<float>(n);
        const float r = std::sqrt(std::max(0.0f, 1.0f - y * y));
        const float phi = static_cast<float>(i) * 2.399963f;
        dirs.push_back({std::cos(phi) * r, y, std::sin(phi) * r});
    }
    for (int i = 0; i < 8; ++i) dirs.push_back(orchard::normalize(orchard::Vec3{i & 1 ? 1.0f : -1.0f, i & 2 ? 1.0f : -1.0f, i & 4 ? 1.0f : -1.0f}));
    std::set<std::size_t> picked;
    for (const orchard::Vec3& d : dirs) {
        std::size_t best = 0;
        float best_dot = -1e30f;
        for (std::size_t i = 0; i < g.vertices.size(); ++i) {
            const float k = orchard::dot(g.vertices[i] * scale, d);
            if (k > best_dot) {
                best_dot = k;
                best = i;
            }
        }
        picked.insert(best);
    }
    for (std::size_t i : picked) out.push_back(v3(g.vertices[i] * scale));
    return out;
}

fp::Shape3D box_of(const ModelGeometry& g, orchard::Vec3 scale) {
    std::vector<RVec3> corners;
    for (int i = 0; i < 8; ++i) corners.push_back(v3(orchard::Vec3{i & 1 ? g.max.x : g.min.x, i & 2 ? g.max.y : g.min.y, i & 4 ? g.max.z : g.min.z} * scale));
    return fp::Shape3D::make_convex(corners);
}

std::optional<fp::Shape3D> model_shape(const std::string& file, bool z_up, orchard::Vec3 scale, bool as_mesh, std::string* error) {
    const std::shared_ptr<const ModelGeometry> g = load_model_geometry(file, z_up, error);
    if (!g || g->empty()) return std::nullopt;
    const std::string key = file + (z_up ? "|z|" : "|y|") + (as_mesh ? "mesh|" : "hull|") + scale_key(scale);
    {
        std::lock_guard<std::mutex> lock(model_mutex());
        auto it = shape_slots().find(key);
        if (it != shape_slots().end()) return it->second;
    }
    fp::Shape3D shape;
    if (as_mesh) {
        std::vector<RVec3> verts;
        verts.reserve(g->vertices.size());
        for (const orchard::Vec3& v : g->vertices) verts.push_back(v3(v * scale));
        std::vector<std::array<std::uint32_t, 3>> tris;
        tris.reserve(g->indices.size() / 3);
        for (std::size_t i = 0; i + 2 < g->indices.size(); i += 3) {
            const std::uint32_t a = g->indices[i], b = g->indices[i + 1], c = g->indices[i + 2];
            if (a == b || b == c || a == c) continue;
            const orchard::Vec3 n = orchard::cross(g->vertices[b] * scale - g->vertices[a] * scale, g->vertices[c] * scale - g->vertices[a] * scale);
            if (orchard::dot(n, n) <= 1e-20f) continue;
            tris.push_back({a, b, c});
        }
        if (tris.empty()) return std::nullopt;
        shape = fp::Shape3D::make_mesh(std::move(verts), std::move(tris), true);
    } else {
        shape = fp::Shape3D::make_convex(hull_points(*g, scale));
        if (!shape.hull || !shape.hull->valid()) shape = box_of(*g, scale);
        if (!shape.hull || !shape.hull->valid()) return std::nullopt;
    }
    std::lock_guard<std::mutex> lock(model_mutex());
    if (shape_slots().size() > 4096) shape_slots().clear();
    return shape_slots().emplace(key, shape).first->second;
}

struct Trunk {
    float radius = 0.3f;
    float bottom = 0.0f;
    float height = 1.0f;
};

Trunk trunk_of(const ModelGeometry& g) {
    Trunk t;
    t.bottom = g.min.y;
    t.height = std::max(0.05f, g.max.y - g.min.y);
    const float cut = g.min.y + t.height * 0.2f;
    float r = 0.0f;
    for (const orchard::Vec3& v : g.vertices)
        if (v.y <= cut) r = std::max(r, std::sqrt(v.x * v.x + v.z * v.z));
    if (r <= 0.0f) r = 0.5f * std::min(g.max.x - g.min.x, g.max.z - g.min.z);
    t.radius = std::max(0.05f, r);
    return t;
}

orchard::Vec3 terrain_axis_scale(orchard::Vec3 s, orchard::Quat frame) {
    auto along = [&](orchard::Vec3 axis) {
        const orchard::Vec3 d = orchard::rotate(frame, axis);
        return std::fabs(d.x) * s.x + std::fabs(d.y) * s.y + std::fabs(d.z) * s.z;
    };
    return {along({1.0f, 0.0f, 0.0f}), along({0.0f, 1.0f, 0.0f}), along({0.0f, 0.0f, 1.0f})};
}

} // namespace

bool models_supported() {
#ifdef XYLEM_WITH_MODELS
    return true;
#else
    return false;
#endif
}

void clear_model_cache() {
    std::lock_guard<std::mutex> lock(model_mutex());
    model_slots().clear();
    shape_slots().clear();
}

std::shared_ptr<const ModelGeometry> load_model_geometry(const std::string& path, bool z_up, std::string* error) {
    if (path.empty()) {
        if (error) *error = "no model file";
        return nullptr;
    }
    std::error_code ec;
    const std::filesystem::file_time_type stamp = std::filesystem::last_write_time(path, ec);
    if (ec) {
        if (error) *error = "can't find " + path;
        return nullptr;
    }
    const std::string key = path + (z_up ? "|z" : "|y");
    {
        std::lock_guard<std::mutex> lock(model_mutex());
        auto it = model_slots().find(key);
        if (it != model_slots().end() && it->second.stamp == stamp) {
            if (!it->second.geometry && error) *error = it->second.error;
            return it->second.geometry;
        }
    }
    ModelSlot slot;
    slot.stamp = stamp;
#ifdef XYLEM_WITH_MODELS
    namespace fg = fizmo::graphics;
    fg::Model3D model;
    fg::ModelImportOptions options;
    options.z_up = z_up;
    options.generate_normals = false;
    std::string why;
    if (fg::Model3D::load(std::filesystem::path(path), model, &why, options)) {
        auto g = std::make_shared<ModelGeometry>();
        std::vector<fg::model_math::Mat4> world;
        model.world_matrices(model.rest_pose(), world);
        for (std::size_t ni = 0; ni < model.nodes.size() && ni < world.size(); ++ni) {
            const int mesh = model.nodes[ni].mesh;
            if (mesh < 0 || static_cast<std::size_t>(mesh) >= model.meshes.size()) continue;
            for (const fg::ModelPrimitive& prim : model.meshes[static_cast<std::size_t>(mesh)].primitives) {
                const auto& verts = prim.mesh.vertices();
                const auto base = static_cast<std::uint32_t>(g->vertices.size());
                for (const fg::Vertex3D& v : verts) {
                    const auto p = fg::model_math::point(world[ni], v.x, v.y, v.z);
                    g->vertices.push_back({static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z)});
                }
                const auto& idx = prim.mesh.indices();
                if (idx.empty()) {
                    for (std::uint32_t i = 0; i + 2 < static_cast<std::uint32_t>(verts.size()); i += 3) g->indices.insert(g->indices.end(), {base + i, base + i + 1, base + i + 2});
                } else {
                    for (std::size_t i = 0; i + 2 < idx.size(); i += 3)
                        if (idx[i] < verts.size() && idx[i + 1] < verts.size() && idx[i + 2] < verts.size()) g->indices.insert(g->indices.end(), {base + idx[i], base + idx[i + 1], base + idx[i + 2]});
                }
            }
        }
        if (!g->vertices.empty()) {
            g->min = g->max = g->vertices.front();
            for (const orchard::Vec3& v : g->vertices) {
                g->min = {std::min(g->min.x, v.x), std::min(g->min.y, v.y), std::min(g->min.z, v.z)};
                g->max = {std::max(g->max.x, v.x), std::max(g->max.y, v.y), std::max(g->max.z, v.z)};
            }
        }
        if (g->empty()) slot.error = path + " has no triangles";
        else slot.geometry = std::move(g);
    } else {
        slot.error = why.empty() ? "can't load " + path : why;
    }
#else
    static_cast<void>(z_up);
    slot.error = "this build of Xylem can't read model files (Fizmo graphics isn't linked)";
#endif
    if (!slot.geometry && error) *error = slot.error;
    std::lock_guard<std::mutex> lock(model_mutex());
    ModelSlot& stored = model_slots()[key];
    stored = std::move(slot);
    return stored.geometry;
}

const std::vector<FlagOption>& flag_options() {
    using F = fp::PhysicsFlags;
    auto bit = [](F f) { return static_cast<std::uint64_t>(f); };
    static const std::vector<FlagOption> all = {
        {bit(F::LinearForces), "forces", "Pushed by forces", "Forces", true},
        {bit(F::Torque), "torque", "Turned by torque", "Forces", true},
        {bit(F::Wind), "wind", "Wind and fluid zones", "Forces", true},
        {bit(F::Buoyancy), "buoyancy", "Buoyancy", "Forces", true},
        {bit(F::Magnetism), "magnetism", "Magnetism", "Forces", true},
        {bit(F::Springs), "springs", "Springs", "Forces", true},
        {bit(F::UserForceField), "force-fields", "Force fields", "Forces", true},
        {bit(F::LinearVelocity), "moves", "Moves", "Motion", true},
        {bit(F::AngularVelocity), "turns", "Turns", "Motion", true},
        {bit(F::LinearDamping), "drag", "Drag", "Motion", true},
        {bit(F::AngularDamping), "spin-drag", "Spin drag", "Motion", true},
        {bit(F::VelocityClamping), "speed-cap", "Speed cap", "Motion", true},
        {bit(F::AngularClamping), "spin-cap", "Spin cap", "Motion", true},
        {bit(F::LinearImpulses), "pushes", "Takes pushes", "Impulses", true},
        {bit(F::AngularImpulses), "twists", "Takes twists", "Impulses", true},
        {bit(F::ContactImpulses), "contact-impulses", "Pushed by contacts", "Impulses", true},
        {bit(F::JointImpulses), "joint-impulses", "Pulled by joints", "Impulses", true},
        {bit(F::BroadphaseActive), "broadphase", "In the broadphase", "Collision", true},
        {bit(F::SpeculativeContacts), "speculative", "Speculative contacts", "Collision", true},
        {bit(F::PreSolveEvents), "pre-solve", "Pre-solve events", "Collision", true},
        {bit(F::PostSolveEvents), "post-solve", "Post-solve events", "Collision", true},
        {bit(F::IslandMember), "islands", "Joins islands", "Solver", true},
        {bit(F::WarmStarting), "warm-start", "Warm starting", "Solver", true},
        {bit(F::PositionCorrection), "correction", "Position correction", "Solver", true},
        {bit(F::AutoDisable), "auto-disable", "Disable after long sleep", "Lifetime", true},
        {bit(F::DestroyOnSleep), "destroy-sleep", "Remove when asleep", "Lifetime", true},
        {bit(F::DestroyOffScreen), "destroy-out", "Remove outside the world", "Lifetime", true},
        {bit(F::Draggable), "draggable", "Draggable", "Lifetime", true},
        {bit(F::Visible), "visible", "Visible", "Debug", true},
        {bit(F::DebugDraw), "debug", "Debug draw", "Debug", true},
        {bit(F::ShowVelocity), "show-velocity", "Show velocity", "Debug", true},
        {bit(F::ShowContacts), "show-contacts", "Show contacts", "Debug", true},
        {bit(F::ImpactSounds), "impact-sounds", "Impact sounds", "Effects", true},
        {bit(F::TrailEffect), "trail", "Trail", "Effects", true},
        {bit(F::ParticleOnImpact), "impact-particles", "Impact particles", "Effects", true},
        {bit(F::UserFlag1), "user-1", "User flag 1", "User", true},
        {bit(F::UserFlag2), "user-2", "User flag 2", "User", true},
        {bit(F::UserFlag3), "user-3", "User flag 3", "User", true},
        {bit(F::UserFlag4), "user-4", "User flag 4", "User", true},
        {bit(F::UserFlag5), "user-5", "User flag 5", "User", true},
        {bit(F::UserFlag6), "user-6", "User flag 6", "User", true},
        {bit(F::UserFlag7), "user-7", "User flag 7", "User", true},
        {bit(F::UserFlag8), "user-8", "User flag 8", "User", true},
        {bit(F::UserFlag9), "user-9", "User flag 9", "User", true},
        {bit(F::UserFlag10), "user-10", "User flag 10", "User", true},
    };
    return all;
}

std::uint64_t base_flags(const orchard::Body& b) {
    return static_cast<std::uint64_t>(flags_for(b));
}

std::uint64_t body_flags(const orchard::Body& b) {
    return static_cast<std::uint64_t>(flags_with_overrides(b));
}

bool flag_on(const orchard::Body& b, std::uint64_t bit) {
    return (body_flags(b) & bit) != 0;
}

void set_flag(orchard::Body& b, std::uint64_t bit, bool on) {
    b.flags_on &= ~bit;
    b.flags_off &= ~bit;
    const bool base = (base_flags(b) & bit) != 0;
    if (on == base) return;
    if (on) b.flags_on |= bit;
    else b.flags_off |= bit;
}

orchard::Quat world_rotation(const orchard::Scene& scene, orchard::Entity e) {
    orchard::Quat q;
    int depth = 0;
    for (orchard::Entity at = e; scene.alive(at) && depth < 1024; at = scene.parent(at), ++depth) q = scene.transform(at)->rotation * q;
    return orchard::normalize(q);
}

orchard::Vec3 world_position(const orchard::Scene& scene, orchard::Entity e) {
    const orchard::Vec4 p = scene.world_matrix(e) * orchard::Vec4{0.0f, 0.0f, 0.0f, 1.0f};
    return {p.x, p.y, p.z};
}

orchard::Vec3 world_scale(const orchard::Scene& scene, orchard::Entity e) {
    orchard::Vec3 s{1.0f, 1.0f, 1.0f};
    int depth = 0;
    for (orchard::Entity at = e; scene.alive(at) && depth < 1024; at = scene.parent(at), ++depth) s = s * scene.transform(at)->scale;
    return {std::fabs(s.x), std::fabs(s.y), std::fabs(s.z)};
}

struct World::Impl {
    orchard::Dimension dimension;
    Settings settings;
    orchard::Scene* scene = nullptr;
    std::unique_ptr<fp::Simulation2D> sim2;
    std::unique_ptr<fp::Simulation3D> sim3;
    std::unordered_map<std::uint64_t, fp::RigidBody2D*> bodies2;
    std::unordered_map<std::uint64_t, fp::RigidBody3D*> bodies3;
    std::unordered_map<const void*, orchard::Entity> owners;
    std::map<std::pair<std::uint64_t, std::uint64_t>, int> touching;
    std::vector<Contact> contacts;
    std::size_t colliders = 0;
    Real accumulator = 0.0_r;
    struct Pose {
        orchard::Vec3 position;
        orchard::Quat rotation;
    };
    std::unordered_map<std::uint64_t, Pose> previous;

    bool body_pose(std::uint64_t id, Pose& out) const {
        if (auto it = bodies3.find(id); it != bodies3.end()) {
            out.position = ov(it->second->position());
            out.rotation = to_orchard(it->second->orientation());
            return true;
        }
        if (auto it = bodies2.find(id); it != bodies2.end()) {
            const RVec2 p = it->second->position();
            out.position = {static_cast<float>(p.x), static_cast<float>(p.y), 0.0f};
            const float a = static_cast<float>(it->second->angle()) * 0.5f;
            out.rotation = {0.0f, 0.0f, std::sin(a), std::cos(a)};
            return true;
        }
        return false;
    }

    void remember_poses() {
        previous.clear();
        Pose p;
        for (const auto& [id, body] : bodies3)
            if (body->inv_mass() > 0.0_r && body_pose(id, p)) previous[id] = p;
        for (const auto& [id, body] : bodies2)
            if (body->inv_mass() > 0.0_r && body_pose(id, p)) previous[id] = p;
    }
    std::unique_ptr<fp::FluidWorld3D> fluids;
    std::vector<std::pair<orchard::Entity, fp::FluidVolume3D*>> volumes;
    std::vector<orchard::Fluid> volume_looks;
    std::unique_ptr<fp::FluidWorld2D> fluids2;
    std::vector<std::pair<orchard::Entity, fp::FluidVolume2D*>> volumes2;
    mutable std::vector<fp::FluidRenderer2D> painters2;
    std::string asset_root;
    TerrainSource terrain_source;
    std::vector<std::pair<orchard::Entity, fp::RigidBody3D*>> statics;
    std::size_t terrains = 0;
    std::size_t scatter_colliders = 0;
    std::vector<std::string> warnings;
    std::map<std::string, std::optional<Trunk>> trunks;

    struct Push {
        orchard::Vec3 accel;
        bool local = false;
    };
    std::unordered_map<std::uint64_t, Push> pushes;

    struct Walker {
        Real gravity_scale = 1.0_r;
        bool grounded = false;
        Real jump_lock = 0.0_r;
    };
    std::unordered_map<std::uint64_t, Walker> walkers;

    std::string resolve(const std::string& file) const {
        return orchard::terrain_file_path(asset_root, file);
    }

    bool models_z_up() const {
        return settings.up.z > 0.5f;
    }

    void warn(const std::string& w) {
        if (warnings.size() < 64 && std::find(warnings.begin(), warnings.end(), w) == warnings.end()) warnings.push_back(w);
    }

    const Trunk* trunk_for(const std::string& model) {
        auto it = trunks.find(model);
        if (it == trunks.end()) {
            std::string why;
            const std::shared_ptr<const ModelGeometry> g = load_model_geometry(resolve(model), false, &why);
            std::optional<Trunk> t;
            if (g && !g->empty()) t = trunk_of(*g);
            else warn("scatter model " + model + ": " + why);
            it = trunks.emplace(model, t).first;
        }
        return it->second ? &*it->second : nullptr;
    }

    void build_terrain_slices(const orchard::Scene& s) {
        const orchard::Quat frame = orchard::terrain_frame(settings.up);
        for (orchard::Entity e : s.entities()) {
            const orchard::Terrain* t = s.terrain(e);
            if (!t) continue;
            std::shared_ptr<const orchard::TerrainHeights> heights = terrain_source ? terrain_source(e, *t) : nullptr;
            if (!heights || !heights->valid()) {
                auto made = std::make_shared<orchard::TerrainHeights>();
                std::string why;
                if (!orchard::build_terrain_heights(*t, asset_root, *made, &why) && !why.empty()) warn(s.name(e) + ": " + why);
                heights = made;
            }
            if (!heights->valid()) continue;
            const orchard::Vec3 pos = world_position(s, e);
            const orchard::Quat rot = orchard::normalize(world_rotation(s, e) * frame);
            const orchard::Vec3 ts = terrain_axis_scale(world_scale(s, e), frame);
            const orchard::Vec3 to_up = orchard::rotate(rot, orchard::Vec3{0.0f, 1.0f, 0.0f});
            if (to_up.y < 0.999f) {
                warn(s.name(e) + ": only an unrotated, Y-up terrain can be sliced for 2D play");
                continue;
            }
            const float lz = (0.0f - pos.z) / std::max(1e-6f, ts.z);
            const orchard::Vec3 first = heights->local_position(0, 0), last = heights->local_position(heights->width - 1, heights->depth - 1);
            if (lz < std::min(first.z, last.z) || lz > std::max(first.z, last.z)) continue;
            const int n = std::clamp(heights->width, 2, 1024);
            fp::RigidBody2D& body = sim2->add_body(fp::PhysicsFlags::DefaultStatic);
            fp::Material mat(fp::MassSpec::from_density(1000.0_r), std::max(0.0f, t->friction), std::max(0.0f, t->restitution));
            RVec2 prev{};
            for (int i = 0; i < n; ++i) {
                const float lx = first.x + (last.x - first.x) * static_cast<float>(i) / static_cast<float>(n - 1);
                const RVec2 at{static_cast<Real>(lx * ts.x), static_cast<Real>(heights->height_at(lx, lz) * ts.y)};
                if (i > 0) body.add_collider(fp::Shape2D::make_edge(prev, at), mat, fp::Transform2D(), fp::CollisionFilter{}, false);
                prev = at;
            }
            if (const orchard::Body* owner = s.body(e)) body.set_collision_filter(owner->collision_layer, owner->collision_mask);
            body.set_position(RVec2{pos.x, pos.y});
            body.mark_mass_dirty();
            owners[&body] = e;
            ++colliders;
            ++terrains;
        }
    }

    void build_terrains(const orchard::Scene& s) {
        if (!sim3) {
            if (sim2) build_terrain_slices(s);
            return;
        }
        const orchard::Quat frame = orchard::terrain_frame(settings.up);
        for (orchard::Entity e : s.entities()) {
            const orchard::Terrain* t = s.terrain(e);
            if (!t) continue;
            std::shared_ptr<const orchard::TerrainHeights> heights = terrain_source ? terrain_source(e, *t) : nullptr;
            if (!heights || !heights->valid()) {
                auto made = std::make_shared<orchard::TerrainHeights>();
                std::string why;
                if (!orchard::build_terrain_heights(*t, asset_root, *made, &why) && !why.empty()) warn(s.name(e) + ": " + why);
                heights = made;
            }
            if (!heights->valid()) continue;
            const orchard::Vec3 pos = world_position(s, e);
            const orchard::Quat rot = orchard::normalize(world_rotation(s, e) * frame);
            const orchard::Vec3 ts = terrain_axis_scale(world_scale(s, e), frame);
            const orchard::Body* owner = s.body(e);
            fp::HeightfieldShape hf;
            hf.columns = heights->width;
            hf.rows = heights->depth;
            hf.cell_x = std::max(1e-4_r, static_cast<Real>(heights->spacing_x() * ts.x));
            hf.cell_z = std::max(1e-4_r, static_cast<Real>(heights->spacing_z() * ts.z));
            hf.heights.reserve(heights->heights.size());
            for (float h : heights->heights) hf.heights.push_back(static_cast<Real>(h * ts.y));
            fp::RigidBody3D& body = sim3->add_body(fp::PhysicsFlags::DefaultStatic);
            fp::Material3D mat(fp::MassSpec3D::from_density(1000.0_r), std::max(0.0f, t->friction), std::max(0.0f, t->restitution));
            body.add_collider(fp::Shape3D::make_heightfield(std::move(hf)), mat, fp::Transform3D(), fp::CollisionFilter{}, false);
            if (owner) body.set_collision_filter(owner->collision_layer, owner->collision_mask);
            body.set_position(v3(pos));
            body.set_orientation(to_fizmo(rot));
            body.mark_mass_dirty();
            owners[&body] = e;
            statics.emplace_back(e, &body);
            ++colliders;
            ++terrains;
            struct TrunkCell {
                std::vector<RVec3> vertices;
                std::vector<std::array<std::uint32_t, 3>> triangles;
            };
            const float cell = 64.0f;
            const int sides = 8;
            std::map<std::pair<int, int>, TrunkCell> cells;
            for (const orchard::ScatterInstance& inst : orchard::scatter_instances(*t, *heights)) {
                if (!inst.collide) continue;
                const Trunk* trunk = trunk_for(inst.model);
                if (!trunk) continue;
                const std::pair<int, int> key{static_cast<int>(std::floor(inst.position.x / cell)), static_cast<int>(std::floor(inst.position.z / cell))};
                const orchard::Vec3 cell_center{(static_cast<float>(key.first) + 0.5f) * cell, 0.0f, (static_cast<float>(key.second) + 0.5f) * cell};
                TrunkCell& tc = cells[key];
                const auto base = static_cast<std::uint32_t>(tc.vertices.size());
                const float r = trunk->radius * inst.scale;
                const float y0 = trunk->bottom * inst.scale, y1 = (trunk->bottom + trunk->height) * inst.scale;
                for (int k = 0; k < sides; ++k) {
                    const float a = static_cast<float>(k) * 2.0f * orchard::pi / static_cast<float>(sides);
                    for (float y : {y0, y1}) {
                        const orchard::Vec3 local = inst.position + orchard::rotate(inst.rotation, orchard::Vec3{r * std::cos(a), y, r * std::sin(a)});
                        tc.vertices.push_back(v3((local - cell_center) * ts));
                    }
                }
                for (int k = 0; k < sides; ++k) {
                    const std::uint32_t b0 = base + static_cast<std::uint32_t>(k * 2), t0 = b0 + 1;
                    const std::uint32_t b1 = base + static_cast<std::uint32_t>(((k + 1) % sides) * 2), t1 = b1 + 1;
                    tc.triangles.push_back({b0, t0, b1});
                    tc.triangles.push_back({b1, t0, t1});
                    if (k > 0 && k + 1 < sides) {
                        tc.triangles.push_back({base + 1, base + static_cast<std::uint32_t>((k + 1) * 2 + 1), base + static_cast<std::uint32_t>(k * 2 + 1)});
                        tc.triangles.push_back({base, base + static_cast<std::uint32_t>(k * 2), base + static_cast<std::uint32_t>((k + 1) * 2)});
                    }
                }
                ++scatter_colliders;
            }
            for (auto& kv : cells) {
                const orchard::Vec3 cell_center{(static_cast<float>(kv.first.first) + 0.5f) * cell, 0.0f, (static_cast<float>(kv.first.second) + 0.5f) * cell};
                fp::RigidBody3D& group = sim3->add_body(fp::PhysicsFlags::DefaultStatic);
                fp::Material3D tm(fp::MassSpec3D::from_density(1000.0_r), 0.5_r, 0.0_r);
                group.add_collider(fp::Shape3D::make_mesh(std::move(kv.second.vertices), std::move(kv.second.triangles), true), tm, fp::Transform3D(), fp::CollisionFilter{}, false);
                if (owner) group.set_collision_filter(owner->collision_layer, owner->collision_mask);
                group.set_position(v3(pos + orchard::rotate(rot, cell_center * ts)));
                group.set_orientation(to_fizmo(rot));
                group.mark_mass_dirty();
                owners[&group] = e;
                statics.emplace_back(e, &group);
                ++colliders;
            }
        }
    }

    static std::pair<std::uint64_t, std::uint64_t> key(orchard::Entity a, orchard::Entity b) {
        const std::uint64_t x = a.id(), y = b.id();
        return x < y ? std::make_pair(x, y) : std::make_pair(y, x);
    }

    void refresh_contacts() {
        contacts.clear();
        for (const auto& [k, n] : touching) {
            if (n > 0) contacts.push_back(Contact{orchard::Entity::from_id(k.first), orchard::Entity::from_id(k.second), false});
        }
    }

    void began(const void* a, const void* b) {
        const auto ia = owners.find(a), ib = owners.find(b);
        if (ia == owners.end() || ib == owners.end()) return;
        ++touching[key(ia->second, ib->second)];
    }

    void ended(const void* a, const void* b) {
        const auto ia = owners.find(a), ib = owners.find(b);
        if (ia == owners.end() || ib == owners.end()) return;
        auto it = touching.find(key(ia->second, ib->second));
        if (it != touching.end() && --it->second <= 0) touching.erase(it);
    }

    void place(orchard::Entity e) {
        if (!scene || !scene->alive(e)) return;
        const orchard::Vec3 pos = world_position(*scene, e);
        const orchard::Quat rot = world_rotation(*scene, e);
        if (dimension == orchard::Dimension::two_d) {
            auto it = bodies2.find(e.id());
            if (it == bodies2.end()) return;
            it->second->set_position(RVec2{pos.x, pos.y});
            it->second->set_angle(angle_2d(rot));
            it->second->wake();
        } else {
            auto it = bodies3.find(e.id());
            if (it == bodies3.end()) return;
            it->second->set_position(v3(pos));
            it->second->set_orientation(to_fizmo(rot));
            it->second->wake();
        }
    }

    template <class Config>
    void apply_common(Config& cfg) const {
        const orchard::PhysicsSettings& ph = settings.physics;
        cfg.velocity_iters = std::max(1, ph.velocity_iterations);
        cfg.position_iters = std::max(0, ph.position_iterations);
        cfg.warm_starting = ph.warm_starting;
        cfg.allow_sleeping = ph.sleeping;
        cfg.baumgarte_factor = ph.correction;
        cfg.world_half_extent = std::max(1.0_r, static_cast<Real>(ph.world_size));
        cfg.substeps = std::max(1, ph.substeps);
    }

    void apply_physics() {
        const orchard::PhysicsSettings& ph = settings.physics;
        if (sim2) {
            apply_common(sim2->config());
            sim2->config().velocity_iters = std::max(1, ph.velocity_iterations * 8 / 10);
            sim2->config().slop = std::max(0.0_r, static_cast<Real>(ph.slop) * 2.5_r);
        }
        if (sim3) {
            fp::Simulation3D::Config& cfg = sim3->config();
            apply_common(cfg);
            cfg.slop = std::max(0.0_r, static_cast<Real>(ph.slop));
            cfg.gyroscopic = ph.gyroscopic;
            cfg.continuous = ph.continuous;
            cfg.continuous_all = ph.continuous && ph.continuous_all;
            cfg.speculative_distance = std::clamp(static_cast<Real>(ph.speculative_distance), 0.0_r, cfg.broadphase_margin * 0.9_r);
            cfg.solver = ph.soft_solver ? fp::Solver3D::SoftStep : fp::Solver3D::Classic;
            cfg.broadphase = ph.octree ? fp::Broadphase3D::Octree : fp::Broadphase3D::DynamicTree;
            cfg.threads = std::clamp(ph.threads, 1, 64);
            cfg.simd = ph.wide_solver;
            cfg.deterministic = ph.deterministic;
            cfg.reuse_contacts = ph.reuse_contacts;
            cfg.rotation_aware_ccd = ph.spin_aware_ccd;
            cfg.soft_substeps = std::clamp(ph.soft_substeps, 1, 32);
            cfg.contact_hertz = std::max(0.0_r, static_cast<Real>(ph.contact_hertz));
            cfg.contact_damping_ratio = std::max(0.0_r, static_cast<Real>(ph.contact_damping));
            cfg.joint_hertz = std::max(0.0_r, static_cast<Real>(ph.joint_hertz));
            cfg.max_push_speed = std::max(0.0_r, static_cast<Real>(ph.push_speed));
            cfg.reuse_distance = std::min(cfg.reuse_distance, 0.00002_r);
            cfg.reuse_angle = std::min(cfg.reuse_angle, 0.0001_r);
        }
        if (sim2) {
            sim2->config().dynamic_tree = !ph.octree;
            sim2->config().continuous = ph.continuous;
            sim2->config().continuous_all = ph.continuous && ph.continuous_all;
        }
    }

    void build(orchard::Scene& s) {
        scene = &s;
        bodies2.clear();
        bodies3.clear();
        owners.clear();
        touching.clear();
        contacts.clear();
        colliders = 0;
        accumulator = 0.0_r;
        sim2.reset();
        sim3.reset();
        statics.clear();
        terrains = 0;
        scatter_colliders = 0;
        warnings.clear();
        trunks.clear();
        pushes.clear();
        walkers.clear();
        if (dimension == orchard::Dimension::two_d) {
            fp::Simulation2D::Config cfg;
            cfg.gravity = RVec2{settings.gravity.x, settings.gravity.y};
            cfg.friction_combine = to_fizmo(settings.friction_combine);
            cfg.restitution_combine = to_fizmo(settings.bounce_combine);
            cfg.fixed_dt = settings.fixed_dt;
            sim2 = std::make_unique<fp::Simulation2D>(cfg);
            sim2->on_contact_begin([this](const fp::ContactEvent& ev) { began(ev.body_a, ev.body_b); });
            sim2->on_contact_end([this](const fp::ContactEvent& ev) { ended(ev.body_a, ev.body_b); });
        } else {
            fp::Simulation3D::Config cfg;
            cfg.gravity = v3(settings.gravity);
            cfg.friction_combine = to_fizmo(settings.friction_combine);
            cfg.restitution_combine = to_fizmo(settings.bounce_combine);
            cfg.fixed_dt = settings.fixed_dt;
            sim3 = std::make_unique<fp::Simulation3D>(cfg);
            sim3->on_contact_begin([this](const fp::ContactEvent3D& ev) { began(ev.body_a, ev.body_b); });
            sim3->on_contact_end([this](const fp::ContactEvent3D& ev) { ended(ev.body_a, ev.body_b); });
        }
        apply_physics();
        for (orchard::Entity e : s.entities()) {
            const orchard::Body* b = s.body(e);
            if (!b) continue;
            const std::vector<Part> parts = parts_of(s, e);
            if (parts.empty()) continue;
            if (dimension == orchard::Dimension::two_d) add_2d(e, *b, parts);
            else add_3d(e, *b, parts);
        }
        build_terrains(s);
        build_fluids(s);
    }

    void add_2d(orchard::Entity e, const orchard::Body& b, const std::vector<Part>& parts) {
        const bool flatten = scene && scene->dimension() == orchard::Dimension::three_d;
        const orchard::Quat owner_rot = scene ? world_rotation(*scene, e) : orchard::Quat{};
        const float theta = angle_2d(owner_rot);
        std::size_t kept = 0;
        fp::RigidBody2D& body = sim2->add_body(flags_with_overrides(b));
        for (const Part& p : parts) {
            const orchard::Collider& c = p.collider;
            fp::Shape2D shape;
            const orchard::Vec3 d = orchard::rotate(owner_rot, p.offset);
            const orchard::Quat rw = orchard::normalize(owner_rot * p.rotation);
            const float phi = angle_2d(rw);
            const orchard::Quat rest = orchard::normalize(about_z(-phi) * rw);
            const orchard::Vec3 s3{std::fabs(c.size.x * p.scale.x), std::fabs(c.size.y * p.scale.y), std::fabs(c.size.z * p.scale.z)};
            const orchard::Vec3 ax = orchard::rotate(rest, orchard::Vec3{1.0f, 0.0f, 0.0f}), ay = orchard::rotate(rest, orchard::Vec3{0.0f, 1.0f, 0.0f}), az = orchard::rotate(rest, orchard::Vec3{0.0f, 0.0f, 1.0f});
            if (flatten) {
                const float ez = 0.5f * (std::fabs(ax.z) * s3.x + std::fabs(ay.z) * s3.y + std::fabs(az.z) * s3.z);
                if (std::fabs(d.z) - ez > 0.5f) continue;
            }
            const bool upright = std::fabs(ay.y) > 0.999f && std::fabs(ax.x) > 0.999f;
            const float ct = std::cos(-theta), st = std::sin(-theta);
            const RVec2 at{static_cast<Real>(ct * d.x - st * d.y), static_cast<Real>(st * d.x + ct * d.y)};
            const Real sx = std::max(0.001_r, static_cast<Real>(upright ? s3.x : std::fabs(ax.x) * s3.x + std::fabs(ay.x) * s3.y + std::fabs(az.x) * s3.z));
            const Real sy = std::max(0.001_r, static_cast<Real>(upright ? s3.y : std::fabs(ax.y) * s3.x + std::fabs(ay.y) * s3.y + std::fabs(az.y) * s3.z));
            const bool flat_scene_shape = !flatten && c.shape != orchard::ColliderShape::triangle;
            if (c.shape == orchard::ColliderShape::sphere) {
                shape = fp::Shape2D::make_circle(RVec2{0.0_r, 0.0_r}, static_cast<Real>(flatten ? std::max({s3.x, s3.y, s3.z}) : std::max(s3.x, s3.y)) * 0.5_r);
            } else if (c.shape == orchard::ColliderShape::capsule && (upright || std::fabs(ay.y) > 0.98f)) {
                const Real r = static_cast<Real>(flatten ? std::max(s3.x, s3.z) : s3.x) * 0.5_r;
                const Real half = std::max(0.0_r, static_cast<Real>(s3.y) * 0.5_r - r);
                shape = fp::Shape2D::make_capsule(RVec2{0.0_r, -half}, RVec2{0.0_r, half}, r);
            } else if (c.shape == orchard::ColliderShape::capsule && std::fabs(ay.x) > 0.98f) {
                const Real r = static_cast<Real>(std::max(s3.x, s3.z)) * 0.5_r;
                const Real half = std::max(0.0_r, static_cast<Real>(s3.y) * 0.5_r - r);
                shape = fp::Shape2D::make_capsule(RVec2{-half, 0.0_r}, RVec2{half, 0.0_r}, r);
            } else if (flat_scene_shape || ((c.shape == orchard::ColliderShape::box || c.shape == orchard::ColliderShape::rounded_box) && upright)) {
                shape = fp::Shape2D::make_box(sx * 0.5_r, sy * 0.5_r);
            } else {
                std::vector<orchard::Vec2> flat;
                for (const orchard::Vec3& q : outline_points(c, s3)) {
                    const orchard::Vec3 r = orchard::rotate(rest, q);
                    flat.push_back({r.x, r.y});
                }
                const std::vector<orchard::Vec2> h = hull_2d(std::move(flat));
                float area = 0.0f;
                for (std::size_t i = 0; i < h.size(); ++i) area += h[i].x * h[(i + 1) % h.size()].y - h[(i + 1) % h.size()].x * h[i].y;
                if (h.size() >= 3 && std::fabs(area) > 1e-6f) {
                    std::vector<RVec2> pts;
                    for (const orchard::Vec2& q : h) pts.push_back(RVec2{static_cast<Real>(q.x), static_cast<Real>(q.y)});
                    shape = fp::Shape2D::make_polygon(pts.data(), pts.size());
                } else {
                    shape = fp::Shape2D::make_box(sx * 0.5_r, sy * 0.5_r);
                }
            }
            Real density = std::max(0.0001_r, static_cast<Real>(c.density)) * (flatten ? static_cast<Real>(orchard::collider_density_unit(orchard::Dimension::three_d)) : 1.0_r);
            if (c.locked(orchard::collider_lock_mass) && c.mass > 0.0f) {
                if (flatten) {
                    const float volume = orchard::collider_volume(c, p.scale, orchard::Dimension::three_d);
                    if (volume > 0.0f) density = std::max(0.0001_r, static_cast<Real>(c.mass / volume));
                } else {
                    const Real area = fp::compute_shape_area(shape);
                    if (area > 0.0_r) density = static_cast<Real>(c.mass) / area;
                }
            }
            fp::Material mat(fp::MassSpec::from_density(density), c.friction, c.restitution);
            mat.friction_combine = to_fizmo(c.friction_combine);
            mat.restitution_combine = to_fizmo(c.bounce_combine);
            body.add_collider(shape, mat, fp::Transform2D(at, phi - theta), fp::CollisionFilter{}, c.sensor);
            ++colliders;
            ++kept;
        }
        if (kept == 0) {
            sim2->remove_body(body);
            return;
        }
        body.set_gravity_scale(b.gravity_scale);
        body.set_linear_damping(b.linear_damping);
        body.set_angular_damping(b.angular_damping);
        body.set_collision_filter(b.collision_layer, b.collision_mask);
        if (b.max_speed > 0.0f) body.set_max_linear_speed(b.max_speed);
        if (b.max_spin > 0.0f) body.set_max_angular_speed(b.max_spin);
        body.mark_mass_dirty();
        bodies2[e.id()] = &body;
        owners[&body] = e;
        place(e);
        start_motion(e, b);
    }

    void add_3d(orchard::Entity e, const orchard::Body& b, const std::vector<Part>& parts) {
        fp::RigidBody3D& body = sim3->add_body(flags_with_overrides(b));
        for (const Part& p : parts) {
            const orchard::Collider& c = p.collider;
            fp::Shape3D shape;
            std::vector<fp::Shape3D> extra;
            const Real sx = std::max(0.001_r, static_cast<Real>(c.size.x * p.scale.x));
            const Real sy = std::max(0.001_r, static_cast<Real>(c.size.y * p.scale.y));
            const Real sz = std::max(0.001_r, static_cast<Real>(c.size.z * p.scale.z));
            if (c.shape == orchard::ColliderShape::sphere) {
                shape = fp::Shape3D::make_sphere(RVec3{0.0_r, 0.0_r, 0.0_r}, std::max({sx, sy, sz}) * 0.5_r);
            } else if (c.shape == orchard::ColliderShape::capsule) {
                const Real r = std::max(sx, sz) * 0.5_r;
                const Real half = std::max(0.0_r, sy * 0.5_r - r);
                shape = fp::Shape3D::make_capsule(RVec3{0.0_r, -half, 0.0_r}, RVec3{0.0_r, half, 0.0_r}, r);
            } else if (c.shape == orchard::ColliderShape::cylinder) {
                shape = fp::Shape3D::make_cylinder(sy * 0.5_r, std::max(sx, sz) * 0.5_r);
            } else if (c.shape == orchard::ColliderShape::cone) {
                shape = fp::Shape3D::make_cone(sy * 0.5_r, std::max(sx, sz) * 0.5_r);
            } else if (c.shape == orchard::ColliderShape::rounded_box) {
                const Real r = std::clamp(static_cast<Real>(c.rounding), 0.0_r, std::min({sx, sy, sz}) * 0.5_r - 0.0005_r);
                shape = fp::Shape3D::make_rounded_box(sx * 0.5_r, sy * 0.5_r, sz * 0.5_r, r);
            } else if (c.shape == orchard::ColliderShape::sphere_section) {
                const fp::SphereSection3D section = section_of(c, std::max({sx, sy, sz}));
                if (section.is_convex() || section.is_full_sphere()) {
                    shape = fp::Shape3D::make_sphere_section(section);
                } else {
                    std::vector<fp::Shape3D> pieces = fp::Shape3D::make_sphere_section_parts(section);
                    if (!pieces.empty()) {
                        shape = pieces.back();
                        pieces.pop_back();
                        for (const fp::Shape3D& piece : pieces) extra.push_back(piece);
                    } else {
                        shape = fp::Shape3D::make_sphere(RVec3{0.0_r, 0.0_r, 0.0_r}, std::max({sx, sy, sz}) * 0.5_r);
                    }
                }
            } else if (c.shape == orchard::ColliderShape::triangle) {
                std::vector<RVec3> pts;
                for (const orchard::Vec3& q : orchard::triangle_prism_points(c, p.scale)) pts.push_back(RVec3{static_cast<Real>(q.x), static_cast<Real>(q.y), static_cast<Real>(q.z)});
                shape = fp::Shape3D::make_convex(pts);
            } else if (c.shape == orchard::ColliderShape::convex_hull || c.shape == orchard::ColliderShape::mesh) {
                const bool as_mesh = c.shape == orchard::ColliderShape::mesh && b.type == orchard::BodyType::fixed;
                std::string why;
                const orchard::Vec3 scale{c.size.x * p.scale.x, c.size.y * p.scale.y, c.size.z * p.scale.z};
                if (std::optional<fp::Shape3D> made = model_shape(resolve(c.source), models_z_up(), scale, as_mesh, &why)) {
                    shape = *made;
                    if (c.shape == orchard::ColliderShape::mesh && !as_mesh) warn(c.source + ": mesh colliders only work on fixed bodies, so this one uses a convex hull");
                } else {
                    warn((c.source.empty() ? std::string("model collider") : c.source) + ": " + (why.empty() ? std::string("no model") : why) + "; using a box");
                    shape = fp::Shape3D::make_box(sx * 0.5_r, sy * 0.5_r, sz * 0.5_r);
                }
            } else {
                shape = fp::Shape3D::make_box(sx * 0.5_r, sy * 0.5_r, sz * 0.5_r);
            }
            Real density = std::max(0.0001_r, static_cast<Real>(c.density)) * 1000.0_r;
            if (c.locked(orchard::collider_lock_mass) && c.mass > 0.0f) {
                Real volume = fp::compute_shape_volume(shape);
                for (const fp::Shape3D& piece : extra) volume += fp::compute_shape_volume(piece);
                if (volume > 0.0_r) density = static_cast<Real>(c.mass) / volume;
            }
            fp::Material3D mat(fp::MassSpec3D::from_density(density), c.friction, c.restitution);
            mat.friction_combine = to_fizmo(c.friction_combine);
            mat.restitution_combine = to_fizmo(c.bounce_combine);
            mat.rolling_friction = std::max(0.0_r, static_cast<Real>(c.rolling_friction));
            mat.spinning_friction = std::max(0.0_r, static_cast<Real>(c.spinning_friction));
            mat.anisotropic_x = std::max(0.0_r, static_cast<Real>(c.grip.x));
            mat.anisotropic_y = std::max(0.0_r, static_cast<Real>(c.grip.y));
            mat.anisotropic_z = std::max(0.0_r, static_cast<Real>(c.grip.z));
            body.add_collider(shape, mat, fp::Transform3D(v3(p.offset), to_fizmo(p.rotation)), fp::CollisionFilter{}, c.sensor);
            ++colliders;
            for (const fp::Shape3D& piece : extra) {
                body.add_collider(piece, mat, fp::Transform3D(v3(p.offset), to_fizmo(p.rotation)), fp::CollisionFilter{}, c.sensor);
                ++colliders;
            }
        }
        body.set_gravity_scale(b.gravity_scale);
        body.set_linear_damping(b.linear_damping);
        body.set_angular_damping(b.angular_damping);
        body.set_collision_filter(b.collision_layer, b.collision_mask);
        if (b.max_speed > 0.0f) body.set_max_linear_speed(b.max_speed);
        if (b.max_spin > 0.0f) body.set_max_angular_speed(b.max_spin);
        body.set_quality(to_fizmo(b.quality));
        body.set_continuous_mode(to_fizmo(b.continuous));
        body.set_velocity_iterations(b.velocity_iterations);
        body.set_position_iterations(b.position_iterations);
        body.set_sleep_tolerances(b.sleep_speed, b.sleep_spin);
        body.set_sleep_time(b.sleep_delay);
        body.mark_mass_dirty();
        bodies3[e.id()] = &body;
        owners[&body] = e;
        place(e);
        start_motion(e, b);
    }

    void start_motion(orchard::Entity e, const orchard::Body& b) {
        if (b.type == orchard::BodyType::fixed) return;
        const orchard::Quat rot = b.motion_local ? world_rotation(*scene, e) : orchard::Quat{};
        const orchard::Vec3 v = b.motion_local ? orchard::rotate(rot, b.start_velocity) : b.start_velocity;
        const orchard::Vec3 w = (b.motion_local ? orchard::rotate(rot, b.start_spin) : b.start_spin) * static_cast<float>(deg_to_rad);
        const auto zero = [](orchard::Vec3 x) { return x.x == 0.0f && x.y == 0.0f && x.z == 0.0f; };
        if (auto it = bodies2.find(e.id()); it != bodies2.end()) {
            if (!zero(v)) it->second->set_linear_velocity(RVec2{v.x, v.y});
            if (w.z != 0.0f) it->second->set_angular_velocity(static_cast<Real>(w.z));
        }
        if (auto it = bodies3.find(e.id()); it != bodies3.end()) {
            if (!zero(v)) it->second->set_linear_velocity(v3(v));
            if (!zero(w)) it->second->set_angular_velocity(v3(w));
        }
        if (!zero(b.acceleration)) pushes[e.id()] = Push{b.acceleration, b.motion_local};
        else pushes.erase(e.id());
    }

    void apply_pushes() {
        const Real dt = static_cast<Real>(settings.fixed_dt);
        for (auto it = pushes.begin(); it != pushes.end();) {
            const orchard::Entity e = orchard::Entity::from_id(it->first);
            const Push& p = it->second;
            bool found = false;
            if (auto b2 = bodies2.find(it->first); b2 != bodies2.end()) {
                found = true;
                fp::RigidBody2D& body = *b2->second;
                orchard::Vec3 a = p.accel;
                if (p.local) a = orchard::rotate(about_z(static_cast<float>(body.angle())), a);
                body.wake();
                if (body.inv_mass() > 0.0_r) body.apply_force(RVec2{a.x, a.y} * body.mass());
                else body.set_linear_velocity(body.linear_velocity() + RVec2{a.x, a.y} * dt);
            }
            if (auto b3 = bodies3.find(it->first); b3 != bodies3.end()) {
                found = true;
                fp::RigidBody3D& body = *b3->second;
                orchard::Vec3 a = p.accel;
                if (p.local) a = orchard::rotate(to_orchard(body.orientation()), a);
                body.wake();
                if (body.inv_mass() > 0.0_r) body.apply_force(v3(a) * body.mass());
                else body.set_linear_velocity(body.linear_velocity() + v3(a) * dt);
            }
            if (!found || !scene || !scene->alive(e)) it = pushes.erase(it);
            else ++it;
        }
    }

    void write_back() {
        if (!scene) return;
        auto store = [this](orchard::Entity e, orchard::Vec3 world_pos, orchard::Quat world_rot) {
            orchard::Transform* t = scene->transform(e);
            if (!t) return;
            const orchard::Entity parent = scene->parent(e);
            if (!scene->alive(parent)) {
                t->position = world_pos;
                t->rotation = world_rot;
                return;
            }
            const orchard::Vec3 pp = world_position(*scene, parent);
            const orchard::Quat pr = world_rotation(*scene, parent);
            const orchard::Vec3 ps = world_scale(*scene, parent);
            const orchard::Vec3 local = orchard::rotate(conjugate(pr), world_pos - pp);
            t->position = {ps.x != 0.0f ? local.x / ps.x : local.x, ps.y != 0.0f ? local.y / ps.y : local.y, ps.z != 0.0f ? local.z / ps.z : local.z};
            t->rotation = orchard::normalize(conjugate(pr) * world_rot);
        };
        if (sim2) {
            for (auto& [id, body] : bodies2) {
                const orchard::Entity e = orchard::Entity::from_id(id);
                if (!scene->alive(e) || (body->inv_mass() <= 0.0_r && !body->has(fp::PhysicsFlags::LinearVelocity))) continue;
                const RVec2 p = body->position();
                const orchard::Vec3 old = world_position(*scene, e);
                const orchard::Quat old_rot = world_rotation(*scene, e);
                const float keep_tilt = angle_2d(old_rot);
                orchard::Quat rot = about_z(static_cast<float>(body->angle())) * conjugate(about_z(keep_tilt)) * old_rot;
                store(e, orchard::Vec3{static_cast<float>(p.x), static_cast<float>(p.y), old.z}, orchard::normalize(rot));
            }
        }
        if (sim3) {
            for (auto& [id, body] : bodies3) {
                const orchard::Entity e = orchard::Entity::from_id(id);
                if (!scene->alive(e) || (body->inv_mass() <= 0.0_r && !body->has(fp::PhysicsFlags::LinearVelocity))) continue;
                store(e, ov(body->position()), to_orchard(body->orientation()));
            }
        }
    }

    static fp::SandCell2D sand_cell_for(const orchard::Fluid& f) {
        const std::string& p = f.preset;
        if (p == "oil") return fp::SandCell2D::oil;
        if (p == "lava") return fp::SandCell2D::lava;
        if (p == "sand" || p == "mud") return fp::SandCell2D::sand;
        if (p == "acid") return fp::SandCell2D::acid;
        return fp::SandCell2D::water;
    }

    fp::FluidMaterial material_for(const orchard::Fluid& f) const {
        fp::FluidMaterial mat = fp::FluidMaterial::water();
        mat.preset = fp::FluidPreset::custom;
        mat.density = std::max(1.0_r, static_cast<Real>(f.density));
        mat.viscosity = std::max(0.0_r, static_cast<Real>(f.viscosity));
        mat.surface_tension = std::max(0.0_r, static_cast<Real>(f.surface_tension));
        mat.linear_drag = static_cast<Real>(f.drag);
        mat.quadratic_drag = static_cast<Real>(f.quadratic_drag);
        mat.angular_drag = static_cast<Real>(f.angular_drag);
        mat.friction = static_cast<Real>(f.friction);
        mat.restitution = static_cast<Real>(f.restitution);
        mat.color = fp::FluidColor{f.color.x, f.color.y, f.color.z, f.opacity};
        return mat;
    }

    void build_fluids_2d(const orchard::Scene& s) {
        for (orchard::Entity e : s.entities()) {
            const orchard::Fluid* f = s.fluid(e);
            if (!f || !f->enabled) continue;
            if (!fluids2) {
                fp::FluidWorld2D::Config cfg;
                cfg.gravity = sim2->config().gravity;
                cfg.fixed_dt = static_cast<Real>(settings.fixed_dt);
                cfg.default_level = to_fizmo(settings.physics.fluid_level);
                cfg.quality = static_cast<fp::FluidQuality>(std::clamp(settings.physics.fluid_quality, 0, 3));
                cfg.threads = std::max(1, settings.physics.threads);
                fluids2 = std::make_unique<fp::FluidWorld2D>(cfg);
                fluids2->set_quality(cfg.quality);
                fluids2->set_threads(cfg.threads);
            }
            const orchard::Vec3 c = world_position(s, e);
            const orchard::Vec3 sc = world_scale(s, e);
            const Real hx = std::max(0.025_r, static_cast<Real>(std::fabs(f->size.x * sc.x)) * 0.5_r);
            const Real hy = std::max(0.025_r, static_cast<Real>(std::fabs(f->size.y * sc.y)) * 0.5_r);
            const fp::FluidLevel level = to_fizmo(fluid_level_for(*f, settings.physics.fluid_level));
            const fp::AABB box(RVec2{static_cast<Real>(c.x) - hx, static_cast<Real>(c.y) - hy}, RVec2{static_cast<Real>(c.x) + hx, static_cast<Real>(c.y) + hy});
            fp::FluidVolumeDef2D def = fp::FluidVolumeDef2D::box(level, box, material_for(*f), std::clamp(static_cast<Real>(f->fill), 0.0_r, 1.0_r));
            def.current = RVec2{static_cast<Real>(f->current.x), static_cast<Real>(f->current.y)};
            def.waves.swell_amplitude = static_cast<Real>(f->wave_height);
            def.sand.fill_cell = sand_cell_for(*f);
            def.flowing.keep_sources_full = f->infinite;
            const int q = std::clamp(settings.physics.fluid_quality, 0, 3);
            const Real longest = std::max(hx, hy) * 2.0_r;
            const Real across[4] = {64.0_r, 128.0_r, 192.0_r, 320.0_r};
            const Real sand_across[4] = {128.0_r, 256.0_r, 384.0_r, 512.0_r};
            if (f->resolution > 0.0f) def.cell_size = static_cast<Real>(f->resolution);
            else if (level == fp::FluidLevel::blocky) def.cell_size = std::max(0.01_r, longest / sand_across[q]);
            else def.cell_size = std::max(0.01_r, longest / across[q]);
            fp::FluidVolume2D& v = fluids2->add_volume(def);
            if (level != fp::FluidLevel::still) v.voxelize(*sim2);
            if (f->infinite && level == fp::FluidLevel::realistic) {
                fp::FluidEmitter2D em;
                em.position = RVec2{static_cast<Real>(c.x), static_cast<Real>(c.y) + hy * 0.8_r};
                em.radius = std::max(def.cell_size * 2.0_r, hx * 0.1_r);
                em.rate = 400.0_r;
                v.add_emitter(em);
            }
            volumes2.emplace_back(e, &v);
            volume_looks.push_back(*f);
        }
        painters2.assign(volumes2.size(), fp::FluidRenderer2D{});
    }

    void build_fluids(const orchard::Scene& s) {
        fluids.reset();
        volumes.clear();
        volume_looks.clear();
        fluids2.reset();
        volumes2.clear();
        painters2.clear();
        if (sim2) {
            build_fluids_2d(s);
            return;
        }
        if (!sim3) return;
        for (orchard::Entity e : s.entities()) {
            const orchard::Fluid* f = s.fluid(e);
            if (!f || !f->enabled) continue;
            if (!fluids) {
                fp::FluidWorld3D::Config cfg;
                cfg.gravity = sim3->config().gravity;
                cfg.fixed_dt = static_cast<Real>(settings.fixed_dt);
                cfg.default_level = to_fizmo(settings.physics.fluid_level);
                cfg.quality = static_cast<fp::FluidQuality>(std::clamp(settings.physics.fluid_quality, 0, 3));
                cfg.threads = std::max(1, settings.physics.threads);
                fluids = std::make_unique<fp::FluidWorld3D>(cfg);
                fluids->set_quality(cfg.quality);
            }
            const orchard::Vec3 c = world_position(s, e);
            const orchard::Vec3 sc = world_scale(s, e);
            const orchard::Vec3 size{std::max(0.05f, std::fabs(f->size.x * sc.x)), std::max(0.05f, std::fabs(f->size.y * sc.y)), std::max(0.05f, std::fabs(f->size.z * sc.z))};
            const RVec3 half{static_cast<Real>(size.x) * 0.5_r, static_cast<Real>(size.y) * 0.5_r, static_cast<Real>(size.z) * 0.5_r};
            fp::FluidMaterial mat = fp::FluidMaterial::water();
            mat.preset = fp::FluidPreset::custom;
            mat.density = std::max(1.0_r, static_cast<Real>(f->density));
            mat.viscosity = std::max(0.0_r, static_cast<Real>(f->viscosity));
            mat.surface_tension = std::max(0.0_r, static_cast<Real>(f->surface_tension));
            mat.linear_drag = static_cast<Real>(f->drag);
            mat.quadratic_drag = static_cast<Real>(f->quadratic_drag);
            mat.angular_drag = static_cast<Real>(f->angular_drag);
            mat.friction = static_cast<Real>(f->friction);
            mat.restitution = static_cast<Real>(f->restitution);
            mat.color = fp::FluidColor{f->color.x, f->color.y, f->color.z, f->opacity};
            const fp::FluidLevel level = to_fizmo(fluid_level_for(*f, settings.physics.fluid_level));
            fp::FluidVolumeDef def = fp::FluidVolumeDef::box(level, fp::AABB3D(v3(c) - half, v3(c) + half), mat, std::clamp(static_cast<Real>(f->fill), 0.0_r, 1.0_r));
            def.current = v3(f->current);
            def.waves.swell_amplitude = static_cast<Real>(f->wave_height);
            def.blocky.infinite_sources = f->infinite;
            def.flowing.keep_sources_full = f->infinite;
            const Real longest = std::max({half.x, half.y, half.z}) * 2.0_r;
            if (f->resolution > 0.0f) {
                def.cell_size = static_cast<Real>(f->resolution);
                def.particles.radius = static_cast<Real>(f->resolution) * 0.5_r;
            } else {
                def.cell_size = std::clamp(longest / 40.0_r, 0.1_r, 1.0_r);
                def.particles.radius = std::clamp(longest / 80.0_r, 0.04_r, 0.25_r);
            }
            if (level == fp::FluidLevel::blocky) def.cell_size = std::max(def.cell_size, 0.5_r);
            fp::FluidVolume3D& v = fluids->add_volume(def);
            if (level == fp::FluidLevel::blocky || level == fp::FluidLevel::flowing || level == fp::FluidLevel::waves) v.voxelize(*sim3);
            const orchard::Entity holder = s.body_owner(e);
            if (auto own = bodies3.find(holder.id()); s.alive(holder) && own != bodies3.end()) v.ignore_body(*own->second);
            volumes.emplace_back(e, &v);
            volume_looks.push_back(*f);
        }
    }

    void prune() {
        if (!scene) return;
        if (sim2) {
            for (auto it = bodies2.begin(); it != bodies2.end();) {
                if (!scene->alive(orchard::Entity::from_id(it->first))) {
                    owners.erase(it->second);
                    sim2->remove_body(*it->second);
                    it = bodies2.erase(it);
                } else {
                    ++it;
                }
            }
        }
        if (sim3) {
            for (auto it = bodies3.begin(); it != bodies3.end();) {
                if (!scene->alive(orchard::Entity::from_id(it->first))) {
                    owners.erase(it->second);
                    sim3->remove_body(*it->second);
                    it = bodies3.erase(it);
                } else {
                    ++it;
                }
            }
            for (auto it = statics.begin(); it != statics.end();) {
                if (!scene->alive(it->first)) {
                    owners.erase(it->second);
                    sim3->remove_body(*it->second);
                    it = statics.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    static orchard::Vec3 flatten_up(orchard::Vec3 v, orchard::Vec3 up) {
        return v - up * orchard::dot(v, up);
    }

    template <class Body>
    bool settles(const Body& b) const {
        if (!b.is_awake() || b.inv_mass() <= 0.0_r || !b.has(fp::PhysicsFlags::AllowSleep)) return false;
        const auto it = owners.find(&b);
        return it == owners.end() || walkers.count(it->second.id()) == 0;
    }

    void settle() {
        const Real rate = static_cast<Real>(settings.physics.settle);
        if (rate <= 0.0_r || !settings.physics.sleeping) return;
        const orchard::Vec3 g = settings.gravity;
        if (orchard::length(g) < 1e-4f) return;
        const orchard::Vec3 up = orchard::normalize(g * -1.0f);
        const Real dt = static_cast<Real>(settings.fixed_dt);
        const Real keep = std::exp(-rate * dt);
        constexpr Real slow_speed = 0.2_r;
        constexpr Real slow_spin = 1.0_r;
        if (sim3) {
            std::set<fp::RigidBody3D*> resting;
            for (const fp::ContactManifold3D& mf : sim3->contacts()) {
                if (mf.point_count <= 0) continue;
                const float facing = orchard::dot(ov(mf.normal), up);
                if (facing > 0.5f && mf.body_b) resting.insert(mf.body_b);
                if (facing < -0.5f && mf.body_a) resting.insert(mf.body_a);
            }
            for (fp::RigidBody3D* b : resting) {
                if (!settles(*b)) continue;
                const RVec3 v = b->linear_velocity();
                const RVec3 w = b->angular_velocity();
                const orchard::Vec3 tumble = flatten_up(ov(w), up);
                if (std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z) >= slow_speed || orchard::length(tumble) >= static_cast<float>(slow_spin)) continue;
                b->set_linear_velocity(v * keep);
                b->set_angular_velocity(w * keep);
            }
        }
        if (sim2) {
            std::set<fp::RigidBody2D*> resting;
            for (const fp::ContactManifold& mf : sim2->contacts()) {
                if (mf.point_count <= 0) continue;
                const float facing = static_cast<float>(mf.normal.x) * up.x + static_cast<float>(mf.normal.y) * up.y;
                if (facing > 0.5f && mf.body_b) resting.insert(mf.body_b);
                if (facing < -0.5f && mf.body_a) resting.insert(mf.body_a);
            }
            for (fp::RigidBody2D* b : resting) {
                if (!settles(*b)) continue;
                const RVec2 v = b->linear_velocity();
                const Real w = b->angular_velocity();
                if (std::sqrt(v.x * v.x + v.y * v.y) >= slow_speed || std::fabs(w) >= slow_spin) continue;
                b->set_linear_velocity(v * keep);
                b->set_angular_velocity(w * keep);
            }
        }
    }

    void step_once() {
        prune();
        apply_pushes();
        if (sim2 && fluids2) fluids2->step(*sim2, static_cast<Real>(settings.fixed_dt));
        if (sim2) sim2->step(settings.fixed_dt);
        if (sim3 && fluids) fluids->step(*sim3, static_cast<Real>(settings.fixed_dt));
        if (sim3) sim3->step(settings.fixed_dt);
        settle();
        refresh_contacts();
    }
};

FlatOutline flat_outline(const orchard::Scene& scene, orchard::Entity e, const orchard::Collider& c) {
    FlatOutline out;
    if (!scene.alive(e)) return out;
    const orchard::Entity owner = scene.body_owner(e);
    const orchard::Vec3 owner_pos = world_position(scene, scene.alive(owner) ? owner : e);
    const orchard::Vec3 pos = world_position(scene, e);
    const orchard::Quat rot = world_rotation(scene, e);
    const orchard::Vec3 scale = world_scale(scene, e);
    const orchard::Vec3 center = pos + orchard::rotate(rot, c.offset * scale);
    const float phi = angle_2d(rot);
    const orchard::Quat rest = orchard::normalize(about_z(-phi) * rot);
    const orchard::Vec3 s3{std::fabs(c.size.x * scale.x), std::fabs(c.size.y * scale.y), std::fabs(c.size.z * scale.z)};
    const orchard::Vec3 ax = orchard::rotate(rest, orchard::Vec3{1.0f, 0.0f, 0.0f}), ay = orchard::rotate(rest, orchard::Vec3{0.0f, 1.0f, 0.0f}), az = orchard::rotate(rest, orchard::Vec3{0.0f, 0.0f, 1.0f});
    const float ez = 0.5f * (std::fabs(ax.z) * s3.x + std::fabs(ay.z) * s3.y + std::fabs(az.z) * s3.z);
    out.in_plane = !(scene.dimension() == orchard::Dimension::three_d && std::fabs(center.z - owner_pos.z) - ez > 0.5f);
    out.center = {center.x, center.y};
    if (c.shape == orchard::ColliderShape::sphere) {
        out.circle = true;
        out.radius = std::max({s3.x, s3.y, s3.z}) * 0.5f;
        return out;
    }
    std::vector<orchard::Vec2> flat;
    if (c.shape == orchard::ColliderShape::capsule) {
        const float r = std::max(s3.x, s3.z) * 0.5f, half = std::max(0.0f, s3.y * 0.5f - r);
        const float al = std::sqrt(ay.x * ay.x + ay.y * ay.y);
        const orchard::Vec2 dir = al > 1e-4f ? orchard::Vec2{ay.x / al, ay.y / al} : orchard::Vec2{0.0f, 1.0f};
        const float reach = half * al;
        const float base = std::atan2(dir.y, dir.x);
        for (int k = 0; k <= 8; ++k) {
            const float a = base - 1.5707963f + 3.1415926f * static_cast<float>(k) / 8.0f;
            flat.push_back({dir.x * reach + r * std::cos(a), dir.y * reach + r * std::sin(a)});
        }
        for (int k = 0; k <= 8; ++k) {
            const float a = base + 1.5707963f + 3.1415926f * static_cast<float>(k) / 8.0f;
            flat.push_back({-dir.x * reach + r * std::cos(a), -dir.y * reach + r * std::sin(a)});
        }
    } else {
        for (const orchard::Vec3& q : outline_points(c, s3)) {
            const orchard::Vec3 r = orchard::rotate(rest, q);
            flat.push_back({r.x, r.y});
        }
    }
    const std::vector<orchard::Vec2> h = c.shape == orchard::ColliderShape::capsule ? flat : hull_2d(std::move(flat));
    const float cp = std::cos(phi), sp = std::sin(phi);
    for (const orchard::Vec2& q : h) out.points.push_back({center.x + cp * q.x - sp * q.y, center.y + sp * q.x + cp * q.y});
    return out;
}

World::World(orchard::Dimension dimension, Settings settings) : m(std::make_unique<Impl>()) {
    m->dimension = dimension;
    m->settings = settings;
}

World::~World() = default;

void World::build(orchard::Scene& scene) {
    m->build(scene);
}

void World::clear() {
    m->bodies2.clear();
    m->bodies3.clear();
    m->owners.clear();
    m->touching.clear();
    m->contacts.clear();
    m->sim2.reset();
    m->fluids.reset();
    m->volumes.clear();
    m->fluids2.reset();
    m->volumes2.clear();
    m->painters2.clear();
    m->volume_looks.clear();
    m->statics.clear();
    m->terrains = 0;
    m->scatter_colliders = 0;
    m->trunks.clear();
    m->sim3.reset();
    m->scene = nullptr;
    m->colliders = 0;
}

std::size_t World::fluid_count() const {
    return m->volumes.size() + m->volumes2.size();
}

void World::fluid_draws(std::vector<FluidDraw>& out) const {
    out.clear();
    for (std::size_t n = 0; n < m->volumes2.size(); ++n) {
        const fp::FluidVolume2D& v = *m->volumes2[n].second;
        const orchard::Fluid& look = m->volume_looks[n];
        FluidDraw d;
        d.entity = m->volumes2[n].first;
        d.flat = true;
        d.color = look.color;
        d.opacity = look.opacity;
        d.reflectivity = look.reflectivity;
        d.reflections = look.reflections;
        const fp::AABB b = v.bounds();
        d.image_min = orchard::Vec3{static_cast<float>(b.min.x), static_cast<float>(b.min.y), 0.0f};
        d.image_max = orchard::Vec3{static_cast<float>(b.max.x), static_cast<float>(b.max.y), 0.0f};
        d.origin = d.image_min;
        d.size = d.image_max - d.image_min;
        d.cell = static_cast<float>(v.cell_size());
        switch (v.level()) {
        case fp::FluidLevel::still: d.level = orchard::FluidLevel::still; break;
        case fp::FluidLevel::blocky: d.level = orchard::FluidLevel::blocky; break;
        case fp::FluidLevel::flowing: d.level = orchard::FluidLevel::flowing; break;
        case fp::FluidLevel::waves: d.level = orchard::FluidLevel::waves; break;
        case fp::FluidLevel::realistic: d.level = orchard::FluidLevel::realistic; break;
        }
        d.level_height = v.level() == fp::FluidLevel::still ? static_cast<float>(v.still_level()) : d.size.y * std::clamp(look.fill, 0.0f, 1.0f);
        if (v.level() != fp::FluidLevel::still && n < m->painters2.size()) {
            fp::FluidImageSettings2D is;
            const Real w = std::max(1e-3_r, b.max.x - b.min.x), h = std::max(1e-3_r, b.max.y - b.min.y);
            if (v.level() == fp::FluidLevel::blocky) {
                is.width = std::max(1, v.cells_x());
                is.height = std::max(1, v.cells_y());
            } else {
                is.width = std::clamp(static_cast<int>(std::ceil(w / std::max(1e-4_r, v.cell_size()) * 2.0_r)), 32, 640);
                is.height = std::clamp(static_cast<int>(std::ceil(static_cast<Real>(is.width) * h / w)), 16, 640);
            }
            is.region = b;
            is.flip_y = true;
            is.opacity = static_cast<Real>(std::clamp(look.opacity, 0.05f, 1.0f));
            const std::vector<std::uint8_t>& rgba = m->painters2[n].render(v, is);
            d.image_width = m->painters2[n].width();
            d.image_height = m->painters2[n].height();
            d.image = rgba;
        }
        out.push_back(std::move(d));
    }
    for (std::size_t n = 0; n < m->volumes.size(); ++n) {
        const fp::FluidVolume3D& v = *m->volumes[n].second;
        const orchard::Fluid& look = m->volume_looks[n];
        FluidDraw d;
        d.entity = m->volumes[n].first;
        d.color = look.color;
        d.opacity = look.opacity;
        d.reflectivity = look.reflectivity;
        d.reflections = look.reflections;
        d.origin = ov(v.origin());
        d.axis_u = ov(v.world_dir(RVec3{1.0_r, 0.0_r, 0.0_r}));
        d.axis_up = ov(v.up());
        d.axis_w = ov(v.world_dir(RVec3{0.0_r, 0.0_r, 1.0_r}));
        d.size = ov(v.size());
        switch (v.level()) {
        case fp::FluidLevel::still:
            d.level = orchard::FluidLevel::still;
            d.level_height = static_cast<float>(v.still_level());
            break;
        case fp::FluidLevel::blocky:
        case fp::FluidLevel::flowing: {
            d.level = v.level() == fp::FluidLevel::blocky ? orchard::FluidLevel::blocky : orchard::FluidLevel::flowing;
            d.cell = static_cast<float>(v.cell_size());
            d.nx = v.cells_x();
            d.ny = v.cells_y();
            d.nz = v.cells_z();
            const std::vector<Real>& fill = v.cell_fill();
            d.values.reserve(fill.size());
            for (Real x : fill) d.values.push_back(static_cast<float>(x));
            break;
        }
        case fp::FluidLevel::waves: {
            d.level = orchard::FluidLevel::waves;
            d.cell = static_cast<float>(v.column_size());
            d.nx = v.columns_x();
            d.nz = v.columns_z();
            const std::vector<Real>& h = v.heights();
            const std::vector<Real>& depth = v.depths();
            d.values.reserve(h.size() * 2);
            for (Real x : h) d.values.push_back(static_cast<float>(x));
            for (Real x : depth) d.values.push_back(static_cast<float>(x));
            break;
        }
        case fp::FluidLevel::realistic: {
            d.level = orchard::FluidLevel::realistic;
            d.radius = static_cast<float>(v.particle_radius());
            const fp::FluidSurfaceMesh3D& mesh = v.surface_mesh();
            if (!mesh.empty()) {
                d.mesh_positions.reserve(mesh.positions.size());
                for (const RVec3& x : mesh.positions) d.mesh_positions.push_back(ov(x));
                d.mesh_normals.reserve(mesh.normals.size());
                for (const RVec3& x : mesh.normals) d.mesh_normals.push_back(ov(x));
                d.mesh_indices = mesh.indices;
                d.mesh_foam.reserve(mesh.foam.size());
                for (Real x : mesh.foam) d.mesh_foam.push_back(static_cast<float>(x));
                d.spray.reserve(mesh.spray.size());
                for (const RVec3& x : mesh.spray) d.spray.push_back(ov(x));
                break;
            }
            const std::vector<RVec3>& p = v.positions();
            d.particles.reserve(p.size());
            for (const RVec3& x : p) d.particles.push_back(ov(x));
            break;
        }
        }
        out.push_back(std::move(d));
    }
}

float World::fluid_surface(orchard::Vec3 point) const {
    if (m->fluids2) return static_cast<float>(m->fluids2->surface_height(RVec2{static_cast<Real>(point.x), static_cast<Real>(point.y)}));
    return m->fluids ? static_cast<float>(m->fluids->surface_height(v3(point))) : -1e30f;
}

float World::fluid_depth(orchard::Vec3 point) const {
    if (m->fluids2) return static_cast<float>(m->fluids2->depth_at(RVec2{static_cast<Real>(point.x), static_cast<Real>(point.y)}));
    return m->fluids ? static_cast<float>(m->fluids->depth_at(v3(point))) : 0.0f;
}

orchard::Vec3 World::fluid_velocity(orchard::Vec3 point) const {
    if (m->fluids2) {
        const RVec2 v = m->fluids2->velocity_at(RVec2{static_cast<Real>(point.x), static_cast<Real>(point.y)});
        return orchard::Vec3{static_cast<float>(v.x), static_cast<float>(v.y), 0.0f};
    }
    return m->fluids ? ov(m->fluids->velocity_at(v3(point))) : orchard::Vec3{};
}

std::size_t World::fluid_brush(int brush, orchard::Vec3 point, float radius, orchard::Vec3 velocity, int material) {
    return fluid_brush(static_cast<FluidBrush>(std::clamp(brush, 0, 6)), point, radius, velocity, material);
}

std::size_t World::fluid_brush(FluidBrush brush, orchard::Vec3 point, float radius, orchard::Vec3 velocity, int material) {
    if (!m->fluids2) return 0;
    const RVec2 p{static_cast<Real>(point.x), static_cast<Real>(point.y)};
    const RVec2 vel{static_cast<Real>(velocity.x), static_cast<Real>(velocity.y)};
    const Real r = std::max(0.001_r, static_cast<Real>(radius));
    if (brush == FluidBrush::explode) {
        if (m->sim2) m->fluids2->explode(*m->sim2, p, r, std::max(0.0_r, static_cast<Real>(length(velocity))) + 1.0_r);
        return 1;
    }
    if (brush == FluidBrush::push) {
        m->fluids2->push(p, r, vel);
        return 1;
    }
    std::size_t changed = 0;
    for (auto& pair : m->volumes2) {
        fp::FluidVolume2D& v = *pair.second;
        const fp::AABB b = v.bounds();
        if (p.x + r < b.min.x || p.x - r > b.max.x || p.y + r < b.min.y || p.y - r > b.max.y) continue;
        switch (brush) {
        case FluidBrush::pour:
            if (v.level() == fp::FluidLevel::realistic) changed += v.add_fluid_circle(p, r, vel);
            else if (v.level() == fp::FluidLevel::blocky) changed += v.paint_cells(p, r, v.def().sand.fill_cell, 1.0_r, false);
            else if (v.level() == fp::FluidLevel::flowing) {
                v.fill_box(fp::AABB(p - RVec2{r, r}, p + RVec2{r, r}), 1.0_r);
                ++changed;
            } else if (v.level() == fp::FluidLevel::waves) {
                v.disturb(p, r, r);
                ++changed;
            }
            break;
        case FluidBrush::erase:
            if (v.level() == fp::FluidLevel::realistic) changed += v.remove_fluid_circle(p, r);
            else if (v.level() == fp::FluidLevel::blocky) changed += v.erase_cells(p, r);
            else if (v.level() == fp::FluidLevel::flowing) {
                v.fill_box(fp::AABB(p - RVec2{r, r}, p + RVec2{r, r}), 0.0_r);
                ++changed;
            }
            break;
        case FluidBrush::sand:
            if (v.level() == fp::FluidLevel::blocky) changed += v.paint_cells(p, r, static_cast<fp::SandCell2D>(std::clamp(material, 0, 12)), 1.0_r, true);
            break;
        case FluidBrush::wall:
            v.paint_solid(p, r, material >= 0);
            ++changed;
            break;
        case FluidBrush::heat:
            if (v.level() == fp::FluidLevel::blocky) {
                v.add_heat(p, r, static_cast<Real>(velocity.x == 0.0f ? 400.0f : velocity.x));
                ++changed;
            }
            break;
        default:
            break;
        }
        v.wake();
    }
    return changed;
}

orchard::FluidLevel fluid_level_for(const orchard::Fluid& f, orchard::FluidLevel project_level) {
    if (f.level != orchard::FluidLevel::project_default) return f.level;
    return project_level == orchard::FluidLevel::project_default ? orchard::FluidLevel::waves : project_level;
}

void preview_fluids(const orchard::Scene& scene, orchard::FluidLevel project_level, std::vector<FluidDraw>& out) {
    out.clear();
    for (orchard::Entity e : scene.entities()) {
        const orchard::Fluid* f = scene.fluid(e);
        if (!f || !f->enabled) continue;
        const orchard::Vec3 c = world_position(scene, e);
        const orchard::Vec3 sc = world_scale(scene, e);
        FluidDraw d;
        d.entity = e;
        d.level = orchard::FluidLevel::still;
        d.color = f->color;
        d.opacity = f->opacity;
        d.reflectivity = f->reflectivity;
        d.reflections = f->reflections;
        d.size = {std::fabs(f->size.x * sc.x), std::fabs(f->size.y * sc.y), std::fabs(f->size.z * sc.z)};
        d.level = fluid_level_for(*f, project_level);
        d.cell = -1.0f;
        d.origin = c - d.size * 0.5f;
        d.level_height = d.size.y * std::clamp(f->fill, 0.0f, 1.0f);
        out.push_back(std::move(d));
    }
}

int World::step(float dt) {
    if (!m->sim2 && !m->sim3) return 0;
    m->accumulator += std::max(0.0_r, static_cast<Real>(dt));
    int steps = 0;
    while (m->accumulator >= m->settings.fixed_dt && steps < m->settings.max_steps) {
        m->remember_poses();
        m->step_once();
        m->accumulator -= m->settings.fixed_dt;
        ++steps;
    }
    if (steps == m->settings.max_steps) m->accumulator = 0.0_r;
    if (steps > 0) m->write_back();
    return steps;
}

void World::step_once() {
    m->step_once();
    m->write_back();
}

void World::write_back() {
    m->write_back();
}

orchard::Dimension World::dimension() const {
    return m->dimension;
}

const Settings& World::settings() const {
    return m->settings;
}

void World::set_gravity(orchard::Vec3 g) {
    m->settings.gravity = g;
    if (m->sim2) m->sim2->set_gravity(RVec2{g.x, g.y});
    if (m->sim3) m->sim3->set_gravity(v3(g));
}

void World::set_physics(const orchard::PhysicsSettings& physics) {
    m->settings.physics = physics;
    m->apply_physics();
}

std::size_t World::body_count() const {
    return m->bodies2.size() + m->bodies3.size();
}

std::size_t World::collider_count() const {
    return m->colliders;
}

void World::set_asset_root(const std::string& path) {
    m->asset_root = path;
}

const std::string& World::asset_root() const {
    return m->asset_root;
}

void World::set_terrain_source(TerrainSource source) {
    m->terrain_source = std::move(source);
}

std::size_t World::terrain_count() const {
    return m->terrains;
}

std::size_t World::scatter_collider_count() const {
    return m->scatter_colliders;
}

const std::vector<std::string>& World::warnings() const {
    return m->warnings;
}

const std::vector<Contact>& World::contacts() const {
    return m->contacts;
}

bool World::awake(orchard::Entity e) const {
    if (auto it = m->bodies2.find(e.id()); it != m->bodies2.end()) return it->second->is_awake();
    if (auto it = m->bodies3.find(e.id()); it != m->bodies3.end()) return it->second->is_awake();
    return false;
}

bool World::has_body(orchard::Entity e) const {
    return m->bodies2.count(e.id()) > 0 || m->bodies3.count(e.id()) > 0;
}

orchard::Vec3 World::velocity(orchard::Entity e) const {
    if (auto it = m->bodies2.find(e.id()); it != m->bodies2.end()) {
        const RVec2 v = it->second->linear_velocity();
        return {static_cast<float>(v.x), static_cast<float>(v.y), 0.0f};
    }
    if (auto it = m->bodies3.find(e.id()); it != m->bodies3.end()) return ov(it->second->linear_velocity());
    return {};
}

void World::set_velocity(orchard::Entity e, orchard::Vec3 v) {
    if (auto it = m->bodies2.find(e.id()); it != m->bodies2.end()) it->second->set_linear_velocity(RVec2{v.x, v.y});
    if (auto it = m->bodies3.find(e.id()); it != m->bodies3.end()) it->second->set_linear_velocity(v3(v));
}

orchard::Vec3 World::spin(orchard::Entity e) const {
    if (auto it = m->bodies2.find(e.id()); it != m->bodies2.end()) return {0.0f, 0.0f, static_cast<float>(it->second->angular_velocity() * rad_to_deg)};
    if (auto it = m->bodies3.find(e.id()); it != m->bodies3.end()) return ov(it->second->angular_velocity() * rad_to_deg);
    return {};
}

void World::set_spin(orchard::Entity e, orchard::Vec3 v) {
    if (auto it = m->bodies2.find(e.id()); it != m->bodies2.end()) it->second->set_angular_velocity(v.z * deg_to_rad);
    if (auto it = m->bodies3.find(e.id()); it != m->bodies3.end()) it->second->set_angular_velocity(v3(v) * deg_to_rad);
}

void World::impulse(orchard::Entity e, orchard::Vec3 v) {
    if (auto it = m->bodies2.find(e.id()); it != m->bodies2.end()) {
        it->second->wake();
        it->second->apply_linear_impulse(RVec2{v.x, v.y});
    }
    if (auto it = m->bodies3.find(e.id()); it != m->bodies3.end()) {
        it->second->wake();
        it->second->apply_linear_impulse(v3(v));
    }
}

void World::force(orchard::Entity e, orchard::Vec3 v) {
    if (auto it = m->bodies2.find(e.id()); it != m->bodies2.end()) it->second->apply_force(RVec2{v.x, v.y});
    if (auto it = m->bodies3.find(e.id()); it != m->bodies3.end()) it->second->apply_force(v3(v));
}

void World::torque(orchard::Entity e, orchard::Vec3 v) {
    if (auto it = m->bodies2.find(e.id()); it != m->bodies2.end()) it->second->apply_torque(v.z);
    if (auto it = m->bodies3.find(e.id()); it != m->bodies3.end()) it->second->apply_torque(v3(v));
}

void World::moved(orchard::Entity e) {
    Impl::Pose before, after;
    const bool had = m->body_pose(e.id(), before);
    m->place(e);
    auto it = m->previous.find(e.id());
    if (it == m->previous.end()) return;
    if (!had || !m->body_pose(e.id(), after)) {
        m->previous.erase(it);
        return;
    }
    it->second.position = it->second.position + (after.position - before.position);
    it->second.rotation = orchard::normalize(after.rotation * conjugate(before.rotation) * it->second.rotation);
}

float World::interpolation() const {
    const Real f = m->settings.fixed_dt;
    return f > 0.0_r ? static_cast<float>(std::clamp(m->accumulator / f, 0.0_r, 1.0_r)) : 1.0f;
}

bool World::render_pose(orchard::Entity e, orchard::Vec3& position, orchard::Quat& rotation) const {
    if (!m->scene || !m->scene->alive(e)) return false;
    auto it = m->previous.find(e.id());
    Impl::Pose now;
    if (it == m->previous.end() || !m->body_pose(e.id(), now)) return false;
    const float a = interpolation();
    const orchard::Vec3 lerped = it->second.position + (now.position - it->second.position) * a;
    if (m->dimension == orchard::Dimension::two_d) {
        position = {lerped.x, lerped.y, world_position(*m->scene, e).z};
        const float turn = angle_2d(now.rotation) - angle_2d(it->second.rotation);
        const float back = std::remainder(turn, 6.2831853f) * (a - 1.0f);
        rotation = orchard::normalize(about_z(back) * world_rotation(*m->scene, e));
        return true;
    }
    position = lerped;
    if (m->walkers.count(e.id())) {
        rotation = world_rotation(*m->scene, e);
        return true;
    }
    orchard::Quat q0 = it->second.rotation;
    const orchard::Quat& q1 = now.rotation;
    if (q0.x * q1.x + q0.y * q1.y + q0.z * q1.z + q0.w * q1.w < 0.0f) q0 = {-q0.x, -q0.y, -q0.z, -q0.w};
    rotation = orchard::normalize(orchard::Quat{q0.x + (q1.x - q0.x) * a, q0.y + (q1.y - q0.y) * a, q0.z + (q1.z - q0.z) * a, q0.w + (q1.w - q0.w) * a});
    return true;
}

void World::added(orchard::Entity e) {
    if (!m->scene || !m->scene->alive(e)) return;
    const orchard::Scene& s = *m->scene;
    for (orchard::Entity at : s.subtree(e)) {
        const orchard::Body* b = s.body(at);
        if (!b) continue;
        if (m->bodies2.count(at.id()) || m->bodies3.count(at.id())) continue;
        const std::vector<Part> parts = parts_of(s, at);
        if (parts.empty()) continue;
        if (m->dimension == orchard::Dimension::two_d) {
            if (m->sim2) m->add_2d(at, *b, parts);
        } else if (m->sim3) {
            m->add_3d(at, *b, parts);
        }
    }
}

orchard::Entity World::raycast(orchard::Vec3 origin, orchard::Vec3 direction, float max_distance, orchard::Vec3* point, orchard::Vec3* normal, float* distance) const {
    auto owner = [this](const void* body) {
        const auto it = m->owners.find(body);
        return it == m->owners.end() ? orchard::Entity{} : it->second;
    };
    if (m->sim3) {
        const auto hit = m->sim3->raycast(v3(origin), v3(direction), max_distance);
        if (!hit) return {};
        if (point) *point = {static_cast<float>(hit->point.x), static_cast<float>(hit->point.y), static_cast<float>(hit->point.z)};
        if (normal) *normal = {static_cast<float>(hit->normal.x), static_cast<float>(hit->normal.y), static_cast<float>(hit->normal.z)};
        if (distance) *distance = static_cast<float>(hit->distance);
        return owner(hit->body);
    }
    if (m->sim2) {
        const auto hit = m->sim2->raycast(RVec2{origin.x, origin.y}, RVec2{direction.x, direction.y}, max_distance);
        if (!hit) return {};
        if (point) *point = {static_cast<float>(hit->point.x), static_cast<float>(hit->point.y), 0.0f};
        if (normal) *normal = {static_cast<float>(hit->normal.x), static_cast<float>(hit->normal.y), 0.0f};
        if (distance) *distance = static_cast<float>(hit->distance);
        return owner(hit->body);
    }
    return {};
}

std::vector<orchard::Entity> World::overlap_sphere(orchard::Vec3 center, float radius) const {
    std::vector<orchard::Entity> out;
    auto add = [&](const void* body) {
        const auto it = m->owners.find(body);
        if (it != m->owners.end()) out.push_back(it->second);
    };
    fp::QueryFilter3D f3;
    f3.include_sensors = true;
    fp::QueryFilter2D f2;
    f2.include_sensors = true;
    if (m->sim3) for (fp::RigidBody3D* b : m->sim3->overlap_sphere(v3(center), radius, f3)) add(b);
    if (m->sim2) for (fp::RigidBody2D* b : m->sim2->overlap_circle(RVec2{center.x, center.y}, radius, f2)) add(b);
    return out;
}

void World::set_layers(orchard::Entity e, std::uint64_t layer, std::uint64_t mask) {
    if (m->sim3) {
        const auto it = m->bodies3.find(e.id());
        if (it != m->bodies3.end()) {
            it->second->set_collision_filter(layer, mask);
            it->second->wake();
        }
    }
    if (m->sim2) {
        const auto it = m->bodies2.find(e.id());
        if (it != m->bodies2.end()) {
            it->second->set_collision_filter(layer, mask);
            it->second->wake();
        }
    }
}

orchard::Entity World::raycast_ignoring(orchard::Entity ignore, orchard::Vec3 origin, orchard::Vec3 direction, float max_distance, orchard::Vec3* point, orchard::Vec3* normal, float* distance) const {
    auto owner = [this](const void* body) {
        const auto it = m->owners.find(body);
        return it == m->owners.end() ? orchard::Entity{} : it->second;
    };
    if (m->sim3) {
        fp::QueryFilter3D filter;
        if (auto it = m->bodies3.find(ignore.id()); it != m->bodies3.end()) filter.ignore = it->second;
        const auto hit = m->sim3->raycast(v3(origin), v3(direction), max_distance, filter);
        if (!hit) return {};
        if (point) *point = ov(hit->point);
        if (normal) *normal = ov(hit->normal);
        if (distance) *distance = static_cast<float>(hit->distance);
        return owner(hit->body);
    }
    if (m->sim2) {
        fp::QueryFilter2D filter;
        if (auto it = m->bodies2.find(ignore.id()); it != m->bodies2.end()) filter.ignore = it->second;
        const auto hit = m->sim2->raycast(RVec2{origin.x, origin.y}, RVec2{direction.x, direction.y}, max_distance, filter);
        if (!hit) return {};
        if (point) *point = {static_cast<float>(hit->point.x), static_cast<float>(hit->point.y), 0.0f};
        if (normal) *normal = {static_cast<float>(hit->normal.x), static_cast<float>(hit->normal.y), 0.0f};
        if (distance) *distance = static_cast<float>(hit->distance);
        return owner(hit->body);
    }
    return {};
}

namespace {

orchard::Vec3 approach(orchard::Vec3 from, orchard::Vec3 to, float max_change) {
    const orchard::Vec3 d = to - from;
    const float len = orchard::length(d);
    if (len <= max_change || len <= 1e-6f) return to;
    return from + d * (max_change / len);
}

orchard::Vec3 flatten(orchard::Vec3 v, orchard::Vec3 n) {
    return v - n * orchard::dot(v, n);
}

} // namespace

CharacterState World::move_character(orchard::Entity e, const CharacterMove& move, float dt) {
    CharacterState st;
    if (!has_body(e) || !m->scene) return st;
    st.body = true;
    dt = std::clamp(dt, 0.0f, 0.1f);
    const orchard::Vec3 g = m->settings.gravity;
    const orchard::Vec3 up = orchard::length(g) > 1e-4f ? orchard::normalize(g * -1.0f) : orchard::normalize(m->settings.up);
    Impl::Walker& w = m->walkers.try_emplace(e.id()).first->second;
    if (w.jump_lock == 0.0_r && !w.grounded) {
        const orchard::Body* b = m->scene->body(e);
        w.gravity_scale = b ? static_cast<Real>(b->gravity_scale) : 1.0_r;
    }

    orchard::Vec3 lo{1e30f, 1e30f, 1e30f}, hi{-1e30f, -1e30f, -1e30f};
    bool any = false;
    auto grow = [&](orchard::Vec3 a, orchard::Vec3 b) {
        lo = {std::min(lo.x, a.x), std::min(lo.y, a.y), std::min(lo.z, a.z)};
        hi = {std::max(hi.x, b.x), std::max(hi.y, b.y), std::max(hi.z, b.z)};
        any = true;
    };
    if (auto it = m->bodies3.find(e.id()); it != m->bodies3.end()) {
        fp::RigidBody3D& body = *it->second;
        if (move.upright) {
            body.set_flag(fp::PhysicsFlags::LockRotationX);
            body.set_flag(fp::PhysicsFlags::LockRotationY);
            body.set_flag(fp::PhysicsFlags::LockRotationZ);
            body.set_angular_velocity(RVec3{0.0_r, 0.0_r, 0.0_r});
        }
        for (const fp::Collider3D& c : body.colliders())
            if (!c.is_sensor) grow(ov(c.world_aabb.min), ov(c.world_aabb.max));
    } else if (auto it2 = m->bodies2.find(e.id()); it2 != m->bodies2.end()) {
        fp::RigidBody2D& body = *it2->second;
        if (move.upright) {
            body.set_flag(fp::PhysicsFlags::LockRotationZ);
            body.set_angular_velocity(0.0_r);
        }
        for (const fp::Collider2D& c : body.colliders())
            if (!c.is_sensor) grow({static_cast<float>(c.world_aabb.min.x), static_cast<float>(c.world_aabb.min.y), 0.0f}, {static_cast<float>(c.world_aabb.max.x), static_cast<float>(c.world_aabb.max.y), 0.0f});
    }
    if (!any) return st;

    const orchard::Vec3 center = (lo + hi) * 0.5f;
    float low = 1e30f;
    for (int i = 0; i < 8; ++i) {
        const orchard::Vec3 corner{i & 1 ? hi.x : lo.x, i & 2 ? hi.y : lo.y, i & 4 ? hi.z : lo.z};
        low = std::min(low, orchard::dot(corner, up));
    }
    const float feet = orchard::dot(center, up) - low;
    const orchard::Vec3 ext = hi - lo;
    orchard::Vec3 side_a = orchard::normalize(orchard::cross(up, std::fabs(up.x) < 0.9f ? orchard::Vec3{1.0f, 0.0f, 0.0f} : orchard::Vec3{0.0f, 0.0f, 1.0f}));
    orchard::Vec3 side_b = orchard::normalize(orchard::cross(up, side_a));
    const float width_a = std::fabs(side_a.x) * ext.x + std::fabs(side_a.y) * ext.y + std::fabs(side_a.z) * ext.z;
    const float width_b = std::fabs(side_b.x) * ext.x + std::fabs(side_b.y) * ext.y + std::fabs(side_b.z) * ext.z;
    const float radius = 0.3f * std::max(0.0f, m->dimension == orchard::Dimension::two_d ? width_a : std::min(width_a, width_b));
    std::vector<orchard::Vec3> starts{center};
    if (radius > 0.001f) {
        starts.push_back(center + side_a * radius);
        starts.push_back(center - side_a * radius);
        if (m->dimension == orchard::Dimension::three_d) {
            starts.push_back(center + side_b * radius);
            starts.push_back(center - side_b * radius);
        }
    }
    const std::size_t inner_rays = starts.size();
    const float foot_a = 0.45f * width_a, foot_b = 0.45f * width_b;
    if (move.ride_height > 0.0f && foot_a > radius + 0.01f) {
        starts.push_back(center + side_a * foot_a);
        starts.push_back(center - side_a * foot_a);
        if (m->dimension == orchard::Dimension::three_d) {
            starts.push_back(center + side_b * foot_b);
            starts.push_back(center - side_b * foot_b);
            for (int sa = -1; sa <= 1; sa += 2)
                for (int sb = -1; sb <= 1; sb += 2) starts.push_back(center + side_a * (foot_a * 0.7f * static_cast<float>(sa)) + side_b * (foot_b * 0.7f * static_cast<float>(sb)));
        }
    }
    const float min_up = std::cos(std::clamp(move.max_slope, 0.0f, 89.0f) * static_cast<float>(deg_to_rad));
    const float reach = feet + std::max(0.01f, move.ground_probe);
    float best = 1e30f;
    float best_inner = 1e30f;
    orchard::Vec3 best_normal = up;
    orchard::Entity best_hit;
    bool center_hit = false;
    orchard::Vec3 center_normal = up;
    for (std::size_t r = 0; r < starts.size(); ++r) {
        orchard::Vec3 n;
        float d = -1.0f;
        const orchard::Entity hit = raycast_ignoring(e, starts[r], up * -1.0f, reach, nullptr, &n, &d);
        if (d < 0.0f) continue;
        const orchard::Vec3 nn = orchard::normalize(n);
        if (orchard::dot(nn, up) < min_up) continue;
        if (r == 0) {
            center_hit = true;
            center_normal = nn;
        }
        if (r >= inner_rays) {
            best = std::min(best, d);
            continue;
        }
        best_inner = std::min(best_inner, d);
        if (d <= best) {
            best = d;
            best_normal = nn;
            best_hit = hit;
        }
    }
    const bool touching_ground = best_inner < 1e29f;
    if (center_hit) best_normal = center_normal;
    if (orchard::dot(best_normal, up) < min_up) best_normal = up;

    orchard::Vec3 v = velocity(e);
    float vu = orchard::dot(v, up);
    w.jump_lock = std::max(0.0_r, w.jump_lock - static_cast<Real>(dt));
    const float launch = std::max(8.0f, move.jump_speed * 1.6f);
    bool grounded = touching_ground && w.jump_lock <= 0.0_r && vu <= launch;
    if (!grounded && w.grounded && w.jump_lock <= 0.0_r && vu > 0.0f && vu <= launch) {
        v = v - up * vu;
        vu = 0.0f;
    }
    orchard::Vec3 wish = flatten(move.wish, up);
    if (m->dimension == orchard::Dimension::two_d) wish.z = 0.0f;

    if (grounded && move.jump) {
        const orchard::Vec3 h = flatten(v, up);
        v = approach(h, wish, move.ground_accel * dt) + up * std::max(0.0f, move.jump_speed);
        w.jump_lock = 0.25_r;
        grounded = false;
        st.jumped = true;
    } else if (grounded) {
        const orchard::Vec3 n = best_normal;
        const float speed = orchard::length(wish);
        orchard::Vec3 along = flatten(wish, n);
        if (orchard::length(along) > 1e-5f) along = orchard::normalize(along) * speed;
        const orchard::Vec3 tangent = approach(flatten(v, n), along, move.ground_accel * dt);
        const float hover = std::max(0.0f, move.ride_height);
        const float gap = best - feet - hover;
        const float snap = hover > 0.0f ? std::clamp(gap / std::max(dt, 1e-3f) * 0.5f, -3.0f, 4.0f) : std::clamp(gap / std::max(dt, 1e-3f), 0.0f, 4.0f) + 0.05f;
        v = tangent - n * snap;
    } else {
        const orchard::Vec3 h = flatten(v, up);
        v = approach(h, wish, move.air_accel * dt) + up * vu;
    }
    if (m->dimension == orchard::Dimension::two_d) v.z = 0.0f;
    w.grounded = grounded;

    const Real scale = grounded ? 0.0_r : w.gravity_scale;
    if (auto it = m->bodies3.find(e.id()); it != m->bodies3.end()) {
        it->second->set_gravity_scale(scale);
        it->second->wake();
        it->second->set_linear_velocity(v3(v));
    } else if (auto it2 = m->bodies2.find(e.id()); it2 != m->bodies2.end()) {
        it2->second->set_gravity_scale(scale);
        it2->second->wake();
        it2->second->set_linear_velocity(RVec2{v.x, v.y});
    }
    st.grounded = grounded;
    st.ground = best_hit;
    st.ground_normal = touching_ground ? best_normal : orchard::Vec3{};
    st.velocity = v;
    return st;
}

void World::release_character(orchard::Entity e) {
    auto it = m->walkers.find(e.id());
    if (it == m->walkers.end()) return;
    const Real scale = it->second.gravity_scale;
    if (auto b3 = m->bodies3.find(e.id()); b3 != m->bodies3.end()) b3->second->set_gravity_scale(scale);
    if (auto b2 = m->bodies2.find(e.id()); b2 != m->bodies2.end()) b2->second->set_gravity_scale(scale);
    m->walkers.erase(it);
}

bool World::touching(orchard::Entity a, orchard::Entity b) const {
    if (!b.valid()) {
        for (const Contact& c : m->contacts) {
            if (c.a == a || c.b == a) return true;
        }
        return false;
    }
    return m->touching.count(Impl::key(a, b)) > 0;
}

} // namespace xylem
