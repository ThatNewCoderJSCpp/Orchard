#ifndef ORCHARD_SCRIPT_SCRIPT_API_HPP
#define ORCHARD_SCRIPT_SCRIPT_API_HPP

#include "orchard_api.h"

#include <cmath>
#include <functional>
#include <string>

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
};

} // namespace script
} // namespace orchard

#endif // ORCHARD_SCRIPT_SCRIPT_API_HPP
