#include <orchard/gameplay.hpp>

#include <orchard/effects.hpp>
#include <orchard/scripting.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace orchard {

float Gameplay::climate_intensity(Entity zone) const {
    if (!m_scene || !m_scene->alive(zone)) return 0.0f;
    const Climate* c = m_scene->climate(zone);
    if (!c || !c->active) return 0.0f;
    const auto it = m_zones.find(zone.id());
    const double started = it == m_zones.end() ? 0.0 : it->second.started;
    double t = m_now - started - c->delay;
    if (t < 0.0) return 0.0f;
    float k = 1.0f;
    if (c->fade_in > 0.0f) k = std::min(1.0f, static_cast<float>(t) / c->fade_in);
    if (c->duration >= 0.0f) {
        const double end = c->fade_in + c->duration;
        if (t > end) k *= c->fade_out > 0.0f ? std::max(0.0f, 1.0f - static_cast<float>(t - end) / c->fade_out) : 0.0f;
    }
    if (it != m_zones.end() && it->second.stopping >= 0.0) {
        const float since = static_cast<float>(m_now - it->second.stopping);
        k *= it->second.stop_fade > 0.0f ? std::max(0.0f, 1.0f - since / it->second.stop_fade) : 0.0f;
    }
    return std::clamp(k, 0.0f, 1.0f);
}

float Gameplay::climate_weight(Entity zone, Vec3 point) const {
    const float k = climate_intensity(zone);
    if (k <= 0.0f) return 0.0f;
    const Climate& c = *m_scene->climate(zone);
    if (c.global) return k;
    Mat4 inv;
    if (!invert(m_scene->world_matrix(zone), inv)) return 0.0f;
    const Vec3 local = mul_point(inv, point);
    float outside = 0.0f;
    if (c.shape == "sphere") {
        outside = length(local) - c.size.x * 0.5f;
    } else {
        const bool flat = m_scene->dimension() == Dimension::two_d;
        const float dx = std::max(0.0f, std::fabs(local.x) - c.size.x * 0.5f);
        const float dy = std::max(0.0f, std::fabs(local.y) - c.size.y * 0.5f);
        const float dz = flat ? 0.0f : std::max(0.0f, std::fabs(local.z) - c.size.z * 0.5f);
        outside = std::sqrt(dx * dx + dy * dy + dz * dz);
    }
    if (outside <= 0.0f) return k;
    if (c.blend <= 0.0f || outside >= c.blend) return 0.0f;
    return k * (1.0f - outside / c.blend);
}

ClimateSample Gameplay::climate_at(Vec3 point) const {
    ClimateSample out;
    if (!m_scene) return out;
    std::vector<std::pair<int, Entity>> zones;
    for (Entity e : m_scene->entities())
        if (const Climate* c = m_scene->climate(e)) zones.emplace_back(c->priority, e);
    std::stable_sort(zones.begin(), zones.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    const bool flat = m_scene->dimension() == Dimension::two_d;
    for (const auto& [prio, e] : zones) {
        const float w = climate_weight(e, point);
        if (w <= 0.0f) continue;
        const Climate& c = *m_scene->climate(e);
        out.any = true;
        if (c.relative) out.temperature += c.temperature * w;
        else out.temperature = out.temperature + (c.temperature - out.temperature) * w;
        Vec3 dir = length(c.wind_dir) > 1e-4f ? normalize(c.wind_dir) : Vec3{1.0f, 0.0f, 0.0f};
        if (flat) dir = Vec3{dir.x >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f};
        float gust = 0.0f;
        if (c.gust > 0.0f) {
            const auto it = m_zones.find(e.id());
            const float ph = it == m_zones.end() ? 0.0f : it->second.gust_phase.x;
            const float t = static_cast<float>(m_now) * c.gust_rate * 6.2831853f + ph;
            gust = c.gust * (0.5f + 0.5f * std::sin(t) * std::sin(t * 0.37f + 1.3f));
        }
        out.wind = out.wind + dir * ((c.wind_speed + gust) * w);
        out.rain = std::max(out.rain, c.rain * w);
        out.snow = std::max(out.snow, c.snow * w);
        out.fog = std::max(out.fog, c.fog * w);
        out.humidity = out.humidity + (c.humidity - out.humidity) * w;
        out.lightning = std::max(out.lightning, c.lightning * w);
    }
    return out;
}

bool Gameplay::sheltered(Vec3 point) const {
    if (!m_physics) return false;
    Vec3 p;
    float d = 0.0f;
    return m_physics->raycast(point + Vec3{0.0f, 0.3f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f}, 60.0f, &p, nullptr, &d).valid();
}

Entity Gameplay::start_weather(const std::string& preset, float duration, float fade_in, float fade_out, Vec3 at, float radius) {
    if (!m_scene) return Entity{};
    const Entity e = m_scene->create("Weather: " + preset);
    Climate& c = m_scene->add_climate(e);
    if (!apply_climate_preset(c, preset)) apply_climate_preset(c, "clear");
    c.global = radius <= 0.0f;
    c.shape = "sphere";
    c.size = Vec3{radius * 2.0f, radius * 2.0f, radius * 2.0f};
    c.duration = duration;
    c.fade_in = std::max(0.0f, fade_in);
    c.fade_out = std::max(0.0f, fade_out);
    c.priority = 10;
    if (Transform* t = m_scene->transform(e)) t->position = at;
    Zone& z = m_zones[e.id()];
    z.started = m_now;
    z.temporary = true;
    z.gust_phase = Vec3{static_cast<float>(random(0.0, 6.28)), 0.0f, 0.0f};
    return e;
}

bool Gameplay::stop_weather(Entity zone, float fade_out) {
    if (!m_scene || !m_scene->alive(zone) || !m_scene->climate(zone)) return false;
    Zone& z = m_zones[zone.id()];
    if (z.stopping >= 0.0) return true;
    z.stopping = m_now;
    z.stop_fade = std::max(0.0f, fade_out);
    return true;
}

int Gameplay::stop_all_weather(float fade_out) {
    int n = 0;
    if (!m_scene) return 0;
    for (Entity e : m_scene->entities())
        if (m_scene->climate(e) && stop_weather(e, fade_out)) ++n;
    return n;
}

bool Gameplay::set_climate(Entity zone, const std::string& key, const std::string& value) {
    if (!m_scene || !m_scene->alive(zone)) return false;
    Climate& c = m_scene->climate(zone) ? *m_scene->climate(zone) : m_scene->add_climate(zone);
    if (key == "preset") return apply_climate_preset(c, value);
    if (float* f = climate_value(c, key)) {
        *f = static_cast<float>(std::atof(value.c_str()));
        return true;
    }
    const bool on = value != "0" && value != "false" && !value.empty();
    if (key == "global") c.global = on;
    else if (key == "relative") c.relative = on;
    else if (key == "loop") c.loop = on;
    else if (key == "active") c.active = on;
    else if (key == "visuals") c.visuals = on;
    else if (key == "shape") c.shape = value;
    else if (key == "after") c.after = value;
    else if (key == "priority") c.priority = std::atoi(value.c_str());
    else if (key == "restart") m_zones[zone.id()].started = m_now;
    else return false;
    return true;
}

void Gameplay::tick_climate(double now, float dt) {
    if (!m_scene) return;
    std::vector<Entity> zones;
    for (Entity e : m_scene->entities())
        if (m_scene->climate(e)) zones.push_back(e);
    if (zones.empty()) return;
    for (Entity e : zones) {
        Climate& c = *m_scene->climate(e);
        Zone& z = m_zones[e.id()];
        if (z.gust_phase.x == 0.0f) z.gust_phase.x = static_cast<float>(random(0.1, 6.28));
        const double t = now - z.started - c.delay;
        const bool ended = (c.duration >= 0.0f && t > c.fade_in + c.duration + c.fade_out) || (z.stopping >= 0.0 && now - z.stopping >= z.stop_fade);
        if (ended) {
            if (z.stopping < 0.0 && !c.after.empty()) {
                apply_climate_preset(c, c.after);
                c.after.clear();
                z.started = now;
                c.delay = 0.0f;
                continue;
            }
            if (z.stopping < 0.0 && c.loop) {
                z.started = now;
                continue;
            }
            if (m_scene->alive(z.rain_fx)) m_scene->destroy(z.rain_fx);
            z.rain_fx = Entity{};
            if (z.temporary) {
                m_doomed.push_back(e);
                m_zones.erase(e.id());
            } else {
                c.active = false;
                z.stopping = -1.0;
            }
            continue;
        }
        const float k = climate_intensity(e);
        if (c.lightning > 0.0f && k > 0.0f && now >= z.next_strike) {
            z.next_strike = now + 60.0 / std::max(0.1f, c.lightning * k) * random(0.4, 1.6);
            if (z.next_strike - now < 120.0 && t > 1.0) {
                Vec3 centre = position(e);
                const float r = c.global ? 60.0f : std::max(2.0f, c.size.x * 0.5f);
                const Vec3 at = centre + Vec3{static_cast<float>(random(-r, r)), 80.0f, m_scene->dimension() == Dimension::two_d ? 0.0f : static_cast<float>(random(-r, r))};
                Vec3 ground = at - Vec3{0.0f, 160.0f, 0.0f};
                if (m_physics) {
                    Vec3 p;
                    float d = 0.0f;
                    if (m_physics->raycast(at, Vec3{0.0f, -1.0f, 0.0f}, 200.0f, &p, nullptr, &d).valid()) ground = p;
                }
                const Entity fx = effect("electric", ground, 2.0f);
                if (m_scene->alive(fx)) m_temporary.emplace_back(fx, now + 1.0);
                if (m_physics)
                    for (Entity hit : m_physics->overlap_sphere(ground, 3.0f)) {
                        const Entity h = holder(hit);
                        if (!m_scene->alive(h)) continue;
                        Damage d;
                        d.amount = 80.0f;
                        d.type = "shock";
                        apply_damage(h, d);
                        apply_effect(h, "shocked", 3.0f);
                        if (m_scene->health(h)->flammable) ignite(h, 8.0f);
                    }
            }
        }
    }
    const Entity cam = m_scene->active_camera();
    if (m_scene->alive(cam)) {
        const Vec3 at = position(cam);
        const ClimateSample here = climate_at(at);
        Entity owner;
        for (Entity e : zones)
            if (climate_weight(e, at) > 0.0f && m_scene->climate(e)->visuals) owner = e;
        for (Entity e : zones) {
            Zone& z = m_zones[e.id()];
            const bool want = e == owner && (here.rain > 0.02f || here.snow > 0.02f);
            if (!want && m_scene->alive(z.rain_fx)) {
                m_scene->destroy(z.rain_fx);
                z.rain_fx = Entity{};
            }
            if (!want) continue;
            const bool snow = here.snow > here.rain;
            const std::string kind = snow ? "snow" : "rain";
            if (m_scene->alive(z.rain_fx) && m_scene->name(z.rain_fx) != "Weather " + kind) {
                m_scene->destroy(z.rain_fx);
                z.rain_fx = Entity{};
            }
            if (!m_scene->alive(z.rain_fx)) {
                z.rain_fx = m_scene->create("Weather " + kind, cam);
                if (const auto em = Emitter::named(kind)) m_scene->add_emitter(z.rain_fx) = *em;
                if (Transform* t = m_scene->transform(z.rain_fx)) t->position = Vec3{0.0f, 8.0f, 0.0f};
            }
            if (Emitter* em = m_scene->emitter(z.rain_fx)) {
                const auto base = Emitter::named(kind);
                em->rate = (base ? base->rate : 200.0f) * std::max(0.05f, snow ? here.snow : here.rain) * 2.0f;
                em->wind = here.wind;
            }
        }
    }
    for (Entity e : m_scene->entities()) {
        if (!m_physics || !m_physics->has_body(e)) continue;
        const Body* b = m_scene->body(e);
        if (!b || b->type != BodyType::dynamic) continue;
        const ClimateSample s = climate_at(position(e));
        if (!s.any || length(s.wind) < 0.5f) continue;
        const Vec3 rel = s.wind - m_physics->velocity(e);
        const float speed = length(rel);
        if (speed < 0.1f) continue;
        const float area = std::clamp(facing_area(e, rel * (1.0f / speed)), 0.01f, 4.0f);
        m_physics->force(e, rel * (0.5f * 1.2f * area * speed));
    }
    for (Entity e : m_scene->entities()) {
        Survival* sv = m_scene->survival(e);
        if (!sv) continue;
        const Vec3 p = position(e) + Vec3{0.0f, sv->head_height, 0.0f};
        const ClimateSample s = climate_at(p);
        double& next = m_next_wet[e.id()];
        if (now < next) continue;
        next = now + 0.5;
        if ((s.rain > 0.05f || s.snow > 0.3f) && !sheltered(p)) {
            bool proof = false;
            if (const Entity o = inventory_owner(e); m_scene->alive(o))
                for (const auto& [slot, st] : m_scene->inventory(o)->worn)
                    if (const ItemDef* d = st.empty() ? nullptr : item_def(st.item); d && d->has("waterproof")) proof = true;
            if (!proof) {
                const Entity h = m_scene->alive(holder(e)) ? holder(e) : e;
                apply_effect(h, "wet", 30.0f, std::max(s.rain, s.snow * 0.5f));
            }
        }
    }
    static_cast<void>(dt);
}

} // namespace orchard
