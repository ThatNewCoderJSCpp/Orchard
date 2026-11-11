#include "particle_view.hpp"
#include "particle_textures.hpp"

#ifndef FIZMO
#define FIZMO
#endif
#ifndef FIZMO_GRAPHICS
#define FIZMO_GRAPHICS
#endif
#include <fizmo/includes.hpp>
#include <fizmo/../particles.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <future>
#include <mutex>
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
    case EmitterShape::fluid_surface: return fp::EmissionShape::Box;
    case EmitterShape::parts: return fp::EmissionShape::Box;
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

std::unique_ptr<fg::Texture> make_texture(const ParticlePixels& p) {
    if (p.width <= 0 || p.height <= 0) return nullptr;
    fizmo::images::BitmapImage img(static_cast<unsigned>(p.width), static_cast<unsigned>(p.height));
    std::vector<fizmo::colors::Color>& px = img.mutable_pixels();
    const std::size_t n = std::min(px.size(), p.rgba.size() / 4);
    for (std::size_t i = 0; i < n; ++i) px[i] = fizmo::colors::Color(p.rgba[i * 4], p.rgba[i * 4 + 1], p.rgba[i * 4 + 2], p.rgba[i * 4 + 3]);
    auto tex = std::make_unique<fg::Texture>(std::move(img), fg::SampleFilter::Bilinear, fg::WrapMode::Clamp);
    tex->set_mipmaps(true);
    return tex;
}

struct BuiltinCache {
    std::mutex mutex;
    std::unordered_map<std::string, std::unique_ptr<fg::Texture>> ready;
    std::future<std::vector<std::pair<std::string, ParticlePixels>>> warming;
};

BuiltinCache& builtin_cache() {
    static BuiltinCache cache;
    return cache;
}

void warm_builtin_textures() {
    BuiltinCache& c = builtin_cache();
    std::lock_guard<std::mutex> lock(c.mutex);
    if (c.warming.valid() || !c.ready.empty()) return;
    c.warming = std::async(std::launch::async, [] {
        std::vector<std::pair<std::string, ParticlePixels>> out;
        for (const BuiltinParticleTexture& t : builtin_particle_textures()) out.emplace_back(t.name, generate_builtin_particle_texture(t.name));
        return out;
    });
}

std::string effective_texture(const Emitter& e) {
    if (!e.texture.empty()) return e.texture;
    if (const char* b = builtin_particle_texture_for_preset(e.preset)) return b;
    return std::string();
}

void flipbook_grid(int frames, unsigned width, unsigned height, int& columns, int& rows) {
    columns = std::max(1, frames);
    rows = 1;
    if (frames <= 1 || width == 0 || height == 0) return;
    float best = 1e30f;
    for (int c = 1; c <= frames; ++c) {
        const int r = (frames + c - 1) / c;
        if (c * r - frames >= c) continue;
        const float aspect = (static_cast<float>(width) / static_cast<float>(c)) / (static_cast<float>(height) / static_cast<float>(r));
        const float score = std::fabs(std::log(aspect));
        if (score < best - 1e-4f) {
            best = score;
            columns = c;
            rows = r;
        }
    }
}

void apply_builtin(fp::EmitterSettings& s, const BuiltinParticleTexture& b, float up_sign) {
    s.frames_x = std::max(1, b.columns);
    s.frames_y = std::max(1, b.rows);
    s.frame_count = s.frames_x * s.frames_y;
    s.random_start_frame = false;
    switch (b.mode) {
    case ParticleFlipbook::single: s.flipbook = fp::FlipbookMode::OverLife; break;
    case ParticleFlipbook::over_life: s.flipbook = fp::FlipbookMode::OverLife; break;
    case ParticleFlipbook::random: s.flipbook = fp::FlipbookMode::Random; break;
    case ParticleFlipbook::fps:
        s.flipbook = fp::FlipbookMode::Fps;
        s.frame_fps = b.fps;
        s.random_start_frame = true;
        break;
    }
    const bool billboard = s.render_mode == fp::RenderMode::Billboard;
    if (billboard && b.rotation > 0.0f) s.rotation = b.rotation >= 360.0f ? fp::RangeF(0.0f, 360.0f) : fp::RangeF(-b.rotation, b.rotation);
    if (billboard && b.spin > 0.0f) s.spin = fp::RangeF(-b.spin, b.spin);
    const float a0 = s.alpha_over_life.keys[0].v;
    const float a1 = s.alpha_over_life.evaluate(1.0f);
    const float r = s.size_over_life.evaluate(1.0f);
    const std::string_view name = b.name;
    auto mixf = [](float x, float y, float t) { return x + (y - x) * t; };
    if (name == "builtin:smoke-soft" || name == "builtin:steam-wisp") {
        s.alpha_over_life = fp::LifeCurve::constant(0.0f);
        s.alpha_over_life.add(0.15f, a0).add(0.5f, mixf(a0, a1, 0.45f)).add(1.0f, a1);
        s.size_over_life = fp::LifeCurve::constant(1.0f);
        s.size_over_life.add(0.25f, 1.0f + (r - 1.0f) * 0.5f).add(1.0f, r);
        s.spin_over_life = fp::LifeCurve::linear(1.0f, 0.3f);
        s.soft = true;
        s.soft_distance = 0.8f;
        return;
    }
    if (name == "builtin:flame") {
        s.alpha_over_life = fp::LifeCurve::constant(0.0f);
        s.alpha_over_life.add(0.1f, a0).add(0.6f, mixf(a0, a1, 0.3f)).add(1.0f, a1);
        s.size_over_life = fp::LifeCurve::constant(0.6f);
        s.size_over_life.add(0.25f, 1.0f).add(1.0f, r);
        s.soft = true;
        s.soft_distance = 0.25f;
        s.pivot_y = 0.38f * up_sign;
        return;
    }
    if (name == "builtin:flash") {
        s.alpha_over_life = fp::LifeCurve::constant(a0);
        s.alpha_over_life.add(0.25f, mixf(a0, a1, 0.3f)).add(1.0f, a1);
        return;
    }
    if (name == "builtin:smoke" || name == "builtin:steam" || name == "builtin:dust") {
        s.alpha_over_life = fp::LifeCurve::constant(0.0f);
        s.alpha_over_life.add(name == "builtin:dust" ? 0.25f : 0.1f, a0).add(0.5f, mixf(a0, a1, 0.4f)).add(1.0f, a1);
        s.size_over_life = fp::LifeCurve::constant(1.0f);
        s.size_over_life.add(0.3f, 1.0f + (r - 1.0f) * 0.55f).add(1.0f, r);
        s.spin_over_life = fp::LifeCurve::linear(1.0f, 0.3f);
        s.soft = true;
        s.soft_distance = 0.6f;
        if (name != "builtin:dust") s.pivot_y = 0.22f * up_sign;
    } else if (name == "builtin:fire") {
        s.alpha_over_life = fp::LifeCurve::constant(0.0f);
        s.alpha_over_life.add(0.08f, a0).add(0.55f, mixf(a0, a1, 0.35f)).add(1.0f, a1);
        s.size_over_life = fp::LifeCurve::constant(0.75f);
        s.size_over_life.add(0.2f, 1.0f).add(1.0f, r);
        s.soft = true;
        s.soft_distance = 0.3f;
        s.pivot_y = 0.36f * up_sign;
    } else if (name == "builtin:explosion") {
        s.alpha_over_life = fp::LifeCurve::constant(a0);
        s.alpha_over_life.add(0.3f, a0).add(1.0f, a1);
        s.size_over_life = fp::LifeCurve::constant(0.45f);
        s.size_over_life.add(0.25f, 1.0f + (r - 1.0f) * 0.45f).add(1.0f, r);
        s.spin_over_life = fp::LifeCurve::linear(1.0f, 0.2f);
        s.soft = true;
        s.soft_distance = 0.6f;
    } else if (name == "builtin:magic") {
        s.alpha_over_life = fp::LifeCurve::constant(0.0f);
        s.alpha_over_life.add(0.12f, a0).add(1.0f, a1);
        s.size_over_life = fp::LifeCurve::constant(0.2f);
        s.size_over_life.add(0.15f, 1.0f).add(1.0f, r);
    } else if (name == "builtin:bubble") {
        s.alpha_over_life = fp::LifeCurve::constant(0.0f);
        s.alpha_over_life.add(0.1f, a0).add(0.92f, a1).add(1.0f, 0.0f);
        s.size_over_life = fp::LifeCurve::constant(0.3f);
        s.size_over_life.add(0.12f, 1.0f).add(1.0f, r);
    } else if (name == "builtin:snow" || name == "builtin:ember") {
        s.alpha_over_life = fp::LifeCurve::constant(0.0f);
        s.alpha_over_life.add(0.06f, a0).add(1.0f, a1);
    }
}

} // namespace

const fg::Texture* builtin_particle_texture(const std::string& name) {
    const BuiltinParticleTexture* info = find_builtin_particle_texture(name);
    if (!info) return nullptr;
    BuiltinCache& c = builtin_cache();
    std::lock_guard<std::mutex> lock(c.mutex);
    const auto found = c.ready.find(name);
    if (found != c.ready.end()) return found->second.get();
    if (c.warming.valid() && c.warming.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        for (const auto& [n, pixels] : c.warming.get())
            if (!c.ready.count(n))
                if (auto t = make_texture(pixels)) c.ready.emplace(n, std::move(t));
        const auto again = c.ready.find(name);
        if (again != c.ready.end()) return again->second.get();
    }
    auto t = make_texture(generate_builtin_particle_texture(info->name));
    const fg::Texture* out = t.get();
    if (t) c.ready.emplace(name, std::move(t));
    return out;
}

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
    const bool decal = e.flat && !flat && up.y > 0.9f;
    if (decal) s.render_mode = fp::RenderMode::Horizontal;
    s.stretch = std::max(0.0f, e.stretch);
    s.blend = additive ? fp::ParticleBlend::Additive : fp::ParticleBlend::Alpha;
    s.lit = e.lit;
    s.near_fade = flat ? 0.0f : 0.3f;
    s.texture = effective_texture(e);
    s.frames_x = std::max(1, e.frames);
    s.frames_y = 1;
    s.frame_count = std::max(1, e.frames);
    if (const BuiltinParticleTexture* b = find_builtin_particle_texture(s.texture)) apply_builtin(s, *b, flat && up.y < 0.0f ? -1.0f : 1.0f);
    const bool soft_look = e.preset == "fire" || e.preset == "smoke" || e.preset == "steam" || e.preset == "explosion" || e.preset == "dust";
    if (flat || e.soft == "off") {
        s.soft = false;
    } else if (soft_look || s.soft || e.soft == "on") {
        s.soft = true;
        const float base = e.preset == "fire" ? 0.3f : (e.preset == "dust" ? 0.4f : 0.6f);
        s.soft_distance = std::clamp(std::max({s.soft_distance, base, size * 0.45f}), 0.05f, 4.0f);
        if (e.soft_distance > 0.0f) s.soft_distance = e.soft_distance;
    }
    if (e.color_mid_at >= 0.0f) {
        const float at = std::clamp(e.color_mid_at, 0.02f, 0.98f);
        s.color_over_life = fp::LifeGradient::constant(fp::ColorF(e.color.x, e.color.y, e.color.z, 1.0f));
        s.color_over_life.add(at, fp::ColorF(e.color_mid.x, e.color_mid.y, e.color_mid.z, 1.0f)).add(1.0f, fp::ColorF(e.color_end.x, e.color_end.y, e.color_end.z, 1.0f));
    }
    if (e.fade_in > 0.0f) {
        const float a0 = std::clamp(e.alpha * boost, 0.0f, 1.0f), a1 = std::clamp(e.alpha_end * boost, 0.0f, 1.0f);
        const float in = std::clamp(e.fade_in, 0.01f, 0.9f);
        s.alpha_over_life = fp::LifeCurve::constant(0.0f);
        s.alpha_over_life.add(in, a0).add(in + (1.0f - in) * 0.45f, a0 + (a1 - a0) * 0.35f).add(1.0f, a1);
    }
    if (e.grow > 0.0f) {
        const float end = safe_ratio(e.size_end, size);
        const float k = 1.0f + 5.0f * std::clamp(e.grow, 0.0f, 1.0f);
        s.size_over_life = fp::LifeCurve::constant(1.0f);
        for (int i = 1; i <= 6; ++i) {
            const float t = static_cast<float>(i) / 6.0f;
            s.size_over_life.add(t, 1.0f + (end - 1.0f) * (1.0f - std::pow(1.0f - t, k)));
        }
    }
    if (e.spin > 0.0f && s.render_mode == fp::RenderMode::Billboard) {
        s.rotation = fp::RangeF(0.0f, 360.0f);
        s.spin = fp::RangeF(-e.spin, e.spin);
    }
    if (e.variation > 0.0f) {
        const float lo = 1.0f - std::clamp(e.variation, 0.0f, 1.0f);
        s.color = fp::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
        s.color_alt = fp::ColorF(lo, lo, lo, 1.0f);
    }
    if (decal) {
        const float a0 = std::clamp(e.alpha, 0.0f, 1.0f), a1 = std::clamp(e.alpha_end, 0.0f, 1.0f);
        s.alpha_over_life = fp::LifeCurve::constant(0.0f);
        s.alpha_over_life.add(0.002f, a0).add(0.9f, a0 + (a1 - a0) * 0.9f).add(1.0f, 0.0f);
        s.rotation = fp::RangeF(0.0f, 360.0f);
        s.spin = fp::RangeF(0.0f, 0.0f);
        s.soft = false;
        s.near_fade = 0.0f;
    }
    s.start_delay = std::max(0.0f, e.start_delay);
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
    struct Area {
        fp::Emitter* emitter = nullptr;
        fp::EmitterSettings settings;
    };
    struct Slot {
        std::vector<Area> areas;
        std::string texture;
        const fg::Texture* tex = nullptr;
        bool resolved = false;
        bool seen = false;
    };
    struct Place {
        Vec3 position;
        Vec3 right{1.0f, 0.0f, 0.0f};
        Vec3 up{0.0f, 1.0f, 0.0f};
        Vec3 forward{0.0f, 0.0f, 1.0f};
        EmitterShape shape = EmitterShape::point;
        Vec3 size;
        float share = 1.0f;
    };
    std::unordered_map<std::uint64_t, Slot> slots;
    std::vector<std::pair<Entity, std::pair<int, int>>> pending;
    std::function<const fg::Texture*(const std::string&)> lookup;
    bool flat = false;

    Impl() : system(defaults()) {
        warm_builtin_textures();
    }

    const fg::Texture* resolve(const std::string& name) const {
        if (name.empty()) return nullptr;
        if (is_builtin_particle_texture(name)) return builtin_particle_texture(name);
        return lookup ? lookup(name) : nullptr;
    }

    void texture_children(fp::Emitter& e) const {
        for (std::size_t i = 0; i < e.child_count(); ++i) {
            fp::Emitter* c = e.child(i);
            if (!c) continue;
            const fg::Texture* t = resolve(c->settings().texture);
            if (c->texture() != t) c->set_texture(t);
            texture_children(*c);
        }
    }

    static fp::SystemSettings defaults() {
        fp::SystemSettings s;
        s.budget = 60000;
        s.adaptive = true;
        s.target_ms = 2.5f;
        s.threads = 1;
        return s;
    }

    void apply(Slot& slot, int action, int count) {
        const std::size_t parts = std::max<std::size_t>(1, slot.areas.size());
        for (Area& a : slot.areas) {
            if (!a.emitter) continue;
            if (action == 0) a.emitter->stop();
            else if (action == 1) a.emitter->play();
            else if (action == 2) {
                int n = count;
                if (n <= 0) {
                    for (const fp::Burst& b : a.settings.bursts) n = std::max(n, b.count);
                    if (n <= 0) n = std::max(8, static_cast<int>(a.settings.rate * 0.5f));
                } else {
                    n = std::max(1, static_cast<int>(n / static_cast<int>(parts)));
                }
                a.emitter->burst(std::min(n, 100000));
            } else if (action == 3) {
                a.emitter->clear();
            }
        }
    }

    void destroy_areas(Slot& slot) {
        for (Area& a : slot.areas)
            if (a.emitter) system.destroy(a.emitter);
        slot.areas.clear();
    }

    void clear_all() {
        for (auto& [id, slot] : slots) destroy_areas(slot);
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
        const Mat4 w = scene.world_matrix(e);
        std::vector<Impl::Place> places;
        auto basis_of = [](const Mat4& m4, Impl::Place& p) {
            p.right = unit_or(column(m4, 0), Vec3{1.0f, 0.0f, 0.0f});
            p.up = unit_or(column(m4, 1), Vec3{0.0f, 1.0f, 0.0f});
            p.forward = unit_or(column(m4, 2), Vec3{0.0f, 0.0f, 1.0f});
        };
        if (pe->shape == EmitterShape::fluid_surface) {
            Entity owner = e;
            for (Entity at = e; scene.alive(at) && !scene.fluid(owner); at = scene.parent(at)) owner = at;
            if (!scene.fluid(owner))
                for (Entity c : scene.subtree(e))
                    if (scene.fluid(c)) {
                        owner = c;
                        break;
                    }
            Impl::Place p;
            const Mat4 fw = scene.world_matrix(owner);
            basis_of(fw, p);
            p.shape = EmitterShape::box;
            if (const Fluid* fl = scene.fluid(owner)) {
                const Vec3 sx = column(fw, 0), sy = column(fw, 1), sz = column(fw, 2);
                const float hx = length(sx) * fl->size.x, hy = length(sy) * fl->size.y, hz = length(sz) * fl->size.z;
                const float level = std::clamp(fl->fill, 0.0f, 1.0f);
                p.position = mul_point(fw, Vec3{0.0f, 0.0f, 0.0f}) + p.up * (hy * (level - 0.5f));
                p.size = flat ? Vec3{hx, 0.01f, 0.01f} : Vec3{hx, 0.02f, hz};
            } else {
                p.position = mul_point(w, Vec3{0.0f, 0.0f, 0.0f});
                p.size = pe->shape_size;
            }
            places.push_back(p);
        } else if (pe->shape == EmitterShape::parts) {
            std::string filter;
            for (char ch : pe->part) filter += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            float total = 0.0f;
            Entity source = e;
            auto has_parts = [&scene](Entity at) {
                for (Entity c : scene.subtree(at))
                    if (const std::vector<Collider>* cs = scene.colliders(c); cs && !cs->empty()) return true;
                return false;
            };
            while (!has_parts(source) && scene.alive(scene.parent(source))) source = scene.parent(source);
            for (Entity c : scene.subtree(source)) {
                const std::vector<Collider>* cs = scene.colliders(c);
                if (!cs) continue;
                if (!filter.empty()) {
                    std::string n;
                    for (char ch : scene.name(c)) n += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                    bool hit = false;
                    std::size_t from = 0;
                    while (from <= filter.size()) {
                        const std::size_t comma = filter.find(',', from);
                        std::string want = filter.substr(from, comma == std::string::npos ? std::string::npos : comma - from);
                        while (!want.empty() && want.front() == ' ') want.erase(want.begin());
                        while (!want.empty() && want.back() == ' ') want.pop_back();
                        if (!want.empty() && n.find(want) != std::string::npos) hit = true;
                        if (comma == std::string::npos) break;
                        from = comma + 1;
                    }
                    if (!hit) continue;
                }
                const Mat4 cw = scene.world_matrix(c);
                for (const Collider& col : *cs) {
                    if (!col.enabled || places.size() >= 64) continue;
                    Impl::Place p;
                    basis_of(cw, p);
                    p.position = mul_point(cw, col.offset);
                    const Vec3 sc{length(column(cw, 0)), length(column(cw, 1)), length(column(cw, 2))};
                    p.size = Vec3{col.size.x * sc.x, col.size.y * sc.y, col.size.z * sc.z};
                    p.shape = col.shape == ColliderShape::sphere ? EmitterShape::sphere : EmitterShape::box;
                    if (p.shape == EmitterShape::sphere) p.size = p.size * 0.5f;
                    p.share = std::max(1e-4f, p.size.x * p.size.y * (flat ? 1.0f : p.size.z));
                    total += p.share;
                    places.push_back(p);
                }
            }
            for (Impl::Place& p : places) p.share = total > 0.0f ? p.share / total : 1.0f;
            if (places.empty()) {
                Impl::Place p;
                basis_of(w, p);
                p.position = mul_point(w, Vec3{0.0f, 0.0f, 0.0f});
                places.push_back(p);
            }
        } else {
            Impl::Place p;
            basis_of(w, p);
            p.position = mul_point(w, Vec3{0.0f, 0.0f, 0.0f});
            p.shape = pe->shape;
            p.size = pe->shape_size;
            places.push_back(p);
        }
        if (slot.areas.size() != places.size()) {
            m->destroy_areas(slot);
            slot.areas.resize(places.size());
        }
        const std::string texture_name = effective_texture(*pe);
        const bool new_texture = slot.texture != texture_name || !slot.resolved || (!slot.tex && !texture_name.empty());
        if (new_texture) {
            slot.texture = texture_name;
            slot.tex = m->resolve(texture_name);
            slot.resolved = true;
        }
        const bool builtin = is_builtin_particle_texture(texture_name);
        for (std::size_t i = 0; i < places.size(); ++i) {
            const Impl::Place& p = places[i];
            Emitter local = *pe;
            local.shape = p.shape;
            local.shape_size = p.size;
            if (places.size() > 1) {
                local.rate = pe->rate * p.share;
                local.burst = std::max(pe->burst > 0 ? 1 : 0, static_cast<int>(std::lround(pe->burst * p.share)));
                local.max_particles = std::max(8, static_cast<int>(std::lround(pe->max_particles * p.share)));
            }
            if (pe->shape == EmitterShape::fluid_surface && local.spread < 1.0f) local.spread = 1.0f;
            fp::EmitterSettings want = particle_settings(local, flat, up);
            if (!builtin && slot.tex && pe->frames > 1) {
                int columns = 1, rows = 1;
                flipbook_grid(pe->frames, slot.tex->width(), slot.tex->height(), columns, rows);
                want.frames_x = columns;
                want.frames_y = rows;
                want.frame_count = pe->frames;
            }
            any_collide = any_collide || want.collide;
            Impl::Area& a = slot.areas[i];
            if (!a.emitter) {
                a.settings = want;
                a.emitter = &m->system.create_emitter(want, dv(p.position));
                if (!pe->autoplay) a.emitter->stop();
                a.emitter->set_texture(slot.tex);
            } else if (!(a.settings == want)) {
                const bool was = a.emitter->playing();
                a.settings = want;
                a.emitter->set_settings(want);
                if (!was && !pe->autoplay) a.emitter->stop();
            }
            if (new_texture || a.emitter->texture() != slot.tex) a.emitter->set_texture(slot.tex);
            m->texture_children(*a.emitter);
            if (flat) {
                a.emitter->set_position(p.position.x, p.position.y, 0.0);
                a.emitter->set_angle(std::atan2(p.right.y, p.right.x) * 180.0 / 3.14159265358979323846);
            } else {
                a.emitter->set_position(dv(p.position));
                a.emitter->set_basis(dv(p.right), dv(p.up), dv(p.forward));
            }
        }
    }
    for (auto it = m->slots.begin(); it != m->slots.end();) {
        if (!it->second.seen) {
            m->destroy_areas(it->second);
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
