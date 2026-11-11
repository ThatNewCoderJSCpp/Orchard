#include <orchard/ai/steering.hpp>

#include <algorithm>
#include <cmath>

namespace orchard {
namespace ai {

Vec3 limit(Vec3 v, float max_length) {
    const float l = length(v);
    return l > max_length && l > 1e-9f ? v * (max_length / l) : v;
}

Vec3 flatten(Vec3 v) {
    return Vec3{v.x, 0.0f, v.z};
}

Vec3 seek(const Mover& m, Vec3 target) {
    const Vec3 d = target - m.position;
    const float l = length(d);
    if (l < 1e-6f) return Vec3{} - m.velocity;
    return d * (m.max_speed / l) - m.velocity;
}

Vec3 flee(const Mover& m, Vec3 threat, float panic) {
    const Vec3 d = m.position - threat;
    const float l = length(d);
    if (l > panic) return Vec3{};
    if (l < 1e-6f) return Vec3{m.max_speed, 0.0f, 0.0f} - m.velocity;
    return d * (m.max_speed / l) - m.velocity;
}

Vec3 arrive(const Mover& m, Vec3 target, float slow_radius, float stop_radius) {
    const Vec3 d = target - m.position;
    const float l = length(d);
    if (l <= stop_radius) return Vec3{} - m.velocity;
    const float speed = l < slow_radius ? m.max_speed * (l / std::max(slow_radius, 1e-3f)) : m.max_speed;
    return d * (speed / l) - m.velocity;
}

Vec3 pursue(const Mover& m, Vec3 target, Vec3 tv, float max_prediction) {
    const float l = length(target - m.position);
    const float speed = std::max(0.1f, length(m.velocity) + m.max_speed * 0.5f);
    const float t = std::min(max_prediction, l / speed);
    return seek(m, target + tv * t);
}

Vec3 evade(const Mover& m, Vec3 threat, Vec3 tv, float max_prediction) {
    const float l = length(threat - m.position);
    const float t = std::min(max_prediction, l / std::max(0.1f, m.max_speed));
    return flee(m, threat + tv * t);
}

Vec3 wander(const Mover& m, float& angle, Random& random, float radius, float distance, float jitter, bool flat) {
    angle += static_cast<float>(random.range(-jitter, jitter));
    Vec3 fwd = length(m.velocity) > 1e-3f ? normalize(m.velocity) : Vec3{0.0f, 0.0f, 1.0f};
    if (flat) fwd = length(flatten(fwd)) > 1e-3f ? normalize(flatten(fwd)) : Vec3{0.0f, 0.0f, 1.0f};
    const Vec3 center = m.position + fwd * distance;
    const Vec3 offset{std::cos(angle) * radius, 0.0f, std::sin(angle) * radius};
    return seek(m, center + offset);
}

Vec3 separation(const Mover& m, const std::vector<Vec3>& neighbors, float range) {
    Vec3 force{};
    for (const Vec3& n : neighbors) {
        const Vec3 d = m.position - n;
        const float l = length(d);
        if (l > 1e-4f && l < range) force += d * (1.0f / (l * l));
    }
    return force * m.max_speed;
}

Vec3 alignment(const Mover& m, const std::vector<Vec3>& vels) {
    if (vels.empty()) return Vec3{};
    Vec3 avg{};
    for (const Vec3& v : vels) avg += v;
    avg = avg * (1.0f / static_cast<float>(vels.size()));
    return avg - m.velocity;
}

Vec3 cohesion(const Mover& m, const std::vector<Vec3>& neighbors) {
    if (neighbors.empty()) return Vec3{};
    Vec3 c{};
    for (const Vec3& n : neighbors) c += n;
    return seek(m, c * (1.0f / static_cast<float>(neighbors.size())));
}

Vec3 flock(const Mover& m, const std::vector<Mover>& neighbors, float range, float sw, float aw, float cw) {
    std::vector<Vec3> pos, vel;
    for (const Mover& n : neighbors) {
        if (length(n.position - m.position) > range) continue;
        pos.push_back(n.position);
        vel.push_back(n.velocity);
    }
    return separation(m, pos, range * 0.5f) * sw + alignment(m, vel) * aw + cohesion(m, pos) * cw;
}

Vec3 avoid_obstacles(const Mover& m, const std::function<bool(Vec3, Vec3, float, Vec3&, float&)>& ray, float look_ahead, int feelers, bool flat) {
    if (!ray) return Vec3{};
    Vec3 fwd = length(m.velocity) > 1e-3f ? normalize(m.velocity) : Vec3{};
    if (flat) fwd = length(flatten(fwd)) > 1e-3f ? normalize(flatten(fwd)) : Vec3{};
    if (length(fwd) < 0.5f) return Vec3{};
    const float speed_scale = 0.5f + length(m.velocity) / std::max(0.1f, m.max_speed);
    Vec3 force{};
    for (int i = 0; i < std::max(1, feelers); ++i) {
        const float a = feelers <= 1 ? 0.0f : (static_cast<float>(i) / static_cast<float>(feelers - 1) - 0.5f) * 1.2f;
        const Vec3 dir = normalize(Vec3{fwd.x * std::cos(a) - fwd.z * std::sin(a), fwd.y, fwd.x * std::sin(a) + fwd.z * std::cos(a)});
        Vec3 n{};
        float hit = 0.0f;
        const float reach = look_ahead * speed_scale * (i == feelers / 2 ? 1.0f : 0.7f);
        if (ray(m.position, dir, reach, n, hit)) {
            const float push = (reach - hit) / std::max(reach, 1e-3f);
            Vec3 nn = flat ? flatten(n) : n;
            if (length(nn) < 1e-3f) nn = Vec3{} - dir;
            force += normalize(nn) * (m.max_speed * 2.0f * push);
        }
    }
    return force;
}

Vec3 follow_path(const Mover& m, const std::vector<Vec3>& path, std::size_t& index, float reach, bool loop) {
    if (path.empty()) return Vec3{};
    if (index >= path.size()) index = loop ? 0 : path.size() - 1;
    if (length(path[index] - m.position) < reach) {
        if (index + 1 < path.size()) ++index;
        else if (loop) index = 0;
    }
    return index + 1 == path.size() && !loop ? arrive(m, path[index]) : seek(m, path[index]);
}

Vec3 interpose(const Mover& m, Vec3 a, Vec3 b) {
    return arrive(m, (a + b) * 0.5f);
}

Vec3 hide(const Mover& m, Vec3 threat, const std::vector<Vec3>& obstacles, float r) {
    float best = 1e30f;
    Vec3 spot = m.position;
    for (const Vec3& o : obstacles) {
        Vec3 away = o - threat;
        const float l = length(away);
        if (l < 1e-3f) continue;
        const Vec3 p = o + away * ((r + m.radius + 0.5f) / l);
        const float d = length(p - m.position);
        if (d < best) {
            best = d;
            spot = p;
        }
    }
    return best < 1e29f ? arrive(m, spot) : flee(m, threat);
}

Vec3 offset_pursuit(const Mover& m, Vec3 leader, Vec3 forward, Vec3 offset) {
    Vec3 f = length(flatten(forward)) > 1e-3f ? normalize(flatten(forward)) : Vec3{0.0f, 0.0f, 1.0f};
    const Vec3 right{f.z, 0.0f, -f.x};
    return arrive(m, leader + right * offset.x + Vec3{0.0f, offset.y, 0.0f} + f * offset.z, 1.5f);
}

Vec3 avoid_neighbors(const Mover& m, const std::vector<Mover>& others, float horizon) {
    Vec3 force{};
    for (const Mover& o : others) {
        const Vec3 rp = o.position - m.position;
        const Vec3 rv = o.velocity - m.velocity;
        const float rv2 = dot(rv, rv);
        if (rv2 < 1e-6f) continue;
        const float t = -dot(rp, rv) / rv2;
        if (t < 0.0f || t > horizon) continue;
        const Vec3 closest = rp + rv * t;
        const float dist = length(closest);
        const float r = m.radius + o.radius + 0.1f;
        if (dist >= r) continue;
        Vec3 away = Vec3{} - closest;
        if (length(away) < 1e-4f) away = Vec3{-rp.z, 0.0f, rp.x};
        force += normalize(away) * (m.max_speed * (1.0f - t / horizon));
    }
    return force;
}

} // namespace ai
} // namespace orchard
