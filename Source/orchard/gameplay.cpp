#include <orchard/gameplay.hpp>

#include <orchard/effects.hpp>
#include <orchard/scripting.hpp>

#include <algorithm>
#include <cmath>

namespace orchard {

namespace {

Quat from_basis(Vec3 right, Vec3 up, Vec3 forward) {
    const float m00 = right.x, m01 = up.x, m02 = forward.x;
    const float m10 = right.y, m11 = up.y, m12 = forward.y;
    const float m20 = right.z, m21 = up.z, m22 = forward.z;
    const float trace = m00 + m11 + m22;
    Quat q;
    if (trace > 0.0f) {
        const float s = std::sqrt(trace + 1.0f) * 2.0f;
        q = {(m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s, 0.25f * s};
    } else if (m00 > m11 && m00 > m22) {
        const float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
        q = {0.25f * s, (m01 + m10) / s, (m02 + m20) / s, (m21 - m12) / s};
    } else if (m11 > m22) {
        const float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
        q = {(m01 + m10) / s, 0.25f * s, (m12 + m21) / s, (m02 - m20) / s};
    } else {
        const float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
        q = {(m02 + m20) / s, (m12 + m21) / s, 0.25f * s, (m10 - m01) / s};
    }
    return normalize(q);
}

Quat world_rot(const Scene& s, Entity e) {
    Quat q{0.0f, 0.0f, 0.0f, 1.0f};
    for (Entity at = e; s.alive(at); at = s.parent(at))
        if (const Transform* t = s.transform(at)) q = t->rotation * q;
    return normalize(q);
}

} // namespace

void Gameplay::start(Scene* scene, ScriptPhysics* physics) {
    stop();
    m_scene = scene;
    m_physics = physics;
    m_now = 0.0;
    if (!m_scene) return;
    for (Entity e : m_scene->entities()) {
        if (!m_scene->health(e)) continue;
        State& st = state(e);
        if (const Transform* t = m_scene->transform(e)) {
            st.start = *t;
            st.have_start = true;
        }
    }
}

void Gameplay::stop() {
    m_scene = nullptr;
    m_physics = nullptr;
    m_state.clear();
    m_fires.clear();
    m_temporary.clear();
    m_doomed.clear();
    m_blasts.clear();
}

Gameplay::State& Gameplay::state(Entity e) {
    return m_state[e.id()];
}

Entity Gameplay::holder(Entity e) const {
    if (!m_scene) return Entity{};
    for (Entity at = e; m_scene->alive(at); at = m_scene->parent(at))
        if (m_scene->health(at)) return at;
    return Entity{};
}

Vec3 Gameplay::position(Entity e) const {
    if (!m_scene || !m_scene->alive(e)) return Vec3{};
    return mul_point(m_scene->world_matrix(e), Vec3{0.0f, 0.0f, 0.0f});
}

float Gameplay::size_of(Entity e) const {
    float best = 0.3f;
    if (!m_scene) return best;
    for (Entity c : m_scene->subtree(e))
        if (const std::vector<Collider>* cs = m_scene->colliders(c))
            for (const Collider& col : *cs) best = std::max(best, std::max({col.size.x, col.size.y, col.size.z}));
    return best;
}

float Gameplay::health(Entity e) const {
    const Entity h = holder(e);
    const Health* hp = m_scene && m_scene->alive(h) ? m_scene->health(h) : nullptr;
    return hp ? hp->value : 0.0f;
}

float Gameplay::max_health(Entity e) const {
    const Entity h = holder(e);
    const Health* hp = m_scene && m_scene->alive(h) ? m_scene->health(h) : nullptr;
    return hp ? hp->max : 0.0f;
}

bool Gameplay::set_health(Entity e, float value) {
    if (!m_scene || !m_scene->alive(e)) return false;
    Entity h = holder(e);
    if (!m_scene->alive(h)) {
        h = e;
        m_scene->add_health(h);
    }
    Health& hp = *m_scene->health(h);
    hp.value = std::clamp(value, 0.0f, std::max(hp.max, value));
    if (hp.max < hp.value) hp.max = hp.value;
    State& st = state(h);
    if (hp.value <= 0.0f && !st.dead) die(h, Entity{});
    else if (hp.value > 0.0f) st.dead = false;
    return true;
}

bool Gameplay::set_max_health(Entity e, float value) {
    if (!m_scene || !m_scene->alive(e)) return false;
    Entity h = holder(e);
    if (!m_scene->alive(h)) {
        h = e;
        m_scene->add_health(h).value = std::max(0.0f, value);
    }
    Health& hp = *m_scene->health(h);
    hp.max = std::max(0.0f, value);
    hp.value = std::min(hp.value, hp.max);
    return true;
}

float Gameplay::heal(Entity e, float amount) {
    const Entity h = holder(e);
    if (!m_scene || !m_scene->alive(h)) return 0.0f;
    Health& hp = *m_scene->health(h);
    State& st = state(h);
    if (st.dead) return hp.value;
    if (amount < 0.0f) return damage(h, -amount);
    hp.value = std::min(hp.max, hp.value + amount);
    return hp.value;
}

float Gameplay::damage(Entity e, float amount, Entity attacker) {
    const Entity h = holder(e);
    if (!m_scene || !m_scene->alive(h)) return 0.0f;
    Health& hp = *m_scene->health(h);
    State& st = state(h);
    if (amount < 0.0f) return heal(h, -amount);
    if (st.dead || hp.invulnerable || amount <= 0.0f) return hp.value;
    hp.value = std::max(0.0f, hp.value - amount);
    st.pending += amount;
    st.hurt_at = m_now;
    if (m_scene->alive(attacker)) st.attacker = attacker;
    if (hp.value <= 0.0f) die(h, attacker);
    return hp.value;
}

bool Gameplay::kill(Entity e, Entity attacker) {
    const Entity h = holder(e);
    if (!m_scene || !m_scene->alive(h)) return false;
    Health& hp = *m_scene->health(h);
    if (state(h).dead) return false;
    state(h).pending += hp.value;
    hp.value = 0.0f;
    die(h, attacker);
    return true;
}

bool Gameplay::dead(Entity e) const {
    const Entity h = holder(e);
    if (!m_scene || !m_scene->alive(h)) return false;
    const auto it = m_state.find(h.id());
    return it != m_state.end() && it->second.dead;
}

bool Gameplay::revive(Entity e, float to) {
    const Entity h = holder(e);
    if (!m_scene || !m_scene->alive(h)) return false;
    Health& hp = *m_scene->health(h);
    State& st = state(h);
    st.dead = false;
    st.respawn_at = -1.0;
    hp.value = to > 0.0f ? std::min(to, std::max(hp.max, to)) : hp.max;
    return true;
}

bool Gameplay::set_invulnerable(Entity e, bool on) {
    const Entity h = holder(e);
    if (!m_scene || !m_scene->alive(h)) return false;
    m_scene->health(h)->invulnerable = on;
    return true;
}

float Gameplay::damage_taken(Entity e) const {
    const auto it = m_state.find(holder(e).id());
    return it == m_state.end() ? 0.0f : it->second.last;
}

bool Gameplay::died(Entity e) const {
    const auto it = m_state.find(holder(e).id());
    return it != m_state.end() && it->second.died_now;
}

Entity Gameplay::last_attacker(Entity e) const {
    const auto it = m_state.find(holder(e).id());
    return it == m_state.end() ? Entity{} : it->second.attacker;
}

void Gameplay::die(Entity e, Entity attacker) {
    State& st = state(e);
    st.dead = true;
    st.died_pending = true;
    if (m_scene->alive(attacker)) st.attacker = attacker;
    const Health& hp = *m_scene->health(e);
    switch (hp.on_death) {
    case DeathAction::nothing: break;
    case DeathAction::destroy: m_doomed.push_back(e); break;
    case DeathAction::explode:
        m_blasts.push_back(Blast{position(e), hp.explode_radius, hp.explode_force, hp.explode_damage, e});
        m_doomed.push_back(e);
        break;
    case DeathAction::shatter:
        effect("impact", position(e), std::max(1.0f, size_of(e) * 1.5f));
        m_doomed.push_back(e);
        break;
    case DeathAction::respawn: st.respawn_at = m_now + hp.respawn_delay; break;
    }
}

bool Gameplay::ignite(Entity e, float seconds) {
    if (!m_scene || !m_scene->alive(e)) return false;
    for (Fire& f : m_fires)
        if (f.target == e) {
            f.until = std::max(f.until, m_now + seconds);
            return true;
        }
    const Entity fx = add_effect(*m_scene, "burning", e, std::max(0.6f, std::min(3.0f, size_of(e))), m_now);
    m_fires.push_back(Fire{e, fx, m_now + std::max(0.1f, seconds), m_now + 0.5});
    return true;
}

bool Gameplay::extinguish(Entity e) {
    if (!m_scene) return false;
    bool any = false;
    for (auto it = m_fires.begin(); it != m_fires.end();) {
        if (it->target == e) {
            if (m_scene->alive(it->effect)) m_scene->destroy(it->effect);
            it = m_fires.erase(it);
            any = true;
        } else {
            ++it;
        }
    }
    return any;
}

bool Gameplay::burning(Entity e) const {
    for (const Fire& f : m_fires)
        if (f.target == e) return true;
    return false;
}

Entity Gameplay::effect(const std::string& name, Vec3 at, float scale, Entity parent) {
    if (!m_scene) return Entity{};
    if (scale <= 0.0f) return Entity{};
    const Entity fx = add_effect(*m_scene, name, parent, scale, m_now);
    if (!m_scene->alive(fx)) return fx;
    if (Transform* t = m_scene->transform(fx)) t->position = at;
    if (effect_is_one_shot(name)) m_temporary.emplace_back(fx, m_now + effect_length(name) * std::max(1.0f, scale));
    return fx;
}

int Gameplay::explode(Vec3 at, float radius, float force, float dmg, Entity attacker, bool show) {
    if (!m_scene) return 0;
    radius = std::max(0.1f, radius);
    if (show) {
        const float scale = std::clamp(radius / 5.0f, 0.3f, 3.0f);
        const Entity fx = effect("explosion", at, scale);
        if (m_scene->alive(fx))
            for (Entity c : m_scene->children(fx))
                if (m_scene->name(c) == "Scorch") {
                    m_scene->destroy(c);
                    break;
                }
        scorch(at, radius * 0.75f, scale, attacker);
    }
    std::vector<Entity> pushed, hurt;
    const std::vector<Entity> hits = m_physics ? m_physics->overlap_sphere(at, radius) : std::vector<Entity>{};
    int count = 0;
    for (Entity hit : hits) {
        Entity owner = m_scene->body_owner(hit);
        if (!m_scene->alive(owner)) owner = hit;
        const Vec3 p = position(owner);
        const Vec3 d = p - at;
        const float dist = length(d);
        const float fall = std::clamp(1.0f - dist / radius, 0.0f, 1.0f);
        if (std::find(pushed.begin(), pushed.end(), owner) == pushed.end()) {
            pushed.push_back(owner);
            ++count;
            if (m_physics && m_physics->has_body(owner) && force != 0.0f) {
                Vec3 dir = dist > 1e-4f ? d * (1.0f / dist) : Vec3{0.0f, 1.0f, 0.0f};
                dir = normalize(dir + Vec3{0.0f, 0.35f, 0.0f});
                m_physics->impulse(owner, dir * (force * fall));
                const Vec3 v = m_physics->velocity(owner);
                const float cap = 40.0f;
                if (length(v) > cap) m_physics->set_velocity(owner, normalize(v) * cap);
            }
        }
        const Entity h = holder(hit);
        if (m_scene->alive(h) && h != attacker && std::find(hurt.begin(), hurt.end(), h) == hurt.end()) {
            hurt.push_back(h);
            if (dmg > 0.0f) damage(h, dmg * fall, attacker);
            if (const Health* hp = m_scene->health(h); hp && hp->flammable && fall > 0.4f) ignite(h, 8.0f);
        }
    }
    return count;
}

void Gameplay::char_it(Entity e, float dt, Entity skip) {
    if (!m_scene || !m_scene->alive(e) || dt <= 0.0f) return;
    const float keep = std::exp(-0.08f * dt);
    for (Entity at : m_scene->subtree(e)) {
        if (at == skip || (m_scene->alive(skip) && m_scene->parent(at) == skip)) continue;
        if (m_scene->emitter(at) || m_scene->light(at)) continue;
        const bool shaped = m_scene->model(at) || (m_scene->colliders(at) && !m_scene->colliders(at)->empty());
        Material* m = m_scene->material(at);
        if (!m && !shaped) continue;
        if (!m) m = &m_scene->add_material(at);
        const Vec3 c = m->color;
        const float floor = 0.035f;
        m->color = Vec3{std::max(floor, c.x * keep), std::max(floor, c.y * keep), std::max(floor * 0.9f, c.z * keep)};
        m->roughness = std::min(1.0f, m->roughness + 0.05f * dt);
    }
}

Entity Gameplay::scorch(Vec3 at, float reach, float scale, Entity skip) {
    if (!m_scene || !m_physics || scale <= 0.0f) return Entity{};
    Vec3 from = at + Vec3{0.0f, 0.3f, 0.0f};
    float left = reach + 0.3f;
    for (int tries = 0; tries < 16 && left > 0.0f; ++tries) {
        Vec3 point, normal;
        float d = -1.0f;
        const Entity hit = m_physics->raycast(from, Vec3{0.0f, -1.0f, 0.0f}, left, &point, &normal, &d);
        if (!m_scene->alive(hit) || d < 0.0f) return Entity{};
        Entity owner = m_scene->body_owner(hit);
        if (!m_scene->alive(owner)) owner = hit;
        const Body* b = m_scene->body(owner);
        const bool moving = b && b->type == BodyType::dynamic;
        const bool own = m_scene->alive(skip) && (owner == skip || holder(hit) == skip);
        if (own || moving || normal.y < 0.6f) {
            const float hop = d < 0.02f ? 0.2f : 0.01f;
            from = point + Vec3{0.0f, -hop, 0.0f};
            left -= d + hop;
            continue;
        }
        const Entity fx = add_effect(*m_scene, "scorch", Entity{}, scale, m_now);
        if (!m_scene->alive(fx)) return fx;
        if (Transform* t = m_scene->transform(fx)) t->position = point;
        m_temporary.emplace_back(fx, m_now + effect_length("scorch"));
        return fx;
    }
    return Entity{};
}

bool Gameplay::teleport(Entity e, Vec3 to) {
    if (!m_scene || !m_scene->alive(e)) return false;
    Transform* t = m_scene->transform(e);
    if (!t) return false;
    const Entity p = m_scene->parent(e);
    if (m_scene->alive(p)) {
        Mat4 inv;
        if (!invert(m_scene->world_matrix(p), inv)) return false;
        t->position = mul_point(inv, to);
    } else {
        t->position = to;
    }
    if (m_physics) {
        m_physics->moved(e);
        if (m_physics->has_body(e)) {
            m_physics->set_velocity(e, Vec3{});
            m_physics->set_spin(e, Vec3{});
        }
    }
    return true;
}

bool Gameplay::look_at(Entity e, Vec3 target) {
    if (!m_scene || !m_scene->alive(e)) return false;
    Transform* t = m_scene->transform(e);
    if (!t) return false;
    const Vec3 from = position(e);
    Vec3 f = target - from;
    if (length(f) < 1e-5f) return false;
    f = normalize(f);
    Vec3 up{0.0f, 1.0f, 0.0f};
    if (std::fabs(dot(f, up)) > 0.999f) up = Vec3{0.0f, 0.0f, 1.0f};
    const Vec3 right = normalize(cross(up, f));
    const Vec3 up2 = cross(f, right);
    Quat world = from_basis(right, up2, f);
    const Entity p = m_scene->parent(e);
    if (m_scene->alive(p)) {
        const Quat pr = world_rot(*m_scene, p);
        world = Quat{-pr.x, -pr.y, -pr.z, pr.w} * world;
    }
    t->rotation = normalize(world);
    if (m_physics) m_physics->moved(e);
    return true;
}

float Gameplay::distance(Entity a, Entity b) const {
    return length(position(a) - position(b));
}

Entity Gameplay::nearest_tagged(const std::string& tag, Vec3 from, Entity skip) const {
    if (!m_scene) return Entity{};
    Entity best;
    float best_d = 1e30f;
    for (Entity e : m_scene->tagged(tag)) {
        if (e == skip) continue;
        const float d = length(position(e) - from);
        if (d < best_d) {
            best_d = d;
            best = e;
        }
    }
    return best;
}

double Gameplay::random(double low, double high) {
    if (high < low) std::swap(low, high);
    return std::uniform_real_distribution<double>(low, high)(m_rng);
}

void Gameplay::tick(double dt, double now) {
    if (!m_scene) return;
    m_now = now;
    const float fdt = static_cast<float>(std::max(0.0, dt));
    for (Entity e : m_scene->entities()) {
        Health* hp = m_scene->health(e);
        if (!hp) continue;
        State& st = state(e);
        if (!st.have_start) {
            if (const Transform* t = m_scene->transform(e)) st.start = *t;
            st.have_start = true;
        }
        if (!st.dead && hp->regen != 0.0f && now - st.hurt_at >= hp->regen_delay) {
            hp->value = std::clamp(hp->value + hp->regen * fdt, 0.0f, hp->max);
            if (hp->value <= 0.0f) die(e, Entity{});
        }
        if (hp->impact_damage > 0.0f && m_physics && m_physics->has_body(e)) {
            const Vec3 v = m_physics->velocity(e);
            if (st.have_velocity) {
                const float dv = length(v - st.velocity);
                if (dv > 7.0f) damage(e, hp->impact_damage * (dv - 7.0f) * (dv - 7.0f));
            }
            st.velocity = v;
            st.have_velocity = true;
        }
        if (st.dead && st.respawn_at >= 0.0 && now >= st.respawn_at) {
            st.dead = false;
            st.respawn_at = -1.0;
            hp->value = hp->max;
            if (Transform* t = m_scene->transform(e)) {
                *t = st.start;
                if (m_physics) {
                    m_physics->moved(e);
                    if (m_physics->has_body(e)) {
                        m_physics->set_velocity(e, Vec3{});
                        m_physics->set_spin(e, Vec3{});
                    }
                }
            }
            st.have_velocity = false;
            extinguish(e);
        }
    }
    for (std::size_t i = 0; i < m_fires.size();) {
        Fire f = m_fires[i];
        if (!m_scene->alive(f.target) || now >= f.until) {
            if (m_scene->alive(f.effect)) m_scene->destroy(f.effect);
            m_fires.erase(m_fires.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        const Entity h = holder(f.target);
        if (m_scene->alive(h))
            if (const Health* hp = m_scene->health(h)) damage(h, hp->burn_damage * fdt);
        char_it(f.target, fdt, f.effect);
        if (now >= f.next_spread && m_physics) {
            m_fires[i].next_spread = now + 0.5;
            const float reach = size_of(f.target) * 0.75f + 0.3f;
            for (Entity hit : m_physics->overlap_sphere(position(f.target), reach)) {
                const Entity other = holder(hit);
                if (!m_scene->alive(other) || other == holder(f.target) || burning(other)) continue;
                if (const Health* hp = m_scene->health(other); hp && hp->flammable) ignite(other, static_cast<float>(f.until - now) + 2.0f);
            }
        }
        ++i;
    }
    const std::vector<Blast> blasts = std::move(m_blasts);
    m_blasts.clear();
    for (const Blast& b : blasts) explode(b.at, b.radius, b.force, b.damage, b.attacker, true);
    for (auto it = m_temporary.begin(); it != m_temporary.end();) {
        if (now >= it->second || !m_scene->alive(it->first)) {
            if (m_scene->alive(it->first)) m_scene->destroy(it->first);
            it = m_temporary.erase(it);
        } else {
            ++it;
        }
    }
    const std::vector<Entity> doomed = std::move(m_doomed);
    m_doomed.clear();
    for (Entity e : doomed) {
        extinguish(e);
        if (m_scene->alive(e)) m_scene->destroy(e);
    }
    for (auto& [id, st] : m_state) {
        st.last = st.pending;
        st.pending = 0.0f;
        st.died_now = st.died_pending;
        st.died_pending = false;
    }
}

} // namespace orchard
