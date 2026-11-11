#ifndef ORCHARD_SCRIPT_SCRIPT_API_HPP
#define ORCHARD_SCRIPT_SCRIPT_API_HPP

#include "orchard_api.h"

#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace orchard {
namespace script {

namespace detail {

inline const orchard_host_api*& host() {
    static const orchard_host_api* h = nullptr;
    return h;
}

inline orchard_entity& current() {
    static orchard_entity e = 0;
    return e;
}

inline double& dt() {
    static double d = 0.0;
    return d;
}

inline std::function<void()>& hook(int which) {
    static std::function<void()> start, tick, stop;
    return which == ORCHARD_HOOK_START ? start : which == ORCHARD_HOOK_TICK ? tick : stop;
}

inline std::string text(const char* s) {
    return s ? std::string(s) : std::string();
}

} // namespace detail

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    Vec3() = default;
    Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator-() const { return Vec3(-x, -y, -z); }
    Vec3 operator*(double s) const { return Vec3(x * s, y * s, z * s); }
    Vec3 operator/(double s) const { return Vec3(x / s, y / s, z / s); }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(double s) { x *= s; y *= s; z *= s; return *this; }
    double dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 cross(const Vec3& o) const { return Vec3(y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x); }
    double length() const { return std::sqrt(dot(*this)); }
    Vec3 normalized() const { const double l = length(); return l > 1e-12 ? *this / l : Vec3(); }
};

inline Vec3 operator*(double s, const Vec3& v) { return v * s; }

class Entity {
public:
    Entity() = default;
    explicit Entity(orchard_entity id) : m_id(id) {}

    static Entity current() { return Entity(detail::current()); }
    static Entity none() { return Entity(); }

    orchard_entity id() const { return m_id; }
    bool exists() const { return m_id != 0 && detail::host() && detail::host()->exists(m_id) != 0; }
    bool same(const Entity& other) const { return m_id == other.m_id; }

    std::string name() const { return exists() ? detail::text(detail::host()->name(m_id)) : std::string(); }
    void set_name(const std::string& name) const { if (exists()) detail::host()->set_name(m_id, name.c_str()); }
    Entity parent() const { return exists() ? Entity(detail::host()->parent(m_id)) : Entity(); }
    void destroy() const { if (exists()) detail::host()->destroy(m_id); }

    double get(int which, int axis) const {
        if (!exists()) return 0.0;
        double v[3] = { 0.0, 0.0, 0.0 };
        detail::host()->get_vec(m_id, which, v);
        return axis >= 0 && axis < 3 ? v[axis] : 0.0;
    }
    void set(int which, double x, double y, double z) const { if (exists()) detail::host()->set_vec(m_id, which, x, y, z); }
    void push(int kind, double x, double y, double z) const { if (exists()) detail::host()->push(m_id, kind, x, y, z); }
    bool has_body() const { return exists() && detail::host()->has_body(m_id) != 0; }
    bool touching(const Entity& other) const { return exists() && detail::host()->touching(m_id, other.m_id) != 0; }
    void set_layers(long long layer, long long mask) const {
        if (exists() && ORCHARD_HOST_HAS(detail::host(), set_layers)) detail::host()->set_layers(m_id, static_cast<unsigned long long>(layer), static_cast<unsigned long long>(mask));
    }

    Vec3 vec(int which) const {
        double v[3] = { 0.0, 0.0, 0.0 };
        if (exists()) detail::host()->get_vec(m_id, which, v);
        return Vec3(v[0], v[1], v[2]);
    }
    void set_vec(int which, const Vec3& v) const { set(which, v.x, v.y, v.z); }

    Vec3 position() const { return vec(ORCHARD_VEC_POSITION); }
    void set_position(const Vec3& v) const { set_vec(ORCHARD_VEC_POSITION, v); }
    Vec3 rotation() const { return vec(ORCHARD_VEC_ROTATION); }
    void set_rotation(const Vec3& degrees) const { set_vec(ORCHARD_VEC_ROTATION, degrees); }
    Vec3 scale() const { return vec(ORCHARD_VEC_SCALE); }
    void set_scale(const Vec3& v) const { set_vec(ORCHARD_VEC_SCALE, v); }
    Vec3 velocity() const { return vec(ORCHARD_VEC_VELOCITY); }
    void set_velocity(const Vec3& v) const { set_vec(ORCHARD_VEC_VELOCITY, v); }
    Vec3 spin() const { return vec(ORCHARD_VEC_SPIN); }
    void set_spin(const Vec3& degrees_per_second) const { set_vec(ORCHARD_VEC_SPIN, degrees_per_second); }
    Vec3 world_position() const { return vec(ORCHARD_VEC_WORLD_POSITION); }
    void translate(const Vec3& by) const { set_position(position() + by); }
    void rotate(const Vec3& degrees) const { set_rotation(rotation() + degrees); }
    void add_impulse(const Vec3& v) const { push(ORCHARD_PUSH_IMPULSE, v.x, v.y, v.z); }
    void add_force(const Vec3& v) const { push(ORCHARD_PUSH_FORCE, v.x, v.y, v.z); }
    void add_torque(const Vec3& v) const { push(ORCHARD_PUSH_TORQUE, v.x, v.y, v.z); }
    explicit operator bool() const { return exists(); }
    bool operator==(const Entity& other) const { return m_id == other.m_id; }
    bool operator!=(const Entity& other) const { return m_id != other.m_id; }

private:
    orchard_entity m_id = 0;
};

class World {
public:
    static void on_start(std::function<void()> fn) { detail::hook(ORCHARD_HOOK_START) = std::move(fn); }
    static void on_tick(std::function<void()> fn) { detail::hook(ORCHARD_HOOK_TICK) = std::move(fn); }
    static void on_stop(std::function<void()> fn) { detail::hook(ORCHARD_HOOK_STOP) = std::move(fn); }

    static void log(const std::string& message) { if (detail::host()) detail::host()->log(message.c_str(), 0); }
    static void warn(const std::string& message) { if (detail::host()) detail::host()->log(message.c_str(), 1); }
    static double dt() { return detail::dt(); }
    static double time() { return detail::host() ? detail::host()->time() : 0.0; }
    static bool is_2d() { return detail::host() && detail::host()->is_2d() != 0; }
    static bool key_down(const std::string& key) { return detail::host() && detail::host()->key_down(key.c_str()) != 0; }
    static Entity find(const std::string& name) { return detail::host() ? Entity(detail::host()->find(name.c_str())) : Entity(); }
    static Entity spawn(const std::string& name) { return detail::host() ? Entity(detail::host()->spawn(name.c_str(), 0)) : Entity(); }
    static Entity spawn_child(const std::string& name, const Entity& parent) { return detail::host() ? Entity(detail::host()->spawn(name.c_str(), parent.id())) : Entity(); }
    static Entity instantiate(const std::string& entity_template, double x, double y, double z) { return ORCHARD_HOST_HAS(detail::host(), instantiate) ? Entity(detail::host()->instantiate(entity_template.c_str(), x, y, z, 0)) : Entity(); }
    static Entity instantiate_child(const std::string& entity_template, double x, double y, double z, const Entity& parent) { return ORCHARD_HOST_HAS(detail::host(), instantiate) ? Entity(detail::host()->instantiate(entity_template.c_str(), x, y, z, parent.id())) : Entity(); }
    static void play_particles(const Entity& e) { if (ORCHARD_HOST_HAS(detail::host(), particles)) detail::host()->particles(e.id(), ORCHARD_PARTICLES_PLAY, 0); }
    static void stop_particles(const Entity& e) { if (ORCHARD_HOST_HAS(detail::host(), particles)) detail::host()->particles(e.id(), ORCHARD_PARTICLES_STOP, 0); }
    static void burst_particles(const Entity& e, long long count) { if (ORCHARD_HOST_HAS(detail::host(), particles)) detail::host()->particles(e.id(), ORCHARD_PARTICLES_BURST, static_cast<int>(count)); }
    static long long fluid_brush(long long brush, double x, double y, double z, double radius, double vx, double vy, double vz, long long material) { return ORCHARD_HOST_HAS(detail::host(), fluid_brush) ? detail::host()->fluid_brush(static_cast<int>(brush), x, y, z, radius, vx, vy, vz, static_cast<int>(material)) : 0; }
    static long long pour_fluid(double x, double y, double radius) { return fluid_brush(ORCHARD_FLUID_POUR, x, y, 0.0, radius, 0.0, 0.0, 0.0, 0); }
    static long long erase_fluid(double x, double y, double radius) { return fluid_brush(ORCHARD_FLUID_ERASE, x, y, 0.0, radius, 0.0, 0.0, 0.0, 0); }
    static long long push_fluid(double x, double y, double radius, double vx, double vy) { return fluid_brush(ORCHARD_FLUID_PUSH, x, y, 0.0, radius, vx, vy, 0.0, 0); }
    static long long splash_fluid(double x, double y, double radius, double strength) { return fluid_brush(ORCHARD_FLUID_EXPLODE, x, y, 0.0, radius, strength, 0.0, 0.0, 0); }
    static long long paint_sand(double x, double y, double radius, long long cell) { return fluid_brush(ORCHARD_FLUID_SAND, x, y, 0.0, radius, 0.0, 0.0, 0.0, cell); }
    static double animation(const Entity& e, long long action, const std::string& clip, double value) { return ORCHARD_HOST_HAS(detail::host(), animation) ? detail::host()->animation(e.id(), static_cast<int>(action), clip.c_str(), value) : 0.0; }
    static void play_animation(const Entity& e, const std::string& clip, double fade) { animation(e, ORCHARD_ANIM_PLAY, clip, fade); }
    static void play_animation_once(const Entity& e, const std::string& clip, double fade) { animation(e, ORCHARD_ANIM_PLAY_ONCE, clip, fade); }
    static void stop_animation(const Entity& e) { animation(e, ORCHARD_ANIM_STOP, std::string(), 0.0); }
    static void resume_animation(const Entity& e) { animation(e, ORCHARD_ANIM_RESUME, std::string(), 0.0); }
    static void set_animation_speed(const Entity& e, double speed) { animation(e, ORCHARD_ANIM_SPEED, std::string(), speed); }
    static double animation_time(const Entity& e) { return animation(e, ORCHARD_ANIM_TIME, std::string(), 0.0); }
    static void set_animation_time(const Entity& e, double seconds) { animation(e, ORCHARD_ANIM_SET_TIME, std::string(), seconds); }
    static double animation_length(const Entity& e) { return animation(e, ORCHARD_ANIM_LENGTH, std::string(), 0.0); }
    static bool animation_playing(const Entity& e) { return animation(e, ORCHARD_ANIM_PLAYING, std::string(), 0.0) != 0.0; }
    static std::string animation_event(const Entity& e) {
        if (!ORCHARD_HOST_HAS(detail::host(), animation_event)) return std::string();
        const char* s = detail::host()->animation_event(e.id());
        return s ? std::string(s) : std::string();
    }
    static void clear_particles(const Entity& e) { if (ORCHARD_HOST_HAS(detail::host(), particles)) detail::host()->particles(e.id(), ORCHARD_PARTICLES_CLEAR, 0); }

    static double game(long long op, const Entity& e, const Entity& other, const std::vector<double>& args, const std::string& text) {
        return ORCHARD_HOST_HAS(detail::host(), game) ? detail::host()->game(static_cast<int>(op), e.id(), other.id(), args.empty() ? nullptr : args.data(), static_cast<int>(args.size()), text.c_str()) : 0.0;
    }
    static Entity game_entity(long long op, const Entity& e, const Entity& other, const std::vector<double>& args, const std::string& text) {
        return ORCHARD_HOST_HAS(detail::host(), game_entity) ? Entity(detail::host()->game_entity(static_cast<int>(op), e.id(), other.id(), args.empty() ? nullptr : args.data(), static_cast<int>(args.size()), text.c_str())) : Entity();
    }
    static double health(const Entity& e) { return game(ORCHARD_GAME_HEALTH, e, Entity(), {}, std::string()); }
    static double max_health(const Entity& e) { return game(ORCHARD_GAME_MAX_HEALTH, e, Entity(), {}, std::string()); }
    static bool has_health(const Entity& e) { return game(ORCHARD_GAME_HAS_HEALTH, e, Entity(), {}, std::string()) != 0.0; }
    static void set_health(const Entity& e, double value) { game(ORCHARD_GAME_SET_HEALTH, e, Entity(), {value}, std::string()); }
    static void set_max_health(const Entity& e, double value) { game(ORCHARD_GAME_SET_MAX_HEALTH, e, Entity(), {value}, std::string()); }
    static double add_health(const Entity& e, double amount) { return game(ORCHARD_GAME_HEAL, e, Entity(), {amount}, std::string()); }
    static double heal(const Entity& e, double amount) { return add_health(e, amount); }
    static double damage(const Entity& e, double amount) { return game(ORCHARD_GAME_DAMAGE, e, Entity(), {amount}, std::string()); }
    static double damage_from(const Entity& e, double amount, const Entity& attacker) { return game(ORCHARD_GAME_DAMAGE, e, attacker, {amount}, std::string()); }
    static bool kill(const Entity& e) { return game(ORCHARD_GAME_KILL, e, Entity(), {}, std::string()) != 0.0; }
    static bool is_dead(const Entity& e) { return game(ORCHARD_GAME_IS_DEAD, e, Entity(), {}, std::string()) != 0.0; }
    static bool revive(const Entity& e) { return game(ORCHARD_GAME_REVIVE, e, Entity(), {-1.0}, std::string()) != 0.0; }
    static bool revive_with(const Entity& e, double health) { return game(ORCHARD_GAME_REVIVE, e, Entity(), {health}, std::string()) != 0.0; }
    static void set_invulnerable(const Entity& e, bool on) { game(ORCHARD_GAME_SET_INVULNERABLE, e, Entity(), {on ? 1.0 : 0.0}, std::string()); }
    static double damage_taken(const Entity& e) { return game(ORCHARD_GAME_DAMAGE_TAKEN, e, Entity(), {}, std::string()); }
    static bool died(const Entity& e) { return game(ORCHARD_GAME_DIED, e, Entity(), {}, std::string()) != 0.0; }
    static Entity last_attacker(const Entity& e) { return game_entity(ORCHARD_GAME_LAST_ATTACKER, e, Entity(), {}, std::string()); }
    static void ignite(const Entity& e, double seconds) { game(ORCHARD_GAME_IGNITE, e, Entity(), {seconds}, std::string()); }
    static void extinguish(const Entity& e) { game(ORCHARD_GAME_EXTINGUISH, e, Entity(), {}, std::string()); }
    static bool is_burning(const Entity& e) { return game(ORCHARD_GAME_IS_BURNING, e, Entity(), {}, std::string()) != 0.0; }
    static long long explode(double x, double y, double z, double radius, double force, double damage) { return static_cast<long long>(game(ORCHARD_GAME_EXPLODE, Entity(), Entity(), {x, y, z, radius, force, damage, 1.0}, std::string())); }
    static long long explode_by(const Entity& attacker, double x, double y, double z, double radius, double force, double damage) { return static_cast<long long>(game(ORCHARD_GAME_EXPLODE, attacker, Entity(), {x, y, z, radius, force, damage, 1.0}, std::string())); }
    static long long push_away(double x, double y, double z, double radius, double force) { return static_cast<long long>(game(ORCHARD_GAME_EXPLODE, Entity(), Entity(), {x, y, z, radius, force, 0.0, 0.0}, std::string())); }
    static Entity play_effect(const std::string& effect, double x, double y, double z, double scale) { return game_entity(ORCHARD_GAME_PLAY_EFFECT, Entity(), Entity(), {x, y, z, scale}, effect); }
    static Entity attach_effect(const std::string& effect, const Entity& parent, double scale) { return game_entity(ORCHARD_GAME_PLAY_EFFECT, parent, Entity(), {0.0, 0.0, 0.0, scale}, effect); }
    static void add_tag(const Entity& e, const std::string& tag) { game(ORCHARD_GAME_ADD_TAG, e, Entity(), {}, tag); }
    static void remove_tag(const Entity& e, const std::string& tag) { game(ORCHARD_GAME_REMOVE_TAG, e, Entity(), {}, tag); }
    static bool has_tag(const Entity& e, const std::string& tag) { return game(ORCHARD_GAME_HAS_TAG, e, Entity(), {}, tag) != 0.0; }
    static long long tagged_count(const std::string& tag) { return static_cast<long long>(game(ORCHARD_GAME_TAGGED_COUNT, Entity(), Entity(), {}, tag)); }
    static Entity tagged_at(const std::string& tag, long long i) { return game_entity(ORCHARD_GAME_TAGGED_AT, Entity(), Entity(), {static_cast<double>(i)}, tag); }
    static std::vector<Entity> find_tagged(const std::string& tag) {
        std::vector<Entity> out;
        const long long n = tagged_count(tag);
        for (long long i = 0; i < n; ++i) out.push_back(tagged_at(tag, i));
        return out;
    }
    static Entity nearest_tagged(const std::string& tag, const Entity& from) { return game_entity(ORCHARD_GAME_NEAREST_TAGGED, from, Entity(), {}, tag); }
    static double distance(const Entity& a, const Entity& b) { return game(ORCHARD_GAME_DISTANCE, a, b, {}, std::string()); }
    static void teleport(const Entity& e, double x, double y, double z) { game(ORCHARD_GAME_TELEPORT, e, Entity(), {x, y, z}, std::string()); }
    static void look_at(const Entity& e, double x, double y, double z) { game(ORCHARD_GAME_LOOK_AT, e, Entity(), {x, y, z}, std::string()); }
    static double random(double low, double high) { return game(ORCHARD_GAME_RANDOM, Entity(), Entity(), {low, high}, std::string()); }
    static long long random_int(long long low, long long high) {
        const long long r = static_cast<long long>(std::floor(random(static_cast<double>(low), static_cast<double>(high) + 1.0)));
        return r > high ? high : r;
    }
    static double clamp(double v, double low, double high) { return v < low ? low : (v > high ? high : v); }
    static double lerp(double a, double b, double t) { return a + (b - a) * t; }
    static double move_towards(double current, double target, double max_step) {
        const double d = target - current;
        return d > max_step ? current + max_step : (d < -max_step ? current - max_step : target);
    }

    static Entity raycast(double ox, double oy, double oz, double dx, double dy, double dz, double max_distance) {
        double* hit = last_hit();
        for (int i = 0; i < 7; ++i) hit[i] = 0.0;
        if (!ORCHARD_HOST_HAS(detail::host(), raycast)) return Entity();
        return Entity(detail::host()->raycast(ox, oy, oz, dx, dy, dz, max_distance, hit));
    }
    static double hit(long long which) { return which >= 0 && which < 7 ? last_hit()[which] : 0.0; }
    static long long overlap_sphere(double x, double y, double z, double radius) {
        std::vector<orchard_entity>& found = last_found();
        found.assign(256, 0);
        const int n = ORCHARD_HOST_HAS(detail::host(), overlap_sphere) ? detail::host()->overlap_sphere(x, y, z, radius, found.data(), 256) : 0;
        found.resize(static_cast<std::size_t>(n < 0 ? 0 : (n > 256 ? 256 : n)));
        return static_cast<long long>(found.size());
    }
    static Entity overlap_at(long long i) {
        const std::vector<orchard_entity>& found = last_found();
        return i >= 0 && static_cast<std::size_t>(i) < found.size() ? Entity(found[static_cast<std::size_t>(i)]) : Entity();
    }

private:
    static double* last_hit() {
        static double h[7] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        return h;
    }
    static std::vector<orchard_entity>& last_found() {
        static std::vector<orchard_entity> f;
        return f;
    }
};

} // namespace script
} // namespace orchard

#endif // ORCHARD_SCRIPT_SCRIPT_API_HPP
