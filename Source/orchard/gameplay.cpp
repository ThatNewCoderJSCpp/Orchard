#include <orchard/gameplay.hpp>

#include <orchard/ai/expression.hpp>
#include <orchard/effects.hpp>
#include <orchard/scripting.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

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

EffectDef effect(const std::string& name, const std::string& type, float damage, float interval, float duration, int stacks) {
    EffectDef d;
    d.name = name;
    d.damage_type = type;
    d.damage = damage;
    d.interval = interval;
    d.duration = duration;
    d.max_stacks = stacks;
    return d;
}

std::vector<EffectDef> make_presets() {
    std::vector<EffectDef> v;
    v.push_back(effect("burning", "fire", 0.0f, 0.0f, 10.0f, 1));
    v.back().warmth = 30.0f;
    v.back().particle = "burning";
    v.push_back(effect("poison", "poison", 2.0f, 1.0f, 10.0f, 5));
    v.push_back(effect("bleeding", "bleed", 1.0f, 0.5f, 6.0f, 10));
    v.back().speed = 0.95f;
    v.push_back(effect("shocked", "shock", 4.0f, 0.0f, 3.0f, 1));
    v.back().speed = 0.5f;
    v.back().particle = "electric";
    v.push_back(effect("frozen", "cold", 0.0f, 0.0f, 5.0f, 1));
    v.back().speed = 0.4f;
    v.back().jump = 0.5f;
    v.back().warmth = -15.0f;
    v.push_back(effect("chilled", "cold", 0.0f, 0.0f, 10.0f, 1));
    v.back().speed = 0.85f;
    v.back().warmth = -8.0f;
    v.push_back(effect("warm", "magic", 0.0f, 0.0f, 30.0f, 1));
    v.back().warmth = 15.0f;
    v.push_back(effect("slowed", "magic", 0.0f, 0.0f, 5.0f, 1));
    v.back().speed = 0.6f;
    v.push_back(effect("stunned", "magic", 0.0f, 0.0f, 2.0f, 1));
    v.back().stuns = true;
    v.back().speed = 0.0f;
    v.back().jump = 0.0f;
    v.push_back(effect("staggered", "physical", 0.0f, 0.0f, 0.5f, 1));
    v.back().stuns = true;
    v.back().speed = 0.35f;
    v.back().jump = 0.0f;
    v.push_back(effect("regeneration", "magic", 0.0f, 0.0f, 10.0f, 3));
    v.back().heal = 2.0f;
    v.push_back(effect("haste", "magic", 0.0f, 0.0f, 10.0f, 1));
    v.back().speed = 1.3f;
    v.push_back(effect("strength", "magic", 0.0f, 0.0f, 30.0f, 1));
    v.back().damage_dealt = 1.5f;
    v.push_back(effect("weakness", "magic", 0.0f, 0.0f, 30.0f, 1));
    v.back().damage_dealt = 0.6f;
    v.push_back(effect("resistance", "magic", 0.0f, 0.0f, 30.0f, 1));
    v.back().damage_taken = 0.6f;
    v.push_back(effect("vulnerable", "magic", 0.0f, 0.0f, 10.0f, 1));
    v.back().damage_taken = 1.5f;
    v.push_back(effect("exhausted", "magic", 0.0f, 0.0f, 10.0f, 1));
    v.back().stamina_regen = 0.3f;
    v.back().speed = 0.8f;
    v.push_back(effect("well_fed", "magic", 0.0f, 0.0f, 60.0f, 1));
    v.back().heal = 0.5f;
    v.push_back(effect("food_poisoning", "poison", 0.5f, 1.0f, 15.0f, 1));
    v.back().hunger_rate = 4.0f;
    v.push_back(effect("wet", "cold", 0.0f, 0.0f, 30.0f, 1));
    v.back().warmth = -6.0f;
    v.back().stamina_regen = 0.9f;
    v.push_back(effect("fire_resistance", "magic", 0.0f, 0.0f, 120.0f, 1));
    v.back().resists = "fire";
    v.back().resist = 0.75f;
    v.push_back(effect("cold_resistance", "magic", 0.0f, 0.0f, 120.0f, 1));
    v.back().resists = "cold";
    v.back().resist = 0.75f;
    v.push_back(effect("poison_resistance", "magic", 0.0f, 0.0f, 120.0f, 1));
    v.back().resists = "poison";
    v.back().resist = 0.9f;
    return v;
}

float scaled(float base, float strength) {
    return std::max(0.0f, 1.0f + (base - 1.0f) * strength);
}

} // namespace

const char* to_string(PushRepeat v) {
    switch (v) {
    case PushRepeat::once: return "once";
    case PushRepeat::every_pulse: return "every_pulse";
    case PushRepeat::continuous: return "continuous";
    }
    return "once";
}

std::optional<PushRepeat> parse_push_repeat(const std::string& text) {
    if (text == "once" || text == "0") return PushRepeat::once;
    if (text == "every_pulse" || text == "every pulse" || text == "pulse" || text == "1") return PushRepeat::every_pulse;
    if (text == "continuous" || text == "always" || text == "2") return PushRepeat::continuous;
    return std::nullopt;
}

const std::vector<EffectDef>& effect_presets() {
    static const std::vector<EffectDef> all = make_presets();
    return all;
}

bool set_effect_field(EffectDef& d, const std::string& key, const std::string& value) {
    const float x = static_cast<float>(std::atof(value.c_str()));
    if (key == "damage_type" || key == "type") d.damage_type = value;
    else if (key == "particle") d.particle = value;
    else if (key == "damage") d.damage = x;
    else if (key == "interval") d.interval = std::max(0.0f, x);
    else if (key == "heal") d.heal = x;
    else if (key == "speed") d.speed = std::max(0.0f, x);
    else if (key == "jump") d.jump = std::max(0.0f, x);
    else if (key == "damage_taken") d.damage_taken = std::max(0.0f, x);
    else if (key == "damage_dealt") d.damage_dealt = std::max(0.0f, x);
    else if (key == "stamina_regen") d.stamina_regen = std::max(0.0f, x);
    else if (key == "hunger_rate") d.hunger_rate = std::max(0.0f, x);
    else if (key == "warmth") d.warmth = x;
    else if (key == "resists") d.resists = value;
    else if (key == "resist") d.resist = x;
    else if (key == "stuns") d.stuns = value != "0" && value != "false";
    else if (key == "max_stacks" || key == "stacks") d.max_stacks = std::max(1, static_cast<int>(x));
    else if (key == "duration") d.duration = x;
    else return false;
    return true;
}

void Gameplay::start(Scene* scene, ScriptPhysics* physics) {
    stop();
    m_defs.clear();
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
    m_dots.clear();
    m_effects.clear();
    m_carry.clear();
    m_zones.clear();
    m_projectiles.clear();
    m_tracers.clear();
    m_respawns.clear();
    m_ground.clear();
    m_ground_override.clear();
    m_volume_spots.clear();
    m_volume_built = false;
    m_next_wet.clear();
    m_slosh.clear();
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

float Gameplay::facing_area(Entity body, Vec3 dir) const {
    if (!m_scene) return 0.0f;
    const bool flat = m_scene->dimension() == Dimension::two_d;
    const Vec3 d{std::fabs(dir.x), std::fabs(dir.y), std::fabs(dir.z)};
    float area = 0.0f;
    for (Entity c : m_scene->subtree(body)) {
        if (c != body && m_scene->body_owner(c) != body) continue;
        const std::vector<Collider>* cs = m_scene->colliders(c);
        if (!cs) continue;
        const Mat4 w = m_scene->world_matrix(c);
        const Vec3 sc{length(Vec3{w.at(0, 0), w.at(1, 0), w.at(2, 0)}), length(Vec3{w.at(0, 1), w.at(1, 1), w.at(2, 1)}), length(Vec3{w.at(0, 2), w.at(1, 2), w.at(2, 2)})};
        for (const Collider& col : *cs) {
            if (!col.enabled || col.sensor) continue;
            const float x = std::fabs(col.size.x * sc.x), y = std::fabs(col.size.y * sc.y), z = std::fabs(col.size.z * sc.z);
            const bool curved = col.shape == ColliderShape::sphere || col.shape == ColliderShape::capsule || col.shape == ColliderShape::cylinder || col.shape == ColliderShape::cone;
            const float round = curved ? 0.785f : 1.0f;
            area += round * (flat ? d.x * y + d.y * x : d.x * y * z + d.y * x * z + d.z * x * y);
        }
    }
    return area;
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

float Gameplay::formula(const std::string& text, const std::function<bool(const std::string&, double&)>& vars, float fallback) {
    if (text.empty()) return fallback;
    auto it = m_formulas.find(text);
    if (it == m_formulas.end()) {
        std::shared_ptr<const ai::Expression> parsed;
        if (std::optional<ai::Expression> e = ai::Expression::parse(text)) parsed = std::make_shared<const ai::Expression>(std::move(*e));
        it = m_formulas.emplace(text, parsed).first;
    }
    if (!it->second) return fallback;
    const double v = it->second->evaluate([&](const std::string& name, const std::vector<double>& args, double& out) {
        if (args.empty() && vars(name, out)) return true;
        if (name == "random") {
            const double lo = args.size() >= 2 ? args[0] : 0.0, hi = args.size() >= 2 ? args[1] : (args.empty() ? 1.0 : args[0]);
            out = random(lo, hi);
            return true;
        }
        if (name == "time" && args.empty()) {
            out = m_now;
            return true;
        }
        return false;
    });
    return std::isfinite(v) ? static_cast<float>(v) : fallback;
}

const Gameplay::Active* Gameplay::active(Entity e, const std::string& name) const {
    for (const Active& a : m_effects)
        if (a.target == e && a.name == name) return &a;
    return nullptr;
}

Gameplay::Active* Gameplay::active(Entity e, const std::string& name) {
    for (Active& a : m_effects)
        if (a.target == e && a.name == name) return &a;
    return nullptr;
}

float Gameplay::effect_product(Entity e, float EffectDef::*field) const {
    float out = 1.0f;
    const Entity h = holder(e);
    for (const Active& a : m_effects) {
        if (a.target != e && a.target != h) continue;
        if (const EffectDef* d = effect_def(a.name)) out *= scaled(d->*field, a.strength);
    }
    if (burning(e) || (m_scene && m_scene->alive(h) && h != e && burning(h)))
        if (const EffectDef* d = effect_def("burning")) out *= d->*field;
    return out;
}

float Gameplay::effect_sum(Entity e, float EffectDef::*field) const {
    float out = 0.0f;
    const Entity h = holder(e);
    for (const Active& a : m_effects) {
        if (a.target != e && a.target != h) continue;
        if (const EffectDef* d = effect_def(a.name)) out += d->*field * a.strength * static_cast<float>(a.stacks);
    }
    if (burning(e) || (m_scene && m_scene->alive(h) && h != e && burning(h)))
        if (const EffectDef* d = effect_def("burning")) out += d->*field;
    return out;
}

void Gameplay::push(Entity e, Vec3 impulse, float stagger_for) {
    if (!m_scene || !m_scene->alive(e)) return;
    Entity body = e;
    while (m_scene->alive(body) && !(m_physics && m_physics->has_body(body))) body = m_scene->parent(body);
    if (m_physics && m_scene->alive(body) && length(impulse) > 0.0f) m_physics->impulse(body, impulse);
    const Entity h = holder(e);
    if (!m_scene->alive(h)) return;
    if (stagger_for > 0.0f) {
        stagger(h, stagger_for);
        return;
    }
    if (stagger_for == 0.0f || length(impulse) <= 0.0f || !m_physics || !m_scene->alive(body)) return;
    const float mass = m_physics->mass(body);
    if (mass <= 0.0f) return;
    const Health& hp = *m_scene->health(h);
    const float dv = length(impulse) / mass;
    if (dv <= hp.poise || hp.stagger_scale <= 0.0f) return;
    stagger(h, std::min(1.5f, 0.2f + 0.15f * (dv - hp.poise)) * hp.stagger_scale);
}

float Gameplay::armored(Health& hp, const Damage& d, float amount, const std::function<bool(const std::string&, double&)>& vars) {
    static_cast<void>(d);
    if (hp.armor_mode == "none") return amount;
    if (hp.armor_mode == "formula") return hp.armor_formula.empty() ? amount : formula(hp.armor_formula, vars, amount);
    if (hp.armor <= 0.0f) return amount;
    if (hp.armor_mode == "percent") return amount * (1.0f - std::clamp(hp.armor / 100.0f, 0.0f, std::clamp(hp.armor_cap, 0.0f, 1.0f)));
    if (hp.armor_mode == "flat") return std::max(amount - hp.armor, amount * std::clamp(hp.armor_min, 0.0f, 1.0f));
    const float k = std::max(1e-3f, hp.armor_constant);
    return amount * k / (k + hp.armor);
}

float Gameplay::hurt(Entity e, const Damage& d, float amount) {
    const Entity h = holder(e);
    if (!m_scene || !m_scene->alive(h)) return 0.0f;
    Health& hp = *m_scene->health(h);
    const float base = amount;
    float resist = 0.0f;
    if (amount > 0.0f && d.type != "true") {
        if (const auto r = hp.resist.find(d.type); r != hp.resist.end()) resist = r->second;
        resist += worn_resist(h, d.type);
        amount *= 1.0f - std::min(resist, 1.0f);
        amount *= effect_product(h, &EffectDef::damage_taken);
        float shielded = 0.0f;
        for (const Active& a : m_effects) {
            if (a.target != h) continue;
            const EffectDef* ed = effect_def(a.name);
            if (!ed || ed->resist == 0.0f) continue;
            std::string cur;
            for (char ch : ed->resists + ",") {
                if (ch == ',') {
                    if (cur == d.type || cur == "all") shielded += ed->resist * a.strength;
                    cur.clear();
                } else if (ch != ' ') {
                    cur += ch;
                }
            }
        }
        amount *= 1.0f - std::clamp(shielded, 0.0f, 1.0f);
        if (const Carry* c = carry_of(inventory_owner(h)); c && c->blocking && m_scene->alive(d.attacker)) {
            const Entity o = inventory_owner(h);
            float block = 0.0f;
            for (const char* hand : {"offhand", "mainhand"})
                if (const auto it = m_scene->inventory(o)->worn.find(hand); it != m_scene->inventory(o)->worn.end() && !it->second.empty())
                    if (const ItemDef* bd = item_def(it->second.item)) block = std::max(block, bd->block);
            Vec3 origin, facing;
            aim_of(o, origin, facing);
            const Vec3 to = position(d.attacker) - position(h);
            if (block > 0.0f && length(to) > 1e-3f && dot(normalize(Vec3{to.x, 0.0f, to.z}), normalize(Vec3{facing.x, 0.0f, facing.z} + Vec3{0.0f, 0.0f, 1e-4f})) > 0.3f) {
                amount *= 1.0f - block;
                use_stamina(h, 5.0f + amount * 0.5f);
            }
        }
        if (m_scene->alive(d.attacker)) amount *= damage_dealt_scale(d.attacker);
    }
    float current = amount;
    auto vars = [&](const std::string& name, double& out) {
        if (name == "amount") out = current;
        else if (name == "base") out = base;
        else if (name == "health") out = hp.value;
        else if (name == "max_health") out = hp.max;
        else if (name == "health_fraction") out = hp.max > 0.0f ? hp.value / hp.max : 0.0f;
        else if (name == "armor") out = hp.armor;
        else if (name == "constant") out = hp.armor_constant;
        else if (name == "shield") out = hp.shield;
        else if (name == "max_shield") out = hp.max_shield;
        else if (name == "shield_fraction") out = hp.max_shield > 0.0f ? hp.shield / hp.max_shield : 0.0f;
        else if (name == "resist") out = resist;
        else if (name == "over_time") out = d.over_time() ? 1.0 : 0.0;
        else if (name == "staggered") out = staggered(h) > 0.0f ? 1.0 : 0.0;
        else if (name == "knockback") out = length(d.knockback);
        else if (name == "has_attacker") out = m_scene->alive(d.attacker) ? 1.0 : 0.0;
        else if (name == "distance") out = m_scene->alive(d.attacker) ? distance(h, d.attacker) : 0.0;
        else if (name.rfind("is_", 0) == 0) out = name.substr(3) == d.type ? 1.0 : 0.0;
        else if (name.rfind("has_", 0) == 0) out = effect_stacks(h, name.substr(4));
        else return false;
        return true;
    };
    const float worn = d.type == "true" ? 0.0f : worn_armor(h, d.type);
    if (amount > 0.0f && (type_listed(hp.armor_types, d.type, false) || worn > 0.0f)) {
        const float before = amount;
        const float own = hp.armor;
        if (!type_listed(hp.armor_types, d.type, false)) hp.armor = 0.0f;
        hp.armor += worn;
        hp.armor *= std::max(0.0f, d.armor_scale);
        amount = std::max(0.0f, armored(hp, d, amount, vars));
        hp.armor = own;
        if (hp.armor_wear > 0.0f && before > amount) hp.armor = std::max(0.0f, hp.armor - (before - amount) * hp.armor_wear);
    }
    current = amount;
    if (!hp.damage_formula.empty()) amount = formula(hp.damage_formula, vars, amount);
    State& st = state(h);
    if (amount > 0.0f && hp.shield > 0.0f && type_listed(hp.shield_types, d.type, true)) {
        current = amount;
        float absorbed = hp.shield_formula.empty() ? amount * std::clamp(hp.shield_absorb, 0.0f, 1.0f) : formula(hp.shield_formula, vars, amount);
        absorbed = std::clamp(absorbed, 0.0f, std::min(amount, hp.shield));
        hp.shield -= absorbed;
        amount -= absorbed;
        st.hurt_at = m_now;
        st.last_type = d.type;
        if (m_scene->alive(d.attacker)) st.attacker = d.attacker;
    }
    if (amount == 0.0f) return hp.value;
    st.last_type = d.type;
    if (m_scene->alive(d.attacker) && d.key != "thorns" && d.key != "extra" && !d.over_time() && d.attacker != h) {
        const Entity o = inventory_owner(h);
        if (const Inventory* inv = m_scene->alive(o) ? m_scene->inventory(o) : nullptr) {
            std::string fx, extra;
            for (const auto& [slot, s] : inv->worn) {
                if (s.empty() || slot == "mainhand" || slot == "offhand") continue;
                const ItemDef* wd = item_def(s.item);
                if (!wd) continue;
                if (!wd->thorns.empty()) fx += (fx.empty() ? "" : ", ") + wd->thorns;
                if (!wd->thorns_damage.empty()) extra += (extra.empty() ? "" : ", ") + wd->thorns_damage;
            }
            if (!fx.empty() || !extra.empty()) {
                const Entity attacker = d.attacker;
                const float left = damage(h, amount, d.attacker);
                for (const auto& [type, a] : parse_amounts(extra)) {
                    Damage back;
                    back.amount = a;
                    back.type = type;
                    back.attacker = h;
                    back.key = "thorns";
                    if (m_scene->alive(attacker) && m_scene->health(holder(attacker))) apply_damage(attacker, back);
                }
                if (m_scene->alive(attacker) && m_scene->alive(holder(attacker)))
                    for (const EffectRoll& r : parse_effect_rolls(fx))
                        if (random(0.0, 1.0) < r.chance) apply_effect(holder(attacker), r.effect, r.duration, r.strength, h);
                return left;
            }
        }
    }
    return damage(h, amount, d.attacker);
}

float Gameplay::apply_damage(Entity e, const Damage& d) {
    if (!m_scene || !m_scene->alive(e)) return 0.0f;
    if (d.over_time()) {
        const std::string key = d.key.empty() ? d.type : d.key;
        if (!d.stack)
            for (Dot& dot : m_dots)
                if (dot.target == e && (dot.damage.key.empty() ? dot.damage.type : dot.damage.key) == key) {
                    dot.damage = d;
                    dot.until = d.forever() ? -1.0 : m_now + d.duration;
                    return health(e);
                }
        Dot dot;
        dot.target = e;
        dot.damage = d;
        dot.until = d.forever() ? -1.0 : m_now + d.duration;
        dot.next_pulse = d.interval > 0.0f ? m_now : 0.0;
        dot.next_push = m_now;
        m_dots.push_back(dot);
        return health(e);
    }
    push(e, d.knockback, d.stagger);
    return hurt(e, d, d.amount);
}

bool Gameplay::stop_damage(Entity e, const std::string& key) {
    const std::size_t before = m_dots.size();
    m_dots.erase(std::remove_if(m_dots.begin(), m_dots.end(), [&](const Dot& d) {
        return d.target == e && (key.empty() || (d.damage.key.empty() ? d.damage.type : d.damage.key) == key);
    }), m_dots.end());
    return m_dots.size() != before;
}

int Gameplay::damage_over_time_count(Entity e) const {
    int n = 0;
    for (const Dot& d : m_dots)
        if (d.target == e) ++n;
    return n;
}

std::string Gameplay::last_damage_type(Entity e) const {
    const auto it = m_state.find(holder(e).id());
    return it == m_state.end() ? std::string() : it->second.last_type;
}

float Gameplay::resistance(Entity e, const std::string& type) const {
    const Entity h = holder(e);
    if (!m_scene || !m_scene->alive(h)) return 0.0f;
    const auto& r = m_scene->health(h)->resist;
    const auto it = r.find(type);
    return it == r.end() ? 0.0f : it->second;
}

bool Gameplay::set_resistance(Entity e, const std::string& type, float value) {
    const Entity h = holder(e);
    if (!m_scene || !m_scene->alive(h) || type.empty()) return false;
    auto& r = m_scene->health(h)->resist;
    if (value == 0.0f) r.erase(type);
    else r[type] = value;
    return true;
}

bool Gameplay::set_damage_formula(Entity e, const std::string& text) {
    const Entity h = holder(e);
    if (!m_scene || !m_scene->alive(h)) return false;
    m_scene->health(h)->damage_formula = text;
    return true;
}

bool Gameplay::set_effect_formula(Entity e, const std::string& text) {
    const Entity h = holder(e);
    if (!m_scene || !m_scene->alive(h)) return false;
    m_scene->health(h)->effect_formula = text;
    return true;
}

void Gameplay::define_effect(const EffectDef& def) {
    if (!def.name.empty()) m_defs[def.name] = def;
}

const EffectDef* Gameplay::effect_def(const std::string& name) const {
    if (const auto it = m_defs.find(name); it != m_defs.end()) return &it->second;
    for (const EffectDef& d : effect_presets())
        if (d.name == name) return &d;
    return nullptr;
}

bool Gameplay::set_effect_value(const std::string& name, const std::string& key, const std::string& value) {
    if (name.empty()) return false;
    auto it = m_defs.find(name);
    if (it == m_defs.end()) {
        const EffectDef* base = effect_def(name);
        EffectDef d = base ? *base : EffectDef{};
        d.name = name;
        it = m_defs.emplace(name, d).first;
    }
    return set_effect_field(it->second, key, value);
}

bool Gameplay::apply_effect(Entity e, const std::string& name, float duration, float strength, Entity source) {
    if (!m_scene || !m_scene->alive(e) || name.empty()) return false;
    const EffectDef* def = effect_def(name);
    if (!def) {
        set_effect_value(name, "duration", "10");
        def = effect_def(name);
    }
    if (duration == 0.0f) duration = def->duration;
    const Entity h = holder(e);
    if (m_scene->alive(h)) {
        const Health& hp = *m_scene->health(h);
        float resist = 0.0f;
        if (const auto r = hp.resist.find(name); r != hp.resist.end()) resist = r->second;
        else if (const auto t = hp.resist.find(def->damage_type); t != hp.resist.end() && (def->damage > 0.0f || def->stuns)) resist = t->second;
        if (resist >= 1.0f) return false;
        if (duration > 0.0f) duration *= 1.0f - resist;
        if (!hp.effect_formula.empty()) {
            const Active* now = active(e, name);
            const float base = duration;
            duration = formula(hp.effect_formula, [&](const std::string& n, double& out) {
                if (n == "duration") out = base;
                else if (n == "strength") out = strength;
                else if (n == "stacks") out = now ? now->stacks : 0;
                else if (n == "resist") out = resist;
                else if (n == "health") out = hp.value;
                else if (n == "max_health") out = hp.max;
                else if (n == "forever") out = base < 0.0f ? 1.0 : 0.0;
                else if (n.rfind("is_", 0) == 0) out = n.substr(3) == name ? 1.0 : 0.0;
                else return false;
                return true;
            }, duration);
            if (duration == 0.0f) return false;
        }
    }
    if (name == "burning") return ignite(e, duration < 0.0f ? 1e9f : duration);
    if (Active* a = active(e, name)) {
        a->stacks = std::min(def->max_stacks, a->stacks + 1);
        a->strength = std::max(a->strength, strength);
        if (duration < 0.0f || a->until < 0.0) a->until = -1.0;
        else a->until = std::max(a->until, m_now + duration);
        if (m_scene->alive(source)) a->source = source;
        return true;
    }
    Active a;
    a.target = e;
    a.name = name;
    a.until = duration < 0.0f ? -1.0 : m_now + duration;
    a.strength = strength;
    a.next_pulse = m_now;
    a.source = source;
    if (!def->particle.empty()) a.fx = add_effect(*m_scene, def->particle, e, std::max(0.6f, std::min(3.0f, size_of(e))), m_now);
    m_effects.push_back(a);
    return true;
}

bool Gameplay::remove_effect(Entity e, const std::string& name) {
    if (name == "burning") return extinguish(e);
    for (auto it = m_effects.begin(); it != m_effects.end(); ++it)
        if (it->target == e && it->name == name) {
            if (m_scene && m_scene->alive(it->fx)) m_scene->destroy(it->fx);
            m_effects.erase(it);
            return true;
        }
    return false;
}

int Gameplay::clear_effects(Entity e) {
    int n = 0;
    for (auto it = m_effects.begin(); it != m_effects.end();) {
        if (it->target == e) {
            if (m_scene && m_scene->alive(it->fx)) m_scene->destroy(it->fx);
            it = m_effects.erase(it);
            ++n;
        } else {
            ++it;
        }
    }
    if (extinguish(e)) ++n;
    return n;
}

int Gameplay::effect_stacks(Entity e, const std::string& name) const {
    if (name == "burning") return burning(e) ? 1 : 0;
    const Active* a = active(e, name);
    return a ? a->stacks : 0;
}

float Gameplay::effect_time_left(Entity e, const std::string& name) const {
    if (name == "burning") {
        for (const Fire& f : m_fires)
            if (f.target == e) return f.until - m_now > 1e8 ? -1.0f : static_cast<float>(std::max(0.0, f.until - m_now));
        return 0.0f;
    }
    const Active* a = active(e, name);
    if (!a) return 0.0f;
    return a->until < 0.0 ? -1.0f : static_cast<float>(std::max(0.0, a->until - m_now));
}

float Gameplay::effect_strength(Entity e, const std::string& name) const {
    if (name == "burning") return burning(e) ? 1.0f : 0.0f;
    const Active* a = active(e, name);
    return a ? a->strength : 0.0f;
}

std::vector<std::string> Gameplay::effects(Entity e) const {
    std::vector<std::string> out;
    if (burning(e)) out.push_back("burning");
    for (const Active& a : m_effects)
        if (a.target == e) out.push_back(a.name);
    return out;
}

bool Gameplay::stagger(Entity e, float seconds) {
    if (!m_scene || !m_scene->alive(e) || seconds <= 0.0f) return false;
    if (Active* a = active(e, "staggered")) {
        a->until = std::max(a->until, m_now + seconds);
        return true;
    }
    return apply_effect(e, "staggered", seconds);
}

float Gameplay::staggered(Entity e) const {
    const Entity h = holder(e);
    const float own = effect_time_left(e, "staggered");
    return own != 0.0f || !m_scene || !m_scene->alive(h) || h == e ? own : effect_time_left(h, "staggered");
}

bool Gameplay::stunned(Entity e) const {
    const Entity h = holder(e);
    for (const Active& a : m_effects) {
        if (a.target != e && a.target != h) continue;
        if (const EffectDef* d = effect_def(a.name); d && d->stuns) return true;
    }
    return false;
}

float Gameplay::move_scale(Entity e) const {
    return effect_product(e, &EffectDef::speed) * load_speed(e) * ground_speed(e);
}

float Gameplay::jump_scale(Entity e) const {
    const Entity s = survivor(e);
    const bool heavy = m_scene && m_scene->alive(s) && m_scene->survival(s)->weight_on && encumbrance(e) > 1.0f;
    return heavy ? 0.0f : effect_product(e, &EffectDef::jump) * load_speed(e) * ground_jump(e);
}

float Gameplay::carried_weight(Entity e) const {
    const Entity s = survivor(e);
    if (!m_scene || !m_scene->alive(s)) return 0.0f;
    const Survival& sv = *m_scene->survival(s);
    float w = sv.carried;
    const Entity o = inventory_owner(s);
    if (m_scene->alive(o) && m_scene->inventory(o)->count_weight) {
        const Inventory& inv = *m_scene->inventory(o);
        const ItemCatalog& cat = m_scene->items();
        w += inventory_weight(cat, inv);
        const bool worn_rule = m_scene->rule_on("worn_weight"), held_rule = m_scene->rule_on("held_weight");
        for (const auto& [slot, st] : inv.worn) {
            if (st.empty()) continue;
            const bool hand = slot == "mainhand" || slot == "offhand";
            if (hand && !held_rule) w -= stack_weight(cat, st);
            if (!hand && !worn_rule && inv.worn_weight) {
                float inside = 0.0f;
                for (const ItemStack& c : st.contents) inside += stack_weight(cat, c);
                w -= stack_weight(cat, st) - inside;
            }
        }
    }
    if (!sv.count_items) return w;
    for (Entity c : m_scene->subtree(s)) {
        if (c == s || !m_scene->has_tag(c, "item")) continue;
        const std::vector<Collider>* cs = m_scene->colliders(c);
        if (!cs) continue;
        const Mat4 m = m_scene->world_matrix(c);
        const Vec3 sc{length(Vec3{m.at(0, 0), m.at(1, 0), m.at(2, 0)}), length(Vec3{m.at(0, 1), m.at(1, 1), m.at(2, 1)}), length(Vec3{m.at(0, 2), m.at(1, 2), m.at(2, 2)})};
        for (const Collider& col : *cs)
            if (col.enabled && !col.sensor) w += collider_mass(col, sc, m_scene->dimension());
    }
    return w;
}

float Gameplay::encumbrance(Entity e) const {
    const Entity s = survivor(e);
    if (!m_scene || !m_scene->alive(s) || !m_scene->survival(s)->weight_on) return 0.0f;
    return carried_weight(s) / std::max(1e-3f, m_scene->survival(s)->carry_limit + worn_carry_bonus(s));
}

float Gameplay::load_speed(Entity e) const {
    const Entity s = survivor(e);
    if (!m_scene || !m_scene->alive(s) || !m_scene->survival(s)->weight_on) return 1.0f;
    const Survival& sv = *m_scene->survival(s);
    const float load = encumbrance(s);
    const float light = std::clamp(sv.light_load, 0.0f, 0.999f);
    if (load <= light) return 1.0f;
    if (load <= 1.0f) return 1.0f + (sv.heavy_speed - 1.0f) * (load - light) / (1.0f - light);
    if (sv.overload <= 1.0f || load >= sv.overload) return 0.0f;
    return sv.heavy_speed * (1.0f - (load - 1.0f) / (sv.overload - 1.0f));
}

bool Gameplay::set_health_setting(Entity e, const std::string& key, const std::string& value) {
    const Entity h = holder(e);
    if (!m_scene || !m_scene->alive(h)) return false;
    Health& hp = *m_scene->health(h);
    if (key == "resist") {
        hp.resist = parse_resist(value);
        return true;
    }
    if (key == "armor_mode" && std::find(armor_modes().begin(), armor_modes().end(), value) == armor_modes().end()) return false;
    if (std::string* t = health_text(hp, key)) {
        *t = value;
        return true;
    }
    if (float* v = health_value(hp, key)) {
        *v = static_cast<float>(std::atof(value.c_str()));
        hp.shield = std::clamp(hp.shield, 0.0f, std::max(hp.shield, hp.max_shield));
        return true;
    }
    return false;
}

std::string Gameplay::health_setting(Entity e, const std::string& key) const {
    const Entity h = holder(e);
    if (!m_scene || !m_scene->alive(h)) return std::string();
    Health hp = *m_scene->health(h);
    if (key == "resist") return resist_text(hp.resist);
    if (const std::string* t = health_text(hp, key)) return *t;
    if (const float* v = health_value(hp, key)) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(*v));
        return buf;
    }
    return std::string();
}

float Gameplay::damage_dealt_scale(Entity e) const {
    return effect_product(e, &EffectDef::damage_dealt);
}

Entity Gameplay::survivor(Entity e) const {
    if (!m_scene) return Entity{};
    for (Entity at = e; m_scene->alive(at); at = m_scene->parent(at))
        if (m_scene->survival(at)) return at;
    return Entity{};
}

float Gameplay::stat(Entity e, const std::string& key) const {
    if (key == "health") return health(e);
    if (key == "max_health") return max_health(e);
    if (key == "weight") return carried_weight(e);
    if (key == "encumbrance") return encumbrance(e);
    if (m_scene && m_scene->alive(holder(e))) {
        Health hp = *m_scene->health(holder(e));
        if (const float* v = health_value(hp, key)) return *v;
    }
    const Entity s = survivor(e);
    if (!m_scene || !m_scene->alive(s)) return 0.0f;
    Survival copy = *m_scene->survival(s);
    if (const float* v = survival_value(copy, key)) return *v;
    if (const bool* b = survival_switch(copy, key)) return *b ? 1.0f : 0.0f;
    return 0.0f;
}

bool Gameplay::set_stat(Entity e, const std::string& key, float value) {
    if (key == "health") return set_health(e, value);
    if (key == "max_health") return set_max_health(e, value);
    if (m_scene && m_scene->alive(holder(e)) && health_value(*m_scene->health(holder(e)), key)) {
        Health& hp = *m_scene->health(holder(e));
        *health_value(hp, key) = value;
        hp.shield = std::clamp(hp.shield, 0.0f, std::max(0.0f, hp.max_shield));
        return true;
    }
    if (!m_scene || !m_scene->alive(e)) return false;
    Entity s = survivor(e);
    if (!m_scene->alive(s)) {
        if (!survival_value(m_scene->add_survival(e), key) && !survival_switch(*m_scene->survival(e), key)) {
            m_scene->remove_survival(e);
            return false;
        }
        s = e;
    }
    Survival& sv = *m_scene->survival(s);
    if (bool* b = survival_switch(sv, key)) {
        *b = value != 0.0f;
        return true;
    }
    float* v = survival_value(sv, key);
    if (!v) return false;
    *v = value;
    sv.stamina = std::clamp(sv.stamina, 0.0f, std::max(0.0f, sv.max_stamina));
    sv.hunger = std::clamp(sv.hunger, 0.0f, std::max(0.0f, sv.max_hunger));
    sv.saturation = std::clamp(sv.saturation, 0.0f, sv.hunger);
    sv.thirst = std::clamp(sv.thirst, 0.0f, std::max(0.0f, sv.max_thirst));
    sv.breath = std::clamp(sv.breath, 0.0f, std::max(0.0f, sv.max_breath));
    return true;
}

float Gameplay::add_stat(Entity e, const std::string& key, float delta) {
    if (key == "health") return heal(e, delta);
    set_stat(e, key, stat(e, key) + delta);
    return stat(e, key);
}

float Gameplay::eat(Entity e, float food, float saturation) {
    const Entity s = survivor(e);
    if (!m_scene || !m_scene->alive(s)) return 0.0f;
    Survival& sv = *m_scene->survival(s);
    sv.hunger = std::clamp(sv.hunger + food, 0.0f, sv.max_hunger);
    sv.saturation = std::clamp(sv.saturation + saturation, 0.0f, sv.hunger);
    return sv.hunger;
}

float Gameplay::drink(Entity e, float amount) {
    const Entity s = survivor(e);
    if (!m_scene || !m_scene->alive(s)) return 0.0f;
    Survival& sv = *m_scene->survival(s);
    sv.thirst = std::clamp(sv.thirst + amount, 0.0f, sv.max_thirst);
    return sv.thirst;
}

bool Gameplay::use_stamina(Entity e, float amount) {
    const Entity s = survivor(e);
    if (!m_scene || !m_scene->alive(s)) return true;
    Survival& sv = *m_scene->survival(s);
    if (!sv.stamina_on || amount <= 0.0f) return true;
    if (sv.stamina < amount) return false;
    sv.stamina -= amount;
    state(s).stamina_used_at = m_now;
    return true;
}

Exertion Gameplay::exert(Entity e, float walk_speed, float sprint_speed, bool sprint, bool moving, bool jump, float dt) {
    Exertion out;
    if (m_scene && m_scene->alive(e)) m_ground[e.id()];
    const float scale = move_scale(e);
    out.slip = ground_slip(e);
    out.jump = jump_scale(e);
    out.can_jump = jump && out.jump > 0.0f && !stunned(e);
    const Entity s = survivor(e);
    Survival* sv = m_scene && m_scene->alive(s) ? m_scene->survival(s) : nullptr;
    bool may_sprint = sprint && moving && !stunned(e);
    if (may_sprint && m_scene && m_scene->alive(survivor(e)) && m_scene->survival(survivor(e))->weight_on && encumbrance(e) > m_scene->survival(survivor(e))->sprint_load) may_sprint = false;
    if (sv && sv->stamina_on) {
        State& st = state(s);
        const float need = st.sprinted ? 0.0f : sv->sprint_min;
        if (may_sprint && (sv->stamina <= 0.0f || sv->stamina < need)) may_sprint = false;
        if (may_sprint) {
            auto gi = m_ground.find(e.id());
            const float ground_cost = gi == m_ground.end() ? 1.0f : gi->second.stamina;
            sv->stamina = std::max(0.0f, sv->stamina - sv->sprint_cost * ground_cost * (1.0f + encumbrance(s) * sv->load_stamina) * std::max(0.0f, dt));
            st.stamina_used_at = m_now;
        }
        if (out.can_jump && sv->stamina < sv->jump_cost * 0.5f) out.can_jump = false;
        st.sprinted = may_sprint;
    }
    if (sv) state(s).sprinting = may_sprint;
    out.sprinting = may_sprint;
    out.speed = std::max(0.0f, (may_sprint ? sprint_speed : walk_speed) * scale - ground_minus(e));
    return out;
}

void Gameplay::jumped(Entity e) {
    const Entity s = survivor(e);
    if (!m_scene || !m_scene->alive(s)) return;
    Survival& sv = *m_scene->survival(s);
    if (sv.stamina_on) {
        sv.stamina = std::max(0.0f, sv.stamina - sv.jump_cost * (1.0f + encumbrance(s) * sv.load_stamina));
        state(s).stamina_used_at = m_now;
    }
    if (sv.hunger_on) sv.exhaustion += sv.jump_exhaustion;
}

bool Gameplay::underwater(Entity e) const {
    if (!m_scene || !m_physics || !m_scene->alive(e)) return false;
    const Entity s = survivor(e);
    const float head = m_scene->alive(s) ? m_scene->survival(s)->head_height : 1.6f;
    return m_physics->fluid_depth(position(m_scene->alive(s) ? s : e) + Vec3{0.0f, head, 0.0f}) > 0.0f;
}

void Gameplay::tick_damage(double now, float dt) {
    for (std::size_t i = 0; i < m_dots.size();) {
        Dot& dot = m_dots[i];
        if (!m_scene->alive(dot.target) || dead(dot.target) || (dot.until >= 0.0 && now >= dot.until)) {
            m_dots.erase(m_dots.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        const Damage d = dot.damage;
        const Entity target = dot.target;
        bool pulsed = false;
        if (d.interval > 0.0f) {
            int guard = 0;
            while (now >= m_dots[i].next_pulse && guard++ < 64) {
                m_dots[i].next_pulse += d.interval;
                pulsed = true;
                hurt(target, d, d.amount);
            }
        } else if (dt > 0.0f) {
            hurt(target, d, d.amount * dt);
        }
        Dot& cur = m_dots[i];
        if (length(d.knockback) > 0.0f) {
            if (d.push == PushRepeat::continuous) {
                push(target, d.knockback * dt, 0.0f);
            } else if (d.push == PushRepeat::once) {
                if (!cur.pushed) push(target, d.knockback, d.stagger);
                cur.pushed = true;
            } else if (d.interval > 0.0f ? pulsed : now >= cur.next_push) {
                push(target, d.knockback, d.stagger);
                cur.next_push = now + 1.0;
            }
        }
        ++i;
    }
}

void Gameplay::tick_effects(double now, float dt) {
    for (std::size_t i = 0; i < m_effects.size();) {
        Active a = m_effects[i];
        const EffectDef* def = effect_def(a.name);
        if (!m_scene->alive(a.target) || !def || (a.until >= 0.0 && now >= a.until) || dead(a.target)) {
            if (m_scene->alive(a.fx)) m_scene->destroy(a.fx);
            m_effects.erase(m_effects.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        const float k = a.strength * static_cast<float>(a.stacks);
        Damage d;
        d.type = def->damage_type;
        d.attacker = a.source;
        if (def->damage != 0.0f) {
            if (def->interval > 0.0f) {
                int guard = 0;
                while (now >= m_effects[i].next_pulse && guard++ < 64) {
                    m_effects[i].next_pulse += def->interval;
                    hurt(a.target, d, def->damage * k);
                }
            } else {
                hurt(a.target, d, def->damage * k * dt);
            }
        }
        if (def->heal != 0.0f) heal(a.target, def->heal * k * dt);
        ++i;
    }
}

void Gameplay::tick_survival(Entity e, Survival& sv, double now, float dt) {
    State& st = state(e);
    if (dead(e)) return;
    Damage d;
    if (sv.stamina_on && !st.sprinting && now - st.stamina_used_at >= sv.stamina_delay)
        sv.stamina = std::min(sv.max_stamina, sv.stamina + sv.stamina_regen * effect_product(e, &EffectDef::stamina_regen) * dt);
    if (sv.hunger_on) {
        sv.exhaustion += (sv.idle_exhaustion + (st.sprinting ? sv.sprint_exhaustion * (1.0f + encumbrance(e) * sv.load_stamina) : 0.0f)) * effect_product(e, &EffectDef::hunger_rate) * dt;
        while (sv.exhaustion >= 4.0f) {
            sv.exhaustion -= 4.0f;
            if (sv.saturation > 0.0f) sv.saturation = std::max(0.0f, sv.saturation - 1.0f);
            else sv.hunger = std::max(0.0f, sv.hunger - 1.0f);
        }
        sv.saturation = std::min(sv.saturation, sv.hunger);
        const Entity h = holder(e);
        if (sv.hunger >= sv.fed_level && m_scene->alive(h) && health(h) < max_health(h) && sv.fed_heal > 0.0f) {
            const float before = health(h);
            heal(h, sv.fed_heal * dt);
            sv.exhaustion += sv.heal_exhaustion * (health(h) - before);
        }
        if (sv.hunger <= 0.0f && sv.starve_damage > 0.0f) {
            d.type = "starve";
            hurt(e, d, sv.starve_damage * dt);
        }
    }
    if (sv.thirst_on) {
        sv.thirst = std::max(0.0f, sv.thirst - sv.thirst_rate * (st.sprinting ? sv.sprint_thirst : 1.0f) * dt);
        if (sv.thirst <= 0.0f && sv.dehydrate_damage > 0.0f) {
            d.type = "thirst";
            hurt(e, d, sv.dehydrate_damage * dt);
        }
    }
    if (sv.breath_on) {
        if (underwater(e)) {
            sv.breath = std::max(0.0f, sv.breath - dt);
            if (sv.breath <= 0.0f && sv.drown_damage > 0.0f) {
                d.type = "drown";
                hurt(e, d, sv.drown_damage * dt);
            }
        } else {
            sv.breath = std::min(sv.max_breath, sv.breath + sv.breath_recover * dt);
        }
    }
    if (sv.temperature_on) {
        const ClimateSample air = climate_at(position(e) + Vec3{0.0f, sv.head_height * 0.6f, 0.0f});
        const float wind_chill = air.any ? std::min(10.0f, length(air.wind) * 0.4f) : 0.0f;
        const float felt = (air.any ? air.temperature - wind_chill : sv.ambient) + sv.insulation + worn_insulation(e) + effect_sum(e, &EffectDef::warmth);
        if (felt < sv.comfort_low) sv.temperature -= sv.exposure * (sv.comfort_low - felt) * dt;
        else if (felt > sv.comfort_high) sv.temperature += sv.exposure * (felt - sv.comfort_high) * dt;
        else sv.temperature += (37.0f - sv.temperature) * std::min(1.0f, 0.1f * dt);
        sv.temperature = std::clamp(sv.temperature, 20.0f, 45.0f);
        if (sv.temperature < 35.0f && sv.cold_damage > 0.0f) {
            d.type = "cold";
            hurt(e, d, sv.cold_damage * (35.0f - sv.temperature) * dt);
        } else if (sv.temperature > 40.0f && sv.heat_damage > 0.0f) {
            d.type = "fire";
            hurt(e, d, sv.heat_damage * (sv.temperature - 40.0f) * dt);
        }
    }
    if (sv.mana_on) sv.mana = std::clamp(sv.mana + sv.mana_regen * dt, 0.0f, sv.max_mana);
    st.sprinting = false;
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
    std::vector<Entity> pushed, harmed;
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
                const Vec3 dir = dist > 1e-4f ? d * (1.0f / dist) : Vec3{0.0f, 1.0f, 0.0f};
                const float pressure = force * 0.01f * fall;
                const float area = std::max(0.01f, facing_area(owner, dir));
                push(owner, dir * (pressure * area), -1.0f);
                const Vec3 v = m_physics->velocity(owner);
                const float cap = 40.0f;
                if (length(v) > cap) m_physics->set_velocity(owner, normalize(v) * cap);
            }
        }
        const Entity h = holder(hit);
        if (m_scene->alive(h) && h != attacker && std::find(harmed.begin(), harmed.end(), h) == harmed.end()) {
            harmed.push_back(h);
            if (dmg > 0.0f) {
                Damage blast;
                blast.type = "explosion";
                blast.attacker = attacker;
                hurt(h, blast, dmg * fall);
            }
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
    const Vec3 down{0.0f, -1.0f, 0.0f};
    auto passes = [&](Entity hit, Vec3 normal) {
        Entity owner = m_scene->body_owner(hit);
        if (!m_scene->alive(owner)) owner = hit;
        const Body* b = m_scene->body(owner);
        const bool moving = b && b->type == BodyType::dynamic;
        const bool own = m_scene->alive(skip) && (owner == skip || holder(hit) == skip);
        return own || moving || normal.y < 0.6f;
    };
    float jumped = 0.0f;
    for (int tries = 0; tries < 16 && left > 0.0f; ++tries) {
        Vec3 point, normal;
        float d = -1.0f;
        const Entity hit = m_physics->raycast(from, down, left, &point, &normal, &d);
        if (!m_scene->alive(hit) || d < 0.0f) return Entity{};
        if (passes(hit, normal)) {
            jumped = d < 0.02f ? 0.2f : 0.01f;
            from = point + Vec3{0.0f, -jumped, 0.0f};
            left -= d + jumped;
            continue;
        }
        if (d < 0.02f && jumped > 0.02f) {
            float lo = point.y, hi = point.y + jumped;
            for (int i = 0; i < 10; ++i) {
                const float mid = (lo + hi) * 0.5f;
                Vec3 p2, n2;
                float d2 = -1.0f;
                const Entity h2 = m_physics->raycast(Vec3{point.x, mid, point.z}, down, 0.02f, &p2, &n2, &d2);
                if (m_scene->alive(h2) && d2 >= 0.0f && !passes(h2, n2)) lo = mid;
                else hi = mid;
            }
            point.y = lo;
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
        if (!st.dead && hp->max_shield > 0.0f && hp->shield_regen != 0.0f && now - st.hurt_at >= hp->shield_delay)
            hp->shield = std::clamp(hp->shield + hp->shield_regen * fdt, 0.0f, hp->max_shield);
        if (!st.dead && hp->regen != 0.0f && now - st.hurt_at >= hp->regen_delay) {
            hp->value = std::clamp(hp->value + hp->regen * fdt, 0.0f, hp->max);
            if (hp->value <= 0.0f) die(e, Entity{});
        }
        if (hp->impact_damage > 0.0f && m_physics && m_physics->has_body(e)) {
            const Vec3 v = m_physics->velocity(e);
            if (st.have_velocity) {
                const float dv = length(v - st.velocity);
                if (dv > 7.0f) {
                    Damage hit;
                    hit.type = st.velocity.y < -0.5f * dv ? "fall" : "physical";
                    hurt(e, hit, hp->impact_damage * (dv - 7.0f) * (dv - 7.0f));
                }
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
            stop_damage(e, std::string());
            clear_effects(e);
        }
    }
    for (Entity e : m_scene->entities())
        if (Survival* sv = m_scene->survival(e)) tick_survival(e, *sv, now, fdt);
    tick_damage(now, fdt);
    tick_effects(now, fdt);
    tick_items(now, fdt);
    tick_climate(now, fdt);
    for (std::size_t i = 0; i < m_fires.size();) {
        Fire f = m_fires[i];
        if (!m_scene->alive(f.target) || now >= f.until) {
            if (m_scene->alive(f.effect)) m_scene->destroy(f.effect);
            m_fires.erase(m_fires.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        const Entity h = holder(f.target);
        if (m_scene->alive(h))
            if (const Health* hp = m_scene->health(h)) {
                Damage burn;
                burn.type = "fire";
                hurt(h, burn, hp->burn_damage * fdt);
            }
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
