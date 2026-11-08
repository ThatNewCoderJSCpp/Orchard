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

} // namespace

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
    }

    void add_2d(orchard::Entity e, const orchard::Body& b, const std::vector<Part>& parts) {
        fp::RigidBody2D& body = sim2->add_body(flags_with_overrides(b));
        for (const Part& p : parts) {
            const orchard::Collider& c = p.collider;
            fp::Shape2D shape;
            const Real sx = std::max(0.001_r, static_cast<Real>(c.size.x * p.scale.x));
            const Real sy = std::max(0.001_r, static_cast<Real>(c.size.y * p.scale.y));
            if (c.shape == orchard::ColliderShape::sphere) {
                shape = fp::Shape2D::make_circle(RVec2{0.0_r, 0.0_r}, std::max(sx, sy) * 0.5_r);
            } else if (c.shape == orchard::ColliderShape::capsule) {
                const Real r = sx * 0.5_r;
                const Real half = std::max(0.0_r, sy * 0.5_r - r);
                shape = fp::Shape2D::make_capsule(RVec2{0.0_r, -half}, RVec2{0.0_r, half}, r);
            } else {
                shape = fp::Shape2D::make_box(sx * 0.5_r, sy * 0.5_r);
            }
            fp::Material mat(fp::MassSpec::from_density(std::max(0.0001_r, static_cast<Real>(c.density))), c.friction, c.restitution);
            mat.friction_combine = to_fizmo(c.friction_combine);
            mat.restitution_combine = to_fizmo(c.bounce_combine);
            body.add_collider(shape, mat, fp::Transform2D(RVec2{p.offset.x, p.offset.y}, angle_z(p.rotation)), fp::CollisionFilter{}, c.sensor);
            ++colliders;
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
    }

    void add_3d(orchard::Entity e, const orchard::Body& b, const std::vector<Part>& parts) {
        fp::RigidBody3D& body = sim3->add_body(flags_with_overrides(b));
        for (const Part& p : parts) {
            const orchard::Collider& c = p.collider;
            fp::Shape3D shape;
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
            } else {
                shape = fp::Shape3D::make_box(sx * 0.5_r, sy * 0.5_r, sz * 0.5_r);
            }
            fp::Material3D mat(fp::MassSpec3D::from_density(std::max(0.0001_r, static_cast<Real>(c.density)) * 1000.0_r), c.friction, c.restitution);
            mat.friction_combine = to_fizmo(c.friction_combine);
            mat.restitution_combine = to_fizmo(c.bounce_combine);
            mat.rolling_friction = std::max(0.0_r, static_cast<Real>(c.rolling_friction));
            mat.spinning_friction = std::max(0.0_r, static_cast<Real>(c.spinning_friction));
            mat.anisotropic_x = std::max(0.0_r, static_cast<Real>(c.grip.x));
            mat.anisotropic_y = std::max(0.0_r, static_cast<Real>(c.grip.y));
            mat.anisotropic_z = std::max(0.0_r, static_cast<Real>(c.grip.z));
            body.add_collider(shape, mat, fp::Transform3D(v3(p.offset), to_fizmo(p.rotation)), fp::CollisionFilter{}, c.sensor);
            ++colliders;
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
                const float keep_tilt = angle_z(old_rot);
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

int World::step(float dt) {
    if (!m->sim2 && !m->sim3) return 0;
    m->accumulator += std::max(0.0_r, static_cast<Real>(dt));
    int steps = 0;
    while (m->accumulator >= m->settings.fixed_dt && steps < m->settings.max_steps) {
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
    m->place(e);
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
