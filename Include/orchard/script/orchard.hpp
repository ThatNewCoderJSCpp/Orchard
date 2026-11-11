#ifndef ORCHARD_SCRIPT_ORCHARD_HPP
#define ORCHARD_SCRIPT_ORCHARD_HPP

#include "script_api.hpp"
#include "script_lib.hpp"

#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace orchard {
namespace script {

inline void log(const std::string& message) { World::log(message); }
inline void warn(const std::string& message) { World::warn(message); }
inline double dt() { return World::dt(); }
inline double time() { return World::time(); }
inline bool is_2d() { return World::is_2d(); }
inline bool key_down(const std::string& key) { return World::key_down(key); }
inline Entity self() { return Entity::current(); }
inline Entity find(const std::string& name) { return World::find(name); }
inline Entity spawn(const std::string& name) { return World::spawn(name); }
inline Entity spawn(const std::string& name, const Entity& parent) { return World::spawn_child(name, parent); }
inline Entity instantiate(const std::string& entity_template, double x, double y, double z) { return World::instantiate(entity_template, x, y, z); }
inline Entity instantiate(const std::string& entity_template, double x, double y, double z, const Entity& parent) { return World::instantiate_child(entity_template, x, y, z, parent); }
inline void play_particles(const Entity& e) { World::play_particles(e); }
inline void stop_particles(const Entity& e) { World::stop_particles(e); }
inline void burst_particles(const Entity& e, long long count) { World::burst_particles(e, count); }
inline void clear_particles(const Entity& e) { World::clear_particles(e); }
inline void play_animation(const Entity& e, const std::string& clip, double fade = 0.25) { World::play_animation(e, clip, fade); }
inline void play_animation_once(const Entity& e, const std::string& clip, double fade = 0.25) { World::play_animation_once(e, clip, fade); }
inline void stop_animation(const Entity& e) { World::stop_animation(e); }
inline void resume_animation(const Entity& e) { World::resume_animation(e); }
inline void set_animation_speed(const Entity& e, double speed) { World::set_animation_speed(e, speed); }
inline double animation_time(const Entity& e) { return World::animation_time(e); }
inline void set_animation_time(const Entity& e, double seconds) { World::set_animation_time(e, seconds); }
inline double animation_length(const Entity& e) { return World::animation_length(e); }
inline bool animation_playing(const Entity& e) { return World::animation_playing(e); }
inline std::string animation_event(const Entity& e) { return World::animation_event(e); }
inline long long pour_fluid(double x, double y, double radius) { return World::pour_fluid(x, y, radius); }
inline long long erase_fluid(double x, double y, double radius) { return World::erase_fluid(x, y, radius); }
inline long long push_fluid(double x, double y, double radius, double vx, double vy) { return World::push_fluid(x, y, radius, vx, vy); }
inline long long splash_fluid(double x, double y, double radius, double strength) { return World::splash_fluid(x, y, radius, strength); }
inline long long paint_sand(double x, double y, double radius, long long cell) { return World::paint_sand(x, y, radius, cell); }
inline double health(const Entity& e) { return World::health(e); }
inline double max_health(const Entity& e) { return World::max_health(e); }
inline bool has_health(const Entity& e) { return World::has_health(e); }
inline void set_health(const Entity& e, double value) { World::set_health(e, value); }
inline void set_max_health(const Entity& e, double value) { World::set_max_health(e, value); }
inline double add_health(const Entity& e, double amount) { return World::add_health(e, amount); }
inline double heal(const Entity& e, double amount) { return World::heal(e, amount); }
inline double damage(const Entity& e, double amount) { return World::damage(e, amount); }
inline double damage(const Entity& e, double amount, const Entity& attacker) { return World::damage_from(e, amount, attacker); }
inline bool kill(const Entity& e) { return World::kill(e); }
inline bool is_dead(const Entity& e) { return World::is_dead(e); }
inline bool revive(const Entity& e) { return World::revive(e); }
inline bool revive(const Entity& e, double health) { return World::revive_with(e, health); }
inline void set_invulnerable(const Entity& e, bool on) { World::set_invulnerable(e, on); }
inline double damage_taken(const Entity& e) { return World::damage_taken(e); }
inline bool died(const Entity& e) { return World::died(e); }
inline Entity last_attacker(const Entity& e) { return World::last_attacker(e); }
inline void ignite(const Entity& e, double seconds = 10.0) { World::ignite(e, seconds); }
inline void extinguish(const Entity& e) { World::extinguish(e); }
inline bool is_burning(const Entity& e) { return World::is_burning(e); }
inline long long explode(const Vec3& at, double radius = 5.0, double force = 20000.0, double damage = 100.0) { return World::explode(at.x, at.y, at.z, radius, force, damage); }
inline long long explode(const Entity& attacker, const Vec3& at, double radius = 5.0, double force = 20000.0, double damage = 100.0) { return World::explode_by(attacker, at.x, at.y, at.z, radius, force, damage); }
inline long long push_away(const Vec3& at, double radius, double force) { return World::push_away(at.x, at.y, at.z, radius, force); }
inline Entity play_effect(const std::string& effect, const Vec3& at, double scale = 1.0) { return World::play_effect(effect, at.x, at.y, at.z, scale); }
inline Entity attach_effect(const std::string& effect, const Entity& parent, double scale = 1.0) { return World::attach_effect(effect, parent, scale); }
inline void add_tag(const Entity& e, const std::string& tag) { World::add_tag(e, tag); }
inline void remove_tag(const Entity& e, const std::string& tag) { World::remove_tag(e, tag); }
inline bool has_tag(const Entity& e, const std::string& tag) { return World::has_tag(e, tag); }
inline std::vector<Entity> find_tagged(const std::string& tag) { return World::find_tagged(tag); }
inline Entity nearest_tagged(const std::string& tag, const Entity& from) { return World::nearest_tagged(tag, from); }
inline double distance(const Entity& a, const Entity& b) { return World::distance(a, b); }
inline void teleport(const Entity& e, const Vec3& to) { World::teleport(e, to.x, to.y, to.z); }
inline void look_at(const Entity& e, const Vec3& target) { World::look_at(e, target.x, target.y, target.z); }
inline void look_at(const Entity& e, const Entity& target) { const Vec3 p = target.world_position(); World::look_at(e, p.x, p.y, p.z); }
inline double random(double low, double high) { return World::random(low, high); }
inline long long random_int(long long low, long long high) { return World::random_int(low, high); }
inline double clamp(double v, double low, double high) { return World::clamp(v, low, high); }
inline double lerp(double a, double b, double t) { return World::lerp(a, b, t); }
inline double move_towards(double current, double target, double max_step) { return World::move_towards(current, target, max_step); }

template <class... Parts>
std::string text(Parts&&... parts) {
    std::ostringstream out;
    (out << ... << std::forward<Parts>(parts));
    return out.str();
}

inline std::ostream& operator<<(std::ostream& out, const Vec3& v) {
    return out << "(" << v.x << ", " << v.y << ", " << v.z << ")";
}

} // namespace script
} // namespace orchard

#endif // ORCHARD_SCRIPT_ORCHARD_HPP
