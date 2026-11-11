#include <orchard/ai/tactics.hpp>

#include <algorithm>
#include <cmath>

namespace orchard {
namespace ai {

bool ballistic_velocity(Vec3 from, Vec3 to, float speed, float gravity, bool high_arc, Vec3& out) {
    const Vec3 d = to - from;
    const float dx = std::sqrt(d.x * d.x + d.z * d.z);
    const float dy = d.y;
    const float g = std::fabs(gravity);
    if (g < 1e-6f) {
        const float l = length(d);
        if (l < 1e-6f) return false;
        out = d * (speed / l);
        return true;
    }
    if (dx < 1e-4f) {
        out = Vec3{0.0f, dy >= 0.0f ? speed : -speed, 0.0f};
        return speed * speed >= 2.0f * g * std::max(0.0f, dy);
    }
    const float v2 = speed * speed;
    const float disc = v2 * v2 - g * (g * dx * dx + 2.0f * dy * v2);
    if (disc < 0.0f) return false;
    const float root = std::sqrt(disc);
    const float angle = std::atan((v2 + (high_arc ? root : -root)) / (g * dx));
    const Vec3 flat = Vec3{d.x / dx, 0.0f, d.z / dx};
    out = flat * (speed * std::cos(angle)) + Vec3{0.0f, speed * std::sin(angle), 0.0f};
    return true;
}

Vec3 lob_velocity(Vec3 from, Vec3 to, float apex, float gravity) {
    const float g = std::max(1e-3f, std::fabs(gravity));
    const float top = std::max({from.y, to.y}) + std::max(0.1f, apex);
    const float up = std::sqrt(2.0f * g * (top - from.y));
    const float t_up = up / g;
    const float t_down = std::sqrt(2.0f * std::max(0.0f, top - to.y) / g);
    const float t = std::max(0.05f, t_up + t_down);
    return Vec3{(to.x - from.x) / t, up, (to.z - from.z) / t};
}

float ballistic_range(float speed, float gravity, float height) {
    const float g = std::max(1e-3f, std::fabs(gravity));
    const float v = speed * 0.70710678f;
    const float t = (v + std::sqrt(v * v + 2.0f * g * std::max(0.0f, height))) / g;
    return v * t;
}

bool intercept(Vec3 shooter, Vec3 target, Vec3 tv, float speed, Vec3& aim, float* time) {
    const Vec3 r = target - shooter;
    const float a = dot(tv, tv) - speed * speed;
    const float b = 2.0f * dot(r, tv);
    const float c = dot(r, r);
    float t = -1.0f;
    if (std::fabs(a) < 1e-6f) {
        if (std::fabs(b) > 1e-6f) t = -c / b;
    } else {
        const float disc = b * b - 4.0f * a * c;
        if (disc >= 0.0f) {
            const float s = std::sqrt(disc);
            const float t1 = (-b - s) / (2.0f * a), t2 = (-b + s) / (2.0f * a);
            t = std::min(t1, t2) > 0.0f ? std::min(t1, t2) : std::max(t1, t2);
        }
    }
    if (t <= 0.0f) {
        aim = target;
        return false;
    }
    aim = target + tv * t;
    if (time) *time = t;
    return true;
}

Vec3 aim_with_error(Vec3 direction, float accuracy, Random& random, float max_spread) {
    const float spread = max_spread * (1.0f - std::clamp(accuracy, 0.0f, 1.0f));
    if (spread <= 1e-3f) return normalize(direction);
    const float cone = std::fabs(static_cast<float>(random.gaussian(0.0, spread * 0.5)));
    return random.direction_in_cone(direction, std::min(cone, spread * 2.0f));
}

float hit_chance(float distance, float accuracy, float target_size, float max_spread) {
    const float spread = radians(max_spread * (1.0f - std::clamp(accuracy, 0.0f, 1.0f)));
    if (spread <= 1e-5f) return 1.0f;
    const float miss_radius = std::tan(spread) * std::max(0.01f, distance);
    return std::clamp(target_size / std::max(1e-4f, miss_radius), 0.0f, 1.0f);
}

void Director::tick(float dt) {
    m_phase_time += dt;
    m_intensity += m_pending_damage * damage_weight;
    m_pending_damage = 0.0f;
    if (m_nearby > 0) m_intensity += nearby_weight * static_cast<float>(m_nearby) * dt * (m_closest < 5.0f ? 2.0f : 1.0f);
    if (m_seen) m_intensity += sight_weight * dt;
    if (m_phase == Phase::build_up) m_intensity += build_up_rate * dt * 0.2f;
    m_intensity = std::clamp(m_intensity - decay * dt * (m_phase == Phase::relax ? 2.0f : 0.5f), 0.0f, 1.0f);
    switch (m_phase) {
    case Phase::build_up:
        if (m_intensity >= peak_at) {
            m_phase = Phase::peak;
            m_phase_time = 0.0f;
        }
        break;
    case Phase::peak:
        if (m_phase_time >= peak_time) {
            m_phase = Phase::relax;
            m_phase_time = 0.0f;
        }
        break;
    case Phase::relax:
        if (m_phase_time >= relax_time && m_intensity < peak_at * 0.4f) {
            m_phase = Phase::build_up;
            m_phase_time = 0.0f;
        }
        break;
    }
    const float near = m_hunter_distance < 25.0f ? (1.0f - m_hunter_distance / 25.0f) : 0.0f;
    m_menace += (m_seen ? menace_rise * 1.5f : 0.0f) * dt + near * menace_rise * dt;
    if (!m_seen && near <= 0.0f) m_menace -= menace_fall * dt;
    m_menace = std::clamp(m_menace, 0.0f, menace_limit);
    if (m_menace >= menace_limit * 0.95f) m_backoff = true;
    else if (m_menace <= menace_limit * 0.25f) m_backoff = false;
    m_nearby = 0;
    m_closest = 1e9f;
    m_seen = false;
    m_hunter_distance = 1e9f;
}

void Director::report_damage(float amount) {
    m_pending_damage += std::max(0.0f, amount);
}

void Director::report_nearby(int threats, float closest) {
    m_nearby += std::max(0, threats);
    m_closest = std::min(m_closest, closest);
}

void Director::report_seen(bool visible, float distance) {
    m_seen = m_seen || visible;
    m_hunter_distance = std::min(m_hunter_distance, distance);
}

void Director::note(const std::string& name, float amount) {
    m_habits[name] += amount;
}

float Director::habit(const std::string& name) const {
    const auto it = m_habits.find(name);
    return it == m_habits.end() ? 0.0f : it->second;
}

const char* Director::phase_name() const {
    switch (m_phase) {
    case Phase::build_up: return "build-up";
    case Phase::peak: return "peak";
    case Phase::relax: return "relax";
    }
    return "build-up";
}

bool Director::wants_spawn(int alive) const {
    if (alive >= max_spawns) return false;
    if (m_phase == Phase::relax) return false;
    if (m_phase == Phase::peak) return true;
    return m_intensity < peak_at;
}

float Director::hint_radius() const {
    return 4.0f + 16.0f * std::clamp(m_menace, 0.0f, 1.0f);
}

void Director::reset() {
    m_phase = Phase::build_up;
    m_intensity = 0.0f;
    m_menace = 0.0f;
    m_phase_time = 0.0f;
    m_backoff = false;
    m_habits.clear();
}

} // namespace ai
} // namespace orchard
