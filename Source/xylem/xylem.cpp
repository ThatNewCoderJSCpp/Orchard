#include <xylem/xylem.hpp>

#include <fizmo/../physics.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <unordered_map>
#include <utility>

namespace xylem {

namespace fp = fizmo::physics;
using fizmo::linalg::QuatD;
using fizmo::linalg::vector2d;
using fizmo::linalg::vector3d;

namespace {

constexpr double rad_to_deg = 57.29577951308232;
constexpr double deg_to_rad = 0.017453292519943295;

QuatD to_fizmo(orchard::Quat q) {
    return QuatD(static_cast<double>(q.w), static_cast<double>(q.x), static_cast<double>(q.y), static_cast<double>(q.z));
}

orchard::Quat to_orchard(const QuatD& q) {
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

vector3d v3(orchard::Vec3 v) {
    return vector3d{static_cast<double>(v.x), static_cast<double>(v.y), static_cast<double>(v.z)};
}

orchard::Vec3 ov(const vector3d& v) {
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
    return f;
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

} // namespace

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
    double accumulator = 0.0;

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
            it->second->set_position(vector2d{pos.x, pos.y});
            it->second->set_angle(angle_z(rot));
            it->second->wake();
        } else {
            auto it = bodies3.find(e.id());
            if (it == bodies3.end()) return;
            it->second->set_position(v3(pos));
            it->second->set_orientation(to_fizmo(rot));
            it->second->wake();
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
        accumulator = 0.0;
        sim2.reset();
        sim3.reset();
        if (dimension == orchard::Dimension::two_d) {
            fp::Simulation2D::Config cfg;
            cfg.gravity = vector2d{settings.gravity.x, settings.gravity.y};
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
        for (orchard::Entity e : s.entities()) {
            const orchard::Body* b = s.body(e);
            if (!b) continue;
            const std::vector<Part> parts = parts_of(s, e);
            if (parts.empty()) continue;
            if (dimension == orchard::Dimension::two_d) add_2d(e, *b, parts);
            else add_3d(e, *b, parts);
        }
    }

    void add_2d(orchard::Entity e, const orchard::Body& b, const std::vector<Part>& parts) {
        fp::RigidBody2D& body = sim2->add_body(flags_for(b));
        for (const Part& p : parts) {
            const orchard::Collider& c = p.collider;
            fp::Shape2D shape;
            const double sx = std::max(0.001, static_cast<double>(c.size.x * p.scale.x));
            const double sy = std::max(0.001, static_cast<double>(c.size.y * p.scale.y));
            if (c.shape == orchard::ColliderShape::sphere) {
                shape = fp::Shape2D::make_circle(vector2d{0.0, 0.0}, std::max(sx, sy) * 0.5);
            } else if (c.shape == orchard::ColliderShape::capsule) {
                const double r = sx * 0.5;
                const double half = std::max(0.0, sy * 0.5 - r);
                shape = fp::Shape2D::make_capsule(vector2d{0.0, -half}, vector2d{0.0, half}, r);
            } else {
                shape = fp::Shape2D::make_box(sx * 0.5, sy * 0.5);
            }
            fp::Material mat(fp::MassSpec::from_density(std::max(0.0001, static_cast<double>(c.density))), c.friction, c.restitution);
            mat.friction_combine = to_fizmo(c.friction_combine);
            mat.restitution_combine = to_fizmo(c.bounce_combine);
            body.add_collider(shape, mat, fp::Transform2D(vector2d{p.offset.x, p.offset.y}, angle_z(p.rotation)), fp::CollisionFilter{}, c.sensor);
            ++colliders;
        }
        body.set_gravity_scale(b.gravity_scale);
        body.set_linear_damping(b.linear_damping);
        body.set_angular_damping(b.angular_damping);
        body.mark_mass_dirty();
        bodies2[e.id()] = &body;
        owners[&body] = e;
        place(e);
    }

    void add_3d(orchard::Entity e, const orchard::Body& b, const std::vector<Part>& parts) {
        fp::RigidBody3D& body = sim3->add_body(flags_for(b));
        for (const Part& p : parts) {
            const orchard::Collider& c = p.collider;
            fp::Shape3D shape;
            const double sx = std::max(0.001, static_cast<double>(c.size.x * p.scale.x));
            const double sy = std::max(0.001, static_cast<double>(c.size.y * p.scale.y));
            const double sz = std::max(0.001, static_cast<double>(c.size.z * p.scale.z));
            if (c.shape == orchard::ColliderShape::sphere) {
                shape = fp::Shape3D::make_sphere(vector3d{0.0, 0.0, 0.0}, std::max({sx, sy, sz}) * 0.5);
            } else if (c.shape == orchard::ColliderShape::capsule) {
                const double r = std::max(sx, sz) * 0.5;
                const double half = std::max(0.0, sy * 0.5 - r);
                shape = fp::Shape3D::make_capsule(vector3d{0.0, -half, 0.0}, vector3d{0.0, half, 0.0}, r);
            } else {
                shape = fp::Shape3D::make_box(sx * 0.5, sy * 0.5, sz * 0.5);
            }
            fp::Material3D mat(fp::MassSpec3D::from_density(std::max(0.0001, static_cast<double>(c.density)) * 1000.0), c.friction, c.restitution);
            mat.friction_combine = to_fizmo(c.friction_combine);
            mat.restitution_combine = to_fizmo(c.bounce_combine);
            body.add_collider(shape, mat, fp::Transform3D(v3(p.offset), to_fizmo(p.rotation)), fp::CollisionFilter{}, c.sensor);
            ++colliders;
        }
        body.set_gravity_scale(b.gravity_scale);
        body.set_linear_damping(b.linear_damping);
        body.set_angular_damping(b.angular_damping);
        body.mark_mass_dirty();
        bodies3[e.id()] = &body;
        owners[&body] = e;
        place(e);
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
                if (!scene->alive(e) || (body->inv_mass() <= 0.0 && !body->has(fp::PhysicsFlags::LinearVelocity))) continue;
                const vector2d p = body->position();
                const orchard::Vec3 old = world_position(*scene, e);
                const orchard::Quat old_rot = world_rotation(*scene, e);
                const float keep_tilt = angle_z(old_rot);
                orchard::Quat rot = about_z(static_cast<float>(body->angle())) * conjugate(about_z(keep_tilt)) * old_rot;
                store(e, orchard::Vec3{static_cast<float>(p.x), static_cast<float>(p.y), old.z}, orchard::normalize(rot));
            }
        }
        if (sim3) {
            for (auto& [id, body] : bodies3) {
                const orchard::Entity e = orchard::Entity::from_id(id);
                if (!scene->alive(e) || (body->inv_mass() <= 0.0 && !body->has(fp::PhysicsFlags::LinearVelocity))) continue;
                store(e, ov(body->position()), to_orchard(body->orientation()));
            }
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
        }
    }

    void step_once() {
        prune();
        if (sim2) sim2->step(settings.fixed_dt);
        if (sim3) sim3->step(settings.fixed_dt);
        refresh_contacts();
    }
};

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
    m->sim3.reset();
    m->scene = nullptr;
    m->colliders = 0;
}

int World::step(double dt) {
    if (!m->sim2 && !m->sim3) return 0;
    m->accumulator += std::max(0.0, dt);
    int steps = 0;
    while (m->accumulator >= m->settings.fixed_dt && steps < m->settings.max_steps) {
        m->step_once();
        m->accumulator -= m->settings.fixed_dt;
        ++steps;
    }
    if (steps == m->settings.max_steps) m->accumulator = 0.0;
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
    if (m->sim2) m->sim2->set_gravity(vector2d{g.x, g.y});
    if (m->sim3) m->sim3->set_gravity(v3(g));
}

std::size_t World::body_count() const {
    return m->bodies2.size() + m->bodies3.size();
}

std::size_t World::collider_count() const {
    return m->colliders;
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
        const vector2d v = it->second->linear_velocity();
        return {static_cast<float>(v.x), static_cast<float>(v.y), 0.0f};
    }
    if (auto it = m->bodies3.find(e.id()); it != m->bodies3.end()) return ov(it->second->linear_velocity());
    return {};
}

void World::set_velocity(orchard::Entity e, orchard::Vec3 v) {
    if (auto it = m->bodies2.find(e.id()); it != m->bodies2.end()) it->second->set_linear_velocity(vector2d{v.x, v.y});
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
        it->second->apply_linear_impulse(vector2d{v.x, v.y});
    }
    if (auto it = m->bodies3.find(e.id()); it != m->bodies3.end()) {
        it->second->wake();
        it->second->apply_linear_impulse(v3(v));
    }
}

void World::force(orchard::Entity e, orchard::Vec3 v) {
    if (auto it = m->bodies2.find(e.id()); it != m->bodies2.end()) it->second->apply_force(vector2d{v.x, v.y});
    if (auto it = m->bodies3.find(e.id()); it != m->bodies3.end()) it->second->apply_force(v3(v));
}

void World::torque(orchard::Entity e, orchard::Vec3 v) {
    if (auto it = m->bodies2.find(e.id()); it != m->bodies2.end()) it->second->apply_torque(v.z);
    if (auto it = m->bodies3.find(e.id()); it != m->bodies3.end()) it->second->apply_torque(v3(v));
}

void World::moved(orchard::Entity e) {
    m->place(e);
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
