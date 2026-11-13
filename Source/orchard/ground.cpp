#include <orchard/effects.hpp>
#include <orchard/gameplay.hpp>
#include <orchard/scripting.hpp>

#include <algorithm>
#include <cmath>

namespace orchard {

namespace {

std::int64_t cell_key(float x, float z) {
    const auto cx = static_cast<std::int64_t>(std::floor(x / 8.0f));
    const auto cz = static_cast<std::int64_t>(std::floor(z / 8.0f));
    return (cx << 32) ^ (cz & 0xffffffffLL);
}

Quat turn_to(Vec3 from, Vec3 to) {
    const Vec3 a = normalize(from), b = normalize(to);
    const float d = dot(a, b);
    if (d > 0.999999f) return Quat{};
    if (d < -0.999999f) return Quat{1.0f, 0.0f, 0.0f, 0.0f};
    const Vec3 c = cross(a, b);
    return normalize(Quat{c.x, c.y, c.z, 1.0f + d});
}

} // namespace

const SurfaceDef* Gameplay::surface_def(const std::string& id) const {
    if (!m_scene) return builtin_surface(id.empty() ? std::string("default") : id);
    const SurfaceDef* s = m_scene->surfaces().find(id);
    return s ? s : builtin_surface("default");
}

bool Gameplay::define_surface(const std::string& id, const std::string& key, const std::string& value) {
    if (!m_scene || id.empty()) return false;
    SurfaceDef& s = m_scene->surfaces().define(id);
    if (key.empty()) return true;
    return set_surface_field(s, key, value);
}

std::string Gameplay::surface_setting(const std::string& id, const std::string& key) const {
    const SurfaceDef* s = surface_def(id);
    return s ? surface_field(*s, key) : std::string();
}

bool Gameplay::set_surface(Entity e, const std::string& id) {
    if (!m_scene || !m_scene->alive(e)) return false;
    bool any = false;
    if (std::vector<Collider>* cs = m_scene->colliders(e))
        for (Collider& c : *cs) {
            c.surface = id;
            any = true;
        }
    if (Terrain* t = m_scene->terrain(e)) {
        for (TerrainLayer& l : t->layers) l.surface = id;
        any = true;
    }
    return any;
}

std::string Gameplay::terrain_surface(Entity terrain, Vec3 point) const {
    const Terrain* t = m_scene ? m_scene->terrain(terrain) : nullptr;
    if (!t) return std::string();
    auto layer_surface = [&](std::size_t i) {
        if (i >= t->layers.size()) return std::string("grass");
        const TerrainLayer& l = t->layers[i];
        if (!l.surface.empty()) return l.surface;
        std::string g = guess_surface(l.texture);
        if (g.empty()) g = guess_surface(l.normal_map);
        if (g.empty()) g = l.color.x > 0.8f && l.color.y > 0.8f && l.color.z > 0.8f && l.texture.empty() ? std::string("snow") : std::string("dirt");
        return g;
    };
    if (t->layers.size() <= 1) return layer_surface(0);
    std::shared_ptr<const TerrainHeights> h = m_physics ? m_physics->terrain_heights(terrain) : nullptr;
    if (!h || !h->valid()) return layer_surface(0);
    const Vec3 up = m_physics ? m_physics->up_axis() : Vec3{0.0f, 1.0f, 0.0f};
    const Vec3 local = terrain_to_local(terrain_matrix(m_scene->world_matrix(terrain), up), point);
    const float scale = std::fabs(h->size.y) > 1e-6f ? 1.0f / h->size.y : 0.0f;
    float w[terrain_max_splat_layers];
    const std::size_t count = std::min<std::size_t>(t->layers.size(), terrain_max_splat_layers);
    terrain_layer_weights(*t, h->height_at(local.x, local.z) * scale, h->slope_at(local.x, local.z), w, count);
    std::size_t best = 0;
    for (std::size_t i = 1; i < count; ++i)
        if (w[i] > w[best]) best = i;
    return layer_surface(best);
}

std::string Gameplay::surface_of(Entity hit, Vec3 point) const {
    if (!m_scene || !m_scene->alive(hit)) return "default";
    for (Entity at = hit; m_scene->alive(at); at = m_scene->parent(at)) {
        if (const std::vector<Collider>* cs = m_scene->colliders(at))
            for (const Collider& c : *cs)
                if (!c.surface.empty()) return c.surface;
        if (m_scene->terrain(at)) return terrain_surface(at, point);
        if (m_scene->fluid(at)) return "water";
    }
    Entity owner = m_scene->body_owner(hit);
    if (!m_scene->alive(owner)) owner = hit;
    const Entity h = holder(owner);
    if (m_scene->alive(h) && (m_scene->survival(h) || m_scene->agent(h) || m_scene->inventory(h))) return "flesh";
    for (Entity at = hit; m_scene->alive(at); at = m_scene->parent(at)) {
        if (const Material* m = m_scene->material(at)) {
            std::string g = guess_surface(m->base_map);
            if (g.empty()) g = guess_surface(m->preset);
            if (!g.empty()) return g;
            if (m->metallic > 0.6f) return "metal";
            if (m->transmission > 0.5f) return "glass";
        }
        if (const Model* md = m_scene->model(at)) {
            const std::string g = guess_surface(md->source);
            if (!g.empty()) return g;
        }
        const std::string g = guess_surface(m_scene->name(at));
        if (!g.empty()) return g;
    }
    return "default";
}

std::string Gameplay::surface_at(Vec3 point) const {
    if (!m_physics) return "default";
    const Vec3 up = m_physics->up_axis();
    Vec3 p, n;
    float d = 0.0f;
    const Entity hit = m_physics->raycast(point + up * 0.5f, up * -1.0f, 5.0f, &p, &n, &d);
    return m_scene && m_scene->alive(hit) ? surface_of(hit, p) : std::string("default");
}

std::string Gameplay::volume_at(Vec3 feet, float height, float* depth) const {
    if (depth) *depth = 0.0f;
    if (!m_scene) return std::string();
    const Vec3 up = m_physics ? m_physics->up_axis() : Vec3{0.0f, 1.0f, 0.0f};
    if (m_physics) {
        const float d = m_physics->fluid_depth(feet + up * 0.02f);
        if (d > 0.02f) {
            if (depth) *depth = d;
            return d > height * 0.55f ? "water" : "water_shallow";
        }
        for (Entity hit : m_physics->overlap_sphere(feet + up * 0.35f, 0.35f)) {
            Entity owner = m_scene->body_owner(hit);
            if (!m_scene->alive(owner)) owner = hit;
            const std::vector<Collider>* cs = m_scene->colliders(owner);
            bool sensor = false;
            if (cs)
                for (const Collider& c : *cs) sensor = sensor || c.sensor;
            if (!sensor) continue;
            const std::string s = surface_of(owner, feet);
            if (const SurfaceDef* sd = surface_def(s); sd && sd->volume) return s;
        }
    }
    if (!m_volume_built) {
        m_volume_built = true;
        m_volume_spots.clear();
        if (m_physics)
            for (Entity e : m_scene->entities()) {
                const Terrain* t = m_scene->terrain(e);
                if (!t || t->scatter.empty()) continue;
                bool wanted = false;
                for (const TerrainScatter& sc : t->scatter) {
                    const std::string s = sc.surface.empty() ? guess_surface(sc.model) : sc.surface;
                    const SurfaceDef* sd = s.empty() ? nullptr : surface_def(s);
                    wanted = wanted || (sd && sd->volume && !sc.collide);
                }
                if (!wanted) continue;
                std::shared_ptr<const TerrainHeights> h = m_physics->terrain_heights(e);
                if (!h || !h->valid()) continue;
                const Mat4 world = terrain_matrix(m_scene->world_matrix(e), up);
                for (const ScatterInstance& inst : scatter_instances(*t, *h)) {
                    if (inst.collide) continue;
                    const std::string s = inst.surface.empty() ? guess_surface(inst.model) : inst.surface;
                    const SurfaceDef* sd = s.empty() ? nullptr : surface_def(s);
                    if (!sd || !sd->volume) continue;
                    VolumeSpot spot;
                    spot.at = mul_point(world, inst.position);
                    spot.radius = 0.7f * inst.scale;
                    spot.height = 1.1f * inst.scale;
                    spot.surface = s;
                    m_volume_spots[static_cast<std::uint64_t>(cell_key(spot.at.x, spot.at.z))].push_back(spot);
                }
            }
    }
    if (!m_volume_spots.empty()) {
        for (int dx = -1; dx <= 1; ++dx)
            for (int dz = -1; dz <= 1; ++dz) {
                auto it = m_volume_spots.find(static_cast<std::uint64_t>(cell_key(feet.x + dx * 8.0f, feet.z + dz * 8.0f)));
                if (it == m_volume_spots.end()) continue;
                for (const VolumeSpot& v : it->second) {
                    const float dist = std::hypot(feet.x - v.at.x, feet.z - v.at.z);
                    if (dist < v.radius && feet.y < v.at.y + v.height && feet.y > v.at.y - 0.5f) return v.surface;
                }
            }
    }
    return std::string();
}

std::string Gameplay::surface_under(Entity e) const {
    auto it = m_ground.find(e.id());
    if (it != m_ground.end() && !it->second.surface.empty()) return it->second.surface;
    return surface_at(position(e));
}

std::string Gameplay::wading_in(Entity e) const {
    auto it = m_ground.find(e.id());
    return it == m_ground.end() ? std::string() : it->second.wade;
}

float Gameplay::ground_speed(Entity e) const {
    auto it = m_ground.find(e.id());
    return it == m_ground.end() ? 1.0f : it->second.speed;
}

float Gameplay::ground_minus(Entity e) const {
    auto it = m_ground.find(e.id());
    return it == m_ground.end() ? 0.0f : it->second.minus;
}

float Gameplay::ground_jump(Entity e) const {
    auto it = m_ground.find(e.id());
    return it == m_ground.end() ? 1.0f : it->second.jump;
}

float Gameplay::ground_slip(Entity e) const {
    auto it = m_ground.find(e.id());
    return it == m_ground.end() ? 0.0f : it->second.slip;
}

bool Gameplay::set_ground_speed(Entity e, const std::string& text) {
    if (!m_scene || !m_scene->alive(e)) return false;
    if (text.empty()) {
        m_ground_override.erase(e.id());
        return true;
    }
    float m = 1.0f, minus = 0.0f;
    if (!parse_speed_change(text, m, minus)) return false;
    m_ground_override[e.id()] = text;
    m_ground[e.id()];
    return true;
}

std::string Gameplay::ground_speed_override(Entity e) const {
    auto it = m_ground_override.find(e.id());
    return it == m_ground_override.end() ? std::string() : it->second;
}

void Gameplay::surface_hit(Entity hit, Vec3 point, Vec3 normal, const std::string& surface) {
    if (!m_scene) return;
    const SurfaceDef* sd = surface_def(surface);
    std::string fx = sd ? sd->impact : std::string("impact");
    if (fx == "impact-flesh" && !m_scene->rule_on("blood")) fx = "impact-dust";
    const auto& names = effect_names();
    if (std::find(names.begin(), names.end(), fx) == names.end()) fx = "impact";
    const Entity e = add_effect(*m_scene, fx, Entity{}, 0.35f, m_now);
    if (m_scene->alive(e)) {
        if (Transform* t = m_scene->transform(e)) {
            t->position = point + normal * 0.01f;
            if (length(normal) > 0.5f) t->rotation = turn_to(Vec3{0.0f, 1.0f, 0.0f}, normal);
        }
        m_temporary.emplace_back(e, m_now + 1.0);
    }
    static_cast<void>(hit);
    if (sd && !sd->impact_sound.empty()) cue_sound(sd->impact_sound, point, Entity{}, 0.55f);
}

void Gameplay::tick_ground(float dt) {
    if (!m_scene || !m_physics) return;
    for (Entity e : m_scene->entities())
        if (m_scene->survival(e) || m_scene->agent(e)) m_ground[e.id()];
    const bool moving_rule = m_scene->rule_on("surface_movement");
    const float strength = std::max(0.0f, m_scene->rule_number("surface_strength")) / 100.0f;
    const bool steps = m_scene->rule_on("footsteps") && m_scene->rule_on("sounds");
    const float step_noise = std::max(0.0f, m_scene->rule_number("footstep_noise"));
    const bool weather = m_scene->rule_on("weather_ground");
    const Vec3 up = m_physics->up_axis();
    for (auto it = m_ground.begin(); it != m_ground.end();) {
        const Entity e = Entity::from_id(it->first);
        if (!m_scene->alive(e)) {
            it = m_ground.erase(it);
            continue;
        }
        Ground& g = it->second;
        ++it;
        const Vec3 at = position(e);
        if (m_now >= g.next_probe) {
            g.next_probe = m_now + 0.1;
            Vec3 p, n;
            float d = 0.0f;
            const Entity hit = ray_skip(at + up * 0.4f, up * -1.0f, 3.0f, e, &p, &n, &d);
            const float height = m_scene->survival(survivor(e)) ? std::max(0.5f, m_scene->survival(survivor(e))->head_height + 0.15f) : 1.8f;
            const bool grounded = m_scene->alive(hit) && d < 2.4f;
            Vec3 feet = grounded ? p : at;
            g.surface = grounded ? surface_of(hit, p) : std::string();
            if (grounded && weather && (m_scene->terrain(hit) || g.surface == "dirt" || g.surface == "grass" || g.surface == "leaves" || g.surface == "concrete" || g.surface == "asphalt" || g.surface == "gravel")) {
                const ClimateSample cs = climate_at(feet);
                if (cs.any && !sheltered(feet)) {
                    if (cs.snow >= 0.75f) g.surface = "deep_snow";
                    else if (cs.snow >= 0.25f) g.surface = "snow";
                    else if (cs.rain >= 0.5f && (g.surface == "dirt" || g.surface == "grass" || g.surface == "leaves")) g.surface = "mud";
                }
            }
            g.wade = volume_at(feet, height, &g.depth);
            float speed = 1.0f, minus = 0.0f, jump = 1.0f, stamina = 1.0f, slip = 0.0f;
            if (moving_rule) {
                Inventory* inv = m_scene->inventory(inventory_owner(e)) ? m_scene->inventory(inventory_owner(e)) : nullptr;
                auto ease_of = [&](const std::string& surface) {
                    float best = 0.0f;
                    if (inv)
                        for (const auto& [slot, st] : inv->worn)
                            if (const ItemDef* d = st.empty() ? nullptr : item_def(st.item)) best = std::max(best, ease_for(d->surface_ease, surface));
                    return best;
                };
                auto apply = [&](const std::string& id, bool wading) {
                    const SurfaceDef* sd = id.empty() ? nullptr : surface_def(id);
                    if (!sd) return;
                    float m = wading ? sd->wade : sd->walk, mi = wading ? 0.0f : sd->walk_minus;
                    const std::string rule = m_scene->rule("ground_" + id);
                    if (!rule.empty()) {
                        float rm = 1.0f, rmi = 0.0f;
                        if (parse_speed_change(rule, rm, rmi)) {
                            m = rm;
                            mi = rmi;
                        }
                    }
                    const float ease = ease_of(id);
                    m = 1.0f - (1.0f - m) * (1.0f - ease) * strength;
                    mi *= (1.0f - ease) * strength;
                    speed *= std::max(0.0f, m);
                    minus += mi;
                    jump *= 1.0f - (1.0f - sd->jump) * (1.0f - ease) * strength;
                    stamina = std::max(stamina, 1.0f + (sd->stamina - 1.0f) * (1.0f - ease) * strength);
                    slip = std::max(slip, sd->slip * (1.0f - ease) * strength);
                };
                apply(g.surface, false);
                if (!g.wade.empty()) apply(g.wade, true);
            }
            auto ov = m_ground_override.find(e.id());
            if (ov != m_ground_override.end()) {
                float m = 1.0f, mi = 0.0f;
                if (parse_speed_change(ov->second, m, mi)) {
                    speed = m;
                    minus = mi;
                }
            }
            g.speed = speed;
            g.minus = minus;
            g.jump = jump;
            g.stamina = stamina;
            g.slip = slip;
            if (steps && grounded && d < 0.4f + 1.3f) {
                const Vec3 flat_now{at.x, 0.0f, at.z};
                if (g.have_last) g.stride += length(flat_now - g.last);
                g.last = flat_now;
                g.have_last = true;
                const Entity sv = survivor(e);
                const bool sprint = m_scene->alive(sv) && state(sv).sprinting;
                const float stride = sprint ? 1.05f : 0.75f;
                if (g.stride >= stride) {
                    g.stride = 0.0f;
                    const std::string sid = !g.wade.empty() ? g.wade : g.surface;
                    const SurfaceDef* sd = surface_def(sid);
                    const std::string st = stance(e);
                    const float quiet = st == "crouch" ? 0.45f : st == "prone" ? 0.3f : 1.0f;
                    if (sd && !sd->step_sound.empty()) cue_sound(sd->step_sound, feet, e, 0.32f * sd->noise * quiet, 1.0f, step_noise * sd->noise * quiet * (sprint ? 1.6f : 1.0f));
                    if (sid == "snow" || sid == "deep_snow" || sid == "water_shallow" || sid == "water" || sid == "sand" || sid == "mud") {
                        const std::string fx = sid == "snow" || sid == "deep_snow" ? "footstep-snow" : sid == "sand" ? "footstep-dust" : sid == "mud" ? "footstep-mud" : "footstep-splash";
                        const Entity puff = add_effect(*m_scene, fx, Entity{}, 0.3f, m_now);
                        if (m_scene->alive(puff)) {
                            if (Transform* t = m_scene->transform(puff)) t->position = feet + up * (sid == "water" || sid == "water_shallow" ? std::max(0.0f, g.depth) : 0.02f);
                            m_temporary.emplace_back(puff, m_now + 1.2);
                        }
                    }
                }
            } else {
                g.have_last = false;
            }
        }
        static_cast<void>(dt);
    }
}

} // namespace orchard
