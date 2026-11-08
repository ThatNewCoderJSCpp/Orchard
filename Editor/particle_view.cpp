#include "particle_view.hpp"

#ifndef FIZMO
#define FIZMO
#endif
#ifndef FIZMO_GRAPHICS
#define FIZMO_GRAPHICS
#endif
#include <fizmo/includes.hpp>
#include <fizmo/../particles.hpp>

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <utility>
#include <vector>

namespace orchard {
namespace editor {

namespace fp = fizmo::particles;
namespace fg = fizmo::graphics;
using fizmo::linalg::vector3d;

namespace {

fp::EmissionShape to_fizmo(EmitterShape s) {
    switch (s) {
    case EmitterShape::point: return fp::EmissionShape::Point;
    case EmitterShape::sphere: return fp::EmissionShape::Sphere;
    case EmitterShape::hemisphere: return fp::EmissionShape::Hemisphere;
    case EmitterShape::cone: return fp::EmissionShape::Cone;
    case EmitterShape::box: return fp::EmissionShape::Box;
    case EmitterShape::circle: return fp::EmissionShape::Circle;
    case EmitterShape::line: return fp::EmissionShape::Line;
    }
    return fp::EmissionShape::Point;
}

float safe_ratio(float a, float b) {
    return b > 1e-6f ? a / b : 0.0f;
}

Vec3 column(const Mat4& m, int c) {
    return Vec3{m.at(0, c), m.at(1, c), m.at(2, c)};
}

Vec3 unit_or(Vec3 v, Vec3 fallback) {
    const float l = length(v);
    return l > 1e-6f ? v * (1.0f / l) : fallback;
}

vector3d dv(Vec3 v) {
    return vector3d{v.x, v.y, v.z};
}

} // namespace

fp::EmitterSettings particle_settings(const Emitter& e, bool flat, Vec3 up, int depth) {
    fp::EmitterSettings s;
    s.name = e.preset;
    s.dimension = flat ? fp::ParticleDim::D2 : fp::ParticleDim::D3;
    s.space = e.local_space ? fp::SimulationSpace::Local : fp::SimulationSpace::World;
    s.max_particles = static_cast<std::uint32_t>(std::clamp(e.max_particles, 1, 1000000));
    s.priority = std::clamp(e.priority, 0, 3);
    s.duration = std::max(0.0f, e.duration);
    s.loop = e.loop;
    if (!s.loop && s.duration <= 0.0f) s.duration = std::max(0.05f, e.burst > 0 ? 0.1f : e.lifetime);
    s.auto_play = e.autoplay;
    s.rate = std::max(0.0f, e.rate);
    if (e.burst > 0) {
        fp::Burst b;
        b.time = 0.0f;
        b.count = e.burst;
        b.cycles = e.burst_every > 0.0f ? 1000000 : 1;
        b.interval = e.burst_every > 0.0f ? e.burst_every : 1.0f;
        s.bursts.push_back(b);
    }
    const float life = std::max(0.01f, e.lifetime);
    s.lifetime = fp::RangeF(std::max(0.01f, life - e.lifetime_spread), life + e.lifetime_spread);
    s.shape = to_fizmo(e.shape);
    const float radius = std::max({e.shape_size.x, e.shape_size.y, e.shape_size.z, 0.0f});
    s.shape_radius = e.shape == EmitterShape::box || e.shape == EmitterShape::line ? radius * 0.5f : radius;
    s.shape_size = fp::Float3(e.shape_size.x, e.shape_size.y, e.shape_size.z);
    if (e.shape == EmitterShape::line) s.shape_size = fp::Float3(e.shape_size.x, 0.0f, 0.0f);
    const Vec3 dir = unit_or(e.direction, Vec3{0.0f, 1.0f, 0.0f});
    s.direction = fp::Float3(dir.x, dir.y, dir.z);
    s.direction_mode = e.spread >= 179.5f ? fp::DirectionMode::Random : fp::DirectionMode::Axis;
    s.spread = std::clamp(e.spread, 0.0f, 180.0f);
    s.speed = fp::RangeF(std::max(0.0f, e.speed - e.speed_spread), e.speed + e.speed_spread);
    s.inherit_velocity = e.inherit_velocity;
    const float size = std::max(0.0005f, e.size);
    s.size = fp::RangeF(size * 0.85f, size * 1.15f);
    s.size_over_life = fp::LifeCurve::linear(1.0f, safe_ratio(e.size_end, size));
    const bool additive = e.blend == ParticleBlend::additive;
    float boost = 1.0f;
    if (additive) boost = std::clamp(0.45f + e.glow * 0.25f, 0.2f, 1.0f);
    s.color = fp::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
    s.color_alt = s.color;
    s.color_over_life = fp::LifeGradient::linear(fp::ColorF(e.color.x, e.color.y, e.color.z, 1.0f), fp::ColorF(e.color_end.x, e.color_end.y, e.color_end.z, 1.0f));
    s.alpha_over_life = fp::LifeCurve::linear(std::clamp(e.alpha * boost, 0.0f, 1.0f), std::clamp(e.alpha_end * boost, 0.0f, 1.0f));
    const Vec3 g = unit_or(up, Vec3{0.0f, 1.0f, 0.0f}) * -9.81f;
    s.gravity = fp::Float3(g.x, g.y, g.z);
    s.gravity_scale = e.gravity;
    s.drag = std::max(0.0f, e.drag);
    s.wind = fp::Float3(e.wind.x, e.wind.y, e.wind.z);
    s.turbulence = std::max(0.0f, e.turbulence);
    s.render_mode = e.stretch > 0.01f ? fp::RenderMode::Stretched : fp::RenderMode::Billboard;
    s.stretch = std::max(0.0f, e.stretch);
    s.blend = additive ? fp::ParticleBlend::Additive : fp::ParticleBlend::Alpha;
    s.lit = e.lit;
    s.near_fade = flat ? 0.0f : 0.3f;
    s.texture = e.texture;
    s.frames_x = std::max(1, e.frames);
    s.frames_y = 1;
    s.frame_count = std::max(1, e.frames);
    s.collide = e.collide;
    s.bounce = std::clamp(e.bounce, 0.0f, 1.0f);
    s.friction = std::max(0.0f, e.friction);
    s.kill_on_collision = e.die_on_hit;
    s.collision_radius = 0.5f;
    if (depth == 0 && !e.on_death.empty() && e.on_death != e.preset) {
        if (const std::optional<Emitter> child = Emitter::named(e.on_death)) {
            fp::SubEmitter sub;
            sub.trigger = fp::SubEmitterTrigger::Death;
            sub.count = 1;
            sub.probability = std::clamp(8.0f / std::max(8.0f, e.rate + static_cast<float>(e.burst)), 0.05f, 0.6f);
            sub.inherit_velocity = 0.2f;
            Emitter c = *child;
            c.max_particles = std::min(c.max_particles, std::max(16, e.max_particles / 2));
            c.priority = std::max(0, e.priority - 1);
            sub.settings = particle_settings(c, flat, up, depth + 1);
            sub.settings.loop = false;
            sub.settings.rate = 0.0f;
            sub.settings.bursts.clear();
            s.sub_emitters.push_back(std::move(sub));
        }
    }
    return s;
}

struct ParticleView::Impl {
    fp::ParticleSystem system;
    fp::DrawList3D list3;
    fp::DrawList2D list2;
    struct Slot {
        fp::Emitter* emitter = nullptr;
        fp::EmitterSettings settings;
        std::string texture;
        bool seen = false;
    };
    std::unordered_map<std::uint64_t, Slot> slots;
    std::vector<std::pair<Entity, std::pair<int, int>>> pending;
    std::function<const fg::Texture*(const std::string&)> lookup;
    bool flat = false;

    Impl() : system(defaults()) {}

    static fp::SystemSettings defaults() {
        fp::SystemSettings s;
        s.budget = 60000;
        s.adaptive = true;
        s.target_ms = 2.5f;
        s.threads = 1;
        return s;
    }

    void apply(Slot& slot, int action, int count) {
        if (!slot.emitter) return;
        if (action == 0) slot.emitter->stop();
        else if (action == 1) slot.emitter->play();
        else if (action == 2) {
            int n = count;
            if (n <= 0) {
                for (const fp::Burst& b : slot.settings.bursts) n = std::max(n, b.count);
                if (n <= 0) n = std::max(8, static_cast<int>(slot.settings.rate * 0.5f));
            }
            slot.emitter->burst(std::min(n, 100000));
        } else if (action == 3) {
            slot.emitter->clear();
        }
    }

    void clear_all() {
        for (auto& [id, slot] : slots)
            if (slot.emitter) system.destroy(slot.emitter);
        slots.clear();
        system.clear();
        pending.clear();
    }
};

ParticleView::ParticleView() : m(std::make_unique<Impl>()) {}

ParticleView::~ParticleView() = default;

void ParticleView::set_budget(int budget, int quality) {
    fp::SystemSettings s = m->system.settings();
    const std::size_t want = static_cast<std::size_t>(std::clamp(budget, 100, 2000000));
    const float scales[4] = {0.35f, 1.0f, 1.6f, 2.5f};
    const float scale = scales[std::clamp(quality, 0, 3)];
    const float target = 1.5f + 1.0f * scale;
    if (s.budget == want && std::fabs(s.target_ms - target) < 1e-3f) return;
    s.budget = want;
    s.target_ms = target;
    s.lod[0].distance = 25.0f * scale;
    s.lod[1].distance = 50.0f * scale;
    s.lod[2].distance = 100.0f * scale;
    s.lod[3].distance = 200.0f * scale;
    s.cull_distance = 300.0f * scale;
    m->system.set_settings(s);
}

void ParticleView::set_texture_lookup(std::function<const fg::Texture*(const std::string&)> lookup) {
    m->lookup = std::move(lookup);
}

void ParticleView::sync(const Scene& scene, Vec3 up, bool running) {
    static_cast<void>(running);
    const bool flat = scene.dimension() == Dimension::two_d;
    if (flat != m->flat) {
        m->clear_all();
        m->flat = flat;
    }
    for (auto& [id, slot] : m->slots) slot.seen = false;
    bool any_collide = false;
    for (Entity e : scene.entities()) {
        const Emitter* pe = scene.emitter(e);
        if (!pe || !pe->enabled) continue;
        Impl::Slot& slot = m->slots[e.id()];
        slot.seen = true;
        fp::EmitterSettings want = particle_settings(*pe, flat, up);
        any_collide = any_collide || want.collide;
        const Mat4 w = scene.world_matrix(e);
        const Vec3 pos = mul_point(w, Vec3{0.0f, 0.0f, 0.0f});
        if (!slot.emitter) {
            slot.settings = want;
            slot.emitter = &m->system.create_emitter(want, dv(pos));
            if (!pe->autoplay) slot.emitter->stop();
        } else if (!(slot.settings == want)) {
            const bool was = slot.emitter->playing();
            slot.settings = want;
            slot.emitter->set_settings(want);
            if (!was && !pe->autoplay) slot.emitter->stop();
        }
        if (slot.texture != pe->texture) {
            slot.texture = pe->texture;
            slot.emitter->set_texture(!pe->texture.empty() && m->lookup ? m->lookup(pe->texture) : nullptr);
        }
        if (flat) {
            slot.emitter->set_position(pos.x, pos.y, 0.0);
            const Vec3 xa = column(w, 0);
            slot.emitter->set_angle(std::atan2(xa.y, xa.x) * 180.0 / 3.14159265358979323846);
        } else {
            slot.emitter->set_position(dv(pos));
            const Vec3 right = unit_or(column(w, 0), Vec3{1.0f, 0.0f, 0.0f});
            const Vec3 upv = unit_or(column(w, 1), Vec3{0.0f, 1.0f, 0.0f});
            const Vec3 fwd = unit_or(column(w, 2), Vec3{0.0f, 0.0f, 1.0f});
            slot.emitter->set_basis(dv(right), dv(upv), dv(fwd));
        }
    }
    for (auto it = m->slots.begin(); it != m->slots.end();) {
        if (!it->second.seen) {
            if (it->second.emitter) m->system.destroy(it->second.emitter);
            it = m->slots.erase(it);
        } else {
            ++it;
        }
    }
    m->system.clear_colliders();
    if (any_collide) {
        std::size_t added = 0;
        for (Entity e : scene.entities()) {
            const std::vector<Collider>* cs = scene.colliders(e);
            if (!cs || cs->empty()) continue;
            const Mat4 w = scene.world_matrix(e);
            for (const Collider& c : *cs) {
                if (!c.enabled || c.sensor || added >= 512) continue;
                const Vec3 center = mul_point(w, c.offset);
                if (c.shape == ColliderShape::sphere) {
                    const float s = std::max({length(column(w, 0)), length(column(w, 1)), length(column(w, 2))});
                    m->system.add_sphere(dv(center), std::max(c.size.x, std::max(c.size.y, c.size.z)) * 0.5f * s);
                } else {
                    Vec3 lo{1e30f, 1e30f, 1e30f}, hi{-1e30f, -1e30f, -1e30f};
                    for (int k = 0; k < 8; ++k) {
                        const Vec3 corner{(k & 1 ? 0.5f : -0.5f) * c.size.x, (k & 2 ? 0.5f : -0.5f) * c.size.y, (k & 4 ? 0.5f : -0.5f) * (flat ? 1.0f : c.size.z)};
                        const Vec3 p = mul_point(w, c.offset + corner);
                        lo = Vec3{std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
                        hi = Vec3{std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
                    }
                    if (flat) {
                        lo.z = -1e6f;
                        hi.z = 1e6f;
                    }
                    m->system.add_box(dv(lo), dv(hi));
                }
                ++added;
            }
        }
    }
    if (!m->pending.empty()) {
        std::vector<std::pair<Entity, std::pair<int, int>>> keep;
        for (const auto& p : m->pending) {
            const auto found = m->slots.find(p.first.id());
            if (found == m->slots.end()) {
                if (scene.alive(p.first) && scene.emitter(p.first) && keep.size() < 4096) keep.push_back(p);
                continue;
            }
            m->apply(found->second, p.second.first, p.second.second);
        }
        m->pending.swap(keep);
    }
}

void ParticleView::command(Entity e, int action, int count) {
    const auto found = m->slots.find(e.id());
    if (found != m->slots.end()) {
        m->apply(found->second, action, count);
        return;
    }
    if (m->pending.size() < 4096) m->pending.push_back({e, {action, count}});
}

void ParticleView::reset() {
    m->clear_all();
}

void ParticleView::step_3d(double dt, const fg::Camera3D& camera) {
    m->system.step(std::clamp(dt, 0.0, 0.25), fp::CameraView::from_camera(camera));
}

void ParticleView::step_2d(double dt, float left, float bottom, float right, float top) {
    m->system.step(std::clamp(dt, 0.0, 0.25), fp::CameraView::from_rect(left, bottom, right, top));
}

std::size_t ParticleView::draw_3d(fg::Renderer& renderer) {
    if (m->system.particle_count() == 0) return 0;
    fp::build_draw_list(m->system, m->system.last_view(), m->list3);
    return fp::draw(renderer, m->list3);
}

std::size_t ParticleView::draw_2d(fg::Renderer& renderer, const std::function<void(float, float, float&, float&)>& to_screen) {
    if (m->system.particle_count() == 0) return 0;
    fp::DrawOptions options;
    options.min_pixel_size = 0.0f;
    fp::build_draw_list_2d(m->system, m->system.last_view(), m->list2, options);
    for (fg::Vertex2D& v : m->list2.vertices) {
        float sx = 0.0f, sy = 0.0f;
        to_screen(v.x, v.y, sx, sy);
        v.x = sx;
        v.y = sy;
    }
    return fp::draw(renderer, m->list2);
}

ParticleViewStats ParticleView::stats() const {
    const fp::ParticleStats& s = m->system.stats();
    ParticleViewStats out;
    out.particles = m->system.particle_count();
    out.emitters = m->system.emitter_count();
    out.budget = s.budget;
    out.step_ms = s.step_ms;
    out.budget_scale = s.budget_scale;
    return out;
}

std::size_t ParticleView::emitter_count() const {
    return m->slots.size();
}

} // namespace editor
} // namespace orchard
